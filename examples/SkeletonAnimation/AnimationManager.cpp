//============================================================================
// Name        : AnimationManager.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Animation Manager
//============================================================================

#include "AnimationManager.h"

namespace p3d {

    AnimationManager::AnimationManager() {
    }

    AnimationManager::~AnimationManager() {
        // Destroy
        if (animationLoader!=NULL) delete animationLoader;
    }
    
    void AnimationManager::LoadAnimation(const std::string& AnimationFile, RenderingComponent* Component)
    {
        animationLoader = new AnimationLoader();
        animationLoader->Load(AnimationFile);
        
        renderingComponent = Component;
        
        // Get Skeleton
        skeleton = Component->GetSkeleton();

        // Create Vectors to Store Values
        Bones = std::vector<Matrix>(skeleton.size());
        boneTransformation = std::vector<Matrix>(skeleton.size());
        
        // Copy Bind Pose Matrix
        for (std::map<StringID,Bone>::iterator a=skeleton.begin();a!=skeleton.end();a++)
        {
            // Set Bones Transformation based on Bone ID
            boneTransformation[(*a).second.self] = (*a).second.bindPoseMat; // Copy BindPose
            MapLocalToGlobalIDs[(*a).second.self] = (*a).first; // Save Map
        }
    }
    
    void AnimationManager::Update(const double& time)
    {
        Animation Anim = animationLoader->animations[0];

        // Copy Bind Pose Matrix
        for (std::map<StringID,Bone>::iterator a=skeleton.begin();a!=skeleton.end();a++)
        {
            // Set Bones Transformation based on Bone ID
            boneTransformation[(*a).second.self] = (*a).second.bindPoseMat; // Copy BindPose
            MapLocalToGlobalIDs[(*a).second.self] = (*a).first; // Save Map
        }
        
        // Transform bones from animation
        for (uint32 a=0;a<Anim.Channels.size();a++)
        {
            Channel ch = Anim.Channels[a];
            Vec3 curPosition, curScale;
            Quaternion curRotation;
            
            size_t posIndex = 0;
            while( 1 )
            {
                if (posIndex+1>=ch.positions.size())
                    break;
                if (ch.positions[posIndex+1].Time > time)
                    break;
                posIndex++;
            }
            curPosition = ch.positions[posIndex].Pos;
            
            size_t rotIndex = 0;
            while( 1 )
            {
                if( rotIndex +1 >= ch.rotations.size() )
                break;

                if( ch.rotations[rotIndex + 1].Time > time )
                break;
                rotIndex++;
            }
            curRotation = ch.rotations[rotIndex].Rot;
            
            // size_t scaleIndex = 0;
            // while( 1 )
            // {
            //     if( scaleIndex +1 >= ch.scales.size() )
            //     break;

            //     if( ch.scales[scaleIndex + 1].Time > time )
            //     break;
            //     scaleIndex++;
            // }            
            // curScale = ch.scales[scaleIndex].Scale;
            
            Matrix trafo = curRotation.ConvertToMatrix();
            trafo.Translate(curPosition);
            // Not in Use
            // trafo.Scale(curScale);
            
            uint32 id = MakeStringID(ch.NodeName);
            boneTransformation[skeleton[id].self] = trafo;
        }
        
        // Multiply bones with its parent        
        for (std::map<StringID,Bone>::iterator a=skeleton.begin();a!=skeleton.end();a++)
        {
            Bones[(*a).second.self] = GetParentMatrix((*a).second.parent, boneTransformation) * boneTransformation[(*a).second.self];
        }
        
        // Send SubMesh Bones to Material
        for (std::vector<RenderingMesh*>::iterator i = renderingComponent->GetMeshes().begin(); i != renderingComponent->GetMeshes().end(); i++)
        {
            // Creates a vector of Matrices
            std::vector<Matrix> uBones = std::vector<Matrix> ((*i)->MapBoneIDs.size());
            for (std::map<int32,int32>::iterator k = (*i)->MapBoneIDs.begin();k!=(*i)->MapBoneIDs.end();k++)
            {
                // Set list of Bones Matrices
                uBones[(*k).second] = Bones[(*k).first] * (*i)->BoneOffsetMatrix[(*k).first];
            }
            // Send Skinning Bones Matrices Specific for this Mesh
            (*i)->SkinningBones.resize(uBones.size());
            memcpy(&(*i)->SkinningBones[0],&uBones[0],uBones.size()*sizeof(Matrix));
        }
    }
    
    // Get Parent Matrix
    Matrix AnimationManager::GetParentMatrix(const int32 &id, const std::vector<Matrix> &bones)
    {
        if(MapLocalToGlobalIDs.find(id)!=MapLocalToGlobalIDs.end())
        {
            // Keep a copy of Skeleton ID
            uint32 boneOnSkeleton = MapLocalToGlobalIDs[id];
            // Keep a copy of Bone internal ID
            uint32 boneID = skeleton[boneOnSkeleton].self;

            if (skeleton[boneOnSkeleton].parent>-1)
            {
                return GetParentMatrix(skeleton[boneOnSkeleton].parent, bones) * bones[boneID];
            }
            return bones[boneID];
        }
        return Matrix();       
    }
    
    Matrix AnimationManager::GetBoneMatrix(const uint32 &id)
    {
        return Bones[skeleton[id].self];
    }
}