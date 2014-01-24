//============================================================================
// Name        : AnimationLoader.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads model animation based on Assimp
//============================================================================

#include "AnimationLoader.h"

namespace p3d {

    AnimationLoader::AnimationLoader() {}

    AnimationLoader::~AnimationLoader() {}

    void AnimationLoader::Load(const std::string& Filename)
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

            for (int32 k=0;k<channelsSize;k++)
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
                bin->Read(&ch.positions[0], sizeof(PositionData)*positionSize);

                // Rotations
                int32 rotationSize;
                bin->Read(&rotationSize, sizeof(int32));
                ch.rotations.resize(rotationSize);
                bin->Read(&ch.rotations[0], sizeof(RotationData)*rotationSize);

                // Scales
                int32 scalingSize;
                bin->Read(&scalingSize, sizeof(int32));
                ch.scales.resize(scalingSize);
                bin->Read(&ch.scales[0], sizeof(ScalingData)*scalingSize);

                animation.Channels.push_back(ch);
            }
            // Duration
            bin->Read(&animation.Duration, sizeof(uint32));

            // Ticks Per Second
            bin->Read(&animation.TicksPerSecond, sizeof(f64));

            // Add Animation
            animations.push_back(animation);
        }

        bin->Close();
        delete bin;
    }
}