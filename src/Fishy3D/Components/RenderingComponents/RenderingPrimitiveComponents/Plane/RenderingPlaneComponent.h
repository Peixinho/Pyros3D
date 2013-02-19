//============================================================================
// Name        : Rendering Plane Component.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Plane Component
//============================================================================

#ifndef RENDERINGPLANECOMPONENT_H
#define	RENDERINGPLANECOMPONENT_H

#include "../RenderingPrimitiveComponent.h"

namespace Fishy3D {

    class RenderingPlaneComponent : public RenderingPrimitiveComponent {
        public:
            
            float width, height;
            
            RenderingPlaneComponent(); 
            RenderingPlaneComponent(const std::string &Name, const float &width, const float &height, SuperSmartPointer<IMaterial> material, bool SNormals = false);
            virtual ~RenderingPlaneComponent();
            
            virtual void Start();
            virtual void Update();
            virtual void Shutdown();
            
        private:

    };

}

#endif	/* RENDERINGPLANECOMPONENT_H */