//============================================================================
// Name        : ILightComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Lights
//============================================================================

#include "ILightComponent.h"

namespace p3d {
    
    // Initialize Rendering Components vector
    std::vector<IComponent*> ILightComponent::Components;
    
    ILightComponent::ILightComponent() {}
    
    void ILightComponent::Register(SceneGraph* Scene)
    {
        if (!Registered)
        {
            // Add Self to Components vector
            Components.push_back(this);

            // Set Flag
            Registered = true;
        }
    }
    void ILightComponent::Unregister(SceneGraph* Scene)
    {
        for (std::vector<IComponent*>::iterator i=Components.begin();i!=Components.end();i++)
        {
            if ((*i)==this)
            {
                Components.erase(i);
                break;
            }
        }
        
        // Unset Flag
        Registered = false;
    }
    
    std::vector<IComponent*> ILightComponent::GetComponents()
    {
        return Components;
    }
    
    const Vec4 &ILightComponent::GetLightColor() const
    {
        return Color;
    }
    
};