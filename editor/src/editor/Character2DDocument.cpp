//=============================================================================
// Name        : Character2DDocument.cpp
// Description : See the header.
//=============================================================================

#include "Character2DDocument.h"
#include "Character2DPreview.h"

#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/Core/Logs/Log.h>

#include <algorithm>
#include <filesystem>

using namespace p3d;
using namespace p3d::Math;
namespace fs = std::filesystem;

// ---- undo -------------------------------------------------------------------
// The whole asset, snapshotted as JSON. A character is a few kilobytes and
// there are a dozen kinds of edit; a per-edit inverse would be a dozen more
// things to get right, each exercised only by its own button.
namespace {

class Character2DSnapshotCommand : public IUndoableCommand {
public:
	Character2DSnapshotCommand(Character2DDocument* doc, const std::string& before,
		const std::string& after, const std::string& description)
		: doc_(doc), before_(before), after_(after), desc_(description) {}

	void Undo() override { doc_->Restore(before_); }
	void Redo() override { doc_->Restore(after_); }
	std::string Description() const override { return desc_; }
	size_t MemoryCost() const override { return sizeof(*this) + before_.size() + after_.size(); }

private:
	Character2DDocument* doc_;
	std::string before_, after_, desc_;
};

// Rebuilds a bone's bind matrix from its rest pos/rot. Kept in one place
// because every edit that moves a bone has to do it, and a bind matrix that
// disagrees with the pos/rot beside it produces a rig that saves correctly and
// draws wrong.
void RebuildBindPose(Bone& b)
{
	b.bindPoseMat = Matrix();
	b.bindPoseMat.Translate(b.pos);
	b.bindPoseMat *= b.rot.ConvertToMatrix();
}

} // namespace

Character2DDocument::Character2DDocument()
{
	ConfigureAnimDocument();
}

void Character2DDocument::ConfigureAnimDocument()
{
	// The character owns the undo stack; the animation document's own history
	// would be a second Ctrl+Z that only covered keyframes. That makes wiring
	// onClipsCommitted mandatory - without it every dope-sheet edit (keying,
	// retiming, deleting a key, changing interpolation) would land on NO undo
	// stack, and Ctrl+Z would throw the keyframe work away by restoring an
	// older character snapshot.
	anim.useOwnUndo = false;
	Character2DDocument* self = this;
	anim.onClipsCommitted = [self](const std::vector<Animation>& beforeClips, const std::string& label) {
		// The dope sheet has already applied the edit to anim.clips. Rebuild
		// what the character looked like before it by swapping the old clips
		// back in just long enough to snapshot, so the undo entry is a whole
		// consistent character rather than a clip-only diff the rest of the
		// undo stack could not interleave with.
		std::vector<Animation> after = self->anim.clips;
		self->anim.clips = beforeClips;
		const std::string before = self->Snapshot();
		self->anim.clips = after;
		self->SyncClipsToAsset();
		self->PushEdit(before, label);
	};

	// Rotation only, by default. A cutout limb never changes its offset from
	// the joint it hangs off - that offset is the character's SHAPE, authored
	// in the Bones stage - so position keys on one are noise at best. At worst
	// they are a trap: they pin the bone to wherever it sat when the clip was
	// keyed, so going back and moving a bone's rest position later appears to
	// do nothing whenever a clip is playing.
	//
	// The transport bar still exposes the toggle, for the root bone, which is
	// the one case where translating a bone is the point (a hop, a lunge).
	anim.keyPosition = false;
	anim.keyRotation = true;
}

int Character2DDocument::FindSprite(const std::string& name) const
{
	for (size_t i = 0; i < asset.parts.size(); i++)
		if (asset.parts[i].name == name) return (int)i;
	return -1;
}

// ---- file -------------------------------------------------------------------

bool Character2DDocument::LoadFromFile(const std::string& path, std::string& errOut)
{
	Character2DAsset loaded;
	if (!LoadCharacter2D(path, loaded, &errOut)) return false;

	asset = loaded;
	absolutePath = path;
	displayName = fs::path(path).stem().string();
	dirty = false;
	undo.Clear();
	selectedBone.clear();
	selectedSprite.clear();
	SyncClipsFromAsset();
	TouchRig();
	dirty = false;   // TouchRig marks dirty; a freshly loaded file is not.
	return true;
}

