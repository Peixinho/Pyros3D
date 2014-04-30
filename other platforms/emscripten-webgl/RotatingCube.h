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

// date_default_timezone_set('Europe/Lisbon');

// WebGL Hello World

// Here is my first try on WebGL

// Since I've heard about WebGL that I've wanted to try to do something with it, but my main focus has been C++ and OpenGL. I'm working on my little game engine for some time now, and I haven't had the try webgl and js.

// Then I've seen that <a href="https://github.com/kripken/emscripten" target="blank">esmcripten</a> is here for people like me, that likes to develop in c++ and then it compiles the c++ code, do its magic and output JS.

// So here is my first atempt on using <a href="https://github.com/kripken/emscripten" target="blank">esmcripten</a> with part of my engine to build a simple rotating cube.
