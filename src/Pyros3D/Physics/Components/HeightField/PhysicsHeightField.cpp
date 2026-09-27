//============================================================================
// Name        : PhysicsHeightField.cpp
// Author      : Duarte Peixinho
// Description : See PhysicsHeightField.h.
//============================================================================

#include <Pyros3D/Physics/Components/HeightField/PhysicsHeightField.h>

namespace p3d {

	PhysicsHeightField::PhysicsHeightField(IPhysics* engine, const std::shared_ptr<const HeightfieldData> &data)
		: IPhysicsComponent(0.f, CollisionShapes::HeightFieldTerrain, engine, false), data(data) {}

	PhysicsHeightField::~PhysicsHeightField() {}

}
