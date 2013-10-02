//============================================================================
// Name        : GameObject.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GameObject
//============================================================================

#include "GameObject.h"

namespace Fishy3D {

    GameObject::GameObject(const std::string &Name) 
    {
        
        // Initial values
        componentsAdded = false;
        componentsRemoved = false;
        IsLookingAtGO = false;
        IsLookingAtVec = false;
        GameObjectName = Name;
        ID = StringID(MakeStringID(Name));
        
    }    
    GameObject::~GameObject() {}
    
    unsigned long GameObject::GetID() const
    {
        return ID;
    }
    std::string GameObject::GetName() const
    {
        return GameObjectName;
    }
    
    void GameObject::RegisterComponents(void *ptr, bool SceneMandatory)
    {
        if (SceneMandatory == true)
        {
             for (std::map<StringID,ComponentList>::iterator i=ComponentIDs.begin();i!=ComponentIDs.end();i++) {
                // unregister component
                (*i).second.component.Get()->Register(ptr);
            }
        } else if (componentsAdded==true)
        {
            for (std::map<StringID,ComponentList>::iterator i=ComponentIDs.begin();i!=ComponentIDs.end();i++) {
                // Registers all components
                if ((*i).second.active==true)
                (*i).second.component.Get()->Register(ptr);
            }
        }
        componentsAdded = false;        
    }
    
    void GameObject::UnRegisterComponents(void *ptr, bool SceneMandatory)
    {
        if (SceneMandatory == true)
        {
            for (std::map<StringID,ComponentList>::iterator i=ComponentIDs.begin();i!=ComponentIDs.end();i++) {
                // unregister component
                (*i).second.component.Get()->UnRegister(ptr);
            }
        }
        else if (componentsRemoved==true)
        {
            for (std::map<StringID,ComponentList>::iterator i=ComponentIDs.begin();i!=ComponentIDs.end();i++) 
            {
                if ((*i).second.active==false)
                {
                    // unregister component
                    (*i).second.component.Get()->UnRegister(ptr);
                    // remove from list        
                    (*i).second.component->SetOwner(NULL);
                    ComponentIDs.erase(i);
                }
            }            
        }
        componentsRemoved = false;
    }    
    
    void GameObject::UpdateComponents() 
    {

        for (std::map<StringID,ComponentList>::iterator i=ComponentIDs.begin();i!=ComponentIDs.end();i++) {
            // Updates all Components
            (*i).second.component.Get()->Update();
        }
        
    }
    
    void GameObject::UpdateMatrix(const Matrix &Self)
    {
        Matrix Parent;
        UpdateMatrix(Parent, Self);
    }
    void GameObject::UpdateMatrix(const Matrix& Parent, const Matrix &Self)
    {
        WorldMatrix = Parent * Self;
    }
    
    vec3 GameObject::GetWorldPosition()
    { 
        return GetWorldMatrix().GetTranslation();
    }
    vec3 GameObject::GetLocalPosition()
    {
        return transformation.GetPosition();
    }
    
    Matrix GameObject::GetLocalMatrix()
    {
        
        // Check if Transformation is not updated
        transformation.update();
        
        // Apply this ONLY if is Looking At
        if (IsLookingAtVec==true || IsLookingAtGO==true)
        {
            
            // Transformation    
            Matrix TransfMatrix;

            // Translation value
            vec3 Translation;                     

            TransfMatrix = transformation.getTransformationMatrix();
            
            vec3 target;
            if (IsLookingAtGO==true)
                target=goTarget->GetWorldPosition();                    
            else if (IsLookingAtVec==true) {
                target=vecTarget;
            }
            else target = vec3::ZERO;

            // only executes this if its pointing to
            // a different location than its position
            
            Translation = TransfMatrix.GetTranslation();
            switch (CoordinateSystem)
            {
                case LH:                            
                    TransfMatrix.LookAtLH(Translation,target,vec3::UP);
                    break;
                case RH:
                default:
                    TransfMatrix.LookAtRH(Translation,target,vec3::UP);
                    break;
            }

            TransfMatrix=TransfMatrix.Inverse();
            TransfMatrix.Translate(Translation);
            
            return TransfMatrix;
        }                
        
        else return transformation.getTransformationMatrix();
    }
    
    Matrix GameObject::GetWorldMatrix() 
    {
        
        // Apply this ONLY if is Looking At
        if (IsLookingAtVec==true || IsLookingAtGO==true)
        {
            
            // Transformation    
            Matrix TransfMatrix;

            // Translation value
            vec3 Translation;                     

            TransfMatrix = WorldMatrix;
            
            vec3 target;
            if (IsLookingAtGO==true)
                target=goTarget->GetWorldPosition();                    
            else if (IsLookingAtVec==true) {
                target=vecTarget;
            }
            else target = vec3::ZERO;

            // only executes this if its pointing to
            // a different location than its position
            
            Translation = TransfMatrix.GetTranslation();
            if (Translation!=target)
            {
                switch (CoordinateSystem)
                {
                    case LH:                            
                        TransfMatrix.LookAtLH(Translation,target,vec3::UP);
                        break;
                    case RH:
                    default:
                        TransfMatrix.LookAtRH(Translation,target,vec3::UP);
                        break;
                }

                TransfMatrix=TransfMatrix.Inverse();
                TransfMatrix.Translate(Translation);            
            }
            WorldMatrix = TransfMatrix;
        }
                
        return WorldMatrix;
    }
    
