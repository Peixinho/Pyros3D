//============================================================================
// Name        : AnimationEditorDocument.cpp
// Description : See AnimationEditorDocument.h.
//============================================================================

#include "AnimationEditorDocument.h"
#include "AnimationPreview.h"
#include "RetargetSession.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>

using namespace p3d;

namespace {

bool SameTime(float a, float b) { return std::fabs(a - b) <= kAnimKeyEpsilon; }

// Sorts a component's key list by time. The runtime sampler
// (SkeletonAnimationInstance::SampleChannel) walks forward from index 0 and
// stops at the first key whose successor is in the future - which silently
// returns the wrong key, with no error, if the list is ever out of order. So
// every insertion path re-sorts rather than trusting the caller.
template <typename T>
void SortByTime(std::vector<T>& keys)
{
	std::stable_sort(keys.begin(), keys.end(),
		[](const T& a, const T& b) { return a.Time < b.Time; });
}

template <typename T>
bool EraseAtTime(std::vector<T>& keys, float time)
{
	const size_t before = keys.size();
	keys.erase(std::remove_if(keys.begin(), keys.end(),
		[time](const T& k) { return SameTime(k.Time, time); }), keys.end());
	return keys.size() != before;
}

template <typename T>
bool RetimeAtTime(std::vector<T>& keys, float fromTime, float toTime)
{
	bool moved = false;
	for (size_t i = 0; i < keys.size(); i++)
	{
		if (SameTime(keys[i].Time, fromTime))
		{
			keys[i].Time = toTime;
			moved = true;
		}
	}
	if (!moved) return false;

	// Collapse anything the move landed on top of, keeping the key that was
	// actually dragged (it now sits at toTime and was written last above,
	// so scan from the back and drop the earlier duplicates).
	SortByTime(keys);
	for (size_t i = 0; i + 1 < keys.size();)
	{
		if (SameTime(keys[i].Time, keys[i + 1].Time)) keys.erase(keys.begin() + i);
		else i++;
	}
	return true;
}

template <typename T>
void CollectTimes(const std::vector<T>& keys, std::vector<float>& out)
{
	for (size_t i = 0; i < keys.size(); i++) out.push_back(keys[i].Time);
}

void UniqueTimes(std::vector<float>& times)
{
	std::sort(times.begin(), times.end());
	times.erase(std::unique(times.begin(), times.end(),
		[](float a, float b) { return SameTime(a, b); }), times.end());
}

// Undo command holding a whole before/after copy of the document's clips.
// See AnimationEditorDocument::PushSnapshotEdit.
class AnimationSnapshotCommand : public IUndoableCommand {
public:
	AnimationSnapshotCommand(AnimationEditorDocument* doc,
		const AnimationEditorDocument::ClipsSnapshot& before,
		const AnimationEditorDocument::ClipsSnapshot& after,
		const std::string& description)
		: doc_(doc), before_(before), after_(after), description_(description) {}

	void Undo() override { doc_->Restore(before_); }
	void Redo() override { doc_->Restore(after_); }
	std::string Description() const override { return description_; }
	size_t MemoryCost() const override
	{
		return sizeof(*this) + before_.ByteSize() + after_.ByteSize();
	}

private:
	AnimationEditorDocument* doc_;
	AnimationEditorDocument::ClipsSnapshot before_, after_;
	std::string description_;
};

} // namespace

AnimationEditorDocument::AnimationEditorDocument() {}
AnimationEditorDocument::~AnimationEditorDocument() {}

size_t AnimationEditorDocument::ClipsSnapshot::ByteSize() const
{
	size_t bytes = sizeof(ClipsSnapshot);
	for (size_t c = 0; c < clips.size(); c++)
	{
		bytes += sizeof(Animation) + clips[c].AnimationName.capacity();
		for (size_t ch = 0; ch < clips[c].Channels.size(); ch++)
		{
			const Channel& channel = clips[c].Channels[ch];
			bytes += sizeof(Channel) + channel.NodeName.capacity()
				+ channel.positions.size() * sizeof(PositionData)
				+ channel.rotations.size() * sizeof(RotationData)
				+ channel.scales.size() * sizeof(ScalingData);
		}
		bytes += clips[c].Events.size() * sizeof(AnimationEvent);
		for (size_t t = 0; t < clips[c].Tracks.size(); t++)
			bytes += sizeof(PropertyTrack) + clips[c].Tracks[t].Keys.size() * sizeof(PropertyKey);
	}

	// Rig side. Small next to the clips, but counted so UndoStack's byte cap
	// still reflects reality rather than quietly under-reporting.
	bytes += ikHandles.size() * sizeof(IKHandle);
	for (size_t i = 0; i < ikHandles.size(); i++)
		bytes += ikHandles[i].name.capacity() + ikHandles[i].rootBone.capacity()
			+ ikHandles[i].effectorBone.capacity();
	bytes += rig.BoneMasks.size() * sizeof(p3d::BoneMask)
		+ rig.IKChains.size() * sizeof(p3d::IKChainDef)
		+ rig.JointLimits.size() * (sizeof(p3d::JointLimit) + sizeof(std::string));

	bytes += blendEntries.size() * sizeof(AnimationBlendEntry);
	for (size_t i = 0; i < blendEntries.size(); i++)
		bytes += blendEntries[i].layer.capacity();
	bytes += blendLayers.size() * sizeof(AnimationBlendLayer);
	for (size_t i = 0; i < blendLayers.size(); i++)
	{
		bytes += blendLayers[i].name.capacity();
		for (size_t b = 0; b < blendLayers[i].bones.size(); b++)
			bytes += blendLayers[i].bones[b].capacity() + sizeof(std::string);
	}
	return bytes;
}

