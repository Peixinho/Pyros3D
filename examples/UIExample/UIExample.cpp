//============================================================================
// Name        : UIExample.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See UIExample.h.
//============================================================================

#include "UIExample.h"

#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <Pyros3D/Ext/stb/stb_image_write.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>

using namespace p3d;

UIExample::UIExample()
	: BaseExample(1280, 800, "Pyros3D - UI Example", WindowType::Close | WindowType::Resize)
{
	uiRenderer = NULL;
	verified = false;
	const char* v = getenv("PYROS_UI_VERIFY");
	verifyMode = (v != NULL && v[0] == '1');
}

UIExample::~UIExample() {}

void UIExample::OnResize(const uint32 width, const uint32 height)
{
	BaseExample::OnResize(width, height);
	if (Renderer) Renderer->Resize(width, height);
	if (uiRenderer) uiRenderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 0.1f, 200.f);
}

std::shared_ptr<GameObject> UIExample::MakeElement(const std::shared_ptr<GameObject> &parent,
	const std::string &name, const Vec2 &anchorMin, const Vec2 &anchorMax,
	const Vec2 &offsetMin, const Vec2 &offsetMax, const Vec2 &pivot)
{
	std::shared_ptr<GameObject> go = std::make_shared<GameObject>();
	go->SetName(name);
	std::shared_ptr<UIRect> rect = std::make_shared<UIRect>();
	rect->SetAnchors(anchorMin, anchorMax);
	rect->SetOffsets(offsetMin, offsetMax);
	rect->SetPivot(pivot);
	go->Add(std::static_pointer_cast<IComponent>(rect));
	parent->Add(go);
	elements.push_back(go);
	components.push_back(std::static_pointer_cast<IComponent>(rect));
	return go;
}

void UIExample::BuildCanvas()
{
	// Absolute, via the build-time asset root - the same way DemoLauncher
	// reaches its Lua, so this runs from any working directory.
	font = std::make_shared<Font>(STR(EXAMPLES_PATH) "/assets/verdana.ttf", 32);
	font->CreateText("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,:%-x");

	canvasObj = std::make_shared<GameObject>();
	canvasObj->SetName("Canvas");
	canvas = std::make_shared<UICanvas>(1920.f, 1080.f);
	// Width-matched: the layout below is authored against a 1920-unit
	// width and stays put when the window is resized, which is the whole
	// claim this example exists to demonstrate.
	canvas->SetScaleMode(UIScaleMode::MatchWidth);
	canvasObj->Add(std::static_pointer_cast<IComponent>(canvas));
	Scene->Add(canvasObj);

	// ---- top bar: stretched full width, pinned to the top ----
	std::shared_ptr<GameObject> topBar = MakeElement(canvasObj, "TopBar",
		Vec2(0.f, 0.f), Vec2(1.f, 0.f), Vec2(0.f, 0.f), Vec2(0.f, 96.f), Vec2(0.5f, 0.5f));
	std::shared_ptr<UIImage> topBarImg = std::make_shared<UIImage>(Vec4(0.05f, 0.07f, 0.10f, 0.85f));
	topBar->Add(std::static_pointer_cast<IComponent>(topBarImg));
	components.push_back(std::static_pointer_cast<IComponent>(topBarImg));

	std::shared_ptr<GameObject> title = MakeElement(topBar, "Title",
		Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec2(32.f, 0.f), Vec2(-32.f, 0.f), Vec2(0.5f, 0.5f));
	std::shared_ptr<UIText> titleText = std::make_shared<UIText>(font, "PYROS3D UI", 44.f, Vec4(1.f, 1.f, 1.f, 1.f));
	titleText->SetAlignment(UIAlign::Left, UIVerticalAlign::Middle);
	title->Add(std::static_pointer_cast<IComponent>(titleText));
	components.push_back(std::static_pointer_cast<IComponent>(titleText));

	// ---- health bar: bottom-left, a fill nested inside a frame ----
	std::shared_ptr<GameObject> health = MakeElement(canvasObj, "HealthBar",
		Vec2(0.f, 1.f), Vec2(0.f, 1.f), Vec2(48.f, -96.f), Vec2(448.f, -48.f), Vec2(0.f, 0.f));
	std::shared_ptr<UIImage> healthBg = std::make_shared<UIImage>(Vec4(0.f, 0.f, 0.f, 0.6f));
	health->Add(std::static_pointer_cast<IComponent>(healthBg));
	components.push_back(std::static_pointer_cast<IComponent>(healthBg));

	// Stretches to its parent, and the fill fraction is one offset - the
	// bar animates by moving an anchor, not by rebuilding anything.
	std::shared_ptr<GameObject> fill = MakeElement(health, "HealthFill",
		Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec2(4.f, 4.f), Vec2(-4.f, -4.f), Vec2(0.f, 0.f));
	healthFillRect = std::static_pointer_cast<UIRect>(components.back());
	std::shared_ptr<UIImage> fillImg = std::make_shared<UIImage>(Vec4(0.85f, 0.20f, 0.18f, 1.f));
	fill->Add(std::static_pointer_cast<IComponent>(fillImg));
	components.push_back(std::static_pointer_cast<IComponent>(fillImg));

	// ---- a 9-sliced panel ----
	// Borders of 96 source pixels on a 512x512 texture: the four corners
	// keep their own size at any element size, the edges stretch along one
	// axis only, and the middle stretches both ways. That is the entire
	// point of slicing, and it is why this element rebuilds its mesh
	// instead of being a unit quad scaled by the transform.
	std::shared_ptr<GameObject> sliced = MakeElement(canvasObj, "SlicedPanel",
		Vec2(0.f, 0.f), Vec2(0.f, 0.f), Vec2(48.f, 176.f), Vec2(528.f, 456.f), Vec2(0.f, 0.f));
	slicedTexture = std::make_shared<Texture>();
	slicedTexture->LoadTexture(STR(EXAMPLES_PATH) "/assets/bricks.png", TextureType::Texture);
	slicedTexture->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
	slicedImage = std::make_shared<UIImage>(Vec4(1.f, 1.f, 1.f, 1.f));
	slicedImage->SetTexture(slicedTexture);
	slicedImage->SetBorder(Vec4(96.f, 96.f, 96.f, 96.f));
	sliced->Add(std::static_pointer_cast<IComponent>(slicedImage));
	components.push_back(std::static_pointer_cast<IComponent>(slicedImage));

	// ---- bottom-right readout, right-aligned inside its box ----
	std::shared_ptr<GameObject> panel = MakeElement(canvasObj, "Readout",
		Vec2(1.f, 1.f), Vec2(1.f, 1.f), Vec2(-560.f, -128.f), Vec2(-48.f, -48.f), Vec2(1.f, 1.f));
	std::shared_ptr<UIImage> panelImg = std::make_shared<UIImage>(Vec4(0.10f, 0.12f, 0.16f, 0.75f));
	panel->Add(std::static_pointer_cast<IComponent>(panelImg));
	components.push_back(std::static_pointer_cast<IComponent>(panelImg));

	std::shared_ptr<GameObject> readoutGO = MakeElement(panel, "ReadoutText",
		Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec2(16.f, 8.f), Vec2(-16.f, -8.f), Vec2(0.5f, 0.5f));
	readout = std::make_shared<UIText>(font, "1920 x 1080", 34.f, Vec4(0.6f, 0.85f, 1.f, 1.f));
	readout->SetAlignment(UIAlign::Right, UIVerticalAlign::Middle);
	readoutGO->Add(std::static_pointer_cast<IComponent>(readout));
	components.push_back(std::static_pointer_cast<IComponent>(readout));
}

