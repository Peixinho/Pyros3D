//============================================================================
// Name        : Projection.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Projection
//============================================================================
#include <Pyros3D/Core/Projection/Projection.h>

namespace p3d {
    
        Projection::Projection() {}
        
        void Projection::Perspective(const f32 fov, const f32 aspect, const f32 near, const f32 far)
        {
            Near = near;
            Far = far;
            Aspect = aspect;
            Fov = fov;
            
            m = Matrix::PerspectiveMatrix(fov,aspect,near,far);
            
        }
        void Projection::Ortho(const f32 left, const f32 right, const f32 bottom, const f32 top, const f32 near, const f32 far)
        {
            Near = near;
            Far = far;
            Left = left;
            Right = right;
            Top = top;
            Bottom = bottom;
            
            m = Matrix::OrthoMatrix(left,right,bottom,top,near,far);
        }
        
        Matrix Projection::GetProjectionMatrix()
        {
            return m;
        }
};
