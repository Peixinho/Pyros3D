//============================================================================
// Name        : Culling.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Culling Class
//============================================================================
#ifndef CULLING_H
#define CULLING_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>

namespace p3d {

    namespace CullingGeometry
    {
        enum {
            Sphere = 0,
            Box,
            Point
        };
    };
    
    namespace CullingMode
    {
        enum
        {
            FrustumCulling = 0
        };
    };
    
    struct PYROS3D_API FrustumPlane {
        f32 constant;
        Vec3 normal;
        
        void Set3Points(const Vec3 &A, const Vec3 &B, const Vec3 &C) 
        {
            normal = (B - A).cross(C - A).normalize();    
            constant = -normal.dotProduct(A);
                        
        }
        void SetNormalAndPoint(const Vec3 &Normal, const Vec3 &Point)
        {
            normal = Normal;
            constant = -normal.dotProduct(Point);
        }
        void SetNormalAndConstant(const f32 a, const f32 b, const f32 c, const f32 w)
        {
            normal = Vec3(a,b,c);
            constant = w;
        }
        f32 Distance(const Vec3 &point) 
        {
           return normal.dotProduct(point)+constant;
        }
        void normalize() 
        {
            f32 inverseNormalLength = 1.0 / this->normal.magnitude();
            this->normal *= inverseNormalLength;
            this->constant *= inverseNormalLength;
	}
        
    };
    
    struct PYROS3D_API AABox {

        f32 xmax,ymax,zmax,xmin,ymin,zmin;
        AABox(Vec3* v)
        {
            xmax=xmin=v[0].x;ymax=ymin=v[0].y;zmax=zmin=v[0].z;

            for (int i=1;i<8;i++) 
            {
                if (v[i].x>xmax) xmax=v[i].x;
                if (v[i].x<xmin) xmin=v[i].x;
                if (v[i].y>ymax) ymax=v[i].y;
                if (v[i].y>ymin) ymin=v[i].y;
                if (v[i].z>zmax) zmax=v[i].z;
                if (v[i].z>zmin) zmin=v[i].z;
            }
            
        }
        
        AABox(const Vec3 &min, const Vec3 &max)
        {
            xmin = min.x;
            xmax = max.x;
            ymin = min.y;
            ymax = max.y;
            zmin = min.z;
            zmax = max.z;
        }
        
        Vec3 GetPositiveVertex(const Vec3 &normal)
        {
            Vec3 p = Vec3(xmin,ymin,zmin);
            if (normal.x>=0)
                p.x = xmax;
            if (normal.y>=0)
                p.y = ymax;
            if (normal.z>=0)
                p.z = zmax;
            
            return p;
        }
        Vec3 GetNegativeVertex(const Vec3 &normal)
        {
            Vec3 n = Vec3(xmax,ymax,zmax);
            if (normal.x>=0)
                n.x = xmin;
            if (normal.y>=0)
                n.y = ymin;
            if (normal.z>=0)
                n.z = zmin;
            
            return n;
        }
    };
    
    
    struct OBBox {
        // Oriented Box Center
        Vec3 center;
        // Direction Vectors
        Vec3  vx, vy, vz;        
    };
    
    class PYROS3D_API Culling {
        public:
            Culling();
            virtual ~Culling();
        private:

    };

}; 
#endif	/* CULLING_H */