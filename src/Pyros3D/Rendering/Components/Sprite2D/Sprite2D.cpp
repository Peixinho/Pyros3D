//============================================================================
// Name        : Sprite2D.cpp
// Description : See the header.
//============================================================================

#include <Pyros3D/Rendering/Components/Sprite2D/Sprite2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/SpriteBatch2D.h>
#include "SpriteGeometry2D.h"
#include <Pyros3D/Rendering/Components/Layer2D/Layer2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/SpriteMask2D.h>
#include <cstring>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Assets/TileSet2D/TileSet2D.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Rendering/Culling/Culling.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <mutex>

namespace p3d {

	namespace {

		std::atomic<int> gLiveSprite2D(0);
		// Goes up whenever any sprite changes how it would be drawn - see
		// Sprite2D::ChangeCount.
		std::atomic<uint64_t> gSpriteChanges(1);
		std::atomic<bool> gClocksRunning(true);

		const std::string kNoName;

		// Sprites whose quads are stale. They are rebuilt just before their
		// scene is drawn (Sprite2D::FlushScene), not on their own next
		// Update(): a script that moves a sprite and flips it in the same
		// frame would otherwise have the move drawn this frame and the flip
		// the next. Main thread only, like everything that edits a scene.
		std::vector<Sprite2D*> &Pending()
		{
			static std::vector<Sprite2D*> pending;
			return pending;
		}
		// The list is touched wherever a sprite is made or changed, and a
		// streamed cell is put together on a worker thread: the lock is for
		// that, not for the flush, which is the main thread's alone.
		std::mutex &PendingLock()
		{
			static std::mutex m;
			return m;
		}
		// The sprites that are masked - see Sprite2D::FlushScene. Under the
		// same lock.
		std::vector<Sprite2D*> &Masked()
		{
			static std::vector<Sprite2D*> all;
			return all;
		}

