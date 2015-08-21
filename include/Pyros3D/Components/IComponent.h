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
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Other/Export.h>

namespace p3d {
    
    // Circular Dependency
    class PYROS3D_API GameObject;
    class PYROS3D_API SceneGraph;
    
    class PYROS3D_API IComponent {
        
        friend class GameObject;
        
        public:
            
			IComponent() { Owner = NULL; Registered = false; active = true; }
            virtual ~IComponent() {}
            
            virtual void Register(SceneGraph* Scene) = 0;
            virtual void Init() = 0;
            virtual void Update() = 0;
            virtual void Destroy() = 0;
            virtual void Unregister(SceneGraph* Scene) = 0;
            
            GameObject* GetOwner() { return Owner; }

            bool IsActive() { return active; }
            void Disable() { active = false; }
            void Enable() { active = true; }
            
        protected:
        
            GameObject* Owner;
            
            bool Registered;
            
            bool active;

			// Bounds of the Component
			f32 BoundingSphereRadius;
			Vec3 BoundingSphereCenter;
			Vec3 maxBounds, minBounds;

            
    };
    
};

#endif /* ICOMPONENT_H */
