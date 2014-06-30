//============================================================================
// Name        : PhysicsCone.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Cone  
//============================================================================

#ifndef PHYSICSCONE_H
#define	PHYSICSCONE_H

#include "../IPhysicsComponent.h"

namespace p3d {

    class PYROS3D_API PhysicsCone : public IPhysicsComponent {
        public:

            PhysicsCone(IPhysics* engine, const f32 &radius, const f32 &height, const f32 &mass = 0.f);

            virtual ~PhysicsCone();

            const f32 GetRadius()const  { return radius; }
            const f32 GetHeight()const  { return height; }
            
        protected:

            f32 radius;
            f32 height;

    };

}

#endif	/* PHYSICSCONE_H */