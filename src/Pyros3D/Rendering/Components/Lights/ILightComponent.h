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
            const Vec4 &GetLightColor() const;
            
        protected:
        
            // Light Color
            Vec4 Color;
            
            // Components
            static std::vector<IComponent*> Components;
            
    };
    
};

#endif /* ILIGHTCOMPONENT_H */