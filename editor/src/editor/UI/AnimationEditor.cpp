//=============================================================================
// Name        : AnimationEditor.cpp
// Description : See AnimationEditor.h.
//=============================================================================

#include "AnimationEditor.h"
#include "../AnimationEditorDocument.h"
#include "../AnimationPreview.h"

#include <Pyros3D/AnimationManager/IKSolver.h>
#include "EasingPreview.h"
#include "../PoseTools.h"
#include "../RetargetSession.h"

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
		h.targetRotation = pv->instance->GetBoneGlobalTransform(eff).ConvertToQuaternion();
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
	return p3d::IKSolver::SolveWithEndRotation(pv->instance, root, effector, h.target,
		h.usePole ? h.pole : p3d::Vec3(0.f, 0.f, 0.f), h.holdRotation ? &h.targetRotation : NULL, 30,
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
	// Events are addressed by index: remove the highest first so the rest
	// still point where they did.
	std::stable_sort(targets.begin(), targets.end(), [](const AnimKeyRef& a, const AnimKeyRef& b) {
		if ((a.kind == ANIM_KEY_EVENT) != (b.kind == ANIM_KEY_EVENT)) return b.kind == ANIM_KEY_EVENT;
		return a.kind == ANIM_KEY_EVENT && a.channel > b.channel;
	});
	int removed = 0;
	doc.PushSnapshotEdit(targets.size() == 1 ? "Delete key" : "Delete " + std::to_string(targets.size()) + " keys", [&]() {
		for (size_t i = 0; i < targets.size(); i++)
			if (doc.DeleteKeyRef(doc.activeClip, targets[i])) removed++;
		doc.PruneEmptyChannels(doc.activeClip);
		doc.PruneEmptyTracks(doc.activeClip);
	});
	doc.selectedKeys.clear();
	return removed;
}

namespace {
// Local transforms by BONE NAME, so a pose copied on one rig pastes onto the
// bones of the same name on another (a re-imported model, a 2D character).
std::map<std::string, Matrix>& PoseClipboard()
{
	static std::map<std::string, Matrix> pose;
	return pose;
}
}

bool HasPoseClipboard() { return !PoseClipboard().empty(); }

