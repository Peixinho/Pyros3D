//============================================================================
// Name        : PhysicsSphere.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Box  
//============================================================================


#include "PhysicsSphere.h"

namespace p3d {  

    PhysicsSphere::PhysicsSphere(IPhysics* engine, const f32 &radius, const f32 &mass) : IPhysicsComponent(mass, CollisionShapes::Sphere,engine)
    {
        
        this->radius=radius;
        
    }

    PhysicsSphere::~PhysicsSphere() {}

}
