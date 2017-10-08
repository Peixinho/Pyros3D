//============================================================================
// Name        : PhysicsSphere.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Sphere  
//============================================================================

#ifndef PHYSICSSPHERE_H
#define	PHYSICSSPHERE_H

#include <Pyros3D/Physics/Components/IPhysicsComponent.h>

namespace p3d {

	class PYROS3D_API PhysicsSphere : public IPhysicsComponent {
	public:

		PhysicsSphere();
		PhysicsSphere(IPhysics* engine, const f32 radius, const f32 mass = 0.f, bool ghost = false);

		virtual ~PhysicsSphere();

		const f32 GetRadius() const { return radius; }

	protected:

		f32 radius;

	};

}

#endif	/* PHYSICSSPHERE_H */

