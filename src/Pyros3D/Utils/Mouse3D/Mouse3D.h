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
#include "../../Other/Export.h"

namespace p3d {

    class PYROS3D_API Mouse3D {
    public:
        Mouse3D();    
        virtual ~Mouse3D();
        bool rayOrigin(const f32 &windowWidth, const f32 &windowHeight, const f32 &mouseX, const f32 &mouseY, const Matrix &ModelView, const Matrix &Projection);
        bool rayDirection(const f32 &windowWidth, const f32 &windowHeight, const f32 &mouseX, const f32 &mouseY, const Matrix &ModelView, const Matrix &Projection);
        bool rayIntersectionTriangle(const Vec3 &v1, const Vec3 &v2, const Vec3 &v3, Vec3 *IntersectionPoint32, f32 *t) const;
        const Vec3 &GetOrigin() const;
        const Vec3 &GetDirection() const;
    private:
        
        Vec3 Origin, Direction;
        bool UnProject(const f32& winX, const f32& winY, const f32& winZ, const f32 model[16], const f32 proj[16], const int32 view[4], f32 *objx, f32 *objy, f32 *objz);
        bool __gluInvertMatrixf(const f32 m[16], f32 invOut[16]);
        void __gluMultMatrixVecf(const f32 matrix[16], const f32 in[4], f32 out[4]);
        void __gluMultMatricesf(const f32 a[16], const f32 b[16], f32 r[16]);
    };

}

#endif  /* MOUSE3D_H */