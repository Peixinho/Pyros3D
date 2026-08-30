//============================================================================
// Name        : UIExample.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See UIExample.h.
//============================================================================

#include "UIExample.h"

#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <Pyros3D/Ext/stb/stb_image_write.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>

using namespace p3d;

//=============================================================================
// Palette
//=============================================================================
static const Vec4 kInk       (0.03f, 0.04f, 0.06f, 1.00f);
static const Vec4 kCard      (0.10f, 0.12f, 0.17f, 0.98f);
static const Vec4 kRowIdle   (1.00f, 1.00f, 1.00f, 0.05f);
static const Vec4 kAccent    (0.22f, 0.74f, 0.98f, 1.00f);
static const Vec4 kTextHi    (0.96f, 0.97f, 1.00f, 1.00f);
static const Vec4 kTextMid   (0.62f, 0.68f, 0.80f, 1.00f);
static const Vec4 kTextDim   (0.40f, 0.46f, 0.58f, 1.00f);

//=============================================================================
// Procedural art
//
// Everything the UI is drawn with is baked here, at startup, into small
// textures used as 9-slices. A rounded corner is not something a quad can
// have, and a soft shadow is not something a solid tint can be - so without
// these the whole system can only ever produce flat rectangles.
//=============================================================================

static std::shared_ptr<Texture> UploadRGBA(const uint32 w, const uint32 h, std::vector<uchar> &rgba)
{
	std::shared_ptr<Texture> t = std::make_shared<Texture>();
	t->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, (int32)w, (int32)h, false);
	t->UpdateData(rgba.data());
	t->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
	t->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	return t;
}

// Signed distance to a rounded rectangle: negative inside, and it is the
// real distance, which is what makes both antialiasing and the shadow's
// falloff below a one-liner.
static f32 RoundRectSDF(const f32 px, const f32 py, const f32 w, const f32 h, const f32 r)
{
	const f32 qx = fabsf(px - w * 0.5f) - (w * 0.5f - r);
	const f32 qy = fabsf(py - h * 0.5f) - (h * 0.5f - r);
	const f32 ax = qx > 0.f ? qx : 0.f;
	const f32 ay = qy > 0.f ? qy : 0.f;
	const f32 inner = (qx > qy ? qx : qy);
	return sqrtf(ax * ax + ay * ay) + (inner < 0.f ? inner : 0.f) - r;
}

