//============================================================================
// Name        : PhysicsBox.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Box  
//============================================================================


#include "PhysicsBox.h"

namespace p3d {   

    PhysicsBox::PhysicsBox(IPhysics* engine, const f32 &width, const f32 &height, const f32 &depth, const f32 &mass) : IPhysicsComponent(mass,CollisionShapes::Box, engine)
    {
        
        this->width=width;
        this->height=height;
        this->depth=depth;
        
    }

    PhysicsBox::~PhysicsBox() {}

}
