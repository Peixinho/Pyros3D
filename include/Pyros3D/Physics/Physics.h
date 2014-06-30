//============================================================================
// Name        : Physics.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics
//============================================================================

#ifndef PHYSICS_H
#define PHYSICS_H

#include "../Other/Export.h"

#define BULLETPHYSICS

#ifdef BULLETPHYSICS
#include "PhysicsEngines/BulletPhysics/BulletPhysics.h"
#endif

namespace p3d {

    #ifdef BULLETPHYSICS
    class PYROS3D_API Physics : public BulletPhysics {
    #endif
        
        public:

            Physics();
            virtual ~Physics();
        
    };
    
};

#endif /*PHYSICS_H*/