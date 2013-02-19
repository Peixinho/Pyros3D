//============================================================================
// Name        : ILightComponent.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Light Interface
//============================================================================

#ifndef ILIGHTCOMPONENT_H
#define	ILIGHTCOMPONENT_H

#include "../IComponent.h"
#include "../../Core/Math/Math.h"
#include "../../SceneGraph/SceneGraph.h"
#include "../../Core/Buffers/FrameBuffer.h"
#include "../../Core/Projection/Projection.h"

namespace Fishy3D {

    class ILightComponent : public IComponent {
        public:       

            ILightComponent();    
            ILightComponent(const std::string &name, const vec4 &color);
            virtual ~ILightComponent();

            virtual void Start() = 0;
            virtual void Update() = 0;
            virtual void Shutdown() = 0;        

            virtual void Register(void* ptr);
            virtual void UnRegister(void* ptr);

            // Get Light Color
            vec4 GetColor();
       
            // RTT Methods
            void Bind();
            void UnBind();
            Texture GetTexture();
            
        protected:
            
            // Light Color
            vec4 Color;
	    
    };

}

#endif	/* ILIGHTCOMPONENT_H */