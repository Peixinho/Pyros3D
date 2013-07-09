//============================================================================
// Name        : ILightComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Lights
//============================================================================

#ifndef ILIGHTCOMPONENT_H
#define	ILIGHTCOMPONENT_H

#include "../../../Components/IComponent.h"
#include "../../../Core/Math/Math.h"
#include <vector>
#include <map>

namespace p3d {
    
    class ILightComponent : public IComponent {
        
        public:
            
            ILightComponent();
            
            virtual ~ILightComponent() {}
            
            virtual void Register(SceneGraph* Scene);
            virtual void Init() {}
            virtual void Update() {}
            virtual void Destroy() {}
            virtual void Unregister(SceneGraph* Scene);
            
            static std::vector<IComponent*> GetComponents();
            static std::vector<IComponent*> GetComponentsOnScene(SceneGraph* Scene);
            const Vec4 &GetLightColor() const;
            
        protected:
        
            // Light Color
            Vec4 Color;
            
            // INTERNAL - List of Lights
            static std::vector<IComponent*> Components;
            // Internal - Lights on Scene
            static std::map<SceneGraph*, std::vector<IComponent*> > LightsOnScene;
            
    };
    
};

#endif /* ILIGHTCOMPONENT_H */