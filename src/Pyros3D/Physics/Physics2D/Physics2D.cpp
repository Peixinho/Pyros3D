//============================================================================
// Name        : Physics2D
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Box2D-backed rigid body for 2D scenes
//============================================================================

#include <Pyros3D/Physics/Physics2D/Physics2D.h>

namespace p3d {

	Physics2D::Physics2D(const uint32 bodyType, const uint32 shape, const Vec2 &size)
	{
		this->bodyType = bodyType;
		this->shapeType = shape;
		this->size = size;
		density = 1.f;
		friction = 0.3f;
		restitution = 0.f;
		fixedRotation = false;
		haveBody = false;
		bodyIndex = 0;
		bodyWorld = 0;
		bodyGeneration = 0;
	}

	Physics2D::~Physics2D() {}

	void Physics2D::SetBodyHandle(const int32 index, const uint16 world, const uint16 generation)
	{
		bodyIndex = index;
		bodyWorld = world;
		bodyGeneration = generation;
		haveBody = true;
	}

};
