//============================================================================
// Name        : TextRendering.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Text Rendering Example
//============================================================================

#ifndef TEXTRENDERING_H
#define	TEXTRENDERING_H

#ifdef _SDL2
    #include "../WindowManagers/SDL2/SDL2Context.h"
#define ClassName SDLContext
#else
    #include "../WindowManagers/SFML/SFMLContext.h"
    #define ClassName SFMLContext
#endif

#include "Pyros3D/SceneGraph/SceneGraph.h"
#include "Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h"
#include "Pyros3D/Rendering/Components/Rendering/RenderingComponent.h"
#include "Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h"
#include "Pyros3D/AssetManager/Assets/Font/Font.h"

using namespace p3d;

class TextRendering : public ClassName {

    public:
        
        TextRendering();   
        virtual ~TextRendering();
        
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
        GameObject* Text;
        // Rendering Component
        RenderingComponent* rText;
        // Font
        Font* font;
        // Text Material
        GenericShaderMaterial* textMaterial;

};

#endif	/* TEXTRENDERING_H */

