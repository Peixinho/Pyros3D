//============================================================================
// Name        : PhysicsTriangleMesh.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics TriangleMesh  
//============================================================================


#include "PhysicsTriangleMesh.h"

namespace p3d {   

    PhysicsTriangleMesh::PhysicsTriangleMesh(IPhysics* engine, RenderingComponent* rcomp, const f32 &mass) : IPhysicsComponent(mass,CollisionShapes::TriangleMesh,engine) 
    {
        // Build the triangle mesh from IRendering 
        // If there are many subs, add them as childs
//        if (rcomp->HaveSubComponents())
//        {
//            unsigned indexCount = 0;
//            for (unsigned k=0;k<rcomp->GetSubComponents().size();k++)
//            {
//				IRenderingComponent* rc = (IRenderingComponent*) rcomp->GetSubComponents()[k].Get();
//                for (unsigned i=0;i<rc->GetIndexData().size();i++)
//                {
//                    index.push_back(indexCount++);
//                    vertex.push_back(rc->GetVertexData()[rc->GetIndexData()[i]]);
//                }
//            }
//        }
    }
    
    PhysicsTriangleMesh::PhysicsTriangleMesh(IPhysics* engine, const std::vector<unsigned> &index, const std::vector<Vec3> &vertex, const f32 &mass) : IPhysicsComponent(mass,CollisionShapes::TriangleMesh,engine) 
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
