//============================================================================
// Name        : DirectionalLight.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Directional Light
//============================================================================

#ifndef DIRECTIONALLIGHT_H
#define	DIRECTIONALLIGHT_H

#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>

namespace p3d {

    // Maximum Number of Splits
    #define MAX_SPLITS 4
    // Split Weight
    #define SPLIT_WEIGHT 0.75
    // the 0.2f factor is important because we might get artifacts at
    #define CASCADE_FACTOR 0.2

    struct PYROS3D_API Cascade {

        // Sub Frustum Properties
        f32 Near;
        f32 Far;
        f32 Ratio;
        f32 Fov;
        f32 Width;
        f32 Height;
        Vec3 point[8];
        Matrix CropMatrix;
        Projection ortho;

        void UpdateFrustumPoints(const Vec3& position, const Vec3& direction)
        {
            Vec3 _direction = direction *-1.f;
            Vec3 right = _direction.cross(Vec3::UP);

            Vec3 fc = position + _direction * Far;
            Vec3 nc = position + _direction * Near;

            right.normalizeSelf();
            Vec3 up = right.cross(_direction).normalize();	

            // these heights and widths are half the heights and widths of
            // the near and far plane rectangles
            f32 near_height = tan(Fov/2.0f) * Near;
            f32 near_width = near_height * Ratio;
            f32 far_height = tan(Fov/2.0f) * Far;
            f32 far_width = far_height * Ratio;

            point[0] = nc - up*near_height - right*near_width;
            point[1] = nc + up*near_height - right*near_width;
            point[2] = nc + up*near_height + right*near_width;
            point[3] = nc - up*near_height + right*near_width;

            point[4] = fc - up*far_height - right*far_width;
            point[5] = fc + up*far_height - right*far_width;
            point[6] = fc + up*far_height + right*far_width;
            point[7] = fc - up*far_height + right*far_width;
        }

        // this function builds a projection matrix for rendering from the shadow's POV.
        // First, it computes the appropriate z-range and sets an orthogonal projection.
        // Then, it translates and scales it, so that it exactly captures the bounding box
        // of the current frustum slice
        Matrix CreateCropMatrix(const Matrix &viewMatrix, std::vector<RenderingMesh*> rcomps)
        {
                
            f32 maxX = -10000.0f;
            f32 maxY = -10000.0f;
            f32 maxZ;
            f32 minX =  10000.0f;
            f32 minY =  10000.0f;
            f32 minZ;

            Vec4 transf;
            transf = viewMatrix * Vec4(point[0].x,point[0].y,point[0].z,1.0f);
            minZ = transf.z;
            maxZ = transf.z;

            for(int i=1; i<8; i++)
            {
                transf = viewMatrix * Vec4(point[i].x,point[i].y,point[i].z,1.0f);
                if(transf.z > maxZ) maxZ = transf.z;
                if(transf.z < minZ) minZ = transf.z;
            }
            
            for(std::vector<RenderingMesh*>::iterator i=rcomps.begin(); i!=rcomps.end(); i++)
            {
                Vec3 pos = Vec3((*i)->Geometry->GetBoundingSphereCenter().x*(*i)->renderingComponent->GetOwner()->GetWorldPosition().x,(*i)->Geometry->GetBoundingSphereCenter().y*(*i)->renderingComponent->GetOwner()->GetWorldPosition().y,(*i)->Geometry->GetBoundingSphereCenter().z*(*i)->renderingComponent->GetOwner()->GetWorldPosition().z);
                transf = viewMatrix * Vec4(pos.x,pos.y,pos.z, 1.0f);
                if(transf.z + (*i)->Geometry->GetBoundingSphereRadius() > maxZ) maxZ = transf.z + (*i)->Geometry->GetBoundingSphereRadius();
            }

            // Make Ortho
            ortho.Ortho(-1,1,-1,1,-maxZ,-minZ);
            
            Matrix mvp = ortho.GetProjectionMatrix() * viewMatrix;
            
            // find the extends of the frustum slice as projected in light's homogeneous coordinates        
            for(unsigned i=0; i<8; i++)
            {
                transf = mvp*Vec4(point[i].x,point[i].y,point[i].z,1.0f);

                transf.x /= transf.w;
                transf.y /= transf.w;

                if(transf.x > maxX) maxX = transf.x;
                if(transf.x < minX) minX = transf.x;
                if(transf.y > maxY) maxY = transf.y;
                if(transf.y < minY) minY = transf.y;
            }

            f32 scaleX = 2.0f/(maxX - minX);
            f32 scaleY = 2.0f/(maxY - minY);
            f32 offsetX = -0.5f*(maxX + minX)*scaleX;
            f32 offsetY = -0.5f*(maxY + minY)*scaleY;

            Matrix crop;            
            crop.ScaleX(scaleX);
            crop.ScaleY(scaleY);
            crop.TranslateX(offsetX);
            crop.TranslateY(offsetY);

            CropMatrix = crop * ortho.GetProjectionMatrix();

            return CropMatrix;
        }

