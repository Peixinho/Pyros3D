//============================================================================
// Name        : LightComponent.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : ILight Component
//============================================================================

#include "ILightComponent.h"

namespace Fishy3D {

    ILightComponent::ILightComponent() {}
	ILightComponent::~ILightComponent() {}
    ILightComponent::ILightComponent(const std::string& name, const vec4& color) : IComponent(name) 
    {
        // Light Color
        Color = color;
	}
    
    void ILightComponent::Register(void* ptr) 
    {
        // Register ILight
        GameObject* o = this->_Owner;
        SceneGraph* scene = static_cast<SceneGraph*> (ptr);
        scene->GetRenderingList()->AddLightComponent(o->GetComponentID(this),this);
    }
    void ILightComponent::UnRegister(void* ptr) 
    {
        // UnRegister ILight
        SceneGraph* scene = static_cast<SceneGraph*> (ptr);
        scene->GetRenderingList()->RemoveLightComponent(GetID());
    }    
    
    vec4 ILightComponent::GetColor()
    {
        return Color;
	}
}
