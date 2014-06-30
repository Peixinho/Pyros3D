//============================================================================
// Name        : AnimationLoader.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads model animation - Emscripten Specific
//============================================================================

#include "../../../src/Pyros3D/Utils/ModelLoaders/MultiModelLoader/AnimationLoader.h"
#include "../../../src/Pyros3D/Utils/ModelLoaders/MultiModelLoader/AnimationLoader.cpp"
#include <stdio.h>

namespace p3d {

#ifdef EMSCRIPTEN

    bool AnimationLoader::Load(const std::string& Filename)
    {
        FILE *file;
        file = fopen(Filename.c_str(), "rb");
        std::vector<uchar>destination;
        int n_blocks = 1024;
        while(n_blocks != 0)
        {
            destination.resize(destination.size() + n_blocks);
            n_blocks = fread(&destination[destination.size() - n_blocks], 1, n_blocks, file);
        }
        fclose(file);

        BinaryFile* bin = new BinaryFile();
        bin->OpenFromMemory(&destination[0], destination.size());

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
                for (int l=0;l<positionSize;l++)
                {
                    bin->Read(&ch.positions[l].Time, sizeof(f64));
                    bin->Read(&ch.positions[l].Pos, sizeof(Vec3));
                }
                
                // Rotations
                int32 rotationSize;
                bin->Read(&rotationSize, sizeof(int32));
                ch.rotations.resize(rotationSize);
                for (int l=0;l<rotationSize;l++)
                {
                    bin->Read(&ch.rotations[l].Time, sizeof(f64));
                    bin->Read(&ch.rotations[l].Rot, sizeof(Quaternion));
                }

                // Scales
                int32 scalingSize;
                bin->Read(&scalingSize, sizeof(int32));
                ch.scales.resize(scalingSize);
                for (int l=0;l<scalingSize;l++)
                {
                    bin->Read(&ch.scales[l].Time, sizeof(f64));
                    bin->Read(&ch.scales[l].Scale, sizeof(Vec3));
                }

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

        return true;
    }

#endif
}