        Matrix GetCropMatrix()
        {
            return CropMatrix;
        }
    };
    
    class PYROS3D_API DirectionalLight : public ILightComponent {
        
        public:
            
            DirectionalLight() : ILightComponent() { Color = Vec4(1,1,1,1); Direction = Vec3(0,1,0); }
            DirectionalLight(const Vec4 &color) { Color = color; Direction = Vec3(0,-1,0); }
            DirectionalLight(const Vec4 &color, const Vec3 &direction) { Color = color; Direction = direction; }
            virtual ~DirectionalLight() {}

            virtual void Start() {};
            virtual void Update() {};
            virtual void Destroy() {};
            
            void EnableCastShadows(const uint32 Width, const uint32 Height, const Projection &projection, const f32 Near, const f32 Far, const uint32 Cascades = 1)
            {
                if (!isCastingShadows)
                {
                    // Save Dimensions
                    uint32 ShadowWidthFBO = Width;
                    uint32 ShadowHeightFBO = Height;
                    ShadowWidth = Width;
                    ShadowHeight = Height;
                    
                    // Set Flag
                    isCastingShadows = true;

                    // Initiate FBO
                    shadowsFBO = new FrameBuffer();

                    // Minimum 1 and Maximum is 4 Cascades
                    if (Cascades<=0) ShadowCascades = 1;
                    else if (Cascades>=4) ShadowCascades = 4;
                    else ShadowCascades = Cascades;
                    
                    // Set FBO Dimensions
                    if (ShadowCascades>1)
                    {
                        ShadowWidthFBO = Width*2;
                        ShadowHeightFBO = Height*2;
                    }
                    
                    // GPU Shadow Maps
                    // Create Texture, Frame Buffer and Set the Texture as Attachment
                    ShadowMap = new Texture();
                    ShadowMap->CreateEmptyTexture(TextureType::Texture,TextureDataType::DepthComponent,ShadowWidthFBO,ShadowHeightFBO,false);
                    ShadowMap->SetRepeat(TextureRepeat::Clamp,TextureRepeat::Clamp);
                    ShadowMap->EnableCompareMode();
                    shadowsFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment,TextureType::Texture,ShadowMap);

                    // Near and Far Clip Planes
                    ShadowNear = Near;
                    ShadowFar = Far;
                    
                    // View Projection Matrix
                    ShadowProjection = projection.m;

                    for (uint32 i=0; i<MAX_SPLITS; i++)
                    {
                        // note that fov is in radians here and in OpenGL it is in degrees.
                        this->Cascades[i].Fov = RADTODEG(projection.Fov) + CASCADE_FACTOR;
                        this->Cascades[i].Ratio = projection.Aspect;
                    }
                    
                    // Build Frustums
                    f32 lambda = SPLIT_WEIGHT;
                    f32 ratio = Far/Near;
                    this->Cascades[0].Near = Near;
                    this->Cascades[0].Width = Width;
                    this->Cascades[0].Height = Height;
                    
                    for (uint32 i=1; i<ShadowCascades; i++)
                    {
                        f32 si = (f32)i / (f32)ShadowCascades;
                        
                        this->Cascades[i].Near = lambda*(Near*powf(ratio, si)) + (1.f-lambda)*(Near + (Far - Near)*si);
                        this->Cascades[i-1].Far = this->Cascades[i].Near * 1.005f;
                        this->Cascades[i].Width = Width;
                        this->Cascades[i].Height = Height;
                    }
                    // Set Far on Last Split
                    this->Cascades[ShadowCascades-1].Far = Far;
                    this->Cascades[ShadowCascades-1].Width = Width;
                    this->Cascades[ShadowCascades-1].Height = Height;
                }
            }
        
            Matrix GetLightViewMatrix()
            {
                ShadowViewMatrix.identity();
                ShadowViewMatrix.LookAt(Vec3::ZERO,(Direction.normalize()),Vec3(0.f,0.f,-1.f));
                return ShadowViewMatrix;
            }
            Matrix GetLightProjection(const uint32 Cascade, const std::vector<RenderingMesh*> RCompList)
            {
                return Cascades[Cascade].CreateCropMatrix(ShadowViewMatrix, RCompList);
            }
            void UpdateCascadeFrustumPoints(const uint32 Cascade, const Vec3 &CameraPosition, const Vec3 &CameraDirection)
            {
                Cascades[Cascade].UpdateFrustumPoints(CameraPosition, CameraDirection);
            }
        
            uint32 GetNumberCascades() { return ShadowCascades; }
            
            Cascade GetCascade(const uint32 Cascade)
            {
                return Cascades[Cascade];
            }
            
            const Vec3 &GetLightDirection() const { return Direction; }
            void SetLightDirection(const Vec3 &direction) { Direction = direction; }

        private:

            // Light Direction
            Vec3 Direction;

            // Shadow Cast
            uint32 ShadowCascades;
            Matrix ShadowProjection;
            Cascade Cascades[4];
            Matrix ShadowViewMatrix;
            bool shadowsOnGPU;
        
    };

}

#endif	/* DIRECTIONALLIGHT_H */