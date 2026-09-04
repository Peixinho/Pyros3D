//=============================================================================
// Name        : AnimationEditor.cpp
// Description : See AnimationEditor.h.
//=============================================================================

#include "AnimationEditor.h"
#include "../AnimationEditorDocument.h"
#include "../AnimationPreview.h"

#include <Pyros3D/AnimationManager/IKSolver.h>
#include "EasingPreview.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace p3d;

namespace {

const float kBoneTreeWidth = 250.f;
const float kTimelineHeight = 210.f;
const float kRowHeight = 18.f;
const float kTrackLabelWidth = 150.f;
const float kRulerHeight = 22.f;

std::string FormatTime(float t)
{
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.3fs", t);
	return std::string(buf);
}

// Rebuilds doc.selectedBone from selectedBoneName after a rig change - bone
// ids are indices into a specific skeleton and mean nothing across two.
void ResolveSelectedBone(AnimationEditorDocument& doc)
{
	if (!doc.RigInstance()) { doc.selectedBone = -1; return; }
	if (!doc.selectedBoneName.empty())
	{
		doc.selectedBone = doc.RigFindBone(doc.selectedBoneName);
		if (doc.selectedBone >= 0) return;
	}
	if (doc.selectedBone >= (int)doc.RigBoneCount()) doc.selectedBone = -1;
	if (doc.selectedBone >= 0) doc.selectedBoneName = doc.RigBoneName(doc.selectedBone);
}

// Rows shown in the dope sheet: a channel index (or -1 for a bone with no
// channel yet, which showAllBones surfaces so it can be keyed).
struct TrackRow {
	std::string boneName;
	int channel = -1;
	int boneId = -1;
	int depth = 0;
};

void BuildTracks(const AnimationEditorDocument& doc, std::vector<TrackRow>& out)
{
	out.clear();
	const Animation* clip = doc.ActiveClip();
	if (!clip) return;

	SkeletonAnimationInstance* inst = doc.RigInstance();
	if (doc.showAllBones && inst)
	{
		// Skeleton order, so parents read above their children.
		const std::vector<Bone>& bones = inst->GetSkeletonBones();
		for (size_t i = 0; i < bones.size(); i++)
		{
			TrackRow row;
			row.boneName = bones[i].name;
			row.boneId = bones[i].self;
			row.channel = doc.FindChannel(doc.activeClip, bones[i].name);
			int depth = 0, p = bones[i].parent, guard = 0;
			while (p >= 0 && p < (int)bones.size() && guard++ < (int)bones.size()) { depth++; p = bones[p].parent; }
			row.depth = depth;
			out.push_back(row);
		}
		return;
	}

	// Channel order - which is the file's own order, and stays stable as
	// keys are added or removed.
	for (size_t c = 0; c < clip->Channels.size(); c++)
	{
		TrackRow row;
		row.boneName = clip->Channels[c].NodeName;
		row.channel = (int)c;
		row.boneId = doc.RigFindBone(row.boneName);
		out.push_back(row);
	}
}

} // namespace

