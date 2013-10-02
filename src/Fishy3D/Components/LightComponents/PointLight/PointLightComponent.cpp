//============================================================================
// Name        : PointLightComponent.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : PointLightComponent
//============================================================================

#include "PointLightComponent.h"

namespace Fishy3D {

    PointLightComponent::PointLightComponent() {}

    void PointLightComponent::Destroy() 
    {
		
    }
    
    PointLightComponent::PointLightComponent(const std::string& name, const vec4& color, const float& radius) : ILightComponent(name, color) 
    {
    
        this->radius = radius;
    
    }

    const float PointLightComponent::GetRadius() const
    {
        return radius;
    }
    
    PointLightComponent::~PointLightComponent() {}

}