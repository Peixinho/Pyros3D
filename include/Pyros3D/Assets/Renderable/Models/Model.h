//============================================================================
// Name        : Model
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Model Geometry
//============================================================================

#ifndef MODEL_H
#define MODEL_H

#include <memory>
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

		// One Model for a file, however many ask for it: the same object
		// while anybody still holds it, read again once nobody does. What a
		// script's Model.new() returns. Two things drawn from the same Model
		// can be drawn together (automatic instancing); two copies of the
		// file cannot, and each costs the file's read and its buffers again.
		static std::shared_ptr<Model> LoadShared(const std::string &ModelPath, bool mergeMeshes = true);
		// The file changed on disk: the next LoadShared() reads it again.
		static void ForgetShared(const std::string &ModelPath);
		// Every file LoadShared() has been asked for since the last call to
		// this with clear = true, in the order first asked (what a preload
		// list is made from - see AssetPreload).
		static std::vector<std::string> SharedRequests(const bool clear = false);

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

	// A model with fewer triangles, made from the model itself: its vertices
	// are gathered into the cells of a grid and each cell becomes one vertex,
	// the grid chosen so that about `ratio` of the triangles are left. Coarse
	// - it keeps the outline, not the surface - which is what something drawn
	// far away, or only into a shadow map, needs. Same sub-meshes in the same
	// order as the model, each with its material properties, so whatever
	// materials the model is drawn with fit this too.
	class PYROS3D_API SimplifiedModel : public Renderable {
	public:
		SimplifiedModel(const std::shared_ptr<Model> &source, const f32 ratio);
		virtual ~SimplifiedModel() {}

		const std::string &GetPath() const { return Path; }
		f32 GetRatio() const { return Ratio; }
		uint32 GetTriangleCount() const { return Triangles; }
		uint32 GetSourceTriangleCount() const { return SourceTriangles; }

		// One for a file and a ratio, while anybody holds it.
		static std::shared_ptr<SimplifiedModel> LoadShared(const std::string &ModelPath, const f32 ratio);

	protected:
		std::string Path;
		f32 Ratio;
		uint32 Triangles, SourceTriangles;
	};
};

#endif /* MODEL_H */
