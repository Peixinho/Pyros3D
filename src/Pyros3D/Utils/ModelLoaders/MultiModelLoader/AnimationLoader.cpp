//============================================================================
// Name        : AnimationLoader.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads model animation based on Assimp
//============================================================================

#include <Pyros3D/Utils/ModelLoaders/MultiModelLoader/AnimationLoader.h>
#include <cstdlib>
#include <ctime>
#include <cstring>

namespace p3d {

	AnimationLoader::AnimationLoader() {}

	AnimationLoader::~AnimationLoader() {}

	namespace {
		const char P3DA_MAGIC[4] = { 'P', '3', 'D', 'A' };

		// Writes the three interpolation fields every key type shares.
		void WriteKeyInterp(BinaryFile* bin, const uchar mode, const f32 inTan, const f32 outTan)
		{
			uchar m = mode;
			f32 i = inTan, o = outTan;
			bin->Write(&m, sizeof(uchar));
			bin->Write(&i, sizeof(f32));
			bin->Write(&o, sizeof(f32));
		}

		// v0 files have no interpolation fields; leaving the defaults alone
		// reproduces the linear sampling those files were authored against.
		void ReadKeyInterp(BinaryFile* bin, const uint32 version, uchar &mode, f32 &inTan, f32 &outTan)
		{
			if (version < 1) return;
			bin->Read(&mode, sizeof(uchar));
			bin->Read(&inTan, sizeof(f32));
			bin->Read(&outTan, sizeof(f32));
		}
	}

	std::string AnimationLoader::GenerateGuid()
	{
		// rand() is plenty here: these only have to be distinct within one
		// project's clip list, not unguessable.
		static bool seeded = false;
		if (!seeded) { srand((unsigned)time(NULL)); seeded = true; }

		std::string guid;
		guid.resize(16);
		for (int32 i = 0; i < 16; i++)
			guid[i] = (char)(rand() & 0xFF);
		return guid;
	}

	bool AnimationLoader::Save(const std::string &Filename, const std::vector<Animation> &animations)
	{
		BinaryFile* bin = new BinaryFile();
		if (!bin->Open(Filename.c_str(), 'w'))
		{
			delete bin;
			return false;
		}

		// Header. Always written at the current version - there is no reason
		// to author a v0 file, and Load() still reads the old ones.
		bin->Write((void*)P3DA_MAGIC, sizeof(char) * 4);
		uint32 version = P3DA_VERSION;
		bin->Write(&version, sizeof(uint32));

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

			// Guid - fixed 16 bytes, minted here if the clip came from a v0
			// file or was built in memory, so that saving once is enough to
			// give every clip a stable identity.
			std::string guid = animation.Guid;
			if (guid.size() != 16) guid = GenerateGuid();
			bin->Write((void*)guid.c_str(), sizeof(char) * 16);

			// Flags / authored fps
			uint32 flags = animation.Flags;
			bin->Write(&flags, sizeof(uint32));
			f32 authoredFps = animation.AuthoredFps;
			bin->Write(&authoredFps, sizeof(f32));

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
					WriteKeyInterp(bin, ch.positions[l].Mode, ch.positions[l].InTangent, ch.positions[l].OutTangent);
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
					WriteKeyInterp(bin, ch.rotations[l].Mode, ch.rotations[l].InTangent, ch.rotations[l].OutTangent);
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
					WriteKeyInterp(bin, ch.scales[l].Mode, ch.scales[l].InTangent, ch.scales[l].OutTangent);
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
		if (!bin->Open(Filename.c_str(), 'r'))
		{
			echo(std::string("ERROR: AnimationLoader - couldn't open ") + Filename);
			delete bin;
			return false;
		}

		// Version sniff. A v0 file opens directly with its clip count, so the
		// first four bytes are a small positive int32 and cannot spell
		// "P3DA". BinaryFile cannot seek backwards, but it does not need to:
		// when the magic does not match, those four bytes ARE the v0 clip
		// count, so they get reinterpreted rather than re-read.
		uint32 version = 0;
		int32 animationsSize = 0;
		char magic[4] = { 0, 0, 0, 0 };
		bin->Read(&magic[0], sizeof(char) * 4);
		if (magic[0] == P3DA_MAGIC[0] && magic[1] == P3DA_MAGIC[1]
			&& magic[2] == P3DA_MAGIC[2] && magic[3] == P3DA_MAGIC[3])
		{
			bin->Read(&version, sizeof(uint32));
			if (version > P3DA_VERSION)
			{
				echo("ERROR: AnimationLoader - " + Filename + " is version "
					+ std::to_string(version) + ", this build understands up to "
					+ std::to_string(P3DA_VERSION));
				bin->Close();
				delete bin;
				return false;
			}
			bin->Read(&animationsSize, sizeof(int32));
		}
		else memcpy(&animationsSize, magic, sizeof(int32));
		for (int32 i = 0; i < animationsSize; i++)
		{
			Animation animation;

			// Animation Name
			int32 nameSize;
			bin->Read(&nameSize, sizeof(int32));
			animation.AnimationName.resize(nameSize);
			if (nameSize > 0)
				bin->Read(&animation.AnimationName[0], sizeof(char)*nameSize);

			if (version >= 1)
			{
				animation.Guid.resize(16);
				bin->Read(&animation.Guid[0], sizeof(char) * 16);
				bin->Read(&animation.Flags, sizeof(uint32));
				bin->Read(&animation.AuthoredFps, sizeof(f32));
			}

			// Channels
			int32 channelsSize;
			bin->Read(&channelsSize, sizeof(int32));

			// Duration
			bin->Read(&animation.Duration, sizeof(f32));

			// Ticks Per Second
			bin->Read(&animation.TicksPerSecond, sizeof(f32));
			// Guard the divisions below - a zero here turned every key time
			// and the duration into inf/NaN rather than failing.
			if (animation.TicksPerSecond <= 0.f) animation.TicksPerSecond = 1.f;

			for (int32 k = 0; k < channelsSize; k++)
			{
				// Channel
				Channel ch;

				// Node Name
				int32 channelNameSize;
				bin->Read(&channelNameSize, sizeof(int32));
				ch.NodeName.resize(channelNameSize);
				if (channelNameSize > 0)
					bin->Read(&ch.NodeName[0], sizeof(char)*channelNameSize);

				// Positions
				int32 positionSize;
				bin->Read(&positionSize, sizeof(int32));
				ch.positions.resize(positionSize);
				for (int32 l = 0; l < positionSize; l++)
				{
					bin->Read(&ch.positions[l].Time, sizeof(f32));
					bin->Read(&ch.positions[l].Pos, sizeof(Vec3));
					ReadKeyInterp(bin, version, ch.positions[l].Mode, ch.positions[l].InTangent, ch.positions[l].OutTangent);
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
					ReadKeyInterp(bin, version, ch.rotations[l].Mode, ch.rotations[l].InTangent, ch.rotations[l].OutTangent);
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
					ReadKeyInterp(bin, version, ch.scales[l].Mode, ch.scales[l].InTangent, ch.scales[l].OutTangent);
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