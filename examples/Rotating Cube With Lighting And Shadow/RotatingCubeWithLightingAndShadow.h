//============================================================================
// Name        : RotatingCubeWithLightingAndShadow.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube With Light And Shadow Example
//============================================================================

#ifndef ROTATINGCUBEWITHLIGHTANDSHADOW_H
#define	ROTATINGCUBEWITHLIGHTANDSHADOW_H

#include "Pyros3D/Utils/Context/SFML/SFMLContext.h"
#include "Pyros3D/SceneGraph/SceneGraph.h"
#include "Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h"
#include "Pyros3D/Utils/Colors/Colors.h"
#include "Pyros3D/Rendering/Components/Rendering/RenderingComponent.h"
#include "Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h"
#include "Pyros3D/Rendering/Components/Rendering/RenderingComponent.h"
#include "Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h"

using namespace p3d;

class RotatingCubeWithLightingAndShadow : public SFMLContext {
    public:
        
        RotatingCubeWithLightingAndShadow();   
        virtual ~RotatingCubeWithLightingAndShadow();
        
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
        // Cube GameObject
        GameObject* Cube;
        // Cube Model Handle
        uint32 cubeID;
        // Floor Model Handle
        uint32 floorHandle;
        // Floor GameObject
        GameObject* Floor;
        // Floor Rendering Component
        RenderingComponent* rFloor;
        // Floor Material
        GenericShaderMaterial* FloorMaterial;
        // Rendering Component
        RenderingComponent* rCube;
        // Material
        GenericShaderMaterial* Diffuse;

};

#endif	/* ROTATINGCUBEWITHLIGHTANDSHADOW_H */