namespace AnimationEditor {

void EnsurePreview(AnimationEditorDocument& doc)
{
	if (!doc.preview) doc.preview.reset(new AnimationPreview());
	doc.preview->EnsureInit();
	if (!doc.meshPath.empty() && doc.preview->loadedMeshPath != doc.meshPath)
	{
		doc.preview->LoadMesh(doc.meshPath);
		ResolveSelectedBone(doc);
		// Bone masks, joint limits and IK chains belong to the skeleton, so
		// they arrive with it rather than with the clip.
		doc.LoadRigForMesh();
	}
	else if (doc.meshPath.empty() && !doc.preview->loadedMeshPath.empty())
	{
		doc.preview->ClearMesh();
		doc.selectedBone = -1;
	}
}

bool KeyBoneAtTime(AnimationEditorDocument& doc, int boneId, float time)
{
	SkeletonAnimationInstance* inst = doc.RigInstance();
	if (!inst) return false;
	if (boneId < 0 || boneId >= (int)inst->GetNumberBones()) return false;
	if (!doc.HasActiveClip()) return false;
	if (!doc.keyPosition && !doc.keyRotation) return false;

	const std::string boneName = doc.RigBoneName(boneId);
	if (boneName.empty()) return false;

	const int channel = doc.FindOrCreateChannel(doc.activeClip, boneName);
	if (channel < 0) return false;

	// The bone's local transform is what a channel key stores - the runtime
	// composes the parent chain itself (SkeletonAnimationInstance::
	// RefreshSkinning), so keying a model-space matrix here would bake every
	// ancestor's transform into the child and double it at playback.
	const Matrix local = inst->GetBoneLocalTransform(boneId);
	const Vec3 pos = local.GetTranslation();
	const Quaternion rot = local.ConvertToQuaternion();
	const Vec3 scale = local.GetScale();

	doc.SetKey(doc.activeClip, channel, time, pos, rot, scale,
		doc.keyPosition, doc.keyRotation, /*doScale=*/false);
	return true;
}

int KeyPendingPose(AnimationEditorDocument& doc)
{
	std::map<int, Matrix>* overrides = doc.RigPoseOverrides();
	if (!overrides || overrides->empty() || !doc.HasActiveClip()) return 0;

	std::vector<int> bones;
	for (std::map<int, Matrix>::const_iterator it = overrides->begin(); it != overrides->end(); ++it)
		bones.push_back(it->first);

	const float time = doc.SnapTime(doc.playhead);
	int keyed = 0;
	doc.PushSnapshotEdit("Key pose at " + FormatTime(time), [&]() {
		for (size_t i = 0; i < bones.size(); i++)
			if (KeyBoneAtTime(doc, bones[i], time)) keyed++;
	});
	// The pose is now in the clip; keeping the overrides would pin the rig
	// to it even after scrubbing elsewhere.
	overrides->clear();
	return keyed;
}

int KeyWholeSkeleton(AnimationEditorDocument& doc, float time)
{
	if (!doc.RigInstance() || !doc.HasActiveClip()) return 0;

	const float t = doc.SnapTime(time);
	const uint32 count = doc.RigBoneCount();
	int keyed = 0;
	doc.PushSnapshotEdit("Key whole skeleton at " + FormatTime(t), [&]() {
		for (uint32 i = 0; i < count; i++)
			if (KeyBoneAtTime(doc, (int)i, t)) keyed++;
	});
	if (std::map<int, Matrix>* overrides = doc.RigPoseOverrides()) overrides->clear();
	return keyed;
}

// True when `boneId` is `rootId` or sits anywhere beneath it. Bones store a
// parent id rather than a child list, so descent is tested by walking up -
// the guard is against a malformed rig with a parent cycle, which would
// otherwise hang the UI thread rather than just drawing something wrong.
bool IsBoneInSubtree(const std::vector<Bone>& bones, int boneId, int rootId)
{
	int p = boneId, guard = 0;
	while (p >= 0 && p < (int)bones.size() && guard++ <= (int)bones.size())
	{
		if (p == rootId) return true;
		p = bones[p].parent;
	}
	return false;
}

// Gives every chain that has never had its target placed one at its own
// effector, so it starts out solved and still rather than dragging the limb
// to the world origin. Runs each frame because the rig can be bound after the
// chains are (a chain loaded from the sidecar has no rig to measure against
// until the mesh finishes loading).
void SeedUnplacedIKTargets(AnimationEditorDocument& doc)
{
	AnimationPreview* pv = doc.preview.get();
	if (!pv || !pv->instance) return;
	for (size_t i = 0; i < doc.ikHandles.size(); i++)
	{
		AnimationEditorDocument::IKHandle& h = doc.ikHandles[i];
		if (h.targetSet) continue;
		const int eff = pv->BoneIdByName(h.effectorBone);
		if (eff < 0) continue;
		h.target = pv->instance->GetBoneGlobalTransform(eff).GetTranslation();
		h.targetSet = true;
		// Deliberately not an undo entry: this is the chain's initial
		// placement, not an edit the user made, and pushing it would put a
		// command on the stack that Ctrl+Z could rewind into the broken
		// origin-target state it exists to avoid.
	}
}

// Resolves an IK handle's bone names against the currently bound mesh.
// Returns false if either end is missing, which happens routinely when a
// handle authored for one rig is looked at with another mesh bound.
bool ResolveIK(AnimationEditorDocument& doc, const AnimationEditorDocument::IKHandle& h,
	int& outRoot, int& outEffector)
{
	AnimationPreview* pv = doc.preview.get();
	if (!pv || !pv->instance) return false;
	outRoot = pv->BoneIdByName(h.rootBone);
	outEffector = pv->BoneIdByName(h.effectorBone);
	return outRoot >= 0 && outEffector >= 0;
}

// Solves one handle against the pose currently on the rig.
bool ApplyIK(AnimationEditorDocument& doc, const AnimationEditorDocument::IKHandle& h)
{
	AnimationPreview* pv = doc.preview.get();
	int root = -1, effector = -1;
	if (!ResolveIK(doc, h, root, effector)) return false;

	// Without limits a knee bends backwards: both solutions reach the target
	// and nothing in the maths prefers the anatomically possible one.
	// Resolved per solve rather than cached, since editing a limit has to
	// take effect on the very next solve for the UI to feel connected.
	const std::map<p3d::int32, p3d::JointLimit> limits = doc.rig.ResolveLimits(pv->instance);
	return p3d::IKSolver::Solve(pv->instance, root, effector, h.target,
		h.usePole ? h.pole : p3d::Vec3(0.f, 0.f, 0.f), 10,
		limits.empty() ? NULL : &limits);
}

// Keys every bone of a chain at `time`. The solver has already written the
// chain's local transforms, so this is the same path a hand-posed bone takes -
// which is the whole point: nothing downstream can tell the keys came from IK.
int KeyIKChain(AnimationEditorDocument& doc, const AnimationEditorDocument::IKHandle& h, float time)
{
	AnimationPreview* pv = doc.preview.get();
	int root = -1, effector = -1;
	if (!ResolveIK(doc, h, root, effector)) return 0;

	const std::vector<p3d::int32> chain = p3d::IKSolver::BuildChain(pv->instance, root, effector);
	int keyed = 0;
	for (size_t i = 0; i < chain.size(); i++)
		if (KeyBoneAtTime(doc, (int)chain[i], time)) keyed++;
	return keyed;
}

// Solves and keys across a frame range - "bake over range". This is the
// feature IK exists for in an authoring tool: a foot planted on the ground
// while the hips move is not something anyone keys by hand.
//
// Each frame re-establishes the pose from the clip BEFORE solving, so the
// bake is a pure function of (clip time, target) exactly as IKSolver::Solve
// documents. Carrying the previous frame's solved pose into the next would
// make the result depend on which direction the range was walked.
int BakeIKOverRange(AnimationEditorDocument& doc, const AnimationEditorDocument::IKHandle& h,
	float startTime, float endTime)
{
	AnimationPreview* pv = doc.preview.get();
	if (!pv || !pv->instance || !doc.HasActiveClip()) return 0;

	const float fps = (doc.snapFps > 0.f ? doc.snapFps : 30.f);
	const float step = 1.f / fps;
	if (endTime < startTime) std::swap(startTime, endTime);

	const p3d::Animation* clip = doc.ActiveClip();
	int keyed = 0;
	doc.PushSnapshotEdit("Bake IK: " + h.name, [&]() {
		for (float t = startTime; t <= endTime + step * 0.5f; t += step)
		{
			const float snapped = doc.SnapTime(t);
			// Re-pose from the clip first - see the determinism note above.
			if (clip) pv->instance->ApplyAnimationAtTime(*clip, snapped);
			else      pv->instance->ResetToBindPose();
			if (!ApplyIK(doc, h)) break;
			keyed += KeyIKChain(doc, h, snapped);
		}
	});

	pv->poseOverrides.clear();
	return keyed;
}

int DeleteSelectedKeys(AnimationEditorDocument& doc)
{
	if (doc.selectedKeys.empty() || !doc.HasActiveClip()) return 0;

	std::vector<AnimKeyRef> targets(doc.selectedKeys.begin(), doc.selectedKeys.end());
	int removed = 0;
	doc.PushSnapshotEdit(targets.size() == 1 ? "Delete key" : "Delete " + std::to_string(targets.size()) + " keys", [&]() {
		for (size_t i = 0; i < targets.size(); i++)
			if (doc.DeleteKeysAtTime(doc.activeClip, targets[i].channel, targets[i].time)) removed++;
		doc.PruneEmptyChannels(doc.activeClip);
	});
	doc.selectedKeys.clear();
	return removed;
}

void SetPlayhead(AnimationEditorDocument& doc, float time, bool keepPendingPose)
{
	const Animation* clip = doc.ActiveClip();
	float t = time;
	if (t < 0.f) t = 0.f;
	if (clip && t > clip->Duration) t = clip->Duration;
	if (std::fabs(t - doc.playhead) < 1e-6f) return;

	doc.playhead = t;
	// Uncommitted pose edits belong to the frame they were made at. Carrying
	// them along the timeline would silently apply one bone's edit to every
	// frame the user visits, which is not what "I moved this arm" means.
	if (!keepPendingPose)
		if (std::map<int, Matrix>* overrides = doc.RigPoseOverrides()) overrides->clear();
}

// ---- window ---------------------------------------------------------------

namespace {

void DrawToolbar(AnimationEditorDocument& doc, const std::vector<AnimationMeshChoice>& meshes,
	FrameRequests& requests)
{
	AnimationPreview* pv = doc.preview.get();

	// Rig picker.
	ImGui::TextUnformatted("Rig");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(240.f);
	std::string current = "(none)";
	for (size_t i = 0; i < meshes.size(); i++)
		if (meshes[i].second == doc.meshPath) { current = meshes[i].first; break; }
	if (ImGui::BeginCombo("##rig", current.c_str()))
	{
		if (ImGui::Selectable("(none)", doc.meshPath.empty()))
		{
			requests.meshChanged = true;
			requests.newMeshPath.clear();
		}
		for (size_t i = 0; i < meshes.size(); i++)
		{
			const bool sel = (meshes[i].second == doc.meshPath);
			if (ImGui::Selectable(meshes[i].first.c_str(), sel))
			{
				requests.meshChanged = true;
				requests.newMeshPath = meshes[i].second;
			}
			if (sel) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Skinned .p3dm these clips are previewed on.\nRemembered per project, not stored in the .p3da.");

	ImGui::SameLine();
	if (ImGui::Button("Frame") && pv) pv->FrameCamera();

	// Clip picker.
	ImGui::SameLine();
	ImGui::TextUnformatted("| Clip");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.f);
	const std::string clipLabel = doc.HasActiveClip()
		? doc.clips[doc.activeClip].AnimationName : std::string("(no clips)");
	if (ImGui::BeginCombo("##clip", clipLabel.c_str()))
	{
		for (size_t i = 0; i < doc.clips.size(); i++)
		{
			const bool sel = ((int)i == doc.activeClip);
			// Clip id is the index, and that id is what a scene's saved
			// Play() call refers to - worth showing.
			const std::string label = "[" + std::to_string(i) + "] " + doc.clips[i].AnimationName;
			if (ImGui::Selectable(label.c_str(), sel))
			{
				doc.activeClip = (int)i;
				doc.selectedKeys.clear();
				SetPlayhead(doc, 0.f);
			}
			if (sel) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::Button("+ Clip"))
	{
		doc.PushSnapshotEdit("Add clip", [&]() {
			doc.activeClip = doc.AddClip("Clip", 1.f);
		});
		doc.selectedKeys.clear();
		SetPlayhead(doc, 0.f);
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete Clip") && doc.HasActiveClip())
		ImGui::OpenPopup("##delclip");

	if (ImGui::BeginPopup("##delclip"))
	{
		ImGui::TextUnformatted("Delete this clip?");
		// The old warning here said later clips shift id and scenes playing
		// them would break. Scenes now save a clip's guid and resolve
		// through SkeletonAnimation::ResolveAnimationID, so a renumber is
		// no longer silently destructive - only scenes that referenced THIS
		// clip are affected, and those warn on load.
		if (doc.activeClip < (int)doc.clips.size() - 1)
			ImGui::TextDisabled("Later clips shift id; scenes resolve by guid, so they follow.");
		if (ImGui::Button("Delete"))
		{
			const int idx = doc.activeClip;
			doc.PushSnapshotEdit("Delete clip", [&]() { doc.RemoveClip(idx); });
			SetPlayhead(doc, 0.f);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	if (doc.HasActiveClip())
	{
		Animation& clip = doc.clips[doc.activeClip];

		ImGui::SameLine();
		ImGui::TextUnformatted("| Name");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(150.f);
		std::string name = clip.AnimationName;
		if (ImGui::InputText("##clipname", &name))
		{
			// Typing is per-keystroke; one undo entry per character would
			// be useless, so the edit is only committed (and pushed) when
			// the field is deactivated - ImGui's own idiom for this.
			clip.AnimationName = name;
			doc.dirty = true;
		}
		if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
		if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit("Rename clip");

		ImGui::SameLine();
		ImGui::TextUnformatted("Length");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.f);
		float duration = clip.Duration;
		if (ImGui::DragFloat("##duration", &duration, 0.05f, 0.05f, 600.f, "%.2fs"))
		{
			clip.Duration = std::max(0.05f, duration);
			doc.dirty = true;
			if (doc.playhead > clip.Duration) SetPlayhead(doc, clip.Duration);
		}
		if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
		if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit("Change clip length");

		// Clip-level switches live behind a popup rather than inline - the
		// toolbar is already a single long row, and these are set once per
		// clip rather than adjusted constantly.
		ImGui::SameLine();
		if (ImGui::Button("Clip Settings")) ImGui::OpenPopup("##clipflags");
		if (ImGui::BeginPopup("##clipflags"))
		{
			bool loop = clip.HasFlag(p3d::ANIM_FLAG_LOOP);
			if (ImGui::Checkbox("Loop", &loop))
			{
				doc.PushSnapshotEdit(loop ? "Enable looping" : "Disable looping", [&]() {
					if (loop) clip.Flags |= p3d::ANIM_FLAG_LOOP;
					else      clip.Flags &= ~(uint32_t)p3d::ANIM_FLAG_LOOP;
				});
			}
			ImGui::SetItemTooltip("Records that this clip is meant to cycle.\n"
				"Play()'s repetition argument still drives actual looping.");

			bool applyScale = clip.HasFlag(p3d::ANIM_FLAG_APPLY_SCALE);
			if (ImGui::Checkbox("Apply scale keys", &applyScale))
			{
				doc.PushSnapshotEdit(applyScale ? "Enable scale keys" : "Disable scale keys", [&]() {
					if (applyScale) clip.Flags |= p3d::ANIM_FLAG_APPLY_SCALE;
					else            clip.Flags &= ~(uint32_t)p3d::ANIM_FLAG_APPLY_SCALE;
				});
				doc.clipsRevision++;
			}
			ImGui::SetItemTooltip("Scale keys have always been stored but never applied to the mesh.\n"
				"Turning this on changes how THIS clip deforms - other clips are untouched.");

			ImGui::Separator();
			ImGui::SetNextItemWidth(90.f);
			float fps = clip.AuthoredFps > 0.f ? clip.AuthoredFps : doc.snapFps;
			if (ImGui::DragFloat("Authored fps", &fps, 1.f, 1.f, 240.f, "%.0f"))
			{
				clip.AuthoredFps = fps;
				// The snap grid follows the clip's own frame rate, which is
				// the whole point of recording it - reopening a 24fps clip
				// used to silently snap it to the editor's default 30.
				doc.snapFps = fps;
				doc.dirty = true;
			}
			if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
			if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit("Change authored fps");
			ImGui::EndPopup();
		}
	}

	ImGui::SameLine();
	ImGui::TextUnformatted("|");
	ImGui::SameLine();
	if (ImGui::Button("Save")) requests.save = true;
	ImGui::SameLine();
	if (ImGui::Button("Save As...")) requests.saveAs = true;
}

void DrawBonePanel(AnimationEditorDocument& doc, const char* emptyHint = NULL)
{
	SkeletonAnimationInstance* inst = doc.RigInstance();

	ImGui::BeginChild("##bones", ImVec2(kBoneTreeWidth, 0), true);
	if (!inst)
	{
		ImGui::TextDisabled("No rig bound.");
		ImGui::TextWrapped("%s", emptyHint ? emptyHint
			: "Pick a skinned .p3dm in the Rig box above to see its skeleton and pose it.");
		ImGui::EndChild();
		return;
	}

	const std::vector<Bone>& bones = inst->GetSkeletonBones();
	ImGui::Text("Skeleton (%d bones)", (int)bones.size());
	ImGui::Separator();

	ImGui::BeginChild("##bonelist", ImVec2(0, -150.f));
	// Flat list with indentation rather than a real tree: bone parents are
	// ids, not a child list, and rebuilding a child map every frame to feed
	// TreeNode buys nothing here - the indent already reads as hierarchy and
	// every bone stays clickable without expanding anything.
	for (size_t i = 0; i < bones.size(); i++)
	{
		int depth = 0, p = bones[i].parent, guard = 0;
		while (p >= 0 && p < (int)bones.size() && guard++ < (int)bones.size()) { depth++; p = bones[p].parent; }

		ImGui::PushID((int)i);
		if (depth > 0) ImGui::Indent(depth * 10.f);
		const bool selected = (doc.selectedBone == bones[i].self);
		const bool animated = doc.FindChannel(doc.activeClip, bones[i].name) >= 0;
		if (!animated) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.62f, 1.f));
		if (ImGui::Selectable(bones[i].name.c_str(), selected))
		{
			doc.selectedBone = bones[i].self;
			doc.selectedBoneName = bones[i].name;
		}
		if (!animated) ImGui::PopStyleColor();
		if (depth > 0) ImGui::Unindent(depth * 10.f);
		ImGui::PopID();
	}
	ImGui::EndChild();

	ImGui::Separator();
	if (doc.selectedBone >= 0 && doc.selectedBone < (int)bones.size())
	{
		ImGui::Text("Bone: %s", bones[doc.selectedBone].name.c_str());
		const Matrix local = inst->GetBoneLocalTransform(doc.selectedBone);
		Vec3 pos = local.GetTranslation();
		Matrix rotOnly = local;
		// Shown in degrees. The engine's Euler conversions are radians on
		// both sides (Matrix::GetEulerFromRotationMatrix /
		// Quaternion::SetRotationFromEuler), so this converts in and back
		// out - dragging a bone by "0.1" of a radian is not a thing an
		// animator wants to do.
		const Vec3 eulerRad = rotOnly.GetEulerFromRotationMatrix();
		Vec3 euler((f32)RADTODEG(eulerRad.x), (f32)RADTODEG(eulerRad.y), (f32)RADTODEG(eulerRad.z));

		bool edited = false;
		ImGui::SetNextItemWidth(-1);
		if (ImGui::DragFloat3("##pos", &pos.x, 0.01f, 0.f, 0.f, "P %.3f")) edited = true;
		if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
		ImGui::SetNextItemWidth(-1);
		if (ImGui::DragFloat3("##rot", &euler.x, 0.5f, 0.f, 0.f, "R %.1f deg")) edited = true;
		if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();

		if (edited)
		{
			// Rebuild T*R exactly the way the sampler does (rotation matrix
			// then Translate), so a numerically typed pose and a keyed one
			// are the same transform.
			Quaternion q;
			q.SetRotationFromEuler(Vec3((f32)DEGTORAD(euler.x), (f32)DEGTORAD(euler.y), (f32)DEGTORAD(euler.z)));
			Matrix m = q.ConvertToMatrix();
			m.Translate(pos);
			if (std::map<int, Matrix>* ov = doc.RigPoseOverrides()) (*ov)[doc.selectedBone] = m;
			inst->SetBoneLocalTransform(doc.selectedBone, m);
			inst->RefreshSkinning();
		}
		if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit("Pose bone");

		if (ImGui::Button("Reset to Bind"))
		{
			const Matrix bind = inst->GetBindPoseLocal(doc.selectedBone);
			if (std::map<int, Matrix>* ov = doc.RigPoseOverrides()) (*ov)[doc.selectedBone] = bind;
			inst->SetBoneLocalTransform(doc.selectedBone, bind);
			inst->RefreshSkinning();
		}
		ImGui::SameLine();
		if (ImGui::Button("Key Bone"))
		{
			const float t = doc.SnapTime(doc.playhead);
			doc.PushSnapshotEdit("Key bone '" + bones[doc.selectedBone].name + "'", [&]() {
				KeyBoneAtTime(doc, doc.selectedBone, t);
			});
			if (std::map<int, Matrix>* ov = doc.RigPoseOverrides()) ov->erase(doc.selectedBone);
		}
	}
	else ImGui::TextDisabled("No bone selected.\nClick a joint in the viewport.");

	ImGui::EndChild();
}

void DrawBlendPanel(AnimationEditorDocument& doc, FrameRequests& requests)
{
	AnimationPreview* pv = doc.preview.get();

	// Scrolled: entries, a layer per expandable node with a bone list inside,
	// and the export row add up to more than the panel's height as soon as
	// there is anything real in the blend - without this the layer editor and
	// Copy Lua are simply unreachable.
	ImGui::BeginChild("##blendscroll", ImVec2(0, 0), false);

	ImGui::TextWrapped(
		"Plays several clips at once through the engine's own Play()/ChangeProperties() "
		"path, so this previews exactly what the runtime does. The game drives the weights "
		"from gameplay state - use Copy Lua below to take this setup with you.");
	ImGui::Separator();

	// ---- transport ---------------------------------------------------
	if (ImGui::Button(doc.blendPlaying ? "Pause##blend" : "Play##blend"))
		doc.blendPlaying = !doc.blendPlaying;
	ImGui::SameLine();
	if (ImGui::Button("Restart##blend"))
	{
		// Rebuilding restarts every clip from its first frame - the clock
		// itself must keep counting up (the engine subtracts the time it
		// first saw a clip), so it is the Play() set that is remade, not the
		// clock that is rewound.
		doc.TouchBlend();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("clock %.2fs", doc.blendClock);

	ImGui::Spacing();

	// ---- clips in the blend ------------------------------------------
	ImGui::TextUnformatted("Clips in this blend");
	if (doc.blendEntries.empty())
		ImGui::TextDisabled("None yet - add one below.");

	int removeEntry = -1;
	for (size_t i = 0; i < doc.blendEntries.size(); i++)
	{
		AnimationBlendEntry& e = doc.blendEntries[i];
		ImGui::PushID((int)i);
		ImGui::Separator();

		// Clip picker.
		const std::string label = (e.clip >= 0 && e.clip < (int)doc.clips.size())
			? ("[" + std::to_string(e.clip) + "] " + doc.clips[e.clip].AnimationName)
			: std::string("(missing clip)");
		ImGui::SetNextItemWidth(200.f);
		if (ImGui::BeginCombo("##clip", label.c_str()))
		{
			for (size_t c = 0; c < doc.clips.size(); c++)
			{
				const bool sel = ((int)c == e.clip);
				const std::string l = "[" + std::to_string(c) + "] " + doc.clips[c].AnimationName;
				if (ImGui::Selectable(l.c_str(), sel))
					doc.PushBlendEdit("Change blend clip", [&]() { e.clip = (int)c; });
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(160.f);
		// Weight changes do NOT bump blendRevision: they are applied to the
		// already-playing entries (ApplyBlendWeights) so a drag crossfades
		// instead of restarting every clip each frame. The whole drag is one
		// undo entry, and ends without a rebuild for the same reason.
		ImGui::SliderFloat("weight", &e.weight, 0.f, 1.f, "%.2f");
		if (ImGui::IsItemActivated()) doc.BeginBlendEdit();
		if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndBlendEdit("Change blend weight", false);

		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.f);
		ImGui::DragFloat("speed", &e.speed, 0.01f, -4.f, 4.f, "%.2fx"); // live, like weight
		if (ImGui::IsItemActivated()) doc.BeginBlendEdit();
		if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndBlendEdit("Change blend speed", false);

		ImGui::SameLine();
		// Layer picker.
		ImGui::SetNextItemWidth(130.f);
		const std::string layerLabel = e.layer.empty() ? std::string("(whole body)") : e.layer;
		if (ImGui::BeginCombo("layer", layerLabel.c_str()))
		{
			if (ImGui::Selectable("(whole body)", e.layer.empty()))
				doc.PushBlendEdit("Clear blend layer", [&]() { e.layer.clear(); });
			for (size_t l = 0; l < doc.blendLayers.size(); l++)
			{
				const bool sel = (doc.blendLayers[l].name == e.layer);
				if (ImGui::Selectable(doc.blendLayers[l].name.c_str(), sel))
				{
					const std::string picked = doc.blendLayers[l].name;
					doc.PushBlendEdit("Set blend layer", [&]() { e.layer = picked; });
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		if (ImGui::Button("Remove")) removeEntry = (int)i;
		ImGui::PopID();
	}
	if (removeEntry >= 0)
		doc.PushBlendEdit("Remove clip from blend", [&]() {
			doc.blendEntries.erase(doc.blendEntries.begin() + removeEntry);
		});

	ImGui::Separator();
	if (ImGui::Button("+ Add Clip to Blend") && !doc.clips.empty())
	{
		AnimationBlendEntry e;
		e.clip = (doc.activeClip >= 0 ? doc.activeClip : 0);
		// A second clip added at full weight would completely hide the first
		// (scale 0 wins outright), which reads as "adding a clip broke it".
		// Half weight makes the blend visible immediately.
		e.weight = doc.blendEntries.empty() ? 1.f : 0.5f;
		doc.PushBlendEdit("Add clip to blend", [&]() { doc.blendEntries.push_back(e); });
	}

	// ---- layers -------------------------------------------------------
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextUnformatted("Layers (bone masks)");
	ImGui::TextDisabled("A layer restricts a clip to part of the skeleton -\n"
		"upper body waving over a full-body walk, say.");

	// A layer is created ready to use and renamed in place. Requiring a name
	// to be typed into a separate box BEFORE the layer existed made naming a
	// mandatory first step for a thing whose name barely matters until it is
	// referenced by an entry.
	if (ImGui::Button("+ New Layer"))
	{
		std::string base = "Layer " + std::to_string(doc.blendLayers.size() + 1);
		int suffix = 2;
		while (doc.FindBlendLayer(base))
			base = "Layer " + std::to_string(doc.blendLayers.size() + suffix++);
		doc.PushBlendEdit("Add layer '" + base + "'",
			[&]() { doc.EnsureBlendLayer(base); });
		doc.rigDirty = true;
	}

	if (doc.blendLayers.empty())
		ImGui::TextDisabled("No layers yet.");

	// Membership is edited by clicking bones, so it needs the skeleton.
	const std::vector<Bone>* layerBones = (pv && pv->instance)
		? &pv->instance->GetSkeletonBones() : NULL;

	// Whether a click carries the whole subtree with it. One shared toggle
	// rather than two buttons per bone: "this bone and everything under it"
	// is the normal case (a whole arm, a whole spine), and having it as a
	// mode means a single click builds a limb instead of one click per bone.
	static bool includeChildren = true;

	std::string removeLayer;
	for (size_t i = 0; i < doc.blendLayers.size(); i++)
	{
		AnimationBlendLayer& layer = doc.blendLayers[i];
		ImGui::PushID((int)(1000 + i));
		if (ImGui::TreeNode(layer.name.c_str(), "%s  (%d bones)",
			layer.name.c_str(), (int)layer.bones.size()))
		{
			// Rename in place.
			char nameBuf[64];
			std::snprintf(nameBuf, sizeof(nameBuf), "%s", layer.name.c_str());
			ImGui::SetNextItemWidth(200.f);
			if (ImGui::InputText("name", nameBuf, sizeof(nameBuf)))
				layer.name = nameBuf;
			// InputText writes straight into layer.name, so the name it had
			// when the edit began has to be remembered separately - comparing
			// entries against layer.name at the end would compare them
			// against the NEW name and match nothing.
			static std::string renameFrom;
			if (ImGui::IsItemActivated())
			{
				doc.BeginBlendEdit();
				renameFrom = layer.name;
			}
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				// Entries point at the layer BY NAME, so a rename has to
				// carry them with it or they silently fall back to the whole
				// body - which looks like the layer stopped working.
				for (size_t e = 0; e < doc.blendEntries.size(); e++)
					if (doc.blendEntries[e].layer == renameFrom)
						doc.blendEntries[e].layer = layer.name;
				doc.EndBlendEdit("Rename layer");
				doc.rigDirty = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Delete Layer")) removeLayer = layer.name;

			if (!layerBones)
				ImGui::TextDisabled("Bind a rig to pick bones.");
			else
			{
				ImGui::Checkbox("include children", &includeChildren);
				ImGui::SameLine();
				ImGui::TextDisabled("(?)");
				ImGui::SetItemTooltip("On: clicking a bone adds or removes that bone and\n"
					"everything below it in the hierarchy. Off: just that bone.");

				ImGui::SameLine();
				if (ImGui::SmallButton("Clear"))
					doc.PushBlendEdit("Clear layer '" + layer.name + "'",
						[&]() { layer.bones.clear(); });
				ImGui::SameLine();
				if (ImGui::SmallButton("All"))
					doc.PushBlendEdit("Fill layer '" + layer.name + "'", [&]() {
						layer.bones.clear();
						for (size_t b = 0; b < layerBones->size(); b++)
							layer.bones.push_back((*layerBones)[b].name);
					});

				const std::vector<Bone>& bones = *layerBones;
				ImGui::BeginChild("##layerbones", ImVec2(0, 220.f), true);
				for (size_t b = 0; b < bones.size(); b++)
				{
					int depth = 0, p = bones[b].parent, guard = 0;
					while (p >= 0 && p < (int)bones.size() && guard++ < (int)bones.size())
					{
						depth++;
						p = bones[p].parent;
					}

					ImGui::PushID((int)b);
					if (depth > 0) ImGui::Indent(depth * 10.f);

					bool member = std::find(layer.bones.begin(), layer.bones.end(),
						bones[b].name) != layer.bones.end();
					if (ImGui::Checkbox("##member", &member))
					{
						// `member` now holds the state the user asked for;
						// the whole subtree is driven to it in one undo entry.
						const bool add = member;
						const int rootId = (int)bones[b].self;
						doc.PushBlendEdit(
							(add ? "Add bones to '" : "Remove bones from '") + layer.name + "'",
							[&]() {
								for (size_t k = 0; k < bones.size(); k++)
								{
									if (!includeChildren && (int)bones[k].self != rootId) continue;
									if (includeChildren && !IsBoneInSubtree(bones, (int)bones[k].self, rootId))
										continue;
									std::vector<std::string>::iterator it = std::find(
										layer.bones.begin(), layer.bones.end(), bones[k].name);
									if (add && it == layer.bones.end())
										layer.bones.push_back(bones[k].name);
									else if (!add && it != layer.bones.end())
										layer.bones.erase(it);
								}
							});
						doc.rigDirty = true;
					}

					ImGui::SameLine();
					// Clicking the NAME selects the bone (and highlights it in
					// the viewport) without changing membership, so the two
					// gestures never fight each other.
					const bool selected = (doc.selectedBone == bones[b].self);
					if (!member) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.62f, 1.f));
					if (ImGui::Selectable(bones[b].name.c_str(), selected))
					{
						doc.selectedBone = bones[b].self;
						doc.selectedBoneName = bones[b].name;
					}
					if (!member) ImGui::PopStyleColor();

					if (depth > 0) ImGui::Unindent(depth * 10.f);
					ImGui::PopID();
				}
				ImGui::EndChild();
			}

			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (!removeLayer.empty())
	{
		doc.PushBlendEdit("Delete layer '" + removeLayer + "'",
			[&]() { doc.RemoveBlendLayer(removeLayer); });
		doc.rigDirty = true;
	}

	// ---- export -------------------------------------------------------
	ImGui::Spacing();
	ImGui::Separator();
	if (ImGui::Button("Copy Lua"))
	{
		ImGui::SetClipboardText(doc.BuildBlendLuaSnippet(
			doc.absolutePath.empty() ? std::string() : doc.displayName + ".p3da").c_str());
		requests.copiedLua = true;
	}
	ImGui::SameLine();
	ImGui::TextDisabled("Play()/createLayer()/addBone() calls for a scene script.");
	if (requests.copiedLua)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.5f, 1.f, 0.5f, 1.f), "copied");
	}

	ImGui::EndChild();
}

void DrawIKPanel(AnimationEditorDocument& doc)
{
	AnimationPreview* pv = doc.preview.get();
	if (!pv || !pv->instance)
	{
		ImGui::TextDisabled("Bind a mesh to set up IK chains.");
		return;
	}
	if (!doc.HasActiveClip())
	{
		ImGui::TextDisabled("IK writes ordinary keys, so it needs an active clip.");
		return;
	}

	const std::vector<Bone>& bones = pv->instance->GetSkeletonBones();

	// ---- chain list ----------------------------------------------------
	ImGui::SetNextItemWidth(200.f);
	const std::string label = doc.HasActiveIK() ? doc.ikHandles[doc.activeIK].name : std::string("(no chains)");
	if (ImGui::BeginCombo("##ikchain", label.c_str()))
	{
		for (size_t i = 0; i < doc.ikHandles.size(); i++)
			if (ImGui::Selectable(doc.ikHandles[i].name.c_str(), (int)i == doc.activeIK))
				doc.activeIK = (int)i;
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::Button("+ Chain"))
	{
		doc.BeginInteractiveEdit();
		AnimationEditorDocument::IKHandle h;
		h.name = "Chain " + std::to_string(doc.ikHandles.size() + 1);
		// Seed from the selected bone and its grandparent, which is the
		// two-bone case (thigh/calf/foot) more often than not.
		if (doc.selectedBone >= 0)
		{
			h.effectorBone = pv->BoneName(doc.selectedBone);
			const int parent = bones[doc.selectedBone].parent;
			if (parent >= 0 && bones[parent].parent >= 0)
				h.rootBone = pv->BoneName(bones[parent].parent);
		}
		h.target = pv->instance->GetBoneGlobalTransform(
			doc.selectedBone >= 0 ? doc.selectedBone : 0).GetTranslation();
		h.targetSet = true;
		doc.ikHandles.push_back(h);
		doc.activeIK = (int)doc.ikHandles.size() - 1;
		doc.EndInteractiveEdit("Add IK chain");
	}
	if (doc.HasActiveIK())
	{
		ImGui::SameLine();
		if (ImGui::Button("Remove Chain"))
		{
			const int idx = doc.activeIK;
			doc.PushSnapshotEdit("Remove IK chain", [&]() {
				doc.ikHandles.erase(doc.ikHandles.begin() + idx);
				doc.activeIK = doc.ikHandles.empty() ? -1 : 0;
			});
		}
	}

	if (!doc.HasActiveIK()) return;
	AnimationEditorDocument::IKHandle& h = doc.ikHandles[doc.activeIK];

	ImGui::Separator();
	ImGui::SetNextItemWidth(200.f);
	ImGui::InputText("Name", &h.name);
	// Same per-keystroke reasoning as the clip name field: one undo entry per
	// character typed would be useless.
	if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
	if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit("Rename IK chain");

	// ---- endpoints -----------------------------------------------------
	// Bone pickers rather than free text: a typo'd bone name is a chain
	// that silently never solves.
	const char* ends[2] = { "Root", "Effector" };
	std::string* target[2] = { &h.rootBone, &h.effectorBone };
	for (int e = 0; e < 2; e++)
	{
		ImGui::SetNextItemWidth(200.f);
		if (ImGui::BeginCombo(ends[e], target[e]->c_str()))
		{
			for (size_t i = 0; i < bones.size(); i++)
				if (ImGui::Selectable(bones[i].name.c_str(), bones[i].name == *target[e]))
				{
					const std::string picked = bones[i].name;
					std::string* slot = target[e];
					doc.PushSnapshotEdit(e == 0 ? "Set IK root" : "Set IK effector",
						[&]() { *slot = picked; });
				}
			ImGui::EndCombo();
		}
		if (e == 0)
		{
			ImGui::SameLine();
			if (ImGui::Button("Use selected##root") && doc.selectedBone >= 0)
				doc.PushSnapshotEdit("Set IK root", [&]() { h.rootBone = pv->BoneName(doc.selectedBone); });
		}
		else
		{
			ImGui::SameLine();
			if (ImGui::Button("Use selected##eff") && doc.selectedBone >= 0)
				doc.PushSnapshotEdit("Set IK effector", [&]() { h.effectorBone = pv->BoneName(doc.selectedBone); });
		}
	}

	// Report the resolved chain, since "root and effector are not on the
	// same parent chain" is the one way setup can be wrong and otherwise
	// shows up only as nothing happening.
	int root = -1, effector = -1;
	const bool resolved = ResolveIK(doc, h, root, effector);
	std::vector<p3d::int32> chain;
	if (resolved) chain = p3d::IKSolver::BuildChain(pv->instance, root, effector);

	if (!resolved)
		ImGui::TextColored(ImVec4(1.f, 0.5f, 0.4f, 1.f), "Root or effector is not a bone of this mesh.");
	else if (chain.empty())
		ImGui::TextColored(ImVec4(1.f, 0.5f, 0.4f, 1.f), "Not on the same parent chain - the effector must descend from the root.");
	else
	{
		std::string desc;
		for (size_t i = 0; i < chain.size(); i++)
			desc += (i ? " -> " : "") + pv->BoneName((int)chain[i]);
		ImGui::TextDisabled("%zu bones (%s): %s", chain.size(),
			chain.size() == 3 ? "exact two-bone solve" : "FABRIK", desc.c_str());
	}

	// ---- target --------------------------------------------------------
	ImGui::Separator();
	float t[3] = { h.target.x, h.target.y, h.target.z };
	if (ImGui::DragFloat3("Target", t, 0.05f))
		h.target = p3d::Vec3(t[0], t[1], t[2]);
	if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
	if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit("Move IK target");
	ImGui::SameLine();
	if (ImGui::Button("Snap to effector") && effector >= 0)
		doc.PushSnapshotEdit("Snap IK target to effector",
			[&]() {
				h.target = pv->instance->GetBoneGlobalTransform(effector).GetTranslation();
				h.targetSet = true;
			});

	if (ImGui::Checkbox("Pole", &h.usePole))
	{
		// Checkbox already flipped the value, so the snapshot has to bracket
		// the flip rather than repeat it - push a no-op edit around the state
		// as it now stands.
		const bool now = h.usePole;
		h.usePole = !now;
		doc.PushSnapshotEdit(now ? "Enable IK pole" : "Disable IK pole", [&]() { h.usePole = now; });
	}
	if (h.usePole)
	{
		ImGui::SameLine();
		float pl[3] = { h.pole.x, h.pole.y, h.pole.z };
		ImGui::SetNextItemWidth(220.f);
		if (ImGui::DragFloat3("##pole", pl, 0.05f))
			h.pole = p3d::Vec3(pl[0], pl[1], pl[2]);
		if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
		if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit("Move IK pole");
		ImGui::SetItemTooltip("Which way the knee/elbow points. Only affects the bend direction -\n"
			"it cannot move the effector off the target.");
	}

	// ---- solve and key --------------------------------------------------
	ImGui::Separator();
	const bool solvable = resolved && !chain.empty();
	ImGui::BeginDisabled(!solvable);

	if (ImGui::Button("Solve at playhead"))
	{
		if (const p3d::Animation* clip = doc.ActiveClip())
			pv->instance->ApplyAnimationAtTime(*clip, doc.playhead);
		if (ApplyIK(doc, h))
			for (size_t i = 0; i < chain.size(); i++)
				pv->poseOverrides[(int)chain[i]] = pv->instance->GetBoneLocalTransform(chain[i]);
	}
	ImGui::SetItemTooltip("Poses the chain without keying, so you can look before committing.");

	ImGui::SameLine();
	if (ImGui::Button("Key at playhead"))
	{
		const float time = doc.SnapTime(doc.playhead);
		if (const p3d::Animation* clip = doc.ActiveClip())
			pv->instance->ApplyAnimationAtTime(*clip, time);
		if (ApplyIK(doc, h))
		{
			doc.PushSnapshotEdit("Key IK: " + h.name, [&]() { KeyIKChain(doc, h, time); });
			pv->poseOverrides.clear();
		}
	}

	ImGui::Separator();
	ImGui::TextDisabled("Bake over range");
	ImGui::SetNextItemWidth(90.f);
	ImGui::DragFloat("From", &doc.ikBakeStart, 0.02f, 0.f, doc.clips[doc.activeClip].Duration, "%.2fs");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.f);
	ImGui::DragFloat("To", &doc.ikBakeEnd, 0.02f, 0.f, doc.clips[doc.activeClip].Duration, "%.2fs");
	ImGui::SameLine();
	if (ImGui::Button("Whole clip"))
	{
		doc.ikBakeStart = 0.f;
		doc.ikBakeEnd = doc.clips[doc.activeClip].Duration;
	}

	if (ImGui::Button("Bake"))
	{
		const int keyed = BakeIKOverRange(doc, h, doc.ikBakeStart, doc.ikBakeEnd);
		echo("Animation Editor: baked " + std::to_string(keyed) + " IK keys for '" + h.name + "'");
	}
	ImGui::SetItemTooltip("Solves every frame across the range at the clip's authored fps\n"
		"and stores the result as ordinary rotation keys.\n"
		"The clip is re-sampled before each solve, so the bake does not depend\n"
		"on which direction the range is walked.");
	ImGui::EndDisabled();

	// ---- joint limits ---------------------------------------------------
	// Per bone of this chain, since that is the only place they matter and
	// hunting for a bone in a 45-entry list to clamp a knee is worse than
	// showing the three that can actually bend.
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Joint limits"))
	{
		ImGui::TextDisabled("Degrees. Stops a knee bending backwards - without a limit both\n"
			"solutions reach the target and the solver has no reason to prefer one.");

		for (size_t i = 0; i < chain.size(); i++)
		{
			const std::string boneName = pv->BoneName((int)chain[i]);
			if (boneName.empty()) continue;
			ImGui::PushID((int)i);

			std::map<std::string, p3d::JointLimit>::iterator it = doc.rig.JointLimits.find(boneName);
			bool enabled = (it != doc.rig.JointLimits.end() && it->second.Enabled);
			if (ImGui::Checkbox(boneName.c_str(), &enabled))
			{
				doc.BeginInteractiveEdit();
				if (enabled)
				{
					// A fresh limit starts fully open, so ticking the box
					// cannot itself move the bone - the user then closes it
					// down to taste.
					if (it == doc.rig.JointLimits.end())
					{
						p3d::JointLimit fresh;
						fresh.Enabled = true;
						doc.rig.JointLimits[boneName] = fresh;
					}
					else it->second.Enabled = true;
				}
				else if (it != doc.rig.JointLimits.end()) it->second.Enabled = false;
				doc.rigDirty = true;
				doc.EndInteractiveEdit(enabled ? "Enable joint limit" : "Disable joint limit");
			}

			it = doc.rig.JointLimits.find(boneName);
			if (it != doc.rig.JointLimits.end() && it->second.Enabled)
			{
				p3d::JointLimit& lim = it->second;
				float mn[3] = { (float)RADTODEG(lim.Min.x), (float)RADTODEG(lim.Min.y), (float)RADTODEG(lim.Min.z) };
				float mx[3] = { (float)RADTODEG(lim.Max.x), (float)RADTODEG(lim.Max.y), (float)RADTODEG(lim.Max.z) };
				ImGui::SetNextItemWidth(220.f);
				if (ImGui::DragFloat3("min", mn, 1.f, -180.f, 180.f, "%.0f"))
				{
					lim.Min = p3d::Vec3((f32)DEGTORAD(mn[0]), (f32)DEGTORAD(mn[1]), (f32)DEGTORAD(mn[2]));
					doc.rigDirty = true;
				}
				if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
				if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit("Change joint limit");
				ImGui::SetNextItemWidth(220.f);
				if (ImGui::DragFloat3("max", mx, 1.f, -180.f, 180.f, "%.0f"))
				{
					lim.Max = p3d::Vec3((f32)DEGTORAD(mx[0]), (f32)DEGTORAD(mx[1]), (f32)DEGTORAD(mx[2]));
					doc.rigDirty = true;
				}
				if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
				if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit("Change joint limit");
			}
			ImGui::PopID();
		}
	}

	// ---- the sidecar itself ---------------------------------------------
	ImGui::Separator();
	if (doc.rigPath.empty())
		ImGui::TextDisabled("Bind a mesh to give the rig somewhere to live.");
	else
	{
		if (ImGui::Button("Save Rig"))
		{
			if (doc.SaveRig()) echo("Animation Editor: wrote " + doc.rigPath);
		}
		ImGui::SameLine();
		// The path is worth showing plainly: it is a file the user is meant
		// to be able to open, hand-edit and share between models.
		ImGui::TextDisabled("%s%s", doc.rigPath.c_str(), doc.rigDirty ? " *" : "");
	}
}

void DrawTransport(AnimationEditorDocument& doc, float dt)
{
	const Animation* clip = doc.ActiveClip();
	const float duration = clip ? clip->Duration : 0.f;
	const float frameStep = (doc.snapFps > 0.f ? 1.f / doc.snapFps : 1.f / 30.f);

	// SameLine, unless the next group would run off the right edge - then
	// start a new row. The bar used to be one unconditional chain of
	// SameLine()s, so in a narrow window (a docked tab in a tiled layout, say)
	// everything past the middle was simply unreachable: Key Pose, Key All
	// Bones and Delete Keys were drawn outside the window and could not be
	// clicked at all.
	// Measured from the item just drawn, not from GetContentRegionAvail():
	// once a line has been laid out the cursor is already on the NEXT line, so
	// "avail" is the full window width and the test never fires.
	const auto sameLineOrWrap = [](float groupWidth) {
		const float lastX = ImGui::GetItemRectMax().x;
		const float rightEdge = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
		if (lastX + ImGui::GetStyle().ItemSpacing.x + groupWidth < rightEdge)
			ImGui::SameLine();
	};

	if (ImGui::Button("|<")) SetPlayhead(doc, 0.f);
	ImGui::SameLine();
	if (ImGui::Button("<")) SetPlayhead(doc, doc.playhead - frameStep);
	ImGui::SameLine();
	if (ImGui::Button(doc.playing ? "Pause" : "Play")) doc.playing = !doc.playing;
	ImGui::SameLine();
	if (ImGui::Button("Stop")) { doc.playing = false; SetPlayhead(doc, 0.f); }
	ImGui::SameLine();
	if (ImGui::Button(">")) SetPlayhead(doc, doc.playhead + frameStep);
	ImGui::SameLine();
	if (ImGui::Button(">|")) SetPlayhead(doc, duration);

	sameLineOrWrap(240.f);
	ImGui::Checkbox("Loop", &doc.looping);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.f);
	ImGui::DragFloat("Speed", &doc.playSpeed, 0.01f, -4.f, 4.f, "%.2fx");

	sameLineOrWrap(200.f);
	ImGui::TextUnformatted("|");
	ImGui::SameLine();
	ImGui::Checkbox("Snap", &doc.snapEnabled);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(70.f);
	ImGui::DragFloat("fps", &doc.snapFps, 1.f, 1.f, 240.f, "%.0f");

	sameLineOrWrap(260.f);
	ImGui::TextUnformatted("|");
	ImGui::SameLine();
	ImGui::Checkbox("Auto-key", &doc.autoKey);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Write a key as soon as you finish dragging a bone.");
	ImGui::SameLine();
	ImGui::Checkbox("Pos", &doc.keyPosition);
	ImGui::SameLine();
	ImGui::Checkbox("Rot", &doc.keyRotation);

	sameLineOrWrap(330.f);
	if (ImGui::Button("Key Pose")) KeyPendingPose(doc);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Key every bone you have posed since the last key.");
	ImGui::SameLine();
	if (ImGui::Button("Key All Bones")) KeyWholeSkeleton(doc, doc.playhead);
	ImGui::SameLine();
	if (ImGui::Button("Delete Keys")) DeleteSelectedKeys(doc);

	sameLineOrWrap(200.f);
	ImGui::TextUnformatted("|");
	ImGui::SameLine();
	ImGui::Text("t = %s / %s", FormatTime(doc.playhead).c_str(), FormatTime(duration).c_str());

	const std::map<int, Matrix>* pending = doc.RigPoseOverrides();
	if (pending && !pending->empty())
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.f, 0.8f, 0.2f, 1.f), "  %d bone(s) posed, unkeyed",
			(int)pending->size());
	}

