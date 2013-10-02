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
            
            virtual void RenderScene(const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene, bool clearScreen = true);
            
        private:
            GenericShaderMaterial* shadowMaterial;
        protected:
            
    };
    
};

#endif /* FORWARDRENDERER_H */