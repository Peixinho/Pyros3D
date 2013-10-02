//============================================================================
// Name        : Rendering Primitive Component.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Primitive Component
//===========================================================================

#ifndef RENDERINGPRIMITIVECOMPONENT_H
#define	RENDERINGPRIMITIVECOMPONENT_H

#include "../IRenderingComponent.h"
#include "../../../Core/Buffers/GeometryBuffer.h"
#include "../../../Core/Math/Math.h"
#include "../../../Utils/Pointers/SuperSmartPointer.h"

namespace Fishy3D {    
    
    class RenderingPrimitiveComponent : public IRenderingComponent {
        
        public:
                        
            RenderingPrimitiveComponent();
            RenderingPrimitiveComponent(const std::string &Name, SuperSmartPointer<IMaterial> Material, bool SNormals = true);
            virtual ~RenderingPrimitiveComponent();

            virtual void Start() = 0;
            virtual void Update() = 0;
            virtual void Shutdown() = 0;                                 
            
            virtual void Build();
            virtual void SmoothNormals();            
            
        protected:                                            

            // vectors
            std::vector<vec3> tVertex, tNormal;
            std::vector<vec2> tTexcoord;
            
            virtual void CalculateBounding();

    };

}

#endif	/* RENDERINGPRIMITIVECOMPONENT_H */

