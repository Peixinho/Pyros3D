//============================================================================
// Name        : AnimationLoader.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads model animation based on Assimp
//============================================================================

#ifndef ANIMATIONLOADER_H
#define	ANIMATIONLOADER_H

#include <Pyros3D/Utils/ModelLoaders/IModelLoader.h>
#include <Pyros3D/Core/Math/Quaternion.h>
#include <Pyros3D/Core/Math/Matrix.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Utils/Binary/BinaryFile.h>

namespace p3d {

	// How the curve leaves a key on its way to the next one. Stored per key
	// and honoured by SkeletonAnimationInstance::SampleChannel(), which is the
	// single sampler both runtime playback and the editor's scrub go through.
	//
	// Every mode is expressed as a remap of the normalised span parameter
	// t in [0,1] rather than as a different way of combining the two values.
	// That matters for rotations: the remapped t still feeds Slerp, so eased
	// rotation stays a proper shortest-arc interpolation instead of degrading
	// into a component-wise blend the way a per-channel Bezier would.
	enum InterpolationMode {
		INTERP_LINEAR = 0,   // constant velocity across the span (the v0 behaviour)
		INTERP_STEP = 1,     // hold this key's value until the next key
		INTERP_EASE_IN = 2,  // start slow
		INTERP_EASE_OUT = 3, // end slow
		INTERP_EASE_BOTH = 4,// slow at both ends
		INTERP_BEZIER = 5    // Hermite on t using OutTangent / the next key's InTangent
	};

	// Every key carries the three fields below. InTangent/OutTangent are
	// slopes of the parameter curve and are only read for INTERP_BEZIER; 1.0
	// reproduces LINEAR exactly, which is why they default to it.

	// Stores Positions
	struct PYROS3D_API PositionData {
		// Time
		f32 Time;
		// Position
		Vec3 Pos;
		// Interpolation from this key to the next
		uchar Mode;
		f32 InTangent;
		f32 OutTangent;
		PositionData() : Time(0.f), Mode(INTERP_LINEAR), InTangent(1.f), OutTangent(1.f) {}
		PositionData(const f32 &time, const Vec3 &pos) : Time(time), Pos(pos), Mode(INTERP_LINEAR), InTangent(1.f), OutTangent(1.f) {}
	};

	// Stores Rotations
	struct PYROS3D_API RotationData {
		// Time
		f32 Time;
		// Rotation
		Quaternion Rot;
		// Interpolation from this key to the next
		uchar Mode;
		f32 InTangent;
		f32 OutTangent;
		RotationData() : Time(0.f), Mode(INTERP_LINEAR), InTangent(1.f), OutTangent(1.f) {}
		RotationData(const f32 &time, const Quaternion &rot) : Time(time), Rot(rot), Mode(INTERP_LINEAR), InTangent(1.f), OutTangent(1.f) {}
	};

	// Stores Scaling
	struct PYROS3D_API ScalingData {
		// Time
		f32 Time;
		// Scale
		Vec3 Scale;
		// Interpolation from this key to the next
		uchar Mode;
		f32 InTangent;
		f32 OutTangent;
		ScalingData() : Time(0.f), Scale(1.f, 1.f, 1.f), Mode(INTERP_LINEAR), InTangent(1.f), OutTangent(1.f) {}
		ScalingData(const f32 &time, const Vec3 &scale) : Time(time), Scale(scale), Mode(INTERP_LINEAR), InTangent(1.f), OutTangent(1.f) {}
	};

	// Channel Struct
	struct Channel {

		// Node Name
		std::string NodeName;

		// position, rotation and scale
		std::vector<PositionData> positions;
		std::vector<RotationData> rotations;
		std::vector<ScalingData> scales;
	};

	// Clip-level switches. Deliberately opt-in: a v0 file has no flags word at
	// all and loads as 0, so every clip that exists today keeps behaving
	// exactly as it does now.
	enum AnimationFlags {
		// Playback wraps rather than holding the final pose. Purely descriptive
		// today - Play()'s `repetition` argument still drives looping - but it
		// records the author's intent, which nothing in the format could before.
		ANIM_FLAG_LOOP = 1 << 0,
		// Apply this clip's scale keys to the bone matrix. Scale keys have
		// always round-tripped through the file while SampleChannel() dropped
		// them on the floor; turning that on globally would silently change how
		// every existing clip deforms, so it is a per-clip opt-in instead.
		ANIM_FLAG_APPLY_SCALE = 1 << 1
	};

