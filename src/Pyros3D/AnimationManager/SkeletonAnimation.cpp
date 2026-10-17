//============================================================================
// Name        : SkeletonAnimation.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Animator Interface
//============================================================================

#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/AnimationManager/IKSolver.h>
#include <iterator>
#include <cmath>
#include <algorithm>
#include <Pyros3D/GameObjects/GameObject.h>

namespace p3d {

	SkeletonAnimationInstance::~SkeletonAnimationInstance()
	{
		for (std::map<uint32, _SkeletonAnimation::AnimationLayer*>::iterator i = Layers.begin(); i != Layers.end(); i++)
			delete (*i).second;
	}

	SkeletonAnimationInstance::SkeletonAnimationInstance(SkeletonAnimation* owner, RenderingComponent* Component)
	{
		// Keep Rendering Component
		rcomp = Component;
		Component->SetActiveSkeletonAnimation(this);

		// Resize Vectors
		skeleton.resize(Component->GetSkeleton().size());
		Bones.resize(skeleton.size());
		bindPose.resize(skeleton.size());
		boneTransformation.resize(skeleton.size());

		// Get Skeleton
		for (std::map<uint32, Bone>::const_iterator a = Component->GetSkeleton().begin(); a != Component->GetSkeleton().end(); a++)
		{
			// Set Bones Transformation based on Bone ID
			bindPose[(*a).second.self] = (*a).second.bindPoseMat; // Copy BindPose
			skeleton[(*a).second.self] = (*a).second;
		}

		// Start at the BIND pose, not at identity. boneTransformation is the
		// per-bone LOCAL transform array; leaving it default-constructed made
		// Bones[] identity, so every SkinningBones entry below came out as a
		// bare BoneOffsetMatrix - the inverse of the bone's global bind
		// transform - and the mesh rendered inside-out until something
		// happened to call Play(). The bind local composed up the parent chain
		// is exactly the inverse of BoneOffsetMatrix, so seeding it here makes
		// the product identity and the rig sits in the pose it was modelled
		// in, which is what "no animation playing" should look like.
		boneTransformation = bindPose;

		// Multiply bones with its parent
		for (std::vector<Bone>::iterator a = skeleton.begin(); a != skeleton.end(); a++)
		{
			Bones[(*a).self] = GetParentMatrix((*a).parent, boneTransformation) * boneTransformation[(*a).self];
		}

		// Send SubMesh Bones to Material
		for (std::vector<RenderingMesh*>::iterator j = rcomp->GetMeshes().begin(); j != rcomp->GetMeshes().end(); j++)
		{
			(*j)->SkinningBones = std::vector<Matrix>((*j)->MapBoneIDs.size());
			for (std::map<int32, int32>::iterator k = (*j)->MapBoneIDs.begin(); k != (*j)->MapBoneIDs.end(); k++)
			{
				// Set list of Bones Matrices
				(*j)->SkinningBones[(*k).second] = Bones[(*k).first] * (*j)->BoneOffsetMatrix[(*k).first];
			}
		}

		// Owner
		Owner = owner;

		_paused = false;

		HaveLayers = false;

		// Set Default Affected Bones
		boneIDs.resize(skeleton.size());
		for (std::vector<Bone>::iterator i = skeleton.begin(); i != skeleton.end(); i++)
			boneIDs[(*i).self] = 1;
	}

	int32 SkeletonAnimationInstance::Play(const uint32 animation, const f32 startTime, const f32 repetition, const f32 speed, const f32 scale, const std::string &LayerName)
	{
		_SkeletonAnimation::SkeletonAnimation Anim;

		// Anim.animation below is a raw pointer *into* Owner->animations, so
		// an out-of-range id doesn't fail here - it silently stores a wild
		// pointer that only blows up later, in Update()'s `Animation Anim =
		// *(*_Anim).animation;`, with a backtrace pointing at the copy
		// constructor rather than at whoever asked for a clip that doesn't
		// exist. Reject it up front and say so.
		// Asking for a clip to play is unambiguous: un-pause the instance.
		// _paused was only ever cleared in the constructor, so anything that
		// paused a character - a death, a cutscene, parking it in a pool -
		// left it frozen for ever: a later Play()/playClip() added the clip,
		// advanced nothing, and the character stood still while its
		// neighbours walked. Resume() was the only way out and nobody knew.
		_paused = false;

		if (animation >= Owner->GetNumberAnimations())
		{
			echo("ERROR: SkeletonAnimationInstance::Play - animation id out of range (asked for " + std::to_string(animation) + ", only " + std::to_string(Owner->GetNumberAnimations()) + " loaded)");
			return -1;
		}

		if (GetAnimationPositionInVector(animation) == -1)
		{
			Anim.ID = animation;
			Anim.startTime = startTime; // 0-1
			Anim._startTime = startTime*Owner->animations[animation].Duration; // RealTime
			Anim.animation = &Owner->animations[animation];
			Anim.speed = speed;
			Anim.scale = scale;
			Anim._startTimeClock = -1.f;
			Anim._isPaused = false;
			Anim._resumed = false;
			Anim._pauseStart = -1.f;
			Anim._pauseTime = 0.f;
			Anim._repetition = repetition;
			Anim._currentTime = 0.f;
			Anim.boneTransformationPerAnimation = bindPose;

			// Layers
			if (LayerName.size() > 0)
			{
				HaveLayers = true;

				uint32 LayerID = MakeStringID(LayerName);
				Anim.HaveLayers = true;
				Anim.LayerID = LayerID;
				Anim.Layer = Layers[LayerID];

				// Mark Layer as being used
				Anim.Layer->usingLayer++;

				// Remove this Affected Bones From Other Animations
				for (std::vector<int32>::iterator i = Anim.Layer->boneIDs.begin(); i != Anim.Layer->boneIDs.end(); i++)
				{
					// Set to -1
					if ((*i) == 1)
						boneIDs[i - Anim.Layer->boneIDs.begin()] = -1;
				}

			}
			else {
				Anim.HaveLayers = false;
				Anim.Layer = NULL;
				Anim.LayerID = 0;
			}

			// Add Animation to Queue
			AnimationsToPlay.push_back(Anim);

			return AnimationsToPlay.size() - 1; // Return Order
		}

		// Already queued. This used to return -1 and change nothing, which
		// made Play() a no-op for the one case that matters most: a clip that
		// is already in the list but PAUSED.
		//
		// Pause() sets _isPaused on every entry as well as the instance's own
		// _paused, and Update() gates on the ENTRY. So clearing _paused above
		// was not enough - a character that was paused (a death, a cutscene,
		// parking it in a pool) and then played again kept a paused entry,
		// advanced nothing, and stood frozen for ever while playClip()
		// reported success. In a pooled crowd that is "some of them don't
		// animate", with the frozen ones being exactly those that had died
		// once and come back.
		//
		// Asking for a clip to play means play it: resume the entry and honour
		// the parameters this call passed, the same way a fresh one would.
		const int32 pos = GetAnimationPositionInVector(animation);
		if (pos >= 0)
		{
			_SkeletonAnimation::SkeletonAnimation &cur = AnimationsToPlay[pos];
			cur.startTime = startTime;
			cur._startTime = startTime * Owner->animations[animation].Duration;
			cur.speed = speed;
			cur.scale = scale;
			cur._repetition = repetition;
			// Restart the clock rather than resuming mid-stride: Update()
			// derives currentTime from (time - _startTimeClock), and a clock
			// left at the epoch it was paused at would jump the clip by
			// however long the character sat in the pool.
			cur._startTimeClock = -1.f;
			cur._currentTime = 0.f;
			cur._isPaused = false;
			cur._resumed = false;
			cur._pauseStart = -1.f;
			cur._pauseTime = 0.f;
			cur.fadeDuration = 0.f;
			cur.stopAfterFade = false;
			cur._prevValid = false;
		}
		return pos;
	}
	namespace {
		bool SameLayer(const _SkeletonAnimation::SkeletonAnimation &e, const std::string &layer)
		{
			if (layer.empty()) return !e.HaveLayers;
			return e.HaveLayers && e.Layer && e.Layer->Name == layer;
		}
	}

	int32 SkeletonAnimationInstance::CrossFade(const uint32 animation, const f32 duration,
		const f32 repetition, const f32 speed, const f32 startTime, const std::string &LayerName)
	{
		if (animation >= Owner->GetNumberAnimations())
		{
			echo("ERROR: SkeletonAnimationInstance::CrossFade - animation id out of range");
			return -1;
		}
		_paused = false;

		// Already the only clip on its layer, staying: just make sure it is
		// fully in and not on its way out.
		f32 start = startTime;
		const int32 existing = GetAnimationPositionInVector(animation);
		if (existing >= 0)
		{
			_SkeletonAnimation::SkeletonAnimation &cur = AnimationsToPlay[existing];
			bool alone = SameLayer(cur, LayerName);
			for (size_t i = 0; alone && i < AnimationsToPlay.size(); i++)
				if ((int32)i != existing && SameLayer(AnimationsToPlay[i], LayerName)) alone = false;
			if (alone)
			{
				cur.stopAfterFade = false;
				if (cur.scale != 0.f)
				{
					cur.fadeFrom = cur.scale; cur.fadeTo = 0.f; cur.fadeStart = -1.f;
					cur.fadeDuration = duration > 0.f ? duration : 0.f;
					if (duration <= 0.f) cur.scale = 0.f;
				}
				cur.speed = speed;
				return existing;
			}
			// Keep its phase: it goes to the bottom of the stack below (it has
			// to be the newest entry to sit under the ones fading out), and a
			// walk that restarted from frame 0 there would skip a step.
			if (start < 0.f && cur.animation && cur.animation->Duration > 0.f)
			{
				f32 p = cur._currentTime / cur.animation->Duration;
				start = p - std::floor(p);
			}
			StopAnimation((uint32)existing);
		}
		if (start < 0.f) start = 0.f;
		if (start > 1.f) start = 1.f;

		// Everything else on the layer fades away and is then stopped.
		bool others = false;
		for (size_t i = AnimationsToPlay.size(); i > 0; i--)
		{
			_SkeletonAnimation::SkeletonAnimation &e = AnimationsToPlay[i - 1];
			if (!SameLayer(e, LayerName)) continue;
			if (duration <= 0.f) { StopAnimation((uint32)(i - 1)); continue; }
			others = true;
			e.fadeFrom = e.scale; e.fadeTo = 1.f; e.fadeStart = -1.f;
			e.fadeDuration = duration;
			e.stopAfterFade = true;
		}

		// Newest = bottom of the blend (Update() walks the list in reverse and
		// each earlier clip blends over the result), so at scale 0 it is fully
		// in under clips that are fading to 1. With nothing to fade from, it
		// fades itself in from 1.
		const bool fadeIn = !others && duration > 0.f;
		const int32 order = Play(animation, start, repetition, speed, fadeIn ? 1.f : 0.f, LayerName);
		if (order >= 0 && fadeIn)
		{
			_SkeletonAnimation::SkeletonAnimation &e = AnimationsToPlay[order];
			e.fadeFrom = 1.f; e.fadeTo = 0.f; e.fadeStart = -1.f; e.fadeDuration = duration;
		}
		return order;
	}

