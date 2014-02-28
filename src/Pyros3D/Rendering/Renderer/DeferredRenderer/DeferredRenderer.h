//============================================================================
// Name        : DeferredRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Deferred Renderer
//============================================================================

#ifndef DEFERREDRENDERER_H
#define DEFERREDRENDERER_H

#include "../IRenderer.h"
#include "../../../Core/Projection/Projection.h"
#include "../../../Core/Buffers/FrameBuffer.h"

namespace p3d {
    
    class DeferredRenderer : public IRenderer {
        
        public:
            
            DeferredRenderer(const uint32 &Width, const uint32 &Height, FrameBuffer* fbo);
            
            ~DeferredRenderer();
            
            virtual std::vector<RenderingMesh*> GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera);
            
            virtual void RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene, const uint32 BufferOptions = Buffer_Bit::Color | Buffer_Bit::Depth);

            void SetFBO(FrameBuffer* fbo);
            
            virtual void Resize(const uint32& Width, const uint32& Height);

        private:
            GenericShaderMaterial* shadowMaterial, *shadowSkinnedMaterial;

        protected:

            void CreateQuad();

            // Offscreen Frame Buffer Object
            FrameBuffer* FBO;

            // Point Light Volume
            RenderingComponent *deferredQuad;
            GameObject* Quad;
            RenderingComponent *pointLight;
            GenericShaderMaterial *pointLightMaterial, *deferredMaterial;
    };
    
};

#endif /* DEFERREDRENDERER_H */