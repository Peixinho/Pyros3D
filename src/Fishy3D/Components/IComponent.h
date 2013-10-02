//============================================================================
// Name        : IComponent.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component Interface
//============================================================================

#ifndef ICOMPONENT_H
#define	ICOMPONENT_H

#include "../Utils/Pointers/SuperSmartPointer.h"
#include "../Core/GameObjects/GameObject.h"
#include <vector>

namespace Fishy3D {    
    
    // Link to class GameObject 
    // because of circular dependency
    class GameObject;    
    // Component Class
    class IComponent {  

        public:                
            
            IComponent();
            IComponent(const std::string &Name);
            virtual ~IComponent();
            
            // Update function that will
            // be present in all Components
            virtual void Start() = 0;
            virtual void Update() = 0;    
            virtual void Shutdown() = 0;
            
            // Method used to register the component if needed
            virtual void Register(void* ptr) = 0;
            virtual void UnRegister(void* ptr) = 0;
            
            // TODO virtual void HandleMessage(MessageType message);
            // http://www.glassbottomgames.com/?p=174
            
            GameObject* GetOwner() const;
            void SetOwner(GameObject* gameObject);            
            void RemoveOwner();

            std::string GetName() const;
            unsigned long GetID() const;
            
            std::vector <SuperSmartPointer <IComponent> > &GetSubComponents();
            
            bool HaveSubComponents();
            
            // Dynamic Cast
            template<class c> c *AsDerived() { return dynamic_cast<c *>(this); }
            
        protected:
            
            bool haveOwner;
            bool haveSubComponents;
            
            // Component Name
            std::string ComponentName;
            // Component ID
            unsigned long ComponentID;
            
            // pointer to component owner
            GameObject* _Owner;
            
            // List of sub components (ex: submeshes)
            std::vector <SuperSmartPointer <IComponent> > SubComponents;
                        
    };

}

#endif	/* ICOMPONENT_H */

