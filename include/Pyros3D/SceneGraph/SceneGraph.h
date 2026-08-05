//============================================================================
// Name        : SceneGraph.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SceneGraph
//============================================================================

#ifndef SCENEGRAPH_H
#define	SCENEGRAPH_H

#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Other/Export.h>
#include <memory>

using namespace p3d::Math;

namespace p3d {

	class PYROS3D_API GameObject;
	// Circular Dependency - defined in Components/IComponent.h
	class PYROS3D_API IComponent;
	// Circular Dependency - defined in Rendering/Components/Rendering/RenderingComponent.h
	class PYROS3D_API RenderingMesh;
	class PYROS3D_API RenderingComponent;

	class PYROS3D_API SceneGraph
	{

	public:

		SceneGraph();

		// Update
		void Update(const f64 &Timer);
		// Add Child to Scene
		void Add(const std::shared_ptr<GameObject> &GO);
		// Remove Child from Scene
		void Remove(const std::shared_ptr<GameObject> &GO);
		void Remove(GameObject* GO);
		// Remove every GameObject currently in the scene (e.g. before
		// loading a new scene into an existing SceneGraph).
		void RemoveAll();
		// Get Time
		const f64 &GetTime() const;

		void AddGameObject(const std::shared_ptr<GameObject> &GO);
		void RemoveGameObject(const std::shared_ptr<GameObject> &GO);
		void RemoveGameObject(GameObject* GO);

		std::vector<std::shared_ptr<GameObject>> &GetAllGameObjectList();
		std::vector<std::shared_ptr<GameObject>> &GetStaticGameObjectList();
		std::vector<std::shared_ptr<GameObject>> &GetDynamicGameObjectList();

		const Vec3 &GetMinBounds() const;
		const Vec3 &GetMaxBounds() const;

		// Rendering bookkeeping - owned by the scene instance instead of a
		// global map keyed by SceneGraph*, so it can't outlive (or collide
		// with a reused address of) the scene it belongs to.
		// Observing raw pointers into components owned by GameObjects.
		std::vector<RenderingMesh*> &GetRenderingMeshes() { return _RenderingMeshes; }
		std::vector<RenderingMesh*> &GetRenderingMeshesSorted() { return _RenderingMeshesSorted; }
		void SetRenderingMeshesSorted(const std::vector<RenderingMesh*> &Sorted) { _RenderingMeshesSorted = Sorted; }
		std::vector<RenderingComponent*> &GetRenderingComponents() { return _RenderingComponents; }
		std::vector<IComponent*> &GetLights() { return _Lights; }

	private:

		// GameObject Dynamic List
		std::vector<std::shared_ptr<GameObject>> _GameObjectListDynamic;
		// GameObject Static Lists
		std::vector<std::shared_ptr<GameObject>> _GameObjectListStaticPrevious;
		std::vector<std::shared_ptr<GameObject>> _GameObjectListStaticAfter;
		// GameObject All List
		std::vector<std::shared_ptr<GameObject>> _GameObjectListALL;

		// Registered Rendering Meshes/Components and Lights for this Scene
		std::vector<RenderingMesh*> _RenderingMeshes;
		std::vector<RenderingMesh*> _RenderingMeshesSorted;
		std::vector<RenderingComponent*> _RenderingComponents;
		std::vector<IComponent*> _Lights;

		// Time
		f64 timer;

		Vec3 minBounds, maxBounds;
	};

};

#endif	/* SCENEGRAPH_H */
