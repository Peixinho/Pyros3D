//============================================================================
// Name        : DeferredRendering.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef DEFERREDRENDERING_H
#define DEFERREDRENDERING_H

#if defined(_SDL)
    #include "../WindowManagers/SDL/SDLContext.h"
    #define ClassName SDLContext
#elif defined(_SDL2)
    #include "../WindowManagers/SDL2/SDL2Context.h"
    #define ClassName SDL2Context
#else
    #include "../WindowManagers/SFML/SFMLContext.h"
    #define ClassName SFMLContext
#endif

#include "DeferredRenderer/DeferredRenderer.h"
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>

using namespace p3d;

class DeferredRendering : public ClassName
{
        
    public:
        
        DeferredRendering();   
        virtual ~DeferredRendering();
        
        virtual void Init();
        virtual void Update();
        virtual void Shutdown();
        virtual void OnResize(const uint32 width, const uint32 height);
        
    private:

        // Scene
        SceneGraph* Scene;
        // Renderer
        DeferredRenderer* Renderer;
        // Projection
        Projection projection;
        // Camera - Its a regular GameObject
        GameObject* Camera;
        // Light
        std::vector<GameObject*> Lights;
        std::vector<PointLight*> pLights;

        Renderable* cubeHandle;
        std::vector<GameObject*> Cubes;
        std::vector<RenderingComponent*> rCubes;

        // Material
        CustomShaderMaterial* Diffuse;

        // Deferred Settings
        Texture* albedoTexture, *specularTexture, *normalTexture, *positionTexture, *depthTexture;
        FrameBuffer* deferredFBO;

};

#endif  /* DEFERREDRENDERING_H */

