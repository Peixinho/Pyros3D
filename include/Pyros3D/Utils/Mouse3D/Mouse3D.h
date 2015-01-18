//============================================================================
// Name        : Mouse3D.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Mouse 3D Class
//============================================================================

#ifndef MOUSE3D_H
#define MOUSE3D_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>

#undef isnan
#define isnan(x) ((x) != (x))

namespace p3d {

    class PYROS3D_API Mouse3D {
    public:
        Mouse3D();    
        virtual ~Mouse3D();
        bool GenerateRay(const f32 windowWidth, const f32 windowHeight, const f32 mouseX, const f32 mouseY, const Matrix &Model, const Matrix &View, const Matrix &Projection);
        bool rayIntersectionTriangle(const Vec3 &v1, const Vec3 &v2, const Vec3 &v3, Vec3 *IntersectionPoint32, f32 *t) const;
        bool rayIntersectionPlane(const Vec3 &Normal, const Vec3 &Position, Vec3 *IntersectionPoint32) const;
        const Vec3 &GetOrigin() const;
        const Vec3 &GetDirection() const;
    private:
        
        Vec3 Origin, Direction;
        bool UnProject(const f32 winX, const f32 winY, const f32 winZ, const Matrix &modelview, const Matrix &proj, const Vec4 view, f32 *objx, f32 *objy, f32 *objz);
    };

}

#endif  /* MOUSE3D_H */