	// Advance playback. Driven off the caller's frame delta rather than a
	// wall clock so a paused/background window doesn't jump when it comes
	// back.
	if (doc.playing && duration > 0.f)
	{
		float t = doc.playhead + dt * doc.playSpeed;
		if (t > duration)
		{
			if (doc.looping) t = std::fmod(t, duration);
			else { t = duration; doc.playing = false; }
		}
		else if (t < 0.f)
		{
			if (doc.looping) t = duration + std::fmod(t, duration);
			else { t = 0.f; doc.playing = false; }
		}
		SetPlayhead(doc, t);
	}
}

void DrawTimeline(AnimationEditorDocument& doc)
{
	const Animation* clip = doc.ActiveClip();
	if (!clip)
	{
		ImGui::TextDisabled("No clip. Use '+ Clip' to start one.");
		return;
	}

	std::vector<TrackRow> tracks;
	BuildTracks(doc, tracks);

	ImGui::Checkbox("Show all bones", &doc.showAllBones);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Also list bones this clip doesn't animate yet, so they can be keyed.");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.f);
	ImGui::DragFloat("Zoom", &doc.pixelsPerSecond, 4.f, 20.f, 2000.f, "%.0f px/s");

	ImGui::BeginChild("##dope", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const float contentWidth = kTrackLabelWidth + clip->Duration * doc.pixelsPerSecond + 40.f;
	const float contentHeight = kRulerHeight + (tracks.size() + 1) * kRowHeight + 8.f;

	// One invisible button covering the whole sheet gives us hover/click
	// state for the custom drawing below without ImGui trying to lay any of
	// it out.
	ImGui::InvisibleButton("##dopecanvas", ImVec2(contentWidth, contentHeight));
	const bool canvasHovered = ImGui::IsItemHovered();
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const float localX = mouse.x - origin.x;
	const float localY = mouse.y - origin.y;

	const float trackX = origin.x + kTrackLabelWidth;
	const float timeToX = doc.pixelsPerSecond;

	// ---- ruler -------------------------------------------------------
	dl->AddRectFilled(ImVec2(origin.x, origin.y),
		ImVec2(origin.x + contentWidth, origin.y + kRulerHeight),
		IM_COL32(38, 40, 48, 255));

	// Tick every second, subdivided at the snap rate when that's legible.
	const float secondsShown = clip->Duration;
	for (int s = 0; s <= (int)std::ceil(secondsShown); s++)
	{
		const float x = trackX + s * timeToX;
		dl->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + kRulerHeight), IM_COL32(120, 124, 140, 255));
		char lbl[16];
		std::snprintf(lbl, sizeof(lbl), "%ds", s);
		dl->AddText(ImVec2(x + 3.f, origin.y + 3.f), IM_COL32(180, 184, 200, 255), lbl);
	}
	if (doc.snapFps > 0.f && timeToX / doc.snapFps >= 6.f)
	{
		const int frames = (int)std::ceil(secondsShown * doc.snapFps);
		for (int f = 0; f <= frames; f++)
		{
			const float x = trackX + (f / doc.snapFps) * timeToX;
			dl->AddLine(ImVec2(x, origin.y + kRulerHeight - 5.f), ImVec2(x, origin.y + kRulerHeight),
				IM_COL32(90, 94, 108, 255));
		}
	}

