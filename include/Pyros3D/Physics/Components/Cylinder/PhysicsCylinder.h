//============================================================================
// Name        : PhysicsCylinder.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Cylinder  
//============================================================================

#ifndef PHYSICSCYLINDER_H
#define	PHYSICSCYLINDER_H

#include <Pyros3D/Physics/Components/IPhysicsComponent.h>

namespace p3d {

    class PYROS3D_API PhysicsCylinder : public IPhysicsComponent {
        public:

            PhysicsCylinder();
            PhysicsCylinder(IPhysics* engine, const f32 radius, const f32 height, const f32 mass = 0.f);

            virtual ~PhysicsCylinder();
            
            const f32 GetRadius() const { return radius; }
            const f32 GetHeight() const { return height; }
            
        protected:

            f32 radius;
            f32 height;

    };

}

#endif	/* PHYSICSCYLINDER_H */

