//============================================================================
// Name        : ModelLoader.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads  model formats based on Assimp
//============================================================================

#ifndef MODELLOADER_H
#define	MODELLOADER_H

#include <map>
#include <vector>
#include "../IModelLoader.h"
#include "../../../Core/Math/Math.h"
#include "../../../Ext/StringIDs/StringID.hpp"
#include "../../../AssetManager/Assets/Renderable/Renderables.h"

// Assimp Lib
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>
#include <assimp/Importer.hpp> 


namespace p3d {   
    
    // Bone Structure
    struct Bone {
        std::string name;
        int32 self;
        int32 parent;
        Vec3 pos;
        Quaternion rot;
        Vec3 scale;        
        Matrix bindPoseMat;
        bool skinned;
    };
    
    // Sub Mesh Structure
    struct SubMesh {                
        
        // submesh ID
        uint32 ID;
        
        // submesh Name
        std::string Name;
        
        // index
        std::vector<uint32> tIndex;
        
        // vectors to save data
        bool hasVertex, hasNormal, hasTangentBitangent, hasTexcoord, hasVertexColor;
        std::vector<Vec3> tVertex,tNormal,tTangent,tBitangent;
        std::vector<Vec2> tTexcoord;
        std::vector<Vec4> tVertexColor;
        
        // skeleton
        bool hasBones;
        std::vector<Vec4> tBonesID;
        std::vector<Vec4> tBonesWeight;
        
        // Map Bone IDs
        // Original ID - Internal ID
        std::map<int32, int32> MapBoneIDs;
        std::map<int32, Matrix> BoneOffsetMatrix;
        
        // Materials
        uint32 materialID;
        
    };   
    
    class ModelLoader : public IModelLoader {
        public:

            ModelLoader();
            virtual ~ModelLoader();

            virtual void Load(const std::string &Filename);

            // list of all submeshes
            std::vector<SubMesh> subMeshes;
            // Skeleton
            std::map<StringID, Bone> skeleton;
            // Materials
            std::vector<MaterialProperties> materials;
            
            // Skinning
            uint32 GetBoneID(const std::string &BoneName);
            uint32 GetBoneID(const StringID &BoneID);                        
            
            // Debug Skeleton to String
            void DebugSkeleton();
            
        private:
            
            // assimp model
            const aiScene* assimp_model;
           
            // bone count
            uint32 boneCount;       

            // aux function to show sekeleton
            void GetBoneChilds(std::map<StringID, Bone> Skeleton, const int32 &id, const uint32 &iterations);
            // aux function to construct skeleton            
            void GetBone(aiNode *bone, const int32 &parentID = -1); 
    };

}

#endif	/* MODELLOADER_H */