int ApplyPoseOp(AnimationEditorDocument& doc, PoseOp op)
{
	SkeletonAnimationInstance* inst = doc.RigInstance();
	if (!inst) return 0;
	const unsigned n = inst->GetNumberBones();
	std::vector<Matrix> locals(n);
	for (unsigned b = 0; b < n; b++) locals[b] = inst->GetBoneLocalTransform((int)b);

	if (op == POSE_COPY)
	{
		PoseClipboard().clear();
		for (unsigned b = 0; b < n; b++) PoseClipboard()[doc.RigBoneName((int)b)] = locals[b];
		return (int)n;
	}

	std::vector<Matrix> result = locals;
	std::vector<int> touched;
	if (op == POSE_PASTE || op == POSE_PASTE_MIRRORED)
	{
		if (PoseClipboard().empty()) return 0;
		std::vector<Matrix> src = locals;
		for (unsigned b = 0; b < n; b++)
		{
			std::map<std::string, Matrix>::const_iterator it = PoseClipboard().find(doc.RigBoneName((int)b));
			if (it != PoseClipboard().end()) src[b] = it->second;
		}
		result = op == POSE_PASTE ? src : PoseTools::MirrorPose(inst, src, 0);
		for (unsigned b = 0; b < n; b++) touched.push_back((int)b);
	}
	else if (op == POSE_MIRROR)
	{
		result = PoseTools::MirrorPose(inst, locals, 0);
		for (unsigned b = 0; b < n; b++) touched.push_back((int)b);
	}
	else if (op == POSE_MIRROR_BONE_TO_OTHER_SIDE)
	{
		const int other = PoseTools::CounterpartBone(inst, doc.selectedBone);
		if (other < 0 || other == doc.selectedBone) return 0;
		const std::vector<Matrix> mirrored = PoseTools::MirrorPose(inst, locals, 0);
		// Only the counterpart takes it - its mirrored pose is the selected
		// bone's, reflected.
		result = locals;
		result[other] = mirrored[other];
		touched.push_back(other);
	}

	std::map<int, Matrix>* ov = doc.RigPoseOverrides();
	for (size_t i = 0; i < touched.size(); i++)
	{
		const int b = touched[i];
		inst->SetBoneLocalTransform(b, result[b]);
		if (ov) (*ov)[b] = result[b];
	}
	inst->RefreshSkinning();
	return (int)touched.size();
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

void DrawRetargetPopup(AnimationEditorDocument& doc, const std::vector<AnimationMeshChoice>& meshes)
{
	ImGui::SetNextWindowSize(ImVec2(520.f, 560.f), ImGuiCond_Appearing);
	doc.retargetWindowOpen = ImGui::BeginPopupModal("Retarget clips##retarget", NULL, 0);
	if (!doc.retargetWindowOpen) return;
	static std::string clipChoice, rigChoice;

	ImGui::TextWrapped("Clips keyed for another skeleton, re-keyed for this rig. Each bone takes its source bone's turn from "
		"bind; this rig keeps its own proportions, and the root's travel is scaled by the hip heights.");
	if (!doc.RigInstance()) { ImGui::TextColored(ImVec4(1.f, 0.5f, 0.4f, 1.f), "Bind a rig first (the Rig box)."); }
	ImGui::Separator();

	const auto labelOf = [](const std::vector<AnimationMeshChoice>& list, const std::string& abs) {
		for (size_t i = 0; i < list.size(); i++) if (list[i].second == abs) return list[i].first;
		return abs.empty() ? std::string("(choose)") : abs;
	};
	ImGui::SetNextItemWidth(320.f);
	if (ImGui::BeginCombo("Source clips", labelOf(doc.clipFileChoices, clipChoice).c_str()))
	{
		for (size_t i = 0; i < doc.clipFileChoices.size(); i++)
			if (doc.clipFileChoices[i].second != doc.absolutePath
				&& ImGui::Selectable(doc.clipFileChoices[i].first.c_str(), doc.clipFileChoices[i].second == clipChoice))
				clipChoice = doc.clipFileChoices[i].second;
		ImGui::EndCombo();
	}
	ImGui::SetNextItemWidth(320.f);
	if (ImGui::BeginCombo("Their rig", labelOf(meshes, rigChoice).c_str()))
	{
		for (size_t i = 0; i < meshes.size(); i++)
			if (ImGui::Selectable(meshes[i].first.c_str(), meshes[i].second == rigChoice)) rigChoice = meshes[i].second;
		ImGui::EndCombo();
	}
	ImGui::SetItemTooltip("The skinned model those clips were authored on - its bind pose is what\nthe clips' rotations are measured against.");
	ImGui::BeginDisabled(clipChoice.empty() || rigChoice.empty() || !doc.RigInstance());
	if (ImGui::Button("Load"))
	{
		doc.retarget.reset(new RetargetSession());
		doc.retarget->Load(clipChoice, rigChoice, doc.RigInstance());
	}
	ImGui::EndDisabled();

	RetargetSession* rs = doc.retarget.get();
	if (rs && !rs->error.empty()) ImGui::TextColored(ImVec4(1.f, 0.6f, 0.4f, 1.f), "%s", rs->error.c_str());
	if (rs && rs->Loaded() && doc.RigInstance())
	{
		ImGui::Separator();
		ImGui::TextUnformatted("Clips");
		for (size_t i = 0; i < rs->clips.size(); i++)
		{
			bool c = rs->chosen[i];
			if (ImGui::Checkbox((rs->clips[i].AnimationName + "##rc" + std::to_string(i)).c_str(), &c)) rs->chosen[i] = c;
		}
		ImGui::SetNextItemWidth(90.f);
		ImGui::DragFloat("Sample fps", &rs->fps, 1.f, 1.f, 120.f, "%.0f");

		ImGui::Separator();
		ImGui::Text("Bone map (%d of %u bones)", (int)rs->map.size(), doc.RigBoneCount());
		const std::vector<Bone>& sb = rs->source->GetSkeletonBones();
		ImGui::BeginChild("##rmap", ImVec2(0, -34.f), true);
		for (unsigned b = 0; b < doc.RigBoneCount(); b++)
		{
			const std::string tname = doc.RigBoneName((int)b);
			std::map<std::string, std::string>::iterator it = rs->map.find(tname);
			ImGui::PushID((int)b);
			ImGui::TextUnformatted(tname.c_str());
			ImGui::SameLine(220.f);
			ImGui::SetNextItemWidth(-1.f);
			if (ImGui::BeginCombo("##src", it == rs->map.end() ? "(not driven)" : it->second.c_str()))
			{
				if (ImGui::Selectable("(not driven)", it == rs->map.end())) rs->map.erase(tname);
				for (size_t i = 0; i < sb.size(); i++)
					if (ImGui::Selectable(sb[i].name.c_str(), it != rs->map.end() && it->second == sb[i].name))
						rs->map[tname] = sb[i].name;
				ImGui::EndCombo();
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
		int n = 0;
		for (size_t i = 0; i < rs->chosen.size(); i++) if (rs->chosen[i]) n++;
		ImGui::BeginDisabled(n == 0 || rs->map.empty());
		if (ImGui::Button(("Import " + std::to_string(n) + " clip(s)").c_str()))
		{
			std::vector<std::string> added;
			doc.PushSnapshotEdit("Retarget clips", [&]() { added = rs->ImportInto(doc); });
			doc.selectedKeys.clear();
			SetPlayhead(doc, 0.f);
			echo("Animation Editor: retargeted " + std::to_string(added.size()) + " clip(s) onto this rig");
			doc.retarget.reset();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
	}
	if (ImGui::Button("Close")) { doc.retarget.reset(); ImGui::CloseCurrentPopup(); }
	ImGui::EndPopup();
}

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

			// Root motion: the root's travel leaves the pose and moves the
			// object instead (consumeRootMotion / setApplyRootMotion in Lua).
			ImGui::Separator();
			bool rootMotion = clip.HasFlag(p3d::ANIM_FLAG_ROOT_MOTION);
			if (ImGui::Checkbox("Root motion", &rootMotion))
			{
				doc.PushSnapshotEdit(rootMotion ? "Enable root motion" : "Disable root motion", [&]() {
					if (rootMotion) clip.Flags |= p3d::ANIM_FLAG_ROOT_MOTION;
					else            clip.Flags &= ~(uint32_t)p3d::ANIM_FLAG_ROOT_MOTION;
				});
				doc.clipsRevision++;
			}
			ImGui::SetItemTooltip("The root bone's travel on the ground is taken out of the pose and given to the\n"
				"game: the mesh walks in place and the object moves by exactly what the feet covered.");
			if (rootMotion)
			{
				ImGui::SetNextItemWidth(200.f);
				const std::string shown = clip.RootMotionBone.empty() ? std::string("(first root bone)") : clip.RootMotionBone;
				if (ImGui::BeginCombo("Root bone", shown.c_str()))
				{
					if (ImGui::Selectable("(first root bone)", clip.RootMotionBone.empty()))
						doc.PushSnapshotEdit("Root motion bone", [&]() { clip.RootMotionBone.clear(); });
					for (unsigned b = 0; b < doc.RigBoneCount(); b++)
					{
						const std::string name = doc.RigBoneName((int)b);
						if (ImGui::Selectable(name.c_str(), name == clip.RootMotionBone))
							doc.PushSnapshotEdit("Root motion bone", [&]() { clip.RootMotionBone = name; });
					}
					ImGui::EndCombo();
				}
			}

			// Events, listed: the timeline's Events row is where they are
			// placed, this is where they are read and named precisely.
			ImGui::Separator();
			ImGui::Text("Events (%d)", (int)clip.Events.size());
			int removeEvent = -1;
			for (size_t e = 0; e < clip.Events.size(); e++)
			{
				ImGui::PushID((int)e);
				float t = clip.Events[e].Time;
				ImGui::SetNextItemWidth(70.f);
				if (ImGui::DragFloat("##t", &t, 0.01f, 0.f, clip.Duration, "%.2fs"))
				{
					clip.Events[e].Time = std::max(0.f, std::min(t, clip.Duration));
					doc.dirty = true;
				}
				if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
				if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit("Retime event");
				ImGui::SameLine();
				std::string name = clip.Events[e].Name;
				ImGui::SetNextItemWidth(130.f);
				if (ImGui::InputText("##n", &name) && !name.empty()) { clip.Events[e].Name = name; doc.dirty = true; }
				if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
				if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit("Rename event");
				ImGui::SameLine();
				if (ImGui::SmallButton("x")) removeEvent = (int)e;
				ImGui::PopID();
			}
			if (removeEvent >= 0)
				doc.PushSnapshotEdit("Delete event", [&]() { doc.RemoveEvent(doc.activeClip, removeEvent); });
			if (ImGui::SmallButton("+ Event at playhead"))
			{
				const float t = doc.SnapTime(doc.playhead);
				doc.PushSnapshotEdit("Add event", [&]() { doc.AddEvent(doc.activeClip, t, "event"); });
			}
			ImGui::EndPopup();
		}
	}

	ImGui::SameLine();
	ImGui::TextUnformatted("|");
	ImGui::SameLine();
	if (ImGui::Button("Save")) requests.save = true;
	ImGui::SameLine();
	if (ImGui::Button("Save As...")) requests.saveAs = true;
	ImGui::SameLine();
	if (ImGui::Button("Retarget...")) ImGui::OpenPopup("Retarget clips##retarget");
	ImGui::SetItemTooltip("Bring clips keyed for another skeleton onto this rig.");
	DrawRetargetPopup(doc, meshes);
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
		const int other = PoseTools::CounterpartBone(inst, doc.selectedBone);
		if (other >= 0 && other != doc.selectedBone)
		{
			if (ImGui::Button("Mirror to other side"))
				ApplyPoseOp(doc, POSE_MIRROR_BONE_TO_OTHER_SIDE);
			ImGui::SetItemTooltip("Puts this bone's pose, mirrored, on '%s' (unkeyed until Key Pose).",
				bones[other].name.c_str());
		}
	}
	else ImGui::TextDisabled("No bone selected.\nClick a joint in the viewport.");

	// Whole-pose tools. Results land as an unkeyed pose (Key Pose commits it),
	// so they can be looked at first.
	ImGui::Separator();
	if (ImGui::SmallButton("Copy Pose")) ApplyPoseOp(doc, POSE_COPY);
	ImGui::SameLine();
	if (ImGui::SmallButton("Paste")) ApplyPoseOp(doc, POSE_PASTE);
	ImGui::SameLine();
	if (ImGui::SmallButton("Paste Mirrored")) ApplyPoseOp(doc, POSE_PASTE_MIRRORED);
	if (ImGui::SmallButton("Mirror Pose")) ApplyPoseOp(doc, POSE_MIRROR);
	ImGui::SetItemTooltip("Left and right swap, mirrored across the model's X = 0 plane.\n"
		"Sides are found by name: Left/Right, .L/.R, _L/_R, L_/R_.");

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
		h.targetRotation = pv->instance->GetBoneGlobalTransform(
			doc.selectedBone >= 0 ? doc.selectedBone : 0).ConvertToQuaternion();
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
				h.targetRotation = pv->instance->GetBoneGlobalTransform(effector).ConvertToQuaternion();
				h.targetSet = true;
			});

	if (ImGui::Checkbox("Hold rotation", &h.holdRotation))
	{
		const bool now = h.holdRotation;
		h.holdRotation = !now;
		doc.PushSnapshotEdit(now ? "Hold IK rotation" : "Release IK rotation", [&]() { h.holdRotation = now; });
	}
	ImGui::SetItemTooltip("Keep the effector turned the way it was when the target was last snapped to it -\n"
		"a planted foot stays flat while the hips move, instead of tilting with the shin.");
	if (h.holdRotation)
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("Capture current") && effector >= 0)
			doc.PushSnapshotEdit("Capture IK rotation", [&]() {
				h.targetRotation = pv->instance->GetBoneGlobalTransform(effector).ConvertToQuaternion();
			});
	}

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

// One row of the dope sheet.
struct SheetRow {
	enum Type { SUMMARY, EVENTS, BONE, COMPONENT, TRACK };
	Type type = BONE;
	std::string label;
	int channel = -1;     // BONE/COMPONENT: channel index (-1 = bone not animated yet)
	int boneId = -1;
	int depth = 0;
	int kind = ANIM_KEY_COLUMN;   // COMPONENT: which one
	int track = -1;       // TRACK: index into the clip's Tracks
};