    // Get Driection Vector
    vec3 GameObject::GetDirectionVector()
    {
        Matrix m = GetWorldMatrix();
        return vec3(m.m[8], m.m[9], m.m[10]);
    }
    
    // Adds a component to GameObject
    void GameObject::AddComponent(SuperSmartPointer<IComponent> comp)
    {
        bool found = false;
                        
        if(ComponentIDs.find(comp->GetID()) != ComponentIDs.end()) found=true;
        
        if (!found) {
            // Register ComponentsList ID
            ComponentIDs[comp->GetID()].component = comp;
            ComponentIDs[comp->GetID()].componentName = comp->GetName();            
            
            // set component owner
            comp->SetOwner(this);
            componentsAdded = true;

        }
    }
    
    // Removes a component of GameObject
    void GameObject::RemoveComponent(const std::string &ComponentName)
    {
        
        StringID ID(MakeStringID(ComponentName));
        std::map<StringID,ComponentList>::iterator sID = ComponentIDs.find(ID);
        if(sID != ComponentIDs.end()) {
            componentsRemoved = true;
            // Flag to remove
            (*sID).second.active = false;
        }
    }
    // Removes a component of GameObject
    void GameObject::RemoveComponent(const unsigned long ID)
    {
        std::map<StringID,ComponentList>::iterator sID = ComponentIDs.find(ID);
        if(sID != ComponentIDs.end()) {
            componentsRemoved = true;
            // Flag to remove
            (*sID).second.active = false;
        }
    }    
    // Removes a component of GameObject
    void GameObject::RemoveComponent(SuperSmartPointer<IComponent> component)
    {   
        for (std::map<StringID,ComponentList>::iterator i=ComponentIDs.begin();i!=ComponentIDs.end();i++) {
            if (i->second.component.Get()==component.Get()) {
                componentsRemoved = true;
                // Flag to remove
                (*i).second.active = false;
                break;
            }                
        }
    }
    void GameObject::RemoveAllComponents()
    {
        for (std::map<StringID,ComponentList>::iterator i=ComponentIDs.begin();i!=ComponentIDs.end();i++) {
            // Flag to remove
            (*i).second.active = false;
        }
        componentsRemoved = true;
    }
    bool GameObject::ComponentsAdded()
    {
        return componentsAdded;
    }
    bool GameObject::ComponentsRemoved()
    {
        return componentsRemoved;
    }
    
    // Returns Components List
    const std::map <StringID,ComponentList> &GameObject::GetComponents() const
    {
        return ComponentIDs;
    }
    
    SuperSmartPointer<IComponent> GameObject::GetComponentByID(const std::string& ComponentName)
    {
        StringID ID(MakeStringID(ComponentName));
        return ComponentIDs.find(ID)->second.component;
    }
    
    std::string GameObject::GetComponentID(IComponent* comp)
    {            
        std::string ID;
        for (std::map <StringID, ComponentList>::iterator i=ComponentIDs.begin();i!=ComponentIDs.end();i++) 
        {
            if ((*i).second.component.Get()==comp) ID = (*i).second.componentName;
        }
        return ID;
    }   
    
    void GameObject::LookAt(const vec3& position, COORD_SYSTEM o)
    {
        IsLookingAtVec = true;
        IsLookingAtGO = false;
        vecTarget = position;
        CoordinateSystem = o;
    }
    void GameObject::LookAt(SuperSmartPointer<GameObject> gameObj, COORD_SYSTEM o)
    {
        IsLookingAtGO = true;
        IsLookingAtVec = false;
        goTarget = gameObj.Get();
        CoordinateSystem = o;
    }
    void GameObject::LookAt(GameObject* gameObj, COORD_SYSTEM o)
    {
        IsLookingAtGO = true;
        IsLookingAtVec = false;
        goTarget = gameObj;
        CoordinateSystem = o;
    }
    void GameObject::StopLookAt()
    {
        IsLookingAtGO = false;
        IsLookingAtVec = false;
    }
    bool GameObject::IsLookingAt()
    {
        return (IsLookingAtGO == true || IsLookingAtVec == true);
    }
    
    void GameObject::AddTag(const std::string& tag)
    {
        StringID Tag (MakeStringID(tag));
        AddTag(Tag, tag);
    }
    void GameObject::AddTag(const StringID& tagID, const std::string &tag)
    {
        Tags[tagID]=tag;
    }
    void GameObject::RemoveTag(const std::string& tag)
    {
        StringID Tag (MakeStringID(tag));
        RemoveTag(Tag);
    }
    void GameObject::RemoveTag(const StringID& tag)
    {
        Tags.erase(Tags.find(tag));
    }
    bool GameObject::CheckTag(const StringID& tag)
    {
        return (Tags.find(tag)!=Tags.end());
    }
    bool GameObject::CheckTag(const std::string& tag)
    {
        return CheckTag(MakeStringID(tag));
    }
    
}
