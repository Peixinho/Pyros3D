//============================================================================
// Name        : TextRendering.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Text Rendering Example
//============================================================================

#ifndef TEXTRENDERING_H
#define	TEXTRENDERING_H

#define _STR(path) #path
#define STR(path) _STR(path)

// Context selection (ClassName + the right window-manager header) is
// handled once, correctly, by BaseExample.h - this file used to carry its
// own duplicate copy of that logic that never learned about
// _SDL2VULKAN and silently fell through to SFML for any context it didn't
// recognize (i.e. every Vulkan build), forcing a hard SFML dependency this
// example never otherwise needed.
#include "../BaseExample/BaseExample.h"
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Assets/Renderable/Text/Text.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Assets/Font/Font.h>

using namespace p3d;

class TextRendering : public BaseExample {

public:

	TextRendering();
	virtual ~TextRendering();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void DrawUI();

private:

	// Scene, Renderer and projection are inherited (protected) from
	// BaseExample - redeclaring them here shadowed the base class's
	// members, so BaseExample::Init() initialized BaseExample::Scene while
	// every later unqualified `Scene` in this class resolved to its own,
	// never-initialized shadow copy.

	// Text Camera
	GameObject* textCamera;
	// GameObject
	GameObject* TextObject;
	// Rendering Component
	RenderingComponent* rText;
	// Text Mesh
	Text* textHandle;
	// Font
	Font* font;
	// Text Material
	GenericShaderMaterial* textMaterial;

};

#endif	/* TEXTRENDERING_H */

