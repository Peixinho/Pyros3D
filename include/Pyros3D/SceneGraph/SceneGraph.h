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

		// GetAllGameObjectList() is what was *added* to the scene, and a child
		// attached with GameObject::Add() is not - Add() registers only the
		// object it is handed. That was harmless while scenes were flat, and
		// stopped being harmless when layers arrived: a Layer2D root is a
		// subtree, so every object in a layered scene is a child and any
		// scene-wide system iterating the flat list silently skips all of
		// them. Walks roots and their descendants, deduplicated.
		void CollectGameObjectsRecursive(std::vector<GameObject*> &out);

		// Takes a GameObject out of this scene's ROOT lists without
		// unregistering anything, because the object is not leaving the
		// scene - it is becoming somebody's child, and the traversal will
		// reach it through its parent from now on. Called by
		// GameObject::Add(), which is the only place that transition
		// happens. Remove() is the other half: that one is for an object
		// genuinely leaving, and it does unregister.
		void DetachRoot(GameObject* GO);
		std::vector<std::shared_ptr<GameObject>> &GetStaticGameObjectList();
		std::vector<std::shared_ptr<GameObject>> &GetDynamicGameObjectList();

		const Vec3 &GetMinBounds() const;
		const Vec3 &GetMaxBounds() const;

	private:

		// One object's per-frame step, then the same for its children.
		// Children are not in any of the scene's lists (only roots are), so
		// without this walk their components were never registered and never
		// updated - a child GameObject simply did not render. Runs after the
		// parent's own InternalUpdate() because a child's world transform is
		// relative to the matrix that call has just refreshed.
		void UpdateObjectTree(GameObject* go, bool callUpdate);

	public:

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

		Vec3 minBounds;
		Vec3 maxBounds;
	};

};

#endif	/* SCENEGRAPH_H */
