//============================================================================
// Name        : PointLight.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Point Light
//============================================================================

#ifndef POINTLIGHT_H
#define	POINTLIGHT_H

#include "../ILightComponent.h"

namespace p3d {

    class PointLight : public ILightComponent {
        
        public:
            
            PointLight() { Color = Vec4(1,1,1,1); Radius = 1.f; }
            PointLight(const Vec4 &color, const f32 &radius) { Color = color; Radius = radius; }
            virtual ~PointLight() {}

            virtual void Start() {};
            virtual void Update() {};
            virtual void Destroy() {};
        
            Matrix GetLightProjection() { return ShadowProjection; }
        
            const f32 &GetLightRadius() const { return Radius; }
            
            void EnableCastShadows(const uint32 &Width, const uint32 &Height, const f32 &Near = 0.1f)
            {

                ShadowWidth = Width;
                ShadowHeight = Height;

                // Set Flag
                isCastingShadows = true;

                // Initiate FBO
                shadowsFBO = new FrameBuffer();

                // GPU Shadows
                // Create Texture (CubeMap), Frame Buffer and Set the Texture as Attachment
                ShadowMapID = AssetManager::CreateTexture(TextureType::CubemapNegative_X,TextureDataType::DepthComponent,ShadowWidth,ShadowHeight,false);
                AssetManager::AddTexture(ShadowMapID,TextureType::CubemapNegative_Y,TextureDataType::DepthComponent,ShadowWidth,ShadowHeight,false);
                AssetManager::AddTexture(ShadowMapID,TextureType::CubemapNegative_Z,TextureDataType::DepthComponent,ShadowWidth,ShadowHeight,false);
                AssetManager::AddTexture(ShadowMapID,TextureType::CubemapPositive_X,TextureDataType::DepthComponent,ShadowWidth,ShadowHeight,false);
                AssetManager::AddTexture(ShadowMapID,TextureType::CubemapPositive_Y,TextureDataType::DepthComponent,ShadowWidth,ShadowHeight,false);
                AssetManager::AddTexture(ShadowMapID,TextureType::CubemapPositive_Z,TextureDataType::DepthComponent,ShadowWidth,ShadowHeight,false);

                ShadowMap = static_cast<Texture*> (AssetManager::GetAsset(ShadowMapID)->AssetPTR);
                ShadowMap->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
                ShadowMap->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);
                ShadowMap->EnableCompareMode();

                // Initialize Frame Buffer
                shadowsFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment,TextureType::CubemapNegative_X,ShadowMap,false);

                // Near and Far Clip Planes
                ShadowNear = Near;
                ShadowFar = Radius;
                
                // Create Projection Matrix
                ShadowProjection = Matrix::PerspectiveMatrix(90.f, 1.0, ShadowNear, ShadowFar);
            }
            
        protected:
            
            // Attenuation
            f32 Radius;
            
            Matrix ShadowProjection;

    };

}

#endif	/* POINTLIGHT_H */