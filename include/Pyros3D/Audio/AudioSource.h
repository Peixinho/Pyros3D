//============================================================================
// Name        : AudioSource.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A positional sound emitter bound to a GameObject
//============================================================================

#ifndef AUDIOSOURCE_H
#define	AUDIOSOURCE_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Other/Export.h>
#include <string>
#include <memory>

struct ma_sound;

namespace p3d {

	class AudioBus;

	// A sound that lives on a GameObject and follows it.
	//
	// Attach it and it runs itself: the IComponent::Update() hook the
	// SceneGraph already calls every frame pushes the owner's world position
	// and facing into the audio engine, so a source on a moving object pans
	// and attenuates correctly with no per-frame code from the caller. Same
	// self-driving arrangement as ParticleSystem.
	//
	// Two shapes of emitter, chosen by SetSpatialization():
	//
	//   Spatialized (the default) - positioned in the world, attenuated by
	//   distance from the listener and optionally directional. SetCone() makes
	//   it a beam rather than a point: full volume inside the inner angle,
	//   falling to `outerGain` past the outer angle, aimed along the owner's
	//   forward axis. A PA horn, a directional alarm, a TV facing into a room.
	//
	//   Non-spatialized - constant volume everywhere, ignoring position
	//   entirely. This is what music and ambience want; a track that gets
	//   quieter when the player walks away from an invisible point is a bug,
	//   not atmosphere.
	//
	// Large files should be streamed (see the constructor's `stream` flag)
	// rather than decoded into memory up front - a few minutes of music is
	// tens of megabytes decoded.
	class PYROS3D_API AudioSource : public IComponent {

	public:

		// `stream` decodes on the fly instead of loading the whole file into
		// memory: right for music and long ambience, wasteful for short
		// effects (and a streamed sound can only have one voice, so it cannot
		// overlap with itself).
		//
		// `bus` routes this source's output through an AudioBus (a submix -
		// e.g. "Music" or "SFX") instead of straight into the master mix. A
		// shared_ptr, not a raw AudioBus*: this source keeps its own
		// reference to the bus for as long as it lives, so the bus cannot be
		// destroyed out from under it regardless of the order a script (or
		// its GC) happens to drop things in - see AudioBus's own comment.
		AudioSource(const std::string &file, const bool stream = false, const std::shared_ptr<AudioBus> &bus = nullptr);
		virtual ~AudioSource();

		// False if the file could not be loaded, or there is no AudioManager.
		// Every method below is a safe no-op in that state.
		bool IsLoaded() const { return loaded; }
		const std::string &GetFile() const { return file; }

		// ***************************** Playback *****************************

		void Play();
		void Pause();
		// Stops and rewinds, so the next Play() starts from the beginning.
		void Stop();
		bool IsPlaying() const;

		void SetLooping(const bool looping);
		bool IsLooping() const { return looping; }

		void SetVolume(const f32 volume);
		f32 GetVolume() const { return volume; }

		// 1 = original speed/pitch, 2 = an octave up and twice as fast.
		void SetPitch(const f32 pitch);
		f32 GetPitch() const { return pitch; }

		// Ramps the volume over `milliseconds` - use instead of a hard
		// Play()/Stop() for music, where an abrupt cut is very audible.
		void FadeIn(const f32 milliseconds);
		void FadeOut(const f32 milliseconds);

		// **************************** Positioning ***************************

		// Off makes this an ambient/music source: constant volume, no panning.
		void SetSpatialization(const bool enabled);
		bool IsSpatialized() const { return spatialized; }

		// See AttenuationModel::*. minDistance is the radius within which the
		// sound is at full volume; past maxDistance it is at its quietest.
		void SetAttenuation(const uint32 model, const f32 minDistance, const f32 maxDistance);

		// Turns the source into a directional cone aimed along the owner's
		// forward axis. Angles are in radians and measured from the axis, so
		// they are half-angles: a 60-degree-wide beam is an inner angle of
		// 30 degrees. `outerGain` is the volume multiplier outside the outer
		// cone - 0 is silent from behind, 1 disables the cone entirely.
		void SetCone(const f32 innerAngle, const f32 outerAngle, const f32 outerGain);
		// Undoes SetCone(): the source radiates equally in all directions.
		void ClearCone();

		// How strongly the listener's own facing affects what is heard,
		// 0 (not at all) to 1 (fully). Independent of the source's cone.
		void SetDirectionalAttenuation(const f32 factor);

		// Pitch shift from relative motion. 0 disables it, 1 is physically
		// scaled. Only meaningful on a source that actually moves - and only
		// audible if the source or the listener actually has a velocity,
		// which Update()/AudioManager derive automatically each frame by
		// finite-differencing position; there is nothing else to wire up.
		void SetDopplerFactor(const f32 factor);

