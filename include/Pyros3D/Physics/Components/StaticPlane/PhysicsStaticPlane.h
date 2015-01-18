//============================================================================
// Name        : PhysicsStaticPlane.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics StaticPlane  
//============================================================================

#ifndef PHYSICSSTATICPLANE_H
#define	PHYSICSSSTATICPLANE_H

#include <Pyros3D/Physics/Components/IPhysicsComponent.h>

namespace p3d {

    class PYROS3D_API PhysicsStaticPlane : public IPhysicsComponent {
        public:

            PhysicsStaticPlane(IPhysics* engine, const Vec3 &Normal, const f32 Constant, const f32 mass = 0.f);

            virtual ~PhysicsStaticPlane();
            
            const Vec3 GetNormal() const { return normal; }
            const f32 GetConstant() const { return constant; }
            
        protected:

            Vec3 normal;
            f32 constant;

    };

}

#endif	/* PHYSICSSTATICPLANE_H */

