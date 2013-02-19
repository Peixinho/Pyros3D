//============================================================================
// Name        : DirectionalLightComponent.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : DirectionalLightComponent
//============================================================================

#ifndef DIRECTIONALLIGHTCOMPONENT_H
#define	DIRECTIONALLIGHTCOMPONENT_H

#include "../ILightComponent.h"
#include "../../../Renderer/ShadowMapping/CascadeShadowMapping/CascadeShadowMapping.h"

namespace Fishy3D {

    class DirectionalLightComponent : public ILightComponent {
        
		public:
            
            DirectionalLightComponent();
            DirectionalLightComponent(const std::string &name, const vec4 &color);
            void Destroy();
            virtual ~DirectionalLightComponent();

            virtual void Start() {};
            virtual void Update() {};
            virtual void Shutdown() {};

            // Cascade Shadow Mapping
            void EnableCascadeShadows(const unsigned &splits, const Projection &projection, const float &nearDistance, const float &farDistance, const unsigned &mapResolutionWidth, const unsigned &mapResolutionHeight);
			void DisableCascadeShadows();
            void UpdateCascadeShadowsSplits(const unsigned &splits);
            void UpdateCascadeShadowsProjection(const Projection &projection);
            void UpdateCascadeShadowsDistance(const float &nearDistance, const float &farDistance);
			void UpdateCascadeShadowsResolution(const unsigned &mapResolutionWidth, const unsigned &mapResolutionHeight);
            void UpdateCascadeShadowMapping();
            CascadeShadowMapping* GetCascadeShadowMapping();
			bool IsUsingCascadeShadows();
            
            
        private:

            // Cascade Shadow Mapping
			bool useCascadeShadows;
            SuperSmartPointer<CascadeShadowMapping> Cascade;
            float nearDistance, farDistance;
            unsigned splits;
			unsigned shadowMapHeight, shadowMapWidth;
    };

}

#endif	/* DIRECTIONALLIGHTCOMPONENT_H */