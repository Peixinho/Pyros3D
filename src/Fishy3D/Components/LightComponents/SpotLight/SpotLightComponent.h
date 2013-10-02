//============================================================================
// Name        : SpotlLightComponent.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SpotLightComponent
//============================================================================

#ifndef SPOTLIGHTCOMPONENT_H
#define	SPOTLIGHTCOMPONENT_H

#include "../ILightComponent.h"

namespace Fishy3D {

        class SpotLightComponent : public ILightComponent {
        public:
            
            SpotLightComponent();
            SpotLightComponent(const std::string &name, const vec3& direction, const float &cosOutterCone, const float &cosInnerCone, const vec4& color, const float &radius);
            void Destroy();
            virtual ~SpotLightComponent();

            virtual void Start() {};
            virtual void Update() {};
            virtual void Shutdown() {};
            
            const vec3 GetDirection() const;
            const float GetCosInnerCone() const;
            const float GetCosOutterCone() const;
            const float GetRadius() const;           
            
        protected :
            
            // Light Direction
            vec3 Direction;
            // Cone
            float cosOutterCone, cosInnerCone; 
            // Attenuation
            float radius;
            
        };

}

#endif	/* SPOTLIGHTCOMPONENT_H */