bool Character2DDocument::Save(std::string& errOut)
{
	if (absolutePath.empty()) { errOut = "character has never been saved"; return false; }
	return SaveAs(absolutePath, errOut);
}

bool Character2DDocument::SaveAs(const std::string& path, std::string& errOut)
{
	// The dope sheet edits `anim.clips`; the asset is what gets written.
	SyncClipsToAsset();
	if (!SaveCharacter2D(path, asset, &errOut)) return false;
	absolutePath = path;
	displayName = fs::path(path).stem().string();
	dirty = false;
	return true;
}

// ---- clips <-> the animation document ---------------------------------------

void Character2DDocument::SyncClipsToAsset()
{
	asset.clips = anim.clips;
	clipsRevision++;
}

void Character2DDocument::SyncClipsFromAsset()
{
	anim.clips = asset.clips;
	anim.activeClip = asset.clips.empty() ? -1 : 0;
	anim.playhead = 0.f;
	anim.playing = false;

	clipsRevision++;
}

// ---- snapshot / undo ---------------------------------------------------------

std::string Character2DDocument::Snapshot() const
{
	// The dope sheet edits `anim.clips`; they are part of the character, so a
	// snapshot has to include them or undoing a bone edit would silently
	// revert to whatever clips were last synced.
	const_cast<Character2DDocument*>(this)->asset.clips = anim.clips;
	// Through the FILE serializer, so an undo can never restore something the
	// format cannot express.
	return Character2DToString(asset);
}

void Character2DDocument::Restore(const std::string& snapshot)
{
	if (snapshot.empty()) return;

	Character2DAsset restored;
	std::string err;
	if (!Character2DFromString(snapshot, restored, &err))
	{
		echo("ERROR: Character2D undo could not be restored: " + err);
		return;
	}
	asset = restored;
	SyncClipsFromAsset();
	TouchRig();

	// A selection that no longer exists would leave the panels pointing at a
	// bone that is gone - which is exactly the state an undo of "delete bone"
	// arrives in from the other direction.
	if (!selectedBone.empty() && asset.FindBone(selectedBone) < 0) selectedBone.clear();
	if (!selectedSprite.empty() && FindSprite(selectedSprite) < 0) selectedSprite.clear();
}

void Character2DDocument::BeginInteractiveEdit()
{
	// Re-entrant by design: several widgets in a row can each report being
	// grabbed, and the first grab is the one whose state the whole gesture
	// should be measured against.
	if (interactiveEditActive) return;
	interactiveBefore = Snapshot();
	interactiveEditActive = true;
}

void Character2DDocument::EndInteractiveEdit(const std::string& label)
{
	if (!interactiveEditActive) return;
	interactiveEditActive = false;
	PushEdit(interactiveBefore, label);
	interactiveBefore.clear();
}

void Character2DDocument::PushEdit(const std::string& before, const std::string& label)
{
	// Swallowed while a gesture is open. The edit operations below each push
	// their own entry - which is right when an agent calls one, and wrong when
	// a slider calls one every frame of a drag. The open gesture will push a
	// single entry covering the whole drag when it ends, so this one would only
	// make Ctrl+Z walk back through the slider a frame at a time.
	//
	// Still dirty, though: the edit HAPPENED, it just is not its own undo
	// entry yet, and a document that forgot it had unsaved work would close
	// without asking.
	if (interactiveEditActive) { dirty = true; return; }

	const std::string after = Snapshot();
	if (after == before) return;
	undo.Push(std::unique_ptr<IUndoableCommand>(
		new Character2DSnapshotCommand(this, before, after, label)));
	dirty = true;
}

// ---- bones -------------------------------------------------------------------

