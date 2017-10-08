//============================================================================
// Name        : PhysicsStaticPlane.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics StaticPlane  
//============================================================================


#include <Pyros3D/Physics/Components/StaticPlane/PhysicsStaticPlane.h>

namespace p3d {

	PhysicsStaticPlane::PhysicsStaticPlane(IPhysics* engine, const Vec3& Normal, const f32 Constant, const f32 mass, bool ghost) : IPhysicsComponent(mass, CollisionShapes::StaticPlane, engine, ghost)
	{

		this->normal = Normal;
		this->constant = Constant;

	}

	PhysicsStaticPlane::~PhysicsStaticPlane() {}

}