namespace {

bool SameVec3(const p3d::Math::Vec3& a, const p3d::Math::Vec3& b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

template<typename K>
bool SameInterp(const K& a, const K& b)
{
	return a.Mode == b.Mode && a.InTangent == b.InTangent && a.OutTangent == b.OutTangent;
}

bool SameJointLimits(const std::map<std::string, p3d::JointLimit>& A,
	const std::map<std::string, p3d::JointLimit>& B)
{
	if (A.size() != B.size()) return false;
	std::map<std::string, p3d::JointLimit>::const_iterator ia = A.begin(), ib = B.begin();
	for (; ia != A.end(); ++ia, ++ib)
	{
		if (ia->first != ib->first) return false;
		if (ia->second.Enabled != ib->second.Enabled) return false;
		if (!SameVec3(ia->second.Min, ib->second.Min)) return false;
		if (!SameVec3(ia->second.Max, ib->second.Max)) return false;
	}
	return true;
}

} // namespace

bool AnimationEditorDocument::ClipsSnapshot::Equals(const ClipsSnapshot& o) const
{
	if (activeClip != o.activeClip || activeIK != o.activeIK) return false;

	// ---- clips ----
	if (clips.size() != o.clips.size()) return false;
	for (size_t c = 0; c < clips.size(); c++)
	{
		const p3d::Animation& A = clips[c];
		const p3d::Animation& B = o.clips[c];
		// Every field a clip edit can touch. Interpolation, tangents, flags
		// and fps used to be left out, so a tangent drag - an interactive
		// edit - compared equal to its start and was dropped from undo.
		if (A.AnimationName != B.AnimationName || A.Duration != B.Duration
			|| A.Flags != B.Flags || A.AuthoredFps != B.AuthoredFps
			|| A.RootMotionBone != B.RootMotionBone
			|| !(A.Events == B.Events) || !(A.Tracks == B.Tracks)
			|| A.Channels.size() != B.Channels.size()) return false;
		for (size_t ch = 0; ch < A.Channels.size(); ch++)
		{
			const p3d::Channel& CA = A.Channels[ch];
			const p3d::Channel& CB = B.Channels[ch];
			if (CA.NodeName != CB.NodeName
				|| CA.positions.size() != CB.positions.size()
				|| CA.rotations.size() != CB.rotations.size()
				|| CA.scales.size() != CB.scales.size()) return false;
			for (size_t k = 0; k < CA.positions.size(); k++)
				if (CA.positions[k].Time != CB.positions[k].Time
					|| !SameInterp(CA.positions[k], CB.positions[k])
					|| !SameVec3(CA.positions[k].Pos, CB.positions[k].Pos)) return false;
			for (size_t k = 0; k < CA.rotations.size(); k++)
				if (CA.rotations[k].Time != CB.rotations[k].Time
					|| !SameInterp(CA.rotations[k], CB.rotations[k])
					|| CA.rotations[k].Rot.x != CB.rotations[k].Rot.x
					|| CA.rotations[k].Rot.y != CB.rotations[k].Rot.y
					|| CA.rotations[k].Rot.z != CB.rotations[k].Rot.z
					|| CA.rotations[k].Rot.w != CB.rotations[k].Rot.w) return false;
			for (size_t k = 0; k < CA.scales.size(); k++)
				if (CA.scales[k].Time != CB.scales[k].Time
					|| !SameInterp(CA.scales[k], CB.scales[k])
					|| !SameVec3(CA.scales[k].Scale, CB.scales[k].Scale)) return false;
		}
	}

	// ---- IK chains ----
	if (ikHandles.size() != o.ikHandles.size()) return false;
	for (size_t i = 0; i < ikHandles.size(); i++)
	{
		const IKHandle& A = ikHandles[i];
		const IKHandle& B = o.ikHandles[i];
		if (A.name != B.name || A.rootBone != B.rootBone || A.effectorBone != B.effectorBone
			|| A.usePole != B.usePole || A.enabled != B.enabled || A.targetSet != B.targetSet
			|| !SameVec3(A.target, B.target) || !SameVec3(A.pole, B.pole)
			|| A.holdRotation != B.holdRotation
			|| A.targetRotation.x != B.targetRotation.x || A.targetRotation.y != B.targetRotation.y
			|| A.targetRotation.z != B.targetRotation.z || A.targetRotation.w != B.targetRotation.w) return false;
	}

	// ---- rig sidecar ----
	if (rig.BoneMasks.size() != o.rig.BoneMasks.size()) return false;
	for (size_t i = 0; i < rig.BoneMasks.size(); i++)
		if (rig.BoneMasks[i].Name != o.rig.BoneMasks[i].Name
			|| rig.BoneMasks[i].Bones != o.rig.BoneMasks[i].Bones) return false;
	if (rig.IKChains.size() != o.rig.IKChains.size()) return false;
	for (size_t i = 0; i < rig.IKChains.size(); i++)
	{
		const p3d::IKChainDef& A = rig.IKChains[i];
		const p3d::IKChainDef& B = o.rig.IKChains[i];
		if (A.Name != B.Name || A.RootBone != B.RootBone || A.EffectorBone != B.EffectorBone
			|| A.UsePole != B.UsePole || !SameVec3(A.Pole, B.Pole)) return false;
	}
	if (!SameJointLimits(rig.JointLimits, o.rig.JointLimits)) return false;

	// ---- blend ----
	// playOrder is deliberately not compared: it is a preview handle, not
	// document state, and it changes every time the blend is rebuilt.
	if (blendEntries.size() != o.blendEntries.size()) return false;
	for (size_t i = 0; i < blendEntries.size(); i++)
	{
		const AnimationBlendEntry& A = blendEntries[i];
		const AnimationBlendEntry& B = o.blendEntries[i];
		if (A.clip != B.clip || A.weight != B.weight || A.speed != B.speed
			|| A.repetition != B.repetition || A.layer != B.layer) return false;
	}
	if (blendLayers.size() != o.blendLayers.size()) return false;
	for (size_t i = 0; i < blendLayers.size(); i++)
		if (blendLayers[i].name != o.blendLayers[i].name
			|| blendLayers[i].bones != o.blendLayers[i].bones) return false;

	return true;
}

AnimationEditorDocument::ClipsSnapshot AnimationEditorDocument::Snapshot() const
{
	ClipsSnapshot snap;
	snap.clips = clips;
	snap.activeClip = activeClip;
	snap.ikHandles = ikHandles;
	snap.activeIK = activeIK;
	snap.rig = rig;
	snap.blendEntries = blendEntries;
	snap.blendLayers = blendLayers;
	return snap;
}

void AnimationEditorDocument::Restore(const ClipsSnapshot& snap)
{
	clips = snap.clips;
	activeClip = snap.activeClip;
	if (activeClip >= (int)clips.size()) activeClip = (int)clips.size() - 1;
	// A key that no longer exists must not stay selected - the dope sheet
	// would draw a highlight over empty space and Delete would act on
	// nothing.
	selectedKeys.clear();

	ikHandles = snap.ikHandles;
	activeIK = snap.activeIK;
	if (activeIK >= (int)ikHandles.size()) activeIK = (int)ikHandles.size() - 1;
	rig = snap.rig;
	// The sidecar on disk no longer matches what is in memory. Not written
	// here - undo should not touch the filesystem - but the UI has to stop
	// claiming the rig is saved.
	rigDirty = true;

	blendEntries = snap.blendEntries;
	blendLayers = snap.blendLayers;
	// playOrder is a handle into the preview's currently-playing set, not
	// document state: the restored entries were captured while a different
	// set was playing, so the handles are stale. Clearing them forces
	// RebuildBlend to re-Play() from scratch rather than calling
	// ChangeProperties() on a handle that now means another clip.
	for (size_t i = 0; i < blendEntries.size(); i++)
		blendEntries[i].playOrder = -1;
	blendDataRevision++;
	TouchBlend();

	clipsRevision++;
	dirty = true;
}

// ---- rig access ------------------------------------------------------------
// See the header: `preview` for a .p3da document, `externalRig` for a 2D one.

SkeletonAnimationInstance* AnimationEditorDocument::RigInstance() const
{
	if (externalRig) return externalRig;
	return preview ? preview->instance : NULL;
}

std::map<int, Matrix>* AnimationEditorDocument::RigPoseOverrides()
{
	if (externalRig) return &externalPoseOverrides;
	return preview ? &preview->poseOverrides : NULL;
}

unsigned AnimationEditorDocument::RigBoneCount() const
{
	SkeletonAnimationInstance* inst = RigInstance();
	return inst ? (unsigned)inst->GetNumberBones() : 0u;
}

int AnimationEditorDocument::RigFindBone(const std::string& name) const
{
	SkeletonAnimationInstance* inst = RigInstance();
	if (!inst || name.empty()) return -1;
	const std::vector<Bone>& bones = inst->GetSkeletonBones();
	for (size_t i = 0; i < bones.size(); i++)
		if (bones[i].name == name) return bones[i].self;
	return -1;
}

std::string AnimationEditorDocument::RigBoneName(int boneId) const
{
	SkeletonAnimationInstance* inst = RigInstance();
	if (!inst || boneId < 0) return std::string();
	const std::vector<Bone>& bones = inst->GetSkeletonBones();
	for (size_t i = 0; i < bones.size(); i++)
		if (bones[i].self == boneId) return bones[i].name;
	return std::string();
}

void AnimationEditorDocument::PushSnapshotEdit(const std::string& description, const std::function<void()>& edit)
{
	ClipsSnapshot before = Snapshot();
	edit();
	ClipsSnapshot after = Snapshot();
	if (useOwnUndo)
		undo.Push(std::unique_ptr<IUndoableCommand>(
			new AnimationSnapshotCommand(this, before, after, description)));
	clipsRevision++;
	dirty = true;
	if (onClipsCommitted) onClipsCommitted(before.clips, description);
}

void AnimationEditorDocument::PushBlendEdit(const std::string& description,
	const std::function<void()>& edit, bool rebuild)
{
	ClipsSnapshot before = Snapshot();
	edit();
	ClipsSnapshot after = Snapshot();
	if (after.Equals(before)) return;
	undo.Push(std::unique_ptr<IUndoableCommand>(
		new AnimationSnapshotCommand(this, before, after, description)));
	blendDataRevision++;
	if (rebuild) TouchBlend();
}

void AnimationEditorDocument::BeginBlendEdit()
{
	// Shares interactiveBefore with BeginInteractiveEdit: the two can never
	// be open at once (one is a blend-panel drag, the other a timeline or
	// gizmo drag, and the panels are mutually exclusive modes).
	if (interactiveEditActive) return;
	interactiveBefore = Snapshot();
	interactiveEditActive = true;
}

void AnimationEditorDocument::EndBlendEdit(const std::string& description, bool rebuild)
{
	if (!interactiveEditActive) return;
	interactiveEditActive = false;

	ClipsSnapshot after = Snapshot();
	if (after.Equals(interactiveBefore)) return;
	undo.Push(std::unique_ptr<IUndoableCommand>(
		new AnimationSnapshotCommand(this, interactiveBefore, after, description)));
	blendDataRevision++;
	if (rebuild) TouchBlend();
}

void AnimationEditorDocument::BeginInteractiveEdit()
{
	if (interactiveEditActive) return;
	interactiveBefore = Snapshot();
	interactiveEditActive = true;
}

void AnimationEditorDocument::EndInteractiveEdit(const std::string& description)
{
	if (!interactiveEditActive) return;
	interactiveEditActive = false;

	ClipsSnapshot after = Snapshot();
	// A drag that ended where it started (or a click that only selected)
	// leaves the data untouched - pushing that would make Ctrl+Z appear
	// broken, since undoing it would visibly do nothing.
	if (after.Equals(interactiveBefore)) return;

	if (useOwnUndo)
		undo.Push(std::unique_ptr<IUndoableCommand>(
			new AnimationSnapshotCommand(this, interactiveBefore, after, description)));
	clipsRevision++;
	dirty = true;
	if (onClipsCommitted) onClipsCommitted(interactiveBefore.clips, description);
}

// ---- file I/O --------------------------------------------------------------

bool AnimationEditorDocument::LoadFromFile(const std::string& absPath, std::string& errOut)
{
	AnimationLoader loader;
	if (!loader.Load(absPath))
	{
		errOut = "Failed to read animation file: " + absPath;
		return false;
	}
	clips = loader.animations;
	activeClip = clips.empty() ? -1 : 0;
	absolutePath = absPath;

	// Adopt the first clip's authored frame rate as the snap grid. The
	// editor used to snap at a hard-coded 30 and record nothing, so a clip
	// authored at 24 came back on a grid its keys did not sit on and every
	// subsequent edit quietly nudged them onto 30ths.
	if (!clips.empty() && clips[0].AuthoredFps > 0.f)
		snapFps = clips[0].AuthoredFps;

	const size_t slash = absPath.find_last_of("/\\");
	std::string base = (slash == std::string::npos ? absPath : absPath.substr(slash + 1));
	const size_t dot = base.find_last_of('.');
	displayName = (dot == std::string::npos ? base : base.substr(0, dot));

	dirty = false;
	playhead = 0.f;
	playing = false;
	selectedKeys.clear();
	clipsRevision++;
	undo.Clear();
	return true;
}

bool AnimationEditorDocument::SaveToFile(const std::string& absPath, std::string& errOut)
{
	if (!AnimationLoader::Save(absPath, clips))
	{
		errOut = "Failed to write animation file: " + absPath;
		return false;
	}
	absolutePath = absPath;

	const size_t slash = absPath.find_last_of("/\\");
	std::string base = (slash == std::string::npos ? absPath : absPath.substr(slash + 1));
	const size_t dot = base.find_last_of('.');
	displayName = (dot == std::string::npos ? base : base.substr(0, dot));

	dirty = false;
	return true;
}

// ---- clip operations -------------------------------------------------------

std::string AnimationEditorDocument::MakeUniqueClipName(const std::string& base) const
{
	std::string candidate = base;
	int suffix = 2;
	bool clash = true;
	while (clash)
	{
		clash = false;
		for (size_t i = 0; i < clips.size(); i++)
		{
			if (clips[i].AnimationName == candidate) { clash = true; break; }
		}
		if (clash) candidate = base + " " + std::to_string(suffix++);
	}
	return candidate;
}

int AnimationEditorDocument::AddClip(const std::string& name, float duration)
{
	Animation clip;
	clip.AnimationName = MakeUniqueClipName(name.empty() ? std::string("Clip") : name);
	clip.Duration = (duration > 0.f ? duration : 1.f);
	// Seconds, always - see AnimationLoader::Save's round-trip note. An
	// authored clip that wrote anything else here would be rescaled by the
	// loader on the next open.
	clip.TicksPerSecond = 1.f;
	// A new clip is born on whatever grid the editor is currently snapping
	// to, so reopening it lands back on the same one.
	clip.AuthoredFps = snapFps;
	// Mint the identity here rather than at save time: a scene can reference
	// a clip that has never been written to disk (blend preview plays the
	// document's in-memory clips), and a clip whose guid appeared later
	// would resolve differently before and after the first save.
	clip.Guid = AnimationLoader::GenerateGuid();
	clips.push_back(clip);
	return (int)clips.size() - 1;
}

bool AnimationEditorDocument::RemoveClip(int index)
{
	if (index < 0 || index >= (int)clips.size()) return false;
	clips.erase(clips.begin() + index);
	if (activeClip >= (int)clips.size()) activeClip = (int)clips.size() - 1;
	selectedKeys.clear();
	return true;
}

bool AnimationEditorDocument::RenameClip(int index, const std::string& name)
{
	if (index < 0 || index >= (int)clips.size() || name.empty()) return false;
	clips[index].AnimationName = name;
	return true;
}

bool AnimationEditorDocument::SetClipDuration(int index, float duration)
{
	if (index < 0 || index >= (int)clips.size() || duration <= 0.f) return false;
	clips[index].Duration = duration;
	return true;
}

// ---- channels / keys -------------------------------------------------------

int AnimationEditorDocument::FindChannel(int clipIndex, const std::string& boneName) const
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return -1;
	const Animation& clip = clips[clipIndex];
	for (size_t i = 0; i < clip.Channels.size(); i++)
		if (clip.Channels[i].NodeName == boneName) return (int)i;
	return -1;
}