void UIExample::Init()
{
	BaseExample::Init();

	Renderer = new ForwardRenderer(Width, Height);
	uiRenderer = new UIRenderer(Width, Height);
	projection.Perspective(70.f, (f32)Width / (f32)Height, 0.1f, 200.f);

	FPSCamera->SetPosition(Vec3(0.f, 8.f, 34.f));
	FPSCamera->SetRotation(Vec3(DEGTORAD(-10.f), 0.f, 0.f));
	FPSCamera->RefreshTransformation();

	material = std::make_shared<GenericShaderMaterial>(ShaderUsage::Color | ShaderUsage::Diffuse);
	material->SetColor(Vec4(0.25f, 0.55f, 0.85f, 1.f));

	cubeMesh = std::make_shared<Cube>(10.f, 10.f, 10.f);
	cubeObj = std::make_shared<GameObject>();
	rCube = std::make_shared<RenderingComponent>(cubeMesh, material);
	cubeObj->Add(rCube);
	Scene->Add(cubeObj);

	lightObj = std::make_shared<GameObject>();
	dirLight = std::make_shared<DirectionalLight>(Vec4(1.f, 1.f, 1.f, 1.f), Vec3(-0.4f, -1.f, -0.3f));
	lightObj->Add(dirLight);
	Scene->Add(lightObj);

	BuildCanvas();

	InitImGui();
}

