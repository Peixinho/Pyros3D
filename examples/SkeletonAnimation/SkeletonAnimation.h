//============================================================================
// Name        : SekeletoNanimation.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef SKELETONANIMATION_H
#define	SKELETONANIMATION_H

#ifdef _SDL
    #include "../WindowManagers/SDL/SDLContext.h"
#define ClassName SDLContext
#else
    #include "../WindowManagers/SFML/SFMLContext.h"
    #define ClassName SFMLContext
#endif

#include "Pyros3D/SceneGraph/SceneGraph.h"
#include "Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h"
#include "Pyros3D/Utils/Colors/Colors.h"
#include "Pyros3D/Rendering/Components/Rendering/RenderingComponent.h"
#include "Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h"
#include "Pyros3D/Rendering/Components/Rendering/RenderingComponent.h"
#include "Pyros3D/AnimationManager/AnimationManager.h"

using namespace p3d;

class SkeletonAnimation : public ClassName {

    public:
        
        SkeletonAnimation();   
        virtual ~SkeletonAnimation();
        
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
        GameObject* Model;
        // Rendering Component
        RenderingComponent* rModel;
        // Animation
        AnimationManager* Animation;

        double currentTime; 

};

#endif	/* SKELETONANIMATION_H */

