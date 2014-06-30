//============================================================================
// Name        : PhysicsConvexHull.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics ConvexHull  
//============================================================================

#ifndef PHYSICSCONVEXHULL_H
#define	PHYSICSCONVEXHULL_H

#include "../IPhysicsComponent.h"
#include <vector>

namespace p3d {

    class PYROS3D_API PhysicsConvexHull : public IPhysicsComponent {
        public:

            PhysicsConvexHull(IPhysics* engine, const std::vector<Vec3> &points, const f32 &mass = 0.f);

            virtual ~PhysicsConvexHull();
            
            const std::vector<Vec3> &GetPoints() const { return vertex; }
            
        protected:
            
            std::vector<Vec3> vertex;
            
    };

}

#endif	/* PHYSICSCONVEXHULL_H */

