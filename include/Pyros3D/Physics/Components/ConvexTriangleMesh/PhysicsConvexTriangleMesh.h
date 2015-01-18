//============================================================================
// Name        : PhysicsConvexTriangleMesh.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics ConvexTriangleMesh  
//============================================================================

#ifndef PHYSICSCONVEXTRIANGLEMESH_H
#define	PHYSICSCONVEXTRIANGLEMESH_H

#include <Pyros3D/Physics/Components/IPhysicsComponent.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>

namespace p3d {

    class PYROS3D_API PhysicsConvexTriangleMesh : public IPhysicsComponent {
        public:

            PhysicsConvexTriangleMesh(IPhysics* engine, RenderingComponent* rcomp, const f32 mass = 0.f);
            PhysicsConvexTriangleMesh(IPhysics* engine, const std::vector<unsigned> &index, const std::vector<Vec3> &vertex, const f32 mass = 0.f);

            virtual ~PhysicsConvexTriangleMesh();

            const std::vector<unsigned> &GetIndexData() const { return index; }
            const std::vector<Vec3> &GetVertexData() const { return vertex; }
            
        protected:
            
            // Store Triangle Mesh Data
            std::vector<unsigned> index;
            std::vector<Vec3> vertex;
            
    };

}

#endif	/* PHYSICSCONVEXTRIANGLEMESH_H */

