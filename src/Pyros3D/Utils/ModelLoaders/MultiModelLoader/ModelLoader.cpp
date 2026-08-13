//============================================================================
// Name        : ModelLoader.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads Pyros3D Own Model Format
//============================================================================

#include <Pyros3D/Utils/ModelLoaders/MultiModelLoader/ModelLoader.h>
#include <filesystem>
#include <algorithm>
#include <utility>

namespace fs = std::filesystem;

namespace p3d {

	namespace {
		std::string ResolveModelTexturePath(const std::string& modelFilename, const std::string& storedRaw)
		{
			std::string stored = storedRaw;
			for (size_t i = 0; i < stored.size(); ++i)
				if (stored[i] == '\\') stored[i] = '/';

			// Strip leading "./"
			while (stored.size() >= 2 && stored[0] == '.' && stored[1] == '/')
				stored = stored.substr(2);

			if (stored.empty()) return stored;

			// Embedded Assimp textures (*0) — leave as-is (loader/packager handle).
			if (stored[0] == '*') return stored;

			fs::path storedPath(stored);
			// Absolute paths must not be prefixed with the .p3dm directory.
			if (storedPath.is_absolute())
				return storedPath.lexically_normal().string();
			// Unix-style absolute that fs::path may not treat as absolute on Windows.
			if (!stored.empty() && stored[0] == '/')
				return stored;

			fs::path modelPath(modelFilename);
			fs::path parent = modelPath.parent_path();
			if (parent.empty())
				return storedPath.lexically_normal().generic_string();

			return (parent / storedPath).lexically_normal().string();
		}
	}

	ModelLoader::ModelLoader() {}

	ModelLoader::~ModelLoader() {}

