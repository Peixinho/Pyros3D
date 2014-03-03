//============================================================================
// Name        : DeferredRenderingExample.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef DEFERREDRENDERINGEXAMPLE_H
#define DEFERREDRENDERINGEXAMPLE_H

#ifdef _SDL2
    #include "../WindowManagers/SDL2/SDL2Context.h"
#define ClassName SDLContext
#else
    #include "../WindowManagers/SFML/SFMLContext.h"
    #define ClassName SFMLContext
#endif

#include "Pyros3D/SceneGraph/SceneGraph.h"
#include "DeferredRenderer/DeferredRenderer.h"
#include "Pyros3D/Utils/Colors/Colors.h"
#include "Pyros3D/Rendering/Components/Rendering/RenderingComponent.h"
#include "Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h"
#include "Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h"
#include "Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h"

using namespace p3d;

class DeferredRenderingExample : public ClassName
{
        
    public:
        
        DeferredRenderingExample();   
        virtual ~DeferredRenderingExample();
        
        virtual void Init();
        virtual void Update();
        virtual void Shutdown();
        virtual void OnResize(const uint32 &width, const uint32 &height);
        
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

        std::vector<GameObject*> Cubes;
        std::vector<RenderingComponent*> rCubes;

        // Material
        CustomShaderMaterial* Diffuse;

        // Deferred Settings
        Texture* albedoTexture, *specularTexture, *normalTexture, *positionTexture, *depthTexture;
        FrameBuffer* deferredFBO;

};

#endif  /* DEFERREDRENDERINGEXAMPLE_H */

