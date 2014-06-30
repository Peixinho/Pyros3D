//============================================================================
// Name        : PhysicsTriangleMesh.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics TriangleMesh  
//============================================================================

#ifndef PHYSICSTRIANGLEMESH_H
#define	PHYSICSTRIANGLEMESH_H

#include "../IPhysicsComponent.h"
#include "../../../Rendering/Components/Rendering/RenderingComponent.h"

namespace p3d {

    class PYROS3D_API PhysicsTriangleMesh : public IPhysicsComponent {
        public:

            PhysicsTriangleMesh(IPhysics* engine, RenderingComponent* rcomp, const f32 &mass = 0.f);
            PhysicsTriangleMesh(IPhysics* engine, const std::vector<unsigned> &index, const std::vector<Vec3> &vertex, const f32 &mass = 0.f);

            virtual ~PhysicsTriangleMesh();

            const std::vector<unsigned> &GetIndexData() const { return index; }
            const std::vector<Vec3> &GetVertexData() const { return vertex; }
            
        protected:
            
            // Store Triangle Mesh Data
            std::vector<unsigned> index;
            std::vector<Vec3> vertex;
            
    };

}

#endif	/* PHYSICSTRIANGLEMESH_H */