static f32 Saturate(const f32 v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

// Pure white, with the shape entirely in the alpha channel. Deliberately
// flat: the tint is multiplied by this, so any shading baked into the RGB
// silently darkens every colour an author picks. A first version shaded the
// face to 0.62 to fake a lit rim, and every panel in the menu came out 38%
// darker than the palette said it was. Edges are separated by stacking two
// of these instead (see the card), which keeps the tint honest.
static std::shared_ptr<Texture> BakeRoundedRect(const uint32 size, const f32 radius)
{
	std::vector<uchar> px((size_t)size * size * 4, 0);
	for (uint32 y = 0; y < size; y++)
		for (uint32 x = 0; x < size; x++)
		{
			const f32 d = RoundRectSDF((f32)x + 0.5f, (f32)y + 0.5f, (f32)size, (f32)size, radius);
			const size_t o = ((size_t)y * size + x) * 4;
			px[o + 0] = px[o + 1] = px[o + 2] = 255;
			// 1px of coverage antialiasing, straight off the distance.
			px[o + 3] = (uchar)(Saturate(0.5f - d) * 255.f);
		}
	return UploadRGBA(size, size, px);
}

static std::shared_ptr<Texture> BakeShadow(const uint32 size, const f32 radius, const f32 blur)
{
	std::vector<uchar> px((size_t)size * size * 4, 0);
	for (uint32 y = 0; y < size; y++)
		for (uint32 x = 0; x < size; x++)
		{
			const f32 d = RoundRectSDF((f32)x + 0.5f, (f32)y + 0.5f, (f32)size, (f32)size, radius);
			const f32 a = Saturate(-d / blur);
			const size_t o = ((size_t)y * size + x) * 4;
			px[o + 0] = px[o + 1] = px[o + 2] = 255;
			px[o + 3] = (uchar)(a * a * 255.f);
		}
	return UploadRGBA(size, size, px);
}

// A 1-pixel-tall strip, so a plain stretched quad becomes a left-to-right
// gradient with no shader and no vertex colours.
static std::shared_ptr<Texture> BakeRamp(const uint32 width, const Vec4 &a, const Vec4 &b)
{
	std::vector<uchar> px((size_t)width * 4, 0);
	for (uint32 x = 0; x < width; x++)
	{
		const f32 t = width > 1 ? (f32)x / (f32)(width - 1) : 0.f;
		px[x * 4 + 0] = (uchar)(Saturate(a.x + (b.x - a.x) * t) * 255.f);
		px[x * 4 + 1] = (uchar)(Saturate(a.y + (b.y - a.y) * t) * 255.f);
		px[x * 4 + 2] = (uchar)(Saturate(a.z + (b.z - a.z) * t) * 255.f);
		px[x * 4 + 3] = (uchar)(Saturate(a.w + (b.w - a.w) * t) * 255.f);
	}
	return UploadRGBA(width, 1, px);
}

//=============================================================================

UIExample::UIExample()
	: BaseExample(1600, 900, "Pyros3D - UI Example", WindowType::Close | WindowType::Resize)
{
	uiRenderer = NULL;
	selectedRow = 0;
	verified = false;
	const char* v = getenv("PYROS_UI_VERIFY");
	verifyMode = (v != NULL && v[0] == '1');
	const char* b = getenv("PYROS_UI_BENCH");
	benchElements = b ? atoi(b) : 0;
}

UIExample::~UIExample() {}

void UIExample::OnResize(const uint32 width, const uint32 height)
{
	BaseExample::OnResize(width, height);
	if (Renderer) Renderer->Resize(width, height);
	if (uiRenderer) uiRenderer->Resize(width, height);
	projection.Perspective(58.f, (f32)width / (f32)height, 0.1f, 400.f);
}

//=============================================================================
// Element construction
//=============================================================================

std::shared_ptr<GameObject> UIExample::Element(const std::shared_ptr<GameObject> &parent,
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

// Depth-first by name. Only used by the verification below, where the
// point is to compare the same element across two separately built trees.
UIRect* UIExample::FindRect(const std::shared_ptr<GameObject> &root, const std::string &name)
{
	if (!root) return NULL;
	if (root->GetName() == name) return RectOf(root);
	const std::vector<std::shared_ptr<GameObject> > &kids = root->GetChildren();
	for (size_t i = 0; i < kids.size(); i++)
		if (UIRect* r = FindRect(kids[i], name)) return r;
	return NULL;
}

UIRect* UIExample::RectOf(const std::shared_ptr<GameObject> &go)
{
	const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
	for (size_t i = 0; i < cs.size(); i++)
		if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
			return static_cast<UIRect*>(cs[i].get());
	return NULL;
}

std::shared_ptr<UIImage> UIExample::Image(const std::shared_ptr<GameObject> &on, const Vec4 &tint,
	const std::shared_ptr<Texture> &texture, const Vec4 &border)
{
	std::shared_ptr<UIImage> img = std::make_shared<UIImage>(tint);
	if (texture) img->SetTexture(texture);
	img->SetBorder(border);
	on->Add(std::static_pointer_cast<IComponent>(img));
	components.push_back(std::static_pointer_cast<IComponent>(img));
	return img;
}

std::shared_ptr<UIText> UIExample::Label(const std::shared_ptr<GameObject> &on,
	const std::shared_ptr<Font> &font, const std::string &text, const f32 size,
	const Vec4 &color, const uint32 h, const uint32 v)
{
	std::shared_ptr<UIText> label = std::make_shared<UIText>(font, text, size, color);
	label->SetAlignment(h, v);
	on->Add(std::static_pointer_cast<IComponent>(label));
	components.push_back(std::static_pointer_cast<IComponent>(label));
	return label;
}

//=============================================================================

void UIExample::BakeTextures()
{
	// 24-unit corner on a 64px texture, sliced with a 28-unit border - so
	// the corners keep their curvature at any element size and only the
	// straight edges stretch.
	texPanel = BakeRoundedRect(64, 24.f);
	texPill = BakeRoundedRect(48, 18.f);
	texShadow = BakeShadow(128, 34.f, 30.f);
	texRamp = BakeRamp(64, Vec4(0.15f, 0.62f, 0.92f, 1.f), Vec4(0.45f, 0.92f, 0.95f, 1.f));

	const std::string glyphs =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,:;-_/%+()[]'\"!?";
	fontTitle = std::make_shared<Font>(STR(EXAMPLES_PATH) "/assets/verdana.ttf", 72);
	fontTitle->CreateText(glyphs);
	fontBody = std::make_shared<Font>(STR(EXAMPLES_PATH) "/assets/verdana.ttf", 30);
	fontBody->CreateText(glyphs);
	fontSmall = std::make_shared<Font>(STR(EXAMPLES_PATH) "/assets/verdana.ttf", 20);
	fontSmall->CreateText(glyphs);
}

void UIExample::BuildBackdrop()
{
	// A block field, so there is something with depth and shading behind
	// the menu instead of one flat shape on black.
	blockMaterial = std::make_shared<GenericShaderMaterial>(ShaderUsage::Color | ShaderUsage::Diffuse);
	blockMaterial->SetColor(Vec4(0.16f, 0.20f, 0.30f, 1.f));
	blockMesh = std::make_shared<Cube>(1.f, 1.f, 1.f);

	blockRoot = std::make_shared<GameObject>();
	Scene->Add(blockRoot);

	for (int32 gz = -4; gz <= 4; gz++)
		for (int32 gx = -4; gx <= 4; gx++)
		{
			// Deterministic pseudo-random heights - the same field every
			// run, so the verification pixels below are stable.
			const f32 n = fabsf(sinf((f32)gx * 12.9898f + (f32)gz * 78.233f) * 43758.5453f);
			const f32 height = 2.f + (n - floorf(n)) * 16.f;
			std::shared_ptr<GameObject> b = std::make_shared<GameObject>();
			b->SetPosition(Vec3((f32)gx * 7.f, height * 0.5f, (f32)gz * 7.f));
			b->SetScale(Vec3(4.4f, height, 4.4f));
			std::shared_ptr<RenderingComponent> rc = std::make_shared<RenderingComponent>(blockMesh, blockMaterial);
			b->Add(rc);
			blockRoot->Add(b);
			blocks.push_back(b);
			blockComponents.push_back(rc);
		}

	lightObj = std::make_shared<GameObject>();
	dirLight = std::make_shared<DirectionalLight>(Vec4(1.f, 0.94f, 0.86f, 1.f), Vec3(-0.5f, -1.f, -0.35f));
	lightObj->Add(dirLight);
	Scene->Add(lightObj);
}

void UIExample::BuildHud()
{
	hudObj = std::make_shared<GameObject>();
	hudObj->SetName("HudCanvas");
	hudCanvas = std::make_shared<UICanvas>(1920.f, 1080.f);
	hudCanvas->SetScaleMode(UIScaleMode::MatchWidth);
	hudCanvas->SetSortOrder(0);
	hudObj->Add(std::static_pointer_cast<IComponent>(hudCanvas));
	Scene->Add(hudObj);

	// ---- armour readout, top left ----
	std::shared_ptr<GameObject> group = Element(hudObj, "Armour",
		Vec2(0.f, 0.f), Vec2(0.f, 0.f), Vec2(56.f, 48.f), Vec2(516.f, 152.f), Vec2(0.f, 0.f));

	std::shared_ptr<GameObject> caption = Element(group, "ArmourCaption",
		Vec2(0.f, 0.f), Vec2(1.f, 0.f), Vec2(0.f, 0.f), Vec2(0.f, 30.f), Vec2(0.f, 0.f));
	Label(caption, fontSmall, "ARMOUR", 20.f, kTextDim, UIAlign::Left, UIVerticalAlign::Top);
	armourValue = Label(caption, fontSmall, "84%", 20.f, kAccent, UIAlign::Right, UIVerticalAlign::Top);

	std::shared_ptr<GameObject> track = Element(group, "ArmourTrack",
		Vec2(0.f, 1.f), Vec2(1.f, 1.f), Vec2(0.f, -34.f), Vec2(0.f, 0.f), Vec2(0.f, 1.f));
	Image(track, Vec4(0.f, 0.f, 0.f, 0.55f), texPill, Vec4(20.f, 20.f, 20.f, 20.f));

	std::shared_ptr<GameObject> fill = Element(track, "ArmourFill",
		Vec2(0.f, 0.f), Vec2(0.84f, 1.f), Vec2(4.f, 4.f), Vec2(-4.f, -4.f), Vec2(0.f, 0.f));
	armourFill = std::static_pointer_cast<UIRect>(components.back());
	// The ramp is a plain stretched quad, not a 9-slice - a gradient's
	// whole job is to stretch.
	Image(fill, Vec4(1.f, 1.f, 1.f, 1.f), texRamp);

	// ---- score, top right ----
	std::shared_ptr<GameObject> score = Element(hudObj, "Score",
		Vec2(1.f, 0.f), Vec2(1.f, 0.f), Vec2(-516.f, 48.f), Vec2(-56.f, 152.f), Vec2(1.f, 0.f));
	std::shared_ptr<GameObject> scoreCaption = Element(score, "ScoreCaption",
		Vec2(0.f, 0.f), Vec2(1.f, 0.f), Vec2(0.f, 0.f), Vec2(0.f, 30.f), Vec2(1.f, 0.f));
	Label(scoreCaption, fontSmall, "WAVE 07", 20.f, kTextDim, UIAlign::Right, UIVerticalAlign::Top);
	std::shared_ptr<GameObject> scoreValue = Element(score, "ScoreValue",
		Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec2(0.f, 34.f), Vec2(0.f, 0.f), Vec2(1.f, 0.f));
	Label(scoreValue, fontBody, "128,400", 46.f, kTextHi, UIAlign::Right, UIVerticalAlign::Top);
}

void UIExample::BuildMenu()
{
	menuObj = std::make_shared<GameObject>();
	menuObj->SetName("MenuCanvas");
	menuCanvas = std::make_shared<UICanvas>(1920.f, 1080.f);
	menuCanvas->SetScaleMode(UIScaleMode::MatchWidth);
	// Above the HUD - one number, rather than a hierarchy edit.
	menuCanvas->SetSortOrder(10);
	menuObj->Add(std::static_pointer_cast<IComponent>(menuCanvas));
	Scene->Add(menuObj);

	// ---- scrim over everything ----
	std::shared_ptr<GameObject> scrim = Element(menuObj, "Scrim",
		Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec2(0.f, 0.f), Vec2(0.f, 0.f));
	Image(scrim, Vec4(kInk.x, kInk.y, kInk.z, 0.66f));

	// ---- card, centred, with its shadow behind it ----
	// The shadow is a sibling drawn first rather than part of the card,
	// because draw order here is hierarchy order: earlier siblings paint
	// first, so this is all the "z-index" a UI needs.
	std::shared_ptr<GameObject> shadow = Element(menuObj, "CardShadow",
		Vec2(0.5f, 0.5f), Vec2(0.5f, 0.5f), Vec2(-400.f, -400.f), Vec2(400.f, 432.f));
	Image(shadow, Vec4(0.f, 0.f, 0.f, 0.55f), texShadow, Vec4(60.f, 60.f, 60.f, 60.f));

	// Two stacked rounded rects: a faint light ring, and the real face
	// inset by 2 units inside it. That is the border - a real element with
	// a real colour, rather than shading baked into the art.
	std::shared_ptr<GameObject> cardEdge = Element(menuObj, "CardEdge",
		Vec2(0.5f, 0.5f), Vec2(0.5f, 0.5f), Vec2(-360.f, -368.f), Vec2(360.f, 368.f));
	Image(cardEdge, Vec4(1.f, 1.f, 1.f, 0.14f), texPanel, Vec4(28.f, 28.f, 28.f, 28.f));

	std::shared_ptr<GameObject> card = Element(cardEdge, "Card",
		Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec2(2.f, 2.f), Vec2(-2.f, -2.f), Vec2(0.f, 0.f));
	Image(card, kCard, texPanel, Vec4(28.f, 28.f, 28.f, 28.f));

	// ---- header ----
	std::shared_ptr<GameObject> eyebrow = Element(card, "Eyebrow",
		Vec2(0.f, 0.f), Vec2(1.f, 0.f), Vec2(64.f, 64.f), Vec2(-64.f, 92.f), Vec2(0.f, 0.f));
	Label(eyebrow, fontSmall, "PAUSED", 20.f, kAccent, UIAlign::Center, UIVerticalAlign::Middle);

	std::shared_ptr<GameObject> title = Element(card, "Title",
		Vec2(0.f, 0.f), Vec2(1.f, 0.f), Vec2(64.f, 96.f), Vec2(-64.f, 196.f), Vec2(0.f, 0.f));
	Label(title, fontTitle, "PYROS3D", 76.f, kTextHi, UIAlign::Center, UIVerticalAlign::Middle);

	std::shared_ptr<GameObject> rule = Element(card, "Rule",
		Vec2(0.5f, 0.f), Vec2(0.5f, 0.f), Vec2(-56.f, 208.f), Vec2(56.f, 212.f), Vec2(0.f, 0.f));
	Image(rule, kAccent);

	// ---- rows ----
	const char* names[] = { "Resume", "Loadout", "Settings", "Quit to Menu" };
	const char* hints[] = { "esc", "L", "S", "" };
	const f32 rowTop = 264.f, rowHeight = 76.f, rowGap = 16.f;

	for (int32 i = 0; i < 4; i++)
	{
		const f32 y = rowTop + (rowHeight + rowGap) * (f32)i;
		std::shared_ptr<GameObject> row = Element(card, std::string("Row") + names[i],
			Vec2(0.f, 0.f), Vec2(1.f, 0.f), Vec2(48.f, y), Vec2(-48.f, y + rowHeight), Vec2(0.f, 0.f));
		rowBackgrounds.push_back(Image(row, kRowIdle, texPill, Vec4(18.f, 18.f, 18.f, 18.f)));

		std::shared_ptr<GameObject> inner = Element(row, std::string("RowInner") + names[i],
			Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec2(28.f, 0.f), Vec2(-28.f, 0.f), Vec2(0.f, 0.f));
		rowLabels.push_back(Label(inner, fontBody, names[i], 34.f, kTextMid, UIAlign::Left, UIVerticalAlign::Middle));
		rowHints.push_back(Label(inner, fontSmall, hints[i], 20.f, kTextDim, UIAlign::Right, UIVerticalAlign::Middle));
	}

	// ---- footer ----
	std::shared_ptr<GameObject> footer = Element(card, "Footer",
		Vec2(0.f, 1.f), Vec2(1.f, 1.f), Vec2(64.f, -76.f), Vec2(-64.f, -32.f), Vec2(0.f, 0.f));
	Label(footer, fontSmall, "UICanvas  .  UIRect  .  UIImage  .  UIText", 20.f, kTextDim,
		UIAlign::Center, UIVerticalAlign::Middle);

	SetSelectedRow(0);
}

// Hover/press/selected states are the thing a styling layer will own later;
// for now the demo drives the same three properties by hand, which is a
// useful check that they are the right three.
void UIExample::SetSelectedRow(const int32 row)
{
	selectedRow = row;
	for (size_t i = 0; i < rowBackgrounds.size(); i++)
	{
		const bool on = ((int32)i == row);
		rowBackgrounds[i]->SetTint(on ? kAccent : kRowIdle);
		rowLabels[i]->SetColor(on ? Vec4(0.04f, 0.08f, 0.12f, 1.f) : kTextMid);
		rowHints[i]->SetColor(on ? Vec4(0.06f, 0.20f, 0.29f, 1.f) : kTextDim);
	}
}

void UIExample::Init()
{
	BaseExample::Init();

	Renderer = new ForwardRenderer(Width, Height);
	Renderer->SetBackground(Vec4(0.05f, 0.06f, 0.09f, 1.f));
	Renderer->SetGlobalLight(Vec4(0.18f, 0.20f, 0.26f, 1.f));
	uiRenderer = new UIRenderer(Width, Height);
	projection.Perspective(58.f, (f32)Width / (f32)Height, 0.1f, 400.f);

	FPSCamera->SetPosition(Vec3(0.f, 26.f, 52.f));
	FPSCamera->SetRotation(Vec3(DEGTORAD(-20.f), 0.f, 0.f));
	FPSCamera->RefreshTransformation();

	BakeTextures();
	BuildBackdrop();
	BuildHud();
	BuildMenu();

	InitImGui();
}

void UIExample::Update()
{
	BaseExample::Update();
	Scene->Update(GetTime());

	// Slow orbit, so the backdrop is alive without pulling the eye.
	if (blockRoot) blockRoot->SetRotation(Vec3(0.f, (f32)GetTime() * 0.08f, 0.f));

	const f32 t = (f32)GetTime();
	const f32 armour = 0.55f + 0.42f * (0.5f + 0.5f * sinf(t * 0.7f));
	if (armourFill) armourFill->SetAnchors(Vec2(0.f, 0.f), Vec2(armour, 1.f));
	if (armourValue)
	{
		char buf[16];
		snprintf(buf, sizeof(buf), "%d%%", (int)(armour * 100.f + 0.5f));
		armourValue->SetText(buf);
	}

	// Walk the selection, so the state change is visible without input.
	// Pinned under verification, where "which row is highlighted" would
	// otherwise depend on how long startup happened to take.
	const int32 want = verifyMode ? 0 : ((int32)(t * 0.5f)) % 4;
	if (want != selectedRow) SetSelectedRow(want);

	if (benchElements > 0 && !verified)
	{
		RunBench(benchElements);
		verified = true;
		Close();
		return;
	}

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

//=============================================================================
// Acceptance test
//
// The layout maths is covered headlessly by tools/tests/ui_layout.cpp; this
// is the half that proves quads and glyphs actually reach a target, on a
// machine whose screen cannot be captured.
//=============================================================================
//=============================================================================
// Cost of a UI frame
//
// One element is one material and one draw call today. This measures what
// that costs before any batching work, offscreen and without a present so
// nothing is capped at the display's refresh rate.
//=============================================================================
void UIExample::RunBench(const int elements)
{
	const uint32 W = 1600, H = 900;

	Texture color;
	color.CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, W, H, false);
	color.SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	Texture depth;
	depth.CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, W, H, false);
	FrameBuffer fbo;
	fbo.Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, &depth);
	fbo.AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, &color);

	SceneGraph bench;
	std::shared_ptr<GameObject> canvasGO = std::make_shared<GameObject>();
	std::shared_ptr<UICanvas> canvas = std::make_shared<UICanvas>((f32)W, (f32)H);
	canvas->SetScaleMode(UIScaleMode::ConstantPixel);
	canvasGO->Add(std::static_pointer_cast<IComponent>(canvas));
	bench.Add(canvasGO);

	// A grid of elements over the whole canvas, cycling through three
	// textures and a label every eighth - close enough to a busy HUD that
	// the mix of state changes is representative.
	const int cols = 32;
	const f32 cw = (f32)W / (f32)cols;
	const int rows = (elements + cols - 1) / cols;
	const f32 ch = (f32)H / (f32)(rows > 0 ? rows : 1);
	std::shared_ptr<Texture> tex[3] = { texPanel, texPill, UIImage::WhiteTexture() };
	std::shared_ptr<Font> benchFont = std::make_shared<Font>(STR(EXAMPLES_PATH) "/assets/verdana.ttf", 14);
	benchFont->CreateText("0123456789 HUD");

	int images = 0, labels = 0;
	for (int i = 0; i < elements; i++)
	{
		std::shared_ptr<GameObject> go = std::make_shared<GameObject>();
		std::shared_ptr<UIRect> r = std::make_shared<UIRect>();
		r->SetAnchors(Vec2(0.f, 0.f), Vec2(0.f, 0.f));
		r->SetPivot(Vec2(0.f, 0.f));
		r->SetOffsets(Vec2((i % cols) * cw, (i / cols) * ch),
			Vec2((i % cols) * cw + cw - 2.f, (i / cols) * ch + ch - 2.f));
		go->Add(std::static_pointer_cast<IComponent>(r));

		if (i % 8 == 7)
		{
			std::shared_ptr<UIText> t = std::make_shared<UIText>(benchFont, "HUD 42", 14.f, Vec4(1, 1, 1, 1));
			go->Add(std::static_pointer_cast<IComponent>(t));
			labels++;
		}
		else
		{
			std::shared_ptr<UIImage> img = std::make_shared<UIImage>(Vec4(0.2f, 0.5f, 0.9f, 0.8f));
			img->SetTexture(tex[i % 3]);
			if (i % 3 == 0) img->SetBorder(Vec4(24, 24, 24, 24));
			go->Add(std::static_pointer_cast<IComponent>(img));
			images++;
		}
		canvasGO->Add(go);
	}
	bench.Update(0.0);

	// PYROS_UI_BENCH_NOBATCH=1 measures the same frame with every element
	// on its own, which is the number the batched one is worth comparing to.
	canvas->SetBatching(getenv("PYROS_UI_BENCH_NOBATCH") == NULL);

	UIRenderer benchRenderer(W, H);
	benchRenderer.Resize(W, H);

	const int warmup = 20, iters = 200;
	f64 total = 0.0;
	for (int f = 0; f < warmup + iters; f++)
	{
		const f64 t0 = (f64)clock() / (f64)CLOCKS_PER_SEC;
		fbo.Bind();
		benchRenderer.RenderUI(&bench);
		fbo.UnBind();
		GetActiveRenderDevice().WaitIdle();
		const f64 dt = (f64)clock() / (f64)CLOCKS_PER_SEC - t0;
		if (f >= warmup) total += dt;
	}

	const size_t draws = canvas->GetBatchedDrawList().size();
	printf("      %d elements (%d images, %d labels): %d draws (%d merged), %.3f ms/frame\n",
		elements, images, labels, (int)draws, (int)canvas->GetBatchCount(), total * 1000.0 / iters);

	bench.Remove(canvasGO);
}

