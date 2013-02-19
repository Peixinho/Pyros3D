//============================================================================
// Name        : AnimationManager.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Animation Manager
//============================================================================

#include "AnimationManager.h"

namespace Fishy3D {

    AnimationManager::AnimationManager() {
    }

    AnimationManager::~AnimationManager() {
        // Destroy
        if (animationLoader.Get()!=NULL) animationLoader.Dispose();        
    }
    
    void AnimationManager::LoadAnimation(const std::string& AnimationFile, const SuperSmartPointer<IComponent>& RenderingComponent)
    {
        animationLoader = SuperSmartPointer<AnimationLoader> (new AnimationLoader());
        animationLoader->Load(AnimationFile);
        
        renderingComponent = RenderingComponent;
        RenderingModelComponent* rcomp = static_cast<RenderingModelComponent*> (renderingComponent.Get());
        
        // Get Skeleton
        skeleton = rcomp->GetSkeleton();
        
        // Create Vectors to Store Values
        Bones =  std::vector<Matrix>(rcomp->GetSkeleton().size());
        boneTransformation = std::vector<Matrix>(rcomp->GetSkeleton().size());
        
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

        // Get RComp
        RenderingModelComponent* rcomp = static_cast<RenderingModelComponent*> (renderingComponent.Get());
        
        // Copy Bind Pose Matrix
        for (std::map<StringID,Bone>::iterator a=skeleton.begin();a!=skeleton.end();a++)
        {
            // Set Bones Transformation based on Bone ID
            boneTransformation[(*a).second.self] = (*a).second.bindPoseMat; // Copy BindPose
            MapLocalToGlobalIDs[(*a).second.self] = (*a).first; // Save Map
        }
        
        // Transform bones from animation
        for (int a=0;a<Anim.Channels.size();a++)
        {
            Channel ch = Anim.Channels[a];
            vec3 curPosition, curScale;
            Quaternion curRotation;
            
            size_t posIndex = 0;
            while(1)
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
            
            size_t scaleIndex = 0;
            while( 1 )
            {
                if( scaleIndex +1 >= ch.scales.size() )
                break;

                if( ch.scales[scaleIndex + 1].Time > time )
                break;
                scaleIndex++;
            }            
            curScale = ch.scales[scaleIndex].Scale;
            
            Matrix trafo = curRotation.ConvertToMatrix();
            trafo.Translate(curPosition);
            // Not in Use
            // trafo.Scale(curScale);
            
            unsigned id = MakeStringID(ch.NodeName);
            boneTransformation[skeleton[id].self] = trafo;
        }
        
        // Multiply bones with its parent        
        for (std::map<StringID,Bone>::iterator a=skeleton.begin();a!=skeleton.end();a++)
        {
            Bones[(*a).second.self] = GetParentMatrix((*a).second.parent, boneTransformation) 
                                       * boneTransformation[(*a).second.self];
        }
        
        // Send SubMesh Bones to Material
        for (std::vector<SuperSmartPointer <IComponent> >::iterator i = rcomp->GetSubComponents().begin(); i != rcomp->GetSubComponents().end(); i++)
        {
            // Casting the Sub Mesh
            RenderingSubMeshComponent* sub = static_cast<RenderingSubMeshComponent*> ((*i).Get());
            // Creates a vector of Matrices
            std::vector<Matrix> uBones = std::vector<Matrix> (sub->MapBoneIDs.size());
            
            for (std::map<short int, short int>::iterator k = sub->MapBoneIDs.begin();k!=sub->MapBoneIDs.end();k++)
            {
                // Set list of Bones Matrices
                uBones[(*k).second] = Bones[(*k).first] * sub->BoneOffsetMatrix[(*k).first];
            }
            // Send Skinning Bones Matrices Specific for this Mesh
            sub->SetSkinningBones(uBones);
        }
    }
    
    // Get Parent Matrix
    Matrix AnimationManager::GetParentMatrix(const unsigned &id, std::vector<Matrix> &bones)
    {
        if(MapLocalToGlobalIDs.find(id)!=MapLocalToGlobalIDs.end())
        {
            // Keep a copy of Skeleton ID
            unsigned boneOnSkeleton = MapLocalToGlobalIDs[id];
            // Keep a copy of Bone internal ID
            short int boneID = skeleton[boneOnSkeleton].self;
            
            if (skeleton[boneOnSkeleton].parent!=-1)
            {
                return GetParentMatrix(skeleton[boneOnSkeleton].parent, bones) * bones[boneID];
            }
            return bones[boneID];
        }
        return Matrix();       
    }
    
    Matrix AnimationManager::GetBoneMatrix(const unsigned id)
    {
        return Bones[skeleton[id].self];
    }
}