//============================================================================
// Name        : IComponent.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component Interface
//============================================================================

#include "IComponent.h"

namespace Fishy3D {

    IComponent::IComponent() {}
    
    IComponent::IComponent(const std::string& Name)
    {
        ComponentName = Name;
        ComponentID = StringID(MakeStringID(Name));
        
        // doesn't have submeshes by default
        haveSubComponents = false;
    }
    
    IComponent::~IComponent() {}
    
    GameObject* IComponent::GetOwner() const
    {
        return _Owner;
    }
    
    void IComponent::SetOwner(GameObject* gameObject) 
    {
        if (HaveSubComponents() == true)
        {
            for (std::vector<SuperSmartPointer <IComponent> >::iterator i = SubComponents.begin();i!=SubComponents.end();i++)
            {
                (*i)->SetOwner(gameObject);
            }
        }

		_Owner = gameObject;
		haveOwner = true;

    }
	void IComponent::RemoveOwner()
	{
		_Owner = NULL;
		haveOwner = false;
	}
    std::string IComponent::GetName() const
    {
        return ComponentName;
    }
    unsigned long IComponent::GetID() const
    {
        return ComponentID;
    }
    std::vector <SuperSmartPointer <IComponent> > &IComponent::GetSubComponents()
    {
        return SubComponents;
    }
    bool IComponent::HaveSubComponents()
    {
        return haveSubComponents;
    }
}