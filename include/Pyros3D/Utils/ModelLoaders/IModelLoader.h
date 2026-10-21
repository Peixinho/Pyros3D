//============================================================================
// Name        : ModelLoader.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Model Loader Interface
//============================================================================

#ifndef IMODELLOADER_H
#define	IMODELLOADER_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Other/Export.h>
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
	// A shape the mesh can take, in part or whole (a smile, a blink, a bulge): how far each vertex
	// and its normal move from the mesh as made, at full weight. RenderingComponent::SetMorphWeight
	// blends them.
	struct MorphTarget {
		std::string name;
		std::vector<Vec3> dPos, dNrm;
	};

	struct SubMesh {

		// submesh ID
		uint32 ID;

		// submesh Name
		std::string Name;

		// index
		std::vector<uint32> tIndex;

		// vectors to save data
		bool hasVertex, hasNormal, hasTangentBitangent, hasTexcoord, hasVertexColor;
		std::vector<Vec3> tVertex, tNormal, tTangent, tBitangent;
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

		// Morph targets (.p3dm version 3)
		std::vector<MorphTarget> morphTargets;

	};

	struct MaterialProperties;

	class PYROS3D_API IModelLoader {

	public:

		IModelLoader();

		virtual ~IModelLoader();

		virtual bool Load(const std::string &Filename) = 0;

		// list of all submeshes
		std::vector<SubMesh> subMeshes;
		// Skeleton
		std::map<uint32, Bone> skeleton;
		// Materials
		std::vector<MaterialProperties> materials;

		// Skinning
		uint32 GetBoneID(const std::string &BoneName);
		uint32 GetBoneID(const StringID &BoneID);

		// Debug Skeleton to String
		void DebugSkeleton();

	private:

		// aux function to show sekeleton
		void GetBoneChilds(std::map<StringID, Bone> Skeleton, const int32 id, const uint32 iterations);

	protected:

		std::string LoadFile(const std::string &Filename);

	};

}

#endif	/* IMODELLOADER_H */