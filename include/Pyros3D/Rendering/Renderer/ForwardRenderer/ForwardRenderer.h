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
#include <algorithm>

namespace p3d {

    class PYROS3D_API ForwardRenderer : public IRenderer {
        
        public:
            
            ForwardRenderer(const uint32 &Width, const uint32 &Height);
            
            ~ForwardRenderer();
            
            virtual std::vector<RenderingMesh*> GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera, const uint32 &Tag = 0);
            
			virtual void RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene);
            virtual void RenderSceneByTag(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene, const uint32 &Tag = 0);
			virtual void RenderSceneByTag(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene, const std::string &Tag = "NULL");
            
        private:
            
            GenericShaderMaterial* shadowMaterial, *shadowSkinnedMaterial;
            
    };
    
};

#endif /* FORWARDRENDERER_H */