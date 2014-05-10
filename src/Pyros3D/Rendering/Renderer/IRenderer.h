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
            None = 0,
            Color = 0x10,
            Depth = 0x20,
            Stencil = 0x40
        };
    }
    
    namespace StencilOp
    {
    	enum {
    		Keep = 0,
    		Zero,
    		Replace,
    		Incr,
    		Incr_Wrap,
    		Decr,
    		Decr_Wrap,
    		Invert
    	};
    }

    namespace StencilFunc
    {
    	enum {
    		Always = 0,
    		Never,
    		Less,
    		LEqual,
    		Greater,
    		GEqual,
    		Equal,
    		Notequal
    	};
    }

    namespace BlendFunc
    {
    	enum {
    		Zero = 0,
    		One,
    		Src_Color,
    		One_Minus_Src_Color,
    		Dst_Color,
    		One_Minus_Dst_Color,
    		Src_Alpha,
    		One_Minus_Src_Alpha,
    		Dst_Alpha,
    		One_Minus_Dst_Alpha,
    		Constant_Color,
    		One_Minus_Constant_Color,
    		Constant_Alpha,
    		One_Minus_Constant_Alpha,
    		Src_Alpha_Saturate,
    		Src1_Color,
    		One_Minus_Src1_Color,
    		Src1_Alpha,
    		One_Minus_Src1_Alpha
    	};
    }

    namespace BlendEq
    {
    	enum {
    		Add = 0,
    		Subtract,
    		Reverse_Subtract
    	};
    }

    class IRenderer {
        
        public:
            
            IRenderer();
            IRenderer(const uint32 &Width, const uint32 &Height);
            virtual ~IRenderer();
            void ClearBufferBit(const uint32 &Option);

            // Depth Buffer
            void EnableDepthTest();
            void DisableDepthTest();
            void EnableDepthWritting();
            void DisableDepthWritting();
            void EnableClearDepthBuffer();
            void DisableClearDepthBuffer();
            void ClearDepthBuffer();

            // Stencil
            void EnableStencil();
            void DisableStencil();
            void ClearStencilBuffer();
            void StencilFunction(const uint32 &func, const uint32 &ref = 0, const uint32 &mask = 1);
            void StencilOperation(const uint32 &sfail = StencilOp::Keep, const uint32 &dpfail = StencilOp::Keep, const uint32 &dppass = StencilOp::Keep);

            // Blending
            void EnableBlending();
            void DisableBlending();
            void BlendingFunction(const uint32 &sfactor, const uint32 &dfactor);
            void BlendingEquation(const uint32 &mode);

            // WireFrame
            void EnableWireFrame();
            void DisableWireFrame();

            // Color
            void ColorMask(const f32 &r,const f32 &g,const f32 &b,const f32 &a);

            // Sorting
            void EnableSorting();
            void DisableSorting();

            void SetBackground(const Vec4 &Color);
            void UnsetBackground();
            void SetGlobalLight(const Vec4 &Light);
            void EnableDepthBias(const Vec2 &Bias);
            void DisableDepthBias();
            void SetViewPort(const uint32 &initX, const uint32 &initY, const uint32 &endX, const uint32 &endY);
            
            // Resize
            void Resize(const uint32 &Width, const uint32 &Height);
            
            // culling
            void ActivateCulling(const uint32 &cullingType);
            void DeactivateCulling();
        
            // Render Scene
            virtual void RenderScene(const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene);
            virtual void RenderSceneByTag(const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene, const std::string &Tag = "");
            virtual void RenderSceneByTag(const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene, const uint32 &Tag = 0);
        
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
            virtual std::vector<RenderingMesh*> GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera, const uint32 &Tag = 0) = 0;
            
            // Reset
            void InitRender();
            
            // Render Object
            void RenderObject(RenderingMesh* rmesh, GameObject* owner, IMaterial* Material);
            
            // End Rendering
            void EndRender();
            
            // Depth Buffer
            void DepthTest();
            void DepthWrite();
            void ClearDepth();

            // Internal Function to Clear Buffers
            void ClearScreen();

            // Send Uniforms
            void SendGlobalUniforms(RenderingMesh* rmesh, IMaterial* Material);
            void SendUserUniforms(RenderingMesh* rmesh, IMaterial* Material);
            void SendModelUniforms(RenderingMesh* rmesh, IMaterial* Material);
            
            // Flags
            uint32 bufferOptions, glBufferOptions;
            bool depthWritting;
            bool depthTesting;
            bool clearDepthBuffer;
            bool sorting;

            // Background
            void DrawBackground();
            bool BackgroundColorSet;
            
            // Properties
            bool
                                blending;
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
            bool CullingSphereTest(RenderingMesh* rmesh, GameObject* owner);
            bool CullingPointTest(RenderingMesh* rmesh, GameObject* owner);
            bool CullingBoxTest(RenderingMesh* rmesh, GameObject* owner);
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