	void SkeletonAnimationInstance::FadeOut(const uint32 animationOrder, const f32 duration)
	{
		if (animationOrder >= AnimationsToPlay.size()) return;
		if (duration <= 0.f) { StopAnimation(animationOrder); return; }
		_SkeletonAnimation::SkeletonAnimation &e = AnimationsToPlay[animationOrder];
		e.fadeFrom = e.scale; e.fadeTo = 1.f; e.fadeStart = -1.f;
		e.fadeDuration = duration;
		e.stopAfterFade = true;
	}

	bool SkeletonAnimationInstance::IsPlaying(const uint32 animation) const
	{
		for (size_t i = 0; i < AnimationsToPlay.size(); i++)
			if (AnimationsToPlay[i].ID == animation && !AnimationsToPlay[i].stopAfterFade) return true;
		return false;
	}

	void SkeletonAnimationInstance::AdvanceFades(const f32 time)
	{
		for (size_t i = AnimationsToPlay.size(); i > 0; i--)
		{
			_SkeletonAnimation::SkeletonAnimation &e = AnimationsToPlay[i - 1];
			if (e.fadeDuration <= 0.f) continue;
			if (e.fadeStart < 0.f) e.fadeStart = time;
			f32 u = (time - e.fadeStart) / e.fadeDuration;
			if (u < 0.f) u = 0.f;
			if (u >= 1.f)
			{
				e.scale = e.fadeTo;
				e.fadeDuration = 0.f;
				if (e.stopAfterFade) StopAnimation((uint32)(i - 1));
				continue;
			}
			// Smoothstep: a linear ramp starts and stops with a visible kink.
			u = u * u * (3.f - 2.f * u);
			e.scale = e.fadeFrom + (e.fadeTo - e.fadeFrom) * u;
		}
	}

	std::vector<SkeletonAnimationInstance::FiredEvent> SkeletonAnimationInstance::TakeEvents()
	{
		std::vector<FiredEvent> out;
		out.swap(eventQueue);
		return out;
	}

	Vec3 SkeletonAnimationInstance::ConsumeRootMotion()
	{
		const Vec3 d = rootMotionAccum;
		rootMotionAccum = Vec3(0.f, 0.f, 0.f);
		return d;
	}

	bool SkeletonAnimationInstance::GetTrackValue(const std::string &target, const std::string &property,
		TrackValue &out) const
	{
		std::map<std::string, TrackValue>::const_iterator it = trackValues.find(target + '\n' + property);
		if (it == trackValues.end()) return false;
		out = it->second;
		return true;
	}

	void SkeletonAnimationInstance::DeliverPending()
	{
		if (!pendingEvents.empty())
		{
			std::vector<FiredEvent> fired;
			fired.swap(pendingEvents);
			for (size_t i = 0; i < fired.size(); i++)
			{
				if (eventCallback) eventCallback(fired[i]);
				// Bounded: an instance nobody polls must not grow for ever.
				if (eventQueue.size() >= 256) eventQueue.erase(eventQueue.begin());
				eventQueue.push_back(fired[i]);
			}
		}

		if (rootMotionPending.x != 0.f || rootMotionPending.y != 0.f || rootMotionPending.z != 0.f)
		{
			const Vec3 d = rootMotionPending;
			rootMotionPending = Vec3(0.f, 0.f, 0.f);
			GameObject* go = (applyRootMotion && rcomp) ? rcomp->GetOwner() : NULL;
			if (go)
			{
				// Model -> world through the owner's rotation and scale (not
				// its translation: this is a displacement), then into the
				// space its position is written in.
				Matrix w = go->GetWorldTransformation();
				w.Translate(Vec3(0.f, 0.f, 0.f));
				Vec3 worldDelta = w * d;
				Vec3 localDelta = worldDelta;
				if (go->GetParent())
				{
					Matrix pw = go->GetParent()->GetWorldTransformation();
					pw.Translate(Vec3(0.f, 0.f, 0.f));
					localDelta = pw.Inverse() * worldDelta;
				}
				go->SetPosition(go->GetPosition() + localDelta);
			}
			else rootMotionAccum = rootMotionAccum + d;
		}
	}

	void SkeletonAnimationInstance::CollectEventsAndRootMotion(_SkeletonAnimation::SkeletonAnimation &e,
		const f32 local, const bool visible, const f32 weight)
	{
		const Animation &A = *e.animation;
		const f32 dur = A.Duration;
		const bool forward = e.speed >= 0.f;
		const bool first = !e._prevValid;
		// Unset on the first update: start just before the clip so an event
		// keyed at its very first frame fires.
		const f32 prev = first ? (forward ? -1e-6f : dur + 1e-6f) : e._prevLocalTime;
		// A loop wrapped when time ran backwards against the direction of play.
		const bool wrapped = !first && (forward ? local < prev - 1e-6f : local > prev + 1e-6f);

		// ---- events ----
		if (visible && !A.Events.empty())
		{
			for (size_t i = 0; i < A.Events.size(); i++)
			{
				const f32 t = A.Events[i].Time;
				bool hit;
				if (forward) hit = wrapped ? (t > prev || t <= local) : (t > prev && t <= local);
				else         hit = wrapped ? (t < prev || t >= local) : (t < prev && t >= local);
				if (!hit) continue;
				FiredEvent ev;
				ev.clip = A.AnimationName;
				ev.name = A.Events[i].Name;
				ev.time = t;
				// Bounded: whoever drives Update() without DeliverPending() (an
				// editor preview, an app updating the animation itself) must not
				// grow this for ever.
				if (pendingEvents.size() >= 256) pendingEvents.erase(pendingEvents.begin());
				pendingEvents.push_back(ev);
			}
		}

		// ---- root motion ----
		if (A.HasFlag(ANIM_FLAG_ROOT_MOTION))
		{
			if (e._rootBone == -2)
			{
				e._rootBone = -1;
				for (size_t b = 0; b < skeleton.size(); b++)
				{
					const bool match = A.RootMotionBone.empty() ? skeleton[b].parent < 0
						: skeleton[b].name == A.RootMotionBone;
					if (match) { e._rootBone = skeleton[b].self; break; }
				}
				for (size_t c = 0; e._rootBone >= 0 && c < A.Channels.size(); c++)
					if (A.Channels[c].NodeName == skeleton[e._rootBone].name) { e._rootChannel = (int32)c; break; }
			}
			if (e._rootBone >= 0 && e._rootChannel >= 0)
			{
				const int32 rb = e._rootBone;
				const Channel &ch = A.Channels[e._rootChannel];
				const Matrix* bind = ((size_t)rb < bindPose.size()) ? &bindPose[rb] : NULL;
				// The parent's bind frame turns the bone's local translation
				// into model space, where "horizontal" means something. Its
				// rotation only: the parent of a root is an armature node that
				// does not move.
				Matrix parentRot;
				const int32 parent = skeleton[rb].parent;
				if (parent >= 0)
				{
					parentRot = GetParentMatrix(parent, bindPose);
					parentRot.Translate(Vec3(0.f, 0.f, 0.f));
				}
				const Matrix parentInv = parentRot.Inverse();

				Matrix &pose = e.boneTransformationPerAnimation[rb];
				const Vec3 now = pose.GetTranslation();
				if (!first)
				{
					Vec3 d;
					if (wrapped)
					{
						const Vec3 endP = SampleChannel(ch, forward ? dur : 0.f, false, bind).GetTranslation();
						const Vec3 startP = SampleChannel(ch, forward ? 0.f : dur, false, bind).GetTranslation();
						d = (endP - e._prevRootPos) + (now - startP);
					}
					else d = now - e._prevRootPos;
					Vec3 dm = parentRot * d;
					dm.y = 0.f;
					rootMotionPending = rootMotionPending + dm * weight;
				}
				e._prevRootPos = now;

				// Out of the pose: the root keeps its clip-start position on the
				// ground plane, so the mesh walks in place and the object moves.
				const Vec3 origin = parentRot * SampleChannel(ch, forward ? 0.f : dur, false, bind).GetTranslation();
				Vec3 m = parentRot * now;
				m.x = origin.x;
				m.z = origin.z;
				pose.Translate(parentInv * m);
			}
		}

		e._prevLocalTime = local;
		e._prevValid = true;
	}

	namespace {
		// A PropertyTrack at `time`: numbers eased between keys, text held.
		void SampleTrack(const PropertyTrack &track, const f32 time, Vec4 &value, std::string &text)
		{
			const std::vector<PropertyKey> &k = track.Keys;
			size_t i = 0;
			while (i + 1 < k.size() && k[i + 1].Time <= time) i++;
			value = k[i].Value;
			text = k[i].Text;
			if (i + 1 < k.size() && time > k[i].Time)
			{
				const f32 span = k[i + 1].Time - k[i].Time;
				f32 u = span > 0.f ? (time - k[i].Time) / span : 0.f;
				u = Ease(u, k[i].Mode, 1.f, 1.f);
				const Vec4 &b = k[i + 1].Value;
				value = Vec4(value.x + (b.x - value.x) * u, value.y + (b.y - value.y) * u,
					value.z + (b.z - value.z) * u, value.w + (b.w - value.w) * u);
			}
		}
	}

