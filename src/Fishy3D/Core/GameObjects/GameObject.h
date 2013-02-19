//============================================================================
// Name        : GameObject.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GameObject
//============================================================================

#ifndef GAMEOBJECT_H
#define	GAMEOBJECT_H

#include "../Math/Math.h"
#include "../Transformations/Transformation.h"
#include "../../Utils/Pointers/SuperSmartPointer.h"
#include "../../Components/IComponent.h"
#include "../../Utils/StringIDs/StringID.hpp"
#include <map>

namespace Fishy3D {
    
    // Coordinate System
    enum COORD_SYSTEM {
        LH = 0,
        RH
    };

    // Link to class Component 
    // because of circular dependency
    class IComponent;
    class GameObject;
    
    
    struct ComponentList {
        std::string componentName;
        SuperSmartPointer<IComponent> component;
        bool active;
        ComponentList() : active(true) {}
        ComponentList(std::string ComponentName, SuperSmartPointer<IComponent> comp) : componentName(ComponentName), component(comp), active(true) {}
    };
        
    // GameObject Class
    class GameObject {
        public:                      

        std::map<StringID, ComponentList > ComponentIDs; 

        // position
        Transformation transformation;

        GameObject(const std::string &Name);
        virtual ~GameObject();

        unsigned long GetID() const;
        std::string GetName() const;
        
        // Register Components
        void RegisterComponents(void *ptr, bool SceneMandatory = false);
        void UnRegisterComponents(void *ptr, bool SceneMandatory = false);
        
        // virtual methods
        void UpdateComponents();
        virtual void Update() {};
        virtual Matrix GetLocalMatrix();
        virtual Matrix GetWorldMatrix();
        virtual vec3 GetLocalPosition();
        virtual vec3 GetWorldPosition();
        virtual vec3 GetDirectionVector();
        
        void UpdateMatrix(const Matrix &Self);
        void UpdateMatrix(const Matrix &Parent, const Matrix &Self);
        
        // components Methods
        void AddComponent(SuperSmartPointer<IComponent> component);
        void RemoveComponent(const std::string &ComponentName);
        void RemoveComponent(const unsigned long ID);
        void RemoveComponent(SuperSmartPointer<IComponent> component);
        void RemoveAllComponents();
        const std::map <StringID,ComponentList> &GetComponents() const;
        bool ComponentsAdded();
        bool ComponentsRemoved();
        
        SuperSmartPointer<IComponent> GetComponentByID(const std::string &ComponentName);
        std::string GetComponentID(IComponent* comp);           

        void LookAt(const vec3 &position, COORD_SYSTEM o = RH);
        void LookAt(SuperSmartPointer<GameObject> gameObj, COORD_SYSTEM o = RH);            
        void LookAt(GameObject *gameObj, COORD_SYSTEM o = RH);
        void StopLookAt();
        bool IsLookingAt();
        
        // tags
        std::map<StringID, std::string> Tags;
        void AddTag(const std::string &tag);
        void AddTag(const StringID& tagID, const std::string &tag);
        void RemoveTag(const std::string &tag);
        void RemoveTag(const StringID &tag);
        bool CheckTag(const StringID &tag);
        bool CheckTag(const std::string &tag);
        
    private:
        
        // Components Updated
        bool componentsAdded, componentsRemoved;
        
        // World Matrix
        Matrix WorldMatrix;
        
        // GameObject Name
        std::string GameObjectName;
        
        // GameObject ID
        unsigned long ID;
        
        // LookAt
        bool IsLookingAtGO, IsLookingAtVec;
        GameObject* goTarget;
        vec3 vecTarget;
        COORD_SYSTEM CoordinateSystem;        
        
    };

}

#endif	/* GAMEOBJECT_H */