		// Call after repositioning this source's owner in a way that is NOT
		// real motion - a respawn, a pool object being parked/reactivated, a
		// scene (re)load, snapping a camera-attached source to a cut. Without
		// this, the very next Update() sees the jump as an ordinary frame's
		// worth of motion and finite-differences it into a velocity of
		// however many thousands of units the jump was divided by one frame's
		// dt - real bug, found by reading the resulting velocity back out of
		// miniaudio during actual play: parking a pooled source at Vec3(0,0,
		// -4000) between uses and later reactivating it produced a reported
		// speed in the hundreds of thousands, which is an audible Doppler
		// shriek for one frame every single time. This makes the following
		// Update() re-baseline instead of computing a velocity from it.
		void ResetVelocityTracking() { hasLastUpdate = false; }

		// Manual stereo balance, -1 (full left) to 1 (full right). Mainly
		// useful on a non-spatialized source (spatialization already computes
		// its own pan from 3D position, so the two fight each other if both
		// are active).
		void SetPan(const f32 pan);
		f32 GetPan() const { return pan; }

		// One low-pass or high-pass filter can be inserted on this source's
		// output at a time - setting one clears the other. `order` is the
		// filter's steepness (2 = 12dB/octave, a reasonable default; higher
		// numbers cut more sharply per octave past the cutoff).
		void SetFilter(const uint32 type, const f32 cutoffHz, const uint32 order = 2);
		void ClearFilter();
		uint32 GetFilterType() const { return filterType; }
		f32 GetFilterCutoff() const { return filterCutoff; }
		uint32 GetFilterOrder() const { return filterOrder; }

		// **************************** Playback state ************************

		// Seconds, using the engine's own decode of the file - real length/
		// position, not anything this class tracks itself.
		f32 GetLengthSeconds() const;
		f32 GetCursorSeconds() const;
		void SeekSeconds(const f32 seconds);
		// True once a non-looping source has played through to its end.
		bool AtEnd() const;

		// **************************** Read-back *****************************
		//
		// miniaudio has no getters for the cone or the attenuation settings, so
		// every setter above also caches its arguments here. That is what lets
		// the scene serializer round-trip a source rather than write out
		// whatever the defaults happened to be.

		bool IsStreamed() const { return streamed; }
		uint32 GetAttenuationModel() const { return attenuationModel; }
		f32 GetMinDistance() const { return minDistance; }
		f32 GetMaxDistance() const { return maxDistance; }
		bool HasCone() const { return hasCone; }
		f32 GetConeInnerAngle() const { return coneInner; }
		f32 GetConeOuterAngle() const { return coneOuter; }
		f32 GetConeOuterGain() const { return coneOuterGain; }
		f32 GetDirectionalAttenuation() const { return directionalAttenuation; }
		f32 GetDopplerFactor() const { return dopplerFactor; }

		// **************************** IComponent ****************************

		virtual uint32 GetComponentType() const { return ComponentType::AudioSource; }

		virtual void Register(SceneGraph* Scene) {}
		virtual void Unregister(SceneGraph* Scene) {}
		virtual void Init() {}
		virtual void Destroy() {}
		// The one real hook: pushes the owner's world position and forward
		// axis into the audio engine.
		virtual void Update(const f64 time = 0);

	private:

		std::string file;
		bool loaded;

		ma_sound* sound;
		// Kept alive purely so the routing target this source's node was
		// attached to at construction (this bus, or the master mix) outlives
		// the source - see the constructor's comment.
		std::shared_ptr<AudioBus> bus;

		bool looping;
		bool spatialized;
		f32 volume;
		f32 pitch;
		f32 pan;

		bool streamed;
		uint32 attenuationModel;
		f32 minDistance, maxDistance;
		bool hasCone;
		f32 coneInner, coneOuter, coneOuterGain;
		f32 directionalAttenuation;
		f32 dopplerFactor;

		// Doppler - see AudioManager::SetListener()'s identical comment for
		// why velocity is derived from position rather than asked for
		// directly.
		Vec3 lastPosition;
		f64 lastUpdateTime;
		bool hasLastUpdate;

		// Opaque (ma_lpf_node/ma_hpf_node are anonymous structs in miniaudio -
		// they have no tag to forward-declare, unlike ma_sound - so this is a
		// plain void* cast back in the .cpp, the same PIMPL idea one level
		// blunter). NULL when no filter is active.
		void* filterNode;
		uint32 filterType;
		f32 filterCutoff;
		uint32 filterOrder;
	};

}

#endif	/* AUDIOSOURCE_H */