// A diamond (or flag) drawn this frame, for hit testing and the marquee.
struct DrawnKey {
	ImVec2 pos;
	AnimKeyRef ref;
	bool summary = false; // a summary diamond stands for every key at its time
};

// Ruler label spacing that stays legible at any zoom: the smallest of these
// steps that puts at least 56 px between labels.
float RulerStep(float pixelsPerSecond)
{
	static const float steps[] = { 0.05f, 0.1f, 0.25f, 0.5f, 1.f, 2.f, 5.f, 10.f, 15.f, 30.f, 60.f, 120.f, 300.f };
	for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++)
		if (steps[i] * pixelsPerSecond >= 56.f) return steps[i];
	return 600.f;
}

const char* ComponentLabel(int kind)
{
	return kind == ANIM_KEY_POSITION ? "Position" : kind == ANIM_KEY_ROTATION ? "Rotation" : "Scale";
}

// Interpolation of whatever a ref stands for (events have none).
bool GetRefInterpolation(const AnimationEditorDocument& doc, const AnimKeyRef& r, int& mode, float& in, float& out)
{
	const Animation* clip = doc.ActiveClip();
	if (!clip) return false;
	if (r.kind == ANIM_KEY_COLUMN) return doc.GetKeyInterpolation(doc.activeClip, r.channel, r.time, mode, in, out);
	if (r.kind == ANIM_KEY_TRACK)
	{
		if (r.channel < 0 || r.channel >= (int)clip->Tracks.size()) return false;
		const std::vector<PropertyKey>& k = clip->Tracks[r.channel].Keys;
		for (size_t i = 0; i < k.size(); i++)
			if (std::fabs(k[i].Time - r.time) <= kAnimKeyEpsilon) { mode = k[i].Mode; in = out = 1.f; return true; }
		return false;
	}
	if (r.kind >= ANIM_KEY_POSITION && r.kind <= ANIM_KEY_SCALE)
	{
		if (r.channel < 0 || r.channel >= (int)clip->Channels.size()) return false;
		const Channel& ch = clip->Channels[r.channel];
		const auto find = [&](const auto& keys) {
			for (size_t i = 0; i < keys.size(); i++)
				if (std::fabs(keys[i].Time - r.time) <= kAnimKeyEpsilon)
				{ mode = keys[i].Mode; in = keys[i].InTangent; out = keys[i].OutTangent; return true; }
			return false;
		};
		if (r.kind == ANIM_KEY_POSITION) return find(ch.positions);
		if (r.kind == ANIM_KEY_ROTATION) return find(ch.rotations);
		return find(ch.scales);
	}
	return false;
}

void SetRefInterpolation(AnimationEditorDocument& doc, const AnimKeyRef& r, int mode, float in, float out)
{
	Animation* clip = doc.ActiveClip();
	if (!clip) return;
	if (r.kind == ANIM_KEY_COLUMN) { doc.SetKeyInterpolation(doc.activeClip, r.channel, r.time, mode, in, out); return; }
	if (r.kind == ANIM_KEY_TRACK)
	{
		if (r.channel < 0 || r.channel >= (int)clip->Tracks.size()) return;
		std::vector<PropertyKey>& k = clip->Tracks[r.channel].Keys;
		for (size_t i = 0; i < k.size(); i++)
			if (std::fabs(k[i].Time - r.time) <= kAnimKeyEpsilon) k[i].Mode = (uchar)mode;
		return;
	}
	if (r.kind >= ANIM_KEY_POSITION && r.kind <= ANIM_KEY_SCALE)
	{
		if (r.channel < 0 || r.channel >= (int)clip->Channels.size()) return;
		Channel& ch = clip->Channels[r.channel];
		const auto set = [&](auto& keys) {
			for (size_t i = 0; i < keys.size(); i++)
				if (std::fabs(keys[i].Time - r.time) <= kAnimKeyEpsilon)
				{ keys[i].Mode = (uchar)mode; keys[i].InTangent = in; keys[i].OutTangent = out; }
		};
		if (r.kind == ANIM_KEY_POSITION) set(ch.positions);
		else if (r.kind == ANIM_KEY_ROTATION) set(ch.rotations);
		else set(ch.scales);
	}
}

// Every key in the active clip at ~time, of every kind - what a summary
// diamond selects.
void RefsAtTime(const AnimationEditorDocument& doc, float time, std::vector<AnimKeyRef>& out)
{
	const Animation* clip = doc.ActiveClip();
	if (!clip) return;
	std::vector<float> times;
	for (size_t c = 0; c < clip->Channels.size(); c++)
	{
		if (!doc.HasKeyAtTime(doc.activeClip, (int)c, time)) continue;
		AnimKeyRef r; r.channel = (int)c; r.time = time; out.push_back(r);
	}
	for (size_t t = 0; t < clip->Tracks.size(); t++)
		for (size_t k = 0; k < clip->Tracks[t].Keys.size(); k++)
			if (std::fabs(clip->Tracks[t].Keys[k].Time - time) <= kAnimKeyEpsilon)
			{ AnimKeyRef r; r.kind = ANIM_KEY_TRACK; r.channel = (int)t; r.time = clip->Tracks[t].Keys[k].Time; out.push_back(r); }
	for (size_t e = 0; e < clip->Events.size(); e++)
		if (std::fabs(clip->Events[e].Time - time) <= kAnimKeyEpsilon)
		{ AnimKeyRef r; r.kind = ANIM_KEY_EVENT; r.channel = (int)e; r.time = clip->Events[e].Time; out.push_back(r); }
}

void DrawDiamond(ImDrawList* dl, const ImVec2& c, float r, ImU32 fill)
{
	dl->AddQuadFilled(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y), fill);
	dl->AddQuad(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y),
		IM_COL32(20, 22, 28, 255));
}

// The clipboard is shared by every animation window, so keys copied in one
// clip (or one file, or a 2D character) paste into another.
AnimKeyClipboard& KeyClipboard()
{
	static AnimKeyClipboard cb;
	return cb;
}

// ---- curve view -------------------------------------------------------------
//
// The selected bone's channel as six curves against time: position x/y/z and
// rotation as Euler x/y/z in degrees. Sampled with p3d::Ease - the function the
// runtime sampler uses - so the shape drawn is the motion played.
//
// Rotation is stored as quaternions; Euler angles are only how it is SHOWN and
// dragged. Consecutive samples are unwrapped so a turn through 180 degrees
// reads as one smooth line rather than a jump to -180.

namespace {

const ImU32 kCurveColors[6] = {
	IM_COL32(240, 100, 100, 255), IM_COL32(110, 220, 110, 255), IM_COL32(110, 150, 250, 255),
	IM_COL32(250, 160, 140, 255), IM_COL32(170, 240, 150, 255), IM_COL32(150, 190, 255, 255),
};
const char* kCurveNames[6] = { "pos x", "pos y", "pos z", "rot x", "rot y", "rot z" };

Vec3 QuatToEulerDeg(const Quaternion& q)
{
	Matrix m = q.ConvertToMatrix();
	const Vec3 e = m.GetEulerFromRotationMatrix();
	return Vec3((f32)RADTODEG(e.x), (f32)RADTODEG(e.y), (f32)RADTODEG(e.z));
}

Quaternion EulerDegToQuat(const Vec3& e)
{
	Quaternion q;
	q.SetRotationFromEuler(Vec3((f32)DEGTORAD(e.x), (f32)DEGTORAD(e.y), (f32)DEGTORAD(e.z)));
	return q;
}

float Unwrap(float v, float closeTo)
{
	while (v - closeTo > 180.f) v -= 360.f;
	while (v - closeTo < -180.f) v += 360.f;
	return v;
}

// One component (0..5) of a channel at `t`.
template <typename K, typename Get>
float SampleComponent(const std::vector<K>& keys, float t, Get get)
{
	if (keys.empty()) return 0.f;
	size_t i = 0;
	while (i + 1 < keys.size() && keys[i + 1].Time <= t) i++;
	if (i + 1 >= keys.size() || t <= keys[i].Time) return get(keys[i], keys[i], 0.f);
	const float span = keys[i + 1].Time - keys[i].Time;
	const float u = span > 0.f ? (t - keys[i].Time) / span : 0.f;
	return get(keys[i], keys[i + 1], p3d::Ease(u, keys[i].Mode, keys[i].OutTangent, keys[i + 1].InTangent));
}

float SampleCurve(const Channel& ch, int c, float t)
{
	if (c < 3)
		return SampleComponent(ch.positions, t, [c](const PositionData& a, const PositionData& b, float u) {
			const Vec3 v = a.Pos.Lerp(b.Pos, u);
			return c == 0 ? v.x : c == 1 ? v.y : v.z;
		});
	return SampleComponent(ch.rotations, t, [c](const RotationData& a, const RotationData& b, float u) {
		const Vec3 e = QuatToEulerDeg(a.Rot.Slerp(b.Rot, u));
		return c == 3 ? e.x : c == 4 ? e.y : e.z;
	});
}

struct CurveDrag {
	bool active = false;
	int channel = -1;
	int component = -1;
	float keyTime = 0.f;
	float grabValue = 0.f;
	float grabY = 0.f;
};

} // namespace

