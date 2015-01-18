//============================================================================
// Name        : PhysicsMultipleSphere.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics MultipleSphere  
//============================================================================


#include <Pyros3D/Physics/Components/MultipleSphere/PhysicsMultipleSphere.h>

namespace p3d {   

    PhysicsMultipleSphere::PhysicsMultipleSphere(IPhysics* engine, const std::vector<Vec3> &positions, const std::vector<f32> &radius, const f32 mass) : IPhysicsComponent(mass,CollisionShapes::MultipleSphere,engine)
    {
        this->positions = positions;
        this->radius = radius;
    }

    PhysicsMultipleSphere::~PhysicsMultipleSphere() {}

}
