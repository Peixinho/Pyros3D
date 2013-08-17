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
            
            // culling
            void ActivateCulling(const unsigned &cullingType);
            void DeactivateCulling();
        
            // Render Scene
        virtual void RenderScene(const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene);
        
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
            virtual void GroupAndSortAssets() = 0;
            
            // Render Object
            void RenderObject(RenderingMesh* rmesh, IMaterial* Material);
            
            // Send Uniforms
            void SendGlobalUniforms(RenderingMesh* rmesh, IMaterial* Material);
            void SendUserUniforms(RenderingMesh* rmesh, IMaterial* Material);
            void SendModelUniforms(RenderingMesh* rmesh, IMaterial* Material);
            
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
            void UpdateCulling(const Matrix &view, const p3d::Projection &projection);
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
            
            unsigned 
                                NumberOfLights;
            d64
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
            // Last Program Used
            lint32
                                LastProgramUsed;
            bool
                                ProjectionMatrixDirty,
                                ViewMatrixDirty;
            
            // Shadow Casting
            std::vector<Texture> 
                                DirectionalShadowMapsTextures, PointShadowMapsTextures, SpotShadowMapsTextures;
            std::vector<uint32>
                                DirectionalShadowMapsUnits, PointShadowMapsUnits, SpotShadowMapsUnits;
            std::vector<Matrix>
                                DirectionalShadowMatrix, PointShadowMatrix, SpotShadowMatrix;
            std::vector<Vec4>
                                DirectionalShadowFar;
            uint32 
                                NumberOfDirectionalShadows, NumberOfPointShadows, NumberOfSpotShadows;
            
        private:
            
    };
    
};

#endif /* IRENDERER_H */