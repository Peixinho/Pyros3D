//============================================================================
// Name        : AnimationLoader.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads model animation based on Assimp
//============================================================================

#include <Pyros3D/Utils/ModelLoaders/MultiModelLoader/AnimationLoader.h>

namespace p3d {

	AnimationLoader::AnimationLoader() {}

	AnimationLoader::~AnimationLoader() {}

	bool AnimationLoader::Save(const std::string &Filename, const std::vector<Animation> &animations)
	{
		BinaryFile* bin = new BinaryFile();
		if (!bin->Open(Filename.c_str(), 'w'))
		{
			delete bin;
			return false;
		}

		int32 animationsSize = (int32)animations.size();
		bin->Write(&animationsSize, sizeof(int32));
		for (int32 i = 0; i < animationsSize; i++)
		{
			const Animation &animation = animations[i];

			// Animation Name
			int32 nameSize = (int32)animation.AnimationName.size();
			bin->Write(&nameSize, sizeof(int32));
			if (nameSize > 0)
				bin->Write((void*)animation.AnimationName.c_str(), sizeof(char)*nameSize);

			// Channels
			int32 channelsSize = (int32)animation.Channels.size();
			bin->Write(&channelsSize, sizeof(int32));

			// Duration
			f32 duration = animation.Duration;
			bin->Write(&duration, sizeof(f32));

			// Ticks Per Second - see the header: a loaded/authored clip is
			// already in seconds, and Load() divides by whatever is written
			// here, so anything but 1 would rescale the clip on every
			// save/load cycle.
			f32 ticksPerSecond = (animation.TicksPerSecond > 0.f ? animation.TicksPerSecond : 1.f);
			bin->Write(&ticksPerSecond, sizeof(f32));

			for (int32 k = 0; k < channelsSize; k++)
			{
				const Channel &ch = animation.Channels[k];

				// Node Name
				int32 channelNameSize = (int32)ch.NodeName.size();
				bin->Write(&channelNameSize, sizeof(int32));
				if (channelNameSize > 0)
					bin->Write((void*)ch.NodeName.c_str(), sizeof(char)*channelNameSize);

				// Positions - field by field, matching Load()'s reads,
				// rather than blitting the structs: PositionData and
				// friends are only tightly packed by luck of the current
				// field order, and a blit would silently write padding
				// bytes into the file the moment that changes.
				int32 positionSize = (int32)ch.positions.size();
				bin->Write(&positionSize, sizeof(int32));
				for (int32 l = 0; l < positionSize; l++)
				{
					f32 t = ch.positions[l].Time;
					Vec3 v = ch.positions[l].Pos;
					bin->Write(&t, sizeof(f32));
					bin->Write(&v, sizeof(Vec3));
				}

				// Rotations
				int32 rotationSize = (int32)ch.rotations.size();
				bin->Write(&rotationSize, sizeof(int32));
				for (int32 l = 0; l < rotationSize; l++)
				{
					f32 t = ch.rotations[l].Time;
					Quaternion q = ch.rotations[l].Rot;
					bin->Write(&t, sizeof(f32));
					bin->Write(&q, sizeof(Quaternion));
				}

				// Scales
				int32 scalingSize = (int32)ch.scales.size();
				bin->Write(&scalingSize, sizeof(int32));
				for (int32 l = 0; l < scalingSize; l++)
				{
					f32 t = ch.scales[l].Time;
					Vec3 v = ch.scales[l].Scale;
					bin->Write(&t, sizeof(f32));
					bin->Write(&v, sizeof(Vec3));
				}
			}
		}

		bin->Close();
		delete bin;
		return true;
	}

	bool AnimationLoader::Load(const std::string &Filename)
	{
		// Load Animations
		BinaryFile* bin = new BinaryFile();
		bin->Open(Filename.c_str(), 'r');

		int32 animationsSize;
		bin->Read(&animationsSize, sizeof(int32));
		for (int32 i = 0; i < animationsSize; i++)
		{
			Animation animation;

			// Animation Name
			int32 nameSize;
			bin->Read(&nameSize, sizeof(int32));
			animation.AnimationName.resize(nameSize);
			bin->Read(&animation.AnimationName[0], sizeof(char)*nameSize);

			// Channels
			int32 channelsSize;
			bin->Read(&channelsSize, sizeof(int32));

			// Duration
			bin->Read(&animation.Duration, sizeof(f32));

			// Ticks Per Second
			bin->Read(&animation.TicksPerSecond, sizeof(f32));

			for (int32 k = 0; k < channelsSize; k++)
			{
				// Channel
				Channel ch;

				// Node Name
				int32 channelNameSize;
				bin->Read(&channelNameSize, sizeof(int32));
				ch.NodeName.resize(channelNameSize);
				bin->Read(&ch.NodeName[0], sizeof(char)*channelNameSize);

				// Positions
				int32 positionSize;
				bin->Read(&positionSize, sizeof(int32));
				ch.positions.resize(positionSize);
				for (int32 l = 0; l < positionSize; l++)
				{
					bin->Read(&ch.positions[l].Time, sizeof(f32));
					bin->Read(&ch.positions[l].Pos, sizeof(Vec3));
					ch.positions[l].Time /= animation.TicksPerSecond;
				}

				// Rotations
				int32 rotationSize;
				bin->Read(&rotationSize, sizeof(int32));
				ch.rotations.resize(rotationSize);
				for (int32 l = 0; l < rotationSize; l++)
				{
					bin->Read(&ch.rotations[l].Time, sizeof(f32));
					bin->Read(&ch.rotations[l].Rot, sizeof(Quaternion));
					ch.rotations[l].Time /= animation.TicksPerSecond;
				}

				// Scales
				int32 scalingSize;
				bin->Read(&scalingSize, sizeof(int32));
				ch.scales.resize(scalingSize);
				for (int32 l = 0; l < scalingSize; l++)
				{
					bin->Read(&ch.scales[l].Time, sizeof(f32));
					bin->Read(&ch.scales[l].Scale, sizeof(Vec3));
					ch.scales[l].Time /= animation.TicksPerSecond;
				}

				animation.Channels.push_back(ch);

			}

			animation.Duration /= animation.TicksPerSecond;

			animation.TicksPerSecond = 1;

			// Add Animation
			animations.push_back(animation);
		}

		bin->Close();
		delete bin;

		return true;
	}

}