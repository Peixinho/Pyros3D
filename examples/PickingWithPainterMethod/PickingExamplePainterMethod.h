//============================================================================
// Name        : PickingExamplePainterMethod.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : PickingExample With Painter Method
//============================================================================

#ifndef PICKINGPAINTERMETHOD_H
#define	PICKINGPAINTERMETHOD_H

#ifdef _SDL2
    #include "../WindowManagers/SDL2/SDL2Context.h"
#define ClassName SDLContext
#else
    #include "../WindowManagers/SFML/SFMLContext.h"
    #define ClassName SFMLContext
#endif

#include "Pyros3D/Core/Projection/Projection.h"
#include "Pyros3D/SceneGraph/SceneGraph.h"
#include "Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h"
#include "Pyros3D/Rendering/Components/Rendering/RenderingComponent.h"
#include "Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h"
#include "Pyros3D/Utils/Mouse3D/PainterPick.h"

using namespace p3d;

class PickingExamplePainterMethod : public ClassName {

    public:
        
        PickingExamplePainterMethod();
        virtual ~PickingExamplePainterMethod();
        
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
        // GameObject
        std::vector<GameObject*> Cubes;
        std::vector<RenderingComponent*> rCubes;

        // Light
        GameObject* Light;
        DirectionalLight* dLight;
        
        // Event On Mouse Release
        void OnMouseRelease(Event::Input::Info e);   
        
        // Painter Method
        PainterPick* picking;
        
        // Material for Selected Mesh
        GenericShaderMaterial *UnselectedMaterial, *SelectedMaterial;
        
        // Selected Mesh
        RenderingMesh* SelectedMesh;
};

#endif	/* PICKINGPAINTERMETHOD_H */
