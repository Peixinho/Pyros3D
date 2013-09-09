//============================================================================
// Name        : ModelLoader.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads  model formats based on Assimp
//============================================================================

#include "ModelLoader.h"

namespace p3d {

    ModelLoader::ModelLoader() {}

    ModelLoader::~ModelLoader() 
    {

    }

    void ModelLoader::Load(const std::string& Filename)
    {

        // Load Model
		assimp_model = aiImportFile(Filename.c_str(),aiProcessPreset_TargetRealtime_Fast | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

        if (!assimp_model)
        {
            std::cout << "Failed To Import Model: " << Filename << std::endl;
        } else {
            
            // Build Skeleton
            // initial bone count
            boneCount = 0;
            // Get Skeleton
            GetBone(assimp_model->mRootNode);
            
            for (uint32 i=0;i<assimp_model->mNumMeshes;++i)
            {
                // loop through meshes
                const aiMesh* mesh = assimp_model->mMeshes[i];

                // create submesh
                SubMesh subMesh;
                
                // set submesh id
                subMesh.ID = i;
                
                // set name
                subMesh.Name = mesh->mName.data;
                
                subMesh.tIndex.reserve(mesh->mNumFaces);
                for (uint32 t = 0; t < mesh->mNumFaces; t++) {
                    const aiFace* face = &mesh->mFaces[t];
                    subMesh.tIndex.push_back(face->mIndices[0]);
                    subMesh.tIndex.push_back(face->mIndices[1]);
                    subMesh.tIndex.push_back(face->mIndices[2]);
                }

                // get posisitons
                if (mesh->HasPositions())
                {
                    subMesh.hasVertex = true;
                    subMesh.tVertex.resize(mesh->mNumVertices);
                    memcpy(&subMesh.tVertex[0],&mesh->mVertices[0],mesh->mNumVertices*sizeof(Vec3));
                } else subMesh.hasVertex = false;

                // get normals
                if (mesh->HasNormals())
                {                    
                    subMesh.hasNormal = true;
                    subMesh.tNormal.resize(mesh->mNumVertices);
                    memcpy(&subMesh.tNormal[0],&mesh->mNormals[0],mesh->mNumVertices*sizeof(Vec3));
                } else subMesh.hasNormal = false;

                // get texcoords
                if (mesh->HasTextureCoords(0))
                {               
                    subMesh.hasTexcoord = true;
                    subMesh.tTexcoord.reserve(mesh->mNumVertices);
                    for (uint32 k = 0; k < mesh->mNumVertices;k++) 
                    {
                        subMesh.tTexcoord.push_back(Vec2(mesh->mTextureCoords[0][k].x,mesh->mTextureCoords[0][k].y));                            
                    }
                } else subMesh.hasTexcoord = false;

                // get tangent
                if (mesh->HasTangentsAndBitangents())
                {
                    subMesh.hasTangentBitangent = true;
                    subMesh.tTangent.resize(mesh->mNumVertices);
                    subMesh.tBitangent.resize(mesh->mNumVertices);
                    memcpy(&subMesh.tTangent[0],&mesh->mTangents[0],mesh->mNumVertices*sizeof(Vec3));
                    memcpy(&subMesh.tBitangent[0],&mesh->mBitangents[0],mesh->mNumVertices*sizeof(Vec3));
                } else subMesh.hasTangentBitangent = false;

                // get vertex colors
                if (mesh->HasVertexColors(0))
                {
                    subMesh.hasVertexColor = true;
                    subMesh.tVertexColor.resize(mesh->mNumVertices);
                    memcpy(&subMesh.tVertexColor[0],&mesh->mColors[0],mesh->mNumVertices*sizeof(Vec4));
                } else subMesh.hasVertexColor = false;                

                // get vertex weights
                if (mesh->HasBones())
                {
                    
                    subMesh.hasBones = true;
                    subMesh.tBonesID.reserve(mesh->mNumVertices);
                    subMesh.tBonesWeight.reserve(mesh->mNumVertices);
                    
                    // Create Bone's Sub Mesh Internal ID
                    uint32 count = 0;
                    for ( uint32 k = 0; k < mesh->mNumBones; k++)
                    {
                        // Save Offset Matrix
                        Matrix _offsetMatrix;
                        _offsetMatrix.m[0] = mesh->mBones[k]->mOffsetMatrix.a1; _offsetMatrix.m[1] = mesh->mBones[k]->mOffsetMatrix.b1; _offsetMatrix.m[2] = mesh->mBones[k]->mOffsetMatrix.c1; _offsetMatrix.m[3] = mesh->mBones[k]->mOffsetMatrix.d1;
                        _offsetMatrix.m[4] = mesh->mBones[k]->mOffsetMatrix.a2; _offsetMatrix.m[5] = mesh->mBones[k]->mOffsetMatrix.b2; _offsetMatrix.m[6] = mesh->mBones[k]->mOffsetMatrix.c2; _offsetMatrix.m[7] = mesh->mBones[k]->mOffsetMatrix.d2;
                        _offsetMatrix.m[8] = mesh->mBones[k]->mOffsetMatrix.a3; _offsetMatrix.m[9] = mesh->mBones[k]->mOffsetMatrix.b3; _offsetMatrix.m[10] = mesh->mBones[k]->mOffsetMatrix.c3; _offsetMatrix.m[11] = mesh->mBones[k]->mOffsetMatrix.d3;
                        _offsetMatrix.m[12] = mesh->mBones[k]->mOffsetMatrix.a4;_offsetMatrix.m[13] = mesh->mBones[k]->mOffsetMatrix.b4;_offsetMatrix.m[14] = mesh->mBones[k]->mOffsetMatrix.c4; _offsetMatrix.m[15] = mesh->mBones[k]->mOffsetMatrix.d4;

                        sint32 boneID = GetBoneID(mesh->mBones[k]->mName.data);
                        subMesh.BoneOffsetMatrix[boneID] = _offsetMatrix;
                        subMesh.MapBoneIDs[boneID] = count;
                        count++;
                    }
                    
                    // Add Bones and Weights to SubMesh Structure, based on Internal IDs
                    for (uint32 j = 0;j< mesh->mNumVertices; j++)
                    {
                        // get values
                        std::vector<int> boneID(4,0);
                        std::vector<f32>weightValue(4,0.f);
                        
                        count = 0;                        
                        for ( uint32 k = 0; k < mesh->mNumBones; k++) 
                        {                            
                            for ( uint32 l = 0; l < mesh->mBones[k]->mNumWeights; l++)
                            {
                                if (mesh->mBones[k]->mWeights[l].mVertexId==j)
                                {
                                    // Convert Bone ID to Internal of the Sub Mesh
                                    boneID[count]=subMesh.MapBoneIDs[GetBoneID(mesh->mBones[k]->mName.data)];
                                    // Add Bone Weight
                                    weightValue[count]=mesh->mBones[k]->mWeights[l].mWeight;
                                    count ++;
                                }
                            }
                        }
                        subMesh.tBonesID.push_back(Vec4(boneID[0],boneID[1],boneID[2],boneID[3]));
                        subMesh.tBonesWeight.push_back(Vec4(weightValue[0],weightValue[1],weightValue[2],weightValue[3]));                        
                    }
                    
                } else subMesh.hasBones = false;
                
                // Get SubMesh Material ID
                subMesh.materialID = mesh->mMaterialIndex;
                
                // add to submeshes vector
                subMeshes.push_back(subMesh);
            }
            
            // Build Materials List
            for (uint32 i=0;i<assimp_model->mNumMaterials;++i)
            {
                Renderables::MaterialProperties material;
                // Get Material
                const aiMaterial* pMaterial = assimp_model->mMaterials[i];
                material.id = i;
                
                aiString name;
                pMaterial->Get(AI_MATKEY_NAME, name);
                material.Name.resize(name.length);
                memcpy(&material.Name[0], name.data, name.length);                
                
                aiColor3D color;
				material.haveColor = false;
                if (pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color)==AI_SUCCESS) 
                {
                    material.haveColor = true;
                    material.Color = Vec4(color.r, color.g, color.b, 1.0);                
                }
				material.haveAmbient = false;
                if (pMaterial->Get(AI_MATKEY_COLOR_AMBIENT, color)==AI_SUCCESS) 
                {
                    material.haveAmbient = true;
                    material.Ambient = Vec4(color.r, color.g, color.b, 1.0);                                
                }
				material.haveSpecular = false;
                if (pMaterial->Get(AI_MATKEY_COLOR_SPECULAR, color)==AI_SUCCESS) 
                {
                    material.haveSpecular = true;
                    material.Specular = Vec4(color.r, color.g, color.b, 1.0);
                }
                material.haveEmissive = false;
                if (pMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, color)==AI_SUCCESS) 
                {
                    material.haveEmissive = true;
                    material.Emissive = Vec4(color.r, color.g, color.b, 1.0);
                }
                
                bool flag = false;
                pMaterial->Get(AI_MATKEY_ENABLE_WIREFRAME, flag);
                material.WireFrame = flag;

                flag = false;
                pMaterial->Get(AI_MATKEY_TWOSIDED, flag);
                material.Twosided = flag;
                
                f32 value = 1.0f;
                pMaterial->Get(AI_MATKEY_OPACITY, value);
                material.Opacity = value;
                
                value = 0.0f;
                pMaterial->Get(AI_MATKEY_SHININESS, value);
                material.Shininess = value;
                
                value = 0.0f;
                pMaterial->Get(AI_MATKEY_SHININESS_STRENGTH, value);
                material.ShininessStrength = value;                
                // Save Properties                
                
                aiString path;
                aiReturn texFound;
                
                // Path Relative to Model File
                std::string RelativePath = Filename.substr(0,Filename.find_last_of("/")+1);
                
                // Diffuse
                texFound = assimp_model->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, 0, &path);
                if (texFound==AI_SUCCESS)
                {
                    material.haveColorMap = true;
                    material.colorMap.resize(path.length);
                    memcpy(&material.colorMap[0],&path.data,path.length);
                    std::replace(material.colorMap.begin(), material.colorMap.end(),'\\','/');
                    material.colorMap = RelativePath + material.colorMap;
                }
                
                // Bump Map
                texFound = assimp_model->mMaterials[i]->GetTexture(aiTextureType_NORMALS, 0, &path);
                if (texFound==AI_SUCCESS)
                {
                    material.haveNormalMap = true;
                    material.normalMap.resize(path.length);
                    memcpy(&material.normalMap[0],&path.data,path.length);
                    std::replace(material.normalMap.begin(), material.normalMap.end(),'\\','/');
                    material.normalMap = RelativePath + material.normalMap;
                } else {                
                    // Height Map
                    texFound = assimp_model->mMaterials[i]->GetTexture(aiTextureType_HEIGHT, 0, &path);
                    if (texFound==AI_SUCCESS)
                    {
                        material.haveNormalMap = true;
                        material.normalMap.resize(path.length);
                        memcpy(&material.normalMap[0],&path.data,path.length);
                        std::replace(material.normalMap.begin(), material.normalMap.end(),'\\','/');
                        material.normalMap = RelativePath + material.normalMap;
                    }
                }
                // Specular
                texFound = assimp_model->mMaterials[i]->GetTexture(aiTextureType_SPECULAR, 0, &path);
                if (texFound==AI_SUCCESS)
                {
                    material.haveSpecularMap = true;
                    material.specularMap.resize(path.length);
                    memcpy(&material.specularMap[0],&path.data,path.length);
                    std::replace(material.specularMap.begin(), material.specularMap.end(),'\\','/');
                    material.specularMap = RelativePath + material.specularMap;
                }
                // Add Material to List
                materials.push_back(material);
            }
        }
    }
    
    void ModelLoader::GetBone(aiNode* bone, const sint32 &parentID)
    {

        aiVector3D _bonePos, _boneScale;
        aiQuaternion _boneRot;
        bone->mTransformation.Decompose(_boneScale, _boneRot, _bonePos);                

        Matrix _boneMatrix;
        _boneMatrix.m[0] = bone->mTransformation.a1; _boneMatrix.m[1] = bone->mTransformation.b1; _boneMatrix.m[2] = bone->mTransformation.c1; _boneMatrix.m[3] = bone->mTransformation.d1;
        _boneMatrix.m[4] = bone->mTransformation.a2; _boneMatrix.m[5] = bone->mTransformation.b2; _boneMatrix.m[6] = bone->mTransformation.c2; _boneMatrix.m[7] = bone->mTransformation.d2;
        _boneMatrix.m[8] = bone->mTransformation.a3; _boneMatrix.m[9] = bone->mTransformation.b3; _boneMatrix.m[10] = bone->mTransformation.c3; _boneMatrix.m[11] = bone->mTransformation.d3;
        _boneMatrix.m[12] = bone->mTransformation.a4; _boneMatrix.m[13] = bone->mTransformation.b4; _boneMatrix.m[14] = bone->mTransformation.c4; _boneMatrix.m[15] = bone->mTransformation.d4;
        
        Bone _bone;
        _bone.name			= bone->mName.data;
        _bone.self			= boneCount;
        _bone.parent        = parentID;
        _bone.pos			= Vec3(_bonePos.x,_bonePos.y,_bonePos.z);
        _bone.rot			= Quaternion(_boneRot.w,_boneRot.x,_boneRot.y,_boneRot.z);
        _bone.scale			= Vec3(_boneScale.x,_boneScale.y,_boneScale.z);
        _bone.bindPoseMat   = _boneMatrix;

        // add bone to Skeleton
        skeleton[StringID (MakeStringID(_bone.name))]=_bone;

        // increase bone count
        boneCount++;
        
        // check and get children
        if (bone->mNumChildren>0) 
        {
            for (uint32 i=0;i<bone->mNumChildren;i++)
                GetBone(bone->mChildren[i], _bone.self);
        }
    }
    
    uint32 ModelLoader::GetBoneID(const std::string& BoneName)
    {
        StringID ID (MakeStringID(BoneName));
        return skeleton[ID].self;
    }
    uint32 ModelLoader::GetBoneID(const StringID& BoneID)
    {        
        return skeleton[BoneID].self;
    }
    
    void ModelLoader::DebugSkeleton()
    {
        // show skeleton
        for (std::map<StringID, Bone>::iterator i=skeleton.begin();i!=skeleton.end();i++)
        {
            if ((*i).second.self==0) 
            {
                std::cout << "ID: "<< (*i).second.self << " Name: " << (*i).second.name << std::endl;
                GetBoneChilds(skeleton,0,0);
            }
        }
    }
    
    void ModelLoader::GetBoneChilds(std::map<StringID,Bone> Skeleton, const int& id, const uint32& iterations)
    {
        for (std::map<StringID, Bone>::iterator i=Skeleton.begin();i!=Skeleton.end();i++)
        {            
            if ((*i).second.parent==id) 
            {
                for (uint32 j=0;j<iterations+1;j++) if (j==iterations) std::cout << " |_"; else std::cout << "   ";
                std::cout << "___" << "ID: "<< (*i).second.self << " Name: " << (*i).second.name << std::endl;              
                GetBoneChilds(Skeleton,(*i).second.self,iterations+1);
            }
        }
    }
    
}