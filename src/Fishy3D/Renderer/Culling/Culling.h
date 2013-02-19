//============================================================================
// Name        : Culling.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Culling Class
//============================================================================
#ifndef CULLING_H
#define CULLING_H

#include "../../Core/Math/Math.h"

namespace Fishy3D {

    namespace PlanePointClassifications
    {
        enum {
            Front = 0,
            Back,
            On_Plane
        };
    };
    
    struct FrustumPlane {
        float a, b, c, d;
        vec3 normal;
        void Set3Points(const vec3 &A, const vec3 &B, const vec3 &C) 
        {
            normal = (B - A).cross(C - A).normalize();        
            
            a = normal.x;
            b = normal.y;
            c = normal.z;            
            d = -normal.dotProduct(A);
                        
        }
        void SetNormalAndPoint(const vec3 &Normal, const vec3 &Point)
        {
            normal = Normal;
            
            a = normal.x;
            b = normal.y;
            c = normal.z;
            d = -normal.dotProduct(Point);
        }
        float Distance(const vec3 &point) 
        {
            normal = vec3(a,b,c);            
           return normal.dotProduct(point)+d;
        }
        unsigned ClassifyPoint(const vec3 &v) const
        {
                float Dot = v.dotProduct(vec3(a, b, c)) + d;

                if(Dot > EPSILON)
                {
                        return PlanePointClassifications::Front;
                }
                else if(Dot < -EPSILON)
                {
                        return PlanePointClassifications::Back;
                };

                return PlanePointClassifications::On_Plane;
        };
        
    };
    
    struct AABox {

        float xmax,ymax,zmax,xmin,ymin,zmin;
        AABox(vec3* v)
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
        
        AABox(const vec3 &min, const vec3 &max)
        {
            xmin = min.x;
            xmax = max.x;
            ymin = min.y;
            ymax = max.y;
            zmin = min.z;
            zmax = max.z;
        }
        
        vec3 GetPositiveVertex(const vec3 &normal)
        {
            vec3 p = vec3(xmin,ymin,zmin);
            if (normal.x>=0)
                p.x = xmax;
            if (normal.y>=0)
                p.y = ymax;
            if (normal.z>=0)
                p.z = zmax;
            
            return p;
        }
        vec3 GetNegativeVertex(const vec3 &normal)
        {
            vec3 n = vec3(xmax,ymax,zmax);
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
        vec3 center;
        // Direction Vectors
        vec3  vx, vy, vz;        
    };
    
    class Culling {
        public:
            Culling();
            virtual ~Culling();
        private:

    };

}; 
#endif	/* CULLING_H */