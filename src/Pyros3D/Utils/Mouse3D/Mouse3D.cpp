//============================================================================
// Name        : Mouse3D.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Mouse 3D Class
//============================================================================

#include <Pyros3D/Utils/Mouse3D/Mouse3D.h>
#include <stdio.h>
#include <string.h>
namespace p3d {

    Mouse3D::Mouse3D() {
    }

    Mouse3D::~Mouse3D() {
    }

    const Vec3 &Mouse3D::GetDirection() const
    {
        return Direction;
    }
    const Vec3 &Mouse3D::GetOrigin() const
    {
        return Origin;
    }
    bool Mouse3D::GenerateRay(const f32 &windowWidth, const f32 &windowHeight, const f32 &mouseX, const f32 &mouseY, const Matrix &Model, const Matrix &View, const Matrix &Projection) 
    {
        
        f32 glMouseY = windowHeight-mouseY;
        
        int32 viewPort[4];
        f32 x1,y1,z1,x2,y2,z2;
                       
        viewPort[0]=0; viewPort[1]=0; viewPort[2]=windowWidth; viewPort[3]=windowHeight;
        
        Vec3 v1,v2;
        
        if (UnProject(mouseX,glMouseY,0,(View*Model),Projection,Vec4(viewPort[0],viewPort[1],viewPort[2],viewPort[3]),&x1,&y1,&z1) == true 
            &&
            UnProject(mouseX,glMouseY,1,(View*Model),Projection,Vec4(viewPort[0],viewPort[1],viewPort[2],viewPort[3]),&x2,&y2,&z2) == true)
        {
            
            v1.x=x1;    v1.y=y1;    v1.z=z1;
            v2.x=x2;    v2.y=y2;    v2.z=z2;

            v2 = v2-v1;
            v2.normalizeSelf();
            
            Origin = v1;
            Direction = v2;               

            return true;
        }
        return false;
    }

    bool Mouse3D::rayIntersectionTriangle(const Vec3 &v1, const Vec3 &v2, const Vec3 &v3, Vec3 *IntersectionPoint32, f32 *t) const
    {

        Vec3 V1 = v1;
        Vec3 V2 = v2;
        Vec3 V3 = v3;
        
        Vec3 e1 = V2 - V1, e2 = V3 - V1;
        Vec3 pvec = Direction.cross(e2);
        f32 det = e1.dotProduct(pvec);
        
        if (det > -EPSILON && det < EPSILON) 
        {
            return false;
        }

        f32 inv_det = 1.0f / det;
        Vec3 tvec = Origin - V1;
        f32 u = tvec.dotProduct(pvec) * inv_det;

        if (u < 0.0 || u > 1.0)
        {
            return false;
        };

        Vec3 qvec = tvec.cross(e1);
        f32 v = Direction.dotProduct(qvec) * inv_det;

        if (v < 0.0 || u + v > 1.0)
        {
            return false;
        };

        f32 _t = e2.dotProduct(qvec) * inv_det;

        if(!isnan((double)_t))
        {
            *t = _t;
        } else return false;

        if(IntersectionPoint32)
        {
            *IntersectionPoint32 = Origin + (Direction * _t);
        } else return false;
        return true;
    }

    bool Mouse3D::rayIntersectionPlane(const Vec3 &Normal, const Vec3 &Position, Vec3 *IntersectionPoint32) const
    {
        Vec3 intersectPoint;

        Vec3 u = Direction - Origin;
        Vec3 w = Origin - Position;
        
        f32 D = Normal.dotProduct(u);
        f32 N = -Normal.dotProduct(w);

        // if (fabs(D) < EPSILON)            // segment is parallel to plane
        // {          
        //     if (N == 0)                     // segment lies in plane
        //         return false;
        //     else
        //         return false;                   // no intersection
        // }

        float s = N / D;
        // if (s < 0 || s > 1)
        //     return false;                      // no intersection

        intersectPoint = Origin + u*s;         // compute segment intersect point

        // IntersectionPoint32->x  = intersectPoint.x;
        // IntersectionPoint32->y  = intersectPoint.y;
        // IntersectionPoint32->z  = intersectPoint.z;

        f32 denom = Normal.dotProduct(Direction);
        Vec3 p0l0 = Position - Origin;
        f32 _t = p0l0.dotProduct(Normal) / denom; 
        *IntersectionPoint32 = Origin + (Direction * _t);
        return true;
    }
    bool Mouse3D::UnProject(const f32& winX, const f32& winY, const f32& winZ, const Matrix &modelview, const Matrix &proj, const Vec4 view, f32 *objx, f32 *objy, f32 *objz)
    {
        Matrix finalMatrix;
        Vec4 in;
        Vec4 out;

        finalMatrix = (proj * modelview).Inverse();
        
        in.x=winX;
        in.y=winY;
        in.z=winZ;
        in.w=1.0;

        /* Map x and y from window coordinates */
        in.x = (in.x - view.x) / view.z;
        in.y = (in.y - view.y) / view.w;

        /* Map to range -1 to 1 */
        in.x = in.x * 2 - 1;
        in.y = in.y * 2 - 1;
        in.z = in.z * 2 - 1;

        out = finalMatrix * in;
        if (out.w == 0.0) return false;
        out.x /= out.w;
        out.y /= out.w;
        out.z /= out.w;
        *objx = out.x;
        *objy = out.y;
        *objz = out.z;

        return true;

    }
}