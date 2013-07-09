//============================================================================
// Name        : DirectionalLight.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Directional Light
//============================================================================

#ifndef DIRECTIONALLIGHT_H
#define	DIRECTIONALLIGHT_H

#include "../ILightComponent.h"

namespace p3d {

    class DirectionalLight : public ILightComponent {
        
        public:
            
            DirectionalLight() { Color = Vec4(1,1,1,1); }
            DirectionalLight(const Vec4 &color) { Color = color; }
            virtual ~DirectionalLight() {}

            virtual void Start() {};
            virtual void Update() {};
            virtual void Destroy() {};
            
        private:

    };

}

#endif	/* DIRECTIONALLIGHT_H */