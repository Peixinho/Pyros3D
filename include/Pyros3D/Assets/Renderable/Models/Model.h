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

		ModelGeometry() : IGeometry() {}

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

		// Neither was stored before this - both ctor params were used
		// once then discarded, leaving no way to recover "what file was
		// this loaded from" after construction (needed e.g. for scene
		// serialization). Empty Path on the default (Decal) constructor,
		// matching Texture::GetFilename()'s same "unrecoverable source"
		// convention.
		const std::string &GetPath() const { return Path; }
		bool GetMergeMeshes() const { return MergeMeshes; }

	protected:

		// mesh is only a scratch loader used during the parameterized
		// constructor (freed and NULLed once its data is copied into
		// Geometries); subclasses using this default constructor (e.g.
		// Decal) never touch it, so it must start NULL rather than
		// uninitialized.
		Model() : mesh(NULL), MergeMeshes(true) {}
		uint32 MaterialOptions;

		std::string Path;
		bool MergeMeshes;

	};
};

#endif /* MODEL_H */
