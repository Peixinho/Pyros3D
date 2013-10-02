//============================================================================
// Name        : Projection.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Projection
//============================================================================

#ifndef PROJECTION_H
#define	PROJECTION_H

#include "../Math/Matrix.h"
#include "../../Utils/Pointers/SuperSmartPointer.h"

namespace Fishy3D {

    class Projection {

        public:                        
                        
            float Fov, Aspect, Near, Far;
            float Left, Right, Bottom, Top;
            
            bool isOrtho;

            Projection();
            ~Projection();

            void MakePerspectiveRH(const float &fov, const float &aspect, const float &near, const float &far);
            void MakeOrthoRH(const float &left, const float &right, const float &bottom, const float &top, const float &near, const float &far);
            void MakePerspectiveLH(const float &fov, const float &aspect, const float &near, const float &far);
            void MakeOrthoLH(const float &left, const float &right, const float &bottom, const float &top, const float &near, const float &far);
            Matrix GetMatrix() const;          

        private:
            
            Matrix projection;
            
        };                   
}

#endif	/* PROJECTION_H */

