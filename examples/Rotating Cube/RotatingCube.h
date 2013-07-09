//============================================================================
// Name        : RotatingCube.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef ROTATINGCUBE_H
#define	ROTATINGCUBE_H

#include "Pyros3D/Utils/Context/SFML/SFMLContext.h"
#include "Pyros3D/SceneGraph/SceneGraph.h"
#include "Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h"
#include "Pyros3D/Utils/Colors/Colors.h"
#include "Pyros3D/Rendering/Components/Rendering/RenderingComponent.h"

using namespace p3d;

class RotatingCube : public SFMLContext {
    public:
        
        RotatingCube();   
        virtual ~RotatingCube();
        
        virtual void Init();
        virtual void Update();
        virtual void Shutdown();
        
    private:

        // Scene
        SceneGraph* Scene;
        // Renderer
        ForwardRenderer* Renderer;
        // Projection
        Matrix Projection;
        // Camera - Its a regular GameObject
        GameObject* Camera;
        // GameObject
        GameObject* Cube;
        // Model Handle
        uint32 cubeID;
        // Rendering Component
        RenderingComponent* rCube;

};

#endif	/* ROTATINGCUBE_H */

