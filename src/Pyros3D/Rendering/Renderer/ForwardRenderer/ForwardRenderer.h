//============================================================================
// Name        : ForwardRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Forward Renderer
//============================================================================

#ifndef FORWARDRENDERER_H
#define FORWARDRENDERER_H

#include "../IRenderer.h"
#include "../../../Core/Projection/Projection.h"

namespace p3d {
    
    class ForwardRenderer : public IRenderer {
        
        public:
            
            ForwardRenderer(const uint32 &Width, const uint32 &Height);
            
            ~ForwardRenderer();
            
            virtual std::vector<RenderingMesh*> GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera);
            
            virtual void RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene, const uint32 BufferOptions = Buffer_Bit::Color | Buffer_Bit::Depth);

            
        private:
            GenericShaderMaterial* shadowMaterial, *shadowSkinnedMaterial;
        protected:
            
    };
    
};

#endif /* FORWARDRENDERER_H */