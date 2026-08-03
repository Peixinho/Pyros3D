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

struct ma_sound;

namespace p3d {

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
		AudioSource(const std::string &file, const bool stream = false);
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
		// scaled. Only meaningful on a source that actually moves.
		void SetDopplerFactor(const f32 factor);

		// **************************** IComponent ****************************

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

		bool looping;
		bool spatialized;
		f32 volume;
		f32 pitch;
	};

}

#endif	/* AUDIOSOURCE_H */