	void SkeletonAnimationInstance::SampleTracks()
	{
		trackValues.clear();
		// Same order and meaning of `scale` as the bone blend: walk the list
		// backwards, each clip blending over what is below it; a picture is
		// taken from a clip that is at least half in.
		for (size_t i = AnimationsToPlay.size(); i > 0; i--)
		{
			const _SkeletonAnimation::SkeletonAnimation &e = AnimationsToPlay[i - 1];
			if (!e.animation || e.animation->Tracks.empty()) continue;
			const f32 s = AnimationsToPlay.size() > 1 ? e.scale : 0.f;
			for (size_t t = 0; t < e.animation->Tracks.size(); t++)
			{
				const PropertyTrack &track = e.animation->Tracks[t];
				if (track.Keys.empty()) continue;
				Vec4 v; std::string text;
				SampleTrack(track, e._currentTime, v, text);
				TrackValue &out = trackValues[track.Target + '\n' + track.Property];
				const bool anyText = !text.empty();
				if (!out.hasValue) { out.value = v; out.hasValue = true; }
				else out.value = Vec4(v.x + (out.value.x - v.x) * s, v.y + (out.value.y - v.y) * s,
					v.z + (out.value.z - v.z) * s, v.w + (out.value.w - v.w) * s);
				if (anyText && (!out.hasText || s <= 0.5f)) { out.text = text; out.hasText = true; }
			}
		}
	}

	void SkeletonAnimationInstance::ChangeProperties(const uint32 animationOrder, const f32 startTime, const f32 repetition, const f32 speed, const f32 scale)
	{
		if (AnimationsToPlay.size() > animationOrder)
		{
			_SkeletonAnimation::SkeletonAnimation *Anim = &AnimationsToPlay[animationOrder];
			Anim->startTime = startTime; // 0-1
			Anim->_startTime = startTime*Owner->animations[Anim->ID].Duration; // RealTime
			Anim->speed = speed;
			Anim->scale = scale;
			if (!Anim->_isPaused)
			{
				Anim->_startTimeClock = -1.f;
				Anim->_isPaused = false;
				Anim->_resumed = false;
				Anim->_pauseStart = -1.f;
				Anim->_pauseTime = 0.f;
			}
			Anim->_repetition = repetition;
		}
		else echo("ERROR: Animation Not Found");
	}
	void SkeletonAnimationInstance::StopAnimation(const uint32 animationOrder)
	{
		if (animationOrder >= AnimationsToPlay.size()) return;

		// Play() leaves Layer NULL for a clip started without a layer name,
		// which is the common case - dereferencing it unconditionally here
		// crashed on stopping any ordinary clip.
		_SkeletonAnimation::AnimationLayer* Layer = AnimationsToPlay[animationOrder].Layer;
		if (Layer)
		{
			// Mark Layer as NOT being used by this
			Layer->usingLayer--;

			// Insert Removed Bones if there isn't any Layer
			if (Layer->usingLayer == 0)
			{
				for (std::vector<int32>::iterator i = Layer->boneIDs.begin(); i != Layer->boneIDs.end(); i++)
					if ((*i) == 1)
						boneIDs[i - Layer->boneIDs.begin()] = 1;
			}
		}

		// Remove Layer if Any
		AnimationsToPlay.erase(AnimationsToPlay.begin() + animationOrder);

		// Same reasoning as Stop(): Update() skips an instance with nothing
		// left playing, so the last clip's final pose would stick otherwise.
		if (AnimationsToPlay.empty())
			ResetToBindPose();
	}

	void SkeletonAnimationInstance::Stop()
	{
		// One at a time, through StopAnimation(): a clip playing on a layer
		// holds a count on it (usingLayer), and clearing the list outright
		// left that count standing. The next clip played on the same layer
		// then looked like a SECOND user of it, took the blend-between-users
		// path with nothing to blend against, and the layer's bones sat in
		// the bind pose - a character whose arms snapped out to a T the
		// moment it changed what its upper body was doing.
		while (!AnimationsToPlay.empty())
			StopAnimation((uint32)AnimationsToPlay.size() - 1);
		// Update() no longer touches an instance with nothing playing, so
		// without this the rig would freeze in whatever pose the last clip
		// left it in. "Stopped" should read as neutral, and neutral is the
		// bind pose - the same thing a freshly constructed instance shows.
		ResetToBindPose();
	}

	void SkeletonAnimationInstance::SetAnimationScale(const uint32 animationOrder, const f32 scale)
	{
		if (animationOrder < AnimationsToPlay.size())
			AnimationsToPlay[animationOrder].scale = scale;
	}

	void SkeletonAnimationInstance::SetAnimationSpeed(const uint32 animationOrder, const f32 speed)
	{
		if (animationOrder >= AnimationsToPlay.size() || speed == 0.f) return;
		_SkeletonAnimation::SkeletonAnimation &anim = AnimationsToPlay[animationOrder];
		if (anim.speed == speed) return;
		// Carry on from where the clip is. Its time is
		//     (elapsed + _startTime / speed) * speed
		// so changing the speed alone would jump it. Move the start so that
		// the same elapsed time gives the same clip time at the new speed.
		// The clock is left running: starting it again from the frame on
		// show costs the clip a frame each time, and a walk whose pace is
		// trimmed every few frames to follow the feet stutters visibly.
		if (anim._startTimeClock != -1.f)
		{
			const f32 elapsed = (anim._currentTime - anim._startTime) / anim.speed;
			anim._startTime = anim._currentTime - elapsed * speed;
		}
		anim.speed = speed;
	}

	void SkeletonAnimationInstance::PauseAnimation(const uint32 animationOrder)
	{
		if (!AnimationsToPlay[animationOrder]._isPaused)
			AnimationsToPlay[animationOrder]._isPaused = true;
	}

	void SkeletonAnimationInstance::Pause()
	{
		if (!_paused)
		{
			for (std::vector<_SkeletonAnimation::SkeletonAnimation>::iterator _Anim = AnimationsToPlay.begin(); _Anim != AnimationsToPlay.end(); _Anim++)
			{
				if (!(*_Anim)._isPaused)
					(*_Anim)._isPaused = true;
			}
			_paused = true;
		}
	}

	void SkeletonAnimationInstance::ResumeAnimation(const uint32 animationOrder)
	{
		if (AnimationsToPlay[animationOrder]._isPaused)
		{
			AnimationsToPlay[animationOrder]._isPaused = false;
			AnimationsToPlay[animationOrder]._resumed = true;
		}
	}

	void SkeletonAnimationInstance::Resume()
	{
		if (_paused)
		{
			for (std::vector<_SkeletonAnimation::SkeletonAnimation>::iterator _Anim = AnimationsToPlay.begin(); _Anim != AnimationsToPlay.end(); _Anim++)
			{
				if ((*_Anim)._isPaused)
				{
					(*_Anim)._isPaused = false;
					(*_Anim)._resumed = true;
				}
			}
			_paused = false;
		}
	}

