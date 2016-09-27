//============================================================================
// Name        : PhysicsConvexTriangleMesh.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics ConvexTriangleMesh  
//============================================================================


#include <Pyros3D/Physics/Components/ConvexTriangleMesh/PhysicsConvexTriangleMesh.h>

namespace p3d {

	PhysicsConvexTriangleMesh::PhysicsConvexTriangleMesh(IPhysics* engine, RenderingComponent* rcomp, const f32 mass) : IPhysicsComponent(mass, CollisionShapes::ConvexTriangleMesh, engine)
	{
		// Build the triangle mesh from Rendering Component
		unsigned indexCount = 0;
		for (unsigned k = 0; k < rcomp->GetMeshes().size(); k++)
		{
			RenderingMesh* rc = (RenderingMesh*)rcomp->GetMeshes()[k];
			for (unsigned i = 0; i < rc->Geometry->GetIndexData().size(); i++)
			{
				index.push_back(indexCount++);
				vertex.push_back(rc->Geometry->GetVertexData()[rc->Geometry->GetIndexData()[i]]);
			}
		}
	}

	PhysicsConvexTriangleMesh::PhysicsConvexTriangleMesh(IPhysics* engine, const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass) : IPhysicsComponent(mass, CollisionShapes::ConvexTriangleMesh, engine)
	{
		this->vertex = vertex;
		this->index = index;
	}

	void AddRendering(RenderingComponent* rcomp)
	{

	}
	void AddIndex(const uint32 index)
	{

	}
	void AddTriangle(const Vec3 &vertex1, const Vec3 &vertex2, const Vec3 &vertex4)
	{

	}

	PhysicsConvexTriangleMesh::~PhysicsConvexTriangleMesh() {}

}
