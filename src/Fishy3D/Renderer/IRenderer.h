//============================================================================
// Name        : Renderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Renderer
//============================================================================

#ifndef IRENDERER_H
#define IRENDERER_H

#include "../SceneGraph/SceneGraph.h"
#include "../Core/Projection/Projection.h"
#include "../Core/Buffers/FrameBuffer.h"
#include "../Core/GameObjects/GameObject.h"
#include "../Core/Projection/Projection.h"
#include "../Components/RenderingComponents/IRenderingComponent.h"
#include "../Components/LightComponents/ILightComponent.h"
#include "Culling/Culling.h"
#include "PostEffects/PostEffectsManager.h"
#include "ShadowMapping/CastShadows/CastShadows.h"

#define GLCHECK() { int error = glGetError(); if(error != GL_NO_ERROR) { std::cout <<  "GL Error: " << std::hex << error << std::endl; } }
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

#include <map>

namespace Fishy3D {

    namespace DebugDrawing
    {
        enum {
            Normals  = 1 << 0,
            BoundingBox = 1 << 1
        };
    }
    
    namespace RendererProjection 
    {
        enum {
            PerspectiveRH = 0,
            PerspectiveLH,
            OrthogonalRH,
            OrthogonalLH
        };
    }

    namespace CullingMode
    {
        enum
        {
            FrustumCulling = 0
        };
    }
    
    class ILightComponent;
    class IRenderingComponent;
    class SceneGraph;
    class CastShadows;

    class IRenderer {
        public:
                        
            IRenderer(const unsigned &width, const unsigned &height);
            
            // Resize
            void Resize(const unsigned &width, const unsigned &height);

            // Sort only neede whit many translucid objects on screen to do a sort by z-order
            void Sort(bool sort = false);
            
            // Render
            virtual void RenderScene(SceneGraph* scene, GameObject* camera, Projection* projection, bool clearScreen = true) = 0;
            virtual void RenderByTag(SceneGraph* scene, GameObject* camera, Projection* projection, const StringID &TagID, bool clearScreen = true) = 0;
            
            void RenderByTag(SceneGraph* scene, GameObject* camera, Projection* projection, const std::string &Tag);
            ~IRenderer();                        
            
            // Wireframe
            void StartRenderWireFrame();
            void StopRenderWireFrame();
            
            // Debug
            void StartDebugMode(const unsigned &DebugMode);
            void StopDebugMode();
            
            // culling
            void ActivateCulling(const unsigned &cullingType);
            void DeactivateCulling();
            
            // Frame Buffer
            void SetFrameBuffer(SuperSmartPointer<FrameBuffer> fbo);
            void DisableFrameBuffer();
            
            // Ambient Light
            void SetAmbientLight(const vec4 &Color = vec4(0.1,0.1,0.1,0.1));
            // Background Color
            void SetBackgroundColor(const vec4 &Color = vec4(0,0,0,0));

			// Clear Screen
			void ClearScreen();
            
        protected:
            
            // Virtual method to Render
            virtual void Render(bool clearScreen = true) = 0;
            
            // Saves Scene ptr
            SceneGraph* scene;
            
            // Temporary Lists
            std::list<IRenderingComponent*> _OpaqueList;
            std::list<IRenderingComponent*> _TranslucidList;
            
            // Frame Buffer
            bool FrameBufferDefined;
            FrameBuffer* FBO;
            
            // Internal Frame Buffer and Post Effects
            SuperSmartPointer<FrameBuffer> internalFBO;
            
            // Cast Shadows
            SuperSmartPointer<CastShadows> castShadows;
            
            // Drawing Type
            unsigned int drawType;
            
            // DEBUG            
            bool debugNormals, debugBoundingBoxes;
            #if _DEBUG
            SuperSmartPointer<IMaterial> debugMaterial;
            #endif
            
            // culling
            bool IsCulling;            
            SuperSmartPointer<Culling> culling;
            
            // camera
            GameObject* camera;
        
            // projection
            Projection* projection;
        
            // Sort
            bool sort;
                        
            // Global Ambient Light
            vec4 GlobalAmbientLight;
            
            // Background Color
            vec4 BackgroundColor;            
            
            // Culling
            bool CullingTest(IRenderingComponent* rcomp);
            void UpdateCulling(const Matrix &view, const Matrix &projection);
            unsigned cullMode;
            
            // Face Culling
            int cullFace;
            
            // Uniforms
            void SendGlobalUniforms(IMaterial* material);
            void SendUserUniforms(IMaterial* material);
            void SendModelUniforms(IRenderingComponent* rcomp, IMaterial* material);
            
            // Dimensions
            unsigned Width, Height;
            
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
            
            vec3
                        CameraPosition;
            
            vec2
                        NearFarPlane;
            
            std::vector<Matrix>
                        Lights;
            
            unsigned 
                        Timer,
                        NumberOfLights;
            
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
            long int 
                        LastProgramUsed;
            bool
                        ProjectionMatrixDirty,
                        ViewMatrixDirty;
            
            // Shadow Casting
            std::vector<Texture> 
                        ShadowMapsTextures;
            std::vector<unsigned>
                        ShadowMapsUnits;
            std::vector<Matrix>
                        ShadowMatrix;
			vec4 
						ShadowFar;
            unsigned 
                        NumberOfShadows;
    };

}

#endif	/* IRENDERER_H */
