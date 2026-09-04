//============================================================================
// Name        : SceneSerializer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Scene save/load to JSON - GameObjects, components, materials
//============================================================================

#ifndef SCENESERIALIZER_H
#define SCENESERIALIZER_H

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Physics/PhysicsEngines/IPhysics.h>
#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <string>
#include <vector>
#include <memory>

// Forward-declared, not #included - sol.hpp is a large single header and
// SceneSerializer.h is a general-purpose header included well outside
// Lua-specific code. Only ever dereferenced inside SceneSerializer.cpp
// (guarded there by #ifdef LUA_BINDINGS), so a pointer-only forward
// declaration is sufficient here.
namespace sol { class state; }

namespace p3d {

	class IMaterial;
	class Texture;
	class Renderable;
	class SkeletonAnimation;
	class TextureAnimation;

	// Precise record of everything one LoadScene() call allocated, so it
	// can be freed exactly (via UnloadScene) with no guessing about
	// sharing/ownership - see VULKAN_ROADMAP.md's demo-launcher work for
	// why: nothing in the engine's type system guarantees a GameObject/
	// component/resource belongs to exactly one Scene or isn't shared
	// elsewhere, so a generic "delete everything currently in this
	// SceneGraph" API would be unsound. This struct only ever contains
	// what SceneSerializer itself constructed for one specific call.
	// `gameObjects` is every GameObject created (both Scene->Add()'d
	// roots and their recursively-created children, flattened) - not
	// just top-level roots, since all of them need deleting even though
	// only roots are ever actually registered with a SceneGraph.
	struct PYROS3D_API LoadedSceneAssets
	{
		// All Stage-1/2 owned types use shared_ptr (SHARED_OWNERSHIP_PLAN.md),
		// including skeleton/texture animations.
		std::vector<std::shared_ptr<GameObject>> gameObjects;
		std::vector<std::shared_ptr<IMaterial>> materials;
		std::vector<std::shared_ptr<Texture>> textures;
		std::vector<std::shared_ptr<Renderable>> renderables;
		std::vector<std::shared_ptr<SkeletonAnimation>> skeletonAnimations;
		std::vector<std::shared_ptr<TextureAnimation>> textureAnimations;
	};

	// Real scene save/load - GameObjects (hierarchy, transform, tags),
	// RenderingComponent (incl. Decal/Text renderables and skeleton/
	// texture animation state), lights, physics (incl. vehicles),
	// particles, generic Lua components, and a deduplicated material
	// pool. See VULKAN_ROADMAP.md's "Scene serialization" sections for
	// the full list of what's in/out of scope - most notably: a
	// LuaComponent only round-trips real scripted behavior if it was
	// built via GameObject:attachScript()/LuaComponent_fromFile() (a
	// .lua file returning a middleclass class) and that class defines
	// serialize()/deserialize() - an anonymous ad-hoc component
	// (on_init/on_update assigned directly) still only round-trips as
	// an existence marker, a live Lua closure fundamentally can't be
	// generically serialized. Materials/textures/models/fonts/
	// animations built from something other than a named file (a raw
	// Shader*, an in-memory texture) have no recoverable source and are
	// skipped with a logged warning rather than silently producing a
	// broken save.
	struct PYROS3D_API SceneMeta
	{
		// Optional scene-level Lua (middleclass class), relative to project/assets
		// root when possible (e.g. "assets/lua/main.lua"). Empty = none.
		std::string mainScript;

		// Flat ambient colour, fed to IRenderer::SetGlobalLight() - only
		// .xyz matter (see PyrosShader.glsl's uAmbientLight.rgb / w is
		// never sampled by any shader). Defaults to IRenderer's own
		// constructor default so a scene saved without ever touching this
		// round-trips to the same look; a scene file predating this field
		// loads the same default too (LoadScene leaves it untouched when
		// the JSON key is absent).
		Vec4 ambientLight = Vec4(0.2f, 0.2f, 0.2f, 0.2f);

		// Environment lighting, the rest of it.
		//
		// ambientIntensity scales ambientLight on its way to
		// IRenderer::SetGlobalLight(), so the colour and how much of it there is
		// are two separate decisions - the same split Unity's Environment
		// Lighting makes between an ambient colour and its intensity multiplier.
		f32 ambientIntensity = 1.f;

		// What the frame clears to, and deliberately NOT the ambient colour.
		// Both used to be 0.2 grey - the editor hardcoded the viewport clear and
		// the ambient default happened to match it exactly - so any surface the
		// lights did not reach came out at precisely the background value and
		// vanished into it. In Deferred that reads as "the renderer draws
		// nothing"; in Forward it merely looks flat. Keeping them apart, with a
		// background darker than any plausible ambient, means unlit geometry is
		// always still a silhouette.
		Vec4 background = Vec4(0.10f, 0.11f, 0.13f, 1.f);

