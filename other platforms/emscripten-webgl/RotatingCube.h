//============================================================================
// Name        : RotatingCube.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef ROTATINGCUBE_H
#define	ROTATINGCUBE_H

#include "src/SDL/SDLContext.h"
#define ClassName SDLContext

#include "../../src/Pyros3D/SceneGraph/SceneGraph.h"
#include "../../src/Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h"
#include "../../src/Pyros3D/Utils/Colors/Colors.h"
#include "../../src/Pyros3D/Rendering/Components/Rendering/RenderingComponent.h"
#include "../../src/Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h"
#include "../../src/Pyros3D/Rendering/Components/Rendering/RenderingComponent.h"

using namespace p3d;

class RotatingCube : public ClassName
{
        
    public:
        
        RotatingCube();   
        virtual ~RotatingCube();
        
        virtual void Init();
        virtual void Update();
        virtual void Shutdown();
        virtual void OnResize(const uint32 &width, const uint32 &height);
        
    private:

        // Scene
        SceneGraph* Scene;
        // Renderer
        ForwardRenderer* Renderer;
        // Projection
        Projection projection;
        // Camera - Its a regular GameObject
        GameObject* Camera;
        // Light
        GameObject* Light;
        DirectionalLight* dLight;
        // GameObject
        GameObject* Cube;
        // Rendering Component
        RenderingComponent* rCube;
        // Material
        GenericShaderMaterial* Diffuse;
        // Material
        GenericShaderMaterial* material;
};

#endif	/* ROTATINGCUBE_H */
