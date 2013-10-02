//============================================================================
// Name        : Rendering Cylinder Component.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Cylinder Component
//============================================================================

#ifndef RENDERINGCYLINDERCOMPONENT_H
#define	RENDERINGCYLINDERCOMPONENT_H

#include "../RenderingPrimitiveComponent.h"

namespace Fishy3D {

    class RenderingCylinderComponent : public RenderingPrimitiveComponent {
        public:
                                
            int segmentsW, segmentsH;
            float radius, height;
            bool openEnded;
            
            RenderingCylinderComponent();
            RenderingCylinderComponent(const std::string &Name, const float &radius, const float &height, const int &segmentsW, const int &segmentsH, const bool &openEnded, SuperSmartPointer<IMaterial> material, bool SNormals = false);
            virtual ~RenderingCylinderComponent();

            virtual void Start();
            virtual void Update();
            virtual void Shutdown();   
            
    };

}

#endif	/* RENDERINGCYLINDERCOMPONENT_H */