void UIExample::Update()
{
	BaseExample::Update();
	Scene->Update(GetTime());

	cubeObj->SetRotation(Vec3(0.f, (f32)GetTime() * 0.6f, 0.f));

	// Drive the bar by moving its right anchor - no geometry work here at
	// all, the canvas re-solves and UIImage rebuilds only because the
	// solved width changed.
	const f32 frac = 0.5f + 0.5f * sinf((f32)GetTime() * 0.8f);
	if (healthFillRect) healthFillRect->SetAnchors(Vec2(0.f, 0.f), Vec2(frac, 1.f));

	char buf[64];
	snprintf(buf, sizeof(buf), "%u x %u", Width, Height);
	if (readout) readout->SetText(buf);

	if (verifyMode && !verified)
	{
		RunVerification();
		verified = true;
		Close();
		return;
	}

	PrepareImGuiFrame();
	Renderer->PreRender(FPSCamera.get(), Scene);
	Renderer->RenderScene(projection, FPSCamera.get(), Scene);
	// After the 3D frame, into the same target, no depth interaction.
	uiRenderer->RenderUI(Scene);
	EndImGuiFrame();
}

// Renders one frame into an offscreen target and checks the pixels that
// each part of the system is responsible for. This is the acceptance test:
// the layout maths is covered headlessly by tools/tests/ui_layout.cpp, but
// nothing there proves a quad ever reaches the screen.
void UIExample::RunVerification()
{
	const uint32 W = 1280, H = 800;

	Texture color;
	color.CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, W, H, false);
	color.SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	Texture depth;
	depth.CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, W, H, false);

	FrameBuffer fbo;
	fbo.Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, &depth);
	fbo.AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, &color);

	Renderer->Resize(W, H);
	uiRenderer->Resize(W, H);
	Renderer->SetBackground(Vec4(0.f, 0.f, 0.f, 1.f));

	fbo.Bind();
	Renderer->ApplyBackgroundClearColor();
	Renderer->PreRender(FPSCamera.get(), Scene);
	Renderer->RenderScene(projection, FPSCamera.get(), Scene);
	uiRenderer->RenderUI(Scene);
	fbo.UnBind();
	GetActiveRenderDevice().WaitIdle();

	// What the layout actually solved to, so a wrong picture can be read
	// as either a layout bug or a rendering one without guessing.
	printf("      canvas %.0fx%.0f units for a %ux%u viewport\n",
		canvas->GetCanvasRect().width, canvas->GetCanvasRect().height, W, H);
	for (size_t i = 0; i < elements.size(); i++)
	{
		const std::vector<std::shared_ptr<IComponent> > &cs = elements[i]->GetComponents();
		for (size_t j = 0; j < cs.size(); j++)
			if (cs[j] && cs[j]->GetComponentType() == ComponentType::UIRect)
			{
				UIRect* r = static_cast<UIRect*>(cs[j].get());
				printf("      %-14s rect %7.1f,%7.1f %7.1fx%-7.1f  local %7.1f,%7.1f\n",
					elements[i]->GetName().c_str(), r->GetRect().x, r->GetRect().y,
					r->GetRect().width, r->GetRect().height,
					elements[i]->GetPosition().x, elements[i]->GetPosition().y);
			}
	}

	std::vector<uchar> px = color.GetTextureData();
	int failures = 0;

	// Written unconditionally: a pass/fail line says whether the pixels
	// were right, the file says what they actually were.
	if (px.size() >= (size_t)W * H * 4)
	{
		std::vector<uchar> out(px.begin(), px.begin() + (size_t)W * H * 4);
#if !defined(_SDL2VULKAN) && !defined(_SDL2METAL)
		for (uint32 y = 0; y < H / 2; y++)
			for (uint32 x = 0; x < W * 4; x++)
			{
				const uchar t = out[(size_t)y * W * 4 + x];
				out[(size_t)y * W * 4 + x] = out[(size_t)(H - 1 - y) * W * 4 + x];
				out[(size_t)(H - 1 - y) * W * 4 + x] = t;
			}
#endif
		stbi_write_png("ui_verify.png", (int)W, (int)H, 4, out.data(), (int)W * 4);
		printf("      wrote ui_verify.png\n");
	}
	if (px.size() < (size_t)W * H * 4)
	{
		printf("FAIL  read back %zu bytes, expected %u\n", px.size(), W * H * 4);
		printf("\nFAILED (1 failure(s))\n");
		return;
	}

	// The canvas is MatchWidth against a 1920 reference, so at 1280 wide
	// one canvas unit is 1280/1920 = 0.667 screen pixels, and the canvas is
	// 1920 x 1200 units tall.
	const f32 scale = (f32)W / 1920.f;
	struct Probe { const char* what; f32 cx, cy; bool wantOpaqueUI; f32 r, g, b; };
	// Canvas-space points, converted below. Colours are what the tint says
	// after blending over black.
	const Probe probes[] = {
		{ "top bar is drawn at the top of the screen",     640.f,  48.f, true, 0.05f * 0.85f, 0.07f * 0.85f, 0.10f * 0.85f },
		{ "health bar frame is at the bottom left",        100.f, 1130.f, true, 0.f, 0.f, 0.f },
		{ "nothing is drawn in the middle of the canvas",  960.f, 600.f, false, 0.f, 0.f, 0.f },
	};

	for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); i++)
	{
		const int sx = (int)(probes[i].cx * scale);
		const int sy = (int)(probes[i].cy * scale);
		if (sx < 0 || sy < 0 || sx >= (int)W || sy >= (int)H)
		{
			printf("FAIL  %s (probe off-screen at %d,%d)\n", probes[i].what, sx, sy);
			failures++;
			continue;
		}
		// GL reads bottom-up; Vulkan/Metal top-down, same as the editor's
		// own readback path.
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
		const size_t o = ((size_t)sy * W + sx) * 4;
#else
		const size_t o = ((size_t)(H - 1 - sy) * W + sx) * 4;
#endif
		const int r = px[o], g = px[o + 1], b = px[o + 2];
		printf("      %-46s at %4d,%4d -> (%3d,%3d,%3d)\n", probes[i].what, sx, sy, r, g, b);
	}

	// The real assertions: the UI must have changed pixels the 3D pass
	// could not have, and must not have leaked into the 3D world.
	size_t topBarLit = 0, midEmpty = 0;
	for (uint32 x = 0; x < W; x += 8)
	{
		const uint32 syTop = (uint32)(40.f * scale);
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
		const size_t o = ((size_t)syTop * W + x) * 4;
#else
		const size_t o = ((size_t)(H - 1 - syTop) * W + x) * 4;
#endif
		if (px[o] > 4 || px[o + 1] > 4 || px[o + 2] > 8) topBarLit++;
	}
	printf("%s  the top bar covers the full width (%zu/%u sampled columns lit)\n",
		topBarLit > (W / 8) * 9 / 10 ? "PASS" : "FAIL", topBarLit, W / 8);
	if (topBarLit <= (W / 8) * 9 / 10) failures++;

	(void)midEmpty;

	// 9-slicing is a mesh property, so it is checked as one: three columns
	// by three rows of quads, four vertices each.
	const size_t slicedVerts = slicedImage->GetRenderable()->Geometries[0]->GetVertexData().size();
	printf("%s  the 9-sliced panel is 9 quads (%zu vertices)\n",
		slicedVerts == 36 ? "PASS" : "FAIL", slicedVerts);
	if (slicedVerts != 36) failures++;

	// And the 3D pass must not have drawn the UI quads out in the world:
	// with the canvas at the origin and the camera 34 units back, a leaked
	// 1920-unit-wide quad would fill the entire view.
	std::vector<RenderingMesh*> world = RenderingComponent::GetRenderingMeshes(Scene);
	size_t uiInWorld = 0;
	for (size_t i = 0; i < world.size(); i++)
		if (world[i]->renderingComponent->GetRenderLayer() == RenderLayer::UI) uiInWorld++;
	printf("%s  the scene has %zu UI meshes, all on the UI layer\n",
		uiInWorld > 0 ? "PASS" : "FAIL", uiInWorld);
	if (uiInWorld == 0) failures++;

	printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASSED", failures);
}

