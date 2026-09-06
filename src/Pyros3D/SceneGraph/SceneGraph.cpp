//============================================================================
// Name        : SceneGraph.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SceneGraph
//============================================================================

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <string.h>
#include <algorithm>

namespace p3d {

	SceneGraph::SceneGraph()
	{
		echo("SUCCESS: Scene Created");
	}

	void SceneGraph::Add(const std::shared_ptr<GameObject> &GO)
	{
		if (!GO)
		{
			echo("ERROR: Null GameObject");
			return;
		}
		if (GO->Scene == NULL)
		{
			_GameObjectListALL.push_back(GO);

			std::vector<std::shared_ptr<GameObject>> *vec = (GO->IsStatic() ? &_GameObjectListStaticPrevious : &_GameObjectListDynamic);

			bool found = false;
			for (std::vector<std::shared_ptr<GameObject>>::iterator i = vec->begin(); i != vec->end(); i++)
			{
				if ((*i).get() == GO.get())
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				vec->push_back(GO);
				// Set Scene Pointer
				GO->Scene = this;

				// First Update
				GO->Update();
				// Update Transforms Not Using Threads
				GO->InternalUpdate();

				Vec3 _min = GO->GetBoundingMinValue();
				Vec3 _max = GO->GetBoundingMaxValue();

				if (_min.x < minBounds.x) minBounds.x = _min.x;
				if (_min.y < minBounds.y) minBounds.y = _min.y;
				if (_min.z < minBounds.z) minBounds.z = _min.z;
				if (_max.x > maxBounds.x) maxBounds.x = _max.x;
				if (_max.y > maxBounds.y) maxBounds.y = _max.y;
				if (_max.z > maxBounds.z) maxBounds.z = _max.z;

				echo("SUCCESS: GameObject Added to Scene");

			}
			else {
				echo("ERROR: Component Already Added in the Scene");
			}
		}
		else {
			echo("ERROR: GameObject Already on a Scene");
		}
	}

	void SceneGraph::Remove(const std::shared_ptr<GameObject> &GO)
	{
		if (GO) Remove(GO.get());
	}

	void SceneGraph::Remove(GameObject* GO)
	{
		if (!GO)
		{
			echo("ERROR: Null GameObject");
			return;
		}
		std::vector<std::shared_ptr<GameObject>> *vec = (GO->IsStatic() ? &_GameObjectListStaticAfter : &_GameObjectListDynamic);

		bool found = false;
		for (std::vector<std::shared_ptr<GameObject>>::iterator i = vec->begin(); i != vec->end(); i++)
		{
			if ((*i).get() == GO)
			{
				// Unregister Components
				(*i)->UnregisterComponentsTree(this);
				// Erase From List
				vec->erase(i);
				// Erase Scene Pointer
				GO->Scene = NULL;
				// Set Flag
				found = true;
				break;
			}
		}
		if (!found && GO->IsStatic())
		{
			vec = &_GameObjectListStaticPrevious;
			for (std::vector<std::shared_ptr<GameObject>>::iterator i = vec->begin(); i != vec->end(); i++)
			{
				if ((*i).get() == GO)
				{
					// Unregister Components
					(*i)->UnregisterComponentsTree(this);
					// Erase From List
					vec->erase(i);
					// Erase Scene Pointer
					GO->Scene = NULL;
					// Set Flag
					found = true;
					break;
				}
			}
		}
		if (!found) echo("GameObject Not Found in Scene");
		else
		{
			echo("SUCCESS: GameObject Removed from Scene");
			// Was never pruned here before - left GetAllGameObjectList()
			// returning dangling/removed entries after any Remove() call.
			std::vector<std::shared_ptr<GameObject>>::iterator all_it = std::find_if(
				_GameObjectListALL.begin(), _GameObjectListALL.end(),
				[GO](const std::shared_ptr<GameObject> &p) { return p.get() == GO; });
			if (all_it != _GameObjectListALL.end()) _GameObjectListALL.erase(all_it);
		}
	}

	void SceneGraph::DetachRoot(GameObject* GO)
	{
		if (!GO) return;

		// Both the static lists, because an object can be sitting in either
		// depending on whether it has been through an update yet.
		std::vector<std::shared_ptr<GameObject>>* lists[3] = {
			&_GameObjectListDynamic, &_GameObjectListStaticAfter, &_GameObjectListStaticPrevious };
		for (int l = 0; l < 3; l++)
			for (std::vector<std::shared_ptr<GameObject>>::iterator i = lists[l]->begin(); i != lists[l]->end(); i++)
				if ((*i).get() == GO) { lists[l]->erase(i); break; }

		std::vector<std::shared_ptr<GameObject>>::iterator all_it = std::find_if(
			_GameObjectListALL.begin(), _GameObjectListALL.end(),
			[GO](const std::shared_ptr<GameObject> &p) { return p.get() == GO; });
		if (all_it != _GameObjectListALL.end()) _GameObjectListALL.erase(all_it);

		GO->Scene = NULL;
	}

	void SceneGraph::RemoveAll()
	{
		// Copy first - Remove() mutates _GameObjectListALL, so iterating
		// the live member while erasing from it would invalidate iterators.
		std::vector<std::shared_ptr<GameObject>> all = _GameObjectListALL;
		for (std::vector<std::shared_ptr<GameObject>>::iterator i = all.begin(); i != all.end(); i++)
			Remove(*i);
	}