void UIExample::RunVerification()
{
	const uint32 W = 1600, H = 900;

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
	projection.Perspective(58.f, (f32)W / (f32)H, 0.1f, 400.f);

	fbo.Bind();
	Renderer->ApplyBackgroundClearColor();
	Renderer->PreRender(FPSCamera.get(), Scene);
	Renderer->RenderScene(projection, FPSCamera.get(), Scene);
	uiRenderer->RenderUI(Scene);
	fbo.UnBind();
	GetActiveRenderDevice().WaitIdle();

	printf("      hud canvas  %.0fx%.0f units, menu canvas %.0fx%.0f, viewport %ux%u\n",
		hudCanvas->GetCanvasRect().width, hudCanvas->GetCanvasRect().height,
		menuCanvas->GetCanvasRect().width, menuCanvas->GetCanvasRect().height, W, H);

	std::vector<uchar> px = color.GetTextureData();
	int failures = 0;
	if (px.size() < (size_t)W * H * 4)
	{
		printf("FAIL  read back %zu bytes, expected %u\n\nFAILED (1 failure(s))\n", px.size(), W * H * 4);
		return;
	}

	{
		std::vector<uchar> out(px.begin(), px.begin() + (size_t)W * H * 4);
#if !defined(_SDL2VULKAN) && !defined(_SDL2METAL)
		for (uint32 y = 0; y < H / 2; y++)
			for (uint32 x = 0; x < W * 4; x++)
			{
				const uchar tmp = out[(size_t)y * W * 4 + x];
				out[(size_t)y * W * 4 + x] = out[(size_t)(H - 1 - y) * W * 4 + x];
				out[(size_t)(H - 1 - y) * W * 4 + x] = tmp;
			}
#endif
		stbi_write_png("ui_verify.png", (int)W, (int)H, 4, out.data(), (int)W * 4);
		printf("      wrote ui_verify.png\n");
	}

	// Canvas units -> readback pixels.
	const f32 s = (f32)W / 1920.f;
	struct Px { int r, g, b; };
	struct Sampler {
		const std::vector<uchar>* px; uint32 W, H; f32 s;
		Px At(const f32 cx, const f32 cy) const {
			int x = (int)(cx * s), y = (int)(cy * s);
			if (x < 0) x = 0; if (y < 0) y = 0;
			if (x >= (int)W) x = (int)W - 1; if (y >= (int)H) y = (int)H - 1;
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
			const size_t o = ((size_t)y * W + x) * 4;
#else
			const size_t o = ((size_t)(H - 1 - y) * W + x) * 4;
#endif
			Px p; p.r = (*px)[o]; p.g = (*px)[o + 1]; p.b = (*px)[o + 2]; return p;
		}
	} S{ &px, W, H, s };

	// Expected colours are computed from the palette, not hardcoded, so
	// changing the design cannot quietly turn these into tautologies.
	struct Expect { const char* what; f32 cx, cy; Vec4 want; };
	const Expect exact[] = {
		// Card face: opaque tint over the scrim, so essentially the tint.
		{ "the card paints its palette colour", 960.f, 250.f, kCard },
		// The selected row is the accent, filled edge to edge.
		{ "the selected row carries the accent tint", 1150.f, 474.f, kAccent },
	};
	for (size_t i = 0; i < sizeof(exact) / sizeof(exact[0]); i++)
	{
		const Px got = S.At(exact[i].cx, exact[i].cy);
		const int wr = (int)(exact[i].want.x * 255.f + 0.5f);
		const int wg = (int)(exact[i].want.y * 255.f + 0.5f);
		const int wb = (int)(exact[i].want.z * 255.f + 0.5f);
		// Generous, because the tint is composited over whatever is under
		// it at whatever alpha the palette asked for.
		const bool ok = abs(got.r - wr) < 14 && abs(got.g - wg) < 14 && abs(got.b - wb) < 14;
		printf("%s  %-44s got (%3d,%3d,%3d) want (%3d,%3d,%3d)\n",
			ok ? "PASS" : "FAIL", exact[i].what, got.r, got.g, got.b, wr, wg, wb);
		if (!ok) failures++;
	}

	// Rounded corners, checked where it matters: just outside the top-left
	// arc must NOT be card, and the same distance in from the straight edge
	// must be.
	const Px offArc = S.At(606.f, 178.f);
	const Px onEdge = S.At(606.f, 540.f);
	const bool rounded = offArc.b < 20 && onEdge.b > 20;
	printf("%s  the corners are actually round (arc %3d,%3d,%3d / edge %3d,%3d,%3d)\n",
		rounded ? "PASS" : "FAIL", offArc.r, offArc.g, offArc.b, onEdge.r, onEdge.g, onEdge.b);
	if (!rounded) failures++;

	// The armour bar: its gradient must be drawn, and it must be brighter
	// than the empty part of the track right next to it.
	const Px fill = S.At(150.f, 135.f);
	const Px empty = S.At(500.f, 135.f);
	const bool bar = fill.b > fill.r && fill.b > empty.b + 30;
	printf("%s  the armour gradient is drawn over its track (fill %3d,%3d,%3d / empty %3d,%3d,%3d)\n",
		bar ? "PASS" : "FAIL", fill.r, fill.g, fill.b, empty.r, empty.g, empty.b);
	if (!bar) failures++;

	// Canvas sort order, as an observable fact rather than a field read:
	// the menu's full-screen scrim is drawn over the HUD, so the bar's
	// gradient arrives dimmed. Unscrimmed it would be around g=160.
	const bool ordered = fill.g > 20 && fill.g < 110;
	printf("%s  the menu canvas paints over the HUD canvas (bar green %d, unscrimmed would be ~160)\n",
		ordered ? "PASS" : "FAIL", fill.g);
	if (!ordered) failures++;

	// 9-slicing is a mesh property, so it is checked as one.
	size_t sliced = 0, plain = 0;
	for (size_t i = 0; i < components.size(); i++)
		if (components[i] && components[i]->GetComponentType() == ComponentType::UIImage)
		{
			UIImage* img = static_cast<UIImage*>(components[i].get());
			const size_t verts = img->GetRenderable()->Geometries[0]->GetVertexData().size();
			if (verts == 36) sliced++; else if (verts == 4) plain++;
		}
	printf("%s  %zu images are 9-sliced (9 quads) and %zu are plain (1 quad)\n",
		(sliced == 8 && plain == 3) ? "PASS" : "FAIL", sliced, plain);
	if (!(sliced == 8 && plain == 3)) failures++;

	// And the 3D pass must never see any of it.
	std::vector<RenderingMesh*> all = RenderingComponent::GetRenderingMeshes(Scene);
	size_t ui = 0, world = 0;
	for (size_t i = 0; i < all.size(); i++)
		(all[i]->renderingComponent->GetRenderLayer() == RenderLayer::UI ? ui : world)++;
	printf("%s  %zu UI meshes and %zu world meshes, on separate layers\n",
		(ui > 0 && world > 0) ? "PASS" : "FAIL", ui, world);
	if (!(ui > 0 && world > 0)) failures++;

	// ---- restyling, live ----
	// Every visual property is component state, not geometry and not a
	// shader variant, so a skin is something that can be applied to an
	// already-built tree. This is the mechanism a .uistyle asset would sit
	// on top of; checking it here keeps that claim honest.
	{
		UIImage* row = rowBackgrounds[1].get();
		UIText* label = rowLabels[1].get();
		const size_t vertsBefore = row->GetRenderable()->Geometries[0]->GetVertexData().size();
		const f32 widthBefore = static_cast<Text*>(label->GetRenderable())->GetAdvanceWidth();

		row->SetTint(Vec4(0.9f, 0.3f, 0.2f, 1.f));
		row->SetBorder(Vec4(0.f, 0.f, 0.f, 0.f));   // 9-slice off
		row->SetTexture(std::shared_ptr<Texture>()); // back to flat colour
		label->SetSize(52.f);
		label->SetColor(Vec4(1.f, 1.f, 1.f, 1.f));
		label->SetAlignment(UIAlign::Center, UIVerticalAlign::Middle);

		// A restyle has to survive a re-solve, which is what a running game
		// would do on the very next frame.
		menuCanvas->Solve(W, H);

		const size_t vertsAfter = row->GetRenderable()->Geometries[0]->GetVertexData().size();
		const f32 widthAfter = static_cast<Text*>(label->GetRenderable())->GetAdvanceWidth();
		const bool restyled = (vertsBefore == 36 && vertsAfter == 4) && (widthAfter > widthBefore * 1.3f);
		printf("%s  a built element restyles live (%zu verts -> %zu, text %.0f -> %.0f wide)\n",
			restyled ? "PASS" : "FAIL", vertsBefore, vertsAfter, widthBefore, widthAfter);
		if (!restyled) failures++;
	}

	// ---- SDF text ----
	// The claim is that one SDF bake stays sharp at any size, where a
	// coverage bake is only sharp at the size it was baked. Measured as edge
	// sharpness: scan a row across the glyphs and count pixels that are
	// neither background nor solid. A crisp edge spends a pixel or two in
	// transition; a blurry one smears over many.
	//
	// In its own scene and its own canvas, at ConstantPixel scale so canvas
	// units are pixels. The first version of this rendered into the demo's
	// own canvas and dutifully measured the pause menu's scrim.
	{
		const uint32 W2 = 512, H2 = 160;
		Texture c2; c2.CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, W2, H2, false);
		c2.SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		Texture d2; d2.CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, W2, H2, false);
		FrameBuffer fbo2;
		fbo2.Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, &d2);
		fbo2.AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, &c2);

		SceneGraph probeScene;
		std::shared_ptr<GameObject> probeCanvasGO = std::make_shared<GameObject>();
		std::shared_ptr<UICanvas> probeCanvas = std::make_shared<UICanvas>((f32)W2, (f32)H2);
		probeCanvas->SetScaleMode(UIScaleMode::ConstantPixel);
		probeCanvasGO->Add(std::static_pointer_cast<IComponent>(probeCanvas));
		probeScene.Add(probeCanvasGO);

		std::shared_ptr<GameObject> probeGO = std::make_shared<GameObject>();
		std::shared_ptr<UIRect> pr = std::make_shared<UIRect>();
		pr->SetAnchors(Vec2(0.f, 0.f), Vec2(1.f, 1.f));
		pr->SetOffsets(Vec2(0.f, 0.f), Vec2(0.f, 0.f));
		probeGO->Add(std::static_pointer_cast<IComponent>(pr));
		probeCanvasGO->Add(probeGO);
		probeScene.Update(0.0);

		// Both baked small and drawn large - the case an SDF exists for.
		std::shared_ptr<Font> bmp = std::make_shared<Font>(STR(EXAMPLES_PATH) "/assets/verdana.ttf", 16, false);
		std::shared_ptr<Font> sdf = std::make_shared<Font>(STR(EXAMPLES_PATH) "/assets/verdana.ttf", 16, true);
		bmp->CreateText("HI"); sdf->CreateText("HI");

		UIRenderer probeRenderer(W2, H2);

		auto transitionPixels = [&](const std::shared_ptr<Font> &font, const char* tag, int &solidOut) -> int {
			std::shared_ptr<UIText> probe = std::make_shared<UIText>(font, "HI", 110.f, Vec4(1, 1, 1, 1));
			probe->SetAlignment(UIAlign::Center, UIVerticalAlign::Middle);
			probeGO->Add(std::static_pointer_cast<IComponent>(probe));
			probeScene.Update(0.0);

			fbo2.Bind();
			GetActiveRenderDevice().SetClearColor(Vec4(0.f, 0.f, 0.f, 1.f));
			// UIRenderer never clears - it is meant to draw over the world -
			// so the second pass here would otherwise composite onto the
			// first. Vulkan hid that: its render pass clears on bind, while
			// GL keeps whatever the FBO already held, and the SDF row came
			// back measuring the bitmap glyphs underneath it.
			// ClearBufferBit() only records the bits for DrawBackground(),
			// which UIRenderer never runs - the clear has to be asked of the
			// device.
			IRenderDevice &dev = GetActiveRenderDevice();
			dev.Clear(dev.TranslateBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth));
			probeRenderer.Resize(W2, H2);
			probeRenderer.RenderUI(&probeScene);
			fbo2.UnBind();
			GetActiveRenderDevice().WaitIdle();

			std::vector<uchar> px2 = c2.GetTextureData();
			int transitions = 0, solid = 0;
			if (px2.size() >= (size_t)W2 * H2 * 4)
				for (uint32 x = 0; x < W2; x++)
				{
					const uint32 y = H2 / 2;
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
					const size_t o = ((size_t)y * W2 + x) * 4;
#else
					const size_t o = ((size_t)(H2 - 1 - y) * W2 + x) * 4;
#endif
					const int v = px2[o];
					if (v > 24 && v < 230) transitions++;
					else if (v >= 230) solid++;
				}
			printf("      %-6s %3d partly-covered, %3d solid pixels across the row\n", tag, transitions, solid);
			solidOut = solid;
			probeGO->Remove(std::static_pointer_cast<IComponent>(probe));
			return transitions;
		};

		int bmpSolid = 0, sdfSolid = 0;
		const int bmpEdge = transitionPixels(bmp, "bitmap", bmpSolid);
		const int sdfEdge = transitionPixels(sdf, "sdf", sdfSolid);
		// Solid pixels on both sides first: without that, a variant that
		// draws nothing at all has the tightest edge of any of them.
		const bool sharper = (bmpEdge > 0) && (bmpSolid > 0) && (sdfSolid > 0)
			&& (sdfEdge * 2 <= bmpEdge);
		printf("%s  a 16px atlas drawn at 110px: the SDF edge is at least twice as tight\n",
			sharper ? "PASS" : "FAIL");
		if (!sharper) failures++;

		probeCanvasGO->Remove(probeGO);
		probeScene.Remove(probeCanvasGO);
	}

	// ---- batching draws the same pixels ----
	// The whole point is that this is invisible: fewer draw calls, same
	// frame. Renders the demo's own two canvases both ways into the same
	// target and compares. Any difference at all here is a bug in the
	// batcher, not a tolerance to widen.
	{
		std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(Scene);
		std::vector<uchar> before, after;
		uint32 drawsUnbatched = 0, drawsBatched = 0;

		for (int pass = 0; pass < 2; pass++)
		{
			for (size_t c = 0; c < canvases.size(); c++) canvases[c]->SetBatching(pass == 1);

			fbo.Bind();
			IRenderDevice &dev = GetActiveRenderDevice();
			dev.SetClearColor(Vec4(0.f, 0.f, 0.f, 1.f));
			dev.Clear(dev.TranslateBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth));
			uiRenderer->Resize(W, H);
			uiRenderer->RenderUI(Scene);
			fbo.UnBind();
			dev.WaitIdle();

			uint32 draws = 0;
			for (size_t c = 0; c < canvases.size(); c++) draws += (uint32)canvases[c]->GetBatchedDrawList().size();
			(pass == 0 ? drawsUnbatched : drawsBatched) = draws;
			(pass == 0 ? before : after) = color.GetTextureData();
		}

		size_t differing = 0;
		int worst = 0;
		if (before.size() == after.size() && !before.empty())
			for (size_t i = 0; i < before.size(); i++)
			{
				const int d = abs((int)before[i] - (int)after[i]);
				if (d > 0) { differing++; if (d > worst) worst = d; }
			}
		else differing = 1;

		const bool identical = (differing == 0);
		const bool fewer = drawsBatched < drawsUnbatched;
		if (!identical && before.size() == after.size())
		{
			// Something to look at rather than reason about: this failing
			// means elements moved, and where they moved to is the answer.
			stbi_write_png("ui_batch_off.png", (int)W, (int)H, 4, before.data(), (int)W * 4);
			stbi_write_png("ui_batch_on.png", (int)W, (int)H, 4, after.data(), (int)W * 4);
			printf("      wrote ui_batch_off.png / ui_batch_on.png\n");
		}
		printf("      %u draws unbatched -> %u batched, %zu differing channels (worst %d)\n",
			drawsUnbatched, drawsBatched, differing, worst);
		printf("%s  batching draws the same pixels in fewer calls\n",
			(identical && fewer) ? "PASS" : "FAIL");
		if (!(identical && fewer)) failures++;
	}

	// ---- batching never reorders what overlaps ----
	// Three stacked elements: red, then green, then red again, with the
	// two reds sharing a texture. Merging them would draw the second red
	// underneath the green - the one case where "same texture, so batch
	// them" is wrong, and the reason an element may only move earlier past
	// things it does not touch.
	{
		const uint32 W2 = 128, H2 = 128;
		Texture c2; c2.CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, W2, H2, false);
		c2.SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		Texture d2; d2.CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, W2, H2, false);
		FrameBuffer fbo2;
		fbo2.Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, &d2);
		fbo2.AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, &c2);

		SceneGraph probe;
		std::shared_ptr<GameObject> canvasGO = std::make_shared<GameObject>();
		std::shared_ptr<UICanvas> canvas = std::make_shared<UICanvas>((f32)W2, (f32)H2);
		canvas->SetScaleMode(UIScaleMode::ConstantPixel);
		canvasGO->Add(std::static_pointer_cast<IComponent>(canvas));
		probe.Add(canvasGO);

		// texPill for the two reds, texPanel for the green, so "same
		// texture" and "same colour" cannot be confused for one another.
		const Vec4 tints[3] = { Vec4(1,0,0,1), Vec4(0,1,0,1), Vec4(1,0,0,1) };
		for (int i = 0; i < 3; i++)
		{
			std::shared_ptr<GameObject> go = std::make_shared<GameObject>();
			std::shared_ptr<UIRect> r = std::make_shared<UIRect>();
			r->SetAnchors(Vec2(0.f, 0.f), Vec2(0.f, 0.f));
			r->SetPivot(Vec2(0.f, 0.f));
			r->SetOffsets(Vec2(16.f, 16.f), Vec2(112.f, 112.f));
			go->Add(std::static_pointer_cast<IComponent>(r));
			std::shared_ptr<UIImage> img = std::make_shared<UIImage>(tints[i]);
			img->SetTexture(i == 1 ? texPanel : texPill);
			go->Add(std::static_pointer_cast<IComponent>(img));
			canvasGO->Add(go);
		}
		probe.Update(0.0);

		UIRenderer probeRenderer(W2, H2);
		probeRenderer.Resize(W2, H2);
		fbo2.Bind();
		IRenderDevice &dev = GetActiveRenderDevice();
		dev.SetClearColor(Vec4(0.f, 0.f, 0.f, 1.f));
		dev.Clear(dev.TranslateBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth));
		probeRenderer.RenderUI(&probe);
		fbo2.UnBind();
		dev.WaitIdle();

		std::vector<uchar> px = c2.GetTextureData();
		const size_t o = ((size_t)(H2 / 2) * W2 + W2 / 2) * 4;
		const int red = px.size() > o + 2 ? px[o] : 0;
		const int green = px.size() > o + 2 ? px[o + 1] : 0;
		const bool topWins = red > 200 && green < 60;
		const bool didNotMerge = canvas->GetBatchCount() == 0;
		printf("      stacked red/green/red: centre (%d,%d,%d), %u batches\n",
			red, green, px.size() > o + 2 ? px[o + 2] : 0, canvas->GetBatchCount());
		printf("%s  an element only moves earlier past what it does not overlap\n",
			(topWins && didNotMerge) ? "PASS" : "FAIL");
		if (!(topWins && didNotMerge)) failures++;

		probe.Remove(canvasGO);
	}

	// ---- the SDF flag survives a save/load ----
	// Two labels asking for the same file at the same size, one crisp and one
	// not, have to come back as two different atlases: the serializer's font
	// cache keys on path+size+mode, and if the mode is left out of the key
	// whichever label loads second silently inherits the other's bake.
	{
		std::shared_ptr<GameObject> src = std::make_shared<GameObject>();
		std::shared_ptr<UICanvas> srcCanvas = std::make_shared<UICanvas>(320.f, 200.f);
		src->Add(std::static_pointer_cast<IComponent>(srcCanvas));
		const char* fontPath = STR(EXAMPLES_PATH) "/assets/verdana.ttf";
		const char* names[2] = { "Plain", "Crisp" };
		for (int i = 0; i < 2; i++)
		{
			std::shared_ptr<GameObject> child = std::make_shared<GameObject>();
			child->SetName(names[i]);
			child->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIRect>()));
			std::shared_ptr<Font> f = std::make_shared<Font>(fontPath, 18, i == 1);
			f->CreateText("Ag");
			child->Add(std::static_pointer_cast<IComponent>(
				std::make_shared<UIText>(f, "Ag", 18.f, Vec4(1, 1, 1, 1))));
			src->Add(child);
		}

		const std::string subtree = SceneSerializer::SerializeSubtree(src.get(), "");
		LoadedSceneAssets loaded;
		std::shared_ptr<GameObject> back = SceneSerializer::DeserializeSubtree(subtree, "", NULL, NULL, &loaded);

		bool ok = back != NULL;
		Font* fonts[2] = { NULL, NULL };
		if (ok)
		{
			const std::vector<std::shared_ptr<GameObject>> &kids = back->GetChildren();
			ok = kids.size() == 2;
			for (size_t k = 0; ok && k < kids.size(); k++)
			{
				UIText* t = NULL;
				const std::vector<std::shared_ptr<IComponent>> &comps = kids[k]->GetComponents();
				for (size_t c = 0; c < comps.size(); c++)
					if (UIText* cast = dynamic_cast<UIText*>(comps[c].get())) t = cast;
				if (!t) { ok = false; break; }
				const int want = kids[k]->GetName() == "Crisp" ? 1 : 0;
				fonts[want] = t->GetFont().get();
				if (t->IsFontSDF() != (want == 1)) ok = false;
			}
		}
		const bool distinct = fonts[0] && fonts[1] && fonts[0] != fonts[1];
		printf("%s  the crisp flag round-trips, and the two bakes stay separate\n",
			(ok && distinct) ? "PASS" : "FAIL");
		if (!(ok && distinct)) failures++;

		SceneGraph tmpScene;
		SceneSerializer::UnloadScene(&tmpScene, loaded);
	}

	// ---- word wrap ----
	// The claim is that a long string breaks at word boundaries to fit the
	// element's rect, and that narrowing the rect re-wraps it. Both are
	// measurable: line count from the mesh, and the widest line never wider
	// than the rect.
	{
		std::shared_ptr<Font> f = fontBody;
		std::shared_ptr<GameObject> go = std::make_shared<GameObject>();
		go->SetName("WrapProbe");
		std::shared_ptr<UIRect> r = std::make_shared<UIRect>();
		r->SetAnchoredPosition(Vec2(0.f, 0.f), Vec2(0.f, 0.f), Vec2(600.f, 400.f));
		go->Add(std::static_pointer_cast<IComponent>(r));
		const std::string sentence = "the quick brown fox jumps over the lazy dog again and again";
		f->CreateText(sentence);
		std::shared_ptr<UIText> t = std::make_shared<UIText>(f, sentence, 30.f, Vec4(1, 1, 1, 1));
		go->Add(std::static_pointer_cast<IComponent>(t));
		menuObj->Add(go);

		Text* mesh = static_cast<Text*>(t->GetRenderable());
		t->OnRectSolved(UIRectValue(0.f, 0.f, 600.f, 400.f), Vec2(0.f, 0.f));
		const uint32 unwrapped = mesh->GetLineCount();
		const f32 unwrappedWidth = mesh->GetAdvanceWidth();

		t->SetWordWrap(true);
		t->OnRectSolved(UIRectValue(0.f, 0.f, 600.f, 400.f), Vec2(0.f, 0.f));
		const uint32 wide = mesh->GetLineCount();
		const f32 wideWidth = mesh->GetAdvanceWidth();

		t->OnRectSolved(UIRectValue(0.f, 0.f, 300.f, 400.f), Vec2(0.f, 0.f));
		const uint32 narrow = mesh->GetLineCount();
		const f32 narrowWidth = mesh->GetAdvanceWidth();

		printf("      unwrapped %u line(s) %.0f wide; wrapped to 600 -> %u line(s) %.0f; to 300 -> %u line(s) %.0f\n",
			unwrapped, unwrappedWidth, wide, wideWidth, narrow, narrowWidth);

		const bool ok = (unwrapped == 1) && (wide > 1) && (narrow > wide)
			&& (wideWidth <= 600.f) && (narrowWidth <= 300.f)
			&& (t->GetText() == sentence);
		printf("%s  word wrap breaks to fit and re-wraps when the rect narrows\n", ok ? "PASS" : "FAIL");
		if (!ok) failures++;

		menuObj->Remove(go);
	}

	// ---- serialization, and therefore prefabs ----
	// A canvas is an ordinary GameObject subtree carrying ordinary
	// components, so it goes through the same serializer as everything
	// else - which means SerializeSubtree/DeserializeSubtree, the exact
	// pair the editor's Create Prefab and Instantiate Prefab are built on,
	// work on a menu with no changes to the prefab layer at all. That is
	// the claim; this checks it.
	{
		const std::string prefab = SceneSerializer::SerializeSubtree(menuObj.get(), std::string());
		const bool mentions =
			prefab.find("\"UICanvas\"") != std::string::npos &&
			prefab.find("\"UIRect\"") != std::string::npos &&
			prefab.find("\"UIImage\"") != std::string::npos &&
			prefab.find("\"UIText\"") != std::string::npos;
		printf("%s  the menu serializes to a prefab carrying all four component types (%zu bytes)\n",
			mentions ? "PASS" : "FAIL", prefab.size());
		if (!mentions) failures++;

		SceneGraph clonedScene;
		LoadedSceneAssets cloneAssets;
		std::shared_ptr<GameObject> clone =
			SceneSerializer::DeserializeSubtree(prefab, std::string(), NULL, NULL, &cloneAssets);
		if (!clone)
		{
			printf("FAIL  the prefab reloads\n");
			failures++;
		}
		else
		{
			clonedScene.Add(clone);
			clonedScene.Update(0.0);

			std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(&clonedScene);
			bool ok = (canvases.size() == 1);
			if (ok)
			{
				canvases[0]->Solve(W, H);
				ok = ok && canvases[0]->GetSortOrder() == menuCanvas->GetSortOrder()
					&& canvases[0]->GetScaleMode() == menuCanvas->GetScaleMode()
					&& fabsf(canvases[0]->GetCanvasRect().width - menuCanvas->GetCanvasRect().width) < 0.5f;

				// And the layout it solves to has to match the original,
				// element for element - a prefab that loads but lands
				// somewhere else is not a prefab. Matched by name rather
				// than by index, so this compares the same element in both
				// trees even as the demo's layout changes.
				UIRect* original = FindRect(menuObj, "RowResume");
				UIRect* copy = FindRect(clone, "RowResume");
				ok = ok && original && copy
					&& fabsf(original->GetRect().width - copy->GetRect().width) < 0.5f
					&& fabsf(original->GetRect().height - copy->GetRect().height) < 0.5f
					&& fabsf(original->GetRect().x - copy->GetRect().x) < 0.5f
					&& fabsf(original->GetRect().y - copy->GetRect().y) < 0.5f;
				if (original && copy)
					printf("      first element: original %.0fx%.0f at %.0f,%.0f  reloaded %.0fx%.0f at %.0f,%.0f\n",
						original->GetRect().width, original->GetRect().height, original->GetRect().x, original->GetRect().y,
						copy->GetRect().width, copy->GetRect().height, copy->GetRect().x, copy->GetRect().y);
			}
			printf("%s  the reloaded prefab solves to the same layout\n", ok ? "PASS" : "FAIL");
			if (!ok) failures++;

			SceneSerializer::UnloadScene(&clonedScene, cloneAssets);
		}
	}

	// ---- and the same thing as a whole scene ----
	// The subtree path above and this one share DeserializeComponent, but
	// only this one goes through SaveScene's root walk and the material
	// pool, which is what the editor and the player actually call.
	{
		const std::string path = "ui_roundtrip.scene.json";
		const bool saved = SceneSerializer::SaveScene(Scene, path);
		SceneGraph reloaded;
		LoadedSceneAssets reloadedAssets;
		const bool loaded = saved && SceneSerializer::LoadScene(&reloaded, path, NULL, NULL, &reloadedAssets);
		bool ok = loaded;
		if (loaded)
		{
			reloaded.Update(0.0);
			std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(&reloaded);
			ok = (canvases.size() == 2);
			for (size_t i = 0; i < canvases.size(); i++) canvases[i]->Solve(W, H);

			UIRect* original = FindRect(menuObj, "RowResume");
			UIRect* copy = NULL;
			// Found by name, the same way anything else in a reloaded scene
			// would be.
			for (size_t i = 0; i < reloadedAssets.gameObjects.size() && !copy; i++)
				if (reloadedAssets.gameObjects[i]->GetName() == "RowResume")
					copy = RectOf(reloadedAssets.gameObjects[i]);
			ok = ok && original && copy
				&& fabsf(original->GetRect().x - copy->GetRect().x) < 0.5f
				&& fabsf(original->GetRect().y - copy->GetRect().y) < 0.5f
				&& fabsf(original->GetRect().width - copy->GetRect().width) < 0.5f;
			if (copy)
				printf("      reloaded RowResume %.0fx%.0f at %.0f,%.0f\n",
					copy->GetRect().width, copy->GetRect().height, copy->GetRect().x, copy->GetRect().y);
			SceneSerializer::UnloadScene(&reloaded, reloadedAssets);
		}
		printf("%s  both canvases survive a whole-scene save and reload\n", ok ? "PASS" : "FAIL");
		if (!ok) failures++;
		remove(path.c_str());
	}

	printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASSED", failures);
	fflush(stdout);
}

