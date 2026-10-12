//============================================================================
// Name        : Line2D.cpp
// Description : See the header.
//============================================================================

#include <Pyros3D/Rendering/Components/Sprite2D/Line2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/Sprite2D.h>
#include <Pyros3D/Rendering/Components/Layer2D/Layer2D.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Assets/SpriteSheet2D/SpriteSheet2D.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <map>
#include <Pyros3D/GameObjects/GameObject.h>
#include "SpriteGeometry2D.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>

namespace p3d {

	namespace {
		std::atomic<int> gLiveLine2D(0);
		std::vector<Line2D*> &Lines()
		{
			static std::vector<Line2D*> all;
			return all;
		}
		std::mutex &LinesLock()
		{
			static std::mutex m;
			return m;
		}

		class LineRenderable : public Renderable {
		public:
			explicit LineRenderable(const SpriteMesh2D &m)
			{
				Geometries.push_back(new SpriteGeometry2D(m));
				CalculateBounding();
			}
		};

		// One material for every line of a kind - additive or not, and which
		// image if any: what makes one line different from another is all in
		// its vertices.
		std::shared_ptr<IMaterial> SharedLineMaterial(const std::string &key, const bool additive,
			const std::string &texturePath, const bool nearest)
		{
			static std::map<std::string, std::weak_ptr<IMaterial> > cache;
			std::map<std::string, std::weak_ptr<IMaterial> >::iterator it = cache.find(key);
			if (it != cache.end())
			{
				if (std::shared_ptr<IMaterial> hit = it->second.lock()) return hit;
				cache.erase(it);
			}
			std::shared_ptr<Texture> tex;
			if (!texturePath.empty())
			{
				tex = std::make_shared<Texture>();
				if (!tex->LoadTexture(texturePath, TextureType::Texture, false))
				{
					echo("WARNING: line could not load its image '" + texturePath + "' - drawn plain");
					tex.reset();
				}
				else
				{
					if (nearest) tex->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
					else tex->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
					// Repeated along the line; the two edges do not wrap
					// into each other only because v stays inside 0..1.
					tex->SetRepeat(TextureRepeat::Repeat, TextureRepeat::Repeat);
				}
			}
			std::shared_ptr<GenericShaderMaterial> mat = std::make_shared<GenericShaderMaterial>(
				ShaderUsage::Color | ShaderUsage::VertexColor | (tex ? ShaderUsage::Texture : 0));
			mat->SetColor(Vec4(1.f, 1.f, 1.f, 1.f));
			if (tex) mat->SetColorMap(tex);
			mat->EnableBlending();
			mat->BlendingEquation(BlendEq::Add);
			if (additive) mat->BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One);
			else mat->BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
			mat->SetTransparencyFlag(true);
			mat->SetCullFace(CullFace::DoubleSided);
			cache[key] = mat;
			return mat;
		}
	}

	uint32 Line2D::LiveCount() { const int n = gLiveLine2D.load(); return n > 0 ? (uint32)n : 0; }

	Line2D::Line2D()
		: closed(false), widthStart(0.1f), widthEnd(0.1f), colorStart(1.f, 1.f, 1.f, 1.f), colorEnd(1.f, 1.f, 1.f, 1.f),
		additive(false), sortOrder(0), trail(false), emitting(true), lifetime(0.5f), minDistance(0.1f),
		lastLaid(0.f, 0.f), haveLastLaid(false),
		lastTime(0.0), haveLastTime(false), dirty(true), built(false), builtAdditive(false)
	{
		gLiveLine2D++;
		std::lock_guard<std::mutex> lock(LinesLock());
		Lines().push_back(this);
	}

	Line2D::~Line2D()
	{
		gLiveLine2D--;
		std::lock_guard<std::mutex> lock(LinesLock());
		std::vector<Line2D*> &all = Lines();
		all.erase(std::remove(all.begin(), all.end(), this), all.end());
	}

	void Line2D::SetPoints(const std::vector<Vec2> &pts) { points = pts; dirty = true; }
	void Line2D::AddPoint(const Vec2 &p) { points.push_back(p); dirty = true; }
	void Line2D::SetPoint(const uint32 index, const Vec2 &p) { if (index < points.size()) { points[index] = p; dirty = true; } }
	void Line2D::ClearPoints() { if (!points.empty()) { points.clear(); dirty = true; } }
	void Line2D::SetClosed(const bool c) { if (c != closed) { closed = c; dirty = true; } }
	void Line2D::SetWidth(const f32 start, const f32 end)
	{
		const f32 a = start > 0.f ? start : 0.f, b = end > 0.f ? end : 0.f;
		if (a != widthStart || b != widthEnd) { widthStart = a; widthEnd = b; dirty = true; }
	}
	void Line2D::SetColors(const Vec4 &start, const Vec4 &end) { colorStart = start; colorEnd = end; dirty = true; }
	void Line2D::SetAdditive(const bool a) { if (a != additive) { additive = a; dirty = true; } }
	void Line2D::SetSortOrder(const int32 order) { if (order != sortOrder) { sortOrder = order; dirty = true; } }
	void Line2D::SetTexture(const std::string &resolvedPath, const std::string &authoredPath)
	{
		if (resolvedPath == textureResolved && authoredPath == texturePath) return;
		textureResolved = resolvedPath;
		texturePath = resolvedPath.empty() ? std::string() : authoredPath;
		dirty = true;
	}
	void Line2D::SetTextureRepeat(const f32 repeatEvery)
	{
		const f32 v = repeatEvery > 0.f ? repeatEvery : 0.f;
		if (v != textureRepeat) { textureRepeat = v; dirty = true; }
	}
	void Line2D::SetTextureNearest(const bool n) { if (n != textureNearest) { textureNearest = n; dirty = true; } }
	void Line2D::SetTrail(const bool on) { if (on != trail) { trail = on; trailPoints.clear(); haveLastTime = false; haveLastLaid = false; dirty = true; } }
	void Line2D::SetTrailLifetime(const f32 seconds) { lifetime = seconds > 0.01f ? seconds : 0.01f; }
	void Line2D::SetTrailMinDistance(const f32 d) { minDistance = d > 0.001f ? d : 0.001f; }
	void Line2D::ClearTrail() { haveLastLaid = false; if (!trailPoints.empty()) { trailPoints.clear(); dirty = true; } }

	void Line2D::Update(const f64 time)
	{
		if (!trail || !Owner) return;
		f32 dt = 0.f;
		if (haveLastTime)
		{
			const f64 d = time - lastTime;
			if (d > 0.0) dt = (f32)(d > 0.25 ? 0.25 : d);
		}
		lastTime = time;
		haveLastTime = true;
		// The editor outside play: nothing is moving of its own accord, and
		// dragging an object about should not smear a trail behind it.
		if (!Sprite2D::ClocksRunning()) return;

		for (size_t i = 0; i < trailPoints.size(); i++) trailPoints[i].age += dt;
		while (!trailPoints.empty() && trailPoints.back().age >= lifetime) trailPoints.pop_back();

		const Vec3 wp = Owner->GetWorldPosition();
		const Vec2 here(wp.x, wp.y);
		if (emitting)
		{
			// A point is left behind each time the object has gone the
			// minimum distance from the last one. Measured from where it was
			// when it started (or was last cleared), so one that is standing
			// still lays nothing and has no trail.
			if (!haveLastLaid) { lastLaid = here; haveLastLaid = true; }
			const f32 dx = here.x - lastLaid.x, dy = here.y - lastLaid.y;
			if (dx * dx + dy * dy >= minDistance * minDistance)
			{
				TrailPoint p; p.world = lastLaid; p.age = 0.f;
				// The first of a new trail starts where the object set off from.
				if (trailPoints.empty()) trailPoints.insert(trailPoints.begin(), p);
				p.world = here;
				trailPoints.insert(trailPoints.begin(), p);
				lastLaid = here;
			}
		}
		dirty = true;
	}

	void Line2D::BuildMesh(const std::vector<Vec2> &pts, const bool closed, const f32 widthStart, const f32 widthEnd,
		const Vec4 &colorStart, const Vec4 &colorEnd, const std::vector<f32>* ages, SpriteMesh2D &out,
		const f32 repeatEvery)
	{
		const size_t n = pts.size();
		if (n < 2) return;
		const bool loop = closed && n >= 3;

		// How far along each point is, 0 at the start to 1 at the end, by
		// length - so the taper is even however unevenly the points fall.
		std::vector<f32> along(n, 0.f);
		f32 total = 0.f;
		for (size_t i = 1; i < n; i++)
		{
			const f32 dx = pts[i].x - pts[i - 1].x, dy = pts[i].y - pts[i - 1].y;
			total += sqrtf(dx * dx + dy * dy);
			along[i] = total;
		}
		f32 whole = total;
		if (loop)
		{
			const f32 dx = pts[0].x - pts[n - 1].x, dy = pts[0].y - pts[n - 1].y;
			whole += sqrtf(dx * dx + dy * dy);
		}
		if (!(whole > 1e-9f)) return;

		// At each point, the two edges of the ribbon: half the width out
		// along the direction that halves the corner, pushed out so the
		// ribbon keeps its width round the bend - but only so far, or a
		// hairpin grows a spike.
		std::vector<Vec2> left(n), right(n);
		std::vector<Vec4> col(n);
		for (size_t i = 0; i < n; i++)
		{
			const bool hasPrev = loop || i > 0, hasNext = loop || i + 1 < n;
			const Vec2 &p = pts[i];
			const Vec2 &a = pts[(i + n - 1) % n], &b = pts[(i + 1) % n];
			Vec2 d0(0.f, 0.f), d1(0.f, 0.f);
			if (hasPrev) { d0 = Vec2(p.x - a.x, p.y - a.y); const f32 l = sqrtf(d0.x * d0.x + d0.y * d0.y); if (l > 1e-9f) d0 = Vec2(d0.x / l, d0.y / l); else d0 = Vec2(0.f, 0.f); }
			if (hasNext) { d1 = Vec2(b.x - p.x, b.y - p.y); const f32 l = sqrtf(d1.x * d1.x + d1.y * d1.y); if (l > 1e-9f) d1 = Vec2(d1.x / l, d1.y / l); else d1 = Vec2(0.f, 0.f); }
			if (d0.x == 0.f && d0.y == 0.f) d0 = d1;
			if (d1.x == 0.f && d1.y == 0.f) d1 = d0;
			Vec2 nrm(-(d0.y + d1.y), d0.x + d1.x);
			f32 nl = sqrtf(nrm.x * nrm.x + nrm.y * nrm.y);
			f32 miter = 1.f;
			if (nl > 1e-6f)
			{
				nrm = Vec2(nrm.x / nl, nrm.y / nl);
				const f32 c = nrm.x * -d0.y + nrm.y * d0.x;        // cosine of half the turn
				miter = c > 0.34f ? 1.f / c : 3.f;
			}
			else nrm = Vec2(-d0.y, d0.x);                           // doubled straight back

			const f32 t = along[i] / whole;
			f32 w = widthStart + (widthEnd - widthStart) * t;
			Vec4 c(colorStart.x + (colorEnd.x - colorStart.x) * t, colorStart.y + (colorEnd.y - colorStart.y) * t,
				colorStart.z + (colorEnd.z - colorStart.z) * t, colorStart.w + (colorEnd.w - colorStart.w) * t);
			if (ages && i < ages->size())
			{
				const f32 life = 1.f - std::max(0.f, std::min((*ages)[i], 1.f));
				c.w *= life;
			}
			const f32 h = w * 0.5f * miter;
			left[i] = Vec2(p.x + nrm.x * h, p.y + nrm.y * h);
			right[i] = Vec2(p.x - nrm.x * h, p.y - nrm.y * h);
			col[i] = c;
		}

		const size_t segments = loop ? n : n - 1;
		for (size_t i = 0; i < segments; i++)
		{
			const size_t j = (i + 1) % n;
			// (a repeating image is measured in distance, so its copies are
			// the same size on a long line as on a short one)
			const f32 per = repeatEvery > 0.f ? repeatEvery : whole;
			const f32 u0 = along[i] / per, u1 = ((j == 0) ? whole : along[j]) / per;
			const __INDEX_C_TYPE__ base = (__INDEX_C_TYPE__)out.vertex.size();
			out.vertex.push_back(Vec3(right[i].x, right[i].y, 0.f)); out.texcoord.push_back(Vec2(u0, 0.f)); out.color.push_back(col[i]);
			out.vertex.push_back(Vec3(right[j].x, right[j].y, 0.f)); out.texcoord.push_back(Vec2(u1, 0.f)); out.color.push_back(col[j]);
			out.vertex.push_back(Vec3(left[j].x, left[j].y, 0.f));   out.texcoord.push_back(Vec2(u1, 1.f)); out.color.push_back(col[j]);
			out.vertex.push_back(Vec3(left[i].x, left[i].y, 0.f));   out.texcoord.push_back(Vec2(u0, 1.f)); out.color.push_back(col[i]);
			for (int k = 0; k < 4; k++)
			{
				out.normal.push_back(Vec3(0.f, 0.f, 1.f));
				out.tangent.push_back(Vec3(1.f, 0.f, 0.f));
				out.bitangent.push_back(Vec3(0.f, 1.f, 0.f));
			}
			out.index.push_back(base + 0); out.index.push_back(base + 1); out.index.push_back(base + 2);
			out.index.push_back(base + 2); out.index.push_back(base + 3); out.index.push_back(base + 0);
		}
		if (!out.vertex.empty())
		{
			out.minBounds = out.maxBounds = out.vertex[0];
			for (size_t i = 1; i < out.vertex.size(); i++)
			{
				const Vec3 &v = out.vertex[i];
				if (v.x < out.minBounds.x) out.minBounds.x = v.x;
				if (v.y < out.minBounds.y) out.minBounds.y = v.y;
				if (v.x > out.maxBounds.x) out.maxBounds.x = v.x;
				if (v.y > out.maxBounds.y) out.maxBounds.y = v.y;
			}
		}
	}

	RenderingComponent* Line2D::FindSibling() const
	{
		if (!Owner) return NULL;
		const std::vector<std::shared_ptr<IComponent> > &comps = Owner->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
			if (comps[i] && comps[i]->GetComponentType() == ComponentType::RenderingComponent)
				return static_cast<RenderingComponent*>(comps[i].get());
		return NULL;
	}

	std::shared_ptr<IMaterial> Line2D::Material()
	{
		const std::string key = std::string(additive ? "add|" : "alpha|") + (textureNearest ? "n|" : "l|") + textureResolved;
		if (!material || builtKey != key)
		{
			material = SharedLineMaterial(key, additive, textureResolved, textureNearest);
			builtKey = key;
			builtAdditive = additive;
		}
		return material;
	}

	void Line2D::Rebuild()
	{
		dirty = false;
		if (!Owner) return;

		SpriteMesh2D mesh;
		if (trail)
		{
			// A trail is where the object WAS, in the world; its quads are
			// in the object's own space like any mesh, so each point is
			// brought back into it as the object stands now.
			if (!trailPoints.empty())
			{
				const Matrix inv = Owner->GetWorldTransformation().Inverse();
				std::vector<Vec2> pts;
				std::vector<f32> ages;
				// The head: the object itself, which in its own space is the
				// origin. Left off once it has stopped emitting - the trail
				// then ends where the last point was laid, and fades there.
				if (emitting) { pts.push_back(Vec2(0.f, 0.f)); ages.push_back(0.f); }
				for (size_t i = 0; i < trailPoints.size(); i++)
				{
					const Vec2 &w = trailPoints[i].world;
					pts.push_back(Vec2(inv.m[0] * w.x + inv.m[4] * w.y + inv.m[12], inv.m[1] * w.x + inv.m[5] * w.y + inv.m[13]));
					ages.push_back(trailPoints[i].age / lifetime);
				}
				BuildMesh(pts, false, widthStart, widthEnd, colorStart, colorEnd, &ages, mesh, textureRepeat);
			}
		}
		else BuildMesh(points, closed, widthStart, widthEnd, colorStart, colorEnd, NULL, mesh, textureRepeat);

		RenderingComponent* rc = FindSibling();
		if (mesh.vertex.empty())
		{
			// Nothing to draw: an empty geometry is a crash, not an empty
			// draw, so the mesh that exists is switched off instead.
			if (rc && built && rc->IsActive()) { rc->Disable(); meshOffByUs = true; }
			return;
		}
		std::shared_ptr<IMaterial> mat = Material();
		if (!rc)
		{
			std::shared_ptr<RenderingComponent> made = std::make_shared<RenderingComponent>(
				std::static_pointer_cast<Renderable>(std::make_shared<Plane>(1.f, 1.f)), mat);
			made->DisableCastShadows();
			Owner->Add(std::static_pointer_cast<IComponent>(made));
			rc = made.get();
			built = false;
		}
		Renderable* r = rc->GetRenderable();
		const bool inPlace = built && r && r->Geometries.size() == 1 && rc->GetMeshes().size() == 1
			&& rc->GetMeshes()[0]->Material == mat;
		if (inPlace) static_cast<SpriteGeometry2D*>(r->Geometries[0])->Fill(mesh);
		else
		{
			std::vector<std::shared_ptr<IMaterial> > mats;
			mats.push_back(mat);
			// Not cull-tested: a trail's extent changes every frame, and
			// keeping the owner's box current would be re-registering it.
			rc->DisableCullTest();
			rc->AdoptGeneratedRenderable(std::make_shared<LineRenderable>(mesh), mats);
		}
		rc->SetSortOrder(sortOrder);
		const bool hidden = Layer2D::HiddenAbove(Owner);
		// Only what it switched off itself is switched back on: the mesh is
		// a RenderingComponent like any other, and a game may have its own
		// reasons to have turned it off.
		if (hidden) { if (rc->IsActive()) { rc->Disable(); meshOffByUs = true; } }
		else if (meshOffByUs) { if (!rc->IsActive()) rc->Enable(); meshOffByUs = false; }
		built = true;
	}

	void Line2D::FlushScene(SceneGraph* scene)
	{
		if (gLiveLine2D.load() <= 0) return;
		std::vector<Line2D*> all;
		{
			std::lock_guard<std::mutex> lock(LinesLock());
			all = Lines();
		}
		for (size_t i = 0; i < all.size(); i++)
		{
			GameObject* o = all[i]->GetOwner();
			if (!o || o->GetOwningScene() != scene) continue;
			// A trail's points are the world's; its quads are the object's.
			// The object moving is therefore a change even on a frame that
			// laid nothing down.
			if (all[i]->dirty || (all[i]->trail && !all[i]->trailPoints.empty())) all[i]->Rebuild();
		}
	}

};
