//============================================================================
// Name        : SimplePhysics.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Simple Physics Example
//============================================================================

#ifndef SIMPLEPHYSICS_H
#define	SIMPLEPHYSICS_H

#ifdef _SDL
    #include "Pyros3D/Utils/Context/SDL/SDLContext.h"
#define ClassName SDLContext
#else
    #include "Pyros3D/Utils/Context/SFML/SFMLContext.h"
    #define ClassName SFMLContext
#endif

#include "Pyros3D/Core/Projection/Projection.h"
#include "Pyros3D/SceneGraph/SceneGraph.h"
#include "Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h"
#include "Pyros3D/Rendering/Components/Rendering/RenderingComponent.h"
#include "Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h"
#include "Pyros3D/Physics/Physics.h"
#include "Pyros3D/Physics/Components/IPhysicsComponent.h"

using namespace p3d;

class SimplePhysics : public ClassName {

    public:
        
        SimplePhysics();   
        virtual ~SimplePhysics();
        
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
        std::vector<IPhysicsComponent*> pCubes;
        // Light
        GameObject* Light;
        DirectionalLight* dLight;
	
        // Floor GameObject
        GameObject* Floor;
        // Floor Rendering Component
        RenderingComponent* rFloor;
        // Floor Physics Component
        IPhysicsComponent* pFloor;
        
        // Physics Method
        Physics* physics;
        
        // Material for Selected Mesh
        GenericShaderMaterial *Diffuse;
        
        // Selected Mesh
        RenderingMesh* SelectedMesh;
};

#endif	/* SIMPLEPHYSICS_H */