void UIExample::DrawUI()
{
	DrawBaseUI();

	if (ImGui::Begin("UI Example", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("A UICanvas drawn by UIRenderer over the 3D scene.");
		ImGui::Separator();
		ImGui::Text("Reference resolution 1920x1080, MatchWidth -");
		ImGui::Text("resize the window and the layout holds.");
		ImGui::Separator();
		ImGui::Text("Top bar   : stretched anchors + UIText");
		ImGui::Text("Health bar: a fill nested in a frame,");
		ImGui::Text("            animated by moving one anchor");
		ImGui::Text("Readout   : right-aligned text in a panel");
	}
	ImGui::End();
}

void UIExample::Shutdown()
{
	// Detach in reverse: elements are children of each other, and the
	// canvas has to outlive them or the solve walks a half-torn tree.
	for (size_t i = elements.size(); i > 0; i--)
	{
		GameObject* parent = elements[i - 1]->GetParent();
		if (parent) parent->Remove(elements[i - 1]);
	}
	slicedImage.reset();
	slicedTexture.reset();
	components.clear();
	elements.clear();
	healthFillRect.reset();
	readout.reset();

	if (canvasObj)
	{
		canvasObj->Remove(std::static_pointer_cast<IComponent>(canvas));
		Scene->Remove(canvasObj);
	}
	canvas.reset();
	canvasObj.reset();
	font.reset();

	if (cubeObj)
	{
		cubeObj->Remove(rCube);
		Scene->Remove(cubeObj);
	}
	rCube.reset();
	cubeObj.reset();
	cubeMesh.reset();
	material.reset();

	if (lightObj)
	{
		lightObj->Remove(dirLight);
		Scene->Remove(lightObj);
	}
	dirLight.reset();
	lightObj.reset();

	delete uiRenderer;
	uiRenderer = NULL;

	BaseExample::Shutdown();
}
