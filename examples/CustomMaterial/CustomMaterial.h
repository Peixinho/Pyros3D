//============================================================================
// Name        : RotatingCube.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef ROTATINGCUBE_H
#define	ROTATINGCUBE_H

#ifdef _SDL2
    #include "../WindowManagers/SDL2/SDL2Context.h"
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
#include "Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h"

using namespace p3d;

class CustomMaterial : public CustomShaderMaterial {
    
        public:
            
            CustomMaterial() : CustomShaderMaterial("../../../../examples/CustomMaterial/vertex.vert","../../../../examples/CustomMaterial/fragment.frag")
            {
                AddUniform(Uniform::Uniform("uProjectionMatrix",Uniform::DataUsage::ProjectionMatrix));
                AddUniform(Uniform::Uniform("uViewMatrix",Uniform::DataUsage::ViewMatrix));
                AddUniform(Uniform::Uniform("uModelMatrix",Uniform::DataUsage::ModelMatrix));
                AddUniform(Uniform::Uniform("uColor",Uniform::DataUsage::Other,Uniform::DataType::Vec4));
            }
            
            virtual void PreRender() 
            {
                srand( time( NULL ) );
                Vec4 color = Vec4((rand() % 100)/100.f,(rand() % 100)/100.f,(rand() % 100)/100.f,1.f);
                SetUniformValue("uColor",&color);
            }
            virtual void Render() {}
            virtual void AfterRender() {}
            
};

class RotatingCube : public ClassName {
        
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
        // GameObject
        GameObject* Cube;
        // Rendering Component
        RenderingComponent* rCube;
        // Custom Material
        CustomMaterial* Material;

};

#endif	/* ROTATINGCUBE_H */

