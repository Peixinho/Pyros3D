//============================================================================
// Name        : SpotlLightComponent.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SpotLightComponent
//============================================================================

#include "SpotLightComponent.h"

namespace Fishy3D {

    SpotLightComponent::SpotLightComponent() {
    }

    void SpotLightComponent::Destroy() 
    {

    }
    
    SpotLightComponent::SpotLightComponent(const std::string& name, const vec3& direction, const float& cosOutterCone, const float& cosInnerCone, const vec4& color, const float& radius): ILightComponent(name, color) 
    {
        this->Direction = direction;
        this->cosInnerCone = cosInnerCone;
        this->cosOutterCone = cosOutterCone;
        this->radius = radius;
    }

    const vec3 SpotLightComponent::GetDirection() const
    {
        return Direction;
    }
    const float SpotLightComponent::GetCosInnerCone() const
    {
        return cosInnerCone;
    }
    const float SpotLightComponent::GetCosOutterCone() const
    {
        return cosOutterCone;
    }
    const float SpotLightComponent::GetRadius() const
    {
        return radius;
    }
    
    SpotLightComponent::~SpotLightComponent() {
    }

}