void DrawCurves(AnimationEditorDocument& doc)
{
	Animation* clip = doc.ActiveClip();
	if (!clip) return;
	const int ch = doc.selectedBoneName.empty() ? -1 : doc.FindChannel(doc.activeClip, doc.selectedBoneName);

	for (int c = 0; c < 6; c++)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kCurveColors[c]));
		ImGui::Checkbox(kCurveNames[c], &doc.curveShown[c]);
		ImGui::PopStyleColor();
		ImGui::SameLine();
	}
	const bool fit = ImGui::Button("Fit");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.f);
	ImGui::DragFloat("Zoom##t", &doc.pixelsPerSecond, 4.f, 20.f, 2000.f, "%.0f px/s");
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Drag a key up/down to change its value, Shift+drag to retime it.\n"
			"Wheel zooms the value axis, middle-drag pans it. Right-click a key for interpolation.");

	if (ch < 0)
	{
		ImGui::TextDisabled(doc.selectedBoneName.empty()
			? "Select a bone to see its curves." : "This bone has no keys in this clip yet.");
		return;
	}
	Channel& channel = clip->Channels[ch];

	ImGui::BeginChild("##curves", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const float height = std::max(80.f, ImGui::GetContentRegionAvail().y - 4.f);
	const float width = std::max(ImGui::GetContentRegionAvail().x, 40.f + clip->Duration * doc.pixelsPerSecond + 40.f);
	ImGui::InvisibleButton("##curvecanvas", ImVec2(width, height));
	const bool hovered = ImGui::IsItemHovered();
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const float left = origin.x + 40.f;
	const float timeToX = doc.pixelsPerSecond;

	// Value axis: fitted on request, the first time, and when the bone changes.
	static std::map<uint32_t, std::string> fittedFor;
	const bool newBone = fittedFor[doc.id] != doc.selectedBoneName;
	fittedFor[doc.id] = doc.selectedBoneName;
	if (fit || newBone || doc.curveZoomY <= 0.f)
	{
		float lo = 1e30f, hi = -1e30f;
		const int samples = std::max(2, (int)(clip->Duration * 60.f));
		for (int c = 0; c < 6; c++)
		{
			if (!doc.curveShown[c]) continue;
			float prev = 0.f;
			for (int i = 0; i <= samples; i++)
			{
				float v = SampleCurve(channel, c, clip->Duration * i / samples);
				if (c >= 3 && i > 0) v = Unwrap(v, prev);
				prev = v;
				lo = std::min(lo, v); hi = std::max(hi, v);
			}
		}
		if (lo > hi) { lo = -1.f; hi = 1.f; }
		if (hi - lo < 1e-3f) { lo -= 1.f; hi += 1.f; }
		doc.curveCenterY = 0.5f * (lo + hi);
		doc.curveZoomY = (height - 24.f) / ((hi - lo) * 1.1f);
	}
	const auto valueToY = [&](float v) { return origin.y + height * 0.5f - (v - doc.curveCenterY) * doc.curveZoomY; };
	const auto yToValue = [&](float y) { return doc.curveCenterY - (y - origin.y - height * 0.5f) / doc.curveZoomY; };

	dl->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), IM_COL32(32, 34, 40, 255));
	// Horizontal grid with value labels.
	{
		const float span = height / doc.curveZoomY;
		float step = std::pow(10.f, std::floor(std::log10(std::max(span / 4.f, 1e-6f))));
		if (span / step > 8.f) step *= 2.f;
		if (span / step > 8.f) step *= 2.5f;
		const float lo = yToValue(origin.y + height), hi = yToValue(origin.y);
		for (float v = std::ceil(lo / step) * step; v <= hi; v += step)
		{
			const float y = valueToY(v);
			dl->AddLine(ImVec2(left, y), ImVec2(origin.x + width, y),
				std::fabs(v) < step * 0.01f ? IM_COL32(110, 112, 125, 255) : IM_COL32(52, 55, 64, 255));
			char lbl[24];
			std::snprintf(lbl, sizeof(lbl), "%g", std::fabs(v) < step * 0.01f ? 0.f : v);
			dl->AddText(ImVec2(origin.x + 2.f, y - 7.f), IM_COL32(150, 154, 168, 255), lbl);
		}
		for (float t = 0.f; t <= clip->Duration + 1e-4f; t += RulerStep(timeToX))
			dl->AddLine(ImVec2(left + t * timeToX, origin.y), ImVec2(left + t * timeToX, origin.y + height), IM_COL32(48, 50, 58, 255));
	}

	// Curves.
	const int samples = std::max(8, (int)(clip->Duration * timeToX / 3.f));
	for (int c = 0; c < 6; c++)
	{
		if (!doc.curveShown[c]) continue;
		if (c < 3 ? channel.positions.empty() : channel.rotations.empty()) continue;
		float prev = 0.f;
		ImVec2 last;
		for (int i = 0; i <= samples; i++)
		{
			const float t = clip->Duration * i / samples;
			float v = SampleCurve(channel, c, t);
			if (c >= 3 && i > 0) v = Unwrap(v, prev);
			prev = v;
			const ImVec2 p(left + t * timeToX, valueToY(v));
			if (i > 0) dl->AddLine(last, p, kCurveColors[c], 1.5f);
			last = p;
		}
	}

	// Keys: one point per component per key, placed on its (unwrapped) curve.
	struct CurveKey { ImVec2 pos; int component; float time; float value; };
	std::vector<CurveKey> keys;
	for (int c = 0; c < 6; c++)
	{
		if (!doc.curveShown[c]) continue;
		std::vector<float> times;
		doc.CollectComponentTimes(doc.activeClip, ch, c < 3 ? ANIM_KEY_POSITION : ANIM_KEY_ROTATION, times);
		for (size_t i = 0; i < times.size(); i++)
		{
			float v = SampleCurve(channel, c, times[i]);
			if (c >= 3)
			{
				// Same branch as the drawn line at this time.
				float prev = SampleCurve(channel, c, 0.f);
				const int steps = std::max(1, (int)(times[i] * 60.f));
				for (int s = 1; s <= steps; s++) prev = Unwrap(SampleCurve(channel, c, times[i] * s / steps), prev);
				v = Unwrap(v, prev);
			}
			CurveKey k; k.pos = ImVec2(left + times[i] * timeToX, valueToY(v)); k.component = c; k.time = times[i]; k.value = v;
			keys.push_back(k);
		}
	}
	int hot = -1;
	if (hovered)
	{
		float best = 1e9f;
		for (size_t i = 0; i < keys.size(); i++)
		{
			const float d = std::fabs(mouse.x - keys[i].pos.x) + std::fabs(mouse.y - keys[i].pos.y);
			if (d < 10.f && d < best) { best = d; hot = (int)i; }
		}
	}
	static std::map<uint32_t, CurveDrag> drags;
	CurveDrag& drag = drags[doc.id];
	for (size_t i = 0; i < keys.size(); i++)
	{
		const bool active = drag.active && drag.component == keys[i].component && std::fabs(drag.keyTime - keys[i].time) <= kAnimKeyEpsilon;
		const float r = ((int)i == hot || active) ? 5.f : 3.5f;
		dl->AddCircleFilled(keys[i].pos, r, active ? IM_COL32(255, 200, 60, 255) : kCurveColors[keys[i].component]);
		dl->AddCircle(keys[i].pos, r, IM_COL32(15, 15, 20, 255));
	}

	// Playhead.
	{
		const float x = left + doc.playhead * timeToX;
		dl->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + height), IM_COL32(255, 90, 90, 255), 1.5f);
	}

	// ---- interaction ----
	if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (hot >= 0)
		{
			drag.active = true;
			drag.channel = ch;
			drag.component = keys[hot].component;
			drag.keyTime = keys[hot].time;
			drag.grabValue = keys[hot].value;
			drag.grabY = mouse.y;
			doc.BeginInteractiveEdit();
		}
		else SetPlayhead(doc, doc.SnapTime((mouse.x - left) / timeToX));
	}
	if (drag.active)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && drag.channel == ch)
		{
			const int c = drag.component;
			if (ImGui::GetIO().KeyShift)
			{
				// Retime this component's key.
				const float to = doc.SnapTime((mouse.x - left) / timeToX);
				AnimKeyRef r; r.kind = c < 3 ? ANIM_KEY_POSITION : ANIM_KEY_ROTATION; r.channel = ch; r.time = drag.keyTime;
				bool ok = false;
				const AnimKeyRef moved = doc.MoveKeyRef(doc.activeClip, r, to, ok);
				if (ok) { drag.keyTime = moved.time; doc.dirty = true; doc.clipsRevision++; }
			}
			else
			{
				const float v = drag.grabValue + (drag.grabY - mouse.y) / doc.curveZoomY;
				if (c < 3)
				{
					for (size_t i = 0; i < channel.positions.size(); i++)
						if (std::fabs(channel.positions[i].Time - drag.keyTime) <= kAnimKeyEpsilon)
						{
							Vec3& p = channel.positions[i].Pos;
							(c == 0 ? p.x : c == 1 ? p.y : p.z) = v;
						}
				}
				else
				{
					for (size_t i = 0; i < channel.rotations.size(); i++)
						if (std::fabs(channel.rotations[i].Time - drag.keyTime) <= kAnimKeyEpsilon)
						{
							Vec3 e = QuatToEulerDeg(channel.rotations[i].Rot);
							(c == 3 ? e.x : c == 4 ? e.y : e.z) = v;
							channel.rotations[i].Rot = EulerDegToQuat(e);
						}
				}
				doc.dirty = true;
				doc.clipsRevision++;
			}
		}
		else
		{
			drag.active = false;
			doc.EndInteractiveEdit("Edit curve");
		}
	}
	if (hovered)
	{
		const float wheel = ImGui::GetIO().MouseWheel;
		if (wheel != 0.f)
		{
			// Zoom about the value under the cursor.
			const float under = yToValue(mouse.y);
			doc.curveZoomY *= std::pow(1.15f, wheel);
			doc.curveZoomY = std::max(1e-4f, std::min(doc.curveZoomY, 1e5f));
			doc.curveCenterY = under + (mouse.y - origin.y - height * 0.5f) / doc.curveZoomY;
		}
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
		{
			doc.curveCenterY += ImGui::GetIO().MouseDelta.y / doc.curveZoomY;
		}
	}
	if (hovered && hot >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		doc.selectedKeys.clear();
		AnimKeyRef r; r.kind = keys[hot].component < 3 ? ANIM_KEY_POSITION : ANIM_KEY_ROTATION; r.channel = ch; r.time = keys[hot].time;
		doc.selectedKeys.insert(r);
		ImGui::OpenPopup("##curvekey");
	}
	if (ImGui::BeginPopup("##curvekey"))
	{
		if (!doc.selectedKeys.empty())
		{
			const AnimKeyRef r = *doc.selectedKeys.begin();
			int mode = p3d::INTERP_LINEAR; float inTan = 1.f, outTan = 1.f;
			GetRefInterpolation(doc, r, mode, inTan, outTan);
			for (unsigned m = 0; m < p3d::kInterpolationModeCount; m++)
				if (ImGui::MenuItem(p3d::InterpolationModeName((uchar)m), NULL, mode == (int)m))
					doc.PushSnapshotEdit(std::string("Set interpolation: ") + p3d::InterpolationModeName((uchar)m),
						[&]() { SetRefInterpolation(doc, r, (int)m, inTan, outTan); });
			ImGui::Separator();
			if (ImGui::MenuItem("Delete key")) DeleteSelectedKeys(doc);
		}
		ImGui::EndPopup();
	}
	if (hovered && hot >= 0 && !drag.active)
		ImGui::SetTooltip("%s @ %s = %.3f%s", kCurveNames[keys[hot].component], FormatTime(keys[hot].time).c_str(),
			keys[hot].value, keys[hot].component >= 3 ? " deg" : "");

	ImGui::EndChild();
}

