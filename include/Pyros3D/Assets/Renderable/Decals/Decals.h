//============================================================================
// Name        : Decals.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Decals Geometry based on:
// https://github.com/spite/THREE.DecalGeometry
// http://blog.wolfire.com/2009/06/how-to-project-decals/
//============================================================================

#ifndef DECALS_H
#define DECALS_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>

namespace p3d {

	struct PYROS3D_API DecalVertex {

		Vec3	vertex,
			normal;
		Vec2	uv;

		// Bones
		Vec4
			bonesID,
			bonesWeight;

		DecalVertex() {}

		DecalVertex(Vec3 v, Vec3 n)
		{
			vertex = v;
			normal = n;
		}

		DecalVertex(Vec3 v, Vec3 n, Vec2 u)
		{
			vertex = v;
			normal = n;
			uv = u;
		}

		DecalVertex(Vec3 v, Vec3 n, Vec2 u, Vec4 BoneIDs, Vec4 BoneWeights)
		{
			vertex = v;
			normal = n;
			uv = u;
			bonesID = BoneIDs;
			bonesWeight = BoneWeights;
		}

		std::string toString()
		{
			return "Vertex: " + vertex.toString() + " - Normal: " + normal.toString() + " - UV: " + uv.toString();
		}
	};

	class PYROS3D_API Decal : public Model
	{
	public:

		Decal(std::vector<DecalVertex> vertices, bool haveBones = false);

	};

	class PYROS3D_API DecalGeometry
	{

	private:

		f32 size;
		Matrix targetTransformation, CubeMatrix, iCubeMatrix;
		Vec3 position, rotation, dimensions, check;
		RenderingMesh* mesh;
		Renderable* decal;
		bool haveBones;

		void clipFace(std::vector<DecalVertex> &inVertices, Vec3 plane);
		DecalVertex clip(DecalVertex v0, DecalVertex v1, Vec3 p);
		void ComputeDecal();

	public:

		DecalGeometry(RenderingMesh* mesh, Matrix targetTransformation, Vec3 position, Vec3 rotation, Vec3 dimensions, Vec3 check = Vec3(1, 1, 1));
		DecalGeometry(RenderingMesh* mesh, Matrix targetTransformation, Matrix transform, Vec3 dimensions, Vec3 check = Vec3(1, 1, 1));
		Renderable* GetDecal() { return decal; }
		virtual ~DecalGeometry();

	};

};

#endif /* DECALS_H */