	bool ModelLoader::Load(const std::string& Filename)
	{
		BinaryFile* bin = new BinaryFile();
		if (!bin->Open(Filename.c_str(), 'r'))
		{
			echo(std::string("ERROR: ModelLoader - couldn't open ") + Filename);
			delete bin;
			return false;
		}

		int32 materialsSize = 0;
		bin->Read(&materialsSize, sizeof(int32));
		if (materialsSize < 0 || materialsSize > 100000)
		{
			echo(std::string("ERROR: ModelLoader - corrupt materials header in ") + Filename);
			bin->Close();
			delete bin;
			return false;
		}
		if (materialsSize > 0)
			materials.reserve((size_t)materialsSize);

		// Materials
		for (int i = 0; i < materialsSize; i++)
		{
			MaterialProperties mat;

			// Material ID
			bin->Read(&mat.id, sizeof(int32));

			// Name
			int nameSize;
			bin->Read(&nameSize, sizeof(int32));
			if (nameSize > 0)
			{
				mat.Name.resize(nameSize);
				bin->Read(&mat.Name[0], sizeof(char) * nameSize);
			}
			// Color
			uchar In;
			bin->Read(&In, sizeof(uchar));
			mat.haveColor = !!In;
			bin->Read(&mat.Color, sizeof(Vec4));

			// Specular
			bin->Read(&In, sizeof(uchar));
			mat.haveSpecular = !!In;
			bin->Read(&mat.Specular, sizeof(Vec4));

			// Ambient
			bin->Read(&In, sizeof(uchar));
			mat.haveAmbient = !!In;
			bin->Read(&mat.Ambient, sizeof(Vec4));

			// Emissive
			bin->Read(&In, sizeof(uchar));
			mat.haveEmissive = !!In;
			bin->Read(&mat.Emissive, sizeof(Vec4));

			// WireFrame
			bin->Read(&In, sizeof(uchar));
			mat.WireFrame = !!In;

			// Twosided
			bin->Read(&In, sizeof(uchar));
			mat.Twosided = !!In;

			// Opacity
			bin->Read(&mat.Opacity, sizeof(f32));

			// Shininess
			bin->Read(&mat.Shininess, sizeof(f32));

			// Shininess
			bin->Read(&mat.ShininessStrength, sizeof(f32));

			// Color Map
			bin->Read(&In, sizeof(uchar));
			mat.haveColorMap = !!In;
			bin->Read(&nameSize, sizeof(int32));
			if (nameSize > 0)
			{
				std::string c; c.resize(nameSize);
				bin->Read(&c[0], nameSize);
				mat.colorMap = ResolveModelTexturePath(Filename, c);
			}

			// Specular Map
			bin->Read(&In, sizeof(uchar));
			mat.haveSpecularMap = !!In;
			bin->Read(&nameSize, sizeof(int32));
			if (nameSize > 0)
			{
				std::string c; c.resize(nameSize);
				bin->Read(&c[0], nameSize);
				mat.specularMap = ResolveModelTexturePath(Filename, c);
			}
			// Normal Map
			bin->Read(&In, sizeof(uchar));
			mat.haveNormalMap = !!In;
			bin->Read(&nameSize, sizeof(int32));
			if (nameSize > 0)
			{
				std::string c; c.resize(nameSize);
				bin->Read(&c[0], nameSize);
				mat.normalMap = ResolveModelTexturePath(Filename, c);
			}

			bin->Read(&In, sizeof(uchar));
			mat.haveBones = !!In;

			materials.push_back(mat);

		}

		// Skeleton
		int skeletonSize;
		bin->Read(&skeletonSize, sizeof(int32));
		for (int32 i = 0; i < skeletonSize; i++)
		{
			int boneID;
			bin->Read(&boneID, sizeof(int32));
			Bone b;

			// Name
			int nameSize;
			bin->Read(&nameSize, sizeof(int32));
			if (nameSize > 0)
			{
				b.name.resize(nameSize);
				bin->Read(&b.name[0], sizeof(char)*nameSize);
			}


			// Self
			bin->Read(&b.self, sizeof(int32));

			// Parent
			bin->Read(&b.parent, sizeof(int32));

			// Pos
			bin->Read(&b.pos, sizeof(Vec3));

			// Rot
			bin->Read(&b.rot, sizeof(Quaternion));

			// Scale
			bin->Read(&b.scale, sizeof(Vec3));

			// BindPose
			bin->Read(&b.bindPoseMat.m[0], sizeof(Matrix));

			// Skinned
			uchar In;
			bin->Read(&In, sizeof(uchar));
			b.skinned = !!In;

			skeleton[boneID] = b;
		}

		// SubMeshes
		int subMeshesSize;
		bin->Read(&subMeshesSize, sizeof(int32));
		// Reserve + move: SubMesh holds full vertex/index arrays. push_back
		// without reserve reallocated the vector repeatedly and copy-ctored
		// every prior mesh (O(N²) memcpy) — island.p3dm (~16MB) looked like
		// a hard hang on project/scene open.
		if (subMeshesSize > 0)
			subMeshes.reserve((size_t)subMeshesSize);

		for (int32 i = 0; i < subMeshesSize; i++)
		{
			SubMesh c_submesh;

			// Name
			int nameSize;
			bin->Read(&nameSize, sizeof(int32));
			if (nameSize > 0)
			{
				c_submesh.Name.resize(nameSize);
				bin->Read(&c_submesh.Name[0], sizeof(char)*nameSize);
			}

			// Index
			int indexSize;
			bin->Read(&indexSize, sizeof(int32));
			if (indexSize > 0)
			{
				c_submesh.tIndex.resize((size_t)indexSize);
				bin->Read(&c_submesh.tIndex[0], sizeof(int32) * (uint32)indexSize);
			}
			// Vertex
			int vertexSize;
			bin->Read(&vertexSize, sizeof(int32));
			if (vertexSize > 0)
			{
				c_submesh.hasVertex = true;
				c_submesh.tVertex.resize((size_t)vertexSize);
				bin->Read(&c_submesh.tVertex[0], sizeof(Vec3) * (uint32)vertexSize);
			}
			else c_submesh.hasVertex = false;

			// Normal
			int normalSize;
			bin->Read(&normalSize, sizeof(int32));
			if (normalSize > 0)
			{
				c_submesh.hasNormal = true;
				c_submesh.tNormal.resize(normalSize);
				bin->Read(&c_submesh.tNormal[0], sizeof(Vec3)*normalSize);
			}
			else c_submesh.hasNormal = false;

			// Tangent
			int tangentSize;
			bin->Read(&tangentSize, sizeof(int32));
			if (tangentSize > 0)
			{
				c_submesh.hasTangentBitangent = true;
				c_submesh.tTangent.resize(tangentSize);
				bin->Read(&c_submesh.tTangent[0], sizeof(Vec3)*tangentSize);
				c_submesh.tBitangent.resize(tangentSize);
				bin->Read(&c_submesh.tBitangent[0], sizeof(Vec3)*tangentSize);
			}
			else c_submesh.hasTangentBitangent = false;

			// Texcoord
			int texcoordSize;
			bin->Read(&texcoordSize, sizeof(int32));
			if (texcoordSize > 0)
			{
				c_submesh.hasTexcoord = true;
				c_submesh.tTexcoord.resize(texcoordSize);
				bin->Read(&c_submesh.tTexcoord[0], sizeof(Vec2)*texcoordSize);
			}
			else c_submesh.hasTexcoord = false;

			// Vertex Color
			int vertexColorSize;
			bin->Read(&vertexColorSize, sizeof(int32));
			if (vertexColorSize > 0)
			{
				c_submesh.hasVertexColor = true;
				c_submesh.tVertexColor.resize(vertexColorSize);
				bin->Read(&c_submesh.tVertexColor[0], sizeof(Vec4)*vertexColorSize);
			}
			else c_submesh.hasVertexColor = false;

			// Bones
			int BonesSize;
			bin->Read(&BonesSize, sizeof(int32));
			if (BonesSize > 0)
			{
				c_submesh.hasBones = true;
				c_submesh.tBonesID.resize(BonesSize);
				bin->Read(&c_submesh.tBonesID[0], sizeof(Vec4)*BonesSize);
				c_submesh.tBonesWeight.resize(BonesSize);
				bin->Read(&c_submesh.tBonesWeight[0], sizeof(Vec4)*BonesSize);
			}
			else c_submesh.hasBones = false;

			// Map Bones IDs
			int MapBoneIDsSize;
			bin->Read(&MapBoneIDsSize, sizeof(int32));
			if (MapBoneIDsSize > 0)
			{
				for (int32 k = 0; k < MapBoneIDsSize; k++)
				{
					int boneID;
					bin->Read(&boneID, sizeof(int32));
					bin->Read(&c_submesh.MapBoneIDs[boneID], sizeof(int32));
				}
			}

			// Offset Matrix
			int BoneOffsetMatrixSize;
			bin->Read(&BoneOffsetMatrixSize, sizeof(int32));
			if (BoneOffsetMatrixSize > 0)
			{
				for (int32 k = 0; k < BoneOffsetMatrixSize; k++)
				{
					int boneID;
					bin->Read(&boneID, sizeof(int32));
					bin->Read(&c_submesh.BoneOffsetMatrix[boneID].m[0], sizeof(Matrix));
				}
			}

			bin->Read(&c_submesh.materialID, sizeof(int32));

			subMeshes.push_back(std::move(c_submesh));

		}

		bin->Close();

		delete bin;

		return true;
	}

}