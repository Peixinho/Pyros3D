//============================================================================
// Name        : PhysicsConvexTriangleMesh.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics ConvexTriangleMesh  
//============================================================================


#include "PhysicsConvexTriangleMesh.h"

namespace p3d {   

	PhysicsConvexTriangleMesh::PhysicsConvexTriangleMesh(IPhysics* engine, RenderingComponent* rcomp, const f32 &mass) : IPhysicsComponent(mass,CollisionShapes::ConvexTriangleMesh,engine) 
    {
        // Build the triangle mesh from IRendering 
        // If there are many subs, add them as childs
//        if (rcomp->HaveSubs())
//        {
//            unsigned indexCount = 0;
//            for (unsigned k=0;k<rcomp->GetSubs().size();k++)
//            {
//                IRendering* rc = (IRendering*) rcomp->GetSubs()[k].Get();
//                for (unsigned i=0;i<rc->GetIndexData().size();i++)
//                {
//                    index.push_back(indexCount++);
//                    vertex.push_back(rc->GetVertexData()[rc->GetIndexData()[i]]);
//                }
//            }
//        }
    }
    
    PhysicsConvexTriangleMesh::PhysicsConvexTriangleMesh(IPhysics* engine, const std::vector<unsigned> &index, const std::vector<Vec3> &vertex, const f32 &mass) : IPhysicsComponent(mass,CollisionShapes::ConvexTriangleMesh,engine) 
    {
        this->vertex = vertex;
        this->index = index;
    }

    void AddRendering(RenderingComponent* rcomp)
    {

    }
    void AddIndex(const unsigned &index)
    {

    }
    void AddTriangle(const Vec3 &vertex1, const Vec3 &vertex2, const Vec3 &vertex4)
    {

    }

    PhysicsConvexTriangleMesh::~PhysicsConvexTriangleMesh() {}

}
