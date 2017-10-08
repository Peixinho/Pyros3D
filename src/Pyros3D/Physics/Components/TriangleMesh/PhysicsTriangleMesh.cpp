//============================================================================
// Name        : PhysicsTriangleMesh.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics TriangleMesh  
//============================================================================


#include <Pyros3D/Physics/Components/TriangleMesh/PhysicsTriangleMesh.h>

namespace p3d {

	PhysicsTriangleMesh::PhysicsTriangleMesh(IPhysics* engine, RenderingComponent* rcomp, const f32 mass, bool ghost) : IPhysicsComponent(mass, CollisionShapes::TriangleMesh, engine, ghost)
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

	PhysicsTriangleMesh::PhysicsTriangleMesh(IPhysics* engine, const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass, bool ghost) : IPhysicsComponent(mass, CollisionShapes::TriangleMesh, engine, ghost)
	{
		// Build the triangle mesh from an index vector and vertex vector
		this->vertex = vertex;
		this->index = index;
	}

	//    void AddRendering(IRendering* rcomp)
	//    {
	//
	//    }
	//    void AddIndex(const unsigned &index)
	//    {
	//
	//    }
	//    void AddTriangle(const Vec3 &vertex1, const Vec3 &vertex2, const Vec3 &vertex4)
	//    {
	//
	//    }

	PhysicsTriangleMesh::~PhysicsTriangleMesh() {}

}
