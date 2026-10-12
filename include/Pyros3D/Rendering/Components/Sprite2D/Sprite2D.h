//============================================================================
// Name        : Sprite2D.h
// Description : A sprite: one frame of a sprite sheet, drawn at the size its
//               pixels say, with a tint, a flip, and clips to play.
//
//               Before this a "sprite" was a Plane primitive with a material
//               of its own, and an animated one swapped whole textures - one
//               PNG per frame, written beside the sheet. That cost a material
//               and a texture per object, could not flip, could not tint
//               without forking the material, had no idea how big its art
//               was, and could play exactly one animation.
//
//               A Sprite2D draws from a SpriteSheet2D (.p3ds): frames are
//               rectangles of ONE texture, so everything about how it looks -
//               which frame, which way it faces, what colour - is in its four
//               vertices, and every sprite on a sheet shares one material.
//               That sharing is also what lets a SpriteBatch2D draw hundreds
//               of them in one call.
//
//               Like TileMap2D, the component owns the description and the
//               geometry lives on a sibling RenderingComponent, which it
//               makes for itself when the object has none.
//============================================================================

#ifndef SPRITE2D_H
#define SPRITE2D_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Assets/SpriteSheet2D/SpriteSheet2D.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Global.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace p3d {

	class RenderingComponent;
	class IMaterial;
	class Texture;
	class SpriteBatch2D;

	namespace SpriteBlend2D {
		enum {
			Alpha = 0,     // ordinary cut-out art
			Additive = 1,  // glows, sparks, light shafts
			// Art whose colour is already multiplied by its alpha, which is
			// what most atlas packers can write and the only kind that
			// filters cleanly at a cut-out's edge.
			Premultiplied = 2
		};
	}

	namespace SpriteLighting2D {
		enum {
			// The art exactly: no light in the scene changes it.
			Unlit = 0,
			// 2D lighting: distance falloff from each light, no surface
			// angle, shadows from Occluder2D. What a lit 2D scene wants.
			Lit2D = 1,
			// The scene's ordinary lighting, surface angle included - for a
			// sprite standing in a 3D world, which should take the sun the
			// way the ground beside it does.
			Lit3D = 2
		};
	}

	class PYROS3D_API Sprite2D : public IComponent {

	public:

		Sprite2D();
		virtual ~Sprite2D();
		// How many exist anywhere: a scene with none is not searched for them.
		static uint32 LiveCount();

		// --- what it draws from ---------------------------------------------

		// The sheet is copied in. Its image size must already be known (see
		// SpriteSheet2D::SetImageSize) or nothing can be cut from it.
		void SetSheet(const SpriteSheet2D &sheet);
		const SpriteSheet2D &GetSheet() const { return sheet; }

		// What the scene file stores: a .p3ds, or a bare image - which is a
		// sheet of one frame covering the whole picture, so dropping a PNG
		// into a scene needs no asset authored first. Kept with its "assets/"
		// prefix; nothing here reads it back.
		void SetSourcePath(const std::string &p) { sourcePath = p; }
		const std::string &GetSourcePath() const { return sourcePath; }

		// The image, already resolved to something loadable. The scene loader
		// resolves while its asset root is still set and hands the answer
		// over - see TileMap2D::SetResolvedAtlasPath for what goes wrong
		// otherwise. Set AFTER SetSheet, which clears it.
		void SetResolvedImagePath(const std::string &absolute);
		// Where the sheet's normal map is on disk, or empty for none: found
		// the way the image was, so a project opened from anywhere finds
		// both or neither.
		std::string WantedNormalImage() const;
		const std::string &GetResolvedImagePath() const { return resolvedImage; }
		void SetPathResolver(const std::function<std::string(const std::string&)> &r) { resolve = r; }

		// Loads `path` (a .p3ds or an image, already resolved) and makes this
		// sprite draw from it. `imageResolver` turns the sheet's own image
		// path into a loadable one; without it the image is looked for beside
		// the sheet. False, with the reason, when it cannot.
		bool LoadSource(const std::string &resolvedPath,
			const std::function<std::string(const std::string&)> &imageResolver = nullptr,
			std::string* errorOut = NULL);

		// --- which frame ------------------------------------------------------

		// Setting a frame by hand stops whatever clip was playing: the two
		// cannot both decide.
		void SetFrame(const int32 index);
		bool SetFrameByName(const std::string &name);
		int32 GetFrame() const { return frame; }
		const std::string &GetFrameName() const;
		int32 FrameCount() const { return (int32)sheet.frames.size(); }

		// --- how it looks -----------------------------------------------------

		void SetFlipX(const bool f);
		void SetFlipY(const bool f);
		bool GetFlipX() const { return flipX; }
		bool GetFlipY() const { return flipY; }

		// Multiplied over the art, alpha included - so fading a sprite out is
		// SetColor with a falling alpha, and it costs nothing shared: the
		// colour is in the vertices.
		void SetColor(const Vec4 &c);
		const Vec4 &GetColor() const { return color; }
		void SetAlpha(const f32 a);

		void SetDrawMode(const int32 mode);
		int32 GetDrawMode() const { return drawMode; }
		// World size for Sliced and Tiled. Simple ignores it.
		void SetSize(const Vec2 &s);
		const Vec2 &GetSize() const { return size; }

		// 0 = the sheet's own.
		void SetPixelsPerUnit(const f32 ppu);
		f32 GetPixelsPerUnit() const { return pixelsPerUnit; }
		f32 EffectivePixelsPerUnit() const { return pixelsPerUnit > 0.f ? pixelsPerUnit : sheet.pixelsPerUnit; }

		// A pivot of this sprite's own, replacing the frame's: normalised,
		// y up, (0.5, 0) is bottom-centre. ClearPivot() goes back to the
		// sheet's.
		void SetPivot(const Vec2 &p);
		void ClearPivot();
		bool HasPivot() const { return hasPivot; }
		// The pivot in effect - this sprite's own, or its current frame's.
		Vec2 GetPivot() const;

		// How the scene's lights reach it - see SpriteLighting2D. Unlit by
		// default: an unlit sprite is its art, exactly.
		void SetLighting(const int32 mode);
		int32 GetLighting() const { return lighting; }
		// The common case by name: 2D lighting on, or no lighting.
		void SetLit(const bool l) { SetLighting(l ? SpriteLighting2D::Lit2D : SpriteLighting2D::Unlit); }
		// A material of its own counts as lit: what it does with the scene's
		// lights is its to say, so it is given them as a lit sprite is.
		bool IsLit() const { return lighting != SpriteLighting2D::Unlit || customMaterial; }

		// Shown only inside a SpriteMask2D, or only outside one - see that
		// header. `maskName` is the mask's object's name; empty means the
		// nearest mask on this object or above it. With no such mask (or
		// with it disabled) the sprite is drawn whole.
		enum { MaskNone = 0, MaskInside = 1, MaskOutside = 2 };
		void SetMaskInteraction(const int32 mode);
		int32 GetMaskInteraction() const { return maskMode; }
		void SetMaskName(const std::string &name);
		const std::string &GetMaskName() const { return maskName; }

		// A material in place of the one a sprite makes for itself - for
		// what Lighting and Blend cannot say: a dissolve, an outline, a
		// flash, a distortion. The sprite still supplies the quad (its
		// frame's shape and texture coordinates, its tint in the vertex
		// colours); the material supplies everything about how that is
		// shaded, textures included, so one written for sprites samples the
		// sheet's image itself. Lighting and Blend are then not consulted.
		// Sprites sharing one material are still drawn together by a batch.
		// `path` is what a scene file stores; it is not read here.
		void SetCustomMaterial(const std::shared_ptr<IMaterial> &material, const std::string &path);
		void ClearCustomMaterial() { SetCustomMaterial(std::shared_ptr<IMaterial>(), std::string()); }
		const std::string &GetMaterialPath() const { return customMaterialPath; }
		bool HasCustomMaterial() const { return customMaterial != NULL; }

		void SetBlend(const int32 b);
		int32 GetBlend() const { return blend; }

		// Drawn over everything with a lower order, whatever its z - see
		// RenderingComponent::SetSortOrder. Among equals z decides.
		void SetSortOrder(const int32 o);
		int32 GetSortOrder() const { return sortOrder; }

		// The rectangle it covers in the owner's local space, pivot at the
		// origin: (minX, minY, maxX, maxY).
		Vec4 GetLocalRect() const;

		// --- clips --------------------------------------------------------------

		// Starts a clip by name. Asking for the clip that is already playing
		// does nothing unless `restart` - so a controller can call
		// play("run") every frame it is running without the animation
		// sticking on its first frame. False when the sheet has no such clip.
		bool Play(const std::string &clip, const bool restart = false);
		void Stop();
		void Pause(const bool paused = true);
		bool IsPlaying() const { return clip >= 0 && !paused && !finished; }
		bool IsFinished() const { return finished; }
		// Empty when nothing is playing.
		const std::string &GetClip() const;
		bool HasClip(const std::string &name) const { return sheet.FindClip(name) >= 0; }
		std::vector<std::string> ClipNames() const;
		void SetSpeed(const f32 s) { speed = s; }
		f32 GetSpeed() const { return speed; }
		// Seconds into the clip.
		f32 GetClipTime() const { return clipTime; }
		void SetClipTime(const f32 seconds);

		// The clip a scene starts this sprite on. Stored, and played when the
		// sprite first updates.
		void SetAutoPlay(const std::string &clip) { autoPlay = clip; autoPlayPending = !clip.empty(); WakeOwner(); }
		const std::string &GetAutoPlay() const { return autoPlay; }

		// A clip's named events as it passes them, and a non-looping clip
		// reaching its end. Plain std::function fields for the reason
		// Physics2D's are: sol2 converts an assigned Lua function into one.
		std::function<void(const std::string&)> OnEvent;
		std::function<void(const std::string&)> OnFinished;

		// Advances the playing clip by `dt` seconds. Update() calls this with
		// the scene's own clock; an editor previewing one sprite calls it
		// directly.
		void Advance(const f32 dt);

		// Whether Update() advances clips at all. An editor turns this off
		// while nothing is playing, so a scene being laid out holds still.
		static void SetClocksRunning(const bool run);
		// An image a sprite draws from was rewritten on disk (an atlas
		// packed again, a picture painted over): the next sprite to load
		// it reads the file, where it would have been handed the picture
		// some other sprite still holds. Sprites already showing it keep
		// what they have until their source is applied again.
		static void ForgetImage(const std::string &imagePath);
		static bool ClocksRunning();

		// --- geometry -------------------------------------------------------------

		// This sprite's quads, appended to `out`. Pure - no GPU, no sibling -
		// which is the seam the tests and the batcher both use.
		bool BuildMesh(SpriteMesh2D &out, const Matrix* transform = NULL) const;

		// The material every sprite with this image, lighting, filter and
		// blend shares. Loads the texture on first use; NULL when it cannot.
		std::shared_ptr<IMaterial> SharedMaterial();
		// What identifies that material, so a batcher can group sprites
		// without building one to compare.
		std::string MaterialKey() const;

		// Bumped whenever anything a batcher would have to redraw changes.
		uint32 GetRevision() const { return revision; }
		// The same for every sprite there is: unchanged since a batch last
		// looked means no sprite anywhere needs it to look again.
		static uint64 ChangeCount();

		// Drawn by a SpriteBatch2D rather than by its own mesh. Set and
		// cleared by the batch.
		void SetBatched(SpriteBatch2D* b);
		SpriteBatch2D* GetBatch() const { return batch; }

		// Applies pending changes to the sibling RenderingComponent now.
		// FlushScene() does this for every changed sprite of a scene just
		// before the scene is drawn; a tool that needs the picture current
		// before then calls it.
		bool Rebuild();

		// Shown or not, without removing it. Hidden, it draws nothing - by
		// itself or in a batch - and its clip does not play.
		void SetVisible(const bool v);
		bool IsVisible() const { return visible; }

		// Looks again at whether and how it should be drawn - after something
		// outside it changed that (a layer shown).
		void Refresh() { Touch(); }

		// Rebuilds the scene's changed sprites and re-merges its batches.
		// IRenderer::PreRender calls this, so nothing else has to.
		static void FlushScene(SceneGraph* scene);

		virtual void Register(SceneGraph* Scene) {}
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual bool NeedsUpdate() const;
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene) {}

		virtual uint32 GetComponentType() const { return ComponentType::Sprite2D; }

	private:

		RenderingComponent* FindSibling() const;
		std::string WantedImage() const;
		void Touch(const bool material = false);
		SpriteMeshParams2D MeshParams() const;
		void ApplyClipTime(const f32 before, const bool fireEvents);

		SpriteSheet2D sheet;
		std::string sourcePath;
		std::string resolvedImage;
		std::function<std::string(const std::string&)> resolve;

		int32 frame;
		bool flipX, flipY;
		Vec4 color;
		int32 drawMode;
		Vec2 size;
		f32 pixelsPerUnit;
		bool hasPivot;
		Vec2 pivot;
		int32 lighting;
		int32 blend;
		int32 sortOrder;

		int32 clip;
		f32 clipTime;
		f32 speed;
		bool paused;
		bool finished;
		std::string autoPlay;
		bool autoPlayPending;
		f64 lastTime;
		bool haveLastTime;

		// The quads are stale.
		bool meshDirty;
		// The material is stale too (image, lighting, filter or blend moved).
		bool materialDirty;
		// A geometry of this sprite's exists on the sibling.
		bool built;
		// What the built geometry's bounds were, so a change that moves them
		// re-adopts rather than refilling in place.
		Vec4 builtRect;
		uint32 revision;

		std::shared_ptr<IMaterial> material;
		std::string materialFor;
		std::shared_ptr<IMaterial> customMaterial;
		std::string customMaterialPath;
		bool visible = true;
		// Whether it was this sprite that switched its mesh off. Its mesh is
		// a RenderingComponent like any other, and a script may switch that
		// off for itself (it is how things were hidden before there were
		// sprites): the sprite puts back only what it took away, or a torch
		// a game had put out would relight at its next frame of animation.
		bool meshOffByUs = false;
		int32 maskMode = MaskNone;
		std::string maskName;
		// What the mask looked like, from here, when the quads were last
		// cut: the mask, its shape's revision, and where it stood relative
		// to this sprite. Any of them different means cut again.
		const void* maskSeen = NULL;
		uint32 maskSeenRevision = 0;
		f32 maskSeenRelative[16] = { 0 };
		bool inMasked = false;
		// True (and the notes above brought up to date) when they changed.
		bool MaskChanged();

		SpriteBatch2D* batch;
		bool inPending;
	};

};

#endif /* SPRITE2D_H */