bool Character2DDocument::AddBone(const std::string& name, const std::string& parent,
	const Vec2& pos, std::string& errOut)
{
	if (name.empty()) { errOut = "a bone needs a name"; return false; }
	if (asset.FindBone(name) >= 0) { errOut = "a bone called '" + name + "' already exists"; return false; }

	int parentId = -1;
	if (!parent.empty())
	{
		parentId = asset.FindBone(parent);
		if (parentId < 0) { errOut = "parent bone '" + parent + "' not found"; return false; }
	}

	const std::string before = Snapshot();

	Bone b;
	b.name = name;
	b.self = (int32)asset.bones.size();
	b.parent = (int32)parentId;
	b.pos = Vec3(pos.x, pos.y, 0.f);
	b.rot = Quaternion();
	b.scale = Vec3(1.f, 1.f, 1.f);
	b.skinned = false;
	RebuildBindPose(b);
	// Appending keeps the "parent before child" invariant for free: a new
	// bone's parent already exists, so it is already earlier in the array.
	asset.bones.push_back(b);

	PushEdit(before, "Add Bone '" + name + "'");
	TouchRig();
	selectedBone = name;
	return true;
}

bool Character2DDocument::RemoveBone(const std::string& name, std::string& errOut)
{
	const int id = asset.FindBone(name);
	if (id < 0) { errOut = "bone '" + name + "' not found"; return false; }

	const std::string before = Snapshot();

	// The bone and everything under it. Collected by walking down rather than
	// by checking parents once: a grandchild's parent is a child, not this
	// bone, and a single pass would leave it behind pointing at a bone that no
	// longer exists.
	std::vector<bool> doomed(asset.bones.size(), false);
	doomed[id] = true;
	for (size_t i = 0; i < asset.bones.size(); i++)
	{
		const int32 p = asset.bones[i].parent;
		if (p >= 0 && (size_t)p < doomed.size() && doomed[p]) doomed[i] = true;
	}

	std::vector<std::string> removedNames;
	for (size_t i = 0; i < asset.bones.size(); i++)
		if (doomed[i]) removedNames.push_back(asset.bones[i].name);

	// Rebuild by name, then re-resolve parents - the whole reason the file
	// stores parents by name. Doing it by index would mean fixing up every
	// bone after each removal.
	std::vector<Bone> kept;
	std::vector<std::string> keptParents;
	for (size_t i = 0; i < asset.bones.size(); i++)
	{
		if (doomed[i]) continue;
		const int32 p = asset.bones[i].parent;
		keptParents.push_back((p >= 0 && (size_t)p < asset.bones.size()) ? asset.bones[p].name : std::string());
		kept.push_back(asset.bones[i]);
	}
	for (size_t i = 0; i < kept.size(); i++)
	{
		kept[i].self = (int32)i;
		kept[i].parent = -1;
		if (keptParents[i].empty()) continue;
		for (size_t k = 0; k < kept.size(); k++)
			if (kept[k].name == keptParents[i]) { kept[i].parent = (int32)k; break; }
	}
	asset.bones = kept;

	// Sprites pinned to a removed bone are UNPINNED, not deleted: the artwork
	// is still wanted, it just has nothing to follow. Deleting it would lose
	// work over what is usually a re-parenting mistake.
	for (size_t i = 0; i < asset.parts.size(); i++)
		for (size_t r = 0; r < removedNames.size(); r++)
			if (asset.parts[i].bone == removedNames[r]) { asset.parts[i].bone.clear(); break; }

	// Channels keyed against a removed bone are dropped. Unlike artwork these
	// are not recoverable work - a channel that names no bone animates nothing
	// and cannot be repointed, since which bone it meant is exactly what was
	// deleted.
	for (size_t c = 0; c < anim.clips.size(); c++)
	{
		std::vector<Channel> keptCh;
		for (size_t ch = 0; ch < anim.clips[c].Channels.size(); ch++)
		{
			bool drop = false;
			for (size_t r = 0; r < removedNames.size(); r++)
				if (anim.clips[c].Channels[ch].NodeName == removedNames[r]) { drop = true; break; }
			if (!drop) keptCh.push_back(anim.clips[c].Channels[ch]);
		}
		anim.clips[c].Channels = keptCh;
	}
	SyncClipsToAsset();

	// Built before the selection is cleared: `name` aliases selectedBone when
	// the panel's Delete button calls this, so clearing it first would label
	// every such entry "Remove Bone ''".
	const std::string label = "Remove Bone '" + name + "'";
	if (selectedBone == name) selectedBone.clear();
	PushEdit(before, label);
	TouchRig();
	return true;
}

