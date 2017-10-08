//============================================================================
// Name        : PhysicsConvexHull.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics ConvexHull  
//============================================================================


#include <Pyros3D/Physics/Components/ConvexHull/PhysicsConvexHull.h>

namespace p3d {

	PhysicsConvexHull::PhysicsConvexHull(IPhysics* engine, const std::vector<Vec3>& points, const f32 mass, bool ghost) : IPhysicsComponent(mass, CollisionShapes::ConvexHull, engine, ghost)
	{
		vertex = points;
	}

	PhysicsConvexHull::~PhysicsConvexHull() {}

}
