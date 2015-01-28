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

    bool AnimationLoader::Load(const std::string &Filename)
    {
        // Load Animations
        BinaryFile* bin = new BinaryFile();
        bin->Open(Filename.c_str(),'r');

        int32 animationsSize;
        bin->Read(&animationsSize, sizeof(int32));
        for (int32 i=0;i<animationsSize;i++)
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

            for (uint32 k=0;k<channelsSize;k++)
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
                for (uint32 l=0;l<positionSize;l++)
                {
                    bin->Read(&ch.positions[l].Time, sizeof(f32));
                    bin->Read(&ch.positions[l].Pos, sizeof(Vec3));
                    ch.positions[l].Time /= animation.TicksPerSecond;
                }
                
                // Rotations
                int32 rotationSize;
                bin->Read(&rotationSize, sizeof(int32));
                ch.rotations.resize(rotationSize);
                for (uint32 l=0;l<rotationSize;l++)
                {
                    bin->Read(&ch.rotations[l].Time, sizeof(f32));
                    bin->Read(&ch.rotations[l].Rot, sizeof(Quaternion));
                    ch.rotations[l].Time /= animation.TicksPerSecond;
                }

                // Scales
                int32 scalingSize;
                bin->Read(&scalingSize, sizeof(int32));
                ch.scales.resize(scalingSize);
                for (uint32 l=0;l<scalingSize;l++)
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