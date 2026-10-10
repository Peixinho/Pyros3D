//============================================================================
// Name        : SceneGraph.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SceneGraph
//============================================================================

#include <cstdio>
#include <cctype>
#include <map>
#include <functional>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <string.h>
#include <algorithm>
#include <unordered_set>
#include <cstring>
#include <set>
#include <string>
#include <typeinfo>
#include <cmath>
#include <cstdlib>
#include <chrono>

namespace p3d {

	SceneGraph::SceneGraph()
	{
		echo("TRACE: Scene Created");
	}

	void SceneGraph::Add(const std::shared_ptr<GameObject> &GO)
	{
		if (GO) GO->Wake();
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

				echo("TRACE: GameObject Added to Scene");

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
		if (!found) echo("TRACE: GameObject Not Found in Scene");
		else
		{
			echo("TRACE: GameObject Removed from Scene");
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

	SceneGraph::~SceneGraph()
	{
		// (nothing that was being watched is left pointing at this scene)
		for (std::map<std::string, NameWatch>::iterator w = nameWatches.begin(); w != nameWatches.end(); ++w)
			for (size_t i = 0; i < w->second.objects.size(); i++) w->second.objects[i]->_WatchScene = NULL;
		nameWatches.clear();
		// Deliberately not RemoveAll(): that routes through Remove(), which
		// echoes a line per object and re-searches the lists each time. The
		// work that actually matters here is the same either way - unregister
		// each subtree while `this` is still a valid scene to unregister
		// from, then drop the back-pointer.
		//
		// UnregisterComponentsTree() is idempotent (every component checks
		// its own Registered flag), so an object sitting in more than one of
		// these lists costs a second walk and nothing else.
		std::vector<std::shared_ptr<GameObject>>* lists[4] = {
			&_GameObjectListDynamic, &_GameObjectListStaticPrevious,
			&_GameObjectListStaticAfter, &_GameObjectListALL };
		for (int l = 0; l < 4; l++)
			for (std::vector<std::shared_ptr<GameObject>>::iterator i = lists[l]->begin(); i != lists[l]->end(); i++)
				if (*i)
				{
					(*i)->UnregisterComponentsTree(this);
					(*i)->Scene = NULL;
				}

		for (int l = 0; l < 4; l++)
			lists[l]->clear();
	}

	void SceneGraph::RemoveAll()
	{
		// Copy first - Remove() mutates _GameObjectListALL, so iterating
		// the live member while erasing from it would invalidate iterators.
		std::vector<std::shared_ptr<GameObject>> all = _GameObjectListALL;
		for (std::vector<std::shared_ptr<GameObject>>::iterator i = all.begin(); i != all.end(); i++)
			Remove(*i);
	}

	void SceneGraph::UpdateObjectTree(GameObject* go, bool callUpdate, bool parentMoved)
	{
		if (!go) return;

		// Asleep: nothing in this subtree has moved, changed or had anything to
		// update for a few frames, so it is not walked - a scene is mostly
		// things that stand still, and walking all of it was a tenth of the
		// frame. What it would have added to the scene's bounds is kept on it.
		// Anything that changes an object wakes it (GameObject::Wake), and one
		// that somehow was not woken is looked at again within kLongestSleep
		// frames regardless.
		static const uint8_t kIdleBeforeSleep = 3, kLongestSleep = 40;
		bool sweep = false;
		if (!parentMoved && !go->_SubtreeAwake)
		{
			if (++go->_SleptFrames < kLongestSleep)
			{
				sleepingThisUpdate++;
				GrowBounds(go->_TreeMin, go->_TreeMax);
				return;
			}
			// the look that is taken regardless: this and all under it
			sweep = true;
		}
		const bool wasAsleep = !go->_SubtreeAwake;
		go->_SubtreeAwake = true;

		// Something that arrived by streaming is brought in a few
		// milliseconds a frame: once this frame's share is spent, an object
		// whose components are still to be registered - and so everything
		// under it - is left exactly as it is until the next frame. It stays
		// awake, so the walk comes back to it.
		const bool outerStreamed = inStreamedSubtree;
		const uint32 deferredBefore = streamedDeferred;
		if (go->_StreamedIn) inStreamedSubtree = true;
		const bool budgeted = inStreamedSubtree && streamedRegistrationBudgetMs > 0.0 && go->_ComponentsChanged;
		// (waiting its turn: it is still put where it belongs - a script may
		// ask where a thing in a cell that has just arrived is - but nothing
		// of it is registered or updated yet)
		const bool waiting = budgeted && streamedRegistrationMs >= streamedRegistrationBudgetMs;
		if (waiting) streamedDeferred++;
		visitedThisUpdate++;
		GameObject::NoteForDraw(go);
		{	// PYROS_WALK_TRACE=1: what the scene walks, and why each is awake - every 300000 walked
			static const bool trace = std::getenv("PYROS_WALK_TRACE") != NULL;
			if (trace)
			{
				static std::map<std::string, uint32> seen; static uint32 calls = 0;
				std::string n = go->GetName(); while (!n.empty() && (isdigit((unsigned char)n.back()) || n.back() == '_' || n.back() == ' ' || n.back() == '(' || n.back() == ')')) n.pop_back();
				if (n.size() > 22) n.resize(22);
				std::string why = sweep ? " [sweep]" : (go->_IsDirty ? " [moved]" : "");
				if (why.empty())
				{
					const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
					for (size_t c = 0; c < cs.size() && why.empty(); c++) if (cs[c] && cs[c]->NeedsUpdate()) { why = std::string(" [") + typeid(*cs[c]).name() + "]"; }
					if (why.empty()) why = go->_IdleFrames < 3 ? " [woken]" : " [child awake]";
				}
				seen[n + why]++;
				if (++calls % 300000 == 0)
				{
					std::vector<std::pair<uint32, std::string> > top;
					for (std::map<std::string, uint32>::iterator i = seen.begin(); i != seen.end(); ++i) top.push_back(std::make_pair(i->second, i->first));
					std::sort(top.rbegin(), top.rend());
					fprintf(stderr, "[WALK] of 300000 walked:");
					for (size_t k = 0; k < top.size() && k < 60; k++) fprintf(stderr, "|%s=%u", top[k].second.c_str(), top[k].first);
					fprintf(stderr, "\n");
					seen.clear();
				}
			}
		}

		const bool wasDirty = go->_IsDirty;
		const bool componentsChanged = go->_ComponentsChanged;
		if (!waiting)
		{
			if (callUpdate) go->Update(timer);
			if (budgeted)
			{
				const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
				go->RegisterComponents(this);
				const f64 took = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count();
				streamedRegistrationMs += took;
				// PYROS_STREAM_TRACE=1: anything whose registration alone is over
				// 3 ms is named - one object can be most of a cell's cost.
				static const bool trace = std::getenv("PYROS_STREAM_TRACE") != NULL;
				if (trace && took > 3.0)
				{
					std::string kinds;
					const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
					for (size_t i = 0; i < cs.size(); i++) if (cs[i]) { kinds += typeid(*cs[i]).name(); kinds += " "; }
					fprintf(stderr, "[stream] %.1f ms to register %s (%s)\n", took, go->GetName().c_str(), kinds.c_str());
				}
			}
			else
				go->RegisterComponents(this);
			go->UpdateComponents(timer);
		}
		go->InternalUpdate();
		// (InternalUpdate has just put the world matrix it had in _PrvWorldMatrix)
		const bool moved = std::memcmp(go->_PrvWorldMatrix.m, go->_WorldMatrix.m, sizeof(go->_WorldMatrix.m)) != 0;

		Vec3 _min = go->GetBoundingMinValue();
		Vec3 _max = go->GetBoundingMaxValue();
		GrowBounds(_min, _max);

		// Children after the parent, deliberately: InternalUpdate() above has
		// just refreshed this object's world matrix, and a child's transform
		// is relative to it. Copied because a component's Update() may add or
		// remove children while this walk is in progress.
		const std::vector<std::shared_ptr<GameObject>> kids = go->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
			UpdateObjectTree(kids[i].get(), callUpdate, parentMoved || moved || sweep);

		// Busy if it moved, was changed, or has something that updates; idle
		// for a few frames running and it sleeps - once everything under it
		// does. (Read after the children are all done: one of them may have
		// woken another that had already been passed over.)
		bool busy = moved || wasDirty || go->_IsDirty || componentsChanged || go->_ComponentsChanged || go->WantsUpdate();
		if (!busy)
		{
			const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
			for (size_t i = 0; i < comps.size() && !busy; i++)
				if (comps[i] && comps[i]->NeedsUpdate()) busy = true;
		}
		if (busy) go->_IdleFrames = 0;
		else if (go->_IdleFrames < 255) go->_IdleFrames++;

		bool childAwake = false;
		go->_TreeMin = _min; go->_TreeMax = _max;
		const std::vector<std::shared_ptr<GameObject>> &now = go->GetChildren();
		for (size_t i = 0; i < now.size(); i++)
		{
			GameObject* k = now[i].get();
			if (!k) continue;
			if (k->_SubtreeAwake) childAwake = true;
			if (k->_TreeMin.x < go->_TreeMin.x) go->_TreeMin.x = k->_TreeMin.x;
			if (k->_TreeMin.y < go->_TreeMin.y) go->_TreeMin.y = k->_TreeMin.y;
			if (k->_TreeMin.z < go->_TreeMin.z) go->_TreeMin.z = k->_TreeMin.z;
			if (k->_TreeMax.x > go->_TreeMax.x) go->_TreeMax.x = k->_TreeMax.x;
			if (k->_TreeMax.y > go->_TreeMax.y) go->_TreeMax.y = k->_TreeMax.y;
			if (k->_TreeMax.z > go->_TreeMax.z) go->_TreeMax.z = k->_TreeMax.z;
		}
		go->_SubtreeAwake = childAwake || go->_IdleFrames < kIdleBeforeSleep;
		// (all of what streamed in under this is in: from here on it is like anything else)
		if (go->_StreamedIn && streamedDeferred == deferredBefore) go->_StreamedIn = false;
		inStreamedSubtree = outerStreamed;
		// (falling asleep, or going back to sleep after a look: the next look
		// regardless is some frames off, and not the same frame for everybody)
		if (!go->_SubtreeAwake) go->_SleptFrames = wasAsleep ? 0 : (uint8_t)((reinterpret_cast<uintptr_t>(go) >> 6) % kLongestSleep);
	}

	void SceneGraph::GrowBounds(const Vec3 &_min, const Vec3 &_max)
	{
		if (_min.x < minBounds.x) minBounds.x = _min.x;
		if (_min.y < minBounds.y) minBounds.y = _min.y;
		if (_min.z < minBounds.z) minBounds.z = _min.z;
		if (_max.x > maxBounds.x) maxBounds.x = _max.x;
		if (_max.y > maxBounds.y) maxBounds.y = _max.y;
		if (_max.z > maxBounds.z) maxBounds.z = _max.z;
	}

	namespace {
		void SettleTree(GameObject* go, const bool parentMoved)
		{
			if (!go) return;
			// (asleep, and nothing above it moved: nothing here is dirty - a
			// dirty object is awake)
			if (!parentMoved && !go->IsAwake()) return;
			const bool moved = go->SettleTransformation(parentMoved);
			const std::vector<std::shared_ptr<GameObject>> &kids = go->GetChildren();
			for (size_t i = 0; i < kids.size(); i++)
				SettleTree(kids[i].get(), moved);
		}
	}

	void SceneGraph::SettleTransforms()
	{
		PYROS_PROFILE_SCOPE("SceneGraph.Settle");
		for (size_t i = 0; i < _GameObjectListDynamic.size(); i++)
		{
			GameObject* go = _GameObjectListDynamic[i].get();
			if (go && go->Scene == this) SettleTree(go, false);
		}
	}

	void SceneGraph::Update(const f64 &Timer)
	{
		PYROS_PROFILE_SCOPE("SceneGraph.Update");

		// (what counts "drawn lately" for skinned meshes posed only when seen)
		RenderingComponent::SeenEpoch++;
		watchReleased.clear();

		// Save Time
		timer = Timer;
		visitedThisUpdate = sleepingThisUpdate = 0;
		streamedRegistrationMs = 0.0; streamedDeferred = 0; inStreamedSubtree = false;

		minBounds = maxBounds = Vec3();

		// (skeletons, on every core, before the walk: see RenderingComponent::AnimateAhead)
		RenderingComponent::AnimateAhead(this, timer);

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
				UpdateObjectTree(go.get(), true, false);
			}
		}

		{
			PYROS_PROFILE_SCOPE("Scene.StaticAfter");
			const std::vector<std::shared_ptr<GameObject>> staticAfterSnapshot = _GameObjectListStaticAfter;
			for (const std::shared_ptr<GameObject> &go : staticAfterSnapshot)
			{
				if (!go || go->Scene != this) continue;
				UpdateObjectTree(go.get(), false, false);
			}
		}

		{
			PYROS_PROFILE_SCOPE("Scene.StaticInit");
			// No i++ in the loop header: erase() already returns the
			// iterator to the next element, and incrementing it as well
			// steps over that element.
			//
			// This processed every OTHER static object per frame. The
			// skipped ones stayed in the list and were picked up on the
			// next Update - again every other one - so a scene of six
			// static objects had three of them registered after the
			// first frame, five after the second, and all six after the
			// third. Nothing looked broken for long, which is why it
			// survived: by the time anyone looked at a running scene it
			// had converged.
			//
			// What it did break is anything that reads the scene ONCE,
			// early. A GI bake at load time extracted half the geometry
			// and lit the room through the missing walls; that was
			// worked around by walking GetAllGameObjectList() instead
			// of the rendering registry, and the real cause is this
			// line.
			for (std::vector<std::shared_ptr<GameObject>>::iterator i = _GameObjectListStaticPrevious.begin(); i != _GameObjectListStaticPrevious.end(); )
			{
				UpdateObjectTree((*i).get(), true, false);

				_GameObjectListStaticAfter.push_back((*i));
				i = _GameObjectListStaticPrevious.erase(i);
			}
		}
		// PYROS_VERIFY_SLEEP=1: every object the walk left asleep is checked for
		// anything the walk would have done to it - a transform not applied, a
		// component waiting to be registered or with something to update. The
		// count is a counter (Scene.SleepViolations) and the first of each kind
		// goes to stderr. A missed wake-up shows here instead of as something
		// that stands still, or is drawn where it was, some frames too long.
		{
			static const bool verify = std::getenv("PYROS_VERIFY_SLEEP") != NULL;
			if (verify)
			{
				uint32 bad = 0;
				std::vector<std::pair<GameObject*, bool> > stack;
				for (size_t l = 0; l < 2; l++)
				{
					const std::vector<std::shared_ptr<GameObject>> &list = l == 0 ? _GameObjectListDynamic : _GameObjectListStaticAfter;
					for (size_t k = 0; k < list.size(); k++) if (list[k] && list[k]->Scene == this) stack.push_back(std::make_pair(list[k].get(), false));
				}
				static std::set<std::string> told;
				while (!stack.empty())
				{
					GameObject* go = stack.back().first;
					const bool under = stack.back().second || !go->_SubtreeAwake;
					stack.pop_back();
					if (under)
					{
						const char* why = NULL;
						std::string what;
						if (go->_IsDirty) why = "dirty";
						else if (go->_ComponentsChanged) why = "components changed";
						else if (go->WantsUpdate()) { why = "wants update"; what = typeid(*go).name(); }
						else
						{
							const Matrix expect = go->_HaveOwner ? (go->_Owner->_WorldMatrix * go->_LocalMatrix) : go->_LocalMatrix;
							for (uint32 m = 0; m < 16 && !why; m++)
								if (fabs(expect.m[m] - go->_WorldMatrix.m[m]) > 1e-4f * (1.f + fabs(expect.m[m]))) why = "world matrix stale";
							const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
							for (size_t c = 0; c < cs.size() && !why; c++)
								if (cs[c] && cs[c]->NeedsUpdate()) { why = "component needs update"; what = typeid(*cs[c]).name(); }
						}
						if (why)
						{
							bad++;
							const std::string key = std::string(why) + "/" + what + "/" + go->GetName();
							if (told.size() < 60 && told.insert(key).second)
								fprintf(stderr, "[sleep] %s asleep but %s %s\n", go->GetName().c_str(), why, what.c_str());
						}
					}
					const std::vector<std::shared_ptr<GameObject>> &kids = go->GetChildren();
					for (size_t k = 0; k < kids.size(); k++) if (kids[k]) stack.push_back(std::make_pair(kids[k].get(), under));
				}
				FrameProfiler::Instance().Counter("Scene.SleepViolations", (f64)bad);
				static uint64 total = 0, frames = 0;
				total += bad; frames++;
				if (frames % 600 == 0) fprintf(stderr, "[sleep] %llu frames checked, %llu violations in all\n", (unsigned long long)frames, (unsigned long long)total);
			}
		}
		FrameProfiler::Instance().Counter("Scene.Walked", (f64)visitedThisUpdate);
		FrameProfiler::Instance().Counter("Scene.Asleep", (f64)sleepingThisUpdate);
		if (streamedDeferred > 0 || streamedRegistrationMs > 0.0)
		{
			FrameProfiler::Instance().Counter("Scene.StreamRegMs", streamedRegistrationMs);
			FrameProfiler::Instance().Counter("Scene.StreamWaiting", (f64)streamedDeferred);
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

	namespace {
		// The pointer that owns `go`: its parent's, or the scene's for a root.
		std::shared_ptr<GameObject> OwnerOf(GameObject* go, const std::vector<std::shared_ptr<GameObject> > &roots)
		{
			if (GameObject* parent = go->GetParent())
			{
				const std::vector<std::shared_ptr<GameObject> > &kids = parent->GetChildren();
				for (size_t i = 0; i < kids.size(); i++) if (kids[i].get() == go) return kids[i];
				return std::shared_ptr<GameObject>();
			}
			for (size_t i = 0; i < roots.size(); i++) if (roots[i].get() == go) return roots[i];
			return std::shared_ptr<GameObject>();
		}
	}

	const std::vector<std::shared_ptr<GameObject> > &SceneGraph::Watch(const std::string &prefix)
	{
		std::map<std::string, NameWatch>::iterator it = nameWatches.find(prefix);
		if (it != nameWatches.end()) return it->second.objects;
		NameWatch &w = nameWatches[prefix];
		std::function<void(GameObject*)> look = [&](GameObject* go)
		{
			if (go->GetName().compare(0, prefix.size(), prefix) == 0 && w.have.find(go) == w.have.end())
			{
				if (std::shared_ptr<GameObject> held = OwnerOf(go, GetAllGameObjectList())) { w.have.insert(go); w.objects.push_back(held); go->_WatchScene = this; }
			}
			const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
			for (size_t i = 0; i < kids.size(); i++) if (kids[i]) look(kids[i].get());
		};
		const std::vector<std::shared_ptr<GameObject> > &roots = GetAllGameObjectList();
		for (size_t i = 0; i < roots.size(); i++)
			if (roots[i] && !roots[i]->GetParent()) look(roots[i].get());
		return w.objects;
	}

	uint32 SceneGraph::WatchedVersion(const std::string &prefix)
	{
		Watch(prefix);
		return nameWatches[prefix].version;
	}

	void SceneGraph::_NoteEntered(GameObject* go)
	{
		if (nameWatches.empty()) return;
		const std::string &name = go->GetName();
		for (std::map<std::string, NameWatch>::iterator w = nameWatches.begin(); w != nameWatches.end(); ++w)
			if (name.compare(0, w->first.size(), w->first) == 0 && w->second.have.find(go) == w->second.have.end())
			{
				std::shared_ptr<GameObject> held = OwnerOf(go, GetAllGameObjectList());
				if (!held) continue;
				w->second.have.insert(go);
				w->second.objects.push_back(held);
				w->second.version++;
				go->_WatchScene = this;
			}
	}

	void SceneGraph::_NoteLeft(GameObject* go)
	{
		if (go->_WatchScene != this) return;
		go->_WatchScene = NULL;
		for (std::map<std::string, NameWatch>::iterator w = nameWatches.begin(); w != nameWatches.end(); ++w)
			if (w->second.have.erase(go))
			{
				std::vector<std::shared_ptr<GameObject> > &list = w->second.objects;
				for (size_t i = 0; i < list.size(); i++) if (list[i].get() == go) { watchReleased.push_back(list[i]); list.erase(list.begin() + i); break; }
				w->second.version++;
			}
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

	static void CollectSubtree(GameObject* go, std::vector<GameObject*> &out, std::unordered_set<GameObject*> &seen)
	{
		if (go == NULL || !seen.insert(go).second) return;
		out.push_back(go);
		const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
			CollectSubtree(kids[i].get(), out, seen);
	}

	void SceneGraph::CollectGameObjectsRecursive(std::vector<GameObject*> &out)
	{
		// The duplicate check used to be a linear search of `out` per node -
		// quadratic, and two 2D systems (Occluder2D, Physics2DWorld) run
		// this every frame even in scenes with nothing 2D in them.
		std::unordered_set<GameObject*> seen(out.begin(), out.end());
		for (size_t i = 0; i < _GameObjectListALL.size(); i++)
			CollectSubtree(_GameObjectListALL[i].get(), out, seen);
	}

};
