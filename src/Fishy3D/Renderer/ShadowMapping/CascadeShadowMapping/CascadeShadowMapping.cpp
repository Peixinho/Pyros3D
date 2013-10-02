//============================================================================
// Name        : CascadeShadowMapping.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Class to Create the Shadow Mapping Cascades
//============================================================================

#include "GL/glew.h"
#include "CascadeShadowMapping.h"

namespace Fishy3D {
    
    CascadeShadowMapping::CascadeShadowMapping() : splits(0) {}

    void CascadeShadowMapping::Init(const float &Fov, const float& Ratio)
    {
        for (unsigned i=0; i<MAX_SPLITS; i++)
        {
            // note that fov is in radians here and in OpenGL it is in degrees.
            f[i].Fov = RADTODEG(Fov) + CASCADE_FACTOR;
            f[i].Ratio = Ratio;
        } 
    }

    void CascadeShadowMapping::UpdateSplitDist(const unsigned &splits, const float &nd, const float &fd)
    {
        this->splits = splits;
        float lambda = SPLIT_WEIGHT;
        float ratio = fd/nd;
        f[0].Near = nd;

        for(int i=1; i<splits; i++)
        {
            float si = i / (float)splits;

            f[i].Near = lambda*(nd*powf(ratio, si)) + (1-lambda)*(nd + (fd - nd)*si);
            f[i-1].Far = f[i].Near * 1.005f;
        }
        f[splits-1].Far = fd;
    }

    Matrix CascadeShadowMapping::GetViewMatrix()
    {
            return ViewMatrix;
    }
};