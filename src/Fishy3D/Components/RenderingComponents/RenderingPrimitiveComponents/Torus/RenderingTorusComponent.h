//============================================================================
// Name        : Rendering Torus Component.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Torus Component
//============================================================================

#ifndef RENDERINGTORUSCOMPONENT_H
#define	RENDERINGTORUSCOMPONENT_H

#include "../RenderingPrimitiveComponent.h"

namespace Fishy3D {

        class RenderingTorusComponent : public RenderingPrimitiveComponent {
                public:
                    float radius, tube;
                    int segmentsW, segmentsH;
                    
                    RenderingTorusComponent();
                    RenderingTorusComponent(const std::string &Name, const float &radius, const float &tube, SuperSmartPointer<IMaterial> material, const int &segmentsW = 8, const int segmentsH = 6, bool SNormals = false);
                    virtual ~RenderingTorusComponent();                               
            
                    virtual void Start();
                    virtual void Update();
                    virtual void Shutdown();
        };

}

#endif	/* RENDERINGTORUSCOMPONENT_H */

