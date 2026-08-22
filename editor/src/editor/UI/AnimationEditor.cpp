//=============================================================================
// Name        : AnimationEditor.cpp
// Description : See AnimationEditor.h.
//=============================================================================

#include "AnimationEditor.h"
#include "../AnimationEditorDocument.h"
#include "../AnimationPreview.h"

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
	AnimationPreview* pv = doc.preview.get();
	if (!pv || !pv->instance) { doc.selectedBone = -1; return; }
	if (!doc.selectedBoneName.empty())
	{
		doc.selectedBone = pv->FindBone(doc.selectedBoneName);
		if (doc.selectedBone >= 0) return;
	}
	if (doc.selectedBone >= (int)pv->instance->GetNumberBones()) doc.selectedBone = -1;
	if (doc.selectedBone >= 0) doc.selectedBoneName = pv->BoneName(doc.selectedBone);
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

	AnimationPreview* pv = doc.preview.get();
	if (doc.showAllBones && pv && pv->instance)
	{
		// Skeleton order, so parents read above their children.
		const std::vector<Bone>& bones = pv->instance->GetSkeletonBones();
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
		row.boneId = (pv ? pv->FindBone(row.boneName) : -1);
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
	}
	else if (doc.meshPath.empty() && !doc.preview->loadedMeshPath.empty())
	{
		doc.preview->ClearMesh();
		doc.selectedBone = -1;
	}
}

bool KeyBoneAtTime(AnimationEditorDocument& doc, int boneId, float time)
{
	AnimationPreview* pv = doc.preview.get();
	if (!pv || !pv->instance) return false;
	if (boneId < 0 || boneId >= (int)pv->instance->GetNumberBones()) return false;
	if (!doc.HasActiveClip()) return false;
	if (!doc.keyPosition && !doc.keyRotation) return false;

	const std::string boneName = pv->BoneName(boneId);
	if (boneName.empty()) return false;

	const int channel = doc.FindOrCreateChannel(doc.activeClip, boneName);
	if (channel < 0) return false;

	// The bone's local transform is what a channel key stores - the runtime
	// composes the parent chain itself (SkeletonAnimationInstance::
	// RefreshSkinning), so keying a model-space matrix here would bake every
	// ancestor's transform into the child and double it at playback.
	const Matrix local = pv->instance->GetBoneLocalTransform(boneId);
	const Vec3 pos = local.GetTranslation();
	const Quaternion rot = local.ConvertToQuaternion();
	const Vec3 scale = local.GetScale();

	doc.SetKey(doc.activeClip, channel, time, pos, rot, scale,
		doc.keyPosition, doc.keyRotation, /*doScale=*/false);
	return true;
}

int KeyPendingPose(AnimationEditorDocument& doc)
{
	AnimationPreview* pv = doc.preview.get();
	if (!pv || pv->poseOverrides.empty() || !doc.HasActiveClip()) return 0;

	std::vector<int> bones;
	for (std::map<int, Matrix>::const_iterator it = pv->poseOverrides.begin(); it != pv->poseOverrides.end(); ++it)
		bones.push_back(it->first);

	const float time = doc.SnapTime(doc.playhead);
	int keyed = 0;
	doc.PushSnapshotEdit("Key pose at " + FormatTime(time), [&]() {
		for (size_t i = 0; i < bones.size(); i++)
			if (KeyBoneAtTime(doc, bones[i], time)) keyed++;
	});
	// The pose is now in the clip; keeping the overrides would pin the rig
	// to it even after scrubbing elsewhere.
	pv->poseOverrides.clear();
	return keyed;
}

int KeyWholeSkeleton(AnimationEditorDocument& doc, float time)
{
	AnimationPreview* pv = doc.preview.get();
	if (!pv || !pv->instance || !doc.HasActiveClip()) return 0;

	const float t = doc.SnapTime(time);
	const uint32 count = pv->instance->GetNumberBones();
	int keyed = 0;
	doc.PushSnapshotEdit("Key whole skeleton at " + FormatTime(t), [&]() {
		for (uint32 i = 0; i < count; i++)
			if (KeyBoneAtTime(doc, (int)i, t)) keyed++;
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
	if (doc.preview && !keepPendingPose) doc.preview->poseOverrides.clear();
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
		// Removing a clip shifts every later clip's id down by one, and
		// those ids are what scenes play by. Saying so beats a silent
		// renumber that breaks a scene the user isn't looking at.
		if (doc.activeClip < (int)doc.clips.size() - 1)
			ImGui::TextColored(ImVec4(1.f, 0.75f, 0.3f, 1.f),
				"Clips after it shift down one id.\nScenes playing those ids will play the wrong clip.");
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
	}

	ImGui::SameLine();
	ImGui::TextUnformatted("|");
	ImGui::SameLine();
	if (ImGui::Button("Save")) requests.save = true;
	ImGui::SameLine();
	if (ImGui::Button("Save As...")) requests.saveAs = true;
}

void DrawBonePanel(AnimationEditorDocument& doc)
{
	AnimationPreview* pv = doc.preview.get();

	ImGui::BeginChild("##bones", ImVec2(kBoneTreeWidth, 0), true);
	if (!pv || !pv->instance)
	{
		ImGui::TextDisabled("No rig bound.");
		ImGui::TextWrapped("Pick a skinned .p3dm in the Rig box above to see its skeleton and pose it.");
		ImGui::EndChild();
		return;
	}

	const std::vector<Bone>& bones = pv->instance->GetSkeletonBones();
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
		const Matrix local = pv->instance->GetBoneLocalTransform(doc.selectedBone);
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
			pv->poseOverrides[doc.selectedBone] = m;
			pv->instance->SetBoneLocalTransform(doc.selectedBone, m);
			pv->instance->RefreshSkinning();
		}
		if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit("Pose bone");

		if (ImGui::Button("Reset to Bind"))
		{
			pv->poseOverrides[doc.selectedBone] = pv->instance->GetBindPoseLocal(doc.selectedBone);
			pv->instance->SetBoneLocalTransform(doc.selectedBone, pv->instance->GetBindPoseLocal(doc.selectedBone));
			pv->instance->RefreshSkinning();
		}
		ImGui::SameLine();
		if (ImGui::Button("Key Bone"))
		{
			const float t = doc.SnapTime(doc.playhead);
			doc.PushSnapshotEdit("Key bone '" + bones[doc.selectedBone].name + "'", [&]() {
				KeyBoneAtTime(doc, doc.selectedBone, t);
			});
			pv->poseOverrides.erase(doc.selectedBone);
		}
	}
	else ImGui::TextDisabled("No bone selected.\nClick a joint in the viewport.");

	ImGui::EndChild();
}

