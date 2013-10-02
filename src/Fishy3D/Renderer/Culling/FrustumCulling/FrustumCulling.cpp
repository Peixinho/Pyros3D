//============================================================================
// Name        : FrustumCulling.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Frustum Culling Class
//============================================================================

#include "FrustumCulling.h"
#include <math.h>
#include <iostream>

namespace  Fishy3D {

    FrustumCulling::FrustumCulling() {}

    FrustumCulling::~FrustumCulling() {}

    void FrustumCulling::Update(const Matrix& View, const Matrix& Projection)
    {
        
        Matrix Clip = (Projection * View);
        vec4 vec;
        float Magnitude = 0.0f;
        
        //Plane 0
        vec = vec4(Clip.m[3] - Clip.m[0],
        Clip.m[7] - Clip.m[4], Clip.m[11] - Clip.m[8],
        Clip.m[15] - Clip.m[12]);
        Magnitude = vec3(vec.x, vec.y, vec.z).magnitude();

        pl[0].a = vec.x / Magnitude;
        pl[0].b = vec.y / Magnitude;
        pl[0].c = vec.z / Magnitude;
        pl[0].d = vec.w / Magnitude;
//        std::cout <<"Plane0: "<< vec.toString() << std::endl;

        //Plane 1
        vec = vec4(Clip.m[3] + Clip.m[0],
        Clip.m[7] + Clip.m[4], Clip.m[11] + Clip.m[8],
        Clip.m[15] + Clip.m[12]);
        Magnitude = vec3(vec.x, vec.y, vec.z).magnitude();

        pl[1].a = vec.x / Magnitude;
        pl[1].b = vec.y / Magnitude;
        pl[1].c = vec.z / Magnitude;
        pl[1].d = vec.w / Magnitude;
//        std::cout <<"Plane1: "<< vec.toString() << std::endl;

        //Plane 2
        vec = vec4(Clip.m[3] + Clip.m[1],
        Clip.m[7] + Clip.m[5],
        Clip.m[11] + Clip.m[9],
        Clip.m[15] + Clip.m[13]);
        Magnitude = vec3(vec.x, vec.y, vec.z).magnitude();

        pl[2].a = vec.x / Magnitude;
        pl[2].b = vec.y / Magnitude;
        pl[2].c = vec.z / Magnitude;
        pl[2].d = vec.w / Magnitude;
//        std::cout <<"Plane2: "<< vec.toString() << std::endl;
        
        //Plane 3
        vec = vec4(Clip.m[3] - Clip.m[1],
        Clip.m[7] - Clip.m[5], Clip.m[11] - Clip.m[9],
        Clip.m[15] - Clip.m[13]);
        Magnitude = vec3(vec.x, vec.y, vec.z).magnitude();

        pl[3].a = vec.x / Magnitude;
        pl[3].b = vec.y / Magnitude;
        pl[3].c = vec.z / Magnitude;
        pl[3].d = vec.w / Magnitude;
//        std::cout <<"Plane3: "<< vec.toString() << std::endl;
        
        //Plane 4
        vec = vec4(Clip.m[3] - Clip.m[2],
        Clip.m[7] - Clip.m[6], Clip.m[11] - Clip.m[10],
        Clip.m[15] - Clip.m[14]);
        Magnitude = vec3(vec.x, vec.y, vec.z).magnitude();
        
        pl[4].a = vec.x / Magnitude;
        pl[4].b = vec.y / Magnitude;
        pl[4].c = vec.z / Magnitude;
        pl[4].d = vec.w / Magnitude;
//        std::cout <<"Plane4: "<< vec.toString() << std::endl;
        
        //Plane 5
        vec = vec4(Clip.m[3] + Clip.m[2],
        Clip.m[7] + Clip.m[6], Clip.m[11] + Clip.m[10],
        Clip.m[15] + Clip.m[14]);
        Magnitude = vec3(vec.x, vec.y, vec.z).magnitude();

        pl[5].a = vec.x / Magnitude;
        pl[5].b = vec.y / Magnitude;
        pl[5].c = vec.z / Magnitude;
        pl[5].d = vec.w / Magnitude;        
//        std::cout <<"Plane5: "<< vec.toString() << std::endl;
    }

    bool FrustumCulling::PointInFrustum(const vec3& p)
    {
        bool result = true;

        for (int i=0; i<6; i++)
        {
            if (pl[i].ClassifyPoint(p)==PlanePointClassifications::Back) return false;
        }
        return result;
    }
    bool FrustumCulling::SphereInFrustum(const vec3& p, const float& radius)
    {
        for( int i = 0; i < 6; ++i )
        {
            if(pl[i].a * p.x + pl[i].b * p.y + pl[i].c * p.z + pl[i].d <= -radius )
                return false;
        };
        return true;
    }
    bool FrustumCulling::ABoxInFrustum(AABox box)
    {
        for(unsigned long i = 0; i < 6; i++)
        {
                vec4 Positive(
                        pl[i].a > 0 ? box.xmax : box.xmin,
                        pl[i].b > 0 ? box.ymax : box.ymin,
                        pl[i].c > 0 ? box.zmax : box.zmin,
                        1.0
                        );

                vec4 Negative(
                        pl[i].a < 0 ? box.xmax : box.xmin,
                        pl[i].b < 0 ? box.ymax : box.ymin,
                        pl[i].c < 0 ? box.zmax : box.zmin,
                        1.0
                        );

                float t = Positive.dotProduct(*(vec4*)&pl[i]);

                // invisible
                if(t < 0)
                    return false;

                t = Negative.dotProduct(*(vec4*)&pl[i]);

                // Intersecting
                if(t < 0)
                    return true;
        };

        return true;
    }
    bool FrustumCulling::OBoxInFrustum(OBBox box)
    {
        return 0;
    }
}