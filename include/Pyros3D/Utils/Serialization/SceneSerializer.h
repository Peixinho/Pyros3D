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

		// Frees exactly what one LoadScene() call recorded into
		// `assets`: detach GameObjects from `scene`, then drop every
		// shared_ptr vector (GOs/components/materials/textures/renderables/
		// skeleton & texture animations). `assets` is non-const so vectors
		// can be cleared. `scene` must be the same SceneGraph the objects
		// were loaded into.
		static void UnloadScene(SceneGraph* scene, LoadedSceneAssets &assets);
	};

}

#endif /* SCENESERIALIZER_H */
