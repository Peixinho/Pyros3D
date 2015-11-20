//============================================================================
// Name        : PointLight.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Point Light
//============================================================================

#ifndef POINTLIGHT_H
#define	POINTLIGHT_H

#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>

namespace p3d {

    class PYROS3D_API PointLight : public ILightComponent {
        
        public:
            
            PointLight() { Color = Vec4(1,1,1,1); Radius = 1.f; }
            PointLight(const Vec4 &color, const f32 radius) 
			{
				Color = color;
				Radius = radius;

				// Bounding
				minBounds = Vec3(-radius*.5f, -radius*.5f, -radius*.5f);
				maxBounds = Vec3(radius*.5f, radius*.5f, radius*.5f);
				BoundingSphereCenter = Vec3();
				BoundingSphereRadius = radius;
			}
            virtual ~PointLight() {}

            virtual void Start() {};
            virtual void Update() {};
            virtual void Destroy() {};
        
            Projection GetLightProjection() { return ShadowProjection; }
        
            const f32 &GetLightRadius() const { return Radius; }
            
            void EnableCastShadows(const uint32 Width, const uint32 Height, const f32 Near = 0.1f)
            {

                ShadowWidth = Width;
                ShadowHeight = Height;

                // Set Flag
                isCastingShadows = true;

                // Initiate FBO
                shadowsFBO = new FrameBuffer();

				ShadowMap = new Texture();
				
#if defined(GLES2)
				// Regular Shadows
				// Create Texture (CubeMap), Frame Buffer and Set the Texture as Attachment
				ShadowMap->CreateEmptyTexture(TextureType::CubemapNegative_X, TextureDataType::RGB, ShadowWidth, ShadowHeight, false);
				ShadowMap->CreateEmptyTexture(TextureType::CubemapNegative_Y, TextureDataType::RGB, ShadowWidth, ShadowHeight, false);
				ShadowMap->CreateEmptyTexture(TextureType::CubemapNegative_Z, TextureDataType::RGB, ShadowWidth, ShadowHeight, false);
				ShadowMap->CreateEmptyTexture(TextureType::CubemapPositive_X, TextureDataType::RGB, ShadowWidth, ShadowHeight, false);
				ShadowMap->CreateEmptyTexture(TextureType::CubemapPositive_Y, TextureDataType::RGB, ShadowWidth, ShadowHeight, false);
				ShadowMap->CreateEmptyTexture(TextureType::CubemapPositive_Z, TextureDataType::RGB, ShadowWidth, ShadowHeight, false);
				ShadowMap->SetRepeat(TextureRepeat::ClampToBorder, TextureRepeat::Clamp, TextureRepeat::Clamp);

				// Initialize Frame Buffer
				shadowsFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, ShadowWidth, ShadowHeight);
				shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0,TextureType::CubemapPositive_X,ShadowMap);

#else
                // GPU Shadows
                // Create Texture (CubeMap), Frame Buffer and Set the Texture as Attachment
                ShadowMap->CreateEmptyTexture(TextureType::CubemapNegative_X,TextureDataType::DepthComponent,ShadowWidth,ShadowHeight,false);
                ShadowMap->CreateEmptyTexture(TextureType::CubemapNegative_Y,TextureDataType::DepthComponent,ShadowWidth,ShadowHeight,false);
                ShadowMap->CreateEmptyTexture(TextureType::CubemapNegative_Z,TextureDataType::DepthComponent,ShadowWidth,ShadowHeight,false);
                ShadowMap->CreateEmptyTexture(TextureType::CubemapPositive_X,TextureDataType::DepthComponent,ShadowWidth,ShadowHeight,false);
                ShadowMap->CreateEmptyTexture(TextureType::CubemapPositive_Y,TextureDataType::DepthComponent,ShadowWidth,ShadowHeight,false);
                ShadowMap->CreateEmptyTexture(TextureType::CubemapPositive_Z,TextureDataType::DepthComponent,ShadowWidth,ShadowHeight,false);
                ShadowMap->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);
                ShadowMap->EnableCompareMode();

                // Initialize Frame Buffer
                shadowsFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment,TextureType::CubemapPositive_X,ShadowMap);

#endif

                // Near and Far Clip Planes
                ShadowNear = Near;
                ShadowFar = Radius;
                
                // Create Projection Matrix
                ShadowProjection.Perspective(90.f, 1.0, ShadowNear, ShadowFar);
            }
            
        protected:
            
            // Attenuation
            f32 Radius;
            
            // Projection
            Projection ShadowProjection;

    };

}

#endif	/* POINTLIGHT_H */