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
		Gather(rcomp);
	}

	void PhysicsTriangleMesh::Rebuild(RenderingComponent* rcomp)
	{
		if (!rcomp) return;
		index.clear();
		vertex.clear();
		Gather(rcomp);
		// (the body, if there is one yet: taken down and made again from what is here now)
		if (PhysicsEngine && RigidBodyRegistered())
			PhysicsEngine->RebuildPhysicsComponent(this);
	}

	void PhysicsTriangleMesh::Gather(RenderingComponent* rcomp)
	{
		// The model's own vertices, each once, and its triangles as indices into them. (This used to
		// write out three vertices for every triangle: a city's colliders held their geometry
		// several times over, 45 MB of a dedicated server's memory that nothing read again.)
		size_t vertexTotal = 0, indexTotal = index.size();
		for (unsigned k = 0; k < rcomp->GetMeshes().size(); k++)
		{
			RenderingMesh* rc = (RenderingMesh*)rcomp->GetMeshes()[k];
			vertexTotal += rc->Geometry->GetVertexData().size();
			const size_t n = rc->Geometry->GetIndexData().size();
			indexTotal += (rc->Material && rc->Material->GetCullFace() == CullFace::DoubleSided) ? n * 2 : n;
		}
		vertex.reserve(vertex.size() + vertexTotal);
		index.reserve(indexTotal);
		for (unsigned k = 0; k < rcomp->GetMeshes().size(); k++)
		{
			RenderingMesh* rc = (RenderingMesh*)rcomp->GetMeshes()[k];
			const std::vector<uint32> &meshIndex = rc->Geometry->GetIndexData();
			const std::vector<Vec3> &meshVertex = rc->Geometry->GetVertexData();
			const unsigned base = (unsigned)vertex.size();
			vertex.insert(vertex.end(), meshVertex.begin(), meshVertex.end());
			const size_t vn = meshVertex.size();
			for (size_t i = 0; i + 2 < meshIndex.size(); i += 3)
			{
				// (an index past the mesh's vertices would have the physics engine read past them)
				if (meshIndex[i] >= vn || meshIndex[i + 1] >= vn || meshIndex[i + 2] >= vn) continue;
				index.push_back(base + meshIndex[i]);
				index.push_back(base + meshIndex[i + 1]);
				index.push_back(base + meshIndex[i + 2]);
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
					if (meshIndex[i] >= vn || meshIndex[i + 1] >= vn || meshIndex[i + 2] >= vn) continue;
					index.push_back(base + meshIndex[i]);
					index.push_back(base + meshIndex[i + 2]);
					index.push_back(base + meshIndex[i + 1]);
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
