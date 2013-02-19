//============================================================================
// Name        : PointLightComponent.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : PointLightComponent
//============================================================================
#ifndef POINTLIGHTCOMPONENT_H
#define	POINTLIGHTCOMPONENT_H

#include "../ILightComponent.h"

namespace Fishy3D {

    class PointLightComponent : public ILightComponent {
    public:

        PointLightComponent();
        PointLightComponent(const std::string &name, const vec4 &color, const float &radius);
        void Destroy();
        virtual ~PointLightComponent();

        virtual void Start() {};
        virtual void Update() {};
        virtual void Shutdown() {};
        
        const float GetRadius() const;
            
    private:

        // Attenuation
        float radius;
    };

}

#endif	/* POINTLIGHTCOMPONENT_H */