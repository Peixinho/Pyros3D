//============================================================================
// Name        : PhysicsStaticPlane.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics StaticPlane  
//============================================================================


#include "PhysicsStaticPlane.h"

namespace p3d {   

    PhysicsStaticPlane::PhysicsStaticPlane(IPhysics* engine, const Vec3& Normal, const f32& Constant, const f32& mass) : IPhysicsComponent(mass,CollisionShapes::StaticPlane,engine)
    {
        
        this->normal = Normal;
        this->constant = Constant;
        
    }

    PhysicsStaticPlane::~PhysicsStaticPlane() {}

}