	// ---- rows --------------------------------------------------------
	float rowY = origin.y + kRulerHeight;

	// Summary row: every key in the clip, whatever bone it belongs to.
	{
		dl->AddRectFilled(ImVec2(origin.x, rowY), ImVec2(origin.x + contentWidth, rowY + kRowHeight),
			IM_COL32(50, 52, 62, 255));
		dl->AddText(ImVec2(origin.x + 4.f, rowY + 2.f), IM_COL32(230, 230, 240, 255), "Summary");
		std::vector<float> times;
		doc.CollectAllKeyTimes(doc.activeClip, times);
		for (size_t i = 0; i < times.size(); i++)
		{
			const float x = trackX + times[i] * timeToX;
			const float y = rowY + kRowHeight * 0.5f;
			dl->AddQuadFilled(ImVec2(x, y - 5.f), ImVec2(x + 5.f, y), ImVec2(x, y + 5.f), ImVec2(x - 5.f, y),
				IM_COL32(220, 220, 235, 255));
		}
		rowY += kRowHeight;
	}

	int hoveredTrack = -1;
	float hoveredTime = 0.f;
	bool hoveredKey = false;

	for (size_t r = 0; r < tracks.size(); r++)
	{
		const TrackRow& row = tracks[r];
		const bool isSelectedBone = (row.boneId >= 0 && row.boneId == doc.selectedBone);
		const ImU32 rowBg = (r % 2 == 0) ? IM_COL32(40, 42, 50, 255) : IM_COL32(44, 46, 55, 255);
		dl->AddRectFilled(ImVec2(origin.x, rowY), ImVec2(origin.x + contentWidth, rowY + kRowHeight),
			isSelectedBone ? IM_COL32(64, 58, 34, 255) : rowBg);

		const ImU32 labelColor = (row.channel >= 0)
			? IM_COL32(220, 224, 235, 255) : IM_COL32(130, 134, 148, 255);
		dl->AddText(ImVec2(origin.x + 4.f + row.depth * 8.f, rowY + 2.f), labelColor, row.boneName.c_str());

		// Key diamonds.
		if (row.channel >= 0)
		{
			std::vector<float> times;
			doc.CollectKeyTimes(doc.activeClip, row.channel, times);
			for (size_t i = 0; i < times.size(); i++)
			{
				const float x = trackX + times[i] * timeToX;
				const float y = rowY + kRowHeight * 0.5f;

				AnimKeyRef ref;
				ref.channel = row.channel;
				ref.time = times[i];
				const bool selected = doc.selectedKeys.count(ref) > 0;
				const ImU32 col = selected ? IM_COL32(255, 200, 60, 255) : IM_COL32(150, 200, 255, 255);
				dl->AddQuadFilled(ImVec2(x, y - 5.f), ImVec2(x + 5.f, y), ImVec2(x, y + 5.f), ImVec2(x - 5.f, y), col);
				dl->AddQuad(ImVec2(x, y - 5.f), ImVec2(x + 5.f, y), ImVec2(x, y + 5.f), ImVec2(x - 5.f, y),
					IM_COL32(20, 22, 28, 255));

				if (canvasHovered && std::fabs(mouse.x - x) <= 6.f && std::fabs(mouse.y - y) <= 7.f)
				{
					hoveredTrack = (int)r;
					hoveredTime = times[i];
					hoveredKey = true;
				}
			}
		}

		// Row hit test for "add key here" clicks.
		if (canvasHovered && !hoveredKey && mouse.y >= rowY && mouse.y < rowY + kRowHeight)
		{
			hoveredTrack = (int)r;
			hoveredTime = (mouse.x - trackX) / timeToX;
		}

		rowY += kRowHeight;
	}