int AnimationEditorDocument::FindOrCreateChannel(int clipIndex, const std::string& boneName)
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return -1;
	const int existing = FindChannel(clipIndex, boneName);
	if (existing >= 0) return existing;

	Channel ch;
	ch.NodeName = boneName;
	clips[clipIndex].Channels.push_back(ch);
	return (int)clips[clipIndex].Channels.size() - 1;
}

void AnimationEditorDocument::CollectKeyTimes(int clipIndex, int channel, std::vector<float>& out) const
{
	out.clear();
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return;
	const Animation& clip = clips[clipIndex];
	if (channel < 0 || channel >= (int)clip.Channels.size()) return;

	const Channel& ch = clip.Channels[channel];
	CollectTimes(ch.positions, out);
	CollectTimes(ch.rotations, out);
	CollectTimes(ch.scales, out);
	UniqueTimes(out);
}

void AnimationEditorDocument::CollectAllKeyTimes(int clipIndex, std::vector<float>& out) const
{
	out.clear();
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return;
	const Animation& clip = clips[clipIndex];
	for (size_t c = 0; c < clip.Channels.size(); c++)
	{
		CollectTimes(clip.Channels[c].positions, out);
		CollectTimes(clip.Channels[c].rotations, out);
		CollectTimes(clip.Channels[c].scales, out);
	}
	UniqueTimes(out);
}

