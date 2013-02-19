//============================================================================
// Name        : Rendering Cube Component.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Cube Component
//============================================================================

#ifndef RENDERINGCUBECOMPONENT_H
#define	RENDERINGCUBECOMPONENT_H

#include "../RenderingPrimitiveComponent.h"

namespace Fishy3D {

    class RenderingCubeComponent : public RenderingPrimitiveComponent {
        public:

            RenderingCubeComponent();
            RenderingCubeComponent(const std::string &Name, const float &width, const float &height, const float &depth, SuperSmartPointer<IMaterial> material, bool SNormals = false);

            virtual ~RenderingCubeComponent();

            virtual void Start();
            virtual void Update();
            virtual void Shutdown();

    };

}

#endif	/* RENDERINGCUBECOMPONENT_H */

