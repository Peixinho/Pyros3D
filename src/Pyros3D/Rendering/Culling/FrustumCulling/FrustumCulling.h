//============================================================================
// Name        : FrustumCulling.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Frustum Culling Class
//============================================================================

#ifndef FRUSTUMCULLING_H
#define	FRUSTUMCULLING_H

#include "../Culling.h"
#include "../../../Core/Math/Math.h"
#include "../../../Core/Projection/Projection.h"

namespace p3d {
    
    class FrustumCulling : public Culling {
        public:
            FrustumCulling();
            virtual ~FrustumCulling();
            
            // Frustum Planes
            FrustumPlane pl[6];
            
            void Update(const Matrix &View, const p3d::Projection &Projection);
            bool PointInFrustum(const Vec3 &p);
            bool SphereInFrustum(const Vec3 &p, const f32 &radius);
            bool ABoxInFrustum(AABox box);
            bool OBoxInFrustum(OBBox box);
            // Not Necessary, USE SPHERE INSTEAD
            // int BoxInFrustum(const vec3 &b);
        private:
            enum {
                TOP = 0,
                BOTTOM = 1,
                LEFT = 2,
                RIGHT = 3,
                NEARD = 4,
                FARD = 5
            };
                        
    };

}

#endif	/* FRUSTUMCULLING_H */