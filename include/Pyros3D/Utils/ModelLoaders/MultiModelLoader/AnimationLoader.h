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

	// Stores Positions
	struct PYROS3D_API PositionData {
		// Time
		f32 Time;
		// Position
		Vec3 Pos;
		PositionData() {}
		PositionData(const f32 &time, const Vec3 &pos) : Time(time), Pos(pos) {}
	};

	// Stores Rotations
	struct PYROS3D_API RotationData {
		// Time
		f32 Time;
		// Rotation
		Quaternion Rot;
		RotationData() {}
		RotationData(const f32 &time, const Quaternion &rot) : Time(time), Rot(rot) {}
	};

	// Stores Scaling
	struct PYROS3D_API ScalingData {
		// Time
		f32 Time;
		// Scale
		Vec3 Scale;
		ScalingData() {}
		ScalingData(const f32 &time, const Vec3 &scale) : Time(time), Scale(scale) {}
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

	// Animation Struct
	struct Animation {

		// Animation Name
		std::string AnimationName;
		// each animation has at least one channel
		std::vector<Channel> Channels;
		// Animation Duration
		f32 Duration;
		// Ticks per Second
		f32 TicksPerSecond;

	};

	class PYROS3D_API AnimationLoader : public IModelLoader {
	public:

		AnimationLoader();
		virtual ~AnimationLoader();
		virtual bool Load(const std::string &Filename);

		// Writes clips back out in exactly the layout Load() reads. Until
		// now the only writer of a .p3da lived in the offline AssimpImporter
		// tool (AssimpAnimationImporter::ConvertToPyrosFormat), so nothing
		// in the engine or the editor could produce or re-save one - which
		// is what the Animation Editor needs in order to author a clip at
		// all.
		//
		// Round-trip note: Load() divides every key time (and Duration) by
		// TicksPerSecond and then stores TicksPerSecond as 1, so times in a
		// loaded Animation are already seconds. Save() therefore writes
		// times verbatim with TicksPerSecond 1 and the next Load() divides
		// by 1 - lossless. A caller building an Animation by hand should do
		// the same: seconds, TicksPerSecond 1.
		//
		// Unlike the importer's version this tolerates empty key vectors
		// (that one does &keys[0] unconditionally, which is UB on an empty
		// clip) - an editor-authored channel commonly keys rotation only.
		static bool Save(const std::string &Filename, const std::vector<Animation> &animations);

		// animations
		std::vector<Animation> animations;
	};

}

#endif	/* ANIMATIONLOADER_H */