//============================================================================
// Name        : UIExample.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A pause screen and a HUD, built from UICanvas / UIRect /
//                UIImage / UIText and drawn by UIRenderer over a 3D scene.
//
//                All of the art is generated at startup (see the Bake*
//                helpers): rounded panels, soft shadows and gradients are
//                small procedural textures used as 9-slices, which is what
//                lets a UI made of quads stop looking like a UI made of
//                quads without shipping a single image file.
//
//                Run with PYROS_UI_VERIFY=1 to render one frame into an
//                FBO, print every solved rect, write ui_verify.png and
//                assert pixels, then exit.
//============================================================================

#ifndef UIEXAMPLE_H
#define	UIEXAMPLE_H

#include "../BaseExample/BaseExample.h"

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/UIRenderer/UIRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/UI/UICanvas.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Rendering/Components/UI/UIImage.h>
#include <Pyros3D/Rendering/Components/UI/UIText.h>
#include <memory>
#include <string>
#include <vector>

using namespace p3d;

class UIExample : public BaseExample {

public:

	UIExample();
	virtual ~UIExample();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void DrawUI();

private:

	// ---- element construction -------------------------------------------
	std::shared_ptr<GameObject> Element(const std::shared_ptr<GameObject> &parent,
		const std::string &name, const Vec2 &anchorMin, const Vec2 &anchorMax,
		const Vec2 &offsetMin, const Vec2 &offsetMax, const Vec2 &pivot = Vec2(0.5f, 0.5f));
	std::shared_ptr<UIImage> Image(const std::shared_ptr<GameObject> &on, const Vec4 &tint,
		const std::shared_ptr<Texture> &texture = std::shared_ptr<Texture>(),
		const Vec4 &border = Vec4(0.f, 0.f, 0.f, 0.f));
	std::shared_ptr<UIText> Label(const std::shared_ptr<GameObject> &on,
		const std::shared_ptr<Font> &font, const std::string &text, const f32 size,
		const Vec4 &color, const uint32 h, const uint32 v);
	static UIRect* RectOf(const std::shared_ptr<GameObject> &go);
	static UIRect* FindRect(const std::shared_ptr<GameObject> &root, const std::string &name);

	void BakeTextures();
	void BuildBackdrop();
	void BuildHud();
	void BuildMenu();
	void SetSelectedRow(const int32 row);
	void RunVerification();
	// PYROS_UI_BENCH=<element count>: times RenderUI() on a canvas of that
	// many elements, offscreen so nothing waits on vsync.
	void RunBench(const int elements);

	Projection projection;
	UIRenderer* uiRenderer;

	// Backdrop
	std::shared_ptr<Renderable> blockMesh;
	std::shared_ptr<GenericShaderMaterial> blockMaterial;
	std::vector<std::shared_ptr<GameObject> > blocks;
	std::vector<std::shared_ptr<RenderingComponent> > blockComponents;
	std::shared_ptr<GameObject> blockRoot;
	std::shared_ptr<GameObject> lightObj;
	std::shared_ptr<DirectionalLight> dirLight;

	// Generated art
	std::shared_ptr<Texture> texPanel;   // rounded rect, lighter rim
	std::shared_ptr<Texture> texShadow;  // blurred rounded rect
	std::shared_ptr<Texture> texPill;    // small-radius rounded rect
	std::shared_ptr<Texture> texRamp;    // horizontal gradient

	std::shared_ptr<Font> fontTitle, fontBody, fontSmall;

	// Canvases
	std::shared_ptr<GameObject> hudObj, menuObj;
	std::shared_ptr<UICanvas> hudCanvas, menuCanvas;

	// Keeps ownership so Shutdown() can take the tree apart in one place.
	std::vector<std::shared_ptr<GameObject> > elements;
	std::vector<std::shared_ptr<IComponent> > components;

	// Animated bits
	std::shared_ptr<UIRect> armourFill;
	std::shared_ptr<UIText> armourValue;
	std::vector<std::shared_ptr<UIImage> > rowBackgrounds;
	std::vector<std::shared_ptr<UIText> > rowLabels;
	std::vector<std::shared_ptr<UIText> > rowHints;
	int32 selectedRow;

	bool verifyMode;
	int benchElements;
	bool verified;
};

#endif	/* UIEXAMPLE_H */
