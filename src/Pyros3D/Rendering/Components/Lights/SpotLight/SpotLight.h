//============================================================================
// Name        : SpotLight.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Spot Light
//============================================================================

#ifndef SPOTLIGHT_H
#define	SPOTLIGHT_H

#include "../ILightComponent.h"

namespace p3d {

        
    class SpotLight : public ILightComponent {
        
        public:
            
            SpotLight() { Color = Vec4(1,1,1,1); Radius = 1.f; }
            SpotLight(const Vec4 &color, const f32 &radius, const Vec3 &direction, const f32 &OutterCone, const f32 &InnerCone) { Color = color; Radius = radius; CosOutterCone = OutterCone; CosInnerCone=InnerCone; Direction = direction; }
            virtual ~SpotLight() {}

            virtual void Start() {};
            virtual void Update() {};
            virtual void Destroy() {};
                       
            const Vec3 &GetLightDirection() const { return Direction; }
            const f32 &GetLightCosInnerCone() const { return CosInnerCone; }
            const f32 &GetLightCosOutterCone() const { return CosOutterCone; }
            const f32 &GetLightRadius() const { return Radius; }
            
            void EnableCastShadows(const uint32 &Width, const uint32 &Height)
            {
                
            }
            
        protected :
            
            // Light Direction
            Vec3 Direction;
            // Cone
            f32 CosOutterCone, CosInnerCone; 
            // Attenuation
            f32 Radius;

    };

}

#endif	/* SPOTLIGHT_H */