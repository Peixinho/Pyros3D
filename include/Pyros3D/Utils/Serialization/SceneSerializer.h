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

// Forward-declared, not #included - sol.hpp is a large single header and
// SceneSerializer.h is a general-purpose header included well outside
// Lua-specific code. Only ever dereferenced inside SceneSerializer.cpp
// (guarded there by #ifdef LUA_BINDINGS), so a pointer-only forward
// declaration is sufficient here.
namespace sol { class state; }

namespace p3d {

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
	class PYROS3D_API SceneSerializer {

	public:

		// Writes every GameObject currently in `scene` (see
		// SceneGraph::GetAllGameObjectList()) to `filePath` as JSON.
		// `lua`, if non-NULL, is used to serialize named-class
		// LuaComponent behavior (see the class comment) - NULL skips
		// that (existence-only, as before).
		static bool SaveScene(SceneGraph* scene, const std::string &filePath, sol::state* lua = NULL);

		// Populates `scene` from `filePath` - does NOT clear it first,
		// call SceneGraph::RemoveAll() beforehand if starting fresh is
		// wanted (the common case). `physics`, if non-NULL, is used to
		// reconstruct any Physics components in the file via its real
		// Create* factories - a scene with physics components loaded
		// with physics == NULL skips them (logs a warning per skipped
		// component), everything else still loads. `lua`, if non-NULL,
		// is used to reconstruct named-class LuaComponent behavior.
		static bool LoadScene(SceneGraph* scene, const std::string &filePath, IPhysics* physics = NULL, sol::state* lua = NULL);
	};

}

#endif /* SCENESERIALIZER_H */
