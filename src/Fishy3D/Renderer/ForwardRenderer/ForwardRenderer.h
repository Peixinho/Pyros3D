//============================================================================
// Name        : ForwardRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Forawrd Renderer
//============================================================================

#ifndef FORWARDRENDERER_H
#define FORWARDRENDERER_H

#include "../IRenderer.h"
#include "../Culling/FrustumCulling/FrustumCulling.h"

#include <map>

namespace Fishy3D {

    class ForwardRenderer : public IRenderer {
        public:

            ForwardRenderer(const unsigned &width, const unsigned &height);

            // Render
            virtual void RenderScene(SceneGraph* scene, GameObject* camera, Projection* projection, bool clearScreen = true);
            virtual void RenderByTag(SceneGraph* scene, GameObject* camera, Projection* projection, const StringID &TagID, bool clearScreen = true);
            ~ForwardRenderer();
            
        private:                        
            
            // Render
            virtual void Render(bool clearScreen = true);
            
            // Draw Component
            void ProcessRenderingComponent(IRenderingComponent* rcomp);
    };

}

#endif	/* FORWARDRENDERER_H */

