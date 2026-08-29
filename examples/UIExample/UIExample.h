//============================================================================
// Name        : UIExample.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Screen-space UI: a UICanvas over a normal 3D scene, built
//                from UIRect / UIImage / UIText and drawn by UIRenderer.
//                Also the acceptance test for that system - run with
//                PYROS_UI_VERIFY=1 to render one frame into an FBO, assert
//                pixels and exit, which is how this gets checked on a
//                machine whose screen cannot be captured.
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

	// Builds one element: a GameObject carrying a UIRect plus whatever
	// element component the caller adds afterwards.
	std::shared_ptr<GameObject> MakeElement(const std::shared_ptr<GameObject> &parent,
		const std::string &name, const Vec2 &anchorMin, const Vec2 &anchorMax,
		const Vec2 &offsetMin, const Vec2 &offsetMax, const Vec2 &pivot);

	void BuildCanvas();
	void RunVerification();

	Projection projection;

	UIRenderer* uiRenderer;

	std::shared_ptr<GameObject> cubeObj;
	std::shared_ptr<RenderingComponent> rCube;
	std::shared_ptr<Renderable> cubeMesh;
	std::shared_ptr<GenericShaderMaterial> material;

	std::shared_ptr<GameObject> lightObj;
	std::shared_ptr<DirectionalLight> dirLight;

	std::shared_ptr<Font> font;

	std::shared_ptr<GameObject> canvasObj;
	std::shared_ptr<UICanvas> canvas;
	// Kept so Shutdown() can detach them, and so Update() can drive the
	// health bar and the readout.
	std::vector<std::shared_ptr<GameObject> > elements;
	std::vector<std::shared_ptr<IComponent> > components;
	std::shared_ptr<UIRect> healthFillRect;
	std::shared_ptr<UIText> readout;
	std::shared_ptr<UIImage> slicedImage;
	std::shared_ptr<Texture> slicedTexture;

	bool verifyMode;
	bool verified;
};

#endif	/* UIEXAMPLE_H */