void AnimationEditorDocument::SetKey(int clipIndex, int channel, float time,
	const Vec3& pos, const Quaternion& rot, const Vec3& scale,
	bool doPos, bool doRot, bool doScale)
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return;
	Animation& clip = clips[clipIndex];
	if (channel < 0 || channel >= (int)clip.Channels.size()) return;
	Channel& ch = clip.Channels[channel];

	if (doPos)
	{
		EraseAtTime(ch.positions, time);
		ch.positions.push_back(PositionData(time, pos));
		SortByTime(ch.positions);
	}
	if (doRot)
	{
		EraseAtTime(ch.rotations, time);
		ch.rotations.push_back(RotationData(time, rot));
		SortByTime(ch.rotations);
	}
	if (doScale)
	{
		EraseAtTime(ch.scales, time);
		ch.scales.push_back(ScalingData(time, scale));
		SortByTime(ch.scales);
	}

	// A key past the end would be unreachable at runtime (playback stops at
	// Duration), so growing the clip to fit is the only non-surprising
	// behaviour - the alternative is silently discarding the user's key.
	if (time > clip.Duration) clip.Duration = time;
}

namespace {

// Applies `fn` to every key within kAnimKeyEpsilon of `time` across all three
// component lists of a channel. The dope sheet edits key *columns* - one
// diamond stands for whatever position/rotation/scale keys share that instant -
// so interpolation has to be set on all of them together or the components
// would drift into different curve shapes behind a single piece of UI.
template<typename Fn>
int ForEachKeyAtTime(p3d::Channel& ch, float time, Fn fn)
{
	int touched = 0;
	for (size_t i = 0; i < ch.positions.size(); i++)
		if (SameTime(ch.positions[i].Time, time))
		{ fn(ch.positions[i].Mode, ch.positions[i].InTangent, ch.positions[i].OutTangent); touched++; }
	for (size_t i = 0; i < ch.rotations.size(); i++)
		if (SameTime(ch.rotations[i].Time, time))
		{ fn(ch.rotations[i].Mode, ch.rotations[i].InTangent, ch.rotations[i].OutTangent); touched++; }
	for (size_t i = 0; i < ch.scales.size(); i++)
		if (SameTime(ch.scales[i].Time, time))
		{ fn(ch.scales[i].Mode, ch.scales[i].InTangent, ch.scales[i].OutTangent); touched++; }
	return touched;
}

} // namespace

int AnimationEditorDocument::SetKeyInterpolation(int clipIndex, int channel, float time,
	int mode, float inTangent, float outTangent)
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return 0;
	Animation& clip = clips[clipIndex];
	if (channel < 0 || channel >= (int)clip.Channels.size()) return 0;

	return ForEachKeyAtTime(clip.Channels[channel], time,
		[&](uchar& m, float& in, float& out)
		{
			m = (uchar)mode;
			in = inTangent;
			out = outTangent;
		});
}

bool AnimationEditorDocument::GetKeyInterpolation(int clipIndex, int channel, float time,
	int& outMode, float& outIn, float& outOut) const
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return false;
	const Animation& clip = clips[clipIndex];
	if (channel < 0 || channel >= (int)clip.Channels.size()) return false;
	const Channel& ch = clip.Channels[channel];

	// First key found at this instant wins. SetKeyInterpolation writes all
	// three components together, so they only disagree for a clip authored
	// before this existed - reporting one of them is still the right thing
	// to show, and editing it re-syncs them.
	for (size_t i = 0; i < ch.positions.size(); i++)
		if (SameTime(ch.positions[i].Time, time))
		{ outMode = ch.positions[i].Mode; outIn = ch.positions[i].InTangent; outOut = ch.positions[i].OutTangent; return true; }
	for (size_t i = 0; i < ch.rotations.size(); i++)
		if (SameTime(ch.rotations[i].Time, time))
		{ outMode = ch.rotations[i].Mode; outIn = ch.rotations[i].InTangent; outOut = ch.rotations[i].OutTangent; return true; }
	for (size_t i = 0; i < ch.scales.size(); i++)
		if (SameTime(ch.scales[i].Time, time))
		{ outMode = ch.scales[i].Mode; outIn = ch.scales[i].InTangent; outOut = ch.scales[i].OutTangent; return true; }
	return false;
}

