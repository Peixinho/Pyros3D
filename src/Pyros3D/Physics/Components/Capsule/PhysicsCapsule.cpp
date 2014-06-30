//============================================================================
// Name        : PhysicsCapsule.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Capsule  
//============================================================================


#include <Pyros3D/Physics/Components/Capsule/PhysicsCapsule.h>

namespace p3d { 

    PhysicsCapsule::PhysicsCapsule(IPhysics* engine, const f32 &radius, const f32 &height, const f32 &mass) : IPhysicsComponent(mass, CollisionShapes::Capsule, engine)
    {
        
        this->radius=radius;
        this->height=height;
        
    }
    
    PhysicsCapsule::~PhysicsCapsule() {}

}