	// ---- playhead ----------------------------------------------------
	{
		const float x = trackX + doc.playhead * timeToX;
		dl->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + contentHeight), IM_COL32(255, 90, 90, 255), 1.5f);
		dl->AddTriangleFilled(ImVec2(x - 5.f, origin.y), ImVec2(x + 5.f, origin.y), ImVec2(x, origin.y + 8.f),
			IM_COL32(255, 90, 90, 255));
	}

	// ---- interaction -------------------------------------------------
	static bool draggingPlayhead = false;
	static bool draggingKeys = false;
	static float dragKeyOrigin = 0.f;

	if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (localY < kRulerHeight)
		{
			draggingPlayhead = true;
		}
		else if (hoveredKey && hoveredTrack >= 0)
		{
			AnimKeyRef ref;
			ref.channel = tracks[hoveredTrack].channel;
			ref.time = hoveredTime;
			if (!ImGui::GetIO().KeyShift) doc.selectedKeys.clear();
			doc.selectedKeys.insert(ref);
			draggingKeys = true;
			dragKeyOrigin = hoveredTime;
			doc.BeginInteractiveEdit();
		}
		else
		{
			doc.selectedKeys.clear();
			// Clicking empty timeline space moves the playhead there - the
			// same gesture as clicking the ruler, which is what people try
			// first.
			SetPlayhead(doc, doc.SnapTime((localX - kTrackLabelWidth) / timeToX));
		}
	}

	if (draggingPlayhead)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
			SetPlayhead(doc, doc.SnapTime((localX - kTrackLabelWidth) / timeToX));
		else draggingPlayhead = false;
	}

	if (draggingKeys)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			const float target = doc.SnapTime((localX - kTrackLabelWidth) / timeToX);
			if (std::fabs(target - dragKeyOrigin) > kAnimKeyEpsilon)
			{
				const float delta = target - dragKeyOrigin;
				// Move every selected column by the same delta, and rebuild
				// the selection at the new times - selection is by time, so
				// it would otherwise still point at where the keys were.
				std::vector<AnimKeyRef> targets(doc.selectedKeys.begin(), doc.selectedKeys.end());
				std::set<AnimKeyRef> moved;
				for (size_t i = 0; i < targets.size(); i++)
				{
					const float to = std::max(0.f, targets[i].time + delta);
					if (doc.MoveKeysAtTime(doc.activeClip, targets[i].channel, targets[i].time, to))
					{
						AnimKeyRef ref;
						ref.channel = targets[i].channel;
						ref.time = to;
						moved.insert(ref);
					}
					else moved.insert(targets[i]);
				}
				doc.selectedKeys = moved;
				dragKeyOrigin = target;
				doc.dirty = true;
			}
		}
		else
		{
			draggingKeys = false;
			doc.EndInteractiveEdit("Move keys");
		}
	}

	// Double-click on an empty row cell: key that bone right there.
	if (canvasHovered && !hoveredKey && hoveredTrack >= 0
		&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		const TrackRow& row = tracks[hoveredTrack];
		const float t = doc.SnapTime(hoveredTime);
		if (row.boneId >= 0)
		{
			doc.PushSnapshotEdit("Key bone '" + row.boneName + "'", [&]() {
				KeyBoneAtTime(doc, row.boneId, t);
			});
		}
	}

	// Right-click a key: open the key menu on it (and any other selected
	// ones). This used to delete immediately, which left nowhere to put
	// per-key interpolation - and made a mis-aimed right-click destructive.
	if (canvasHovered && hoveredKey && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && hoveredTrack >= 0)
	{
		AnimKeyRef ref;
		ref.channel = tracks[hoveredTrack].channel;
		ref.time = hoveredTime;
		if (doc.selectedKeys.count(ref) == 0)
		{
			doc.selectedKeys.clear();
			doc.selectedKeys.insert(ref);
		}
		ImGui::OpenPopup("##keymenu");
	}

	if (ImGui::BeginPopup("##keymenu"))
	{
		const int keyCount = (int)doc.selectedKeys.size();
		ImGui::TextDisabled(keyCount == 1 ? "1 key" : "%d keys", keyCount);
		ImGui::Separator();

		// Interpolation of the first selected key, shown as the current
		// state. A mixed selection just displays the first one's mode -
		// picking any entry applies it to all of them, which is the useful
		// operation regardless of what they started as.
		int mode = p3d::INTERP_LINEAR;
		float inTan = 1.f, outTan = 1.f;
		if (!doc.selectedKeys.empty())
		{
			const AnimKeyRef& first = *doc.selectedKeys.begin();
			doc.GetKeyInterpolation(doc.activeClip, first.channel, first.time, mode, inTan, outTan);
		}

		if (ImGui::BeginMenu("Interpolation"))
		{
			// Names come from p3d::InterpolationModeName so this menu, the
			// particle inspector and the curve thumbnail cannot disagree
			// about what a mode is called.
			for (unsigned m = 0; m < p3d::kInterpolationModeCount; m++)
			{
				const char* label = p3d::InterpolationModeName((uchar)m);
				if (ImGui::MenuItem(label, NULL, mode == (int)m))
				{
					std::vector<AnimKeyRef> targets(doc.selectedKeys.begin(), doc.selectedKeys.end());
					doc.PushSnapshotEdit(std::string("Set interpolation: ") + label, [&]() {
						for (size_t i = 0; i < targets.size(); i++)
							doc.SetKeyInterpolation(doc.activeClip, targets[i].channel, targets[i].time,
								(int)m, inTan, outTan);
					});
					mode = (int)m;
				}
			}
			ImGui::EndMenu();
		}

		// Drawn from p3d::Ease(), the same function the sampler runs, so
		// what is pictured is what the clip will do. Sits under the menu
		// rather than inside it so it stays visible while the tangent
		// sliders below are dragged.
		ImGui::TextDisabled("%s", p3d::InterpolationModeName((uchar)mode));
		EasingUI::Curve((uchar)mode, outTan, inTan);

		// Tangents only mean anything for Bezier, so they are only offered
		// there rather than sitting inert next to every other mode.
		if (mode == p3d::INTERP_BEZIER)
		{
			ImGui::Separator();
			ImGui::TextDisabled("Bezier tangents");
			ImGui::SetNextItemWidth(120.f);
			const bool a = ImGui::SliderFloat("Out", &outTan, 0.f, 3.f, "%.2f");
			if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
			if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit("Change out tangent");
			ImGui::SetNextItemWidth(120.f);
			const bool b = ImGui::SliderFloat("In", &inTan, 0.f, 3.f, "%.2f");
			if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
			if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit("Change in tangent");
			if (a || b)
			{
				// Applied live so the curve can be felt against the preview;
				// the undo entry is the interactive one bracketing the drag,
				// so a whole drag collapses to a single Ctrl+Z rather than
				// one entry per mouse-move.
				for (std::set<AnimKeyRef>::const_iterator it = doc.selectedKeys.begin(); it != doc.selectedKeys.end(); ++it)
					doc.SetKeyInterpolation(doc.activeClip, it->channel, it->time, mode, inTan, outTan);
				doc.clipsRevision++;
				doc.dirty = true;
			}
		}

		ImGui::Separator();
		if (ImGui::MenuItem(keyCount == 1 ? "Delete key" : "Delete keys"))
			DeleteSelectedKeys(doc);
		ImGui::EndPopup();
	}

	if (canvasHovered && hoveredKey && !ImGui::IsPopupOpen("##keymenu"))
	{
		int mode = p3d::INTERP_LINEAR;
		float inTan = 1.f, outTan = 1.f;
		static const char* kShortNames[] = { "Linear", "Step", "Ease In", "Ease Out", "Ease In/Out", "Bezier" };
		const bool have = doc.GetKeyInterpolation(doc.activeClip, tracks[hoveredTrack].channel, hoveredTime, mode, inTan, outTan);
		ImGui::SetTooltip("%s @ %s\nInterpolation: %s\nDrag to retime, right-click for options",
			tracks[hoveredTrack].boneName.c_str(), FormatTime(hoveredTime).c_str(),
			have && mode >= 0 && mode < IM_ARRAYSIZE(kShortNames) ? kShortNames[mode] : "Linear");
	}

	ImGui::EndChild();
}

} // namespace