bool AnimationEditorDocument::DeleteKeysAtTime(int clipIndex, int channel, float time)
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return false;
	Animation& clip = clips[clipIndex];
	if (channel < 0 || channel >= (int)clip.Channels.size()) return false;
	Channel& ch = clip.Channels[channel];

	bool removed = false;
	removed |= EraseAtTime(ch.positions, time);
	removed |= EraseAtTime(ch.rotations, time);
	removed |= EraseAtTime(ch.scales, time);
	return removed;
}

bool AnimationEditorDocument::MoveKeysAtTime(int clipIndex, int channel, float fromTime, float toTime)
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return false;
	Animation& clip = clips[clipIndex];
	if (channel < 0 || channel >= (int)clip.Channels.size()) return false;
	if (SameTime(fromTime, toTime)) return false;
	Channel& ch = clip.Channels[channel];

	bool moved = false;
	moved |= RetimeAtTime(ch.positions, fromTime, toTime);
	moved |= RetimeAtTime(ch.rotations, fromTime, toTime);
	moved |= RetimeAtTime(ch.scales, fromTime, toTime);
	if (moved && toTime > clip.Duration) clip.Duration = toTime;
	return moved;
}

bool AnimationEditorDocument::HasKeyAtTime(int clipIndex, int channel, float time) const
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return false;
	const Animation& clip = clips[clipIndex];
	if (channel < 0 || channel >= (int)clip.Channels.size()) return false;
	const Channel& ch = clip.Channels[channel];

	for (size_t i = 0; i < ch.positions.size(); i++) if (SameTime(ch.positions[i].Time, time)) return true;
	for (size_t i = 0; i < ch.rotations.size(); i++) if (SameTime(ch.rotations[i].Time, time)) return true;
	for (size_t i = 0; i < ch.scales.size(); i++) if (SameTime(ch.scales[i].Time, time)) return true;
	return false;
}

void AnimationEditorDocument::PruneEmptyChannels(int clipIndex)
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return;
	std::vector<Channel>& channels = clips[clipIndex].Channels;
	channels.erase(std::remove_if(channels.begin(), channels.end(),
		[](const Channel& c) {
			return c.positions.empty() && c.rotations.empty() && c.scales.empty();
		}), channels.end());
}

// ---- any key kind ---------------------------------------------------------

namespace {

// The component list a split row stands for.
template <typename Fn>
bool WithComponent(p3d::Channel& ch, int kind, Fn fn)
{
	switch (kind)
	{
	case ANIM_KEY_POSITION: return fn(ch.positions);
	case ANIM_KEY_ROTATION: return fn(ch.rotations);
	case ANIM_KEY_SCALE:    return fn(ch.scales);
	default: return false;
	}
}

} // namespace

bool AnimationEditorDocument::DeleteKeyRef(int clipIndex, const AnimKeyRef& ref)
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return false;
	Animation& clip = clips[clipIndex];
	switch (ref.kind)
	{
	case ANIM_KEY_COLUMN:
		return DeleteKeysAtTime(clipIndex, ref.channel, ref.time);
	case ANIM_KEY_POSITION: case ANIM_KEY_ROTATION: case ANIM_KEY_SCALE:
		if (ref.channel < 0 || ref.channel >= (int)clip.Channels.size()) return false;
		return WithComponent(clip.Channels[ref.channel], ref.kind,
			[&](auto& keys) { return EraseAtTime(keys, ref.time); });
	case ANIM_KEY_TRACK:
		if (ref.channel < 0 || ref.channel >= (int)clip.Tracks.size()) return false;
		return EraseAtTime(clip.Tracks[ref.channel].Keys, ref.time);
	case ANIM_KEY_EVENT:
		return RemoveEvent(clipIndex, ref.channel);
	}
	return false;
}

AnimKeyRef AnimationEditorDocument::MoveKeyRef(int clipIndex, const AnimKeyRef& ref, float toTime, bool& ok)
{
	ok = false;
	AnimKeyRef out = ref;
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return out;
	Animation& clip = clips[clipIndex];
	if (toTime < 0.f) toTime = 0.f;
	switch (ref.kind)
	{
	case ANIM_KEY_COLUMN:
		ok = MoveKeysAtTime(clipIndex, ref.channel, ref.time, toTime);
		break;
	case ANIM_KEY_POSITION: case ANIM_KEY_ROTATION: case ANIM_KEY_SCALE:
		if (ref.channel >= 0 && ref.channel < (int)clip.Channels.size() && !SameTime(ref.time, toTime))
			ok = WithComponent(clip.Channels[ref.channel], ref.kind,
				[&](auto& keys) { return RetimeAtTime(keys, ref.time, toTime); });
		break;
	case ANIM_KEY_TRACK:
		if (ref.channel >= 0 && ref.channel < (int)clip.Tracks.size() && !SameTime(ref.time, toTime))
			ok = RetimeAtTime(clip.Tracks[ref.channel].Keys, ref.time, toTime);
		break;
	case ANIM_KEY_EVENT:
		if (ref.channel >= 0 && ref.channel < (int)clip.Events.size())
		{
			clip.Events[ref.channel].Time = toTime;
			ok = true;
		}
		break;
	}
	if (ok)
	{
		out.time = toTime;
		if (toTime > clip.Duration) clip.Duration = toTime;
	}
	return out;
}

void AnimationEditorDocument::CollectComponentTimes(int clipIndex, int channel, int kind, std::vector<float>& out) const
{
	out.clear();
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return;
	const Animation& clip = clips[clipIndex];
	if (channel < 0 || channel >= (int)clip.Channels.size()) return;
	const Channel& ch = clip.Channels[channel];
	if (kind == ANIM_KEY_POSITION) CollectTimes(ch.positions, out);
	else if (kind == ANIM_KEY_ROTATION) CollectTimes(ch.rotations, out);
	else if (kind == ANIM_KEY_SCALE) CollectTimes(ch.scales, out);
	UniqueTimes(out);
}

int AnimationEditorDocument::FindTrack(int clipIndex, const std::string& target, const std::string& property) const
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return -1;
	const Animation& clip = clips[clipIndex];
	for (size_t i = 0; i < clip.Tracks.size(); i++)
		if (clip.Tracks[i].Target == target && clip.Tracks[i].Property == property) return (int)i;
	return -1;
}

int AnimationEditorDocument::FindOrCreateTrack(int clipIndex, const std::string& target, const std::string& property)
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return -1;
	const int existing = FindTrack(clipIndex, target, property);
	if (existing >= 0) return existing;
	PropertyTrack t;
	t.Target = target;
	t.Property = property;
	clips[clipIndex].Tracks.push_back(t);
	return (int)clips[clipIndex].Tracks.size() - 1;
}