void DrawTimeline(AnimationEditorDocument& doc)
{
	const Animation* clip = doc.ActiveClip();
	if (!clip)
	{
		ImGui::TextDisabled("No clip. Use '+ Clip' to start one.");
		return;
	}

	// Dope sheet or curves: the same keys, by time or by value.
	if (ImGui::RadioButton("Keys", !doc.curveView)) doc.curveView = false;
	ImGui::SameLine();
	if (ImGui::RadioButton("Curves", doc.curveView)) doc.curveView = true;
	ImGui::SameLine();
	if (doc.curveView)
	{
		DrawCurves(doc);
		return;
	}

	std::vector<TrackRow> boneRows;
	BuildTracks(doc, boneRows);

	std::vector<SheetRow> rows;
	{
		SheetRow r; r.type = SheetRow::SUMMARY; r.label = "Summary"; rows.push_back(r);
		SheetRow e; e.type = SheetRow::EVENTS; e.label = "Events"; rows.push_back(e);
	}
	for (size_t i = 0; i < boneRows.size(); i++)
	{
		SheetRow r;
		r.type = SheetRow::BONE;
		r.label = boneRows[i].boneName;
		r.channel = boneRows[i].channel;
		r.boneId = boneRows[i].boneId;
		r.depth = boneRows[i].depth;
		rows.push_back(r);
		if (r.channel >= 0 && doc.splitBones.count(r.label))
			for (int k = ANIM_KEY_POSITION; k <= ANIM_KEY_SCALE; k++)
			{
				SheetRow c = r;
				c.type = SheetRow::COMPONENT;
				c.kind = k;
				c.label = ComponentLabel(k);
				c.depth = r.depth + 2;
				rows.push_back(c);
			}
	}
	for (size_t t = 0; t < clip->Tracks.size(); t++)
	{
		SheetRow r;
		r.type = SheetRow::TRACK;
		r.track = (int)t;
		r.label = clip->Tracks[t].Target + " . " + clip->Tracks[t].Property;
		rows.push_back(r);
	}

	ImGui::Checkbox("Show all bones", &doc.showAllBones);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Also list bones this clip doesn't animate yet, so they can be keyed.");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.f);
	ImGui::DragFloat("Zoom", &doc.pixelsPerSecond, 4.f, 20.f, 2000.f, "%.0f px/s");
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Drag empty space to box-select, Shift adds. Ctrl+C / Ctrl+V copy keys to the playhead.\n"
			"Click a bone's arrow to split it into position / rotation / scale rows.\n"
			"Double-click the Events row to add an event; right-click any key for options.");

	ImGui::BeginChild("##dope", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
	const bool sheetFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
		|| ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const float contentWidth = kTrackLabelWidth + clip->Duration * doc.pixelsPerSecond + 40.f;
	const float contentHeight = kRulerHeight + rows.size() * kRowHeight + 8.f;

	ImGui::InvisibleButton("##dopecanvas", ImVec2(contentWidth, contentHeight));
	const bool canvasHovered = ImGui::IsItemHovered();
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const float localX = mouse.x - origin.x;
	const float localY = mouse.y - origin.y;

	const float trackX = origin.x + kTrackLabelWidth;
	const float timeToX = doc.pixelsPerSecond;
	const auto xToTime = [&](float x) { return (x - trackX) / timeToX; };

	// ---- ruler ---------------------------------------------------------
	dl->AddRectFilled(ImVec2(origin.x, origin.y),
		ImVec2(origin.x + contentWidth, origin.y + kRulerHeight), IM_COL32(38, 40, 48, 255));
	{
		const float step = RulerStep(timeToX);
		const int count = (int)std::ceil(clip->Duration / step + 1e-4f);
		for (int i = 0; i <= count; i++)
		{
			const float s = i * step;
			if (s > clip->Duration + 1e-4f) break;
			const float x = trackX + s * timeToX;
			dl->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + kRulerHeight), IM_COL32(120, 124, 140, 255));
			char lbl[24];
			if (step < 1.f) std::snprintf(lbl, sizeof(lbl), "%.2fs", s);
			else std::snprintf(lbl, sizeof(lbl), "%gs", s);
			dl->AddText(ImVec2(x + 3.f, origin.y + 3.f), IM_COL32(180, 184, 200, 255), lbl);
		}
		if (doc.snapFps > 0.f && timeToX / doc.snapFps >= 6.f)
		{
			const int frames = (int)std::ceil(clip->Duration * doc.snapFps);
			for (int f = 0; f <= frames; f++)
			{
				const float x = trackX + (f / doc.snapFps) * timeToX;
				dl->AddLine(ImVec2(x, origin.y + kRulerHeight - 5.f), ImVec2(x, origin.y + kRulerHeight),
					IM_COL32(90, 94, 108, 255));
			}
		}
	}

	// ---- rows ------------------------------------------------------------
	std::vector<DrawnKey> drawn;
	int hoveredRow = -1;
	float rowY = origin.y + kRulerHeight;
	int splitToggle = -1;     // bone row whose arrow was clicked
	for (size_t r = 0; r < rows.size(); r++, rowY += kRowHeight)
	{
		const SheetRow& row = rows[r];
		const bool isSelectedBone = (row.type == SheetRow::BONE && row.boneId >= 0 && row.boneId == doc.selectedBone);
		ImU32 bg = (r % 2 == 0) ? IM_COL32(40, 42, 50, 255) : IM_COL32(44, 46, 55, 255);
		if (row.type == SheetRow::SUMMARY) bg = IM_COL32(50, 52, 62, 255);
		else if (row.type == SheetRow::EVENTS) bg = IM_COL32(46, 50, 44, 255);
		else if (row.type == SheetRow::TRACK) bg = (r % 2 == 0) ? IM_COL32(42, 40, 52, 255) : IM_COL32(46, 44, 57, 255);
		else if (row.type == SheetRow::COMPONENT) bg = IM_COL32(36, 38, 45, 255);
		if (isSelectedBone) bg = IM_COL32(64, 58, 34, 255);
		dl->AddRectFilled(ImVec2(origin.x, rowY), ImVec2(origin.x + contentWidth, rowY + kRowHeight), bg);

		const ImU32 labelColor = (row.type == SheetRow::BONE && row.channel < 0)
			? IM_COL32(130, 134, 148, 255) : IM_COL32(220, 224, 235, 255);
		float labelX = origin.x + 4.f + row.depth * 8.f;
		if (row.type == SheetRow::BONE && row.channel >= 0)
		{
			// The split arrow.
			const bool open = doc.splitBones.count(row.label) > 0;
			const ImVec2 a(labelX + 1.f, rowY + 5.f);
			if (open) dl->AddTriangleFilled(a, ImVec2(a.x + 8.f, a.y), ImVec2(a.x + 4.f, a.y + 7.f), IM_COL32(170, 175, 190, 255));
			else dl->AddTriangleFilled(a, ImVec2(a.x + 6.f, a.y + 4.f), ImVec2(a.x, a.y + 8.f), IM_COL32(170, 175, 190, 255));
			if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
				&& mouse.x >= labelX - 2.f && mouse.x <= labelX + 11.f && mouse.y >= rowY && mouse.y < rowY + kRowHeight)
				splitToggle = (int)r;
			labelX += 12.f;
		}
		dl->PushClipRect(ImVec2(origin.x, rowY), ImVec2(trackX - 2.f, rowY + kRowHeight), true);
		dl->AddText(ImVec2(labelX, rowY + 2.f),
			row.type == SheetRow::COMPONENT ? IM_COL32(160, 166, 180, 255) : labelColor, row.label.c_str());
		dl->PopClipRect();

		const float cy = rowY + kRowHeight * 0.5f;
		std::vector<float> times;
		switch (row.type)
		{
		case SheetRow::SUMMARY:
		{
			doc.CollectAllKeyTimes(doc.activeClip, times);
			for (size_t t = 0; t < clip->Tracks.size(); t++)
				for (size_t k = 0; k < clip->Tracks[t].Keys.size(); k++) times.push_back(clip->Tracks[t].Keys[k].Time);
			std::sort(times.begin(), times.end());
			times.erase(std::unique(times.begin(), times.end(),
				[](float a, float b) { return std::fabs(a - b) <= kAnimKeyEpsilon; }), times.end());
			for (size_t i = 0; i < times.size(); i++)
			{
				DrawnKey k; k.pos = ImVec2(trackX + times[i] * timeToX, cy); k.ref.time = times[i]; k.summary = true;
				// A summary diamond reads as selected when everything at its time is.
				std::vector<AnimKeyRef> at; RefsAtTime(doc, times[i], at);
				bool all = !at.empty();
				for (size_t a = 0; a < at.size() && all; a++) if (!doc.selectedKeys.count(at[a])) all = false;
				DrawDiamond(dl, k.pos, 5.f, all ? IM_COL32(255, 200, 60, 255) : IM_COL32(220, 220, 235, 255));
				drawn.push_back(k);
			}
			break;
		}
		case SheetRow::EVENTS:
			for (size_t e = 0; e < clip->Events.size(); e++)
			{
				DrawnKey k;
				k.pos = ImVec2(trackX + clip->Events[e].Time * timeToX, cy);
				k.ref.kind = ANIM_KEY_EVENT; k.ref.channel = (int)e; k.ref.time = clip->Events[e].Time;
				const bool sel = doc.selectedKeys.count(k.ref) > 0;
				const ImU32 col = sel ? IM_COL32(255, 200, 60, 255) : IM_COL32(140, 220, 120, 255);
				dl->AddLine(ImVec2(k.pos.x, rowY + 2.f), ImVec2(k.pos.x, rowY + kRowHeight - 2.f), col, 1.5f);
				dl->AddTriangleFilled(ImVec2(k.pos.x, rowY + 2.f), ImVec2(k.pos.x + 7.f, rowY + 5.5f), ImVec2(k.pos.x, rowY + 9.f), col);
				dl->AddText(ImVec2(k.pos.x + 9.f, rowY + 2.f), IM_COL32(190, 225, 180, 255), clip->Events[e].Name.c_str());
				drawn.push_back(k);
			}
			break;
		case SheetRow::BONE:
			if (row.channel >= 0) doc.CollectKeyTimes(doc.activeClip, row.channel, times);
			for (size_t i = 0; i < times.size(); i++)
			{
				DrawnKey k; k.pos = ImVec2(trackX + times[i] * timeToX, cy);
				k.ref.channel = row.channel; k.ref.time = times[i];
				DrawDiamond(dl, k.pos, 5.f, doc.selectedKeys.count(k.ref) ? IM_COL32(255, 200, 60, 255) : IM_COL32(150, 200, 255, 255));
				drawn.push_back(k);
			}
			break;
		case SheetRow::COMPONENT:
		{
			doc.CollectComponentTimes(doc.activeClip, row.channel, row.kind, times);
			const ImU32 base = row.kind == ANIM_KEY_POSITION ? IM_COL32(240, 120, 110, 255)
				: row.kind == ANIM_KEY_ROTATION ? IM_COL32(120, 220, 130, 255) : IM_COL32(120, 160, 245, 255);
			for (size_t i = 0; i < times.size(); i++)
			{
				DrawnKey k; k.pos = ImVec2(trackX + times[i] * timeToX, cy);
				k.ref.kind = row.kind; k.ref.channel = row.channel; k.ref.time = times[i];
				DrawDiamond(dl, k.pos, 4.f, doc.selectedKeys.count(k.ref) ? IM_COL32(255, 200, 60, 255) : base);
				drawn.push_back(k);
			}
			break;
		}
		case SheetRow::TRACK:
		{
			const std::vector<PropertyKey>& keys = clip->Tracks[row.track].Keys;
			for (size_t i = 0; i < keys.size(); i++)
			{
				DrawnKey k; k.pos = ImVec2(trackX + keys[i].Time * timeToX, cy);
				k.ref.kind = ANIM_KEY_TRACK; k.ref.channel = row.track; k.ref.time = keys[i].Time;
				const bool sel = doc.selectedKeys.count(k.ref) > 0;
				dl->AddRectFilled(ImVec2(k.pos.x - 4.f, cy - 4.f), ImVec2(k.pos.x + 4.f, cy + 4.f),
					sel ? IM_COL32(255, 200, 60, 255) : IM_COL32(215, 160, 245, 255));
				dl->AddRect(ImVec2(k.pos.x - 4.f, cy - 4.f), ImVec2(k.pos.x + 4.f, cy + 4.f), IM_COL32(20, 22, 28, 255));
				if (!keys[i].Text.empty())
				{
					// A picture key shows which picture.
					std::string t = keys[i].Text;
					const size_t slash = t.find_last_of("/\\");
					if (slash != std::string::npos) t = t.substr(slash + 1);
					dl->AddText(ImVec2(k.pos.x + 6.f, rowY + 2.f), IM_COL32(200, 185, 220, 255), t.c_str());
				}
				drawn.push_back(k);
			}
			break;
		}
		}

		if (canvasHovered && mouse.y >= rowY && mouse.y < rowY + kRowHeight) hoveredRow = (int)r;
	}

	if (splitToggle >= 0)
	{
		const std::string& name = rows[splitToggle].label;
		if (doc.splitBones.count(name)) doc.splitBones.erase(name);
		else doc.splitBones.insert(name);
	}

	// Nearest drawn key under the cursor.
	int hovered = -1;
	if (canvasHovered && localX > kTrackLabelWidth)
	{
		float best = 1e9f;
		for (size_t i = 0; i < drawn.size(); i++)
		{
			const float dx = std::fabs(mouse.x - drawn[i].pos.x), dy = std::fabs(mouse.y - drawn[i].pos.y);
			if (dx <= 6.f && dy <= 7.f && dx + dy < best) { best = dx + dy; hovered = (int)i; }
		}
	}

	// ---- playhead --------------------------------------------------------
	{
		const float x = trackX + doc.playhead * timeToX;
		dl->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + contentHeight), IM_COL32(255, 90, 90, 255), 1.5f);
		dl->AddTriangleFilled(ImVec2(x - 5.f, origin.y), ImVec2(x + 5.f, origin.y), ImVec2(x, origin.y + 8.f),
			IM_COL32(255, 90, 90, 255));
	}

	// ---- interaction -------------------------------------------------------
	// Per-document, so two animation windows never share a drag in flight.
	struct DragState {
		bool playhead = false, keys = false, pending = false, marquee = false;
		float keyOrigin = 0.f;
		ImVec2 start;
	};
	static std::map<uint32_t, DragState> dragStates;
	DragState& ds = dragStates[doc.id];

	const bool shift = ImGui::GetIO().KeyShift;
	if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && splitToggle < 0)
	{
		if (localY < kRulerHeight)
			ds.playhead = true;
		else if (hovered >= 0)
		{
			const DrawnKey& k = drawn[hovered];
			std::vector<AnimKeyRef> refs;
			if (k.summary) RefsAtTime(doc, k.ref.time, refs);
			else refs.push_back(k.ref);
			bool alreadySelected = !refs.empty();
			for (size_t i = 0; i < refs.size() && alreadySelected; i++) if (!doc.selectedKeys.count(refs[i])) alreadySelected = false;
			// Click on a selected key keeps the selection, so a group can be
			// dragged by any member of it.
			if (!shift && !alreadySelected) doc.selectedKeys.clear();
			for (size_t i = 0; i < refs.size(); i++) doc.selectedKeys.insert(refs[i]);
			ds.keys = true;
			ds.keyOrigin = k.ref.time;
			doc.BeginInteractiveEdit();
		}
		else if (localX > kTrackLabelWidth)
		{
			ds.pending = true;
			ds.start = mouse;
		}
	}

	if (ds.playhead)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) SetPlayhead(doc, doc.SnapTime(xToTime(mouse.x)));
		else ds.playhead = false;
	}

	if (ds.pending)
	{
		const float dx = mouse.x - ds.start.x, dy = mouse.y - ds.start.y;
		if (!ds.marquee && dx * dx + dy * dy > 16.f) ds.marquee = true;
		if (ds.marquee)
		{
			const ImVec2 a(std::min(ds.start.x, mouse.x), std::min(ds.start.y, mouse.y));
			const ImVec2 b(std::max(ds.start.x, mouse.x), std::max(ds.start.y, mouse.y));
			dl->AddRectFilled(a, b, IM_COL32(120, 160, 255, 40));
			dl->AddRect(a, b, IM_COL32(140, 180, 255, 200));
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			if (ds.marquee)
			{
				const ImVec2 a(std::min(ds.start.x, mouse.x), std::min(ds.start.y, mouse.y));
				const ImVec2 b(std::max(ds.start.x, mouse.x), std::max(ds.start.y, mouse.y));
				if (!shift) doc.selectedKeys.clear();
				for (size_t i = 0; i < drawn.size(); i++)
				{
					const ImVec2& p = drawn[i].pos;
					if (p.x < a.x || p.x > b.x || p.y < a.y || p.y > b.y) continue;
					if (drawn[i].summary)
					{
						std::vector<AnimKeyRef> at; RefsAtTime(doc, drawn[i].ref.time, at);
						doc.selectedKeys.insert(at.begin(), at.end());
					}
					else doc.selectedKeys.insert(drawn[i].ref);
				}
			}
			else
			{
				// A plain click on empty space moves the playhead there - the
				// same gesture as clicking the ruler, which is what people try first.
				if (!shift) doc.selectedKeys.clear();
				SetPlayhead(doc, doc.SnapTime(xToTime(ds.start.x)));
			}
			ds.pending = ds.marquee = false;
		}
	}

	if (ds.keys)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			const float target = doc.SnapTime(xToTime(mouse.x));
			if (std::fabs(target - ds.keyOrigin) > kAnimKeyEpsilon)
			{
				const float delta = target - ds.keyOrigin;
				std::vector<AnimKeyRef> targets(doc.selectedKeys.begin(), doc.selectedKeys.end());
				std::set<AnimKeyRef> moved;
				for (size_t i = 0; i < targets.size(); i++)
				{
					bool ok = false;
					const AnimKeyRef to = doc.MoveKeyRef(doc.activeClip, targets[i], std::max(0.f, targets[i].time + delta), ok);
					moved.insert(ok ? to : targets[i]);
				}
				doc.selectedKeys = moved;
				ds.keyOrigin = target;
				doc.dirty = true;
				doc.clipsRevision++;
			}
		}
		else
		{
			ds.keys = false;
			doc.EndInteractiveEdit("Move keys");
		}
	}

	// Double-click an empty cell: key that row right there.
	if (canvasHovered && hovered < 0 && hoveredRow >= 0 && localX > kTrackLabelWidth
		&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		const SheetRow& row = rows[hoveredRow];
		const float t = doc.SnapTime(xToTime(mouse.x));
		if (row.type == SheetRow::BONE && row.boneId >= 0)
			doc.PushSnapshotEdit("Key bone '" + row.label + "'", [&]() { KeyBoneAtTime(doc, row.boneId, t); });
		else if (row.type == SheetRow::EVENTS)
		{
			doc.PushSnapshotEdit("Add event", [&]() { doc.AddEvent(doc.activeClip, t, "event"); });
			AnimKeyRef r; r.kind = ANIM_KEY_EVENT; r.channel = (int)doc.clips[doc.activeClip].Events.size() - 1; r.time = t;
			doc.selectedKeys.clear();
			doc.selectedKeys.insert(r);
			ImGui::OpenPopup("##keymenu");
		}
		else if (row.type == SheetRow::TRACK)
		{
			// Key the value the track has at that instant, so it can then be changed.
			const PropertyTrack& tr = clip->Tracks[row.track];
			PropertyKey v = tr.Keys.empty() ? PropertyKey() : tr.Keys[0];
			for (size_t i = 0; i < tr.Keys.size(); i++) if (tr.Keys[i].Time <= t) v = tr.Keys[i];
			const int track = row.track;
			doc.PushSnapshotEdit("Key " + row.label, [&]() {
				doc.SetTrackKey(doc.activeClip, track, t, v.Value, v.Text, v.Mode);
			});
		}
	}

	// Right-click a key: its menu (on it and any other selected ones).
	if (canvasHovered && hovered >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		std::vector<AnimKeyRef> refs;
		if (drawn[hovered].summary) RefsAtTime(doc, drawn[hovered].ref.time, refs);
		else refs.push_back(drawn[hovered].ref);
		bool anySelected = false;
		for (size_t i = 0; i < refs.size(); i++) if (doc.selectedKeys.count(refs[i])) anySelected = true;
		if (!anySelected) { doc.selectedKeys.clear(); doc.selectedKeys.insert(refs.begin(), refs.end()); }
		ImGui::OpenPopup("##keymenu");
	}
	// Right-click a bone label: split / merge.
	if (canvasHovered && hoveredRow >= 0 && localX <= kTrackLabelWidth && ImGui::IsMouseClicked(ImGuiMouseButton_Right)
		&& rows[hoveredRow].type == SheetRow::BONE && rows[hoveredRow].channel >= 0)
	{
		ImGui::OpenPopup("##rowmenu");
		ImGui::GetStateStorage()->SetInt(ImGui::GetID("##rowmenu_row"), hoveredRow);
	}
	if (ImGui::BeginPopup("##rowmenu"))
	{
		const int r = ImGui::GetStateStorage()->GetInt(ImGui::GetID("##rowmenu_row"), -1);
		if (r >= 0 && r < (int)rows.size())
		{
			const std::string name = rows[r].label;
			const bool open = doc.splitBones.count(name) > 0;
			if (ImGui::MenuItem(open ? "Merge position / rotation / scale" : "Split into position / rotation / scale"))
			{
				if (open) doc.splitBones.erase(name); else doc.splitBones.insert(name);
			}
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("##keymenu"))
	{
		const int keyCount = (int)doc.selectedKeys.size();
		ImGui::TextDisabled(keyCount == 1 ? "1 key" : "%d keys", keyCount);
		ImGui::Separator();

		// A lone event: its name, edited in place.
		if (keyCount == 1 && doc.selectedKeys.begin()->kind == ANIM_KEY_EVENT)
		{
			const int e = doc.selectedKeys.begin()->channel;
			Animation& c = doc.clips[doc.activeClip];
			if (e >= 0 && e < (int)c.Events.size())
			{
				std::string name = c.Events[e].Name;
				ImGui::SetNextItemWidth(180.f);
				if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
				if (ImGui::InputText("Event name", &name, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll) && !name.empty())
				{
					doc.PushSnapshotEdit("Rename event", [&]() { doc.RenameEvent(doc.activeClip, e, name); });
					ImGui::CloseCurrentPopup();
				}
				ImGui::TextDisabled("A script hears it as onEvent(name, clip).");
			}
		}

		// Interpolation, for every selected key that has one.
		AnimKeyRef first;
		bool haveInterp = false;
		for (std::set<AnimKeyRef>::const_iterator it = doc.selectedKeys.begin(); it != doc.selectedKeys.end(); ++it)
			if (it->kind != ANIM_KEY_EVENT) { first = *it; haveInterp = true; break; }
		if (haveInterp)
		{
			int mode = p3d::INTERP_LINEAR;
			float inTan = 1.f, outTan = 1.f;
			GetRefInterpolation(doc, first, mode, inTan, outTan);

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
								if (targets[i].kind != ANIM_KEY_EVENT) SetRefInterpolation(doc, targets[i], (int)m, inTan, outTan);
						});
						mode = (int)m;
					}
				}
				ImGui::EndMenu();
			}

			// Drawn from p3d::Ease(), the same function the sampler runs.
			ImGui::TextDisabled("%s", p3d::InterpolationModeName((uchar)mode));
			EasingUI::Curve((uchar)mode, outTan, inTan);

			if (mode == p3d::INTERP_BEZIER && first.kind != ANIM_KEY_TRACK)
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
					for (std::set<AnimKeyRef>::const_iterator it = doc.selectedKeys.begin(); it != doc.selectedKeys.end(); ++it)
						if (it->kind != ANIM_KEY_EVENT && it->kind != ANIM_KEY_TRACK)
							SetRefInterpolation(doc, *it, mode, inTan, outTan);
					doc.clipsRevision++;
					doc.dirty = true;
				}
			}
		}

		ImGui::Separator();
		if (ImGui::MenuItem("Copy", "Ctrl+C")) doc.CopySelectedKeys(KeyClipboard());
		if (ImGui::MenuItem("Paste at playhead", "Ctrl+V", false, !KeyClipboard().Empty()))
		{
			const float t = doc.SnapTime(doc.playhead);
			doc.PushSnapshotEdit("Paste keys", [&]() { doc.PasteKeys(KeyClipboard(), t); });
		}
		if (ImGui::MenuItem(keyCount == 1 ? "Delete key" : "Delete keys", "Del"))
			DeleteSelectedKeys(doc);
		ImGui::EndPopup();
	}

	// Keyboard, while the sheet is under the mouse or focused.
	if (sheetFocused && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) && !ImGui::GetIO().WantTextInput)
	{
		const bool ctrl = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper;
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) doc.CopySelectedKeys(KeyClipboard());
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false) && !KeyClipboard().Empty())
		{
			const float t = doc.SnapTime(doc.playhead);
			doc.PushSnapshotEdit("Paste keys", [&]() { doc.PasteKeys(KeyClipboard(), t); });
		}
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A, false))
		{
			doc.selectedKeys.clear();
			for (size_t i = 0; i < drawn.size(); i++)
			{
				if (drawn[i].summary) continue;
				doc.selectedKeys.insert(drawn[i].ref);
			}
		}
		const float frame = doc.snapFps > 0.f ? 1.f / doc.snapFps : 1.f / 30.f;
		if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) SetPlayhead(doc, doc.playhead - frame);
		if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) SetPlayhead(doc, doc.playhead + frame);
	}

	if (canvasHovered && hovered >= 0 && !ImGui::IsPopupOpen("##keymenu") && !ds.pending)
	{
		const DrawnKey& k = drawn[hovered];
		if (k.summary)
			ImGui::SetTooltip("All keys @ %s\nClick to select, drag to retime", FormatTime(k.ref.time).c_str());
		else if (k.ref.kind == ANIM_KEY_EVENT)
			ImGui::SetTooltip("Event '%s' @ %s\nDrag to retime, right-click to rename",
				clip->Events[k.ref.channel].Name.c_str(), FormatTime(k.ref.time).c_str());
		else
		{
			int mode = p3d::INTERP_LINEAR; float inTan = 1.f, outTan = 1.f;
			GetRefInterpolation(doc, k.ref, mode, inTan, outTan);
			const std::string what = k.ref.kind == ANIM_KEY_TRACK
				? clip->Tracks[k.ref.channel].Target + " . " + clip->Tracks[k.ref.channel].Property
				: clip->Channels[k.ref.channel].NodeName
					+ (k.ref.kind == ANIM_KEY_COLUMN ? std::string() : std::string(" ") + ComponentLabel(k.ref.kind));
			ImGui::SetTooltip("%s @ %s\nInterpolation: %s\nDrag to retime, right-click for options",
				what.c_str(), FormatTime(k.ref.time).c_str(), p3d::InterpolationModeName((uchar)mode));
		}
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
		ImGui::Checkbox("Onion", &doc.onionSkin);
		ImGui::SetItemTooltip("Ghost skeletons before (blue) and after (green) the playhead.\nRight-click for how many and how far apart.");
		if (ImGui::BeginPopupContextItem("##onionopts"))
		{
			ImGui::SetNextItemWidth(100.f);
			ImGui::SliderInt("Before", &doc.onionBefore, 0, 8);
			ImGui::SetNextItemWidth(100.f);
			ImGui::SliderInt("After", &doc.onionAfter, 0, 8);
			ImGui::SetNextItemWidth(100.f);
			ImGui::SliderInt("Every n frames", &doc.onionStep, 1, 15);
			ImGui::EndPopup();
		}
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
