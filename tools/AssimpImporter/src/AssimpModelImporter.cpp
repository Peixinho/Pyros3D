//============================================================================
// Name        : AssimpModelImporter.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads  model formats based on Assimp
//============================================================================

#include "AssimpModelImporter.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace p3d {

	namespace {

		// Sketchfab (and a lot of DCC exports) store a static mesh under a
		// node chain: an axis fix, a 0.01 scale, and a translation that is
		// the object's position in the original map. Leaving those on the
		// nodes — which this importer does not skin — drops the mesh in
		// the wrong orientation at centimetre scale, which reads as
		// inside-out faces and z-fighting. Bake them, then pull a mesh
		// that landed kilometres away back to the origin.
		void RecenterIfFarFromOrigin(std::vector<SubMesh>& subMeshes)
		{
			bool any = false;
			Vec3 mn, mx;
			for (size_t s = 0; s < subMeshes.size(); ++s)
			{
				const SubMesh& sm = subMeshes[s];
				for (size_t i = 0; i < sm.tVertex.size(); ++i)
				{
					const Vec3& v = sm.tVertex[i];
					if (!any) { mn = mx = v; any = true; continue; }
					if (v.x < mn.x) mn.x = v.x;
					if (v.y < mn.y) mn.y = v.y;
					if (v.z < mn.z) mn.z = v.z;
					if (v.x > mx.x) mx.x = v.x;
					if (v.y > mx.y) mx.y = v.y;
					if (v.z > mx.z) mx.z = v.z;
				}
			}
			if (!any) return;

			const bool originInside =
				mn.x <= 0.f && mx.x >= 0.f &&
				mn.y <= 0.f && mx.y >= 0.f &&
				mn.z <= 0.f && mx.z >= 0.f;
			if (originInside) return;

			const Vec3 center = (mn + mx) * 0.5f;
			const Vec3 extent = (mx - mn) * 0.5f;
			if (center.magnitude() <= extent.magnitude() * 2.f + 1.f) return;

			for (size_t s = 0; s < subMeshes.size(); ++s)
			{
				SubMesh& sm = subMeshes[s];
				for (size_t i = 0; i < sm.tVertex.size(); ++i)
					sm.tVertex[i] -= center;
			}
			echo("Model import: recentered mesh (it was placed far from the origin)");
		}

		// A mesh whose triangles wind against its own normals is invisible
		// from the outside under backface culling, and a mesh whose normals
		// point inward shows the interior. Sketchfab's viewer hides both
		// by drawing double-sided.
		void FixFacing(SubMesh& sm)
		{
			if (!sm.hasNormal || sm.tNormal.size() != sm.tVertex.size() || sm.tIndex.size() < 3)
				return;

			Vec3 mn, mx;
			mn = mx = sm.tVertex[0];
			for (size_t i = 1; i < sm.tVertex.size(); ++i)
			{
				const Vec3& v = sm.tVertex[i];
				if (v.x < mn.x) mn.x = v.x;
				if (v.y < mn.y) mn.y = v.y;
				if (v.z < mn.z) mn.z = v.z;
				if (v.x > mx.x) mx.x = v.x;
				if (v.y > mx.y) mx.y = v.y;
				if (v.z > mx.z) mx.z = v.z;
			}
			const Vec3 center = (mn + mx) * 0.5f;

			double outward = 0.0;
			int outwardN = 0;
			const size_t vstep = std::max<size_t>(1, sm.tVertex.size() / 4000);
			for (size_t i = 0; i < sm.tVertex.size(); i += vstep)
			{
				const Vec3 d = sm.tVertex[i] - center;
				outward += (double)sm.tNormal[i].dotProduct(d);
				++outwardN;
			}
			if (outwardN > 0 && outward < 0.0)
			{
				for (size_t i = 0; i < sm.tNormal.size(); ++i)
					sm.tNormal[i].negateSelf();
				echo("Model import: flipped inward normals on '" + sm.Name + "'");
			}

			int agree = 0, disagree = 0;
			const size_t tris = sm.tIndex.size() / 3;
			const size_t step = std::max<size_t>(1, tris / 4000);
			for (size_t t = 0; t < tris; t += step)
			{
				const uint32 i0 = sm.tIndex[t * 3];
				const uint32 i1 = sm.tIndex[t * 3 + 1];
				const uint32 i2 = sm.tIndex[t * 3 + 2];
				if (i0 >= sm.tVertex.size() || i1 >= sm.tVertex.size() || i2 >= sm.tVertex.size())
					continue;
				const Vec3 e1 = sm.tVertex[i1] - sm.tVertex[i0];
				const Vec3 e2 = sm.tVertex[i2] - sm.tVertex[i0];
				const Vec3 fn = e1.cross(e2);
				const Vec3 vn = sm.tNormal[i0] + sm.tNormal[i1] + sm.tNormal[i2];
				if (fn.dotProduct(vn) >= 0.f) ++agree;
				else ++disagree;
			}
			if (disagree > agree)
			{
				for (size_t t = 0; t < tris; ++t)
					std::swap(sm.tIndex[t * 3 + 1], sm.tIndex[t * 3 + 2]);
				// Tangents were built from the old winding. Negating the
				// bitangent keeps the tangent frame matched to the new one.
				if (sm.hasTangentBitangent)
				{
					for (size_t i = 0; i < sm.tBitangent.size(); ++i)
						sm.tBitangent[i].negateSelf();
				}
				echo("Model import: flipped backfacing triangles on '" + sm.Name + "'");
			}
		}

		std::string EmbeddedExtension(const aiTexture* tex)
		{
			std::string hint;
			if (tex->achFormatHint[0] != '\0')
				hint = tex->achFormatHint;
			for (size_t i = 0; i < hint.size(); ++i)
				hint[i] = (char)tolower((unsigned char)hint[i]);
			if (hint == "jpeg") hint = "jpg";
			if (hint == "png" || hint == "jpg" || hint == "tga" || hint == "bmp" || hint == "webp")
				return hint;

			const unsigned char* b = reinterpret_cast<const unsigned char*>(tex->pcData);
			const size_t n = (tex->mHeight == 0) ? (size_t)tex->mWidth : 0;
			if (n >= 8 && b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G')
				return "png";
			if (n >= 3 && b[0] == 0xFF && b[1] == 0xD8)
				return "jpg";
			return "tga";
		}

		// "*0" is an Assimp embedded texture. Copy it out as a real file
		// the rest of the pipeline already knows how to package.
		std::string MaterializeEmbedded(const aiScene* scene, const std::string& stored,
			std::vector<AssimpModelImporter::PendingEmbeddedTexture>& pending)
		{
			if (stored.empty() || stored[0] != '*') return stored;
			const int idx = atoi(stored.c_str() + 1);
			if (scene == NULL || idx < 0 || (unsigned)idx >= scene->mNumTextures || scene->mTextures[idx] == NULL)
				return stored;

			for (size_t i = 0; i < pending.size(); ++i)
			{
				if (pending[i].relativePath.size() > 0)
				{
					// Same index already extracted.
					const std::string tag = "embedded_" + std::to_string(idx) + ".";
					if (pending[i].relativePath.find(tag) != std::string::npos)
						return pending[i].relativePath;
				}
			}

			const aiTexture* tex = scene->mTextures[idx];
			AssimpModelImporter::PendingEmbeddedTexture out;
			const std::string ext = EmbeddedExtension(tex);
			out.relativePath = "textures/embedded_" + std::to_string(idx) + "." + ext;

			if (tex->mHeight == 0)
			{
				const unsigned char* b = reinterpret_cast<const unsigned char*>(tex->pcData);
				out.bytes.assign(b, b + (size_t)tex->mWidth);
			}
			else
			{
				// Uncompressed BGRA. Write a TGA so we don't need an encoder.
				const int w = (int)tex->mWidth;
				const int h = (int)tex->mHeight;
				out.bytes.resize(18 + (size_t)w * (size_t)h * 4);
				out.bytes[2] = 2;
				out.bytes[12] = (unsigned char)(w & 0xFF);
				out.bytes[13] = (unsigned char)((w >> 8) & 0xFF);
				out.bytes[14] = (unsigned char)(h & 0xFF);
				out.bytes[15] = (unsigned char)((h >> 8) & 0xFF);
				out.bytes[16] = 32;
				out.bytes[17] = 8; // 32-bit, origin top
				unsigned char* dst = &out.bytes[18];
				for (int y = 0; y < h; ++y)
				{
					for (int x = 0; x < w; ++x)
					{
						const aiTexel& p = tex->pcData[y * w + x];
						dst[0] = p.b; dst[1] = p.g; dst[2] = p.r; dst[3] = p.a;
						dst += 4;
					}
				}
				// Extension was guessed as tga when uncompressed; force it.
				out.relativePath = "textures/embedded_" + std::to_string(idx) + ".tga";
			}

			if (out.bytes.empty()) return stored;
			const std::string rel = out.relativePath;
			pending.push_back(std::move(out));
			echo("Model import: extracted embedded texture " + rel);
			return rel;
		}

	}

	namespace fs = std::filesystem;

	AssimpModelImporter::AssimpModelImporter() {}

	AssimpModelImporter::~AssimpModelImporter() {}

	bool AssimpModelImporter::Load(const std::string& Filename)
	{
		// Assimp Importer
		Assimp::Importer Importer;

		pendingEmbedded.clear();

		// Load Model
		const unsigned int flags = aiProcessPreset_TargetRealtime_Fast | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights | aiProcess_FlipUVs | aiProcess_CalcTangentSpace;
		assimp_model = Importer.ReadFile(Filename.c_str(), flags);

		if (!assimp_model)
		{
			echo("Failed To Import Model: " + Filename + " ERROR: " + Importer.GetErrorString());
			return false;
		}

		bool skinned = assimp_model->mNumAnimations > 0;
		if (!skinned)
		{
			for (uint32 i = 0; i < assimp_model->mNumMeshes; i++)
			{
				if (assimp_model->mMeshes[i]->HasBones()) { skinned = true; break; }
			}
		}

		// Static props: bake the node chain (axis, scale, map placement)
		// into the vertices. Skinned meshes keep the node graph so the
		// skeleton stays valid.
		if (!skinned)
		{
			assimp_model = Importer.ReadFile(Filename.c_str(), flags | aiProcess_PreTransformVertices);
			if (!assimp_model)
			{
				echo("Failed To Import Model: " + Filename + " ERROR: " + Importer.GetErrorString());
				return false;
			}
		}

		{
			// Build Skeleton
			// initial bone count
			boneCount = 0;
			// A node hierarchy with no skin weights is not a skeleton.
			// Recording one made every static mesh report HasBones().
			if (skinned)
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

				// Diffuse / base color (glTF PBR often only sets BASE_COLOR)
				texFound = assimp_model->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, 0, &path);
				if (texFound != AI_SUCCESS)
					texFound = assimp_model->mMaterials[i]->GetTexture(aiTextureType_BASE_COLOR, 0, &path);
				if (texFound != AI_SUCCESS)
					texFound = assimp_model->mMaterials[i]->GetTexture(aiTextureType_EMISSIVE, 0, &path);
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

			for (size_t m = 0; m < materials.size(); ++m)
			{
				if (materials[m].haveColorMap)
					materials[m].colorMap = MaterializeEmbedded(assimp_model, materials[m].colorMap, pendingEmbedded);
				if (materials[m].haveNormalMap)
					materials[m].normalMap = MaterializeEmbedded(assimp_model, materials[m].normalMap, pendingEmbedded);
				if (materials[m].haveSpecularMap)
					materials[m].specularMap = MaterializeEmbedded(assimp_model, materials[m].specularMap, pendingEmbedded);
			}

			if (!skinned)
				RecenterIfFarFromOrigin(subMeshes);
			for (size_t s = 0; s < subMeshes.size(); ++s)
				FixFacing(subMeshes[s]);
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
		for (size_t i = 0; i < pendingEmbedded.size(); ++i)
		{
			const fs::path dest = fs::path(Filename).parent_path() / pendingEmbedded[i].relativePath;
			std::error_code ec;
			fs::create_directories(dest.parent_path(), ec);
			std::ofstream out(dest.string().c_str(), std::ios::binary | std::ios::trunc);
			if (out && !pendingEmbedded[i].bytes.empty())
				out.write(reinterpret_cast<const char*>(pendingEmbedded[i].bytes.data()),
					(std::streamsize)pendingEmbedded[i].bytes.size());
		}

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