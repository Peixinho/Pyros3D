//============================================================================
// Name        : SekeletoNanimation.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef SKELETONANIMATIONEXAMPLE_H
#define	SKELETONANIMATIONEXAMPLE_H

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

#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>

using namespace p3d;

class SkeletonAnimationExample : public ClassName {

    public:
        
        SkeletonAnimationExample();   
        virtual ~SkeletonAnimationExample();
        
        virtual void Init();
        virtual void Update();
        virtual void Shutdown();
        virtual void OnResize(const uint32 width, const uint32 height);
        
        void OnMousePress(Event::Input::Info e);
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
        GameObject* ModelObject;
        // Rendering Component
        RenderingComponent* rModel;
        // Model handle
        Renderable* modelHandle;
        // Animation
        SkeletonAnimationInstance* anim;

        SkeletonAnimation* SAnim;

};

#endif	/* SKELETONANIMATIONEXAMPLE_H */

