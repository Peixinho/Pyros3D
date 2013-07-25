//============================================================================
// Name        : Projection.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Projection
//============================================================================

#ifndef PROJECTION_H
#define	PROJECTION_H

#include "../Math/Math.h"

namespace p3d {

    using namespace Math;
    
        class Projection {
            public:

                // vars
                Matrix m;
            
                // for projection only
                f32 Near, Far, Left, Right, Top, Bottom, Aspect, Fov;
            
                // methods
                Projection();

                void Perspective(const f32 &fov, const f32 &aspect, const f32 &near, const f32 &far);
                void Ortho(const f32 &left, const f32 &right, const f32 &bottom, const f32 &top, const f32 &near, const f32 &far);
            
                Matrix GetProjectionMatrix();

        };
    };

#endif	/* PROJECTION_H */