bool Character2DDocument::RenameBone(const std::string& from, const std::string& to, std::string& errOut)
{
	const int id = asset.FindBone(from);
	if (id < 0) { errOut = "bone '" + from + "' not found"; return false; }
	if (to.empty()) { errOut = "a bone needs a name"; return false; }
	if (to == from) return true;
	if (asset.FindBone(to) >= 0) { errOut = "a bone called '" + to + "' already exists"; return false; }

	// Copied before anything is written. A caller may reasonably pass
	// `asset.bones[id].name` itself, and the assignment below would then
	// rewrite the string every comparison after it reads - silently renaming
	// the bone while leaving its sprites and its channels pointing at the old
	// name, which is exactly the breakage this function exists to prevent.
	const std::string oldName = from;
	const std::string newName = to;

	const std::string before = Snapshot();

	asset.bones[id].name = newName;
	// Everything that refers to a bone does so by name, so a rename has to
	// follow through into the artwork and the clips or it silently unpins
	// every sprite on that bone and orphans its animation.
	for (size_t i = 0; i < asset.parts.size(); i++)
		if (asset.parts[i].bone == oldName) asset.parts[i].bone = newName;
	for (size_t c = 0; c < anim.clips.size(); c++)
		for (size_t ch = 0; ch < anim.clips[c].Channels.size(); ch++)
			if (anim.clips[c].Channels[ch].NodeName == oldName) anim.clips[c].Channels[ch].NodeName = newName;
	SyncClipsToAsset();

	if (selectedBone == oldName) selectedBone = newName;
	PushEdit(before, "Rename Bone");
	TouchRig();
	return true;
}

bool Character2DDocument::ReparentBone(const std::string& name, const std::string& newParent,
	std::string& errOut)
{
	const int id = asset.FindBone(name);
	if (id < 0) { errOut = "bone '" + name + "' not found"; return false; }

	int newParentId = -1;
	if (!newParent.empty())
	{
		newParentId = asset.FindBone(newParent);
		if (newParentId < 0) { errOut = "bone '" + newParent + "' not found"; return false; }
		if (newParentId == id) { errOut = "a bone cannot be its own parent"; return false; }
		// Walking UP from the proposed parent: if this bone is on that chain,
		// the result would be a cycle, and a cycle here is an infinite loop in
		// the pose composer rather than a visible mistake.
		for (int p = newParentId; p >= 0; p = asset.bones[p].parent)
			if (p == id) { errOut = "'" + newParent + "' is below '" + name + "'"; return false; }
	}

	const std::string before = Snapshot();

	// Keep the bone where it is on screen. Reparenting is about who drives
	// whom, not about moving artwork - and a limb that jumps across the canvas
	// when you fix its parent is a limb you then have to re-place by hand.
	std::vector<Vec3> worldPos(asset.bones.size(), Vec3(0.f, 0.f, 0.f));
	for (size_t i = 0; i < asset.bones.size(); i++)
	{
		const int32 p = asset.bones[i].parent;
		worldPos[i] = asset.bones[i].pos;
		if (p >= 0 && (size_t)p < i) worldPos[i] = worldPos[p] + asset.bones[i].pos;
	}
	const Vec3 keepWorld = worldPos[id];

	asset.bones[id].parent = (int32)newParentId;
	asset.bones[id].pos = (newParentId >= 0) ? (keepWorld - worldPos[newParentId]) : keepWorld;
	RebuildBindPose(asset.bones[id]);

	// The array must still list every parent before its children. Moving a
	// bone under a LATER one breaks that, so re-sort rather than assume.
	{
		std::vector<Bone> ordered;
		std::vector<bool> placed(asset.bones.size(), false);
		bool progress = true;
		while (progress && ordered.size() < asset.bones.size())
		{
			progress = false;
			for (size_t i = 0; i < asset.bones.size(); i++)
			{
				if (placed[i]) continue;
				const int32 p = asset.bones[i].parent;
				if (p >= 0)
				{
					bool parentPlaced = false;
					for (size_t k = 0; k < ordered.size(); k++)
						if (ordered[k].name == asset.bones[p].name) { parentPlaced = true; break; }
					if (!parentPlaced) continue;
				}
				ordered.push_back(asset.bones[i]);
				placed[i] = true;
				progress = true;
			}
		}
		if (ordered.size() == asset.bones.size())
		{
			// Re-resolve parents against the new order, by name.
			std::vector<std::string> parentNames(ordered.size());
			for (size_t i = 0; i < ordered.size(); i++)
			{
				const int32 p = ordered[i].parent;
				parentNames[i] = (p >= 0 && (size_t)p < asset.bones.size()) ? asset.bones[p].name : std::string();
			}
			for (size_t i = 0; i < ordered.size(); i++)
			{
				ordered[i].self = (int32)i;
				ordered[i].parent = -1;
				if (parentNames[i].empty()) continue;
				for (size_t k = 0; k < ordered.size(); k++)
					if (ordered[k].name == parentNames[i]) { ordered[i].parent = (int32)k; break; }
			}
			asset.bones = ordered;
		}
	}

	PushEdit(before, "Reparent Bone '" + name + "'");
	TouchRig();
	return true;
}

