//============================================================================
// Name        : Physics.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics
//============================================================================

#ifndef PHYSICS_H
#define PHYSICS_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Physics/PhysicsEngines/Box3D/Box3DPhysics.h>

namespace p3d {

	class PYROS3D_API Physics : public Box3DPhysics {

	public:

		Physics();
		virtual ~Physics();

	};

};

#endif /*PHYSICS_H*/
