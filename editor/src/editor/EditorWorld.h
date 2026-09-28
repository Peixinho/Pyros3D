//============================================================================
// Name        : EditorWorld.h
// Author      : Duarte Peixinho
// Description : Editing a streamed world (SceneMeta::World). The world is
//               too big to load whole, so the editor streams it like the
//               game does - around the camera the viewport is looking
//               through - and makes each loaded cell editable: its objects
//               are adopted into the registry (hierarchy, selection, undo)
//               when it arrives and forgotten when it leaves.
//
//               A cell is dirty when what it would save differs from its
//               file, and a dirty cell is never unloaded - it stays in
//               memory until it is saved, so moving the camera away cannot
//               lose an edit. Saving writes every dirty loaded cell to its
//               own file; the scene file itself never holds cell contents.
//
//               Play mode streams too, around the play camera. Edited cells
//               are saved when Play starts, and stopping drops every cell -
//               whatever play did to them is discarded - so the editor
//               camera streams them back exactly as they are on disk.
//============================================================================

#ifndef EDITORWORLD_H
#define EDITORWORLD_H

#include <Pyros3D/Utils/Streaming/WorldStreamer.h>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace sol { class state; }

class SceneObjects;

namespace p3d { class SceneGraph; class IPhysics; class GameObject; }

class EditorWorld
{
public:
	// adopted/forgetting: the editor's hooks for a cell arriving and
	// leaving - registry, helpers, anything else it keeps per object.
	EditorWorld(p3d::SceneGraph* scene, SceneObjects* objects, const std::string &scenePath,
		const p3d::SceneMeta::World &world, p3d::IPhysics* physics, sol::state* lua,
		const std::function<void(p3d::GameObject*)> &adopted);
	~EditorWorld();

	void Update(const p3d::Vec3 &focus);

	// Every loaded cell's root - left out of the scene file on save.
	std::set<const p3d::GameObject*> LoadedRoots() const;
	bool IsCellRoot(const p3d::GameObject* go) const;

	// Writes every loaded cell whose content changed. False on a failed
	// write (reported); the rest are still written.
	bool SaveCells(std::string &error);
	uint32_t DirtyCount() const;

	void EnterPlay();
	void ExitPlay();

	// Edits a cell's JSON does not show - a terrain tile's sculpted
	// heights live in its heightmap file - so a cell holding one would
	// look clean and unload. Asked on top of the JSON check.
	void SetExtraDirty(const std::function<bool(const p3d::GameObject*)> &fn) { extraDirty = fn; }
	// A cell is about to leave: anything keeping pointers into it lets go.
	void SetOnUnloading(const std::function<void(p3d::GameObject*)> &fn) { onUnloading = fn; }

	const p3d::WorldStreamer &Streamer() const { return *streamer; }

	// Cells by position. A cell's root sits at its corner, unrotated.
	float CellSize() const { return streamer->CellSize(); }
	void CellOf(const p3d::Vec3 &p, int32_t &x, int32_t &z) const;
	p3d::Vec3 CellOrigin(const int32_t x, const int32_t z) const;
	std::shared_ptr<p3d::GameObject> CellRoot(const int32_t x, const int32_t z) const { return streamer->GetCellRoot(x, z); }
	bool HasCell(const int32_t x, const int32_t z) const { return streamer->HasCell(x, z); }
	bool IsLoading(const int32_t x, const int32_t z) const { return streamer->IsLoading(x, z); }
	// Registers a cell whose file the caller has just written.
	void AddCell(const int32_t x, const int32_t z) { streamer->AddCell(x, z); }
	// Far versions (see WorldStreamer).
	void AddFarCell(const int32_t x, const int32_t z) { streamer->AddFarCell(x, z); }
	void SetFarRadius(const float r) { streamer->SetFarRadius(r); }
	uint32_t FarShownCount() const { return streamer->FarShownCount(); }
	// Every cell the world has, loaded or not.
	const std::vector<std::pair<int32_t, int32_t> > &Cells() const { return streamer->GetWorld().cells; }

	// Loaded cells with unsaved edits, recomputed at most once a second -
	// for drawing, where serializing every cell every frame would cost.
	const std::set<const p3d::GameObject*> &DirtyRootsCached();
	void SetRadii(const float load, const float unload) { streamer->SetRadii(load, unload); }

	// Writes `root` (already built, not in the scene) as cell (x, z) of a
	// world whose cells live beside scenePath - what splitting a scene does.
	static bool WriteCell(p3d::GameObject* root, const std::string &cellPath, const std::string &scenePath,
		sol::state* lua, std::string &error);

private:
	bool IsDirty(const p3d::GameObject* root) const;

	p3d::SceneGraph* scene;
	SceneObjects* objects;
	std::string scenePath;
	sol::state* lua;
	std::unique_ptr<p3d::WorldStreamer> streamer;
	// What each loaded cell looked like on disk (as serialized when it
	// arrived, or last saved), to tell edits from nothing.
	std::map<const p3d::GameObject*, std::string> saved;
	bool playing = false;
	std::set<const p3d::GameObject*> knownDirty;
	std::function<bool(const p3d::GameObject*)> extraDirty;
	std::function<void(p3d::GameObject*)> onUnloading;
	std::set<const p3d::GameObject*> dirtyCache;
	double dirtyCacheTime = -1.0;
};

#endif
