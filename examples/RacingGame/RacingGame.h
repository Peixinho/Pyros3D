//============================================================================
// Name        : RacingGame.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : RacingGame Example
//============================================================================

#ifndef RACINGGAME_H
#define	RACINGGAME_H

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

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Physics/Physics.h>
#include <Pyros3D/Utils/Mouse3D/PainterPick.h>
#include <Pyros3D/Assets/Renderable/Text/Text.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/CubemapRenderer/CubemapRenderer.h>
#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Physics/Components/IPhysicsComponent.h>
#include <Pyros3D/Physics/Components/TriangleMesh/PhysicsTriangleMesh.h>
#include <Pyros3D/Utils/ModelLoaders/MultiModelLoader/ModelLoader.h>

using namespace p3d;

class RacingGame : public ClassName {
    public:
        
        RacingGame();   
        virtual ~RacingGame();
        
        virtual void Init();
        virtual void Update();
        virtual void Shutdown();
        virtual void OnResize(const uint32 width, const uint32 height);
    private:

        // Scene
        SceneGraph* Scene, *Scene2;
        // Renderer
        ForwardRenderer* Renderer;
        // Projection
        Projection projection, projection2;
        // Physics
        Physics* physics;
        // Camera - Its a regular GameObject
        GameObject* Camera, *Camera2;
        // GameObject
        GameObject* Track;
        // Track Handle
        Renderable *trackHandle, *skyboxHandle, *carHandle, *carHandle2;
        // Track Component
        RenderingComponent* rTrack;
        // Light
        GameObject* Light;
        // Light Component
        DirectionalLight* dLight;
        
        // GameObject
        GameObject* TextRendering;
        // Model Handle
        Text* textID;
        // Rendering Component
        RenderingComponent* rText;
        // Font
        uint32 font;
        // Text Material
        GenericShaderMaterial* textMaterial;
        GenericShaderMaterial* test;

        GameObject* Car, *Car2;
        RenderingComponent* rCar;
        
        GameObject* Skybox;
        RenderingComponent* rSkybox;
        GenericShaderMaterial* SkyboxMaterial;
    
        void MoveFrontPress(Event::Input::Info e);
        void MoveBackPress(Event::Input::Info e);
        void StrafeLeftPress(Event::Input::Info e);
        void StrafeRightPress(Event::Input::Info e);
        void MoveFrontRelease(Event::Input::Info e);
        void MoveBackRelease(Event::Input::Info e);
        void StrafeLeftRelease(Event::Input::Info e);
        void StrafeRightRelease(Event::Input::Info e);            
        void LookTo(Event::Input::Info e);
        void OnMouseRelease(Event::Input::Info e);
        void CloseApp(Event::Input::Info e);
        float counterX, counterY;
        Vec2 mouseCenter, mouseLastPosition, mousePosition;
        bool _moveFront, _moveBack, _strafeLeft, _strafeRight;
    
        GenericShaderMaterial *envMaterial;
        CubemapRenderer* dRenderer;
};

#endif	/* RACINGGAME_H */