// ---- pieces the 2D animation window reuses ---------------------------------
// Thin forwarders out of the anonymous namespace above. The 2D editor is the
// same dope sheet, transport and bone list driven against a scene rig instead
// of a preview viewport (see AnimationEditorDocument::externalRig); giving it its
// own copies would be a second implementation of key selection, retiming and
// interpolation, which is a second set of bugs.

void DrawTimelinePanel(AnimationEditorDocument& doc) { DrawTimeline(doc); }
void DrawTransportBar(AnimationEditorDocument& doc, float dt) { DrawTransport(doc, dt); }
void DrawSkeletonPanel(AnimationEditorDocument& doc, const char* emptyHint)
{
	DrawBonePanel(doc, emptyHint);
}

void ApplyTimelinePose(AnimationEditorDocument& doc)
{
	SkeletonAnimationInstance* inst = doc.RigInstance();
	if (!inst) return;

	const Animation* clip = doc.ActiveClip();
	if (clip) inst->ApplyAnimationAtTime(*clip, doc.playhead);
	else      inst->ResetToBindPose();

	// Bones posed but not yet keyed go back on top, so scrubbing does not
	// throw away work in progress.
	std::map<int, Matrix>* overrides = doc.RigPoseOverrides();
	if (overrides && !overrides->empty())
	{
		for (std::map<int, Matrix>::const_iterator it = overrides->begin(); it != overrides->end(); ++it)
			if (it->first >= 0 && it->first < (int)inst->GetNumberBones())
				inst->SetBoneLocalTransform(it->first, it->second);
		// One hierarchy walk for the whole batch rather than per bone.
		inst->RefreshSkinning();
	}
}

