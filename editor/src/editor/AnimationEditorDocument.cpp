//============================================================================
// Name        : AnimationEditorDocument.cpp
// Description : See AnimationEditorDocument.h.
//============================================================================

#include "AnimationEditorDocument.h"
#include "AnimationPreview.h"

#include <algorithm>
#include <cmath>
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
	return bytes;
}

AnimationEditorDocument::ClipsSnapshot AnimationEditorDocument::Snapshot() const
{
	ClipsSnapshot snap;
	snap.clips = clips;
	snap.activeClip = activeClip;
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
	dirty = true;
}

void AnimationEditorDocument::PushSnapshotEdit(const std::string& description, const std::function<void()>& edit)
{
	ClipsSnapshot before = Snapshot();
	edit();
	ClipsSnapshot after = Snapshot();
	undo.Push(std::unique_ptr<IUndoableCommand>(
		new AnimationSnapshotCommand(this, before, after, description)));
	dirty = true;
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
	if (after.clips.size() == interactiveBefore.clips.size())
	{
		bool identical = true;
		for (size_t c = 0; c < after.clips.size() && identical; c++)
		{
			const p3d::Animation& A = interactiveBefore.clips[c];
			const p3d::Animation& B = after.clips[c];
			if (A.AnimationName != B.AnimationName || A.Duration != B.Duration
				|| A.Channels.size() != B.Channels.size()) { identical = false; break; }
			for (size_t ch = 0; ch < A.Channels.size() && identical; ch++)
			{
				const p3d::Channel& CA = A.Channels[ch];
				const p3d::Channel& CB = B.Channels[ch];
				if (CA.NodeName != CB.NodeName
					|| CA.positions.size() != CB.positions.size()
					|| CA.rotations.size() != CB.rotations.size()
					|| CA.scales.size() != CB.scales.size()) { identical = false; break; }
				for (size_t k = 0; k < CA.positions.size(); k++)
					if (CA.positions[k].Time != CB.positions[k].Time
						|| CA.positions[k].Pos.x != CB.positions[k].Pos.x
						|| CA.positions[k].Pos.y != CB.positions[k].Pos.y
						|| CA.positions[k].Pos.z != CB.positions[k].Pos.z) { identical = false; break; }
				for (size_t k = 0; k < CA.rotations.size() && identical; k++)
					if (CA.rotations[k].Time != CB.rotations[k].Time
						|| CA.rotations[k].Rot.x != CB.rotations[k].Rot.x
						|| CA.rotations[k].Rot.y != CB.rotations[k].Rot.y
						|| CA.rotations[k].Rot.z != CB.rotations[k].Rot.z
						|| CA.rotations[k].Rot.w != CB.rotations[k].Rot.w) { identical = false; break; }
				for (size_t k = 0; k < CA.scales.size() && identical; k++)
					if (CA.scales[k].Time != CB.scales[k].Time
						|| CA.scales[k].Scale.x != CB.scales[k].Scale.x
						|| CA.scales[k].Scale.y != CB.scales[k].Scale.y
						|| CA.scales[k].Scale.z != CB.scales[k].Scale.z) { identical = false; break; }
			}
		}
		if (identical) return;
	}

	undo.Push(std::unique_ptr<IUndoableCommand>(
		new AnimationSnapshotCommand(this, interactiveBefore, after, description)));
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

	const size_t slash = absPath.find_last_of("/\\");
	std::string base = (slash == std::string::npos ? absPath : absPath.substr(slash + 1));
	const size_t dot = base.find_last_of('.');
	displayName = (dot == std::string::npos ? base : base.substr(0, dot));

	dirty = false;
	playhead = 0.f;
	playing = false;
	selectedKeys.clear();
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
