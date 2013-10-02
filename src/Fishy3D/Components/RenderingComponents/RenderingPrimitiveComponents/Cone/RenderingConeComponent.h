//============================================================================
// Name        : Rendering Cone Component.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Cone Component
//============================================================================

#ifndef RENDERINGCONECOMPONENT_H
#define	RENDERINGCONECOMPONENT_H

#include "../RenderingPrimitiveComponent.h"

namespace Fishy3D {

    class RenderingConeComponent : public RenderingPrimitiveComponent {
        public:
            
            float radius, height;
            int segmentsW, segmentsH;
            bool openEnded;
            
            RenderingConeComponent();
            RenderingConeComponent(const std::string &Name, const float &radius, const float &height, const int &segmentsW, const int segmentsH, const bool &openEnded, SuperSmartPointer<IMaterial> material, bool SNormals = false);
            virtual ~RenderingConeComponent();
            
            virtual void Start();
            virtual void Update();
            virtual void Shutdown();
        private:

    };

}

#endif	/* RENDERINGCONECOMPONENT_H */