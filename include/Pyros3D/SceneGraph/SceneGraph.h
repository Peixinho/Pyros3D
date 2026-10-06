//============================================================================
// Name        : SceneGraph.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SceneGraph
//============================================================================

#ifndef SCENEGRAPH_H
#define	SCENEGRAPH_H

#include <string>
#include <map>
#include <unordered_set>
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

		// Unregisters everything still in the scene, so nothing outlives it
		// holding a pointer back to it.
		//
		// GameObject::Scene and RenderingComponent::Scene are both raw
		// SceneGraph pointers, and objects routinely outlive the scene they
		// were in - the editor's SceneObject registry holds a reference to
		// every one of them, and so does any handle a script kept. Without
		// this, closing a scene left those objects pointing at freed memory,
		// and FindScene() hands that pointer straight to Unregister().
		~SceneGraph();

		// Update
		void Update(const f64 &Timer);
		// Whatever has moved since Update() - by a script run after it - put
		// where it now is, before the frame is drawn. See
		// GameObject::SettleTransformation. Cheap when nothing moved.
		void SettleTransforms();
		// The time the last Update() was given: the same all through one
		// frame, which is what makes it a key for work done once a frame.
		f64 GetUpdateTime() const { return timer; }
		// How long a frame may spend registering the components of objects that
		// arrived by streaming (GameObject::SetStreamedIn). What does not fit
		// waits for the next frame: a cell of a few hundred objects - their
		// meshes into the render lists, their bodies into the physics world -
		// was a hundred milliseconds in the frame it arrived. 2 ms by default;
		// 0 registers everything at once.
// Everything in the scene whose name starts with `prefix`, kept up
		// to date as objects come and go - a cell streaming in, a thing a
		// script makes - instead of the scene being walked to look for them.
		// The first call for a prefix looks through the scene once; after
		// that the list is added to and taken from as objects are registered
		// and removed. WatchedVersion() goes up whenever the list changes, so
		// a caller that has dealt with the list need only look again when it
		// has. (By the name an object has when it enters the scene.)
		// (Shared pointers: a caller may keep the list over frames in which a
		// cell unloads - what it holds stays valid, if no longer in the scene.)
		const std::vector<std::shared_ptr<GameObject> > &Watch(const std::string &prefix);
		uint32 WatchedVersion(const std::string &prefix);
		// Where this scene was last looked at from (a renderer says, each time
		// it prepares a view of it), and whether it has been at all: what a
		// script measures "how far from the player's eye" against.
		const Vec3 &GetLastViewPosition() const { return lastViewPosition; }
		bool HasBeenViewed() const { return viewed; }
		void _NoteViewedFrom(const Vec3 &eye) { lastViewPosition = eye; viewed = true; }

		// (GameObject says when it is registered here and when it leaves)
		void _NoteEntered(GameObject* go);
		void _NoteLeft(GameObject* go);

				void SetStreamedRegistrationBudget(const f64 ms) { streamedRegistrationBudgetMs = ms; }
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

		// Permutes the enumeration list (the one GetAllGameObjectList()
		// returns, which is also the order SceneSerializer writes roots in)
		// to match `order`; anything not named keeps its relative position,
		// at the end. Deliberately does NOT touch the static/dynamic lists -
		// those drive update and draw order, and this is about the order the
		// scene is *listed* in, nothing else. Exists so a caller that
		// removes and re-adds a root (undo, or the editor's Stop-time
		// restore) can put it back where it was instead of at the end, which
		// otherwise rewrites the whole roots array of the saved file.
		void ReorderRoots(const std::vector<GameObject*> &order);
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
		void UpdateObjectTree(GameObject* go, bool callUpdate, bool parentMoved);
		void GrowBounds(const Vec3 &_min, const Vec3 &_max);
		// (this Update: objects gone through, and subtrees left asleep)
		uint32 visitedThisUpdate = 0, sleepingThisUpdate = 0;
		// (the walk is inside something that arrived by streaming; and what
		// registering such things has cost this Update, and how many waited)
		bool inStreamedSubtree = false;
Vec3 lastViewPosition; bool viewed = false;
		struct NameWatch { std::vector<std::shared_ptr<GameObject> > objects; std::unordered_set<GameObject*> have; uint32 version = 1; };
		std::map<std::string, NameWatch> nameWatches;
		// (what left a watch is let go of at the next update, not under the
		// feet of whoever is in the middle of removing it)
		std::vector<std::shared_ptr<GameObject> > watchReleased;
				f64 streamedRegistrationMs = 0.0, streamedRegistrationBudgetMs = 2.0;
		uint32 streamedDeferred = 0;

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