void AnimationEditorDocument::SetTrackKey(int clipIndex, int track, float time, const Vec4& value,
	const std::string& text, int mode)
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return;
	Animation& clip = clips[clipIndex];
	if (track < 0 || track >= (int)clip.Tracks.size()) return;
	std::vector<PropertyKey>& keys = clip.Tracks[track].Keys;
	EraseAtTime(keys, time);
	PropertyKey k;
	k.Time = time;
	k.Value = value;
	k.Text = text;
	k.Mode = (uchar)mode;
	keys.push_back(k);
	SortByTime(keys);
	if (time > clip.Duration) clip.Duration = time;
}

void AnimationEditorDocument::PruneEmptyTracks(int clipIndex)
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return;
	std::vector<PropertyTrack>& t = clips[clipIndex].Tracks;
	t.erase(std::remove_if(t.begin(), t.end(), [](const PropertyTrack& x) { return x.Keys.empty(); }), t.end());
}

int AnimationEditorDocument::AddEvent(int clipIndex, float time, const std::string& name)
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return -1;
	clips[clipIndex].Events.push_back(AnimationEvent(time, name.empty() ? std::string("event") : name));
	return (int)clips[clipIndex].Events.size() - 1;
}

bool AnimationEditorDocument::RemoveEvent(int clipIndex, int eventIndex)
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return false;
	std::vector<AnimationEvent>& e = clips[clipIndex].Events;
	if (eventIndex < 0 || eventIndex >= (int)e.size()) return false;
	e.erase(e.begin() + eventIndex);
	// Selection refers to events by index: drop event refs, as the indices
	// above the removed one have all shifted.
	for (std::set<AnimKeyRef>::iterator it = selectedKeys.begin(); it != selectedKeys.end();)
		if (it->kind == ANIM_KEY_EVENT) it = selectedKeys.erase(it); else ++it;
	return true;
}

bool AnimationEditorDocument::RenameEvent(int clipIndex, int eventIndex, const std::string& name)
{
	if (clipIndex < 0 || clipIndex >= (int)clips.size()) return false;
	std::vector<AnimationEvent>& e = clips[clipIndex].Events;
	if (eventIndex < 0 || eventIndex >= (int)e.size() || name.empty()) return false;
	e[eventIndex].Name = name;
	return true;
}

bool AnimationEditorDocument::CopySelectedKeys(AnimKeyClipboard& out) const
{
	out = AnimKeyClipboard();
	const Animation* clip = ActiveClip();
	if (!clip || selectedKeys.empty()) return false;
	float t0 = 1e30f;
	for (std::set<AnimKeyRef>::const_iterator it = selectedKeys.begin(); it != selectedKeys.end(); ++it)
		t0 = std::min(t0, it->time);

	std::map<std::string, size_t> chanOf;
	std::map<std::pair<std::string, std::string>, size_t> trackOf;
	for (std::set<AnimKeyRef>::const_iterator it = selectedKeys.begin(); it != selectedKeys.end(); ++it)
	{
		const AnimKeyRef& r = *it;
		if (r.kind <= ANIM_KEY_SCALE)
		{
			if (r.channel < 0 || r.channel >= (int)clip->Channels.size()) continue;
			const Channel& src = clip->Channels[r.channel];
			std::map<std::string, size_t>::iterator c = chanOf.find(src.NodeName);
			if (c == chanOf.end())
			{
				Channel ch;
				ch.NodeName = src.NodeName;
				out.channels.push_back(ch);
				c = chanOf.insert(std::make_pair(src.NodeName, out.channels.size() - 1)).first;
			}
			Channel& dst = out.channels[c->second];
			const bool all = r.kind == ANIM_KEY_COLUMN;
			if (all || r.kind == ANIM_KEY_POSITION)
				for (size_t k = 0; k < src.positions.size(); k++)
					if (SameTime(src.positions[k].Time, r.time)) { PositionData d = src.positions[k]; d.Time -= t0; dst.positions.push_back(d); }
			if (all || r.kind == ANIM_KEY_ROTATION)
				for (size_t k = 0; k < src.rotations.size(); k++)
					if (SameTime(src.rotations[k].Time, r.time)) { RotationData d = src.rotations[k]; d.Time -= t0; dst.rotations.push_back(d); }
			if (all || r.kind == ANIM_KEY_SCALE)
				for (size_t k = 0; k < src.scales.size(); k++)
					if (SameTime(src.scales[k].Time, r.time)) { ScalingData d = src.scales[k]; d.Time -= t0; dst.scales.push_back(d); }
		}
		else if (r.kind == ANIM_KEY_TRACK)
		{
			if (r.channel < 0 || r.channel >= (int)clip->Tracks.size()) continue;
			const PropertyTrack& src = clip->Tracks[r.channel];
			const std::pair<std::string, std::string> id(src.Target, src.Property);
			std::map<std::pair<std::string, std::string>, size_t>::iterator t = trackOf.find(id);
			if (t == trackOf.end())
			{
				PropertyTrack tr;
				tr.Target = src.Target;
				tr.Property = src.Property;
				out.tracks.push_back(tr);
				t = trackOf.insert(std::make_pair(id, out.tracks.size() - 1)).first;
			}
			for (size_t k = 0; k < src.Keys.size(); k++)
				if (SameTime(src.Keys[k].Time, r.time)) { PropertyKey d = src.Keys[k]; d.Time -= t0; out.tracks[t->second].Keys.push_back(d); }
		}
		else if (r.kind == ANIM_KEY_EVENT)
		{
			if (r.channel < 0 || r.channel >= (int)clip->Events.size()) continue;
			AnimationEvent e = clip->Events[r.channel];
			e.Time -= t0;
			out.events.push_back(e);
		}
	}
	return !out.Empty();
}

int AnimationEditorDocument::PasteKeys(const AnimKeyClipboard& cb, float time)
{
	if (!HasActiveClip() || cb.Empty()) return 0;
	const int ci = activeClip;
	int written = 0;
	selectedKeys.clear();
	for (size_t c = 0; c < cb.channels.size(); c++)
	{
		const Channel& src = cb.channels[c];
		const int ch = FindOrCreateChannel(ci, src.NodeName);
		if (ch < 0) continue;
		Channel& dst = clips[ci].Channels[ch];
		for (size_t k = 0; k < src.positions.size(); k++)
		{
			PositionData d = src.positions[k]; d.Time += time;
			EraseAtTime(dst.positions, d.Time); dst.positions.push_back(d); written++;
			AnimKeyRef r; r.channel = ch; r.time = d.Time; selectedKeys.insert(r);
		}
		for (size_t k = 0; k < src.rotations.size(); k++)
		{
			RotationData d = src.rotations[k]; d.Time += time;
			EraseAtTime(dst.rotations, d.Time); dst.rotations.push_back(d); written++;
			AnimKeyRef r; r.channel = ch; r.time = d.Time; selectedKeys.insert(r);
		}
		for (size_t k = 0; k < src.scales.size(); k++)
		{
			ScalingData d = src.scales[k]; d.Time += time;
			EraseAtTime(dst.scales, d.Time); dst.scales.push_back(d); written++;
			AnimKeyRef r; r.channel = ch; r.time = d.Time; selectedKeys.insert(r);
		}
		SortByTime(dst.positions); SortByTime(dst.rotations); SortByTime(dst.scales);
	}
	for (size_t t = 0; t < cb.tracks.size(); t++)
	{
		const int tr = FindOrCreateTrack(ci, cb.tracks[t].Target, cb.tracks[t].Property);
		for (size_t k = 0; k < cb.tracks[t].Keys.size(); k++)
		{
			const PropertyKey& key = cb.tracks[t].Keys[k];
			SetTrackKey(ci, tr, key.Time + time, key.Value, key.Text, key.Mode);
			written++;
			AnimKeyRef r; r.kind = ANIM_KEY_TRACK; r.channel = tr; r.time = key.Time + time; selectedKeys.insert(r);
		}
	}
	for (size_t e = 0; e < cb.events.size(); e++)
	{
		const int idx = AddEvent(ci, cb.events[e].Time + time, cb.events[e].Name);
		written++;
		AnimKeyRef r; r.kind = ANIM_KEY_EVENT; r.channel = idx; r.time = cb.events[e].Time + time; selectedKeys.insert(r);
	}
	// Keys pasted past the end grow the clip, as keying there does.
	Animation& clip = clips[ci];
	for (std::set<AnimKeyRef>::const_iterator it = selectedKeys.begin(); it != selectedKeys.end(); ++it)
		if (it->time > clip.Duration) clip.Duration = it->time;
	return written;
}

