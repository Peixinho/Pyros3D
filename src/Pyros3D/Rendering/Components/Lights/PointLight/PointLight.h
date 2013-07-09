//============================================================================
// Name        : PointLight.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Point Light
//============================================================================

#ifndef POINTLIGHT_H
#define	POINTLIGHT_H

#include "../ILightComponent.h"

namespace p3d {

    class PointLight : public ILightComponent {
        
        public:
            
            PointLight() { Color = Vec4(1,1,1,1); Radius = 1.f; }
            PointLight(const Vec4 &color, const f32 &radius) { Color = color; Radius = radius; }
            virtual ~PointLight() {}

            virtual void Start() {};
            virtual void Update() {};
            virtual void Destroy() {};
            
            const f32 &GetLightRadius() const { return Radius; }
            
        protected:
            
            // Attenuation
            f32 Radius;

    };

}

#endif	/* POINTLIGHT_H */