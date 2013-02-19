//============================================================================
// Name        : Mouse3D.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Mouse 3D Class
//============================================================================

#ifndef MOUSE3D_H
#define MOUSE3D_H

#include "../../Core/Math/Math.h"

namespace Fishy3D {

    class Mouse3D {
    public:
        Mouse3D();    
        virtual ~Mouse3D();
        bool rayOrigin(const float &windowWidth, const float &windowHeight, const float &mouseX, const float &mouseY, const Matrix &ModelView, const Matrix &Projection);
        bool rayDirection(const float &windowWidth, const float &windowHeight, const float &mouseX, const float &mouseY, const Matrix &ModelView, const Matrix &Projection);
        bool rayIntersectionTriangle(const vec3 &v1, const vec3 &v2, const vec3 &v3, vec3 *IntersectionPoint, float *t) const;
        const vec3 &GetOrigin() const;
        const vec3 &GetDirection() const;
    private:
        
        vec3 Origin, Direction;
        bool UnProject(const float& winX, const float& winY, const float& winZ, const float model[16], const float proj[16], const int view[4], float *objx, float *objy, float *objz);
        bool __gluInvertMatrixf(const float m[16], float invOut[16]);
        void __gluMultMatrixVecf(const float matrix[16], const float in[4], float out[4]);
        void __gluMultMatricesf(const float a[16], const float b[16], float r[16]);
    };

}

#endif  /* MOUSE3D_H */