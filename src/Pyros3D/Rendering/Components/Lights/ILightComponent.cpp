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
    std::map<SceneGraph*, std::vector<IComponent*> > ILightComponent::LightsOnScene;
    
    ILightComponent::ILightComponent() 
    {
        isCastingShadows = false;
        shadowsFBO = NULL;
    }
    
    ILightComponent::~ILightComponent()
    {
        if (isCastingShadows)
        {
            delete shadowsFBO;
        }
    }
    
    void ILightComponent::Register(SceneGraph* Scene)
    {
        if (!Registered)
        {
            // Add Self to Components vector
            Components.push_back(this);

            // Set Flag
            Registered = true;
            
            // Add To Scene
            LightsOnScene[Scene].push_back(this);
        }
    }
    void ILightComponent::Unregister(SceneGraph* Scene)
    {
        for (std::vector<IComponent*>::iterator i=Components.begin();i!=Components.end();i++)
        {
            if ((*i)==this)
            {
                Components.erase(i);
                LightsOnScene[Scene].erase(i);
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
    
    std::vector<IComponent*> ILightComponent::GetComponentsOnScene(SceneGraph* Scene)
    {
        return LightsOnScene[Scene];
    }
    
    const Vec4 &ILightComponent::GetLightColor() const
    {
        return Color;
    }
    
    void ILightComponent::DisableCastShadows()
    {
        if (isCastingShadows)
        {
            isCastingShadows = false;
            delete shadowsFBO;
        }
    }
    
    FrameBuffer* ILightComponent::GetShadowFBO()
    {
        if (isCastingShadows)
        {
            return shadowsFBO;
        }
        else echo ("ERROR: Frame Buffer Is Not Created");
        return NULL;
    }
};