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
#include "../../../Other/Export.h"

namespace p3d {
    
    class PYROS3D_API FrustumCulling : public Culling {
        
        public:
            
            FrustumCulling();
            virtual ~FrustumCulling();
            
            // Frustum Planes
            FrustumPlane pl[6];
            
            void Update(const Matrix &ViewProjectionMatrix);
            bool PointInFrustum(const Vec3 &p);
            bool SphereInFrustum(const Vec3 &p, const f32 &radius);
            bool ABoxInFrustum(AABox box);
            bool OBoxInFrustum(OBBox box);
            
    };

}

#endif	/* FRUSTUMCULLING_H */