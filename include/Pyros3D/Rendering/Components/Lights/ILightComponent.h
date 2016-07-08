//============================================================================
// Name        : ILightComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Lights
//============================================================================

#ifndef ILIGHTCOMPONENT_H
#define	ILIGHTCOMPONENT_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Other/Export.h>
#include <vector>
#include <map>

namespace p3d {

	namespace LIGHT_TYPE
	{
		enum {
			DIRECTIONAL = 0,
			POINT,
			SPOT
		};
	}

    class PYROS3D_API ILightComponent : public IComponent {
        
        public:
            
            ILightComponent(const uint32 type);
            
            virtual ~ILightComponent();
            
            virtual void Register(SceneGraph* Scene);
            virtual void Init() {}
            virtual void Update() {}
            virtual void Destroy() {}
            virtual void Unregister(SceneGraph* Scene);
            
            static std::vector<IComponent*> &GetComponents();
            static std::vector<IComponent*> &GetLightsOnScene(SceneGraph* Scene);
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
            
			void SetShadowBias(const f32 factor, const f32 units)
			{
				ShadowBiasFactor = factor;
				ShadowBiasUnits = units;
			}
			const f32 &GetShadowBiasFactor() const
			{
				return ShadowBiasFactor;
			}

			const f32 &GetShadowBiasUnits() const
			{
				return ShadowBiasUnits;
			}

			const uint32 &GetLightType() const 
			{
				return LightType;
			}
        protected:
            
            // Shadows Mapping
            // FrameBuffer
            FrameBuffer* shadowsFBO;
            // Dimensions
            uint32 ShadowWidth, ShadowHeight;
            // Shadow Map Texture
            Texture* ShadowMap;
            // Far ane Near for Projection
            f32 ShadowNear, ShadowFar;
            // Flag
            bool isCastingShadows;
			// Bias Offset
			f32 ShadowBiasFactor, ShadowBiasUnits;

            // Light Color
            Vec4 Color;
            
            // Internal - List of Lights
            static std::vector<IComponent*> Components;
            // Internal - Lights on Scene
            static std::map<SceneGraph*, std::vector<IComponent*> > LightsOnScene;

			uint32 LightType;
            
    };
    
};

#endif /* ILIGHTCOMPONENT_H */