bool Character2DDocument::SetBoneRest(const std::string& name, const Vec2& pos, float rotDegrees,
	std::string& errOut)
{
	const int id = asset.FindBone(name);
	if (id < 0) { errOut = "bone '" + name + "' not found"; return false; }

	const std::string before = Snapshot();
	asset.bones[id].pos = Vec3(pos.x, pos.y, 0.f);
	// Radians in memory. The engine's Euler angles are radians throughout; the
	// degrees are a UI and file-format convenience only.
	asset.bones[id].rot = Quaternion();
	asset.bones[id].rot.SetRotationFromEuler(Vec3(0.f, 0.f, (f32)DEGTORAD(rotDegrees)), RotationOrder::XYZ);
	RebuildBindPose(asset.bones[id]);
	PushEdit(before, "Move Bone '" + name + "'");
	TouchRig();
	return true;
}

// ---- sprites -----------------------------------------------------------------

bool Character2DDocument::AddSprite(const std::string& name, const std::string& texture,
	const std::string& bone, std::string& errOut)
{
	if (name.empty()) { errOut = "a sprite needs a name"; return false; }
	if (FindSprite(name) >= 0) { errOut = "a sprite called '" + name + "' already exists"; return false; }
	if (!bone.empty() && asset.FindBone(bone) < 0)
	{ errOut = "bone '" + bone + "' not found"; return false; }

	const std::string before = Snapshot();

	SpritePart2D p;
	p.name = name;
	p.bone = bone;
	p.texture = texture;
	// Newest artwork in front. A cutout is assembled front to back far more
	// often than the reverse, and a part that lands behind everything looks
	// like the import failed.
	f32 frontZ = 0.f;
	for (size_t i = 0; i < asset.parts.size(); i++)
		frontZ = std::max(frontZ, asset.parts[i].z);
	p.z = asset.parts.empty() ? 0.f : (frontZ + 0.01f);
	asset.parts.push_back(p);

	PushEdit(before, "Add Sprite '" + name + "'");
	TouchRig();
	selectedSprite = name;
	return true;
}

bool Character2DDocument::RemoveSprite(const std::string& name, std::string& errOut)
{
	const int id = FindSprite(name);
	if (id < 0) { errOut = "sprite '" + name + "' not found"; return false; }

	// Copied for the same reason RemoveClip's is: `name` aliases both the
	// erased element's own field and selectedSprite.
	const std::string spriteName = name;
	const std::string before = Snapshot();
	asset.parts.erase(asset.parts.begin() + id);
	if (selectedSprite == spriteName) selectedSprite.clear();
	PushEdit(before, "Remove Sprite '" + spriteName + "'");
	TouchRig();
	return true;
}