		// Where the ambient comes from: 0 = the flat ambientLight colour above,
		// 1 = a three-band gradient over the surface normal (sky above, ground
		// below, equator around the horizon). Absent from a scene file means 0,
		// so every existing scene keeps the flat colour it was authored with.
		uint32 ambientMode = 0;
		Vec4 ambientSky = Vec4(0.32f, 0.38f, 0.45f, 1.f);
		Vec4 ambientEquator = Vec4(0.20f, 0.20f, 0.20f, 1.f);
		Vec4 ambientGround = Vec4(0.10f, 0.09f, 0.08f, 1.f);

		// A 2D scene: its content is UICanvas trees, not world geometry, so
		// whoever renders it skips the 3D pass entirely and draws only the UI
		// layer. Two uses, and they are the same scene either way - a menu or
		// a whole 2D game played on its own, or a scene shown *over* a running
		// 3D one (see PyrosPlayer::ShowOverlayScene). Absent from a scene file
		// means false, so every existing scene keeps loading as a 3D one.
		bool twoD = false;

		// ---- how a 2D scene is framed -----------------------------------
		// A 2D scene has a viewpoint, not a camera. What is on screen is two
		// numbers - where the view is centred and how much of the world it
		// covers - plus, optionally, what it follows.
		//
		// Modelling that as a GameObject was a 3D concept leaking into 2D:
		// it made you create an object, remember to park it at some positive
		// z looking down -Z, mark it orthographic, set an ortho size, and set
		// it active, all to see a world that has two axes. Worse, none of
		// that lived in the scene file at all - "camera-ness" was recorded in
		// the .editor.json sidecar, keyed by object NAME, so a scene loaded
		// without its sidecar had nothing to render from.
		//
		// Camera GameObjects still work and still take precedence when one is
		// explicitly active; this is what a 2D scene gets for free instead of
		// having to build one.
		struct PYROS3D_API View2D {
			// Off means "this scene is framed by a camera object", which is
			// what every scene written before this field is.
			bool enabled = false;
			// Centre of the view, in world units on the XY plane.
			Vec2 center = Vec2(0.f, 0.f);
			// World units from the centre to the top edge. The width follows
			// from the window's aspect, so the same scene fills any window
			// without the author choosing a resolution.
			f32 halfHeight = 5.f;

			// Name of a GameObject to keep in view. Empty for a fixed view.
			// By NAME rather than by pointer or id because this is scene data
			// that outlives any particular load.
			std::string follow;
			// Seconds for the view to close most of the distance to what it
			// follows. 0 snaps, which is right for a puzzle game and wrong
			// for a platformer.
			f32 followLag = 0.f;
			// Offset from the followed object, so a character can sit low in
			// frame with the level ahead visible.
			Vec2 followOffset = Vec2(0.f, 0.f);
			// Which axes the follow actually moves. A side-scroller wants x
			// only: tracking y as well makes the horizon bob every time the
			// character jumps, and hand-rolling "follow but only in x" is one
			// of the two reasons people go back to moving a camera object
			// themselves.
			bool followX = true;
			bool followY = true;

			// Keeps the view inside the level rather than letting it walk off
			// the end of the artwork when it follows something to the edge.
			bool clamp = false;
			Vec2 clampMin = Vec2(0.f, 0.f);
			Vec2 clampMax = Vec2(0.f, 0.f);

			// ---- one implementation, used by the editor AND the player ----
			// The camera sits far along +Z looking down -Z. Far enough that
			// parallax layers pushed back for draw order stay in front of the
			// near plane; the depth range is linear in an orthographic
			// projection, so a generous one costs no precision.
			static const f32 kCameraZ;
			static const f32 kNear;
			static const f32 kFar;

			// Moves `center` towards `targetWorld` by one frame's worth of
			// followLag. Call only when something is being followed.
			void Track(const Vec3 &targetWorld, const f32 dt);
			// Pulls `center` back inside clampMin/clampMax, honouring the
			// half-extents so the EDGE of the view stops at the bound rather
			// than its centre. No-op when clamp is off.
			void ClampCenter(const f32 aspect);
			// The projection for a viewport of this aspect (width / height).
			Projection MakeProjection(const f32 aspect) const;
			// The camera's world transform.
			Math::Matrix CameraMatrix() const;
		};
		View2D view2D;
	};

	class PYROS3D_API SceneSerializer {

	public:

		// Writes every GameObject currently in `scene` (see
		// SceneGraph::GetAllGameObjectList()) to `filePath` as JSON.
		// `lua`, if non-NULL, is used to serialize named-class
		// LuaComponent behavior (see the class comment) - NULL skips
		// that (existence-only, as before).
		// `meta`, if non-NULL, writes scene-level fields (mainScript, …).
		static bool SaveScene(SceneGraph* scene, const std::string &filePath, sol::state* lua = NULL, const SceneMeta* meta = NULL);