	void SceneGraph::UpdateObjectTree(GameObject* go, bool callUpdate)
	{
		if (!go) return;

		if (callUpdate) go->Update(timer);
		go->RegisterComponents(this);
		go->UpdateComponents(timer);
		go->InternalUpdate();

		Vec3 _min = go->GetBoundingMinValue();
		Vec3 _max = go->GetBoundingMaxValue();
		if (_min.x < minBounds.x) minBounds.x = _min.x;
		if (_min.y < minBounds.y) minBounds.y = _min.y;
		if (_min.z < minBounds.z) minBounds.z = _min.z;
		if (_max.x > maxBounds.x) maxBounds.x = _max.x;
		if (_max.y > maxBounds.y) maxBounds.y = _max.y;
		if (_max.z > maxBounds.z) maxBounds.z = _max.z;

		// Children after the parent, deliberately: InternalUpdate() above has
		// just refreshed this object's world matrix, and a child's transform
		// is relative to it. Copied because a component's Update() may add or
		// remove children while this walk is in progress.
		const std::vector<std::shared_ptr<GameObject>> kids = go->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
			UpdateObjectTree(kids[i].get(), callUpdate);
	}

	void SceneGraph::Update(const f64 &Timer)
	{
		PYROS_PROFILE_SCOPE("SceneGraph.Update");

		// Save Time
		timer = Timer;

		minBounds = maxBounds = Vec3();

		// Snapshot before iterating: Lua (and other) components may
		// scene:add / scene:remove during UpdateComponents, which mutates
		// these vectors and would otherwise invalidate live iterators
		// (e.g. Physics Stress continuous spawn).
		{
			PYROS_PROFILE_SCOPE("Scene.Dynamic");
			const std::vector<std::shared_ptr<GameObject>> dynamicSnapshot = _GameObjectListDynamic;
			for (const std::shared_ptr<GameObject> &go : dynamicSnapshot)
			{
				if (!go || go->Scene != this) continue;
				UpdateObjectTree(go.get(), true);
			}
		}

		{
			PYROS_PROFILE_SCOPE("Scene.StaticAfter");
			const std::vector<std::shared_ptr<GameObject>> staticAfterSnapshot = _GameObjectListStaticAfter;
			for (const std::shared_ptr<GameObject> &go : staticAfterSnapshot)
			{
				if (!go || go->Scene != this) continue;
				UpdateObjectTree(go.get(), false);
			}
		}

		{
			PYROS_PROFILE_SCOPE("Scene.StaticInit");
			for (std::vector<std::shared_ptr<GameObject>>::iterator i = _GameObjectListStaticPrevious.begin(); i != _GameObjectListStaticPrevious.end(); i++)
			{
				UpdateObjectTree((*i).get(), true);

				_GameObjectListStaticAfter.push_back((*i));
				i = _GameObjectListStaticPrevious.erase(i);
				if (i == _GameObjectListStaticPrevious.end()) break;
			}
		}
	}

	const Vec3 &SceneGraph::GetMinBounds() const
	{
		return minBounds;
	}

	const Vec3 &SceneGraph::GetMaxBounds() const
	{
		return maxBounds;
	}

	const f64 &SceneGraph::GetTime() const
	{
		return timer;
	}

	void SceneGraph::AddGameObject(const std::shared_ptr<GameObject> &GO) { Add(GO); }
	void SceneGraph::RemoveGameObject(const std::shared_ptr<GameObject> &GO) { Remove(GO); }
	void SceneGraph::RemoveGameObject(GameObject* GO) { Remove(GO); }

	std::vector<std::shared_ptr<GameObject>> &SceneGraph::GetDynamicGameObjectList()
	{
		return _GameObjectListDynamic;
	}

	std::vector<std::shared_ptr<GameObject>> &SceneGraph::GetStaticGameObjectList()
	{
		return _GameObjectListStaticAfter;
	}

	void SceneGraph::ReorderRoots(const std::vector<GameObject*> &order)
	{
		if (order.empty() || _GameObjectListALL.empty()) return;

		std::vector<std::shared_ptr<GameObject>> reordered;
		reordered.reserve(_GameObjectListALL.size());
		std::vector<bool> taken(_GameObjectListALL.size(), false);

		for (size_t i = 0; i < order.size(); i++)
		{
			for (size_t j = 0; j < _GameObjectListALL.size(); j++)
			{
				if (taken[j] || _GameObjectListALL[j].get() != order[i]) continue;
				reordered.push_back(_GameObjectListALL[j]);
				taken[j] = true;
				break;
			}
		}
		// Whatever the caller did not name - editor furniture, anything added
		// since - keeps the order it already had, after the named ones.
		for (size_t j = 0; j < _GameObjectListALL.size(); j++)
			if (!taken[j]) reordered.push_back(_GameObjectListALL[j]);

		_GameObjectListALL.swap(reordered);
	}

	std::vector<std::shared_ptr<GameObject>> &SceneGraph::GetAllGameObjectList()
	{
		return _GameObjectListALL;
	}

	static void CollectSubtree(GameObject* go, std::vector<GameObject*> &out)
	{
		if (go == NULL) return;
		for (size_t i = 0; i < out.size(); i++)
			if (out[i] == go) return;
		out.push_back(go);
		const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
			CollectSubtree(kids[i].get(), out);
	}

	void SceneGraph::CollectGameObjectsRecursive(std::vector<GameObject*> &out)
	{
		for (size_t i = 0; i < _GameObjectListALL.size(); i++)
			CollectSubtree(_GameObjectListALL[i].get(), out);
	}

};