void DrawTransport(AnimationEditorDocument& doc, float dt)
{
	AnimationPreview* pv = doc.preview.get();
	const Animation* clip = doc.ActiveClip();
	const float duration = clip ? clip->Duration : 0.f;
	const float frameStep = (doc.snapFps > 0.f ? 1.f / doc.snapFps : 1.f / 30.f);

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

	ImGui::SameLine();
	ImGui::Checkbox("Loop", &doc.looping);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.f);
	ImGui::DragFloat("Speed", &doc.playSpeed, 0.01f, -4.f, 4.f, "%.2fx");

	ImGui::SameLine();
	ImGui::TextUnformatted("|");
	ImGui::SameLine();
	ImGui::Checkbox("Snap", &doc.snapEnabled);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(70.f);
	ImGui::DragFloat("fps", &doc.snapFps, 1.f, 1.f, 240.f, "%.0f");

	ImGui::SameLine();
	ImGui::TextUnformatted("|");
	ImGui::SameLine();
	ImGui::Checkbox("Auto-key", &doc.autoKey);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Write a key as soon as you finish dragging a bone.");
	ImGui::SameLine();
	ImGui::Checkbox("Pos", &doc.keyPosition);
	ImGui::SameLine();
	ImGui::Checkbox("Rot", &doc.keyRotation);

	ImGui::SameLine();
	if (ImGui::Button("Key Pose")) KeyPendingPose(doc);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Key every bone you have posed since the last key.");
	ImGui::SameLine();
	if (ImGui::Button("Key All Bones")) KeyWholeSkeleton(doc, doc.playhead);
	ImGui::SameLine();
	if (ImGui::Button("Delete Keys")) DeleteSelectedKeys(doc);

	ImGui::SameLine();
	ImGui::TextUnformatted("|");
	ImGui::SameLine();
	ImGui::Text("t = %s / %s", FormatTime(doc.playhead).c_str(), FormatTime(duration).c_str());

	if (pv && !pv->poseOverrides.empty())
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.f, 0.8f, 0.2f, 1.f), "  %d bone(s) posed, unkeyed",
			(int)pv->poseOverrides.size());
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

	// Right-click a key: delete it (and any other selected ones).
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
		DeleteSelectedKeys(doc);
	}

	if (canvasHovered && hoveredKey)
		ImGui::SetTooltip("%s @ %s\nDrag to retime, right-click to delete",
			tracks[hoveredTrack].boneName.c_str(), FormatTime(hoveredTime).c_str());

	ImGui::EndChild();
}

} // namespace

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
		ImGui::Checkbox("Mesh", &pv->showMesh);
		ImGui::SameLine();
		ImGui::Checkbox("Grid", &pv->showGrid);
		ImGui::SameLine();
		ImGui::TextDisabled("(MMB orbit, RMB pan, wheel zoom)");

		// Pose the rig for this frame before it is drawn.
		pv->SyncPose(doc);

		bool dragEnded = false;
		const bool posed = pv->DrawAndUpdate(doc, dragEnded);
		if (posed) doc.selectedBoneName = pv->BoneName(doc.selectedBone);
		if (dragEnded && doc.autoKey)
			KeyPendingPose(doc);
	}
	ImGui::EndChild();
	ImGui::EndChild();

	ImGui::Separator();
	DrawTransport(doc, dt);
	DrawTimeline(doc);

	// Delete key removes the selected key columns, but only while this
	// window has focus - otherwise it would fire while the user is typing a
	// clip name or working in another panel.
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
		&& !ImGui::GetIO().WantTextInput
		&& ImGui::IsKeyPressed(ImGuiKey_Delete))
		DeleteSelectedKeys(doc);
}

} // namespace AnimationEditor
