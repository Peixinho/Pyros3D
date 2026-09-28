//============================================================================
// Name        : EditorWorld.cpp
// Author      : Duarte Peixinho
// Description : See EditorWorld.h.
//============================================================================

#include "EditorWorld.h"
#include "SceneObjects.h"
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Core/Logs/Log.h>

#include <filesystem>
#include <chrono>
#include <cmath>
#include <fstream>

using namespace p3d;

EditorWorld::EditorWorld(SceneGraph* scene, SceneObjects* objects, const std::string &scenePath,
	const SceneMeta::World &world, IPhysics* physics, sol::state* lua,
	const std::function<void(GameObject*)> &adopted)
	: scene(scene), objects(objects), scenePath(scenePath), lua(lua)
{
	streamer.reset(new WorldStreamer(scene, scenePath, world, physics, lua));
	streamer->SetOnCellLoaded([this, adopted](const std::shared_ptr<GameObject> &root) {
		this->objects->Adopt(root.get(), 0, NULL, NULL, true);
		saved[root.get()] = SceneSerializer::SerializeSubtree(root.get(), this->scenePath, this->lua);
		if (adopted) adopted(root.get());
	});
	streamer->SetOnCellUnloading([this](const std::shared_ptr<GameObject> &root) {
		if (onUnloading) onUnloading(root.get());
		const uint32_t id = this->objects->GetSceneObjectID(root.get());
		if (id) this->objects->Forget(id);
		saved.erase(root.get());
		knownDirty.erase(root.get());
	});
	streamer->SetUnloadVeto([this](const std::shared_ptr<GameObject> &root) {
		// During play only edits made before Play count: physics moving
		// things around is not an edit.
		// During play every cell was saved at Play: nothing to protect.
		if (playing) return false;
		// Found dirty once, kept dirty until saved: a cell waiting past the
		// unload radius is asked every frame, and serializing it every frame
		// to learn the same answer would be waste.
		if (knownDirty.count(root.get())) return true;
		if (extraDirty && extraDirty(root.get())) return true;
		if (!IsDirty(root.get())) return false;
		knownDirty.insert(root.get());
		return true;
	});
}

EditorWorld::~EditorWorld()
{
	// The streamer removes its cells from the scene; the registry entries
	// go through the unloading hook.
	streamer.reset();
}

bool EditorWorld::IsDirty(const GameObject* root) const
{
	std::map<const GameObject*, std::string>::const_iterator it = saved.find(root);
	if (it == saved.end()) return false;
	return SceneSerializer::SerializeSubtree(const_cast<GameObject*>(root), scenePath, lua) != it->second;
}

void EditorWorld::Update(const Vec3 &focus)
{
	streamer->Update(focus);
}

std::set<const GameObject*> EditorWorld::LoadedRoots() const
{
	std::set<const GameObject*> out;
	const std::vector<std::shared_ptr<GameObject> > roots = streamer->LoadedRoots();
	for (size_t i = 0; i < roots.size(); i++) out.insert(roots[i].get());
	return out;
}

bool EditorWorld::IsCellRoot(const GameObject* go) const
{
	int32 x, z;
	return go && streamer->FindCell(go, x, z);
}

uint32_t EditorWorld::DirtyCount() const
{
	uint32_t n = 0;
	const std::vector<std::shared_ptr<GameObject> > roots = streamer->LoadedRoots();
	for (size_t i = 0; i < roots.size(); i++) if (IsDirty(roots[i].get())) n++;
	return n;
}

bool EditorWorld::WriteCell(GameObject* root, const std::string &cellPath, const std::string &scenePath,
	sol::state* lua, std::string &error)
{
	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(cellPath).parent_path(), ec);
	const std::string text = SceneSerializer::SerializeSubtree(root, scenePath, lua);
	std::ofstream out(cellPath.c_str(), std::ios::binary | std::ios::trunc);
	if (!out.is_open()) { error = "could not write " + cellPath; return false; }
	out << text;
	return (bool)out;
}

bool EditorWorld::SaveCells(std::string &error)
{
	bool ok = true;
	const std::vector<std::shared_ptr<GameObject> > roots = streamer->LoadedRoots();
	for (size_t i = 0; i < roots.size(); i++)
	{
		GameObject* root = roots[i].get();
		const std::string text = SceneSerializer::SerializeSubtree(root, scenePath, lua);
		if (text == saved[root]) continue;
		int32 x, z;
		if (!streamer->FindCell(root, x, z)) continue;
		std::string err;
		if (!WriteCell(root, streamer->CellPath(x, z), scenePath, lua, err)) { error = err; ok = false; continue; }
		saved[root] = text;
		knownDirty.erase(root);
	}
	return ok;
}

void EditorWorld::EnterPlay()
{
	// Cells come back from their files when play stops, so their files
	// must hold what the user sees now: edited cells are saved first.
	std::string err;
	const uint32_t dirty = DirtyCount();
	if (dirty && SaveCells(err))
		echo("World: saved " + std::to_string(dirty) + " edited cell(s) before Play");
	else if (dirty)
		echo("ERROR: World: could not save edited cells before Play - " + err);
	playing = true;
}

void EditorWorld::ExitPlay()
{
	playing = false;
	// Everything play touched goes back to how it is on disk: every cell is
	// dropped, and the editor camera streams its own back in from file.
	const std::vector<std::shared_ptr<GameObject> > roots = streamer->LoadedRoots();
	for (size_t i = 0; i < roots.size(); i++)
	{
		int32 x, z;
		if (streamer->FindCell(roots[i].get(), x, z)) streamer->UnloadCell(x, z);
	}
}

void EditorWorld::CellOf(const Vec3 &p, int32_t &x, int32_t &z) const
{
	WorldStreamer::CellOf(p, streamer->CellSize(), x, z);
}

Vec3 EditorWorld::CellOrigin(const int32_t x, const int32_t z) const
{
	return Vec3((float)x * streamer->CellSize(), 0.f, (float)z * streamer->CellSize());
}

const std::set<const GameObject*> &EditorWorld::DirtyRootsCached()
{
	const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
	if (dirtyCacheTime >= 0.0 && now - dirtyCacheTime < 1.0) return dirtyCache;
	dirtyCacheTime = now;
	dirtyCache.clear();
	const std::vector<std::shared_ptr<GameObject> > roots = streamer->LoadedRoots();
	for (size_t i = 0; i < roots.size(); i++)
		if (knownDirty.count(roots[i].get()) || IsDirty(roots[i].get()) || (extraDirty && extraDirty(roots[i].get())))
			dirtyCache.insert(roots[i].get());
	return dirtyCache;
}
