//============================================================================
// Name        : IRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Renderer Interface
//============================================================================

#ifndef IRENDERER_H
#define IRENDERER_H

#include "../../Core/Math/Math.h"
#include "../../SceneGraph/SceneGraph.h"
#include "../../Core/Projection/Projection.h"
#include "../../Materials/IMaterial.h"
#include "../../AssetManager/AssetManager.h"
#include "../Components/Rendering/RenderingComponent.h"
#include "../Components/Lights/DirectionalLight/DirectionalLight.h"
#include "../Components/Lights/SpotLight/SpotLight.h"
#include "../Components/Lights/PointLight/PointLight.h"
#include "../Components/Lights/ILightComponent.h"
#include "../Culling/FrustumCulling/FrustumCulling.h"

namespace p3d {

    namespace Buffer_Bit
    {
        enum {
            Color = 0x1,
            Depth = 0x2,
            Stencil = 0x4
        };
    }
    
    class IRenderer {
        
        public:
            
            IRenderer();
            IRenderer(const uint32 &Width, const uint32 &Height);
            virtual ~IRenderer();
            void ClearScreen(const uint32 &Option);
            void EnableDepthTest();
            void SetBackground(const Vec4 &Color);
            void SetGlobalLight(const Vec4 &Light);
            void EnableDepthBias(const Vec2 &Bias);
            void DisableDepthBias();
            void SetViewPort(const uint32 &initX, const uint32 &initY, const uint32 &endX, const uint32 &endY);
            // Resize
            void Resize(const uint32 &Width, const uint32 &Height);
            
            // culling
            void ActivateCulling(const unsigned &cullingType);
            void DeactivateCulling();
        
            // Render Scene
            virtual void RenderScene(const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene, bool clearScreen = true);
        
        protected:
            
            // Group by:
            //  -Asset and Material
            //  -Asset
            //  -Material
            //  -Owner
            // to avoid state changes - This should be done once
            // the rendering list is changed.
            // e.g. Add/Remove Models
            // Sort Assets (mostly the Translucent ones)
            virtual std::vector<RenderingMesh*> GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera) = 0;
            
            // Reset
            void InitRender();
            
            // Render Object
            void RenderObject(RenderingMesh* rmesh, IMaterial* Material);
            
            // End Rendering
            void EndRender();
            
            // Send Uniforms
            void SendGlobalUniforms(RenderingMesh* rmesh, IMaterial* Material);
            void SendUserUniforms(RenderingMesh* rmesh, IMaterial* Material);
            void SendModelUniforms(RenderingMesh* rmesh, IMaterial* Material);
            
            // Blending
            void EnableBlending();
            void DisableBlending();
            bool Blending;
            
            // WireFrame
            void EnableWireFrame();
            void DisableWireFrame();
            bool WireFrame;
            
            // Properties
            Vec4 
                                BackgroundColor;
            Vec4 
                                GlobalLight;
            
            // Face Culling
            int32
                                cullFace;
            
            // Dimensions
            uint32 
                                Width,
                                Height;
            
            // Scene
            SceneGraph* 
                                Scene;
            // Camera Matrix
            GameObject*
                                Camera;
            // Projection Matrix
            Projection
                                projection;
            
            // Depth Bias
            Vec2 
                                DepthBias;
            bool
                                IsUsingDepthBias;
            
            uint32
                                DrawType, InternalDrawType;
            
            // Culling
            bool CullingSphereTest(RenderingMesh* rmesh);
            bool CullingPointTest(RenderingMesh* rmesh);
            bool CullingBoxTest(RenderingMesh* rmesh);
            void UpdateCulling(const Matrix &ViewProjectionMatrix);
            bool 
                                IsCulling;            
            FrustumCulling*
                                culling;
            
            
            // Universal Uniforms Cache
            Matrix 
                                ProjectionMatrix,
                                ViewMatrix,
                                ViewProjectionMatrix,
                                ProjectionMatrixInverse,
                                ViewMatrixInverse;
            bool
                                ProjectionMatrixInverseIsDirty,
                                ViewMatrixInverseIsDirty,
                                ViewProjectionMatrixIsDirty;
            
            Vec3
                                CameraPosition;
            
            Vec2
                                NearFarPlane;
            
            std::vector<Matrix>
                                Lights;
            
            uint32
                                NumberOfLights;
            f64
                                Timer;
            // Model Specific Cache
            Matrix
                                ModelMatrix,
                                NormalMatrix,
                                ModelViewMatrix,
                                ModelViewProjectionMatrix,
                                ModelMatrixInverse,
                                ModelViewMatrixInverse,
                                ModelMatrixInverseTranspose;
            bool
                                NormalMatrixIsDirty,
                                ModelViewMatrixIsDirty,
                                ModelViewProjectionMatrixIsDirty,
                                ModelMatrixInverseIsDirty,
                                ModelViewMatrixInverseIsDirty,
                                ModelMatrixInverseTransposeIsDirty;            
            bool
                                ProjectionMatrixDirty,
                                ViewMatrixDirty;
            
            // Shadow Casting
            std::vector<Texture*>
                                DirectionalShadowMapsTextures, PointShadowMapsTextures, SpotShadowMapsTextures;
            std::vector<uint32>
                                DirectionalShadowMapsUnits, PointShadowMapsUnits, SpotShadowMapsUnits;
            std::vector<Matrix>
                                DirectionalShadowMatrix, PointShadowMatrix, SpotShadowMatrix;
            Vec4
                                DirectionalShadowFar;
            uint32 
                                NumberOfDirectionalShadows, NumberOfPointShadows, NumberOfSpotShadows;
            
            // ViewPort Size
            uint32              viewPortStartX,
                                viewPortStartY,
                                viewPortEndX,
                                viewPortEndY;
            bool
                                customViewPort;
            
            // Internal ViewPort Dimension
            void _SetViewPort(const uint32 &initX, const uint32 &initY, const uint32 &endX, const uint32 &endY);
            
        private:
            
            void BindMesh(RenderingMesh* rmesh, IMaterial* material);
            void UnbindMesh(RenderingMesh* rmesh, IMaterial* material);
            void SendAttributes(RenderingMesh* rmesh, IMaterial* material);
            void BindShadowMaps(IMaterial* material);
            void UnbindShadowMaps(IMaterial* material);
            
            // Last Mesh Rendered
            int32
                                LastMeshRendered;
            RenderingMesh*
                                LastMeshRenderedPTR;
            
            // Last Material Used
            int32
                                LastMaterialUsed,
                                LastProgramUsed;
            IMaterial*
                                LastMaterialPTR;
            
            // Internal ViewPort Dimension
            static uint32
                                _viewPortStartX,
                                _viewPortStartY,
                                _viewPortEndX,
                                _viewPortEndY;
    };
    
};

#endif /* IRENDERER_H */