//============================================================================
// Name        : Projection.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Projection
//============================================================================
#include "Projection.h"

namespace p3d {
    
        Projection::Projection() {}
        
        void Projection::PerspectiveRH(const f32 &fov, const f32 &aspect, const f32 &near, const f32 &far)
        {
            Near = near;
            Far = far;
            Aspect = aspect;
            Fov = fov;
            
            m = Matrix::PerspectiveMatrixRH(fov,aspect,near,far);
            
        }
        void Projection::PerspectiveLH(const f32 &fov, const f32 &aspect, const f32 &near, const f32 &far)
        {
            Near = near;
            Far = far;
            Aspect = aspect;
            Fov = fov;
            
            m = Matrix::PerspectiveMatrixLH(fov,aspect,near,far);
            
        }
        void Projection::OrthoRH(const f32 &left, const f32 &right, const f32 &bottom, const f32 &top, const f32 &near, const f32 &far)
        {
            Near = near;
            Far = far;
            Left = left;
            Right = right;
            Top = top;
            Bottom = bottom;
            
            m = Matrix::OrthoMatrixRH(left,right,bottom,top,near,far);
        }
        void Projection::OrthoLH(const f32 &left, const f32 &right, const f32 &bottom, const f32 &top, const f32 &near, const f32 &far)
        {
            Near = near;
            Far = far;
            Left = left;
            Right = right;
            Top = top;
            Bottom = bottom;
            
            m = Matrix::OrthoMatrixRH(left,right,bottom,top,near,far);
        }
        
        Matrix Projection::GetProjectionMatrix()
        {
            return m;
        }
};