	bool SkeletonAnimationInstance::IsPaused(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder]._isPaused;
	}

	bool SkeletonAnimationInstance::IsPaused()
	{
		return _paused;
	}

	f32 SkeletonAnimationInstance::GetAnimationCurrentTime(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder]._currentTime;
	}
	f32 SkeletonAnimationInstance::GetAnimationCurrentProgress(const uint32 animationOrder)
	{
		if (AnimationsToPlay[animationOrder]._currentTime > 0)
			return AnimationsToPlay[animationOrder]._currentTime / GetAnimationDuration(animationOrder);
		else return 0;
	}
	f32 SkeletonAnimationInstance::GetAnimationDuration(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder].animation->Duration;
	}
	f32 SkeletonAnimationInstance::GetAnimationSpeed(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder].speed;
	}
	f32 SkeletonAnimationInstance::GetAnimationStartTimeProgress(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder].startTime;
	}
	f32 SkeletonAnimationInstance::GetAnimationStartTime(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder]._startTime;
	}
	uint32 SkeletonAnimationInstance::GetAnimationID(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder].ID;
	}
	f32 SkeletonAnimationInstance::GetAnimationScale(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder].scale;
	}

	std::string SkeletonAnimationInstance::GetLayerName(const uint32 index) const
	{
		std::map<uint32, _SkeletonAnimation::AnimationLayer*>::const_iterator it = Layers.begin();
		std::advance(it, index);
		return it != Layers.end() ? it->second->Name : std::string();
	}
	std::vector<int32> SkeletonAnimationInstance::GetLayerAffectedBoneIDs(const uint32 index) const
	{
		std::vector<int32> result;
		std::map<uint32, _SkeletonAnimation::AnimationLayer*>::const_iterator it = Layers.begin();
		std::advance(it, index);
		if (it == Layers.end()) return result;
		const std::vector<int32> &boneIDs = it->second->boneIDs;
		for (size_t i = 0; i < boneIDs.size(); i++)
			if (boneIDs[i] == 1) result.push_back((int32)i);
		return result;
	}

	int32 SkeletonAnimationInstance::GetAnimationPositionInVector(const uint32 animation)
	{
		for (uint32 i = 0; i < AnimationsToPlay.size(); i++)
		{
			if (AnimationsToPlay[i].ID == animation)
				return i;
		}
		return -1; // Not Found
	}

	uint32 SkeletonAnimationInstance::CreateLayer(const std::string &name)
	{
		uint32 id = MakeStringID(name);
		if (Layers.find(id) == Layers.end())
		{
			Layers[id] = new _SkeletonAnimation::AnimationLayer(name);
			Layers[id]->boneIDs.resize(skeleton.size());
		}
		return id;
	}

	void SkeletonAnimationInstance::AddBone(const uint32 LayerID, const std::string &bone)
	{
		for (std::vector<Bone>::iterator i = skeleton.begin(); i != skeleton.end(); i++)
		{
			if ((*i).name.compare(bone) == 0)
			{
				Layers[LayerID]->boneIDs[(*i).self] = 1;
				break;
			}
		}
	}
	void SkeletonAnimationInstance::AddBone(const std::string &LayerName, const std::string &bone)
	{
		AddBone(MakeStringID(LayerName), bone);
	}
	void SkeletonAnimationInstance::AddBoneAndChilds(const uint32 LayerID, const std::string &bone, bool inclusive)
	{
		for (std::vector<Bone>::iterator i = skeleton.begin(); i != skeleton.end(); i++)
		{
			if ((*i).name.compare(bone) == 0)
			{
				if (inclusive)
					Layers[LayerID]->boneIDs[(*i).self] = 1;
				GetBoneChilds(Layers[LayerID]->boneIDs, skeleton, (*i).self, true);
				break;
			}
		}
	}
	void SkeletonAnimationInstance::AddBoneAndChilds(const std::string &LayerName, const std::string &bone, bool inclusive)
	{
		AddBoneAndChilds(MakeStringID(LayerName), bone, inclusive);
	}
	void SkeletonAnimationInstance::RemoveBone(const uint32 LayerID, const std::string &bone)
	{
		for (std::vector<Bone>::iterator i = skeleton.begin(); i != skeleton.end(); i++)
		{
			if ((*i).name.compare(bone) == 0)
			{
				Layers[LayerID]->boneIDs[(*i).self] = -1;
				break;
			}
		}
	}
	void SkeletonAnimationInstance::RemoveBone(const std::string &LayerName, const std::string &bone)
	{
		RemoveBone(MakeStringID(LayerName), bone);
	}
	void SkeletonAnimationInstance::RemoveBoneAndChilds(const uint32 LayerID, const std::string &bone)
	{
		GetBoneChilds(Layers[LayerID]->boneIDs, skeleton, skeleton[MakeStringID(bone)].self, false);
	}
	void SkeletonAnimationInstance::RemoveBoneAndChilds(const std::string &LayerName, const std::string &bone)
	{
		RemoveBoneAndChilds(MakeStringID(LayerName), bone);
	}
	void SkeletonAnimationInstance::DestroyLayer(const uint32 LayerID)
	{
		// Delete Layer
		delete Layers[LayerID];
	}
	void SkeletonAnimationInstance::DestroyLayer(const std::string &LayerName)
	{
		DestroyLayer(MakeStringID(LayerName));
	}

	void SkeletonAnimationInstance::GetBoneChilds(std::vector<int32> &boneIDs, const std::vector<Bone> &Skeleton, const uint32 id, bool add)
	{
		for (std::vector<Bone>::iterator i = skeleton.begin(); i != skeleton.end(); i++)
		{
			if ((*i).parent == id)
			{
				if (add)
					boneIDs[(*i).self] = 1;
				else if (!add)
					boneIDs[(*i).self] = 1;

				GetBoneChilds(boneIDs, Skeleton, (*i).self);
			}
		}
	}

	void SkeletonAnimation::LoadAnimation(const std::string& AnimationFile)
	{
		// Not stored before this - the path was used once to load, then
		// discarded, same "no recoverable source" gap Model/Texture/Font
		// had before their own path tracking was added.
		Path = AnimationFile;
		// See GetPaths() - Path alone loses every file but the last.
		Paths.push_back(AnimationFile);

		// Loads Animation
		animationLoader = new AnimationLoader();
		animationLoader->Load(AnimationFile);

		// Copy Animations
		for (std::vector<Animation>::iterator i = animationLoader->animations.begin(); i != animationLoader->animations.end(); i++)
		{
			animations.push_back((*i));
		}

		// Deletes Animation Loader
		delete animationLoader;
	}

	const uint32 SkeletonAnimation::GetNumberAnimations() const
	{
		// Returns number of loaded animations
		return animations.size();
	}

	const int32 SkeletonAnimation::GetAnimationIDByName(const std::string &name) const
	{
		for (uint32 i = 0; i < animations.size(); i++)
		{
			if (animations[i].AnimationName.compare(name) == 0) return i;
		}
		return -1;
	}

	bool SkeletonAnimation::AddEvent(const uint32 animation, const f32 time, const std::string &name)
	{
		if (animation >= animations.size()) return false;
		std::vector<AnimationEvent> &evs = animations[animation].Events;
		for (size_t i = 0; i < evs.size(); i++) if (evs[i].Name == name && std::fabs(evs[i].Time - time) < 1e-5f) return false;
		size_t at = 0;
		while (at < evs.size() && evs[at].Time <= time) at++;
		evs.insert(evs.begin() + at, AnimationEvent(time, name));
		return true;
	}

	const int32 SkeletonAnimation::GetAnimationIDByGuid(const std::string &guid) const
	{
		// An empty guid is "no identity recorded", not a clip to match - v0
		// clips all carry one, so matching on it would return clip 0 for
		// every lookup.
		if (guid.empty()) return -1;
		for (uint32 i = 0; i < animations.size(); i++)
		{
			if (animations[i].Guid.compare(guid) == 0) return i;
		}
		return -1;
	}

	const std::string &SkeletonAnimation::GetAnimationGuid(const uint32 id) const
	{
		static const std::string empty;
		return (id < animations.size() ? animations[id].Guid : empty);
	}

	const std::string &SkeletonAnimation::GetAnimationName(const uint32 id) const
	{
		static const std::string empty;
		return (id < animations.size() ? animations[id].AnimationName : empty);
	}

	const int32 SkeletonAnimation::ResolveAnimationID(const std::string &guid, const std::string &name, const int32 fallbackIndex) const
	{
		const int32 byGuid = GetAnimationIDByGuid(guid);
		if (byGuid >= 0) return byGuid;

		if (!name.empty())
		{
			const int32 byName = GetAnimationIDByName(name);
			if (byName >= 0) return byName;
		}

		if (fallbackIndex >= 0 && (uint32)fallbackIndex < animations.size())
			return fallbackIndex;

		return -1;
	}

	SkeletonAnimationInstance* SkeletonAnimation::CreateInstance(RenderingComponent* Component)
	{
		SkeletonAnimationInstance* i = new SkeletonAnimationInstance(this, Component);
		Instances.push_back(i);

		return i;
	}

	// Destroy Instance
	void SkeletonAnimation::DestroyInstance(SkeletonAnimationInstance* Instance)
	{
		for (std::vector<SkeletonAnimationInstance*>::iterator i = Instances.begin(); i != Instances.end(); i++)
		{
			if ((*i) == Instance)
			{
				delete Instance;
				Instances.erase(i);
				break;
			}
		}
	}

	void SkeletonAnimation::Update(const f32 time)
	{
		for (std::vector<SkeletonAnimationInstance*>::iterator i = Instances.begin(); i != Instances.end(); i++)
			UpdateInstance(*i, time);
	}

	void SkeletonAnimation::UpdateInstance(SkeletonAnimationInstance* inst, const f32 time)
	{
		// (held as it is: see SetPoseHeld)
		if (inst->poseHeld) return;
		// Nothing playing and nothing to modify: leave the pose alone.
		//
		// The "Multiply Bones" loop below derives each bone's local
		// transform from the playing list, so with an empty list it fell
		// through to `trafo = Matrix()` and rewrote every bone to IDENTITY
		// on every single frame. That did two bad things: it collapsed a
		// skinned mesh that simply had no clip playing (identity local
		// instead of the bind local), and it stomped any pose written from
		// outside - SetBoneLocalTransform/ApplyPose - the very next tick,
		// which is why an IK solver or the editor's posing could never
		// hold on an instance owned by a running scene.
		//
		// Pose modifiers still run with nothing playing, though: a
		// character standing on uneven ground wants its feet planted
		// whether or not a clip happens to be driving it.
		// Fades first: a crossfade that finishes this frame stops its outgoing
		// clips here, before they are sampled.
		if (!inst->AnimationsToPlay.empty()) inst->AdvanceFades(time);

		if (inst->AnimationsToPlay.empty())
		{
			// Track values are left alone too, like the pose: with nothing
			// playing they are whatever was last put there - the editor's scrub
			// (ApplyAnimationAtTime) sets them and the preview's scene update
			// runs straight after it. Stop() clears them with the pose.
			if (!inst->poseModifiers.empty())
			{
				inst->RunPoseModifiers();
				inst->RefreshSkinning();
			}
			return;
		}

		const bool soloClip = inst->AnimationsToPlay.size() == 1;
		// How much of each clip shows: the pose is mixed from the end of the list
		// to the front, each clip taking (1 - scale) of what is under it, so a
		// clip's share is (1 - its scale) times the scales of the clips before it
		// (over it) - per layer, as they are mixed. "1 - scale" alone called the
		// bottom clip of a blend (always scale 0) fully on at any weight: a
		// walk/jog/run blend at walking pace played the run's footsteps too.
		f32 coverBase = 1.f;
		std::map<const void*, f32> coverLayer;
		for (std::vector<_SkeletonAnimation::SkeletonAnimation>::iterator _Anim = inst->AnimationsToPlay.begin(); _Anim != inst->AnimationsToPlay.end(); _Anim++)
		{
			f32 shown = 1.f;
			{
				f32* cover = &coverBase;
				if ((*_Anim).HaveLayers)
				{
					std::map<const void*, f32>::iterator c = coverLayer.find((const void*)(*_Anim).Layer);
					if (c == coverLayer.end()) c = coverLayer.insert(std::make_pair((const void*)(*_Anim).Layer, 1.f)).first;
					cover = &c->second;
				}
				const f32 sc = std::max(0.f, std::min(1.f, (*_Anim).scale));
				shown = (1.f - sc) * (*cover);
				*cover *= sc;
			}
			if ((*_Anim)._isPaused)
			{
				if ((*_Anim)._pauseStart == -1)
					(*_Anim)._pauseStart = time;
			}
			else {

				if ((*_Anim)._resumed)
				{
					(*_Anim)._resumed = false;
					(*_Anim)._pauseTime += time - (*_Anim)._pauseStart;
					(*_Anim)._pauseStart = -1;
				}

				// A reference: this used to copy the whole clip - every channel and
				// keyframe - per instance per frame, twice the cost of the animation
				// itself in a crowd.
				const Animation &Anim = *(*_Anim).animation;

				if ((*_Anim)._startTimeClock == -1.f)
				{
					(*_Anim)._startTimeClock = time;
					if ((*_Anim).speed < 0 && (*_Anim)._startTime == 0)
						(*_Anim)._startTime = (*_Anim).animation->Duration;
				}

				// Calculate Current Time
				f32 currentTime = time - (*_Anim)._startTimeClock - (*_Anim)._pauseTime + (*_Anim)._startTime / (*_Anim).speed;

				// Check if Ended
				if (
					(currentTime*(*_Anim).speed>0 && currentTime*(*_Anim).speed > (*_Anim).animation->Duration)
					||
					(currentTime*(*_Anim).speed < 0 && currentTime*(*_Anim).speed < 0)
					) // Ended
				{
					if (currentTime*(*_Anim).speed > 0)
					{
						if ((*_Anim)._repetition == -1)
						{
							(*_Anim).startTime = 0;
							(*_Anim)._startTimeClock = time;
							(*_Anim)._startTime = 0;
							(*_Anim)._pauseTime = 0;
						}
						else if ((*_Anim)._repetition > 0) {
							(*_Anim)._repetition--;
							if ((*_Anim)._repetition > 0)
							{
								(*_Anim).startTime = 0;
								(*_Anim)._startTimeClock = time;
								(*_Anim)._startTime = 0;
								(*_Anim)._pauseTime = 0;
							}
						}
					}
					else if (currentTime*(*_Anim).speed < 0)
					{
						if ((*_Anim)._repetition == -1)
						{
							(*_Anim).startTime = 1;
							(*_Anim)._startTimeClock = time;
							(*_Anim)._startTime = (*_Anim).animation->Duration;
							(*_Anim)._pauseTime = 0;
						}
						else if ((*_Anim)._repetition > 0) {
							(*_Anim)._repetition--;
							if ((*_Anim)._repetition > 0)
							{
								(*_Anim).startTime = 1;
								(*_Anim)._startTimeClock = time;
								(*_Anim)._startTime = (*_Anim).animation->Duration;
								(*_Anim)._pauseTime = 0;
							}
						}
					}
				}

				// Save Current Time to Animation Info
				(*_Anim)._currentTime = currentTime *= (*_Anim).speed;

				// Transform bones from animation
				for (uint32 a = 0; a<Anim.Channels.size(); a++)
				{
					// Interpolation moved to SampleChannel() so the
					// editor can evaluate a clip at an arbitrary time
					// (scrubbing a timeline) through the exact same code
					// the runtime plays it with.
					// The bone this channel drives has to be resolved
					// BEFORE sampling now, because its bind pose is what
					// supplies any component the channel does not key.
					// Resolved once per PLAYING CLIP, not per instance: the old
					// cache was keyed by channel index and shared by every clip,
					// so a second clip whose channels came in another order (a
					// hand-keyed aim over an imported walk) wrote its channel 0
					// to whichever bone the first clip's channel 0 was - and a
					// clip with more channels than the rig has bones read past
					// the end of it. -1 = not a bone of this skeleton, found
					// once rather than searched for every frame.
					std::vector<int32> &map = (*_Anim).channelBones;
					if (map.size() != Anim.Channels.size())
					{
						map.assign(Anim.Channels.size(), -1);
						for (size_t c = 0; c < Anim.Channels.size(); c++)
							for (std::vector<Bone>::iterator b = inst->skeleton.begin(); b != inst->skeleton.end(); b++)
								if ((*b).name.compare(Anim.Channels[c].NodeName) == 0) { map[c] = (*b).self; break; }
					}

					// A channel naming a node that is not a bone of this
					// skeleton stays -1, and indexing the pose vector with
					// that is an out-of-bounds *write*. Assimp-exported clips
					// routinely carry channels for armature/helper nodes, and
					// the Animation Editor makes it easy to point a clip at a
					// mesh it wasn't authored for, so this has to be
					// survivable rather than merely unlikely.
					if (map[a] >= 0)
					{
						const int32 bid = map[a];
						const Matrix* bind = ((size_t)bid < inst->bindPose.size())
							? &inst->bindPose[bid] : NULL;
						(*_Anim).boneTransformationPerAnimation[bid] =
							SkeletonAnimationInstance::SampleChannel(Anim.Channels[a], currentTime,
								Anim.HasFlag(ANIM_FLAG_APPLY_SCALE), bind);
					}
				}

				// Events passed and root travel, now that this clip's pose for
				// the frame exists. A clip showing less than half (faded, blended
				// mostly out, or covered by the clips over it) stays quiet, so a
				// crossfade or a blend does not play both clips' footsteps; its
				// share of the root travel is what of it shows.
				const f32 vis = soloClip ? 1.f : shown;
				inst->CollectEventsAndRootMotion(*_Anim, currentTime, vis >= 0.5f, vis);
			}
		}

		// Multiply Bones
		//
		// Two passes. Clips with no layer pose the WHOLE skeleton and blend
		// with each other; a clip on a layer then takes that layer's bones
		// over, as far as its scale lets it (0: entirely, 1: not at all).
		//
		// It used to be one pass in which a bone on a layer in use was
		// skipped by every unlayered clip, and a lone layered clip was
		// written straight in. That left nothing underneath a layer to blend
		// from, so a layered clip could only ever cut in and cut out: a
		// character raising a rifle went from a walk to the aim in one
		// frame. With a base under it, the same clip can be faded in by
		// running its scale from 1 down to 0, and out again the other way.
		// At scale 0 - what playClip() and every existing caller passes -
		// the result is what it always was.
		const bool blending = inst->AnimationsToPlay.size() > 1;
		for (std::vector<Bone>::iterator a = inst->skeleton.begin(); a != inst->skeleton.end(); a++)
		{
			const int32 id = (*a).self;
			Matrix trafo = (blending ? inst->bindPose[id] : Matrix());
			if (!blending)
			{
				// Normal Playback of One Animation
				trafo = inst->AnimationsToPlay[0].boneTransformationPerAnimation[id];
			}
			else
			{
				for (std::vector<_SkeletonAnimation::SkeletonAnimation>::reverse_iterator b = inst->AnimationsToPlay.rbegin(); b != inst->AnimationsToPlay.rend(); b++)
				{
					if (!(*b).HaveLayers)
						trafo = SCALE((*b).boneTransformationPerAnimation[id], trafo, (*b).scale);
				}
				for (std::vector<_SkeletonAnimation::SkeletonAnimation>::reverse_iterator b = inst->AnimationsToPlay.rbegin(); b != inst->AnimationsToPlay.rend(); b++)
				{
					if ((*b).HaveLayers && (*b).Layer->boneIDs[id] == 1)
						trafo = SCALE((*b).boneTransformationPerAnimation[id], trafo, (*b).scale);
				}
			}
			// Apply Final Transformation to Bones
			inst->boneTransformation[id] = trafo;
		}

		// Multiply bones with its parent - Tree
		for (std::vector<Bone>::iterator a = inst->skeleton.begin(); a != inst->skeleton.end(); a++)
		{
			inst->Bones[(*a).self] = inst->GetParentMatrix((*a).parent, inst->boneTransformation) * inst->boneTransformation[(*a).self];
		}

		// Runtime IK and friends run HERE - after the clips have written
		// the pose and the hierarchy is composed, before the result is
		// uploaded. See AddPoseModifier: doing this from a component tick
		// instead would be correct only by accident of update order.
		inst->ApplyBoneAims();
		inst->ApplyFootGround();
		inst->RunPoseModifiers();

		inst->SampleTracks();

		// Send SubMesh Bones to Material
		inst->UploadSkinning();
	}

	void SkeletonAnimation::SetAnimations(const std::vector<Animation> &clips)
	{
		// Stop first: every playing entry holds a raw Animation* into the
		// vector about to be replaced.
		for (std::vector<SkeletonAnimationInstance*>::iterator i = Instances.begin(); i != Instances.end(); i++)
			(*i)->Stop();

		animations = clips;
		Path.clear();
		Paths.clear();
	}

	const std::vector<Animation> SkeletonAnimation::GetAnimations() const
	{
		return animations;
	}

	SkeletonAnimation::~SkeletonAnimation()
	{
		for (std::vector<SkeletonAnimationInstance*>::iterator i = Instances.begin(); i != Instances.end(); i++)
		{
			delete (*i);
		}
	}

	// Get Parent Matrix
	Matrix SkeletonAnimationInstance::GetParentMatrix(const int32 id, const std::vector<Matrix> &bones)
	{
		if (id != -1)
		{
			if (skeleton[id].parent > -1)
			{
				return GetParentMatrix(skeleton[id].parent, bones) * bones[id];
			}
			return bones[id];
		}
		else return Matrix();
	}

	Matrix SkeletonAnimationInstance::GetBoneMatrix(const int32 id)
	{
		return Bones[skeleton[id].self];
	}


	// ---- Direct pose access ------------------------------------------------

	namespace {
		// A local bone matrix taken apart as translation * rotation * scale.
		//
		// Matrix::GetScale() returns the DIAGONAL and Matrix::Scale() only
		// multiplies the diagonal; both are right only for a matrix with no
		// rotation in it, and a bone almost always has some. A column's
		// LENGTH is its scale however the frame is turned, and the rotation
		// has to be read from the columns with that scale divided out, or a
		// scaled bind pose feeds ConvertToQuaternion a non-orthonormal matrix.
		void DecomposeTRS(const Matrix &in, Vec3 &t, Quaternion &r, Vec3 &s)
		{
			t = in.GetTranslation();
			s = Vec3(std::sqrt(in.m[0] * in.m[0] + in.m[1] * in.m[1] + in.m[2] * in.m[2]),
				std::sqrt(in.m[4] * in.m[4] + in.m[5] * in.m[5] + in.m[6] * in.m[6]),
				std::sqrt(in.m[8] * in.m[8] + in.m[9] * in.m[9] + in.m[10] * in.m[10]));
			Matrix rot = in;
			const f32 len[3] = { s.x, s.y, s.z };
			for (int c = 0; c < 3; c++)
			{
				const f32 inv = len[c] > 1e-8f ? 1.f / len[c] : 0.f;
				for (int k = 0; k < 3; k++) rot.m[c * 4 + k] *= inv;
			}
			rot.m[12] = rot.m[13] = rot.m[14] = 0.f;
			r = rot.ConvertToQuaternion();
		}

		// translation * rotation * scale. `applyScale` false leaves a pure
		// rigid transform, which is what every clip without
		// ANIM_FLAG_APPLY_SCALE has always produced.
		Matrix ComposeTRS(const Vec3 &t, const Quaternion &r, const Vec3 &s, const bool applyScale)
		{
			Matrix out = r.ConvertToMatrix();
			if (applyScale)
			{
				const f32 sc[3] = { s.x, s.y, s.z };
				for (int c = 0; c < 3; c++)
					for (int k = 0; k < 3; k++) out.m[c * 4 + k] *= sc[c];
			}
			out.Translate(t);
			return out;
		}

		// Index of the last key at or before `time`, for a key vector already
		// sorted by time. Linear scan, matching what this did inline before -
		// channels carry tens of keys, not thousands.
		template<typename T>
		size_t KeyIndexAt(const std::vector<T> &keys, const f32 time)
		{
			size_t index = 0;
			while (index + 1 < keys.size() && keys[index + 1].Time <= time)
				index++;
			return index;
		}

		// Normalised, mode-remapped position within the span starting at
		// `index`. Zero when `index` is the last key.
		template<typename T>
		f32 SpanDelta(const std::vector<T> &keys, const size_t index, const f32 time)
		{
			if (index + 1 >= keys.size()) return 0.f;
			const f32 span = keys[index + 1].Time - keys[index].Time;
			// Two keys at the same time is a divide by zero (NaN value, and
			// from there a NaN bone matrix that silently collapses the whole
			// mesh) - reachable in the editor by dropping a key onto an
			// existing one.
			const f32 t = (span > 0.f ? (time - keys[index].Time) / span : 0.f);
			return Ease(t, keys[index].Mode, keys[index].OutTangent, keys[index + 1].InTangent);
		}
	}

	Matrix SkeletonAnimationInstance::SampleChannel(const Channel &ch, const f32 time, const bool applyScale,
		const Matrix* bindLocal)
	{
		Vec3 curPosition, curScale(1.f, 1.f, 1.f);
		Quaternion curRotation;

		// Anything the channel does not key comes from the bind pose, not from
		// identity - see the header. Read up front so each block below only
		// has to overwrite what it actually has keys for.
		if (bindLocal)
			DecomposeTRS(*bindLocal, curPosition, curRotation, curScale);

		// Empty key lists were never possible for clips coming out of the
		// Assimp importer (it writes a key per channel per component), but
		// an editor-authored clip only carries the components the user
		// actually keyed, so each of the three blocks below has to cope
		// with having nothing to sample.
		if (!ch.positions.empty())
		{
			const size_t posIndex = KeyIndexAt(ch.positions, time);
			curPosition = ch.positions[posIndex].Pos;
			if (posIndex + 1 < ch.positions.size())
				curPosition = curPosition.Lerp(ch.positions[posIndex + 1].Pos, SpanDelta(ch.positions, posIndex, time));
		}

		if (!ch.rotations.empty())
		{
			const size_t rotIndex = KeyIndexAt(ch.rotations, time);
			curRotation = ch.rotations[rotIndex].Rot;
			if (rotIndex + 1 < ch.rotations.size())
				curRotation = curRotation.Slerp(ch.rotations[rotIndex + 1].Rot, SpanDelta(ch.rotations, rotIndex, time));
		}

		if (!ch.scales.empty())
		{
			const size_t scaleIndex = KeyIndexAt(ch.scales, time);
			curScale = ch.scales[scaleIndex].Scale;
			if (scaleIndex + 1 < ch.scales.size())
				curScale = curScale.Lerp(ch.scales[scaleIndex + 1].Scale, SpanDelta(ch.scales, scaleIndex, time));
		}

		// Scale is off by default and opted into per clip via ANIM_FLAG_APPLY_SCALE.
		// Scale keys have always round-tripped through the file while the
		// sampler dropped them, so any clip authored since then may carry
		// scale keys nobody ever saw applied; turning this on globally would
		// change how those clips deform with no way to tell which.
		return ComposeTRS(curPosition, curRotation, curScale, applyScale);
	}

	bool SkeletonAnimationInstance::SetBoneWorldTransform(const int32 boneId,
		const Vec3 &worldPosition, const Quaternion &worldRotation)
	{
		if (boneId < 0 || (size_t)boneId >= boneTransformation.size()) return false;
		if (!rcomp || !rcomp->GetOwner()) return false;

		// world -> model space, with the owner's SCALE handled separately.
		//
		// A character is typically placed at a scale (0.1 here), and simply
		// inverting the owner's world matrix folds a factor of 10 into the
		// bone's local transform. Bone locals compose down the chain, so that
		// factor multiplies at every joint and the mesh detonates - measured
		// as a model filling the entire frame from three metres away.
		//
		// So: undo the scale to get a rigid owner frame, convert the rotation
		// through that, and convert the translation into model units by
		// dividing it out. What lands in boneTransformation is then a pure
		// rotation+translation, exactly like the one a clip writes.
		//
		// Matrix::GetScale() returns the DIAGONAL and Matrix::Scale() only
		// multiplies the diagonal, both of which are the scale only for a
		// matrix with no rotation in it. A character's owner almost always has
		// rotation - the root yaws to face where it is walking - and at 90
		// degrees of yaw the diagonal of a uniformly scaled frame reads
		// (0, s, 0): the guard below then replaced two of the three axes with
		// 1, the "rigid" frame came out scaled by 10 on two axes, and every
		// bone written through here landed somewhere meaningless. A column's
		// LENGTH is its scale no matter how the frame is turned.
		Matrix ownerWorld = rcomp->GetOwner()->GetWorldTransformation();
		const f32 *om = ownerWorld.m;
		Vec3 sc(
			sqrtf(om[0] * om[0] + om[1] * om[1] + om[2] * om[2]),
			sqrtf(om[4] * om[4] + om[5] * om[5] + om[6] * om[6]),
			sqrtf(om[8] * om[8] + om[9] * om[9] + om[10] * om[10]));
		if (fabs(sc.x) < 1e-6f) sc.x = 1.f;
		if (fabs(sc.y) < 1e-6f) sc.y = 1.f;
		if (fabs(sc.z) < 1e-6f) sc.z = 1.f;
		Matrix ownerRigid = ownerWorld;
		ownerRigid.m[0] /= sc.x; ownerRigid.m[1] /= sc.x; ownerRigid.m[2] /= sc.x;
		ownerRigid.m[4] /= sc.y; ownerRigid.m[5] /= sc.y; ownerRigid.m[6] /= sc.y;
		ownerRigid.m[8] /= sc.z; ownerRigid.m[9] /= sc.z; ownerRigid.m[10] /= sc.z;

		Quaternion rot = worldRotation;
		Matrix bodyWorld = rot.ConvertToMatrix();
		bodyWorld.Translate(worldPosition);

		Matrix model = ownerRigid.Inverse() * bodyWorld;
		Vec3 t = model.GetTranslation();
		model.Translate(Vec3(t.x / sc.x, t.y / sc.y, t.z / sc.z));

		// model -> parent-local. A root bone's parent is the model itself.
		const int32 parent = skeleton[boneId].parent;
		Matrix local = model;
		if (parent >= 0 && (size_t)parent < Bones.size())
		{
			Matrix parentModel = Bones[parent];
			local = parentModel.Inverse() * model;
		}
		boneTransformation[boneId] = local;

		// So the next call in a batch sees this bone's new model transform
		// when it asks for its parent.
		RefreshHierarchy();
		return true;
	}

	void SkeletonAnimationInstance::AddPoseModifier(PoseModifier fn, void* userData)
	{
		if (!fn) return;
		for (size_t i = 0; i < poseModifiers.size(); i++)
		{
			if (poseModifiers[i].userData == userData)
			{
				poseModifiers[i].fn = fn;
				return;
			}
		}
		PoseModifierEntry e;
		e.fn = fn;
		e.userData = userData;
		poseModifiers.push_back(e);
	}

	void SkeletonAnimationInstance::RemovePoseModifier(void* userData)
	{
		for (size_t i = 0; i < poseModifiers.size(); i++)
		{
			if (poseModifiers[i].userData == userData)
			{
				poseModifiers.erase(poseModifiers.begin() + i);
				return;
			}
		}
	}

	void SkeletonAnimationInstance::SetBoneAim(const int32 boneId, const Quaternion &rotation)
	{
		if (boneId < 0 || (size_t)boneId >= boneTransformation.size()) return;
		for (size_t i = 0; i < boneAims.size(); i++)
		{
			if (boneAims[i].bone == boneId) { boneAims[i].rotation = rotation; return; }
		}
		BoneAim a;
		a.bone = boneId;
		a.rotation = rotation;
		boneAims.push_back(a);
	}

	void SkeletonAnimationInstance::ClearBoneAim(const int32 boneId)
	{
		for (size_t i = 0; i < boneAims.size(); i++)
		{
			if (boneAims[i].bone == boneId) { boneAims.erase(boneAims.begin() + i); return; }
		}
	}

	void SkeletonAnimationInstance::ApplyBoneAims()
	{
		if (boneAims.empty()) return;
		for (size_t i = 0; i < boneAims.size(); i++)
		{
			const int32 id = boneAims[i].bone;
			// Turn the bone where it stands: about its own joint, in model
			// space, and then back into the space of its parent.
			const Matrix global = Bones[id];
			const Vec3 at = global.GetTranslation();
			Matrix toJoint, fromJoint;
			toJoint.Translate(-at.x, -at.y, -at.z);
			fromJoint.Translate(at);
			const Matrix turned = fromJoint * boneAims[i].rotation.ConvertToMatrix() * toJoint * global;
			boneTransformation[id] = GetParentMatrix(skeleton[id].parent, boneTransformation).Inverse() * turned;
			RefreshHierarchy();
		}
	}

	Vec3 SkeletonAnimationInstance::GetModelUp(const int32 footBone) const
	{
		if (modelUpAxis < 0)
		{
			modelUpAxis = 1; modelUpSign = 1.f; modelUpMean = 0.f;
			if (skeleton.empty() || bindPose.size() < skeleton.size()) return Vec3(0.f, 1.f, 0.f);
			// the bind pose in model space
			bindAt.assign(skeleton.size(), Vec3(0.f, 0.f, 0.f));
			std::vector<Matrix> global(skeleton.size());
			std::vector<bool> done(skeleton.size(), false);
			std::function<const Matrix&(int32)> g = [&](int32 b) -> const Matrix& {
				if (!done[b])
				{
					done[b] = true;
					const int32 p = skeleton[b].parent;
					global[b] = (p >= 0 && (size_t)p < skeleton.size() && p != b) ? g(p) * bindPose[b] : bindPose[b];
				}
				return global[b];
			};
			Vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f), mean(0.f, 0.f, 0.f);
			for (size_t i = 0; i < skeleton.size(); i++)
			{
				const int32 b = skeleton[i].self;
				if (b < 0 || (size_t)b >= skeleton.size()) continue;
				const Vec3 p = g(b).GetTranslation();
				bindAt[b] = p;
				lo = Vec3(std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z));
				hi = Vec3(std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z));
				mean = mean + p;
			}
			mean = mean * (1.f / (f32)skeleton.size());
			const Vec3 ext = hi - lo;
			if (ext.x > ext.y && ext.x > ext.z) modelUpAxis = 0;
			else if (ext.z > ext.y) modelUpAxis = 2;
			const auto on = [this](const Vec3 &v) { return modelUpAxis == 0 ? v.x : modelUpAxis == 1 ? v.y : v.z; };
			modelUpMean = on(mean);
			// with no foot to go by: the arms, hands and head put more bones
			// above the hips than the legs below them, so the mean lies high
			modelUpSign = (modelUpMean - on(lo)) >= (on(hi) - modelUpMean) ? 1.f : -1.f;
		}
		f32 sign = modelUpSign;
		if (footBone >= 0 && (size_t)footBone < bindAt.size())
		{
			const Vec3 &p = bindAt[footBone];
			const f32 c = modelUpAxis == 0 ? p.x : modelUpAxis == 1 ? p.y : p.z;
			sign = c <= modelUpMean ? 1.f : -1.f;      // the foot is the low end
		}
		return Vec3(modelUpAxis == 0 ? sign : 0.f, modelUpAxis == 1 ? sign : 0.f, modelUpAxis == 2 ? sign : 0.f);
	}

	void SkeletonAnimationInstance::SetFootGround(const int32 upperBone, const int32 footBone, const f32 height,
		const Vec3 &normal, const f32 weight)
	{
		SetFootGroundAlong(upperBone, footBone, height, normal, GetModelUp(footBone), weight);
	}

	void SkeletonAnimationInstance::SetFootGroundAlong(const int32 upperBone, const int32 footBone, const f32 height,
		const Vec3 &normal, const Vec3 &up, const f32 weight)
	{
		if (upperBone < 0 || footBone < 0 || (size_t)upperBone >= skeleton.size() || (size_t)footBone >= skeleton.size()) return;
		FootGround g;
		g.upper = upperBone; g.foot = footBone; g.height = height; g.normal = normal;
		const f32 lu = up.magnitude();
		g.up = lu > 1e-6f ? up * (1.f / lu) : Vec3(0.f, 1.f, 0.f);
		g.weight = std::max(0.f, std::min(1.f, weight));
		for (size_t i = 0; i < footGrounds.size(); i++)
			if (footGrounds[i].foot == footBone) { footGrounds[i] = g; return; }
		footGrounds.push_back(g);
	}

	bool SkeletonAnimationInstance::SetFootGroundWorld(const int32 upperBone, const int32 footBone, const f32 groundY,
		const Vec3 &normal, const f32 weight)
	{
		if (!rcomp || !rcomp->GetOwner() || footBone < 0 || (size_t)footBone >= Bones.size()) return false;
		const Matrix world = rcomp->GetOwner()->GetWorldTransformation();
		const Matrix toModel = world.Inverse();
		const Vec3 foot = world * Bones[footBone].GetTranslation();
		const Vec3 ground = toModel * Vec3(foot.x, groundY, foot.z);
		const Vec3 n = toModel * (Vec3(foot.x, groundY, foot.z) + normal) - ground;
		// the world's up, in the model: Y for a Y-up model stood upright, Z for a Z-up one
		Vec3 up = toModel * Vec3(foot.x, groundY + 1.f, foot.z) - ground;
		const f32 lu = up.magnitude();
		up = lu > 1e-6f ? up * (1.f / lu) : GetModelUp(footBone);
		SetFootGroundAlong(upperBone, footBone, ground.dotProduct(up), n, up, weight);
		return true;
	}

	void SkeletonAnimationInstance::ClearFootGround(const int32 footBone)
	{
		if (footBone < 0) { footGrounds.clear(); return; }
		for (size_t i = 0; i < footGrounds.size(); i++)
			if (footGrounds[i].foot == footBone) { footGrounds.erase(footGrounds.begin() + i); return; }
	}

	void SkeletonAnimationInstance::ApplyFootGround()
	{
		if (footGrounds.empty()) return;
		RefreshHierarchy();
		const auto at = [this](const int32 b) { return Bones[b].GetTranslation(); };
		// The hips come down for the lowest foot (a foot downhill), never up:
		// lifting them would leave the other foot hanging.
		f32 drop = 0.f, leg = 0.f;
		int32 hips = -1;
		for (size_t i = 0; i < footGrounds.size(); i++)
		{
			const FootGround &g = footGrounds[i];
			const int32 knee = skeleton[g.foot].parent;
			if (knee < 0 || skeleton[knee].parent != g.upper) continue;
			drop = std::min(drop, g.height * g.weight);
			leg = std::max(leg, (at(g.upper) - at(knee)).magnitude() + (at(knee) - at(g.foot)).magnitude());
			if (hips < 0) hips = skeleton[g.upper].parent;
		}
		drop = std::max(drop, -footMaxDrop * leg);
		const Vec3 hipsUp = footGrounds[0].up;
		if (drop < 0.f && hips >= 0)
		{
			Matrix down;
			down.Translate(hipsUp * drop);
			const int32 parent = skeleton[hips].parent;
			const Matrix parentGlobal = parent >= 0 ? Bones[parent] : Matrix();
			boneTransformation[hips] = parentGlobal.Inverse() * (down * Bones[hips]);
			RefreshHierarchy();
		}
		for (size_t i = 0; i < footGrounds.size(); i++)
		{
			const FootGround &g = footGrounds[i];
			const Vec3 &up = g.up;
			if (g.weight <= 0.f) continue;
			const int32 knee = skeleton[g.foot].parent;
			if (knee < 0 || skeleton[knee].parent != g.upper) continue;
			// Reach: from where the clip (and the hips) left the foot, by the
			// ground's height there over the floor, less what the hips gave.
			// (Never more than footMaxRaise of a leg: a ground far above the foot - a probe
			// that found the terrain over a body standing under it - folded the leg double,
			// the boots up by the head.)
			const f32 lift = std::min(g.height * g.weight - drop, footMaxRaise * leg);
			if (std::fabs(lift) > 1e-5f)
			{
				const Vec3 hip = at(g.upper), k = at(knee), f = at(g.foot);
				// the knee keeps bending the way it was
				Vec3 bend = k - (hip + f) * 0.5f;
				const Vec3 pole = bend.magnitude() > 1e-4f ? k + bend : Vec3(0.f, 0.f, 0.f);
				IKSolver::Solve(this, g.upper, g.foot, f + up * lift, pole, 10, NULL, false);
			}
			// Flat to the ground: turned about its own joint from up to the normal.
			const f32 ln = g.normal.magnitude();
			if (ln > 1e-5f)
			{
				const Vec3 n = g.normal * (1.f / ln);
				const f32 d = std::max(-1.f, std::min(1.f, up.dotProduct(n)));
				const Vec3 axis = up.cross(n);
				if (axis.magnitude() > 1e-5f && d > 0.f)
				{
					Quaternion q;
					q.AxisToQuaternion(axis.normalize(), std::acos(d) * g.weight);
					const Matrix global = Bones[g.foot];
					const Vec3 j = global.GetTranslation();
					Matrix toJoint, fromJoint;
					toJoint.Translate(-j.x, -j.y, -j.z);
					fromJoint.Translate(j);
					const Matrix turned = fromJoint * q.ConvertToMatrix() * toJoint * global;
					boneTransformation[g.foot] = GetParentMatrix(skeleton[g.foot].parent, boneTransformation).Inverse() * turned;
					RefreshHierarchy();
				}
			}
		}
	}

	void SkeletonAnimationInstance::RunPoseModifiers()
	{
		if (poseModifiers.empty()) return;

		// Each modifier expects composed model-space transforms to read from,
		// and a previous modifier may have invalidated them.
		RefreshHierarchy();
		for (size_t i = 0; i < poseModifiers.size(); i++)
		{
			poseModifiers[i].fn(this, poseModifiers[i].userData);
			RefreshHierarchy();
		}
	}

	void SkeletonAnimationInstance::RefreshHierarchy()
	{
		// Multiply bones with its parent - Tree
		for (std::vector<Bone>::iterator a = skeleton.begin(); a != skeleton.end(); a++)
			Bones[(*a).self] = GetParentMatrix((*a).parent, boneTransformation) * boneTransformation[(*a).self];
	}

	void SkeletonAnimationInstance::RefreshSkinning()
	{
		RefreshHierarchy();

		// Send SubMesh Bones to Material
		UploadSkinning();
	}

	void SkeletonAnimationInstance::SetBoneScale(const int32 boneId, const Vec3 &scale)
	{
		if (boneId < 0 || (size_t)boneId >= skeleton.size()) return;
		for (size_t i = 0; i < boneScales.size(); i++)
		{
			if (boneScales[i].bone == boneId) { boneScales[i].scale = scale; boneScales[i].viewOnly = false; boneScales[i].alone = false; return; }
		}
		BoneScale s;
		s.bone = boneId;
		s.scale = scale;
		boneScales.push_back(s);
	}

	void SkeletonAnimationInstance::SetBoneScaleInView(const int32 boneId, const Vec3 &scale)
	{
		SetBoneScale(boneId, scale);
		for (size_t i = 0; i < boneScales.size(); i++)
			if (boneScales[i].bone == boneId) boneScales[i].viewOnly = true;
	}

	void SkeletonAnimationInstance::SetBoneSkinScaleInView(const int32 boneId, const Vec3 &scale)
	{
		SetBoneScale(boneId, scale);
		for (size_t i = 0; i < boneScales.size(); i++)
			if (boneScales[i].bone == boneId) { boneScales[i].viewOnly = true; boneScales[i].alone = true; }
	}

	void SkeletonAnimationInstance::ClearBoneScale(const int32 boneId)
	{
		for (size_t i = 0; i < boneScales.size(); i++)
		{
			if (boneScales[i].bone == boneId) { boneScales.erase(boneScales.begin() + i); return; }
		}
	}

	void SkeletonAnimationInstance::UploadSkinning()
	{
		// A scaled bone: its skin, and the skin of every bone below it, is
		// scaled about ITS joint - Bones[a] * S * Bones[a]^-1 in model space,
		// put in front of the skinning matrix. The pose itself is untouched.
		// (Twice where a scale is for the view only - SetBoneScaleInView: with
		// every scale for what is looked at, and with only the others for the
		// shadow maps.)
		std::vector<Matrix> pre, preShadow;
		std::vector<uint8> scaled, scaledShadow;
		bool anyViewOnly = false;
		if (!boneScales.empty())
		{
			pre.resize(Bones.size());
			scaled.assign(Bones.size(), 0);
			for (size_t i = 0; i < boneScales.size(); i++) if (boneScales[i].viewOnly) anyViewOnly = true;
			if (anyViewOnly) { preShadow.resize(Bones.size()); scaledShadow.assign(Bones.size(), 0); }
			for (size_t i = 0; i < boneScales.size(); i++)
			{
				const int32 a = boneScales[i].bone;
				if (a < 0 || (size_t)a >= Bones.size()) continue;
				Matrix s;
				s.Scale(boneScales[i].scale);
				const Matrix about = Bones[a] * s * Bones[a].Inverse();
				for (size_t b = 0; b < Bones.size() && b < skeleton.size(); b++)
				{
					int32 p = (int32)b;
					int32 guard = 0;
					if (boneScales[i].alone) { if (p != a) continue; }
					else while (p >= 0 && p != a && guard++ < 256) p = skeleton[p].parent;
					if (p != a) continue;
					pre[b] = about * pre[b];
					scaled[b] = 1;
					if (anyViewOnly && !boneScales[i].viewOnly) { preShadow[b] = about * preShadow[b]; scaledShadow[b] = 1; }
				}
			}
		}
		// Every mesh the component has: the model's own, those of its further
		// levels of detail, and those its shadow is drawn with. (Only the model's
		// own were posed: a simplified level stood in its bind pose.)
		std::vector<RenderingMesh*> all;
		// Counted once: a loop re-reading GetLODSize() while GetMeshes() could
		// add levels never ended on a component with no meshes.
		const uint32 levels = rcomp->GetLODSize();
		for (uint32 l = 0; l < levels; l++)
		{
			std::vector<RenderingMesh*> &level = rcomp->GetMeshes(l);
			all.insert(all.end(), level.begin(), level.end());
		}
		all.insert(all.end(), rcomp->GetShadowMeshes().begin(), rcomp->GetShadowMeshes().end());
		for (std::vector<RenderingMesh*>::iterator j = all.begin(); j != all.end(); j++)
		{
			if ((*j)->MapBoneIDs.empty()) continue;
			if ((*j)->SkinningBones.size() != (*j)->MapBoneIDs.size()) (*j)->SkinningBones.resize((*j)->MapBoneIDs.size());
			RenderingMesh::NoteBonesForDraw(*j);
			if (anyViewOnly) (*j)->ShadowSkinningBones.resize((*j)->SkinningBones.size());
			else if (!(*j)->ShadowSkinningBones.empty()) (*j)->ShadowSkinningBones.clear();
			for (std::map<int32, int32>::iterator k = (*j)->MapBoneIDs.begin(); k != (*j)->MapBoneIDs.end(); k++)
			{
				const int32 b = (*k).first;
				const Matrix skin = Bones[b] * (*j)->BoneOffsetMatrix[b];
				(*j)->SkinningBones[(*k).second] = (!scaled.empty() && (size_t)b < scaled.size() && scaled[b]) ? pre[b] * skin : skin;
				if (anyViewOnly && (size_t)(*k).second < (*j)->ShadowSkinningBones.size())
					(*j)->ShadowSkinningBones[(*k).second] = ((size_t)b < scaledShadow.size() && scaledShadow[b]) ? preShadow[b] * skin : skin;
			}
		}
	}

	void SkeletonAnimationInstance::ApplyPose(const std::vector<Matrix> &localTransforms)
	{
		if (localTransforms.size() != boneTransformation.size())
		{
			echo("ERROR: SkeletonAnimationInstance::ApplyPose - pose has " + std::to_string(localTransforms.size()) + " bones, skeleton has " + std::to_string(boneTransformation.size()));
			return;
		}
		boneTransformation = localTransforms;
		RefreshSkinning();
	}

	void SkeletonAnimationInstance::ResetToBindPose()
	{
		boneTransformation = bindPose;
		// Rest is rest for the parts too: no clip is showing.
		trackValues.clear();
		RefreshSkinning();
	}

	std::vector<f32> SkeletonAnimationInstance::FindContacts(const uint32 animation, const std::string &bone, const f32 band) const
	{
		if (!Owner || animation >= Owner->animations.size()) return std::vector<f32>();
		return FindContacts(Owner->animations[animation], bone, band);
	}

	std::vector<f32> SkeletonAnimationInstance::FindContacts(const Animation &anim, const std::string &bone, const f32 band) const
	{
		std::vector<f32> out;
		if (anim.Duration <= 0.f) return out;
		// skeleton entries by their id, and the bone's chain up to the root
		std::map<int32, size_t> byId;
		int32 target = -1;
		for (size_t i = 0; i < skeleton.size(); i++)
		{
			byId[skeleton[i].self] = i;
			if (skeleton[i].name == bone) target = skeleton[i].self;
		}
		if (target < 0) return out;
		std::vector<int32> chain;
		for (int32 b = target; b >= 0 && chain.size() <= skeleton.size(); )
		{
			chain.push_back(b);
			std::map<int32, size_t>::const_iterator it = byId.find(b);
			b = it == byId.end() ? -1 : skeleton[it->second].parent;
		}
		std::map<int32, const Channel*> channelOf;
		for (size_t c = 0; c < anim.Channels.size(); c++)
			for (size_t k = 0; k < chain.size(); k++)
			{
				std::map<int32, size_t>::const_iterator it = byId.find(chain[k]);
				if (it != byId.end() && skeleton[it->second].name == anim.Channels[c].NodeName) channelOf[chain[k]] = &anim.Channels[c];
			}
		const bool applyScale = anim.HasFlag(ANIM_FLAG_APPLY_SCALE);
		const Vec3 up = GetModelUp(target);
		const int n = std::max(32, std::min(4000, (int)(anim.Duration * 120.f)));
		std::vector<f32> height(n);
		for (int i = 0; i < n; i++)
		{
			const f32 t = anim.Duration * (f32)i / (f32)n;
			Matrix global;
			for (size_t k = chain.size(); k-- > 0; )
			{
				const int32 b = chain[k];
				const Matrix* bind = (size_t)b < bindPose.size() ? &bindPose[b] : NULL;
				std::map<int32, const Channel*>::const_iterator ch = channelOf.find(b);
				const Matrix local = ch != channelOf.end() ? SampleChannel(*ch->second, t, applyScale, bind)
					: (bind ? *bind : Matrix());
				global = global * local;
			}
			height[i] = global.GetTranslation().dotProduct(up);
		}
		f32 lo = height[0], hi = height[0];
		for (int i = 1; i < n; i++) { lo = std::min(lo, height[i]); hi = std::max(hi, height[i]); }
		if (hi - lo < 1e-5f) return out;
		const f32 b = std::max(0.f, std::min(1.f, band));
		const f32 limit = lo + b * (hi - lo);
		// A step only counts once the foot has been lifted again - half way up
		// its range: a walk's ankle dips twice in one stance (heel, then toe).
		const f32 rearm = lo + std::max(0.5f, b) * (hi - lo);
		// Round the loop from the highest sample, so a step across the end is
		// one step.
		int top = 0;
		for (int i = 1; i < n; i++) if (height[i] > height[top]) top = i;
		bool armed = true;
		for (int k = 1; k <= n; k++)
		{
			const int i = (top + k) % n;
			if (armed && height[i] <= limit) { out.push_back(anim.Duration * (f32)i / (f32)n); armed = false; }
			else if (!armed && height[i] >= rearm) armed = true;
		}
		std::sort(out.begin(), out.end());
		return out;
	}

	void SkeletonAnimationInstance::ApplyAnimationAtTime(const Animation &anim, const f32 time)
	{
		// Bones this clip has no channel for keep their BIND transform, not
		// identity - that is what a single Play() of the same clip produces,
		// because Play() seeds boneTransformationPerAnimation with bindPose
		// and Update() only overwrites the entries a channel actually
		// drives. Matching that exactly is the whole point: the editor's
		// scrub has to look like the runtime's playback, and seeding
		// identity here (as this first did) collapsed every unkeyed bone to
		// the origin - which reads as a folded-up heap for a fresh clip and
		// silently disagreed with what the game would show.
		std::vector<Matrix> pose = bindPose;

		for (uint32 a = 0; a < anim.Channels.size(); a++)
		{
			int32 boneId = -1;
			for (std::vector<Bone>::const_iterator b = skeleton.begin(); b != skeleton.end(); b++)
			{
				if ((*b).name.compare(anim.Channels[a].NodeName) == 0)
				{
					boneId = (*b).self;
					break;
				}
			}
			if (boneId < 0) continue; // channel for a node this skeleton doesn't have

			// Same bind-pose fallback the runtime uses, so a scrub and a
			// playback of the same clip cannot disagree.
			pose[boneId] = SampleChannel(anim.Channels[a], time, anim.HasFlag(ANIM_FLAG_APPLY_SCALE),
				(size_t)boneId < bindPose.size() ? &bindPose[boneId] : NULL);
		}

		// The clip's property tracks at this time too, so a scrub shows a
		// part's picture swap or fade exactly as playback would.
		trackValues.clear();
		for (size_t t = 0; t < anim.Tracks.size(); t++)
		{
			if (anim.Tracks[t].Keys.empty()) continue;
			TrackValue &out = trackValues[anim.Tracks[t].Target + '\n' + anim.Tracks[t].Property];
			SampleTrack(anim.Tracks[t], time, out.value, out.text);
			out.hasValue = true;
			out.hasText = !out.text.empty();
		}

		ApplyPose(pose);
	}

	Matrix SkeletonAnimation::SCALE(const Matrix &in, const Matrix &prev, const f32 s)
	{
		// Scale is carried through. It used to be computed (from the diagonal,
		// which is wrong for any turned bone) and then dropped, so a clip with
		// ANIM_FLAG_APPLY_SCALE lost its scale the moment a second clip played.
		// A pose with no scale in it decomposes to 1,1,1 and blends to 1,1,1,
		// so rigid clips are unchanged.
		Vec3 ta, tb, sa, sb;
		Quaternion ra, rb;
		DecomposeTRS(in, ta, ra, sa);
		DecomposeTRS(prev, tb, rb, sb);
		return ComposeTRS(ta.Lerp(tb, s), ra.Slerp(rb, s), sa.Lerp(sb, s), true);
	}

}