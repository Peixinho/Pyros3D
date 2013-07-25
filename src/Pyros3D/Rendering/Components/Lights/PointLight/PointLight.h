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
            
            void EnableCastShadows(const uint32 &Width, const uint32 &Height, const float &Near = 0.1f, bool GPU = true)
            {

                ShadowWidth = Width;
                ShadowHeight = Height;

                // Set Flag
                isCastingShadows = true;

                // Initiate FBO
                shadowsFBO = new FrameBuffer();

                // Set Flag
                isUsingGPUShadows = GPU;

                // Regular Shadow Maps
                if (!GPU)
                {
                    // Regular Shadows
                    // Create Texture (CubeMap), Frame Buffer and Set the Texture as Attachment
                    ShadowMapID = AssetManager::CreateTexture(TextureType::CubemapNegative_X,TextureDataType::RG32F,ShadowWidth,ShadowHeight,false);
                    AssetManager::AddTexture(ShadowMapID,TextureType::CubemapNegative_Y,TextureDataType::RG32F,ShadowWidth,ShadowHeight,false);
                    AssetManager::AddTexture(ShadowMapID,TextureType::CubemapNegative_Z,TextureDataType::RG32F,ShadowWidth,ShadowHeight,false);
                    AssetManager::AddTexture(ShadowMapID,TextureType::CubemapPositive_X,TextureDataType::RG32F,ShadowWidth,ShadowHeight,false);
                    AssetManager::AddTexture(ShadowMapID,TextureType::CubemapPositive_Y,TextureDataType::RG32F,ShadowWidth,ShadowHeight,false);
                    AssetManager::AddTexture(ShadowMapID,TextureType::CubemapPositive_Z,TextureDataType::RG32F,ShadowWidth,ShadowHeight,false);

                    ShadowMap = static_cast<Texture*> (AssetManager::GetAsset(ShadowMapID)->AssetPTR);
                    ShadowMap->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
                    ShadowMap->SetRepeat(TextureRepeat::ClampToBorder,TextureRepeat::Clamp,TextureRepeat::Clamp);

                    // Initialize Frame Buffer
                    shadowsFBO->Init(FrameBufferAttachmentFormat::Color_Attachment0,TextureType::CubemapNegative_X,ShadowMap,RenderBufferType::Color,ShadowWidth,ShadowHeight,true);
//                    shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment,TextureType::CubemapNegative_Y,ShadowMap);
//                    shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment,TextureType::CubemapNegative_Z,ShadowMap);
//                    shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment,TextureType::CubemapPositive_X,ShadowMap);
//                    shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment,TextureType::CubemapPositive_Y,ShadowMap);
//                    shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment,TextureType::CubemapPositive_Z,ShadowMap);
                }
                 
                else {
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
                    ShadowMap->SetRepeat(TextureRepeat::ClampToBorder,TextureRepeat::Clamp,TextureRepeat::Clamp);
                    ShadowMap->EnableCompareMode();

                    // Initialize Frame Buffer
                    shadowsFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment,TextureType::CubemapNegative_X,ShadowMap,false);
//                    shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment,TextureType::CubemapNegative_Y,ShadowMap);
//                    shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment,TextureType::CubemapNegative_Z,ShadowMap);
//                    shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment,TextureType::CubemapPositive_X,ShadowMap);
//                    shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment,TextureType::CubemapPositive_Y,ShadowMap);
//                    shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment,TextureType::CubemapPositive_Z,ShadowMap);
                    
                }
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