float AnimationEditorDocument::SnapTime(float time) const
{
	float snapped = time;
	if (snapEnabled && snapFps > 0.f)
		snapped = std::floor(time * snapFps + 0.5f) / snapFps;
	if (snapped < 0.f) snapped = 0.f;
	const Animation* clip = ActiveClip();
	if (clip && snapped > clip->Duration) snapped = clip->Duration;
	return snapped;
}

// ---- blending --------------------------------------------------------------

AnimationBlendLayer* AnimationEditorDocument::FindBlendLayer(const std::string& name)
{
	for (size_t i = 0; i < blendLayers.size(); i++)
		if (blendLayers[i].name == name) return &blendLayers[i];
	return NULL;
}

const AnimationBlendLayer* AnimationEditorDocument::FindBlendLayer(const std::string& name) const
{
	for (size_t i = 0; i < blendLayers.size(); i++)
		if (blendLayers[i].name == name) return &blendLayers[i];
	return NULL;
}

AnimationBlendLayer& AnimationEditorDocument::EnsureBlendLayer(const std::string& name)
{
	if (AnimationBlendLayer* existing = FindBlendLayer(name)) return *existing;
	AnimationBlendLayer layer;
	layer.name = name;
	blendLayers.push_back(layer);
	return blendLayers.back();
}

void AnimationEditorDocument::RemoveBlendLayer(const std::string& name)
{
	for (size_t i = 0; i < blendLayers.size(); i++)
	{
		if (blendLayers[i].name != name) continue;
		blendLayers.erase(blendLayers.begin() + i);
		break;
	}
	// An entry naming a layer that no longer exists would be played against a
	// null layer at runtime, so drop the reference rather than leave it.
	for (size_t i = 0; i < blendEntries.size(); i++)
		if (blendEntries[i].layer == name) blendEntries[i].layer.clear();
	TouchBlend();
}

std::string AnimationEditorDocument::BuildBlendLuaSnippet(const std::string& animationAssetPath) const
{
	// Deliberately emits the same calls the preview makes, in the same order,
	// so what was tuned here is what the game gets. Weights are written as the
	// engine's own `scale` argument (see AnimationPreview::RebuildBlend for
	// why that is 1 - weight), with the animator-facing value in a comment so
	// the numbers in the editor can still be recognised in the script.
	std::string out;
	out += "-- Generated by the Animation Editor's Blend tab.\n";
	out += "-- Attach to the GameObject whose RenderingComponent carries this rig.\n";
	out += "-- Clip ids are indices into " + (animationAssetPath.empty() ? std::string("the .p3da") : animationAssetPath) + ".\n";
	out += "local Blend = class('Blend')\n\n";
	out += "function Blend:initialize()\n";
	out += "\tself.clock = 0\n";
	out += "end\n\n";
	out += "function Blend:init(owner)\n";
	out += "\tlocal rc = owner:getComponent(\"RenderingComponent\")\n";
	out += "\tif not rc then return end\n";
	out += "\tself.instance = rc:getActiveSkeletonAnimation()\n";
	out += "\tif not self.instance then return end\n";
	out += "\tself.anim = self.instance:getOwner()\n\n";

	for (size_t i = 0; i < blendLayers.size(); i++)
	{
		const AnimationBlendLayer& layer = blendLayers[i];
		bool used = false;
		for (size_t e = 0; e < blendEntries.size(); e++)
			if (blendEntries[e].layer == layer.name) { used = true; break; }
		if (!used) continue;

		out += "\tself.instance:createLayer(\"" + layer.name + "\")\n";
		for (size_t b = 0; b < layer.bones.size(); b++)
			out += "\tself.instance:addBone(\"" + layer.name + "\", \"" + layer.bones[b] + "\")\n";
		out += "\n";
	}

	for (size_t i = 0; i < blendEntries.size(); i++)
	{
		const AnimationBlendEntry& e = blendEntries[i];
		char buf[256];
		std::snprintf(buf, sizeof(buf), "%.3f, %.3f, %.3f", e.repetition, e.speed, BlendWeightToScale(e.weight));
		out += "\tself.order" + std::to_string(i) + " = self.instance:play("
			+ std::to_string(e.clip) + ", 0, " + buf;
		if (!e.layer.empty()) out += ", \"" + e.layer + "\"";
		out += ")";
		char wbuf[64];
		std::snprintf(wbuf, sizeof(wbuf), "%.0f%%", e.weight * 100.f);
		out += "  -- weight " + std::string(wbuf) + "\n";
	}

	out += "end\n\n";
	out += "function Blend:update(dt)\n";
	out += "\t-- SkeletonAnimation:update() wants a clock counting up from the\n";
	out += "\t-- first call; scripts are handed a frame delta, so accumulate it.\n";
	out += "\tself.clock = self.clock + dt\n";
	out += "\tif self.anim then self.anim:update(self.clock) end\n\n";
	out += "\t-- Drive the weights from whatever gameplay state should decide\n";
	out += "\t-- them. changeProperties' last argument is the same `scale` as\n";
	out += "\t-- play()'s above; 0 shows that clip fully, 1 hides it behind the\n";
	out += "\t-- ones after it.\n";
	if (blendEntries.size() >= 2)
	{
		out += "\t-- e.g. crossfade the first two on some 0..1 value `t`:\n";
		out += "\t-- local p = self.instance:getAnimationCurrentProgress(self.order1)\n";
		out += "\t-- self.instance:changeProperties(self.order0, p, -1, 1.0, t)\n";
		out += "\t-- self.instance:changeProperties(self.order1, p, -1, 1.0, 1.0 - t)\n";
	}
	out += "end\n\n";
	out += "return Blend\n";
	return out;
}

