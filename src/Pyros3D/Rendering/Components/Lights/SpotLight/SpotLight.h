//============================================================================
// Name        : SpotLight.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Spot Light
//============================================================================

#ifndef SPOTLIGHT_H
#define	SPOTLIGHT_H

#include "../ILightComponent.h"

namespace p3d {

    class SpotLight : public ILightComponent {
        
        public:
            
            SpotLight() { Color = Vec4(1,1,1,1); Radius = 1.f; }
            
            SpotLight(const Vec4 &color, const f32 &radius, const Vec3 &direction, const f32 &OutterCone, const f32 &InnerCone) 
            { 
                Color = color;
                Radius = radius;
                innerCone = InnerCone;
                outterCone = OutterCone;
                CosOutterCone = cosf(DEGTORAD(OutterCone));
                CosInnerCone= cosf(DEGTORAD(InnerCone));
                Direction = direction;
            }
            
            virtual ~SpotLight() {}

            virtual void Start() {};
            virtual void Update() {};
            virtual void Destroy() {};
                       
            const Vec3 &GetLightDirection() const { return Direction; }
            void SetLightDirection(const Vec3 &direction) { Direction = direction; }
            const f32 &GetLightCosInnerCone() const { return CosInnerCone; }
            const f32 &GetLightCosOutterCone() const { return CosOutterCone; }
            const f32 &GetLightInnerCone() const { return innerCone; }
            const f32 &GetLightOutterCone() const { return outterCone; }
            const f32 &GetLightRadius() const { return Radius; }
            
            Projection GetLightProjection() { return ShadowProjection; }
            
            void EnableCastShadows(const uint32 &Width, const uint32 &Height, const f32 &Near = 0.1f)
            {
                ShadowWidth = Width;
                ShadowHeight = Height;

                // Set Flag
                isCastingShadows = true;

                // Initiate FBO
                shadowsFBO = new FrameBuffer();
                
                // GPU Shadows
                ShadowMap = new Texture();
                ShadowMap->CreateTexture(TextureType::Texture,TextureDataType::DepthComponent,ShadowWidth,ShadowHeight,false);
                ShadowMap->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);
                ShadowMap->EnableCompareMode();

                // Initialize Frame Buffer
                shadowsFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment,TextureType::Texture,ShadowMap,false);

                // Near and Far Clip Planes
                ShadowNear = Near;
                ShadowFar = Radius;
                
                // Create Projection Matrix
                ShadowProjection.Perspective(2*outterCone, 1.0, ShadowNear, ShadowFar);
            }
            
        protected :
            
            // Light Direction
            Vec3 Direction;
            // Cone
            f32 outterCone, CosOutterCone, innerCone, CosInnerCone;
            // Attenuation
            f32 Radius;
            // Shadow Projection
            Projection ShadowProjection;

    };

}

#endif	/* SPOTLIGHT_H */