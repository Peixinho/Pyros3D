//============================================================================
// Name        : Projection.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Projection
//============================================================================

#include "Projection.h"

namespace Fishy3D {

    Projection::Projection() {}

    Projection::~Projection() {
    }

    Matrix Projection::GetMatrix() const {
        return projection;
    }

    void Projection::MakeOrthoRH(const float &left, const float &right, const float &bottom, const float &top, const float &near, const float &far) {
        
        Left = left;
        Right = right;
        Bottom = bottom;
        Top = top;
        Far = far;
        Near = near;
        
        projection=Matrix::OrthoMatrixRH(left,right,bottom,top,near,far);
        isOrtho = true;
    }
    void Projection::MakeOrthoLH(const float &left, const float &right, const float &bottom, const float &top, const float &near, const float &far) {
        
        Left = left;
        Right = right;
        Bottom = bottom;
        Top = top;
        Far = far;
        Near = near;
        
        projection=Matrix::OrthoMatrixLH(left,right,bottom,top,near,far);
        isOrtho = true;
    }
    void Projection::MakePerspectiveRH(const float &fov, const float &aspect, const float &near, const float &far) {
        
        Fov = fov;
        Aspect = aspect;
        Near = near;
        Far = far;
        
        projection=Matrix::PerspectiveMatrixRH(fov, aspect, near, far);
        isOrtho = false;
    }
    void Projection::MakePerspectiveLH(const float &fov, const float &aspect, const float &near, const float &far) {
        
        Fov = fov;
        Aspect = aspect;
        Near = near;
        Far = far;
        
        projection=Matrix::PerspectiveMatrixLH(fov, aspect, near, far);
        isOrtho = false;
    }
    
}
