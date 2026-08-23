//============================================================================
// Name        : AnimationEditorDocument.cpp
// Description : See AnimationEditorDocument.h.
//============================================================================

#include "AnimationEditorDocument.h"
#include "AnimationPreview.h"

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
		if (A.AnimationName != B.AnimationName || A.Duration != B.Duration
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
					|| !SameVec3(CA.positions[k].Pos, CB.positions[k].Pos)) return false;
			for (size_t k = 0; k < CA.rotations.size(); k++)
				if (CA.rotations[k].Time != CB.rotations[k].Time
					|| CA.rotations[k].Rot.x != CB.rotations[k].Rot.x
					|| CA.rotations[k].Rot.y != CB.rotations[k].Rot.y
					|| CA.rotations[k].Rot.z != CB.rotations[k].Rot.z
					|| CA.rotations[k].Rot.w != CB.rotations[k].Rot.w) return false;
			for (size_t k = 0; k < CA.scales.size(); k++)
				if (CA.scales[k].Time != CB.scales[k].Time
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
			|| !SameVec3(A.target, B.target) || !SameVec3(A.pole, B.pole)) return false;
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

void AnimationEditorDocument::PushSnapshotEdit(const std::string& description, const std::function<void()>& edit)
{
	ClipsSnapshot before = Snapshot();
	edit();
	ClipsSnapshot after = Snapshot();
	undo.Push(std::unique_ptr<IUndoableCommand>(
		new AnimationSnapshotCommand(this, before, after, description)));
	clipsRevision++;
	dirty = true;
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

	undo.Push(std::unique_ptr<IUndoableCommand>(
		new AnimationSnapshotCommand(this, interactiveBefore, after, description)));
	clipsRevision++;
	dirty = true;
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