void UIExample::DrawUI()
{
	DrawBaseUI();

	if (ImGui::Begin("UI Example", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Two UICanvases over a 3D scene, drawn by UIRenderer.");
		ImGui::Separator();
		ImGui::Text("HUD  canvas, sort order 0");
		ImGui::Text("Menu canvas, sort order 10 (paints over it)");
		ImGui::Separator();
		ImGui::Text("Reference 1920x1080, MatchWidth - resize the");
		ImGui::Text("window and the whole layout holds.");
		ImGui::Separator();
		ImGui::Text("Every panel, pill and gradient is a procedural");
		ImGui::Text("texture baked at startup, used as a 9-slice.");
		ImGui::Text("No image files.");
	}
	ImGui::End();
}

void UIExample::Shutdown()
{
	for (size_t i = elements.size(); i > 0; i--)
	{
		GameObject* parent = elements[i - 1]->GetParent();
		if (parent) parent->Remove(elements[i - 1]);
	}
	rowBackgrounds.clear();
	rowLabels.clear();
	rowHints.clear();
	armourFill.reset();
	armourValue.reset();
	components.clear();
	elements.clear();

	if (hudObj) { hudObj->Remove(std::static_pointer_cast<IComponent>(hudCanvas)); Scene->Remove(hudObj); }
	if (menuObj) { menuObj->Remove(std::static_pointer_cast<IComponent>(menuCanvas)); Scene->Remove(menuObj); }
	hudCanvas.reset(); menuCanvas.reset();
	hudObj.reset(); menuObj.reset();

	texPanel.reset(); texShadow.reset(); texPill.reset(); texRamp.reset();
	fontTitle.reset(); fontBody.reset(); fontSmall.reset();

	for (size_t i = blocks.size(); i > 0; i--)
	{
		blocks[i - 1]->Remove(blockComponents[i - 1]);
		if (blockRoot) blockRoot->Remove(blocks[i - 1]);
	}
	blockComponents.clear();
	blocks.clear();
	if (blockRoot) Scene->Remove(blockRoot);
	blockRoot.reset();
	blockMesh.reset();
	blockMaterial.reset();

	if (lightObj) { lightObj->Remove(dirLight); Scene->Remove(lightObj); }
	dirLight.reset();
	lightObj.reset();

	delete uiRenderer;
	uiRenderer = NULL;

	BaseExample::Shutdown();
}
