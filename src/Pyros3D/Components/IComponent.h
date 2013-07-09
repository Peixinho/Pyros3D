//============================================================================
// Name        : IComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component Interface
//============================================================================

#ifndef ICOMPONENT_H
#define	ICOMPONENT_H

#include <vector>
#include "../GameObjects/GameObject.h"
#include "../SceneGraph/SceneGraph.h"

namespace p3d {
    
    // Circular Dependency
    class GameObject;
    class SceneGraph;
    
    class IComponent {
        
        friend class GameObject;
        
        public:
            
            IComponent() { Owner = NULL; Registered = false; }
            virtual ~IComponent() {}
            
            virtual void Register(SceneGraph* Scene) = 0;
            virtual void Init() = 0;
            virtual void Update() = 0;
            virtual void Destroy() = 0;
            virtual void Unregister(SceneGraph* Scene) = 0;
            
            GameObject* GetOwner() { return Owner; }
            
        protected:
        
            GameObject* Owner;
            
            bool Registered;
            
    };
    
};

#endif /* ICOMPONENT_H */