	// Animation Struct
	struct Animation {

		// Animation Name
		std::string AnimationName;
		// Stable identity, 16 raw bytes. A clip's runtime id is its index in
		// SkeletonAnimation::animations, so deleting a clip renumbered every
		// later one and any scene that had saved "play clip 2" silently started
		// playing something else. Scenes save this instead. Empty (all-zero) for
		// v0 files, which still resolve by index.
		std::string Guid;
		// each animation has at least one channel
		std::vector<Channel> Channels;
		// Animation Duration
		f32 Duration;
		// Ticks per Second
		f32 TicksPerSecond;
		// AnimationFlags bitfield
		uint32 Flags;
		// Frame rate the clip was authored at. The editor snaps keys to a grid
		// but had nowhere to record which grid, so reopening a clip lost it.
		// 0 means unspecified.
		f32 AuthoredFps;

		Animation() : Duration(0.f), TicksPerSecond(1.f), Flags(0), AuthoredFps(0.f) {}

		bool HasFlag(const uint32 f) const { return (Flags & f) != 0; }
	};

	// ---- .p3da on-disk format -------------------------------------------
	//
	// v0 (everything written before the Animation Editor existed) has NO
	// header at all: the file opens directly with a small positive clip
	// count. v1 opens with the magic below, which is why adding a header was
	// free - an int32 clip count can never collide with "P3DA".
	//
	//   char[4] magic "P3DA"
	//   uint32  version
	//   int32   clipCount
	//     int32 nameLen, char[nameLen] name
	//     char[16] guid                      (v1+)
	//     uint32   flags                     (v1+, AnimationFlags)
	//     f32      authoredFps               (v1+, 0 = unspecified)
	//     int32 channelCount
	//     f32   duration
	//     f32   ticksPerSecond
	//     channelCount x:
	//       int32 nodeNameLen, char[nodeNameLen] nodeName
	//       int32 posCount,   x (f32 time, f32 x,y,z,   uchar mode, f32 inTan, f32 outTan)
	//       int32 rotCount,   x (f32 time, f32 w,x,y,z, uchar mode, f32 inTan, f32 outTan)
	//       int32 scaleCount, x (f32 time, f32 x,y,z,   uchar mode, f32 inTan, f32 outTan)
	//
	// Quaternions are w-FIRST on disk. The per-key mode/tangent triple is
	// absent in v0 and is read back as LINEAR/1/1, which is exactly the
	// interpolation v0 files were played with.
	static const uint32 P3DA_VERSION = 1;

	class PYROS3D_API AnimationLoader : public IModelLoader {
	public:

		AnimationLoader();
		virtual ~AnimationLoader();
		virtual bool Load(const std::string &Filename);

		// Writes clips back out in the layout Load() reads, always at the
		// current version. Until the Animation Editor existed the only writer
		// of a .p3da lived in the offline AssimpImporter tool
		// (AssimpAnimationImporter::ConvertToPyrosFormat), so nothing in the
		// engine or the editor could produce or re-save one.
		//
		// Round-trip note: Load() divides every key time (and Duration) by
		// TicksPerSecond and then stores TicksPerSecond as 1, so times in a
		// loaded Animation are already seconds. Save() therefore writes times
		// verbatim with TicksPerSecond 1 and the next Load() divides by 1 -
		// lossless. A caller building an Animation by hand should do the same:
		// seconds, TicksPerSecond 1.
		//
		// Unlike the importer's version this tolerates empty key vectors
		// (that one does &keys[0] unconditionally, which is UB on an empty
		// clip) - an editor-authored channel commonly keys rotation only.
		static bool Save(const std::string &Filename, const std::vector<Animation> &animations);

		// 16 raw bytes of randomness for Animation::Guid. Not a formatted
		// UUID - nothing parses these, they are only ever compared.
		static std::string GenerateGuid();

		// animations
		std::vector<Animation> animations;
	};

}

#endif	/* ANIMATIONLOADER_H */
