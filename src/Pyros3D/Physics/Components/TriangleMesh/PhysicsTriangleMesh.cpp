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
		fromRenderable = true;
		unsigned indexCount = 0;
		for (unsigned k = 0; k < rcomp->GetMeshes().size(); k++)
		{
			RenderingMesh* rc = (RenderingMesh*)rcomp->GetMeshes()[k];
			const std::vector<uint32> &meshIndex = rc->Geometry->GetIndexData();
			const std::vector<Vec3> &meshVertex = rc->Geometry->GetVertexData();
			for (unsigned i = 0; i < meshIndex.size(); i++)
			{
				index.push_back(indexCount++);
				vertex.push_back(meshVertex[meshIndex[i]]);
			}
			// A triangle only stops what comes at its front. A surface that
			// is drawn from both sides is solid from both: the same
			// triangles again, wound the other way. (A building whose walls
			// have a single face would otherwise let anything inside it
			// walk out.)
			if (rc->Material && rc->Material->GetCullFace() == CullFace::DoubleSided)
			{
				for (size_t i = 0; i + 2 < meshIndex.size(); i += 3)
				{
					index.push_back(indexCount++); vertex.push_back(meshVertex[meshIndex[i]]);
					index.push_back(indexCount++); vertex.push_back(meshVertex[meshIndex[i + 2]]);
					index.push_back(indexCount++); vertex.push_back(meshVertex[meshIndex[i + 1]]);
				}
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
