//============================================================================
// Name        : PhysicsCapsule.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Capsule  
//============================================================================

#ifndef PHYSICSCAPSULE_H
#define	PHYSICSCAPSULE_H

#include <Pyros3D/Physics/Components/IPhysicsComponent.h>

namespace p3d {

	class PYROS3D_API PhysicsCapsule : public IPhysicsComponent {
	public:

		PhysicsCapsule();
		PhysicsCapsule(IPhysics* engine, const f32 radius, const f32 height, const f32 mass = 0.f, bool ghost = false);

		virtual ~PhysicsCapsule();

		const f32 GetRadius()const { return radius; }
		const f32 GetHeight()const { return height; }

	protected:

		f32 radius;
		f32 height;

	};

}

#endif	/* PHYSICSCAPSULE_H */

