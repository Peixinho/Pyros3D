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
				(*i)->UnregisterComponents(this);
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
					(*i)->UnregisterComponents(this);
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

	void SceneGraph::RemoveAll()
	{
		// Copy first - Remove() mutates _GameObjectListALL, so iterating
		// the live member while erasing from it would invalidate iterators.
		std::vector<std::shared_ptr<GameObject>> all = _GameObjectListALL;
		for (std::vector<std::shared_ptr<GameObject>>::iterator i = all.begin(); i != all.end(); i++)
			Remove(*i);
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

				go->Update(timer);
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
			}
		}

		{
			PYROS_PROFILE_SCOPE("Scene.StaticAfter");
			const std::vector<std::shared_ptr<GameObject>> staticAfterSnapshot = _GameObjectListStaticAfter;
			for (const std::shared_ptr<GameObject> &go : staticAfterSnapshot)
			{
				if (!go || go->Scene != this) continue;

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
			}
		}

		{
			PYROS_PROFILE_SCOPE("Scene.StaticInit");
			for (std::vector<std::shared_ptr<GameObject>>::iterator i = _GameObjectListStaticPrevious.begin(); i != _GameObjectListStaticPrevious.end(); i++)
			{
				(*i)->Update(timer);
				(*i)->RegisterComponents(this);
				(*i)->UpdateComponents(timer);
				(*i)->InternalUpdate();

				Vec3 _min = (*i)->GetBoundingMinValue();
				Vec3 _max = (*i)->GetBoundingMaxValue();

				if (_min.x < minBounds.x) minBounds.x = _min.x;
				if (_min.y < minBounds.y) minBounds.y = _min.y;
				if (_min.z < minBounds.z) minBounds.z = _min.z;
				if (_max.x > maxBounds.x) maxBounds.x = _max.x;
				if (_max.y > maxBounds.y) maxBounds.y = _max.y;
				if (_max.z > maxBounds.z) maxBounds.z = _max.z;

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

	std::vector<std::shared_ptr<GameObject>> &SceneGraph::GetAllGameObjectList()
	{
		return _GameObjectListALL;
	}

};
