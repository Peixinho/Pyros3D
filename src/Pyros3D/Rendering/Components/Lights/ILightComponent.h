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
#include "../../../Core/Projection/Projection.h"
#include "../../../Core/Buffers/FrameBuffer.h"
#include <vector>
#include <map>

namespace p3d {
    
    class ILightComponent : public IComponent {
        
        public:
            
            ILightComponent();
            
            virtual ~ILightComponent();
            
            virtual void Register(SceneGraph* Scene);
            virtual void Init() {}
            virtual void Update() {}
            virtual void Destroy() {}
            virtual void Unregister(SceneGraph* Scene);
            
            static std::vector<IComponent*> GetComponents();
            static std::vector<IComponent*> GetComponentsOnScene(SceneGraph* Scene);
            const Vec4 &GetLightColor() const;
            
            bool IsCastingShadows() { return isCastingShadows; }
            void DisableCastShadows();
            
            FrameBuffer* GetShadowFBO();
            
            Texture* GetShadowMapTexture() { return ShadowMap; }
            
            uint32 GetShadowWidth()
            {
                return ShadowWidth;
            }
            
            uint32 GetShadowHeight() 
            {
                return ShadowHeight;
            }
            
            bool IsUsingGPUShadows();
            
        protected:
            
            // Shadows Mapping
            // FrameBuffer
            FrameBuffer* shadowsFBO;
            // Dimensions
            uint32 ShadowWidth, ShadowHeight;
            // Shadow Map Texture
            Texture* ShadowMap;
            // Shadow Map Handle (Asset Manager)
            uint32 ShadowMapID;
            // Far ane Near for Projection
            f32 ShadowNear, ShadowFar;
            // Flag
            bool isCastingShadows;
            bool isUsingGPUShadows;
            // Light Color
            Vec4 Color;
            
            // Internal - List of Lights
            static std::vector<IComponent*> Components;
            // Internal - Lights on Scene
            static std::map<SceneGraph*, std::vector<IComponent*> > LightsOnScene;
            
    };
    
};

#endif /* ILIGHTCOMPONENT_H */