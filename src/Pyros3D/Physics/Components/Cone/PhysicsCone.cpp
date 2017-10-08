//============================================================================
// Name        : PhysicsCone.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Cone  
//============================================================================


#include <Pyros3D/Physics/Components/Cone/PhysicsCone.h>

namespace p3d {

	PhysicsCone::PhysicsCone(IPhysics* engine, const f32 radius, const f32 height, const f32 mass, bool ghost) : IPhysicsComponent(mass, CollisionShapes::Cone, engine, ghost)
	{

		this->radius = radius;
		this->height = height;

	}

	PhysicsCone::~PhysicsCone() {}

}
