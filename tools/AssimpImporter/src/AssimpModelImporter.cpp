//============================================================================
// Name        : AssimpModelImporter.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads  model formats based on Assimp
//============================================================================

#include "AssimpModelImporter.h"

namespace p3d {

	AssimpModelImporter::AssimpModelImporter() {}

	AssimpModelImporter::~AssimpModelImporter() {}

	bool AssimpModelImporter::Load(const std::string& Filename)
	{
		// Assimp Importer
		Assimp::Importer Importer;

		// Load Model
		assimp_model = Importer.ReadFile(Filename.c_str(), aiProcessPreset_TargetRealtime_Fast | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

		// Path Relative to Model File
		std::string RelativePath = Filename.substr(0, Filename.find_last_of("/") + 1);

		if (!assimp_model)
		{
			echo("Failed To Import Model: " + Filename + " ERROR: " + Importer.GetErrorString());
			return false;
		}
		else {

			// Build Skeleton
			// initial bone count
			boneCount = 0;
			// Get Skeleton
			GetBone(assimp_model->mRootNode);

			for (uint32 i = 0; i < assimp_model->mNumMeshes; i++)
			{
				// loop through meshes
				const aiMesh* mesh = assimp_model->mMeshes[i];

				// create submesh
				SubMesh subMesh;

				// set submesh id
				subMesh.ID = i;

				// set name
				subMesh.Name = mesh->mName.data;

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
					memcpy(&subMesh.tVertex[0], &mesh->mVertices[0], mesh->mNumVertices*sizeof(Vec3));
				}
				else subMesh.hasVertex = false;

				// get normals
				if (mesh->HasNormals())
				{
					subMesh.hasNormal = true;
					subMesh.tNormal.resize(mesh->mNumVertices);
					memcpy(&subMesh.tNormal[0], &mesh->mNormals[0], mesh->mNumVertices*sizeof(Vec3));
				}
				else subMesh.hasNormal = false;

				// get texcoords
				if (mesh->HasTextureCoords(0))
				{
					subMesh.hasTexcoord = true;
					for (uint32 k = 0; k < mesh->mNumVertices; k++)
					{
						subMesh.tTexcoord.push_back(Vec2(mesh->mTextureCoords[0][k].x, mesh->mTextureCoords[0][k].y));
					}
				}
				else subMesh.hasTexcoord = false;

				// get tangent
				if (mesh->HasTangentsAndBitangents())
				{
					subMesh.hasTangentBitangent = true;
					subMesh.tTangent.resize(mesh->mNumVertices);
					subMesh.tBitangent.resize(mesh->mNumVertices);
					memcpy(&subMesh.tTangent[0], &mesh->mTangents[0], mesh->mNumVertices*sizeof(Vec3));
					memcpy(&subMesh.tBitangent[0], &mesh->mBitangents[0], mesh->mNumVertices*sizeof(Vec3));
				}
				else subMesh.hasTangentBitangent = false;

				// get vertex colors
				if (mesh->HasVertexColors(0))
				{
					subMesh.hasVertexColor = true;
					subMesh.tVertexColor.resize(mesh->mNumVertices);
					memcpy(&subMesh.tVertexColor[0], &mesh->mColors[0], mesh->mNumVertices*sizeof(Vec4));
				}
				else subMesh.hasVertexColor = false;

				// get vertex weights
				if (mesh->HasBones())
				{
					// Set Flag
					subMesh.hasBones = true;

					// Create Bone's Sub Mesh Internal ID
					uint32 count = 0;
					for (uint32 k = 0; k < mesh->mNumBones; k++)
					{
						// Save Offset Matrix
						Matrix _offsetMatrix;
						_offsetMatrix.m[0] = mesh->mBones[k]->mOffsetMatrix.a1; _offsetMatrix.m[1] = mesh->mBones[k]->mOffsetMatrix.b1;  _offsetMatrix.m[2] = mesh->mBones[k]->mOffsetMatrix.c1; _offsetMatrix.m[3] = mesh->mBones[k]->mOffsetMatrix.d1;
						_offsetMatrix.m[4] = mesh->mBones[k]->mOffsetMatrix.a2; _offsetMatrix.m[5] = mesh->mBones[k]->mOffsetMatrix.b2;  _offsetMatrix.m[6] = mesh->mBones[k]->mOffsetMatrix.c2; _offsetMatrix.m[7] = mesh->mBones[k]->mOffsetMatrix.d2;
						_offsetMatrix.m[8] = mesh->mBones[k]->mOffsetMatrix.a3; _offsetMatrix.m[9] = mesh->mBones[k]->mOffsetMatrix.b3;  _offsetMatrix.m[10] = mesh->mBones[k]->mOffsetMatrix.c3; _offsetMatrix.m[11] = mesh->mBones[k]->mOffsetMatrix.d3;
						_offsetMatrix.m[12] = mesh->mBones[k]->mOffsetMatrix.a4; _offsetMatrix.m[13] = mesh->mBones[k]->mOffsetMatrix.b4;  _offsetMatrix.m[14] = mesh->mBones[k]->mOffsetMatrix.c4; _offsetMatrix.m[15] = mesh->mBones[k]->mOffsetMatrix.d4;

						uint32 boneID = GetBoneID(mesh->mBones[k]->mName.data);
						subMesh.BoneOffsetMatrix[boneID] = _offsetMatrix;
						subMesh.MapBoneIDs[boneID] = count;
						count++;
					}

					// Add Bones and Weights to SubMesh Structure, based on Internal IDs
					for (uint32 j = 0; j < mesh->mNumVertices; j++)
					{
						// get values
						std::vector<uint32> boneID(4, 0);
						std::vector<f32> weightValue(4, 0.f);

						uint32 count = 0;
						for (uint32 k = 0; k < mesh->mNumBones; k++)
						{
							for (uint32 l = 0; l < mesh->mBones[k]->mNumWeights; l++)
							{
								if (mesh->mBones[k]->mWeights[l].mVertexId == j)
								{
									// Convert Bone ID to Internal of the Sub Mesh
									boneID[count] = subMesh.MapBoneIDs[GetBoneID(mesh->mBones[k]->mName.data)];
									// Add Bone Weight
									weightValue[count] = mesh->mBones[k]->mWeights[l].mWeight;
									count++;
								}
							}
						}
						subMesh.tBonesID.push_back(Vec4((f32)boneID[0], (f32)boneID[1], (f32)boneID[2], (f32)boneID[3]));
						subMesh.tBonesWeight.push_back(Vec4(weightValue[0], weightValue[1], weightValue[2], weightValue[3]));
					}

				}
				else subMesh.hasBones = false;

				// Get SubMesh Material ID
				subMesh.materialID = mesh->mMaterialIndex;

				// add to submeshes vector
				subMeshes.push_back(subMesh);
			}

			// Build Materials List
			for (uint32 i = 0; i < assimp_model->mNumMaterials; ++i)
			{
				MaterialProperties material;
				// Get Material
				const aiMaterial* pMaterial = assimp_model->mMaterials[i];
				material.id = i;

				aiString name;
				pMaterial->Get(AI_MATKEY_NAME, name);
				material.Name.resize(name.length);
				memcpy(&material.Name[0], name.data, name.length);

				aiColor3D color;
				material.haveColor = false;
				if (pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
				{
					material.haveColor = true;
					material.Color = Vec4(color.r, color.g, color.b, 1.0);
				}
				material.haveAmbient = false;
				if (pMaterial->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS)
				{
					material.haveAmbient = true;
					material.Ambient = Vec4(color.r, color.g, color.b, 1.0);
				}
				material.haveSpecular = false;
				if (pMaterial->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
				{
					material.haveSpecular = true;
					material.Specular = Vec4(color.r, color.g, color.b, 1.0);
				}
				material.haveEmissive = false;
				if (pMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
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

				// Diffuse
				texFound = assimp_model->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, 0, &path);
				if (texFound == AI_SUCCESS)
				{
					material.haveColorMap = true;
					material.colorMap.resize(path.length);
					memcpy(&material.colorMap[0], &path.data, path.length);
					std::replace(material.colorMap.begin(), material.colorMap.end(), '\\', '/');
				}

				// Bump Map
				texFound = assimp_model->mMaterials[i]->GetTexture(aiTextureType_NORMALS, 0, &path);
				if (texFound == AI_SUCCESS)
				{
					material.haveNormalMap = true;
					material.normalMap.resize(path.length);
					memcpy(&material.normalMap[0], &path.data, path.length);
					std::replace(material.normalMap.begin(), material.normalMap.end(), '\\', '/');
				}
				else {
					// Height Map
					texFound = assimp_model->mMaterials[i]->GetTexture(aiTextureType_HEIGHT, 0, &path);
					if (texFound == AI_SUCCESS)
					{
						material.haveNormalMap = true;
						material.normalMap.resize(path.length);
						memcpy(&material.normalMap[0], &path.data, path.length);
						std::replace(material.normalMap.begin(), material.normalMap.end(), '\\', '/');
					}
				}
				// Specular
				texFound = assimp_model->mMaterials[i]->GetTexture(aiTextureType_SPECULAR, 0, &path);
				if (texFound == AI_SUCCESS)
				{
					material.haveSpecularMap = true;
					material.specularMap.resize(path.length);
					memcpy(&material.specularMap[0], &path.data, path.length);
					std::replace(material.specularMap.begin(), material.specularMap.end(), '\\', '/');
				}
				// Add Material to List
				materials.push_back(material);
			}
		}
		Importer.FreeScene();

		return true;
	}

	void AssimpModelImporter::GetBone(aiNode* bone, const int32 &parentID)
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
		_bone.name = bone->mName.data;
		_bone.self = boneCount;
		_bone.parent = parentID;
		_bone.pos = Vec3(_bonePos.x, _bonePos.y, _bonePos.z);
		_bone.rot = Quaternion(_boneRot.w, _boneRot.x, _boneRot.y, _boneRot.z);
		_bone.scale = Vec3(_boneScale.x, _boneScale.y, _boneScale.z);
		_bone.bindPoseMat = _boneMatrix;

		// add bone to Skeleton
		skeleton[StringID(MakeStringID(_bone.name))] = _bone;

		// increase bone count
		boneCount++;

		// check and get children
		if (bone->mNumChildren > 0)
		{
			for (uint32 i = 0; i < bone->mNumChildren; i++)
				GetBone(bone->mChildren[i], _bone.self);
		}
	}

	bool AssimpModelImporter::ConvertToPyrosFormat(const std::string &Filename)
	{
		BinaryFile *bin = new BinaryFile();

		if (bin->Open(Filename.c_str(), 'w'))
		{
			// Save Materials
			int32 materialsSize = materials.size();
			bin->Write(&materialsSize, sizeof(int32));
			for (std::vector<MaterialProperties>::iterator i = materials.begin(); i != materials.end(); i++)
			{
				// Material ID
				bin->Write(&(*i).id, sizeof(int32));

				// Material Name
				int32 nameSize = (*i).Name.size();
				bin->Write(&nameSize, sizeof(int32));
				if (nameSize > 0)
					bin->Write((*i).Name.c_str(), sizeof(char)*nameSize);

				// Color
				uchar Out = (uchar)(*i).haveColor;
				bin->Write(&Out, sizeof(uchar));

				bin->Write(&(*i).Color, sizeof(Vec4));

				// Specular
				Out = (uchar)(*i).haveSpecular;
				bin->Write(&Out, sizeof(uchar));

				bin->Write(&(*i).Specular, sizeof(Vec4));

				// Ambient
				Out = (uchar)(*i).haveAmbient;
				bin->Write(&Out, sizeof(uchar));

				bin->Write(&(*i).Ambient, sizeof(Vec4));

				// Emissive
				Out = (uchar)(*i).haveEmissive;
				bin->Write(&Out, sizeof(uchar));

				bin->Write(&(*i).Emissive, sizeof(Vec4));

				// WireFrame
				Out = (uchar)(*i).WireFrame;
				bin->Write(&Out, sizeof(uchar));

				// Twosided
				Out = (uchar)(*i).Twosided;
				bin->Write(&Out, sizeof(uchar));

				// Opacity
				bin->Write(&(*i).Opacity, sizeof(f32));

				// Shininess
				bin->Write(&(*i).Shininess, sizeof(f32));

				// Shininess Strength
				bin->Write(&(*i).ShininessStrength, sizeof(f32));

				// Textures
				// Color Map
				Out = (uchar)(*i).haveColorMap;
				bin->Write(&Out, sizeof(uchar));

				int32 colorMapSize = (*i).colorMap.size();
				bin->Write(&colorMapSize, sizeof(int32));
				if (colorMapSize > 0)
					bin->Write((*i).colorMap.c_str(), sizeof(char)*colorMapSize);

				// Specular Map
				Out = (uchar)(*i).haveSpecularMap;
				bin->Write(&Out, sizeof(uchar));

				int32 specularMapSize = (*i).specularMap.size();
				bin->Write(&specularMapSize, sizeof(int32));
				if (specularMapSize > 0)
					bin->Write((*i).specularMap.c_str(), sizeof(char)*specularMapSize);

				// Normal Map
				Out = (uchar)(*i).haveNormalMap;
				bin->Write(&Out, sizeof(uchar));

				int32 normalMapSize = (*i).normalMap.size();
				bin->Write(&normalMapSize, sizeof(int32));
				if (normalMapSize > 0)
					bin->Write((*i).normalMap.c_str(), sizeof(char)*normalMapSize);

				// Have Bones
				Out = (uchar)(*i).haveBones;
				bin->Write(&Out, sizeof(uchar));
			}

			// Skeleton
			int32 skeletonSize = skeleton.size();
			bin->Write(&skeletonSize, sizeof(int32));
			if (skeletonSize > 0)
			{
				// Write ids
				for (std::map<uint32, Bone>::iterator i = skeleton.begin(); i != skeleton.end(); i++)
				{
					bin->Write(&(*i).first, sizeof(uint32));

					// Name
					int32 nameSize = (*i).second.name.size();
					bin->Write(&nameSize, sizeof(int32));
					if (nameSize > 0)
						bin->Write((*i).second.name.c_str(), sizeof(char)*nameSize);

					// Self
					bin->Write(&(*i).second.self, sizeof(int32));

					// Parent
					bin->Write(&(*i).second.parent, sizeof(int32));

					// Pos
					bin->Write(&(*i).second.pos, sizeof(Vec3));

					// Rot
					bin->Write(&(*i).second.rot, sizeof(Quaternion));

					// Scale
					bin->Write(&(*i).second.scale, sizeof(Vec3));

					// BindPose
					bin->Write(&(*i).second.bindPoseMat.m[0], sizeof(Matrix));

					// Skinned
					uchar Out = (uchar)(*i).second.skinned;
					bin->Write(&Out, sizeof(uchar));
				}
			}

			// SubMeshes
			int32 subMeshesSize = subMeshes.size();
			bin->Write(&subMeshesSize, sizeof(int32));
			for (std::vector<SubMesh>::iterator i = subMeshes.begin(); i != subMeshes.end(); i++)
			{
				// Name
				int32 nameSize = (*i).Name.size();
				bin->Write(&nameSize, sizeof(int32));
				if (nameSize > 0)
					bin->Write((*i).Name.c_str(), sizeof(char)*nameSize);

				// Index
				int32 indexSize = (*i).tIndex.size();
				bin->Write(&indexSize, sizeof(int32));
				if (indexSize > 0)
					bin->Write(&(*i).tIndex[0], sizeof(int32)*indexSize);

				// Vertex
				int32 vertexSize = (*i).tVertex.size();
				bin->Write(&vertexSize, sizeof(int32));
				if (vertexSize > 0)
					bin->Write(&(*i).tVertex[0], sizeof(Vec3)*vertexSize);

				// Normal
				int32 normalSize = (*i).tNormal.size();
				bin->Write(&normalSize, sizeof(int32));
				if (normalSize > 0)
					bin->Write(&(*i).tNormal[0], sizeof(Vec3)*normalSize);

				// Tangent
				int32 tangentSize = (*i).tTangent.size();
				bin->Write(&tangentSize, sizeof(int32));
				if (tangentSize > 0)
				{
					bin->Write(&(*i).tTangent[0], sizeof(Vec3)*tangentSize);
					bin->Write(&(*i).tBitangent[0], sizeof(Vec3)*tangentSize);
				}

				// Texcoord
				int32 texcoordSize = (*i).tTexcoord.size();
				bin->Write(&texcoordSize, sizeof(int32));
				if (texcoordSize > 0)
					bin->Write(&(*i).tTexcoord[0], sizeof(Vec2)*texcoordSize);

				// Vertex Color
				int32 vertexColorSize = (*i).tVertexColor.size();
				bin->Write(&vertexColorSize, sizeof(int32));
				if (vertexColorSize > 0)
					bin->Write(&(*i).tVertexColor[0], sizeof(Vec4)*vertexColorSize);

				// Bones
				int32 BonesSize = (*i).tBonesID.size();
				bin->Write(&BonesSize, sizeof(int32));
				if (BonesSize > 0)
				{
					bin->Write(&(*i).tBonesID[0], sizeof(Vec4)*BonesSize);
					bin->Write(&(*i).tBonesWeight[0], sizeof(Vec4)*BonesSize);
				}

				// Map Bones IDs
				int32 MapBoneIDsSize = (*i).MapBoneIDs.size();
				bin->Write(&MapBoneIDsSize, sizeof(int32));
				if (MapBoneIDsSize > 0)
				{
					// Copy Index
					for (std::map<int32, int32>::iterator k = (*i).MapBoneIDs.begin(); k != (*i).MapBoneIDs.end(); k++)
					{
						bin->Write(&(*k).first, sizeof(int32));
						bin->Write(&(*k).second, sizeof(int32));
					}
				}

				// Offset Matrix
				int32 BoneOffsetMatrixSize = (*i).BoneOffsetMatrix.size();
				bin->Write(&BoneOffsetMatrixSize, sizeof(int32));
				if (BoneOffsetMatrixSize > 0)
				{
					// Copy Index
					for (std::map<int32, Matrix>::iterator k = (*i).BoneOffsetMatrix.begin(); k != (*i).BoneOffsetMatrix.end(); k++)
					{
						bin->Write(&(*k).first, sizeof(int32));
						bin->Write(&(*k).second.m[0], sizeof(Matrix));
					}
				}

				// Material ID
				bin->Write(&(*i).materialID, sizeof(int32));
			}

			bin->Close();

			delete bin;

			return true;
		}
		else return false;
	}
}