void AnimationEditorDocument::LoadRigForMesh()
{
	rig = p3d::RigAsset();
	rigPath.clear();
	rigDirty = false;
	if (meshPath.empty()) return;

	rigPath = p3d::RigAsset::SidecarPathFor(meshPath);
	// A missing sidecar loads as empty and is not an error - most models
	// have no authored rig data, and the file appears the first time
	// something is saved into it.
	rig.Load(rigPath);

	// Layers that came from project.json but are not in the sidecar yet are
	// a pending migration - flag the rig dirty so the UI shows there is
	// something unsaved rather than leaving it to be noticed later.
	for (size_t i = 0; i < blendLayers.size(); i++)
		if (!rig.FindMask(blendLayers[i].name)) { rigDirty = true; break; }

	SyncDocumentFromRig();
}

void AnimationEditorDocument::SyncDocumentFromRig()
{
	// Bone masks become the document's blend layers. These used to live in
	// project.json keyed by ANIMATION path, which meant one copy per clip
	// and no way to reuse "UpperBody" across a rig's clips; a mask is a
	// property of the skeleton, so the rig file is its real home.
	//
	// Anything already in blendLayers that the rig does not name is kept -
	// that is a layer authored before the migration, and dropping it would
	// silently discard the user's work.
	for (size_t i = 0; i < rig.BoneMasks.size(); i++)
	{
		const p3d::BoneMask& mask = rig.BoneMasks[i];
		AnimationBlendLayer* existing = FindBlendLayer(mask.Name);
		if (existing) existing->bones = mask.Bones;
		else
		{
			AnimationBlendLayer layer;
			layer.name = mask.Name;
			layer.bones = mask.Bones;
			blendLayers.push_back(layer);
		}
	}

	for (size_t i = 0; i < rig.IKChains.size(); i++)
	{
		const p3d::IKChainDef& def = rig.IKChains[i];
		bool found = false;
		for (size_t k = 0; k < ikHandles.size(); k++)
			if (ikHandles[k].name == def.Name) { found = true; break; }
		if (found) continue;

		IKHandle h;
		h.name = def.Name;
		h.rootBone = def.RootBone;
		h.effectorBone = def.EffectorBone;
		h.pole = def.Pole;
		h.usePole = def.UsePole;
		ikHandles.push_back(h);
	}
	if (activeIK < 0 && !ikHandles.empty()) activeIK = 0;

	TouchBlend();
}

bool AnimationEditorDocument::SaveRig()
{
	// The sidecar's path is derived from the mesh, so with no mesh bound
	// there is nowhere to put it.
	if (rigPath.empty()) return false;

	rig.BoneMasks.clear();
	for (size_t i = 0; i < blendLayers.size(); i++)
	{
		p3d::BoneMask mask;
		mask.Name = blendLayers[i].name;
		mask.Bones = blendLayers[i].bones;
		rig.BoneMasks.push_back(mask);
	}

	rig.IKChains.clear();
	for (size_t i = 0; i < ikHandles.size(); i++)
	{
		// A half-finished chain would come back as an unresolvable one, so
		// it is not worth persisting.
		if (ikHandles[i].rootBone.empty() || ikHandles[i].effectorBone.empty()) continue;
		p3d::IKChainDef def;
		def.Name = ikHandles[i].name;
		def.RootBone = ikHandles[i].rootBone;
		def.EffectorBone = ikHandles[i].effectorBone;
		def.Pole = ikHandles[i].pole;
		def.UsePole = ikHandles[i].usePole;
		rig.IKChains.push_back(def);
	}

	// Joint limits are edited directly on `rig` (they have no working copy -
	// nothing else in the document needs them), so they are already current.
	const bool ok = rig.Save(rigPath);
	if (ok) rigDirty = false;
	return ok;
}

nlohmann::json AnimationEditorDocument::BlendToJson() const
{
	nlohmann::json j;
	nlohmann::json entries = nlohmann::json::array();
	for (size_t i = 0; i < blendEntries.size(); i++)
	{
		nlohmann::json e;
		e["clip"] = blendEntries[i].clip;
		e["weight"] = blendEntries[i].weight;
		e["speed"] = blendEntries[i].speed;
		e["repetition"] = blendEntries[i].repetition;
		e["layer"] = blendEntries[i].layer;
		entries.push_back(e);
	}
	j["entries"] = std::move(entries);

	// Layers are NOT written here when a rig sidecar owns them.
	//
	// A bone mask is a property of the skeleton, not of one clip: keeping it
	// in project.json keyed by animation path meant a separate copy of
	// "UpperBody" per clip, all of which had to be edited together and none
	// of which survived a rename. <model>.rig.json is its real home.
	//
	// They are still written when no mesh is bound, because then there is no
	// sidecar to write to and dropping them here would lose them outright.
	// That makes this a one-way migration: a legacy project's layers are read
	// below, land in the rig on the next Save Rig, and stop being written
	// here from then on.
	if (rigPath.empty())
	{
		nlohmann::json layers = nlohmann::json::array();
		for (size_t i = 0; i < blendLayers.size(); i++)
		{
			nlohmann::json l;
			l["name"] = blendLayers[i].name;
			l["bones"] = blendLayers[i].bones;
			layers.push_back(l);
		}
		j["layers"] = std::move(layers);
	}
	return j;
}

void AnimationEditorDocument::BlendFromJson(const nlohmann::json& j)
{
	blendEntries.clear();
	blendLayers.clear();
	if (!j.is_object()) { TouchBlend(); return; }

	// Still read, for projects written before the rig sidecar existed.
	// SyncDocumentFromRig() merges the sidecar's masks on top of whatever
	// lands here, preferring the sidecar when both name the same layer.
	if (j.contains("layers") && j["layers"].is_array())
	{
		for (const auto& l : j["layers"])
		{
			AnimationBlendLayer layer;
			layer.name = l.value("name", std::string());
			if (layer.name.empty()) continue;
			if (l.contains("bones") && l["bones"].is_array())
				for (const auto& b : l["bones"])
					if (b.is_string()) layer.bones.push_back(b.get<std::string>());
			blendLayers.push_back(layer);
		}
	}
	if (j.contains("entries") && j["entries"].is_array())
	{
		for (const auto& e : j["entries"])
		{
			AnimationBlendEntry entry;
			entry.clip = e.value("clip", 0);
			entry.weight = e.value("weight", 1.f);
			entry.speed = e.value("speed", 1.f);
			entry.repetition = e.value("repetition", -1.f);
			entry.layer = e.value("layer", std::string());
			// A saved entry can outlive the clip it named (clips removed
			// since), and Play() with an out-of-range id is rejected anyway.
			if (entry.clip < 0 || entry.clip >= (int)clips.size()) continue;
			blendEntries.push_back(entry);
		}
	}
	TouchBlend();
}
