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

    AnimationLoader::~AnimationLoader() 
    {

    }

    void AnimationLoader::Load(const std::string& Filename)
    {

        assimp_model = aiImportFile(Filename.c_str(),aiProcessPreset_TargetRealtime_Quality | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs);

        if (!assimp_model)
        {
            std::cout << "Failed To Import Model: " << Filename << std::endl;
        } else {
            
            // Get Animations
            if (assimp_model->HasAnimations()==true) 
            {
                
                // allocate std::vector space
                animations.reserve(assimp_model->mNumAnimations);
                for (uint32 i=0;i<assimp_model->mNumAnimations;i++)
                {
                    Animation animation;
                    
                    // Set Animation Name
                    animation.AnimationName = assimp_model->mAnimations[i]->mName.data;
                    // Set Animation Duration
                    animation.Duration = assimp_model->mAnimations[i]->mDuration;                    
                    // Set Ticks Per Second
                    animation.TicksPerSecond = assimp_model->mAnimations[i]->mTicksPerSecond;
                    
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
                        for (uint32 l=0;l<assimp_model->mAnimations[i]->mChannels[k]->mNumPositionKeys;l++)
                        {
                            Vec3 Pos;
                            Pos.x = assimp_model->mAnimations[i]->mChannels[k]->mPositionKeys[l].mValue.x;
                            Pos.y = assimp_model->mAnimations[i]->mChannels[k]->mPositionKeys[l].mValue.y;
                            Pos.z = assimp_model->mAnimations[i]->mChannels[k]->mPositionKeys[l].mValue.z;
                            
                            channel.positions.push_back(PositionData(assimp_model->mAnimations[i]->mChannels[k]->mPositionKeys[l].mTime,Pos));
                        }
                        for (uint32 l=0;l<assimp_model->mAnimations[i]->mChannels[k]->mNumRotationKeys;l++)
                        {
                            Quaternion Rot;
                            Rot.w = assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mValue.w;
                            Rot.x = assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mValue.x;
                            Rot.y = assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mValue.y;
                            Rot.z = assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mValue.z;
                            
                            channel.rotations.push_back(RotationData(assimp_model->mAnimations[i]->mChannels[k]->mRotationKeys[l].mTime,Rot));
                        }
                        for (uint32 l=0;l<assimp_model->mAnimations[i]->mChannels[k]->mNumScalingKeys;l++)
                        {
                            Vec3 Scale;
                            Scale.x = assimp_model->mAnimations[i]->mChannels[k]->mScalingKeys[l].mValue.x;
                            Scale.y = assimp_model->mAnimations[i]->mChannels[k]->mScalingKeys[l].mValue.y;
                            Scale.z = assimp_model->mAnimations[i]->mChannels[k]->mScalingKeys[l].mValue.z;
                            
                            channel.scales.push_back(ScalingData(assimp_model->mAnimations[i]->mChannels[k]->mScalingKeys[l].mTime,Scale));
                        }
                        
                        // add channel to animation
                        animation.Channels.push_back(channel);
                        
                    }
                    
                    // add animation
                    animations.push_back(animation);
                    
                }
                
            } else {
                std::cout << "No Animations Found in Model: " << Filename << std::endl;
            }
        }
    }
}