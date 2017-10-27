//============================================================================
// Name        : Model
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Model Geometry
//============================================================================

#ifndef MODEL_H
#define MODEL_H

#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Utils/ModelLoaders/MultiModelLoader/ModelLoader.h>
#include <Pyros3D/Other/Export.h>

namespace p3d {

	class PYROS3D_API ModelGeometry : public IGeometry
	{
	public:

		ModelGeometry() : IGeometry(GeometryType::BUFFER) {}

		// Vectors
		std::vector<Vec3> tVertex, tNormal, tTangent, tBitangent;
		std::vector<Vec2> tTexcoord;

		// Bones
		std::vector<Vec4> tBonesID, tBonesWeight;

		void CreateBuffers();

		virtual const std::vector<__INDEX_C_TYPE__> &GetIndexData() const { return index; }
		virtual const std::vector<Vec3> &GetVertexData() const { return tVertex; }
		virtual const std::vector<Vec3> &GetNormalData() const { return tNormal; }

	protected:

		virtual void CalculateBounding();
	};

	class PYROS3D_API Model : public Renderable {

	public:

		Model(const std::string ModelPath, bool mergeMeshes = true);

		virtual ~Model() {}

		// Model loader, skeleton and animation
		IModelLoader* mesh;

		void Build();

		void DebugSkeleton();
		void GetBoneChilds(std::map<StringID, Bone> Skeleton, const int32 id, const uint32 iterations);

	protected:

		Model() {}
		uint32 MaterialOptions;

	};
};

#endif /* MODEL_H */