//============================================================================
// Name        : ModelLoader.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Model Loader Interface
//============================================================================

#ifndef IMODELLOADER_H
#define	IMODELLOADER_H

#include "../../Core/Math/Math.h"
#include "../../AssetManager/Assets/Renderable/Renderables.h"
#include <string.h>
#include <fstream>
#include <iostream>
#include <vector>

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
        // Offset Matrix
        std::map<int32, Matrix> BoneOffsetMatrix;
        
        // Materials
        uint32 materialID;
        
    };

	struct MaterialProperties;

    class IModelLoader {
        
        public:
            
            IModelLoader();

            virtual ~IModelLoader();
            
            virtual void Load(const std::string &Filename) = 0;

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

            // aux function to show sekeleton
            void GetBoneChilds(std::map<StringID, Bone> Skeleton, const int32 &id, const uint32 &iterations);

        protected:
            
            std::string LoadFile(const std::string &Filename);
            
    };

}

#endif	/* IMODELLOADER_H */