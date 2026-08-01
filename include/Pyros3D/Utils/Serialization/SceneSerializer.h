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

namespace p3d {

	// Real scene save/load - GameObjects (hierarchy, transform, tags),
	// RenderingComponent/lights/physics/particles/generic Lua components,
	// and a deduplicated material pool. See VULKAN_ROADMAP.md's "Scene
	// serialization" section for the full list of what's in/out of scope
	// - most notably: only the *existence* of a LuaComponent round-trips,
	// never its scripted behavior (a live Lua closure can't be
	// generically serialized), and materials/textures/models built from
	// something other than a named file (a raw Shader*, an in-memory
	// texture) have no recoverable source and are skipped with a logged
	// warning rather than silently producing a broken save.
	class PYROS3D_API SceneSerializer {

	public:

		// Writes every GameObject currently in `scene` (see
		// SceneGraph::GetAllGameObjectList()) to `filePath` as JSON.
		static bool SaveScene(SceneGraph* scene, const std::string &filePath);

		// Populates `scene` from `filePath` - does NOT clear it first,
		// call SceneGraph::RemoveAll() beforehand if starting fresh is
		// wanted (the common case). `physics`, if non-NULL, is used to
		// reconstruct any Physics components in the file via its real
		// Create* factories - a scene with physics components loaded
		// with physics == NULL skips them (logs a warning per skipped
		// component), everything else still loads.
		static bool LoadScene(SceneGraph* scene, const std::string &filePath, IPhysics* physics = NULL);
	};

}

#endif /* SCENESERIALIZER_H */
