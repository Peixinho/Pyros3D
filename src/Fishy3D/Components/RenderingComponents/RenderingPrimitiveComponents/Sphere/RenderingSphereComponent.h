//============================================================================
// Name        : Rendering Sphere Component.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Sphere Component
//============================================================================

#ifndef RENDERINGSPHERECOMPONENT_H
#define	RENDERINGSPHERECOMPONENT_H

#include "../RenderingPrimitiveComponent.h"

namespace Fishy3D {               
    
    class RenderingSphereComponent : public RenderingPrimitiveComponent {
        public:
            
            int segmentsW, segmentsH;
            float radius;                                   
            
            RenderingSphereComponent();
            RenderingSphereComponent(const std::string &Name, const float &radius, SuperSmartPointer<IMaterial> material, const int &segmentsW = 8, const int segmentsH = 6, bool HalfSphere = false, bool SNormals = false);
            
            virtual void Start();
            virtual void Update();
            virtual void Shutdown();
                
            virtual ~RenderingSphereComponent();

    };

}

#endif	/* RENDERINGSPHERECOMPONENT_H */