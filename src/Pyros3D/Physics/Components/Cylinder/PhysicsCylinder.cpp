//============================================================================
// Name        : PhysicsCylinder.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Cylinder  
//============================================================================


#include <Pyros3D/Physics/Components/Cylinder/PhysicsCylinder.h>

namespace p3d { 

    PhysicsCylinder::PhysicsCylinder(IPhysics* engine, const f32 &radius, const f32 &height, const f32 &mass) : IPhysicsComponent(mass, CollisionShapes::Cylinder,engine)
    {
        
        this->radius=radius;
        this->height=height;
        
    }
    
    PhysicsCylinder::~PhysicsCylinder() {}

}
