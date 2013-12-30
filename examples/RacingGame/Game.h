//============================================================================
// Name        : Game.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#ifndef GAME_H
#define	GAME_H

#include "Pyros3D/Utils/Context/SFML/SFMLContext.h"
#include "Pyros3D/Core/Projection/Projection.h"
#include "Pyros3D/SceneGraph/SceneGraph.h"
#include "Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h"
#include "Pyros3D/Utils/Colors/Colors.h"
#include "Pyros3D/Rendering/Components/Rendering/RenderingComponent.h"
#include "Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h"
#include "Pyros3D/Physics/Physics.h"
#include "Pyros3D/Utils/Mouse3D/PainterPick.h"
#include "Pyros3D/AssetManager/Assets/Renderable/Text/Text.h"
#include "Pyros3D/Rendering/Renderer/SpecialRenderers/CubemapRenderer/CubemapRenderer.h"

using namespace p3d;

class Game : public SFMLContext {
    public:
        
        Game();   
        virtual ~Game();
        
        virtual void Init();
        virtual void Update();
        virtual void Shutdown();
        virtual void OnResize(const uint32 &width, const uint32 &height);
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
        uint32 trackHandle;
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

#endif	/* GAME_H */