		// The lower-cased extension, dot included.
		std::string ExtensionOf(const std::string &path)
		{
			const size_t dot = path.find_last_of('.');
			const size_t slash = path.find_last_of("/\\");
			if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return std::string();
			std::string e = path.substr(dot);
			std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return (char)std::tolower(c); });
			return e;
		}

		std::string DirectoryOf(const std::string &path)
		{
			const size_t slash = path.find_last_of("/\\");
			return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
		}

		std::string FileNameOf(const std::string &path)
		{
			const size_t slash = path.find_last_of("/\\");
			return slash == std::string::npos ? path : path.substr(slash + 1);
		}

		// Textures shared between sprites, but not through Texture::LoadShared
		// - for the reason TileMap2D keeps its own: that cache does not make
		// filter or wrap part of a texture's identity, and a sprite sheet
		// needs both set. Keyed by filter as well, so pixel art and a
		// smooth-filtered use of the same PNG do not fight over one texture.
		std::map<std::string, std::weak_ptr<Texture> > &TextureCache()
		{
			static std::map<std::string, std::weak_ptr<Texture> > cache;
			return cache;
		}

		std::shared_ptr<Texture> LoadSpriteTexture(const std::string &path, const int32 filter)
		{
			const std::string key = path + (filter == SpriteFilter2D::Nearest ? "|n" : "|l");
			std::map<std::string, std::weak_ptr<Texture> > &cache = TextureCache();
			std::map<std::string, std::weak_ptr<Texture> >::iterator it = cache.find(key);
			if (it != cache.end())
			{
				if (std::shared_ptr<Texture> hit = it->second.lock()) return hit;
				cache.erase(it);
			}
			std::shared_ptr<Texture> t = std::make_shared<Texture>();
			// No mipmaps: a minified mip blends neighbouring frames together,
			// which the half-texel inset cannot undo.
			if (!t->LoadTexture(path, TextureType::Texture, false))
				return std::shared_ptr<Texture>();
			if (filter == SpriteFilter2D::Nearest)
				t->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
			else
				t->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
			t->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
			cache[key] = t;
			return t;
		}

		// One material per (image, filter, lit, blend), shared by every sprite
		// that matches. Sharing by POINTER, not by content: it is what makes
		// two sprites batchable at a glance, and a sprite never needs a
		// material of its own because nothing about one sprite is in it.
		std::map<std::string, std::weak_ptr<IMaterial> > &MaterialCache()
		{
			static std::map<std::string, std::weak_ptr<IMaterial> > cache;
			return cache;
		}

		class SpriteRenderable : public Renderable {
		public:
			SpriteRenderable(const SpriteMesh2D &m, const Vec4 &rect)
			{
				Geometries.push_back(new SpriteGeometry2D(m, rect));
				CalculateBounding();
			}
		};

		bool SameRect(const Vec4 &a, const Vec4 &b)
		{
			return fabsf(a.x - b.x) < 1e-5f && fabsf(a.y - b.y) < 1e-5f
				&& fabsf(a.z - b.z) < 1e-5f && fabsf(a.w - b.w) < 1e-5f;
		}

		// The names of the events a clip passes between two times, in order.
		// `from` is exclusive unless `includeStart`, which is how the events
		// on a clip's very first frame fire when it starts.
		void CollectEvents(const SpriteSheet2D &sheet, const SpriteClip2D &clip,
			const f32 from, const f32 to, const bool includeStart,
			std::vector<std::string> &out)
		{
			if (clip.events.empty() || clip.frames.empty() || to < from) return;

			// When each position of one pass begins.
			std::vector<int32> order;
			const int32 n = (int32)clip.frames.size();
			for (int32 i = 0; i < n; i++) order.push_back(i);
			if (clip.pingPong && n > 2)
				for (int32 i = n - 2; i >= 1; i--) order.push_back(i);
			std::vector<f32> start(order.size());
			f32 length = 0.f;
			for (size_t i = 0; i < order.size(); i++)
			{
				start[i] = length;
				const int32 fi = clip.frames[(size_t)order[i]];
				f32 d = (sheet.ValidFrame(fi) && sheet.frames[(size_t)fi].duration > 0.f)
					? sheet.frames[(size_t)fi].duration
					: (clip.fps > 0.0001f ? 1.f / clip.fps : 0.f);
				length += d;
			}
			if (length <= 0.f) return;

			const int32 firstPass = (int32)floorf(from / length);
			const int32 lastPass = clip.loop ? (int32)floorf(to / length) : 0;
			// A stall of several seconds is several passes of a short clip;
			// it does not need every footstep of them replayed.
			const int32 passes = std::min(lastPass, firstPass + 4);
			for (int32 pass = std::max(0, firstPass); pass <= passes && out.size() < 64; pass++)
				for (size_t i = 0; i < order.size(); i++)
				{
					const f32 at = (f32)pass * length + start[i];
					const bool after = includeStart ? (at >= from) : (at > from);
					if (!after || at > to) continue;
					for (size_t e = 0; e < clip.events.size(); e++)
						if (clip.events[e].at == order[i]) out.push_back(clip.events[e].name);
				}
		}

	}

	uint64 Sprite2D::ChangeCount() { return gSpriteChanges.load(); }
	uint32 Sprite2D::LiveCount() { const int n = gLiveSprite2D.load(); return n > 0 ? (uint32)n : 0; }
	void Sprite2D::SetClocksRunning(const bool run) { gClocksRunning = run; }
	bool Sprite2D::ClocksRunning() { return gClocksRunning.load(); }

	Sprite2D::Sprite2D()
		: frame(0), flipX(false), flipY(false), color(1.f, 1.f, 1.f, 1.f),
		drawMode(SpriteDraw2D::Simple), size(1.f, 1.f), pixelsPerUnit(0.f),
		hasPivot(false), pivot(0.5f, 0.5f), lighting(SpriteLighting2D::Unlit), blend(SpriteBlend2D::Alpha), sortOrder(0),
		clip(-1), clipTime(0.f), speed(1.f), paused(false), finished(false),
		autoPlayPending(false), lastTime(0.0), haveLastTime(false),
		meshDirty(true), materialDirty(true), built(false),
		builtRect(0.f, 0.f, 0.f, 0.f), revision(1), batch(NULL), inPending(false)
	{
		gLiveSprite2D++;
		// Dirty from birth: the first flush of whatever scene it joins
		// builds it.
		std::lock_guard<std::mutex> lock(PendingLock());
		inPending = true;
		Pending().push_back(this);
	}

	Sprite2D::~Sprite2D()
	{
		gLiveSprite2D--;
		gSpriteChanges++;
		if (batch) batch->Forget(this);
		{
			std::lock_guard<std::mutex> lock(PendingLock());
			if (inPending)
			{
				std::vector<Sprite2D*> &p = Pending();
				p.erase(std::remove(p.begin(), p.end(), this), p.end());
			}
			if (inMasked)
			{
				std::vector<Sprite2D*> &m = Masked();
				m.erase(std::remove(m.begin(), m.end(), this), m.end());
			}
		}
	}

	void Sprite2D::Touch(const bool mat)
	{
		meshDirty = true;
		if (mat) materialDirty = true;
		revision++;
		gSpriteChanges++;
		if (batch) batch->NoteChanged(this, mat);
		std::lock_guard<std::mutex> lock(PendingLock());
		if (!inPending) { inPending = true; Pending().push_back(this); }
	}

	void Sprite2D::FlushScene(SceneGraph* scene)
	{
		if (gLiveSprite2D.load() <= 0) return;
		// A mask that moved, or a sprite that moved under one, is quads to
		// be cut again - and nothing says so but looking. Only the sprites
		// that are masked are looked at.
		{
			std::vector<Sprite2D*> masked;
			{
				std::lock_guard<std::mutex> lock(PendingLock());
				masked = Masked();
			}
			for (size_t i = 0; i < masked.size(); i++)
			{
				GameObject* o = masked[i]->GetOwner();
				if (o && o->GetOwningScene() == scene && masked[i]->MaskChanged()) masked[i]->Touch();
			}
		}
		// Taken out under the lock and rebuilt outside it: Rebuild() can add
		// a RenderingComponent, and touching a sprite takes the same lock.
		std::vector<Sprite2D*> mine;
		{
			std::lock_guard<std::mutex> lock(PendingLock());
			std::vector<Sprite2D*> &pending = Pending();
			if (!pending.empty())
			{
				std::vector<Sprite2D*> rest;
				for (size_t i = 0; i < pending.size(); i++)
				{
					Sprite2D* s = pending[i];
					GameObject* o = s->GetOwner();
					// Not in a scene yet - or in another one, which will
					// flush it when it draws.
					if (o && o->GetOwningScene() == scene) { s->inPending = false; mine.push_back(s); }
					else rest.push_back(s);
				}
				pending.swap(rest);
			}
		}
		for (size_t i = 0; i < mine.size(); i++)
			if (mine[i]->meshDirty) mine[i]->Rebuild();
		SpriteBatch2D::FlushScene(scene);
	}

	// --- source ---------------------------------------------------------------

	void Sprite2D::SetSheet(const SpriteSheet2D &s)
	{
		sheet = s;
		resolvedImage.clear();
		if (!sheet.ValidFrame(frame)) frame = 0;
		// A clip index meant the old sheet's clips.
		if (clip >= (int32)sheet.clips.size()) { clip = -1; clipTime = 0.f; finished = false; }
		Touch(true);
	}

	void Sprite2D::SetResolvedImagePath(const std::string &absolute)
	{
		if (resolvedImage == absolute) return;
		resolvedImage = absolute;
		Touch(true);
	}

	bool Sprite2D::LoadSource(const std::string &resolvedPath,
		const std::function<std::string(const std::string&)> &imageResolver,
		std::string* errorOut)
	{
		SpriteSheet2D s;
		std::string imageAbs;
		if (ExtensionOf(resolvedPath) == ".p3ds")
		{
			std::string err;
			if (!LoadSpriteSheet2D(resolvedPath, s, &err))
			{
				if (errorOut) *errorOut = err;
				return false;
			}
			if (imageResolver) imageAbs = imageResolver(s.image);
			// Beside the sheet, when nobody can say where else: a sheet and
			// its image are saved together.
			if (imageAbs.empty()) imageAbs = DirectoryOf(resolvedPath) + FileNameOf(s.image);
		}
		else
		{
			// A bare image is a sheet of one frame.
			imageAbs = resolvedPath;
		}

		int32 iw = 0, ih = 0;
		if (!TileSet2DReadImageSize(imageAbs, iw, ih))
		{
			if (errorOut) *errorOut = "could not read the image " + imageAbs;
			return false;
		}
		s.SetImageSize(iw, ih);
		if (s.frames.empty())
		{
			SpriteFrame2D f;
			f.name = "frame0";
			f.w = iw;
			f.h = ih;
			s.frames.push_back(f);
		}
		SetSheet(s);
		SetResolvedImagePath(imageAbs);
		return true;
	}

	std::string Sprite2D::WantedImage() const
	{
		if (!resolvedImage.empty()) return resolvedImage;
		return resolve ? resolve(sheet.image) : sheet.image;
	}

	std::string Sprite2D::WantedNormalImage() const
	{
		if (sheet.normalImage.empty()) return std::string();
		if (!resolvedImage.empty())
		{
			// The image was resolved to somewhere: the normal map is the
			// same distance up the same tree.
			const std::string &img = sheet.image;
			if (!img.empty() && resolvedImage.size() >= img.size()
				&& resolvedImage.compare(resolvedImage.size() - img.size(), img.size(), img) == 0)
				return resolvedImage.substr(0, resolvedImage.size() - img.size()) + sheet.normalImage;
			// Or, failing that, beside it.
			const size_t slash = resolvedImage.find_last_of("/\\");
			const size_t nslash = sheet.normalImage.find_last_of("/\\");
			if (slash != std::string::npos)
				return resolvedImage.substr(0, slash + 1)
					+ (nslash == std::string::npos ? sheet.normalImage : sheet.normalImage.substr(nslash + 1));
		}
		return resolve ? resolve(sheet.normalImage) : sheet.normalImage;
	}

	// --- frame ------------------------------------------------------------------

	void Sprite2D::SetFrame(const int32 index)
	{
		clip = -1;
		finished = false;
		if (!sheet.ValidFrame(index) || index == frame) return;
		frame = index;
		Touch();
	}

	bool Sprite2D::SetFrameByName(const std::string &name)
	{
		const int32 i = sheet.FindFrame(name);
		if (i < 0) return false;
		SetFrame(i);
		return true;
	}

	const std::string &Sprite2D::GetFrameName() const
	{
		return sheet.ValidFrame(frame) ? sheet.frames[(size_t)frame].name : kNoName;
	}

	// --- look -------------------------------------------------------------------

	void Sprite2D::SetFlipX(const bool f) { if (flipX != f) { flipX = f; Touch(); } }
	void Sprite2D::SetFlipY(const bool f) { if (flipY != f) { flipY = f; Touch(); } }

	void Sprite2D::SetColor(const Vec4 &c)
	{
		if (c.x == color.x && c.y == color.y && c.z == color.z && c.w == color.w) return;
		color = c;
		Touch();
	}

	void Sprite2D::SetAlpha(const f32 a) { SetColor(Vec4(color.x, color.y, color.z, a)); }

	void Sprite2D::SetDrawMode(const int32 mode)
	{
		const int32 m = (mode == SpriteDraw2D::Sliced || mode == SpriteDraw2D::Tiled) ? mode : SpriteDraw2D::Simple;
		if (m != drawMode) { drawMode = m; Touch(); }
	}

	void Sprite2D::SetSize(const Vec2 &s)
	{
		if (s.x == size.x && s.y == size.y) return;
		size = s;
		Touch();
	}

	void Sprite2D::SetPixelsPerUnit(const f32 ppu)
	{
		const f32 p = ppu > 0.f ? ppu : 0.f;
		if (p != pixelsPerUnit) { pixelsPerUnit = p; Touch(); }
	}

	void Sprite2D::SetLighting(const int32 mode)
	{
		const int32 m = (mode == SpriteLighting2D::Lit2D || mode == SpriteLighting2D::Lit3D) ? mode : SpriteLighting2D::Unlit;
		if (m != lighting) { lighting = m; Touch(true); }
	}

	void Sprite2D::SetBlend(const int32 b)
	{
		const int32 v = (b == SpriteBlend2D::Additive || b == SpriteBlend2D::Premultiplied) ? b : SpriteBlend2D::Alpha;
		if (v != blend) { blend = v; Touch(true); }
	}

	void Sprite2D::SetSortOrder(const int32 o)
	{
		if (o == sortOrder) return;
		sortOrder = o;
		if (RenderingComponent* rc = FindSibling()) rc->SetSortOrder(o);
		revision++;
		gSpriteChanges++;
		if (batch) batch->NoteChanged(this, false);
	}

	SpriteMeshParams2D Sprite2D::MeshParams() const
	{
		SpriteMeshParams2D p;
		p.frame = frame;
		p.flipX = flipX;
		p.flipY = flipY;
		p.color = color;
		p.mode = drawMode;
		p.size = size;
		p.pixelsPerUnit = pixelsPerUnit;
		p.overridePivot = hasPivot;
		p.pivot = pivot;
		return p;
	}

	Vec4 Sprite2D::GetLocalRect() const
	{
		return SpriteLocalRect2D(sheet, MeshParams());
	}

	void Sprite2D::SetPivot(const Vec2 &p)
	{
		if (hasPivot && p.x == pivot.x && p.y == pivot.y) return;
		hasPivot = true;
		pivot = p;
		Touch();
	}

	void Sprite2D::ClearPivot()
	{
		if (!hasPivot) return;
		hasPivot = false;
		Touch();
	}

	Vec2 Sprite2D::GetPivot() const
	{
		if (hasPivot) return pivot;
		return sheet.ValidFrame(frame) ? sheet.frames[(size_t)frame].pivot : Vec2(0.5f, 0.5f);
	}

	// --- clips ------------------------------------------------------------------

	const std::string &Sprite2D::GetClip() const
	{
		return (clip >= 0 && clip < (int32)sheet.clips.size()) ? sheet.clips[(size_t)clip].name : kNoName;
	}

	std::vector<std::string> Sprite2D::ClipNames() const
	{
		std::vector<std::string> out;
		for (size_t i = 0; i < sheet.clips.size(); i++) out.push_back(sheet.clips[i].name);
		return out;
	}

	bool Sprite2D::Play(const std::string &name, const bool restart)
	{
		const int32 c = sheet.FindClip(name);
		if (c < 0) return false;
		paused = false;
		if (c == clip && !restart && !finished) return true;
		clip = c;
		clipTime = 0.f;
		finished = false;
		autoPlayPending = false;
		// The object may have been asleep: the gap since its last update is
		// not time this clip played.
		haveLastTime = false;
		ApplyClipTime(0.f, true);
		WakeOwner();
		return true;
	}

	void Sprite2D::Stop()
	{
		clip = -1;
		clipTime = 0.f;
		finished = false;
		paused = false;
		autoPlayPending = false;
	}

	void Sprite2D::Pause(const bool p)
	{
		paused = p;
		if (!p) { haveLastTime = false; WakeOwner(); }
	}

	void Sprite2D::SetClipTime(const f32 seconds)
	{
		if (clip < 0) return;
		clipTime = seconds < 0.f ? 0.f : seconds;
		finished = false;
		// Scrubbing is a tool looking at a moment, not the clip being played
		// through it: no events.
		const SpriteClipCursor2D cur = SpriteClip2DAt(sheet, sheet.clips[(size_t)clip], clipTime);
		if (cur.frame >= 0 && cur.frame != frame) { frame = cur.frame; Touch(); }
		finished = cur.finished;
	}

	void Sprite2D::ApplyClipTime(const f32 before, const bool starting)
	{
		if (clip < 0 || clip >= (int32)sheet.clips.size()) return;
		// By value: a handler below is free to Play() another clip or swap
		// the sheet, and must not pull this one out from under the loop.
		const SpriteClip2D playing = sheet.clips[(size_t)clip];

		std::vector<std::string> events;
		if (OnEvent) CollectEvents(sheet, playing, before, clipTime, starting, events);

		const SpriteClipCursor2D cur = SpriteClip2DAt(sheet, playing, clipTime);
		if (cur.frame >= 0 && cur.frame != frame) { frame = cur.frame; Touch(); }
		const bool justFinished = cur.finished && !finished;
		finished = cur.finished;

		// Handlers last, once this sprite's own state is settled.
		for (size_t i = 0; i < events.size(); i++)
			if (OnEvent) OnEvent(events[i]);
		if (justFinished && OnFinished) OnFinished(playing.name);
	}

	void Sprite2D::Advance(const f32 dt)
	{
		if (clip < 0 || paused || finished || dt <= 0.f) return;
		const f32 before = clipTime;
		clipTime += dt * speed;
		if (clipTime < 0.f) clipTime = 0.f;
		ApplyClipTime(before, false);
	}

	// --- geometry -----------------------------------------------------------------

	bool Sprite2D::BuildMesh(SpriteMesh2D &out, const Matrix* transform) const
	{
		const size_t first = out.vertex.size();
		// (hidden: nothing of it goes into a batch; its own mesh is switched
		// off in Rebuild, which still wants the mesh built)
		if (!visible && transform != NULL) return false;
		if (!BuildSpriteMesh2D(sheet, MeshParams(), out, transform)) return false;
		if (maskMode == MaskNone || !Owner) return true;
		SpriteMask2D* mask = SpriteMask2D::Find(Owner, maskName);
		if (!mask) return true;
		std::vector<std::vector<Vec2> > pieces;
		mask->WorldPieces(pieces);
		// A mask with no shape (no size yet, an image that did not load)
		// cuts nothing.
		if (pieces.empty()) return true;
		// The pieces are in the world; the quads are in this sprite's own
		// space, or - appended to a batch - in the batch's.
		Matrix toMesh = Owner->GetWorldTransformation().Inverse();
		if (transform) toMesh = (*transform) * toMesh;
		for (size_t k = 0; k < pieces.size(); k++)
			for (size_t i = 0; i < pieces[k].size(); i++)
			{
				const Vec2 w = pieces[k][i];
				pieces[k][i] = Vec2(toMesh.m[0] * w.x + toMesh.m[4] * w.y + toMesh.m[12], toMesh.m[1] * w.x + toMesh.m[5] * w.y + toMesh.m[13]);
			}
		// A soft edge, for a shape that is one piece. Its width is the
		// world's; the quads may be scaled.
		if (mask->GetFeather() > 0.f && pieces.size() == 1)
		{
			const f32 scale = sqrtf(toMesh.m[0] * toMesh.m[0] + toMesh.m[1] * toMesh.m[1]);
			return ClipSpriteMeshFeathered2D(out, first, pieces[0], maskMode == MaskInside, mask->GetFeather() * scale);
		}
		return ClipSpriteMeshPieces2D(out, first, pieces, maskMode == MaskInside);
	}

	void Sprite2D::SetVisible(const bool v)
	{
		if (v == visible) return;
		visible = v;
		Touch();
	}

	void Sprite2D::SetMaskInteraction(const int32 mode)
	{
		const int32 v = (mode == MaskInside || mode == MaskOutside) ? mode : MaskNone;
		if (v == maskMode) return;
		maskMode = v;
		{
			// Only masked sprites are looked at each frame for a mask that
			// moved; this is the list of them.
			std::lock_guard<std::mutex> lock(PendingLock());
			std::vector<Sprite2D*> &m = Masked();
			if (v != MaskNone && !inMasked) { m.push_back(this); inMasked = true; }
			else if (v == MaskNone && inMasked) { m.erase(std::remove(m.begin(), m.end(), this), m.end()); inMasked = false; }
		}
		maskSeen = NULL;
		Touch();
	}

	void Sprite2D::SetMaskName(const std::string &name)
	{
		if (name == maskName) return;
		maskName = name;
		maskSeen = NULL;
		Touch();
	}

	bool Sprite2D::MaskChanged()
	{
		SpriteMask2D* mask = Owner ? SpriteMask2D::Find(Owner, maskName) : NULL;
		f32 rel[16] = { 0 };
		if (mask && mask->GetOwner())
		{
			const Matrix r = Owner->GetWorldTransformation().Inverse() * mask->GetOwner()->GetWorldTransformation();
			memcpy(rel, r.m, sizeof(rel));
		}
		const uint32 rev = mask ? mask->GetRevision() : 0;
		if (mask == maskSeen && rev == maskSeenRevision && memcmp(rel, maskSeenRelative, sizeof(rel)) == 0) return false;
		maskSeen = mask;
		maskSeenRevision = rev;
		memcpy(maskSeenRelative, rel, sizeof(rel));
		return true;
	}

	void Sprite2D::ForgetImage(const std::string &imagePath)
	{
		if (imagePath.empty()) return;
		// Both caches key on the image's path first: the picture by path
		// and filter, a material by path and everything else it is made of
		// (and by a normal map's path, further along the key).
		std::map<std::string, std::weak_ptr<Texture> > &textures = TextureCache();
		for (std::map<std::string, std::weak_ptr<Texture> >::iterator it = textures.begin(); it != textures.end();)
			if (it->first.compare(0, imagePath.size() + 1, imagePath + "|") == 0) textures.erase(it++);
			else ++it;
		std::map<std::string, std::weak_ptr<IMaterial> > &materials = MaterialCache();
		for (std::map<std::string, std::weak_ptr<IMaterial> >::iterator it = materials.begin(); it != materials.end();)
			if (it->first.compare(0, imagePath.size() + 1, imagePath + "|") == 0
				|| it->first.find("|nm:" + imagePath) != std::string::npos) materials.erase(it++);
			else ++it;
	}

	std::string Sprite2D::MaterialKey() const
	{
		std::string k = WantedImage();
		k += sheet.filter == SpriteFilter2D::Nearest ? "|n" : "|l";
		k += lighting == SpriteLighting2D::Lit2D ? "|lit2d" : (lighting == SpriteLighting2D::Lit3D ? "|lit3d" : "|unlit");
		k += blend == SpriteBlend2D::Additive ? "|add" : (blend == SpriteBlend2D::Premultiplied ? "|pre" : "|alpha");
		// Only where something will read it: an unlit sprite with a normal
		// map is the same material as one without.
		if (lighting != SpriteLighting2D::Unlit && !sheet.normalImage.empty())
			k += "|nm:" + WantedNormalImage();
		return k;
	}

	void Sprite2D::SetCustomMaterial(const std::shared_ptr<IMaterial> &m, const std::string &path)
	{
		if (customMaterial == m && customMaterialPath == path) return;
		customMaterial = m;
		customMaterialPath = m ? path : std::string();
		Touch(true);
	}

	std::shared_ptr<IMaterial> Sprite2D::SharedMaterial()
	{
		if (customMaterial) return customMaterial;
		const std::string key = MaterialKey();
		if (material && materialFor == key) return material;

		std::map<std::string, std::weak_ptr<IMaterial> > &cache = MaterialCache();
		std::map<std::string, std::weak_ptr<IMaterial> >::iterator it = cache.find(key);
		if (it != cache.end())
		{
			if (std::shared_ptr<IMaterial> hit = it->second.lock())
			{
				material = hit;
				materialFor = key;
				return material;
			}
			cache.erase(it);
		}

		const std::string image = WantedImage();
		std::shared_ptr<Texture> tex = image.empty()
			? std::shared_ptr<Texture>() : LoadSpriteTexture(image, sheet.filter);
		if (!tex)
		{
			echo("WARNING: sprite could not load its image '" + sheet.image + "'");
			return std::shared_ptr<IMaterial>();
		}

		// VertexColor carries the tint, so the material holds nothing that is
		// one sprite's. Diffuse is what makes the shader light it at all:
		// without it the texture goes to the screen as it is, which is what
		// "unlit" has to mean - with it and no lights in the scene, every
		// sprite is drawn at the ambient level, dim, and looks broken.
		// Lighting2D is distance falloff with no N.L - see
		// TileMap2D::EnsureMaterial.
		uint32 usage = ShaderUsage::Color | ShaderUsage::Texture | ShaderUsage::VertexColor;
		if (lighting != SpriteLighting2D::Unlit) usage |= ShaderUsage::Diffuse;
		if (lighting == SpriteLighting2D::Lit2D) usage |= ShaderUsage::Lighting2D;
		// Sampled the way the image is: a pixel-art sprite's normals are
		// per texel as its colours are.
		std::shared_ptr<Texture> normals;
		if (lighting != SpriteLighting2D::Unlit && !sheet.normalImage.empty())
		{
			normals = LoadSpriteTexture(WantedNormalImage(), sheet.filter);
			if (!normals) echo("WARNING: sprite could not load its normal map '" + sheet.normalImage + "' - lit flat");
			else usage |= ShaderUsage::BumpMapping;
		}

		std::shared_ptr<GenericShaderMaterial> mat = std::make_shared<GenericShaderMaterial>(usage);
		mat->SetColor(Vec4(1.f, 1.f, 1.f, 1.f));
		mat->SetColorMap(tex);
		if (normals) mat->SetNormalMap(normals);
		mat->EnableBlending();
		mat->BlendingEquation(BlendEq::Add);
		if (blend == SpriteBlend2D::Additive)
			mat->BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One);
		else if (blend == SpriteBlend2D::Premultiplied)
			mat->BlendingFunction(BlendFunc::One, BlendFunc::One_Minus_Src_Alpha);
		else
			mat->BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
		// See SceneEditor::OpAddSprite for why the flag is needed on top of
		// EnableBlending().
		mat->SetTransparencyFlag(true);
		mat->SetCullFace(CullFace::DoubleSided);

		material = mat;
		materialFor = key;
		cache[key] = material;
		return material;
	}

	RenderingComponent* Sprite2D::FindSibling() const
	{
		if (!Owner) return NULL;
		const std::vector<std::shared_ptr<IComponent> > &comps = Owner->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
			if (comps[i] && comps[i]->GetComponentType() == ComponentType::RenderingComponent)
				return static_cast<RenderingComponent*>(comps[i].get());
		return NULL;
	}

	void Sprite2D::SetBatched(SpriteBatch2D* b)
	{
		if (batch == b) return;
		batch = b;
		// In a batch the batch draws it and its own mesh must not; out of
		// one, its own mesh has to come back.
		// (Out of one, Rebuild puts back what was taken away here.)
		if (b)
			if (RenderingComponent* rc = FindSibling())
				if (rc->IsActive()) { rc->Disable(); meshOffByUs = true; }
		Touch();
	}

	bool Sprite2D::Rebuild()
	{
		if (!Owner) return false;
		// Drawn by a batch: there is no mesh of its own to keep. One above
		// it that has not found it yet will, at this same flush - building a
		// mesh now only to switch it off a moment later would be waste.
		SpriteBatch2D* above = batch ? NULL : SpriteBatch2D::Above(Owner);
		if (batch || (above && (!IsLit() || above->BatchesLit())))
		{
			meshDirty = false;
			materialDirty = false;
			return true;
		}

		std::shared_ptr<IMaterial> mat = SharedMaterial();
		SpriteMesh2D mesh;
		const bool have = mat && BuildMesh(mesh);

		RenderingComponent* rc = FindSibling();
		if (!have)
		{
			// Nothing to draw - no image, or no such frame. An empty geometry
			// is a crash rather than an empty draw (see TileChunkMesh2D), so
			// the mesh that exists is switched off instead.
			if (rc && built && rc->IsActive()) { rc->Disable(); meshOffByUs = true; }
			meshDirty = false;
			materialDirty = false;
			return false;
		}

		if (!rc)
		{
			// A sprite draws through a RenderingComponent and makes its own
			// when the object has none, so "add a sprite" is one component to
			// whoever is adding it. A placeholder to construct with; the
			// adopt below replaces it.
			std::shared_ptr<RenderingComponent> made = std::make_shared<RenderingComponent>(
				std::static_pointer_cast<Renderable>(std::make_shared<Plane>(1.f, 1.f)), mat);
			made->DisableCastShadows();
			Owner->Add(std::static_pointer_cast<IComponent>(made));
			rc = made.get();
			built = false;
		}

		const Vec4 rect = GetLocalRect();
		Renderable* r = rc->GetRenderable();
		const bool inPlace = built && !materialDirty && SameRect(rect, builtRect)
			&& r && r->Geometries.size() == 1;
		if (inPlace)
		{
			static_cast<SpriteGeometry2D*>(r->Geometries[0])->Fill(mesh, &rect);
		}
		else
		{
			// Box, for the reason a tilemap is: a sprite pivoted at its feet
			// does not straddle its origin.
			rc->SetCullingGeometry(CullingGeometry::Box);
			std::vector<std::shared_ptr<IMaterial> > mats;
			mats.push_back(mat);
			rc->AdoptGeneratedRenderable(std::make_shared<SpriteRenderable>(mesh, rect), mats);
		}
		rc->SetSortOrder(sortOrder);
		// Under a hidden layer it has its mesh and keeps it switched off.
		const bool hidden = !visible || Layer2D::HiddenAbove(Owner);
		if (hidden) { if (rc->IsActive()) { rc->Disable(); meshOffByUs = true; } }
		else if (meshOffByUs) { if (!rc->IsActive()) rc->Enable(); meshOffByUs = false; }

		built = true;
		builtRect = rect;
		meshDirty = false;
		materialDirty = false;
		return true;
	}

	bool Sprite2D::NeedsUpdate() const
	{
		return autoPlayPending || (clip >= 0 && !paused && !finished);
	}

	void Sprite2D::Update(const f64 time)
	{
		if (autoPlayPending)
		{
			autoPlayPending = false;
			Play(autoPlay, true);
		}

		// The scene's clock is absolute; a clip wants how much of it passed.
		// The first update has nothing to measure from, and a long gap - the
		// object slept, the scene was paused - is not time the clip played.
		f32 dt = 0.f;
		if (haveLastTime)
		{
			const f64 d = time - lastTime;
			if (d > 0.0) dt = (f32)(d > 0.25 ? 0.25 : d);
		}
		lastTime = time;
		haveLastTime = true;

		if (gClocksRunning.load() && visible) Advance(dt);
	}

};