		// Populates `scene` from `filePath` - does NOT clear it first,
		// call SceneGraph::RemoveAll() beforehand if starting fresh is
		// wanted (the common case). `physics`, if non-NULL, is used to
		// reconstruct any Physics components in the file via its real
		// Create* factories - a scene with physics components loaded
		// with physics == NULL skips them (logs a warning per skipped
		// component), everything else still loads. `lua`, if non-NULL,
		// is used to reconstruct named-class LuaComponent behavior.
		// `outAssets`, if non-NULL, is populated with everything this
		// call allocated (see LoadedSceneAssets) - pass it to a later
		// UnloadScene() call to free exactly that, precisely and
		// completely. NULL (the default) preserves prior behavior
		// exactly - nothing is tracked, nothing changes for existing
		// callers.
		// `outMeta`, if non-NULL, receives scene-level fields (mainScript).
		static bool LoadScene(SceneGraph* scene, const std::string &filePath, IPhysics* physics = NULL, sol::state* lua = NULL, LoadedSceneAssets* outAssets = NULL, SceneMeta* outMeta = NULL);

		// LoadScene() with the scene's JSON supplied directly instead of
		// read from disk. `scenePathForAssetRoot` is the path the text
		// *would* have been read from - asset references inside resolve
		// against it exactly as they do for LoadScene(), so this is not
		// merely a convenience: passing the real path is what keeps a
		// scene's models and textures findable.
		//
		// Exists for callers that transform a scene before it reaches the
		// engine. The editor and the player both resolve prefab references
		// that way (see shared/PrefabResolver.h) - the engine deliberately
		// knows nothing about that, and this API says nothing about it
		// either; it is just "load a scene I already have in hand".
		static bool LoadSceneFromText(SceneGraph* scene, const std::string &jsonText, const std::string &scenePathForAssetRoot,
			IPhysics* physics = NULL, sol::state* lua = NULL, LoadedSceneAssets* outAssets = NULL, SceneMeta* outMeta = NULL);

		// Frees exactly what one LoadScene() call recorded into
		// `assets`: detach GameObjects from `scene`, then drop every
		// shared_ptr vector (GOs/components/materials/textures/renderables/
		// skeleton & texture animations). `assets` is non-const so vectors
		// can be cleared. `scene` must be the same SceneGraph the objects
		// were loaded into.
		static void UnloadScene(SceneGraph* scene, LoadedSceneAssets &assets);

		// Captures exactly one GameObject subtree (itself, its full
		// descendant hierarchy, and every component on every node in it,
		// including a materials pool scoped to just what this subtree
		// references) as a JSON string - the same shape SaveScene's
		// per-root entries use, just scoped to one subtree rather than the
		// whole scene. Used by the editor's undo system to snapshot a
		// GameObject before an operation that would otherwise lose it
		// irrecoverably (delete) or need to replay it exactly on redo
		// (add/duplicate). Returns JSON text rather than a json object so
		// this header (like the rest of include/Pyros3D) never needs to
		// expose nlohmann::json in its public API.
		// `scenePathForAssetRoot` should be whatever path SaveScene/LoadScene
		// would be called with for this scene (SceneEditor::GetScenePath()) -
		// it's used the same way those do, to resolve relative asset paths
		// found on materials/models within the subtree; pass an empty string
		// for a scene that has never been saved (matches SaveScene's own
		// behavior in that case).
		static std::string SerializeSubtree(GameObject* root, const std::string &scenePathForAssetRoot, sol::state* lua = NULL);

		// Reconstructs a GameObject subtree previously captured by
		// SerializeSubtree(). Returns the reconstructed root with its
		// children already attached (go->Add()'d), or a null shared_ptr if
		// `subtreeJson` isn't valid JSON. The caller is responsible for
		// scene->Add(result) and re-adopting it into the editor's
		// SceneObjects registry (SceneObjects::Adopt). `outAssets`, if
		// non-NULL, collects everything this call allocated (same contract
		// as LoadScene's outAssets) so a later UnloadScene(scene, assets)
		// call frees it deterministically and GPU-safely - required when an
		// undo/redo command holding this snapshot is discarded (e.g. the
		// undo stack's redo half is cleared by a new edit) before ever
		// being applied to the scene.
		static std::shared_ptr<GameObject> DeserializeSubtree(const std::string &subtreeJson, const std::string &scenePathForAssetRoot,
			IPhysics* physics = NULL, sol::state* lua = NULL, LoadedSceneAssets* outAssets = NULL);
	};

}

#endif /* SCENESERIALIZER_H */