void DrawWindow(AnimationEditorDocument& doc, const std::vector<AnimationMeshChoice>& meshes,
	float dt, FrameRequests& requests)
{
	EnsurePreview(doc);
	AnimationPreview* pv = doc.preview.get();

	DrawToolbar(doc, meshes, requests);
	ImGui::Separator();

	// Viewport + bone panel share the space above the timeline.
	const float availH = ImGui::GetContentRegionAvail().y;
	const float upperH = std::max(180.f, availH - kTimelineHeight);

	ImGui::BeginChild("##upper", ImVec2(0, upperH), false);
	DrawBonePanel(doc);
	ImGui::SameLine();

	ImGui::BeginChild("##viewport", ImVec2(0, 0), true);
	if (pv)
	{
		// Size the offscreen target to the panel so the image is never
		// stretched. Clamped: a collapsed panel would otherwise ask for a
		// zero (or negative) sized framebuffer.
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		pv->width = std::max(64, (int)avail.x);
		pv->height = std::max(64, (int)avail.y - 26);

		// Toolbar over the viewport.
		if (ImGui::RadioButton("Rotate", pv->poseMode == BonePoseMode::Rotate))
			pv->SetPoseMode(BonePoseMode::Rotate);
		ImGui::SameLine();
		if (ImGui::RadioButton("Translate", pv->poseMode == BonePoseMode::Translate))
			pv->SetPoseMode(BonePoseMode::Translate);
		ImGui::SameLine();
		ImGui::Checkbox("Bones", &pv->showBones);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.f);
		int styleIdx = (pv->boneStyle == AnimationPreview::BoneDrawStyle::Octahedral) ? 0 : 1;
		if (ImGui::Combo("##bonestyle", &styleIdx, "Octahedral\0Stick\0"))
			pv->boneStyle = (styleIdx == 0) ? AnimationPreview::BoneDrawStyle::Octahedral
			                                : AnimationPreview::BoneDrawStyle::Stick;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Octahedral bones show which way a joint is rolled;\nStick is easier to read on dense rigs like fingers.");
		ImGui::SameLine();
		ImGui::Checkbox("Mesh", &pv->showMesh);
		ImGui::SameLine();
		ImGui::Checkbox("Grid", &pv->showGrid);
		ImGui::SameLine();
		ImGui::TextDisabled("(MMB orbit, RMB pan, wheel zoom)");

		// Pose the rig for this frame before it is drawn.
		pv->SyncPose(doc, dt);
		SeedUnplacedIKTargets(doc);

		const AnimationPreview::Interaction act = pv->DrawAndUpdate(doc);
		if (act.posed) doc.selectedBoneName = pv->BoneName(doc.selectedBone);
		if (act.boneDragEnded && doc.autoKey)
			KeyPendingPose(doc);

		// Dragging the IK target re-solves the chain every frame, so the rig
		// follows the handle instead of only catching up when a button is
		// pressed. The whole drag is one undo entry: BeginInteractiveEdit
		// captures the target as it was at mouse-down, and the release
		// pushes a single "Move IK target".
		// Taken on the grab frame, not the first move frame - see
		// Interaction::ikDragStarted.
		if (act.ikDragStarted) doc.BeginInteractiveEdit();
		if (act.ikTargetMoved && doc.HasActiveIK())
		{
			const AnimationEditorDocument::IKHandle& h = doc.ikHandles[doc.activeIK];
			// Re-pose from the clip first: the solve has to start from this
			// frame's animated pose, not from the pose the previous solve
			// left behind, or repeated drags accumulate on themselves.
			if (const Animation* clip = doc.ActiveClip())
				pv->instance->ApplyAnimationAtTime(*clip, doc.playhead);
			if (ApplyIK(doc, h))
			{
				int root = -1, effector = -1;
				if (ResolveIK(doc, h, root, effector))
				{
					const std::vector<int32> chain = IKSolver::BuildChain(pv->instance, root, effector);
					for (size_t i = 0; i < chain.size(); i++)
						pv->poseOverrides[(int)chain[i]] = pv->instance->GetBoneLocalTransform(chain[i]);
				}
			}
		}
		if (act.ikDragEnded)
		{
			doc.EndInteractiveEdit("Move IK target");
			if (doc.autoKey && doc.HasActiveIK())
			{
				const AnimationEditorDocument::IKHandle& h = doc.ikHandles[doc.activeIK];
				doc.PushSnapshotEdit("Key IK: " + h.name, [&]() {
					KeyIKChain(doc, h, doc.SnapTime(doc.playhead));
				});
				pv->poseOverrides.clear();
			}
		}
	}
	ImGui::EndChild();
	ImGui::EndChild();

	ImGui::Separator();

	// Timeline vs Blend. Two different jobs on the same rig: authoring a
	// single clip's keys, versus tuning how several finished clips mix. They
	// share the viewport above and swap the panel below.
	if (ImGui::RadioButton("Timeline", !doc.blendMode && !doc.ikMode))
	{
		doc.blendMode = false;
		doc.ikMode = false;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Blend", doc.blendMode) && !doc.blendMode)
	{
		doc.blendMode = true;
		doc.ikMode = false;
		// Force a rebuild on entry so the blend starts from a known state
		// rather than whatever was playing when it was last left.
		doc.TouchBlend();
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("IK", doc.ikMode) && !doc.ikMode)
	{
		doc.ikMode = true;
		doc.blendMode = false;
	}
	ImGui::SameLine();
	ImGui::TextDisabled(doc.blendMode
		? "| previewing the engine's real blend - weights are driven by script in game"
		: (doc.ikMode
			? "| solve a chain onto a target and bake the result as ordinary keys"
			: "| scrub and key one clip"));
	ImGui::Separator();

	if (doc.blendMode)
	{
		// Either counter moving means the stored blend is stale - see
		// blendDataRevision for why a rebuild is not the same question.
		const uint32_t beforeRev = doc.blendRevision;
		const uint32_t beforeData = doc.blendDataRevision;
		DrawBlendPanel(doc, requests);
		if (doc.blendRevision != beforeRev || doc.blendDataRevision != beforeData)
			requests.blendChanged = true;
	}
	else if (doc.ikMode)
	{
		// Transport stays available: the playhead is what "solve at
		// playhead" and the bake range are relative to.
		DrawTransport(doc, dt);
		DrawIKPanel(doc);
	}
	else
	{
		DrawTransport(doc, dt);
		DrawTimeline(doc);
	}

	// Delete key removes the selected key columns, but only while this
	// window has focus - otherwise it would fire while the user is typing a
	// clip name or working in another panel.
	if (!doc.blendMode
		&& ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
		&& !ImGui::GetIO().WantTextInput
		&& ImGui::IsKeyPressed(ImGuiKey_Delete))
		DeleteSelectedKeys(doc);
}

} // namespace AnimationEditor
