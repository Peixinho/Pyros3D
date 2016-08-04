//============================================================================
// Name        : AssimpAnimationImporter.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads  model formats based on Assimp
//============================================================================

#include "AssimpAnimationImporter.h"

namespace p3d {

    AssimpAnimationImporter::AssimpAnimationImporter() {}

    AssimpAnimationImporter::~AssimpAnimationImporter() {}

    bool AssimpAnimationImporter::Load(const std::string& Filename)
    {
        return Load(Filename,-1,-1);
    }

    bool AssimpAnimationImporter::Load(const std::string& Filename, const int32 init, const int32 end, const std::string &AnimationName)
    {

        // Assimp Importer
        Assimp::Importer Importer;

        assimp_model = Importer.ReadFile(Filename.c_str(),aiProcessPreset_TargetRealtime_Fast | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

        if (!assimp_model)
        {
            echo("Failed To Import Model: " + std::string(Filename));
            return false;
        } else {

            int32 positionsInit         = init;
            int32 scalesInit            = init;
            int32 rotationsInit         = init;
            int32 positionsEnd          = end;
            int32 scalesEnd             = end;
            int32 rotationsEnd          = end;
            int32 positionsCount        = -1;
            int32 rotationsCount        = -1;
            int32 scalesCount           = -1;
            f32 positionsTime           = 0.f;
            f32 positionsTimeOffset     = 0.f;
            f32 rotationsTime           = 0.f;
            f32 rotationsTimeOffset     = 0.f;
            f32 scalesTime              = 0.f;
            f32 scalesTimeOffset        = 0.f;
            f32 positionsTimeOffsetEnd  = 0.f;
            f32 rotationsTimeOffsetEnd  = 0.f;
            f32 scalesTimeOffsetEnd     = 0.f;
            
            // Get Animations
            if (assimp_model->HasAnimations()==true) 
            {
                
                for (uint32 i=0;i<assimp_model->mNumAnimations;i++)
                {
                    Animation animation;
                    
                    // Set Animation Name
                    if (positionsInit>-1 && positionsEnd>-1 && AnimationName.size()>0) animation.AnimationName = AnimationName;
                    else animation.AnimationName = assimp_model->mAnimations[i]->mName.data;
                    // Set Animation Duration
                    animation.Duration = (f32)assimp_model->mAnimations[i]->mDuration;                    
                    // Set Ticks Per Second
                    animation.TicksPerSecond = (f32)assimp_model->mAnimations[i]->mTicksPerSecond;
                    
                    // allocate std::vector space
                    animation.Channels.reserve(assimp_model->mAnimations[i]->mNumChannels);
                    for (uint32 k=0;k<assimp_model->mAnimations[i]->mNumChannels;k++)
                    {
                        Channel channel;
                        
                        // Set Channel Node Name
                        channel.NodeName = assimp_model->mAnimations[i]->mChannels[k]->mNodeName.data;
                        
                        // allocate space
                        channel.positions.reserve(assimp_model->mAnimations[i]->mChannels[k]->mNumPositionKeys);
                        channel.rotations.reserve(assimp_model->mAnimations[i]->mChannels[k]->mNumRotationKeys);
                        channel.scales.reserve(assimp_model->mAnimations[i]->mChannels[k]->mNumScalingKeys);
                        
                        // Get Positions, Rotations and Scales

                        // Positions
                        if (positionsInit>-1 && positionsEnd>-1) 
                        {
                            positionsCount = -1;
                            positionsTimeOffset = 0.f;
                        }
                        for (uint32 l=0;l<assimp_model->mAnimations[i]->mChannels[k]->mNumPositionKeys;l++)
                        {
                            if (positionsInit>-1 && positionsEnd>-1)
                            {
                                positionsCount++;
                                if (positionsCount>=positionsInit && positionsCount<positionsEnd)
                                {
                                    Vec3 Pos;
                                    Pos.x = assimp_model->mAnimations[i]->mChannels[k]->mPositionKeys[l].mValue.x;
                                    Pos.y = assimp_model->mAnimations[i]->mChannels[k]->mPositionKeys[l].mValue.y;
                                    Pos.z = assimp_model->mAnimations[i]->mChannels[k]->mPositionKeys[l].mValue.z;
                                    positionsTime = (f32)assimp_model->mAnimations[i]->mChannels[k]->mPositionKeys[l].mTime - positionsTimeOffset;
                                    channel.positions.push_back(PositionData(positionsTime,Pos));
                                } else {
                                    if (positionsCount<positionsInit) 
                                        positionsTimeOffset=(f32)assimp_model->mAnimations[i]->mChannels[k]->mPositionKeys[l].mTime;
                                    else if (positionsCount>=positionsEnd) 
                                        positionsTimeOffsetEnd=(f32)assimp_model->mAnimations[i]->mChannels[k]->mPositionKeys[l].mTime-positionsTimeOffset;
                                }
                            } else {
                                Vec3 Pos;
                                Pos.x = assimp_model->mAnimations[i]->mChannels[k]->mPositionKeys[l].mValue.x;
                                Pos.y = assimp_model->mAnimations[i]->mChannels[k]->mPositionKeys[l].mValue.y;
                                Pos.z = assimp_model->mAnimations[i]->mChannels[k]->mPositionKeys[l].mValue.z;
                                channel.positions.push_back(PositionData((f32)assimp_model->mAnimations[i]->mChannels[k]->mPositionKeys[l].mTime,Pos));
                            }
                        }

                        // Rotations
                        if (rotationsInit>-1 && rotationsEnd>-1) 
                        {
                            rotationsCount = -1;
                            rotationsTimeOffset = 0.f;
                        }
                        for (uint32 l=0;l<assimp_model->mAnimations[i]->mChannels[k]->mNumRotationKeys;l++)
                        {
                            if (rotationsInit>-1 && rotationsEnd>-1)
                            {
                                rotationsCount++;
                                if (rotationsCount>=rotationsInit && rotationsCount<rotationsEnd)
                                {
                                    Quaternion Rot;
                                    Rot.w = assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mValue.w;
                                    Rot.x = assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mValue.x;
                                    Rot.y = assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mValue.y;
                                    Rot.z = assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mValue.z;
                                    rotationsTime = (f32)assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mTime-rotationsTimeOffset;
                                    channel.rotations.push_back(RotationData(rotationsTime,Rot));
                                } else {
                                    if (rotationsCount<rotationsInit) 
                                        rotationsTimeOffset=(f32)assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mTime;
                                    else if (rotationsCount>=rotationsEnd) 
                                        rotationsTimeOffsetEnd=(f32)assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mTime-rotationsTimeOffset;
                                }
                            } else {
                                Quaternion Rot;
                                Rot.w = assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mValue.w;
                                Rot.x = assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mValue.x;
                                Rot.y = assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mValue.y;
                                Rot.z = assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mValue.z;
                                channel.rotations.push_back(RotationData((f32)assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mTime,Rot));
                            }
                        }

                        // Scales
                        if (scalesInit>-1 && scalesEnd>-1) 
                        {
                            scalesCount = -1;
                            scalesTimeOffset = 0.f;
                        }
                        for (uint32 l=0;l<assimp_model->mAnimations[i]->mChannels[k]->mNumScalingKeys;l++)
                        {
                            if (scalesInit>-1 && scalesEnd>-1)
                            {
                                scalesCount++;
                                if (scalesCount>=scalesInit && scalesCount<scalesEnd)
                                {
                                    Vec3 Scale;
                                    Scale.x = assimp_model->mAnimations[i]->mChannels[k]->mScalingKeys[l].mValue.x;
                                    Scale.y = assimp_model->mAnimations[i]->mChannels[k]->mScalingKeys[l].mValue.y;
                                    Scale.z = assimp_model->mAnimations[i]->mChannels[k]->mScalingKeys[l].mValue.z;
                                    scalesTime = (f32)assimp_model->mAnimations[i]->mChannels[k]->mScalingKeys[l].mTime-scalesTimeOffset;
                                    channel.scales.push_back(ScalingData(scalesTime,Scale));
                                } else {
                                    if (scalesCount<scalesInit) 
                                        scalesTimeOffset=(f32)assimp_model->mAnimations[i]->mChannels[k]->mScalingKeys[l].mTime;
                                    else if (scalesCount>=scalesEnd) 
                                        scalesTimeOffsetEnd=(f32)assimp_model->mAnimations[i]->mChannels[k]->mScalingKeys[l].mTime-scalesTimeOffset;
                                }
                            } else {
                                Vec3 Scale;
                                Scale.x = assimp_model->mAnimations[i]->mChannels[k]->mScalingKeys[l].mValue.x;
                                Scale.y = assimp_model->mAnimations[i]->mChannels[k]->mScalingKeys[l].mValue.y;
                                Scale.z = assimp_model->mAnimations[i]->mChannels[k]->mScalingKeys[l].mValue.z;
                                channel.scales.push_back(ScalingData((f32)assimp_model->mAnimations[i]->mChannels[k]->mScalingKeys[l].mTime,Scale));
                            }
                        }
                        
                        // add channel to animation
                        animation.Channels.push_back(channel);
                        
                    }
                    
                    if (positionsInit>-1 && positionsEnd>-1) animation.Duration = animation.Duration - (positionsTimeOffset+positionsTimeOffsetEnd) + positionsTime;
                    
                    // add animation
                    animations.push_back(animation);
                    
                }
                
            } else {
                echo("No Animations Found in Model: " + std::string(Filename));
            }
            return true;
        }
    }

    bool AssimpAnimationImporter::ConvertToPyrosFormat(const std::string &Filename)
    {
        BinaryFile* bin = new BinaryFile();
        if (bin->Open(Filename.c_str(),'w'))
        {

        int32 animationsSize = animations.size();
        bin->Write(&animationsSize, sizeof(int32));
        for (int32 i=0;i<animationsSize;i++)
        {
            // Animation Name
            int32 nameSize = animations[i].AnimationName.size();
            bin->Write(&nameSize, sizeof(int32));
            bin->Write(animations[i].AnimationName.c_str(), sizeof(char)*nameSize);

            // Channels
            int32 channelsSize = animations[i].Channels.size();
            bin->Write(&channelsSize, sizeof(int32));

            // Duration
            bin->Write(&animations[i].Duration, sizeof(f32));
            // Ticks Per Second
            bin->Write(&animations[i].TicksPerSecond, sizeof(f32));

            for (int32 k=0;k<channelsSize;k++)
            {
                // Node Name
                int32 channelNameSize = animations[i].Channels[k].NodeName.size();
                bin->Write(&channelNameSize, sizeof(int32));
                bin->Write(animations[i].Channels[k].NodeName.c_str(), sizeof(char)*channelNameSize);

                // Positions
                int32 positionSize = animations[i].Channels[k].positions.size();
                bin->Write(&positionSize, sizeof(int32));
                bin->Write(&animations[i].Channels[k].positions[0], sizeof(PositionData)*positionSize);

                // Rotations
                int32 rotationSize = animations[i].Channels[k].rotations.size();
                bin->Write(&rotationSize, sizeof(int32));
                bin->Write(&animations[i].Channels[k].rotations[0], sizeof(RotationData)*rotationSize);

                // Scales
                int32 scalingSize = animations[i].Channels[k].scales.size();
                bin->Write(&scalingSize, sizeof(int32));
                bin->Write(&animations[i].Channels[k].scales[0], sizeof(ScalingData)*scalingSize);
            }
        }

        bin->Close();
        delete bin;
        return true;

        } else return false;
    }
}