bool Character2DDocument::ReorderSprite(const std::string& name, int delta, std::string& errOut)
{
	const int id = FindSprite(name);
	if (id < 0) { errOut = "sprite '" + name + "' not found"; return false; }
	const int target = id + delta;
	if (target < 0 || target >= (int)asset.parts.size()) return true;   // already at the end

	const std::string before = Snapshot();
	std::swap(asset.parts[id], asset.parts[target]);
	// Depth follows the list order. These are transparent quads sorted by
	// distance, so the list is only what the user reads - z is what the
	// renderer obeys, and leaving it behind would make the list a lie.
	std::swap(asset.parts[id].z, asset.parts[target].z);
	PushEdit(before, "Reorder Sprite '" + name + "'");
	// Draw order is a Pivot input, not geometry - no rebuild needed.
	TouchParts();
	return true;
}

// ---- clips -------------------------------------------------------------------

bool Character2DDocument::AddClip(const std::string& name, float duration, std::string& errOut)
{
	if (name.empty()) { errOut = "a clip needs a name"; return false; }
	if (asset.FindClip(name) >= 0) { errOut = "a clip called '" + name + "' already exists"; return false; }

	const std::string before = Snapshot();
	Animation clip;
	clip.AnimationName = name;
	clip.Duration = (duration > 0.f) ? duration : 1.f;
	// Seconds, not ticks - the convention every editor-authored clip follows
	// (see AnimationLoader::Save's round-trip note).
	clip.TicksPerSecond = 1.f;
	clip.AuthoredFps = anim.snapFps;
	clip.Guid = AnimationLoader::GenerateGuid();
	anim.clips.push_back(clip);
	anim.activeClip = (int)anim.clips.size() - 1;
	anim.playhead = 0.f;
	SyncClipsToAsset();

	PushEdit(before, "Add Clip '" + name + "'");
	dirty = true;
	return true;
}

bool Character2DDocument::RemoveClip(const std::string& name, std::string& errOut)
{
	const int id = asset.FindClip(name);
	if (id < 0) { errOut = "clip '" + name + "' not found"; return false; }

	// COPIED, not aliased. The UI calls this with a reference into anim.clips
	// (`doc.RemoveClip(clip.AnimationName, err)`), so `name` points inside the
	// very element erased below - every read of it after that is a
	// use-after-free.
	const std::string clipName = name;

	const std::string before = Snapshot();
	anim.clips.erase(anim.clips.begin() + id);
	// Follow the removal rather than only clamping the top end: erasing a clip
	// BEFORE the active one shifts every later index down, so a clamp alone
	// silently leaves the timeline pointing at a different clip than the one
	// that was open.
	if (anim.activeClip == id) anim.activeClip = anim.clips.empty() ? -1 : 0;
	else if (anim.activeClip > id) anim.activeClip--;
	if (anim.activeClip >= (int)anim.clips.size()) anim.activeClip = (int)anim.clips.size() - 1;
	anim.selectedKeys.clear();
	if (asset.defaultClip == clipName) asset.defaultClip.clear();
	SyncClipsToAsset();

	PushEdit(before, "Remove Clip '" + clipName + "'");
	dirty = true;
	return true;
}

bool Character2DDocument::RenameClip(const std::string& from, const std::string& to, std::string& errOut)
{
	const int id = asset.FindClip(from);
	if (id < 0) { errOut = "clip '" + from + "' not found"; return false; }
	if (to.empty()) { errOut = "a clip needs a name"; return false; }
	if (to == from) return true;
	if (asset.FindClip(to) >= 0) { errOut = "a clip called '" + to + "' already exists"; return false; }

	const std::string before = Snapshot();
	// Tested BEFORE the rename. The UI passes a reference into anim.clips, so
	// the assignment below rewrites the very string `from` aliases - after it,
	// `from == to` and this comparison can never match. The default clip would
	// then keep naming a clip that no longer exists, and every scene placing
	// this character would quietly stop auto-playing.
	const bool wasDefault = (asset.defaultClip == from);
	anim.clips[id].AnimationName = to;
	// Scenes address a clip by name, and the asset's own default is the one
	// reference this file owns.
	if (wasDefault) asset.defaultClip = to;
	SyncClipsToAsset();

	PushEdit(before, "Rename Clip");
	dirty = true;
	return true;
}
