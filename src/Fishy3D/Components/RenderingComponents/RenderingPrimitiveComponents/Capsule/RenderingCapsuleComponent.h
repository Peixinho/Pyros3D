//============================================================================
// Name        : Rendering Capsule Component.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Capsule Component
//============================================================================

#ifndef RENDERINGCAPSULECOMPONENT_H
#define	RENDERINGCAPSULECOMPONENT_H

#include "../RenderingPrimitiveComponent.h"

namespace Fishy3D {

    class RenderingCapsuleComponent : public RenderingPrimitiveComponent {
        public:
            
            float radius, height;
            int rings,segmentsW, segmentsH;
            
            RenderingCapsuleComponent();
            RenderingCapsuleComponent(const std::string &Name, const float &mRadius, const float &mHeight, const unsigned &mNumRings, const unsigned &mNumSegments, const unsigned &mNumSegHeight, SuperSmartPointer<IMaterial> material, bool SNormals = false);
            virtual ~RenderingCapsuleComponent();
            
            virtual void Start();
            virtual void Update();
            virtual void Shutdown();
        private:

    };

}

#endif	/* RENDERINGCAPSULECOMPONENT_H */
