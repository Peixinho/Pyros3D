//============================================================================
// Name        : PhysicsMultipleSphere.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics MultipleSphere  
//============================================================================

#ifndef PHYSICSMULTIPLESPHERE_H
#define	PHYSICSMULTIPLESPHERE_H

#include <Pyros3D/Physics/Components/IPhysicsComponent.h>

namespace p3d {

	class PYROS3D_API PhysicsMultipleSphere : public IPhysicsComponent {
	public:

		PhysicsMultipleSphere(IPhysics* engine, const std::vector<Vec3> &positions, const std::vector<f32> &radius, const f32 mass = 0.f);

		virtual ~PhysicsMultipleSphere();

		const std::vector<Vec3> &GetPositions() const { return positions; }
		const std::vector<f32> &GetRadius() const { return radius; }

	protected:

		std::vector <Vec3> positions;
		std::vector <f32> radius;

	};

}

#endif	/* PHYSICSMULTIPLESPHERE_H */

