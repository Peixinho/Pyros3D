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
#include "../../../Core/Math/Quaternion.h"
#include "../../../Core/Math/Matrix.h"
#include "../../../Core/Math/vec4.h"
#include "../../../Core/Math/vec3.h"
#include "../../../Core/Math/vec2.h"
#include "../../../Utils/StringIDs/StringID.hpp"

// Assimp Lib
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>

namespace Fishy3D {   
    
    // Bone Structure
    struct Bone {
        std::string name;
        short int self;
        short int parent;
        vec3 pos;
        Quaternion rot;
        vec3 scale;        
        Matrix bindPoseMat;
        bool skinned;
    };
    
    // Store Each Submesh Material Properties
    struct MaterialProperties
    {
        // Material ID
        unsigned int id;
        // Material Name
        std::string Name;
        // Properties
        vec4 Color;
        vec4 Specular;
        vec4 Ambient;
        vec4 Emissive;
        bool WireFrame;
        bool Twosided;
        float Opacity;
        float Shininess;
        float ShininessStrength;
        // textures        
        std::string colorMap;
        std::string specularMap;
        std::string normalMap;
        // flags
        bool haveColor;
        bool haveSpecular;
        bool haveAmbient;
        bool haveEmissive;        
        bool haveColorMap;
        bool haveSpecularMap;
        bool haveNormalMap;
        
        MaterialProperties() : haveColor(false), haveSpecular(false), haveAmbient(false),haveColorMap(false),haveEmissive(false),haveNormalMap(false),haveSpecularMap(false), Opacity(1.0f), Shininess(0.0f), ShininessStrength(0.0f) {}
    };
    
    // Sub Mesh Structure
    struct SubMesh {                
        
        // submesh ID
        unsigned int ID;
        
        // submesh Name
        std::string Name;
        
        // index
        std::vector<unsigned int> tIndex;
        
        // vectors to save data
        bool hasVertex, hasNormal, hasTangentBitangent, hasTexcoord, hasVertexColor;
        std::vector<vec3> tVertex,tNormal,tTangent,tBitangent;
        std::vector<vec2> tTexcoord;
        std::vector<vec4> tVertexColor;
        
        // skeleton
        bool hasBones;
        std::vector<vec4> tBonesID;
        std::vector<vec4> tBonesWeight;
        
        // Map Bone IDs
        // Original ID - Internal ID
        std::map<short int, short int> MapBoneIDs;
        std::map<short int, Matrix> BoneOffsetMatrix;
        
        // Materials
        unsigned int materialID;
        
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
            unsigned GetBoneID(const std::string &BoneName);
            unsigned GetBoneID(const StringID &BoneID);                        
            
            // Debug Skeleton to String
            void DebugSkeleton();
            
        private:
            
            // assimp model
            const aiScene* assimp_model;
           
            // bone count
            unsigned int boneCount;       

            // aux function to show sekeleton
            void GetBoneChilds(std::map<StringID, Bone> Skeleton, const int &id, const unsigned int &iterations);
            // aux function to construct skeleton            
            void GetBone(aiNode *bone, const short int &parentID = -1); 
    };

}

#endif	/* MODELLOADER_H */