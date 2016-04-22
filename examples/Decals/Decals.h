//============================================================================
// Name        : Decals.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef DECALSEXAMPLE_H
#define	DECALSEXAMPLE_H

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
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Decals/Decals.h>
#include <Pyros3D/Utils/Mouse3D/Mouse3D.h>

using namespace p3d;

class Decals : public ClassName
{
        
    public:
        
        Decals();   
        virtual ~Decals();
        
        virtual void Init();
        virtual void Update();
        virtual void Shutdown();
        virtual void OnResize(const uint32 width, const uint32 height);
        
		void OnMouseRelease(Event::Input::Info e);
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
        GameObject* CubeObject, *SphereObject, *ModelObject;
        // Rendering Component
        RenderingComponent* rCube, *rSphere, *rModel;
        // Mesh
		Renderable* cubeMesh, *sphereMesh, *modelMesh;

		// Events
		void MoveFrontPress(Event::Input::Info e);
		void MoveBackPress(Event::Input::Info e);
		void StrafeLeftPress(Event::Input::Info e);
		void StrafeRightPress(Event::Input::Info e);
		void MoveFrontRelease(Event::Input::Info e);
		void MoveBackRelease(Event::Input::Info e);
		void StrafeLeftRelease(Event::Input::Info e);
		void StrafeRightRelease(Event::Input::Info e);
		void LookTo(Event::Input::Info e);

		GenericShaderMaterial* decalMaterial;
		bool GetIntersectedTriangle(RenderingComponent* rcomp, Mouse3D mouse, Vec3* intersection, Vec3* normal, uint32* meshID);
		void CreateDecal();
		std::vector<DecalGeometry*> decals;
		std::vector<RenderingComponent*> rdecals;

		float counterX, counterY;
		Vec2 mouseCenter, mouseLastPosition, mousePosition;
		bool _moveFront, _moveBack, _strafeLeft, _strafeRight;
};

#endif	/* DECALSEXAMPLE */

