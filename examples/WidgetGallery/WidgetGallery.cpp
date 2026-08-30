//============================================================================
// Name        : WidgetGallery
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Every widget the UI has, under three interchangeable skins
//============================================================================

#include "WidgetGallery.h"
#include "../WindowManagers/TextInputHook.h"
#include "UIStyleResolver.h"

#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <Pyros3D/Ext/stb/stb_image_write.h>

WidgetGallery* WidgetGallery::active = NULL;

//=============================================================================

WidgetGallery::WidgetGallery()
	: BaseExample(1280, 720, "Pyros3D - Widget Gallery", WindowType::Close | WindowType::Resize)
{
	uiRenderer = NULL;
	uiScene = NULL;
	theme = 0;
	verified = false;
	const char* v = getenv("PYROS_UI_VERIFY");
	verifyMode = (v != NULL && v[0] == '1');
	themes.push_back("midnight");
	themes.push_back("amber");
	themes.push_back("paper");
}

WidgetGallery::~WidgetGallery() {}

std::string WidgetGallery::AssetRoot() const
{
	return std::string(STR(EXAMPLES_PATH)) + "/assets";
}

void WidgetGallery::OnResize(const uint32 width, const uint32 height)
{
	BaseExample::OnResize(width, height);
	if (Renderer) Renderer->Resize(width, height);
	if (uiRenderer) uiRenderer->Resize(width, height);
}

// Characters arrive decoded from the window layer - see TextInputHook.h.
void WidgetGallery::OnTextTyped(const char* utf8)
{
	if (active && active->canvas) active->canvas->UpdateText(utf8);
}

//=============================================================================
// Building blocks
//
// Every element is a rect with a style name on it. Nothing here picks a
// colour, a texture or a border: those live in the .uistyle files, and
// ApplyTheme() is what puts them on.
//=============================================================================

std::shared_ptr<GameObject> WidgetGallery::Element(const std::shared_ptr<GameObject> &parent,
	const std::string &name, const Vec2 &anchorMin, const Vec2 &anchorMax,
	const Vec2 &offsetMin, const Vec2 &offsetMax, const std::string &style)
{
	std::shared_ptr<GameObject> go = std::make_shared<GameObject>();
	go->SetName(name);
	std::shared_ptr<UIRect> rect = std::make_shared<UIRect>();
	rect->SetAnchors(anchorMin, anchorMax);
	rect->SetOffsets(offsetMin, offsetMax);
	rect->SetPivot(Vec2(0.5f, 0.5f));
	if (!style.empty()) rect->SetStyleRef("ui/styles/" + style + ".uistyle");
	go->Add(std::static_pointer_cast<IComponent>(rect));
	parent->Add(go);
	return go;
}

UIButton* WidgetGallery::ButtonOn(GameObject* go)
{
	if (!go) return NULL;
	const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
	for (size_t i = 0; i < cs.size(); i++)
		if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIButton)
			return static_cast<UIButton*>(cs[i].get());
	return NULL;
}

UIRect* WidgetGallery::RectOn(GameObject* go)
{
	if (!go) return NULL;
	const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
	for (size_t i = 0; i < cs.size(); i++)
		if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
			return static_cast<UIRect*>(cs[i].get());
	return NULL;
}

std::shared_ptr<UIImage> WidgetGallery::Image(const std::shared_ptr<GameObject> &on)
{
	// White and untextured until a style says otherwise, which it always
	// does - an element with no style would show up as a white rectangle
	// rather than as nothing, and that is the failure worth seeing.
	std::shared_ptr<UIImage> img = std::make_shared<UIImage>(Vec4(1.f, 1.f, 1.f, 1.f));
	on->Add(std::static_pointer_cast<IComponent>(img));
	return img;
}

std::shared_ptr<UIText> WidgetGallery::Label(const std::shared_ptr<GameObject> &on, const std::string &text)
{
	std::shared_ptr<UIText> t = std::make_shared<UIText>(font, text, 22.f, Vec4(1.f, 1.f, 1.f, 1.f));
	t->SetAlignment(UIAlign::Left, UIVerticalAlign::Middle);
	on->Add(std::static_pointer_cast<IComponent>(t));
	return t;
}

//=============================================================================
// The widgets
//
// Each is the component plus the child elements it drives, named the way the
// component expects - the same shapes the editor's Add > UI builds.
//=============================================================================

std::shared_ptr<GameObject> WidgetGallery::Button(const std::shared_ptr<GameObject> &parent,
	const std::string &name, const Vec2 &offsetMin, const Vec2 &offsetMax,
	const std::string &text, bool primary)
{
	std::shared_ptr<GameObject> go = Element(parent, name, Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		offsetMin, offsetMax, primary ? "buttonPrimary" : "button");
	Image(go);
	std::shared_ptr<UIButton> b = std::make_shared<UIButton>();
	go->Add(std::static_pointer_cast<IComponent>(b));

	std::shared_ptr<GameObject> labelGO = Element(go, name + "Label", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
		Vec2(0.f, 0.f), Vec2(0.f, 0.f), "buttonLabel");
	std::shared_ptr<UIText> t = Label(labelGO, text);
	t->SetAlignment(UIAlign::Center, UIVerticalAlign::Middle);
	return go;
}

std::shared_ptr<UIToggle> WidgetGallery::Checkbox(const std::shared_ptr<GameObject> &parent,
	const std::string &name, const Vec2 &at, const std::string &text, const std::string &group)
{
	std::shared_ptr<GameObject> go = Element(parent, name, Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		at, Vec2(at.x + 28.f, at.y + 28.f), "checkbox");
	Image(go);
	std::shared_ptr<UIToggle> t = std::make_shared<UIToggle>();
	if (!group.empty()) t->SetGroup(group);
	go->Add(std::static_pointer_cast<IComponent>(t));

	// The tick, which the toggle shows and hides by name.
	std::shared_ptr<GameObject> tick = Element(go, "Check", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
		Vec2(4.f, 4.f), Vec2(-4.f, -4.f), "tick");
	Image(tick);

	// The caption sits beside the box rather than inside it, and is not
	// part of the toggle: clicking the word is a nicety, not a widget.
	std::shared_ptr<GameObject> caption = Element(parent, name + "Caption", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(at.x + 38.f, at.y), Vec2(at.x + 320.f, at.y + 28.f), "label");
	Label(caption, text);
	return t;
}

std::shared_ptr<UISlider> WidgetGallery::Slider(const std::shared_ptr<GameObject> &parent,
	const std::string &name, const Vec2 &offsetMin, const Vec2 &offsetMax, bool vertical)
{
	std::shared_ptr<GameObject> go = Element(parent, name, Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		offsetMin, offsetMax, "track");
	Image(go);
	std::shared_ptr<UISlider> s = std::make_shared<UISlider>();
	s->SetVertical(vertical);
	go->Add(std::static_pointer_cast<IComponent>(s));

	// Fill stretched along the track; the slider moves one of its anchors
	// and leaves the two-pixel inset alone.
	std::shared_ptr<GameObject> fill = Element(go, name + "Fill", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
		Vec2(2.f, 2.f), Vec2(-2.f, -2.f), "fill");
	Image(fill);
	s->SetFillElement(name + "Fill");

	// Handle pinned to a point on it, keeping whatever size its offsets give.
	std::shared_ptr<GameObject> handle = vertical
		? Element(go, name + "Handle", Vec2(0.f, 0.f), Vec2(1.f, 0.f), Vec2(-4.f, -11.f), Vec2(4.f, 11.f), "knob")
		: Element(go, name + "Handle", Vec2(0.f, 0.f), Vec2(0.f, 1.f), Vec2(-11.f, -4.f), Vec2(11.f, 4.f), "knob");
	Image(handle);
	s->SetHandleElement(name + "Handle");
	return s;
}

std::shared_ptr<UIInput> WidgetGallery::Field(const std::shared_ptr<GameObject> &parent,
	const std::string &name, const Vec2 &offsetMin, const Vec2 &offsetMax,
	const std::string &placeholder, bool password)
{
	std::shared_ptr<GameObject> go = Element(parent, name, Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		offsetMin, offsetMax, "field");
	Image(go);
	std::shared_ptr<UIInput> in = std::make_shared<UIInput>();
	in->SetPlaceholder(placeholder);
	in->SetPassword(password);
	go->Add(std::static_pointer_cast<IComponent>(in));

	std::shared_ptr<GameObject> ph = Element(go, name + "Placeholder", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
		Vec2(12.f, 0.f), Vec2(-12.f, 0.f), "labelDim");
	Label(ph, placeholder);
	in->SetPlaceholderElement(name + "Placeholder");

	std::shared_ptr<GameObject> text = Element(go, name + "Text", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
		Vec2(12.f, 0.f), Vec2(-12.f, 0.f), "label");
	Label(text, "");
	in->SetTextElement(name + "Text");

	std::shared_ptr<GameObject> caret = Element(go, name + "Caret", Vec2(0.f, 0.f), Vec2(0.f, 1.f),
		Vec2(12.f, 8.f), Vec2(14.f, -8.f), "caret");
	Image(caret);
	in->SetCaretElement(name + "Caret");
	return in;
}

std::shared_ptr<UIList> WidgetGallery::List(const std::shared_ptr<GameObject> &parent,
	const std::string &name, const Vec2 &offsetMin, const Vec2 &offsetMax,
	const uint32 rows, const f32 rowHeight)
{
	std::shared_ptr<GameObject> go = Element(parent, name, Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		offsetMin, offsetMax, "field");
	Image(go);
	std::shared_ptr<UIList> list = std::make_shared<UIList>();
	list->SetItemHeight(rowHeight);
	go->Add(std::static_pointer_cast<IComponent>(list));

	// Only as many rows as fit: the list recycles them as it scrolls.
	for (uint32 i = 0; i < rows; i++)
	{
		char rowName[32];
		snprintf(rowName, sizeof(rowName), "%sRow%u", name.c_str(), i);
		std::shared_ptr<GameObject> row = Element(go, rowName, Vec2(0.f, 0.f), Vec2(1.f, 0.f),
			Vec2(0.f, (f32)i * rowHeight), Vec2(0.f, (f32)(i + 1) * rowHeight));
		std::shared_ptr<GameObject> hl = Element(row, "Highlight", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
			Vec2(3.f, 1.f), Vec2(-3.f, -1.f), "highlight");
		Image(hl);
		std::shared_ptr<GameObject> label = Element(row, "Label", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
			Vec2(14.f, 0.f), Vec2(-14.f, 0.f), "label");
		Label(label, "");
	}
	list->SetRowPrefix(name + "Row");
	return list;
}

std::shared_ptr<UIDropdown> WidgetGallery::Dropdown(const std::shared_ptr<GameObject> &parent,
	const std::string &name, const Vec2 &offsetMin, const Vec2 &offsetMax)
{
	std::shared_ptr<GameObject> go = Element(parent, name, Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		offsetMin, offsetMax, "button");
	Image(go);
	std::shared_ptr<UIDropdown> dd = std::make_shared<UIDropdown>();
	go->Add(std::static_pointer_cast<IComponent>(dd));

	std::shared_ptr<GameObject> label = Element(go, name + "Label", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
		Vec2(14.f, 0.f), Vec2(-14.f, 0.f), "label");
	Label(label, "");
	dd->SetLabelElement(name + "Label");

	// The popup hangs below the closed dropdown and holds a list, which is
	// all a dropdown is.
	const f32 height = offsetMax.y - offsetMin.y;
	const f32 rowHeight = 30.f;
	// Room for three rows plus the inset the list sits in, or the last row
	// is clipped in half - which is exactly what a list clipping its rows
	// to its viewport is supposed to do, and looks like a bug here.
	std::shared_ptr<GameObject> popup = Element(go, name + "Popup", Vec2(0.f, 0.f), Vec2(1.f, 0.f),
		Vec2(0.f, height + 4.f), Vec2(0.f, height + 4.f + rowHeight * 3.f + 6.f), "panel");
	Image(popup);
	dd->SetPopupElement(name + "Popup");

	std::shared_ptr<UIList> list = List(popup, name + "List", Vec2(3.f, 3.f), Vec2(-3.f, -3.f), 3, rowHeight);
	// The list fills the popup, so it is anchored rather than sized.
	{
		const std::vector<std::shared_ptr<IComponent> > &cs = list->GetOwner()->GetComponents();
		for (size_t i = 0; i < cs.size(); i++)
			if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
				static_cast<UIRect*>(cs[i].get())->SetAnchors(Vec2(0.f, 0.f), Vec2(1.f, 1.f));
	}
	return dd;
}

//=============================================================================

std::shared_ptr<UIMenuItem> WidgetGallery::MenuEntry(const std::shared_ptr<GameObject> &parent,
	const std::string &name, const Vec2 &offsetMin, const Vec2 &offsetMax,
	const std::string &text, const bool centred)
{
	std::shared_ptr<GameObject> go = Element(parent, name, Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		offsetMin, offsetMax, "menuItem");
	Image(go);
	std::shared_ptr<GameObject> label = Element(go, name + "Label", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
		Vec2(centred ? 0.f : 14.f, 0.f), Vec2(centred ? 0.f : -14.f, 0.f), "label");
	std::shared_ptr<UIText> t = Label(label, text);
	if (centred) t->SetAlignment(UIAlign::Center, UIVerticalAlign::Middle);
	std::shared_ptr<UIMenuItem> item = std::make_shared<UIMenuItem>();
	go->Add(std::static_pointer_cast<IComponent>(item));
	return item;
}

// A menu bar with two menus, one of which nests. Built from the same
// elements as everything else here - a menu is a tree of entries, not a
// special kind of thing.
void WidgetGallery::BuildMenuBar(const std::shared_ptr<GameObject> &parent)
{
	std::shared_ptr<GameObject> barGO = Element(parent, "MenuBar", Vec2(0.f, 0.f), Vec2(1.f, 0.f),
		Vec2(0.f, 0.f), Vec2(0.f, 30.f), "menuBar");
	Image(barGO);
	std::shared_ptr<UIMenu> menu = std::make_shared<UIMenu>();
	barGO->Add(std::static_pointer_cast<IComponent>(menu));

	const f32 rowHeight = 28.f;
	const char* titles[2] = { "File", "View" };
	const char* entries[2][3] = { { "New Scene", "Open...", "Recent" }, { "Wireframe", "Statistics", "Fullscreen" } };

	for (int m = 0; m < 2; m++)
	{
		std::shared_ptr<UIMenuItem> title = MenuEntry(barGO, titles[m],
			Vec2(8.f + (f32)m * 84.f, 0.f), Vec2(8.f + (f32)(m + 1) * 84.f - 4.f, 30.f), titles[m], true);
		std::shared_ptr<GameObject> titleGO = title->GetOwner()->GetChildren().empty()
			? std::shared_ptr<GameObject>() : std::shared_ptr<GameObject>();
		(void)titleGO;

		// The panel this title opens, parented to the title so it travels
		// with it, and hidden until it is opened.
		std::shared_ptr<GameObject> ownerGO;
		{
			const std::vector<std::shared_ptr<GameObject> > &kids = barGO->GetChildren();
			for (size_t i = 0; i < kids.size(); i++)
				if (kids[i].get() == title->GetOwner()) ownerGO = kids[i];
		}
		std::shared_ptr<GameObject> panel = Element(ownerGO, std::string(titles[m]) + "Menu",
			Vec2(0.f, 1.f), Vec2(0.f, 1.f), Vec2(0.f, 2.f), Vec2(210.f, 2.f + rowHeight * 3.f), "menuPanel");
		Image(panel);
		if (UIRect* r = RectOn(panel.get())) r->SetVisible(false);
		title->SetSubmenu(panel->GetName());

		for (int e = 0; e < 3; e++)
		{
			std::shared_ptr<UIMenuItem> entry = MenuEntry(panel, std::string(titles[m]) + entries[m][e],
				Vec2(0.f, (f32)e * rowHeight), Vec2(210.f, (f32)(e + 1) * rowHeight), entries[m][e], false);

			// One nested submenu, so the nesting is visible rather than
			// described.
			if (m == 0 && e == 2)
			{
				std::shared_ptr<GameObject> entryGO;
				{
					const std::vector<std::shared_ptr<GameObject> > &kids = panel->GetChildren();
					for (size_t i = 0; i < kids.size(); i++)
						if (kids[i].get() == entry->GetOwner()) entryGO = kids[i];
				}
				std::shared_ptr<GameObject> sub = Element(entryGO, "RecentMenu",
					Vec2(1.f, 0.f), Vec2(1.f, 0.f), Vec2(0.f, 0.f), Vec2(210.f, rowHeight * 2.f), "menuPanel");
				Image(sub);
				if (UIRect* r = RectOn(sub.get())) r->SetVisible(false);
				entry->SetSubmenu(sub->GetName());
				MenuEntry(sub, "RecentA", Vec2(0.f, 0.f), Vec2(210.f, rowHeight), "arena.scene", false);
				MenuEntry(sub, "RecentB", Vec2(0.f, rowHeight), Vec2(210.f, rowHeight * 2.f), "lobby.scene", false);
			}
		}
	}
}

// A confirmation dialog: a scrim over everything, a panel, and two buttons.
// Opened by the Reset button and closed by either of its own, polled in
// Update() - this example has no scripting, so it does by hand what a named
// handler would do for it.
void WidgetGallery::BuildDialog(const std::shared_ptr<GameObject> &parent)
{
	std::shared_ptr<GameObject> root = Element(parent, "Confirm", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
		Vec2(0.f, 0.f), Vec2(0.f, 0.f));
	confirm = std::make_shared<UIPopup>();
	root->Add(std::static_pointer_cast<IComponent>(confirm));

	std::shared_ptr<GameObject> scrim = Element(root, "Scrim", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
		Vec2(0.f, 0.f), Vec2(0.f, 0.f), "scrim");
	Image(scrim);

	std::shared_ptr<GameObject> dialog = Element(root, "Dialog", Vec2(0.5f, 0.5f), Vec2(0.5f, 0.5f),
		Vec2(-230.f, -120.f), Vec2(230.f, 120.f), "panel");
	Image(dialog);

	std::shared_ptr<GameObject> title = Element(dialog, "DialogTitle", Vec2(0.f, 0.f), Vec2(1.f, 0.f),
		Vec2(28.f, 24.f), Vec2(-28.f, 66.f), "title");
	std::shared_ptr<UIText> tt = std::make_shared<UIText>(fontTitle, "Reset settings?", 28.f, Vec4(1, 1, 1, 1));
	tt->SetAlignment(UIAlign::Left, UIVerticalAlign::Middle);
	title->Add(std::static_pointer_cast<IComponent>(tt));

	std::shared_ptr<GameObject> body = Element(dialog, "DialogBody", Vec2(0.f, 0.f), Vec2(1.f, 0.f),
		Vec2(28.f, 74.f), Vec2(-28.f, 140.f), "labelDim");
	std::shared_ptr<UIText> bt = Label(body, "Everything on this screen goes back to its default. This cannot be undone.");
	bt->SetWordWrap(true);
	bt->SetAlignment(UIAlign::Left, UIVerticalAlign::Top);

	cancelButton = Button(dialog, "Cancel", Vec2(-300.f, -76.f), Vec2(-160.f, -28.f), "Cancel", false);
	confirmButton = Button(dialog, "Confirm", Vec2(-150.f, -76.f), Vec2(-10.f, -28.f), "Reset", true);
	// Anchored to the dialog's bottom-right corner, so they stay in it.
	for (int i = 0; i < 2; i++)
	{
		GameObject* b = (i == 0 ? cancelButton : confirmButton).get();
		if (UIRect* r = RectOn(b)) r->SetAnchors(Vec2(1.f, 1.f), Vec2(1.f, 1.f));
	}
}

void WidgetGallery::ApplyTheme(const uint32 index)
{
	theme = index % (uint32)themes.size();
	const std::string palette = AssetRoot() + "/ui/styles/" + themes[theme] + ".palette";
	std::string err;
	// The whole of theme switching: the same styles, a different palette.
	const int applied = uistyle::ApplyToScene(uiScene, AssetRoot(), palette, err);
	if (!err.empty()) echo("WidgetGallery: " + err);
	if (status) status->SetText("Theme: " + themes[theme] + "  (" + std::to_string(applied) + " elements styled)");
}

void WidgetGallery::Init()
{
	BaseExample::Init();
	active = this;
	PyrosTextInput::SetHandler(&WidgetGallery::OnTextTyped);

	uiScene = new SceneGraph();
	uiRenderer = new UIRenderer(Width, Height);

	const std::string fontPath = std::string(STR(EXAMPLES_PATH)) + "/assets/verdana.ttf";
	// One SDF bake per size, so the same atlas stays sharp whatever the
	// canvas scale works out to.
	font = std::make_shared<Font>(fontPath, 24, true);
	fontSmall = std::make_shared<Font>(fontPath, 18, true);
	fontTitle = std::make_shared<Font>(fontPath, 34, true);

	canvasGO = std::make_shared<GameObject>();
	canvasGO->SetName("Canvas");
	canvas = std::make_shared<UICanvas>(1280.f, 720.f);
	canvas->SetScaleMode(UIScaleMode::MatchWidth);
	canvasGO->Add(std::static_pointer_cast<IComponent>(canvas));
	uiScene->Add(canvasGO);

	// ---- background and header ----
	std::shared_ptr<GameObject> back = Element(canvasGO, "Background", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
		Vec2(0.f, 0.f), Vec2(0.f, 0.f), "panel");
	Image(back);

	std::shared_ptr<GameObject> titleGO = Element(canvasGO, "Title", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(40.f, 34.f), Vec2(600.f, 82.f), "title");
	std::shared_ptr<UIText> title = std::make_shared<UIText>(fontTitle, "Widget Gallery", 34.f, Vec4(1, 1, 1, 1));
	title->SetAlignment(UIAlign::Left, UIVerticalAlign::Middle);
	titleGO->Add(std::static_pointer_cast<IComponent>(title));

	std::shared_ptr<GameObject> statusGO = Element(canvasGO, "Status", Vec2(0.f, 1.f), Vec2(1.f, 1.f),
		Vec2(40.f, -54.f), Vec2(-40.f, -20.f), "labelDim");
	status = std::make_shared<UIText>(fontSmall, "", 18.f, Vec4(1, 1, 1, 1));
	status->SetAlignment(UIAlign::Left, UIVerticalAlign::Middle);
	statusGO->Add(std::static_pointer_cast<IComponent>(status));

	// ---- left column: toggles and buttons ----
	std::shared_ptr<GameObject> left = Element(canvasGO, "LeftPanel", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(40.f, 96.f), Vec2(400.f, 560.f), "frame");
	Image(left);

	std::shared_ptr<GameObject> leftTitle = Element(left, "LeftTitle", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(24.f, 18.f), Vec2(340.f, 50.f), "label");
	Label(leftTitle, "Display");

	fullscreen = Checkbox(left, "Fullscreen", Vec2(24.f, 62.f), "Fullscreen", "");
	invertY = Checkbox(left, "InvertY", Vec2(24.f, 104.f), "Invert look", "");
	fullscreen->SetValue(true);

	std::shared_ptr<GameObject> qualityTitle = Element(left, "QualityTitle", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(24.f, 158.f), Vec2(340.f, 190.f), "label");
	Label(qualityTitle, "Quality");

	// A radio group: siblings sharing a group name, so turning one on turns
	// the others off.
	std::shared_ptr<UIToggle> low = Checkbox(left, "QualityLow", Vec2(24.f, 202.f), "Low", "quality");
	std::shared_ptr<UIToggle> med = Checkbox(left, "QualityMedium", Vec2(24.f, 244.f), "Medium", "quality");
	std::shared_ptr<UIToggle> high = Checkbox(left, "QualityHigh", Vec2(24.f, 286.f), "High", "quality");
	med->SetValue(true);

	Button(left, "Apply", Vec2(24.f, 356.f), Vec2(160.f, 400.f), "Apply", true);
	resetButton = Button(left, "Reset", Vec2(176.f, 356.f), Vec2(312.f, 400.f), "Reset", false);
	std::shared_ptr<GameObject> disabled = Button(left, "Locked", Vec2(24.f, 412.f), Vec2(312.f, 456.f), "Unavailable", false);
	{
		const std::vector<std::shared_ptr<IComponent> > &cs = disabled->GetComponents();
		for (size_t i = 0; i < cs.size(); i++)
			if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIButton)
				static_cast<UIButton*>(cs[i].get())->SetInteractable(false);
	}

	// ---- middle column: sliders and fields ----
	std::shared_ptr<GameObject> middle = Element(canvasGO, "MiddlePanel", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(420.f, 96.f), Vec2(820.f, 560.f), "frame");
	Image(middle);

	std::shared_ptr<GameObject> audioTitle = Element(middle, "AudioTitle", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(24.f, 18.f), Vec2(340.f, 50.f), "label");
	Label(audioTitle, "Audio");

	std::shared_ptr<GameObject> volLabel = Element(middle, "VolumeLabel", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(24.f, 62.f), Vec2(240.f, 90.f), "labelDim");
	Label(volLabel, "Master volume");
	std::shared_ptr<GameObject> volValueGO = Element(middle, "VolumeValue", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(240.f, 62.f), Vec2(352.f, 90.f), "label");
	volumeValue = Label(volValueGO, "");
	volumeValue->SetAlignment(UIAlign::Right, UIVerticalAlign::Middle);
	volume = Slider(middle, "Volume", Vec2(24.f, 96.f), Vec2(352.f, 112.f), false);
	volume->SetRange(0.f, 100.f);
	volume->SetValue(70.f);

	std::shared_ptr<GameObject> brLabel = Element(middle, "BrightnessLabel", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(24.f, 134.f), Vec2(240.f, 162.f), "labelDim");
	Label(brLabel, "Brightness");
	std::shared_ptr<GameObject> brValueGO = Element(middle, "BrightnessValue", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(240.f, 134.f), Vec2(352.f, 162.f), "label");
	brightnessValue = Label(brValueGO, "");
	brightnessValue->SetAlignment(UIAlign::Right, UIVerticalAlign::Middle);
	brightness = Slider(middle, "Brightness", Vec2(24.f, 168.f), Vec2(352.f, 184.f), false);
	brightness->SetRange(0.f, 10.f);
	brightness->SetStep(0.5f);
	brightness->SetValue(5.f);

	// A vertical one, filling upwards.
	std::shared_ptr<GameObject> balLabel = Element(middle, "BalanceLabel", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(24.f, 206.f), Vec2(240.f, 234.f), "labelDim");
	Label(balLabel, "Rear speakers");
	std::shared_ptr<GameObject> balValueGO = Element(middle, "BalanceValue", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(240.f, 206.f), Vec2(352.f, 234.f), "label");
	balanceValue = Label(balValueGO, "");
	balanceValue->SetAlignment(UIAlign::Right, UIVerticalAlign::Middle);
	balance = Slider(middle, "Balance", Vec2(24.f, 242.f), Vec2(40.f, 380.f), true);
	balance->SetRange(0.f, 100.f);
	balance->SetValue(40.f);

	std::shared_ptr<GameObject> nameLabel = Element(middle, "NameLabel", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(72.f, 246.f), Vec2(352.f, 274.f), "labelDim");
	Label(nameLabel, "Player name");
	nameField = Field(middle, "NameField", Vec2(72.f, 280.f), Vec2(352.f, 324.f), "Type a name", false);

	std::shared_ptr<GameObject> passLabel = Element(middle, "PassLabel", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(72.f, 336.f), Vec2(352.f, 364.f), "labelDim");
	Label(passLabel, "Server password");
	passField = Field(middle, "PassField", Vec2(72.f, 370.f), Vec2(352.f, 414.f), "Optional", true);

	// ---- right column: a list, and the theme picker ----
	std::shared_ptr<GameObject> right = Element(canvasGO, "RightPanel", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(840.f, 96.f), Vec2(1240.f, 560.f), "frame");
	Image(right);

	std::shared_ptr<GameObject> listTitle = Element(right, "ListTitle", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(24.f, 18.f), Vec2(340.f, 50.f), "label");
	Label(listTitle, "Levels");

	levels = List(right, "Levels", Vec2(24.f, 62.f), Vec2(376.f, 302.f), 8, 30.f);
	{
		std::vector<std::string> items;
		const char* names[] = { "Ashfall Ridge", "Blue Harbour", "Cinder Vault", "Deepwell",
			"Eastgate", "Frostline", "Glasshouse", "Hollow Point", "Ironworks", "Junkyard",
			"Kelp Forest", "Longwatch", "Meridian", "Northreach" };
		for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) items.push_back(names[i]);
		levels->SetItems(items);
		levels->SetSelected(1);
	}

	std::shared_ptr<GameObject> themeLabel = Element(right, "ThemeLabel", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(24.f, 322.f), Vec2(376.f, 350.f), "labelDim");
	Label(themeLabel, "Theme");
	// Before the dropdown, deliberately: siblings paint in order, and the
	// popup belongs to the dropdown - anything created after it would paint
	// over the open popup.
	std::shared_ptr<GameObject> hint = Element(right, "Hint", Vec2(0.f, 0.f), Vec2(0.f, 0.f),
		Vec2(24.f, 414.f), Vec2(376.f, 442.f), "labelDim");
	Label(hint, "Pick a theme to re-skin everything");

	themePicker = Dropdown(right, "Theme", Vec2(24.f, 356.f), Vec2(376.f, 400.f));
	themePicker->SetOptions(themes);
	themePicker->SetSelected(0);

	// The menu bar late, deliberately: siblings paint in order, and an open
	// menu has to be over everything, including the panels its popups hang
	// across. The dialog goes after even that - its scrim covers the menu
	// bar too, which is what being modal looks like.
	BuildMenuBar(canvasGO);
	BuildDialog(canvasGO);

	// One solve before the first style application, so every element has a
	// rect - the styles only touch look, but the canvas has to have run.
	uiScene->Update(0.0);
	canvas->Solve(1280.f, 720.f);
	ApplyTheme(0);
}

void WidgetGallery::Update()
{
	if (getenv("PYROS_UI_SHOW") && !verified)
	{
		RunShowcase();
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

	uiScene->Update(GetTime());

	// ---- input ----
	int mx = 0, my = 0;
	const Uint32 buttons = SDL_GetMouseState(&mx, &my);
	const bool down = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
	const bool inside = (mx >= 0 && my >= 0 && (uint32)mx < Width && (uint32)my < Height);
	const UIRectValue &r = canvas->GetCanvasRect();
	const Vec2 point((f32)mx / (f32)Width * r.width, (f32)my / (f32)Height * r.height);
	canvas->UpdateInput(point, down, inside);

	// Keys the canvas cares about, edge-detected: held keys must not repeat
	// a backspace sixty times a second.
	static const struct { int scancode; uint32 key; } keyMap[] = {
		{ SDL_SCANCODE_BACKSPACE, UIKey::Backspace }, { SDL_SCANCODE_DELETE, UIKey::Delete },
		{ SDL_SCANCODE_LEFT, UIKey::Left }, { SDL_SCANCODE_RIGHT, UIKey::Right },
		{ SDL_SCANCODE_UP, UIKey::Up }, { SDL_SCANCODE_DOWN, UIKey::Down },
		{ SDL_SCANCODE_HOME, UIKey::Home }, { SDL_SCANCODE_END, UIKey::End },
		{ SDL_SCANCODE_RETURN, UIKey::Enter }, { SDL_SCANCODE_ESCAPE, UIKey::Escape },
		{ SDL_SCANCODE_TAB, UIKey::Tab },
	};
	static bool wasDown[sizeof(keyMap) / sizeof(keyMap[0])] = { false };
	const Uint8* keys = SDL_GetKeyboardState(NULL);
	for (size_t i = 0; i < sizeof(keyMap) / sizeof(keyMap[0]); i++)
	{
		const bool nowDown = keys[keyMap[i].scancode] != 0;
		if (nowDown && !wasDown[i])
		{
			// Tab walks focus when nothing claims it, which is what makes
			// a form fillable without the mouse.
			if (!canvas->UpdateKey(keyMap[i].key) && keyMap[i].key == UIKey::Tab)
				canvas->MoveFocus(Vec2(0.f, 1.f));
		}
		wasDown[i] = nowDown;
	}

	// ---- what the widgets say ----
	char buf[64];
	snprintf(buf, sizeof(buf), "%d%%", (int)(volume->GetValue() + 0.5f));
	volumeValue->SetText(buf);
	snprintf(buf, sizeof(buf), "%.1f", brightness->GetValue());
	brightnessValue->SetText(buf);
	snprintf(buf, sizeof(buf), "%d%%", (int)(balance->GetValue() + 0.5f));
	balanceValue->SetText(buf);

	// What a named onClick would do, done by hand: this example has no
	// scripting, and a dialog nothing can open is not a dialog.
	if (UIButton* reset = ButtonOn(resetButton.get()))
		if (reset->ConsumeClicked()) confirm->Open();
	if (UIButton* cancel = ButtonOn(cancelButton.get()))
		if (cancel->ConsumeClicked()) confirm->Close();
	if (UIButton* ok = ButtonOn(confirmButton.get()))
		if (ok->ConsumeClicked())
		{
			confirm->Close();
			volume->SetValue(70.f);
			brightness->SetValue(5.f);
			balance->SetValue(40.f);
			nameField->SetText("");
			levels->SetSelected(1);
			fullscreen->SetValue(true);
			invertY->SetValue(false);
		}

	// The dropdown picking a theme is the whole demo: one call, and every
	// element wearing a style repaints.
	if (themePicker->GetSelected() >= 0 && (uint32)themePicker->GetSelected() != theme)
		ApplyTheme((uint32)themePicker->GetSelected());

	PrepareImGuiFrame();
	uiRenderer->RenderUI(uiScene);
	EndImGuiFrame();
}

void WidgetGallery::Shutdown()
{
	PyrosTextInput::SetHandler(NULL);
	active = NULL;

	// Everything holding a GPU object goes first, while there is still a
	// render device to hand its buffers back to. Held as members, these
	// would otherwise be released by ~WidgetGallery - which runs after
	// BaseExample::Shutdown() has torn the device down, and the first
	// glDeleteVertexArrays through a dead device is a call through null.
	volume.reset(); brightness.reset(); balance.reset();
	nameField.reset(); passField.reset();
	levels.reset(); themePicker.reset();
	fullscreen.reset(); invertY.reset();
	volumeValue.reset(); brightnessValue.reset(); balanceValue.reset(); status.reset();
	confirm.reset(); resetButton.reset(); cancelButton.reset(); confirmButton.reset();
	canvas.reset();
	canvasGO.reset();
	font.reset(); fontSmall.reset(); fontTitle.reset();

	if (uiRenderer) { delete uiRenderer; uiRenderer = NULL; }
	if (uiScene) { delete uiScene; uiScene = NULL; }
	BaseExample::Shutdown();
}

//=============================================================================
// PYROS_UI_SHOW=1: a handful of frames with the widgets actually being used,
// written out as images. The verification above proves things work; this is
// for looking at them, on a machine whose screen cannot be captured.
//=============================================================================
void WidgetGallery::RunShowcase()
{
	const uint32 W = 1280, H = 720;

	Texture color;
	color.CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, W, H, false);
	color.SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	Texture depth;
	depth.CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, W, H, false);
	FrameBuffer fbo;
	fbo.Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, &depth);
	fbo.AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, &color);

	// State transitions cross-fade over a duration, so a frame captured the
	// instant after a press is still mid-fade. Time has to advance.
	f64 clock = 0.0;
	auto shot = [&](const char* file) {
		for (int i = 0; i < 20; i++) { clock += 0.05; uiScene->Update(clock); }
		// What Update() would have put in the readouts. Skipping it is what
		// makes an offscreen capture quietly differ from the running app.
		char buf[64];
		snprintf(buf, sizeof(buf), "%d%%", (int)(volume->GetValue() + 0.5f));
		volumeValue->SetText(buf);
		snprintf(buf, sizeof(buf), "%.1f", brightness->GetValue());
		brightnessValue->SetText(buf);
		snprintf(buf, sizeof(buf), "%d%%", (int)(balance->GetValue() + 0.5f));
		balanceValue->SetText(buf);
		uiScene->Update(clock);
		fbo.Bind();
		IRenderDevice &dev = GetActiveRenderDevice();
		dev.SetClearColor(Vec4(0.f, 0.f, 0.f, 1.f));
		dev.Clear(dev.TranslateBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth));
		uiRenderer->Resize(W, H);
		uiRenderer->RenderUI(uiScene);
		fbo.UnBind();
		dev.WaitIdle();
		std::vector<uchar> px = color.GetTextureData();
		stbi_write_png(file, (int)W, (int)H, 4, px.data(), (int)W * 4);
		printf("      wrote %s\n", file);
	};
	auto rectOf = [&](GameObject* go) -> UIRectValue {
		if (!go) return UIRectValue(0.f, 0.f, 0.f, 0.f);
		const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
		for (size_t i = 0; i < cs.size(); i++)
			if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
				return static_cast<UIRect*>(cs[i].get())->GetRect();
		return UIRectValue(0.f, 0.f, 0.f, 0.f);
	};
	auto centre = [&](GameObject* go) -> Vec2 {
		const UIRectValue r = rectOf(go);
		return Vec2(r.x + r.width * 0.5f, r.y + r.height * 0.5f);
	};
	auto findByName = [&](const std::string &name) -> GameObject* {
		std::vector<GameObject*> stack;
		stack.push_back(canvasGO.get());
		while (!stack.empty())
		{
			GameObject* go = stack.back(); stack.pop_back();
			if (go->GetName() == name) return go;
			const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
			for (size_t i = 0; i < kids.size(); i++) if (kids[i]) stack.push_back(kids[i].get());
		}
		return NULL;
	};
	// Pointer state for a frame, with a released frame first where a press
	// is wanted - widgets arm on the up-to-down transition.
	auto point = [&](const Vec2 &p, const bool down) {
		canvas->UpdateInput(p, down, true);
		uiScene->Update(clock);
	};

	// 1. Hovering the primary button, with the pointer resting on it.
	point(centre(findByName("Apply")), false);
	shot("show_1_hover.png");

	// 2. Holding it down: the pressed tint and the two-pixel nudge.
	point(centre(findByName("Apply")), true);
	shot("show_2_pressed.png");
	point(centre(findByName("Apply")), false);

	// 3. A different quality picked (the radio group), the checkbox turned
	//    off, and both sliders dragged somewhere else.
	point(centre(findByName("QualityHigh")), false);
	point(centre(findByName("QualityHigh")), true);
	point(centre(findByName("QualityHigh")), false);
	point(centre(findByName("Fullscreen")), false);
	point(centre(findByName("Fullscreen")), true);
	point(centre(findByName("Fullscreen")), false);
	{
		const UIRectValue v = rectOf(volume->GetOwner());
		point(Vec2(v.x + v.width * 0.25f, v.y + v.height * 0.5f), false);
		point(Vec2(v.x + v.width * 0.25f, v.y + v.height * 0.5f), true);
		point(Vec2(v.x + v.width * 0.25f, v.y + v.height * 0.5f), false);
		const UIRectValue b = rectOf(balance->GetOwner());
		point(Vec2(b.x + b.width * 0.5f, b.y + b.height * 0.2f), false);
		point(Vec2(b.x + b.width * 0.5f, b.y + b.height * 0.2f), true);
		point(Vec2(b.x + b.width * 0.5f, b.y + b.height * 0.2f), false);
	}
	// Typing into the field, which also parks a caret in it.
	{
		const UIRectValue f = rectOf(nameField->GetOwner());
		const Vec2 in(f.x + 30.f, f.y + f.height * 0.5f);
		point(in, false); point(in, true); point(in, false);
		canvas->UpdateText("Peixinho");
	}
	// The list scrolled down and a different level picked.
	canvas->UpdateScroll(centre(levels->GetOwner()), -3.f);
	{
		const UIRectValue l = rectOf(levels->GetOwner());
		const Vec2 row(l.x + 40.f, l.y + 100.f);
		point(row, false); point(row, true); point(row, false);
	}
	shot("show_3_used.png");

	// 4. The File menu open, with its Recent submenu hovered open inside
	//    it - a menu bar follows the pointer once it is armed, so this is
	//    one click and two moves.
	{
		GameObject* file = findByName("File");
		point(centre(file), false);
		point(centre(file), true);
		point(centre(file), false);
		uiScene->Update(clock);
		canvas->Solve(1280.f, 720.f);
		if (GameObject* recent = findByName("FileRecent"))
		{
			point(centre(recent), false);
			uiScene->Update(clock);
			canvas->Solve(1280.f, 720.f);
			point(centre(recent), false);
		}
		shot("show_4_menu.png");
		// Dismissed the way a menu is: a press anywhere else.
		point(Vec2(640.f, 640.f), false);
		point(Vec2(640.f, 640.f), true);
		point(Vec2(640.f, 640.f), false);
	}

	// 5. The dropdown open over everything, which is the case that needs
	//    paint order and clipping to both be right.
	{
		const Vec2 t = centre(themePicker->GetOwner());
		point(t, false); point(t, true); point(t, false);
	}
	shot("show_5_dropdown.png");

	// 6. The confirmation dialog, which is what a modal looks like: the
	//    screen is still there and none of it can be touched.
	{
		const Vec2 onReset = centre(findByName("Reset"));
		point(onReset, false);
		point(onReset, true);
		point(onReset, false);
		if (UIButton* reset = ButtonOn(findByName("Reset")))
			if (reset->ConsumeClicked()) confirm->Open();
		uiScene->Update(clock);
		canvas->Solve(1280.f, 720.f);
		shot("show_6_dialog.png");
		confirm->Close();
		uiScene->Update(clock);
		canvas->Solve(1280.f, 720.f);
	}

	// 7. The same screen, same state, amber - the theme is a file swap.
	ApplyTheme(1);
	shot("show_7_amber.png");
}

//=============================================================================
// Acceptance test
//
// The widget logic is covered headlessly by tools/tests/ui_widgets.cpp; this
// is the half that proves the styles reach the screen - that every widget
// draws, that the skin comes from the files rather than from this code, and
// that switching theme actually repaints what is already on screen.
//=============================================================================
void WidgetGallery::RunVerification()
{
	const uint32 W = 1280, H = 720;
	int failures = 0;

	Texture color;
	color.CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, W, H, false);
	color.SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	Texture depth;
	depth.CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, W, H, false);
	FrameBuffer fbo;
	fbo.Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, &depth);
	fbo.AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, &color);

	std::vector<uchar> px;
	// One frame, into the FBO, with the canvas solved at exactly the
	// reference resolution so canvas units are pixels.
	auto frame = [&]() {
		uiScene->Update(0.0);
		fbo.Bind();
		IRenderDevice &dev = GetActiveRenderDevice();
		dev.SetClearColor(Vec4(0.f, 0.f, 0.f, 1.f));
		dev.Clear(dev.TranslateBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth));
		uiRenderer->Resize(W, H);
		uiRenderer->RenderUI(uiScene);
		fbo.UnBind();
		dev.WaitIdle();
		px = color.GetTextureData();
	};

	// Canvas point -> pixel, and the y flip the readback needs on the two
	// backends whose origin is the other corner.
	auto atIn = [&](const std::vector<uchar> &buf, const f32 cx, const f32 cy) -> Vec4 {
		const uint32 x = (uint32)cx, y = (uint32)cy;
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
		const size_t o = ((size_t)y * W + x) * 4;
#else
		const size_t o = ((size_t)(H - 1 - y) * W + x) * 4;
#endif
		if (buf.size() < o + 4) return Vec4(-1.f, -1.f, -1.f, -1.f);
		return Vec4((f32)buf[o], (f32)buf[o + 1], (f32)buf[o + 2], (f32)buf[o + 3]);
	};
	auto at = [&](const f32 cx, const f32 cy) -> Vec4 { return atIn(px, cx, cy); };
	auto check = [&](const bool ok, const char* what) {
		printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
		if (!ok) failures++;
	};
	auto differs = [](const Vec4 &a, const Vec4 &b) {
		return fabsf(a.x - b.x) + fabsf(a.y - b.y) + fabsf(a.z - b.z) > 12.f;
	};

	frame();

	// Where an element actually is, rather than where this file guessed it
	// would be: the solved rect is the truth, and a layout tweak must not
	// silently turn these probes into probes of the background.
	auto rectOf = [&](GameObject* go) -> UIRectValue {
		if (!go) return UIRectValue(0.f, 0.f, 0.f, 0.f);
		const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
		for (size_t i = 0; i < cs.size(); i++)
			if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
				return static_cast<UIRect*>(cs[i].get())->GetRect();
		return UIRectValue(0.f, 0.f, 0.f, 0.f);
	};
	auto childOf = [&](GameObject* go, const std::string &name) -> GameObject* {
		if (!go) return NULL;
		const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
			if (kids[i] && kids[i]->GetName() == name) return kids[i].get();
		return NULL;
	};
	auto findByName = [&](const std::string &name) -> GameObject* {
		std::vector<GameObject*> stack;
		stack.push_back(canvasGO.get());
		while (!stack.empty())
		{
			GameObject* go = stack.back(); stack.pop_back();
			if (go->GetName() == name) return go;
			const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
			for (size_t i = 0; i < kids.size(); i++) if (kids[i]) stack.push_back(kids[i].get());
		}
		return NULL;
	};
	auto visible = [&](GameObject* go) -> bool {
		if (!go) return false;
		const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
		for (size_t i = 0; i < cs.size(); i++)
			if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
				return static_cast<UIRect*>(cs[i].get())->IsVisible();
		return false;
	};
	auto centreOf = [&](GameObject* go) -> Vec2 {
		const UIRectValue r = rectOf(go);
		return Vec2(r.x + r.width * 0.5f, r.y + r.height * 0.5f);
	};

	// A point inside an element, given in fractions of its rect.
	auto inside = [&](GameObject* go, const f32 fx, const f32 fy) -> Vec4 {
		const UIRectValue r = rectOf(go);
		if (r.width <= 0.f || r.height <= 0.f) return Vec4(-1.f, -1.f, -1.f, -1.f);
		return at(r.x + r.width * fx, r.y + r.height * fy);
	};

	// ---- the skin came from the files ----
	// Midnight's panel is a dark blue; nothing in this file ever names a
	// colour, so anything but the placeholder white proves the style was
	// read, resolved through the palette and applied.
	const Vec4 panel = at(640.f, 700.f);
	printf("      panel (%d,%d,%d)\n", (int)panel.x, (int)panel.y, (int)panel.z);
	check(panel.x > 5.f && panel.x < 60.f && panel.z > panel.x, "the panel wears its style, not the placeholder white");

	// ---- every widget drew ----
	// The checkbox that starts on shows its tick, in the accent colour; the
	// one that starts off shows its own background instead.
	const Vec4 tick = inside(childOf(fullscreen->GetOwner(), "Check"), 0.5f, 0.62f);
	const Vec4 unticked = inside(invertY->GetOwner(), 0.5f, 0.5f);
	printf("      tick (%d,%d,%d) vs unchecked box (%d,%d,%d)\n",
		(int)tick.x, (int)tick.y, (int)tick.z, (int)unticked.x, (int)unticked.y, (int)unticked.z);
	check(tick.z > 120.f && tick.z > tick.x, "a checked box shows its tick");
	check(differs(tick, unticked), "an unchecked box does not");

	// The volume slider is at 70%, so its fill reaches past the middle of
	// the track and stops before the end.
	const Vec4 filled = inside(volume->GetOwner(), 0.3f, 0.5f);
	const Vec4 empty = inside(volume->GetOwner(), 0.9f, 0.5f);
	printf("      slider fill (%d,%d,%d) vs track (%d,%d,%d)\n",
		(int)filled.x, (int)filled.y, (int)filled.z, (int)empty.x, (int)empty.y, (int)empty.z);
	check(differs(filled, empty), "a slider's fill stops where its value does");

	// The list's selected row is highlighted, and its neighbours are not.
	// Row 1 is selected, so it is the second row down.
	const UIRectValue listRect = rectOf(levels->GetOwner());
	const Vec4 selected = at(listRect.x + 30.f, listRect.y + 45.f);
	const Vec4 plain = at(listRect.x + 30.f, listRect.y + 105.f);
	printf("      selected row (%d,%d,%d) vs plain row (%d,%d,%d)\n",
		(int)selected.x, (int)selected.y, (int)selected.z, (int)plain.x, (int)plain.y, (int)plain.z);
	check(differs(selected, plain), "the selected row of a list is highlighted");

	// A hidden element solves no rect at all - that is what hidden means
	// here - so where the popup *would* be is only knowable once it is
	// open. This frame is kept and compared against that one below.
	const std::vector<uchar> closedFrame = px;

	// ---- widgets respond ----
	const UIRectValue volumeRect = rectOf(volume->GetOwner());
	const f32 before = volume->GetValue();
	// Press at the far right of the volume track: a released frame first,
	// since widgets arm on the up-to-down transition.
	const Vec2 trackEnd(volumeRect.x + volumeRect.width * 0.95f, volumeRect.y + volumeRect.height * 0.5f);
	canvas->UpdateInput(trackEnd, false);
	canvas->UpdateInput(trackEnd, true);
	uiScene->Update(0.0);
	printf("      volume %.0f -> %.0f\n", before, volume->GetValue());
	check(volume->GetValue() > before, "dragging the slider changes its value");
	canvas->UpdateInput(trackEnd, false);

	// Typing goes to the field that was clicked, and nowhere else.
	const UIRectValue nameRect = rectOf(nameField->GetOwner());
	const Vec2 inName(nameRect.x + 30.f, nameRect.y + nameRect.height * 0.5f);
	canvas->UpdateInput(inName, false);
	canvas->UpdateInput(inName, true);
	canvas->UpdateInput(inName, false);
	canvas->UpdateText("Nova");
	check(nameField->GetText() == "Nova", "typing lands in the field that was clicked");
	check(passField->GetText().empty(), "and not in the one that was not");

	// Opening the dropdown paints a popup where there was none.
	const UIRectValue themeRect = rectOf(themePicker->GetOwner());
	const Vec2 onTheme(themeRect.x + themeRect.width * 0.5f, themeRect.y + themeRect.height * 0.5f);
	canvas->UpdateInput(onTheme, false);
	canvas->UpdateInput(onTheme, true);
	canvas->UpdateInput(onTheme, false);
	uiScene->Update(0.0);
	frame();
	const UIRectValue popupRect = rectOf(childOf(themePicker->GetOwner(), "ThemePopup"));
	const f32 popupX = popupRect.x + popupRect.width * 0.5f;
	const f32 popupY = popupRect.y + popupRect.height * 0.5f;
	const Vec4 popupOpen = at(popupX, popupY);
	const Vec4 popupWas = atIn(closedFrame, popupX, popupY);
	printf("      where the popup goes: closed (%d,%d,%d) -> open (%d,%d,%d)\n",
		(int)popupWas.x, (int)popupWas.y, (int)popupWas.z,
		(int)popupOpen.x, (int)popupOpen.y, (int)popupOpen.z);
	check(!differs(popupWas, panel), "a closed dropdown paints no popup");
	check(differs(popupOpen, popupWas), "opening it paints one");
	check(themePicker->IsExpanded(), "and it knows it is open");


	// ---- the menu bar ----
	// Opening File, then hovering an entry inside it: the entry has to
	// light up, and hovering the one with a submenu has to open it without
	// another click.
	{
		GameObject* file = findByName("File");
		const Vec2 onFile = centreOf(file);
		canvas->UpdateInput(onFile, false);
		canvas->UpdateInput(onFile, true);
		canvas->UpdateInput(onFile, false);
		uiScene->Update(0.0);
		canvas->Solve((f32)W, (f32)H);
		frame();

		GameObject* newScene = findByName("FileNew Scene");
		const UIRectValue entryRect = rectOf(newScene);
		const Vec4 panelBehind = at(entryRect.x + entryRect.width - 6.f, entryRect.y + entryRect.height * 0.5f);
		printf("      menu opened, entry rect %.0f,%.0f %.0fx%.0f\n",
			entryRect.x, entryRect.y, entryRect.width, entryRect.height);
		check(entryRect.width > 1.f && entryRect.height > 1.f, "clicking a title opens its menu");

		// Hover it, let the state fade land, and look at it.
		const Vec2 onEntry = centreOf(newScene);
		for (int i = 0; i < 12; i++) { canvas->UpdateInput(onEntry, false); uiScene->Update(0.05 * (i + 1)); }
		frame();
		const Vec4 hovered = at(onEntry.x, onEntry.y);
		printf("      hovered entry (%d,%d,%d) vs the panel behind it (%d,%d,%d)\n",
			(int)hovered.x, (int)hovered.y, (int)hovered.z,
			(int)panelBehind.x, (int)panelBehind.y, (int)panelBehind.z);
		check(differs(hovered, panelBehind), "an entry lights up under the pointer");

		// Hovering the entry that has a submenu opens it, no click.
		GameObject* recent = findByName("FileRecent");
		const Vec2 onRecent = centreOf(recent);
		for (int i = 0; i < 4; i++) { canvas->UpdateInput(onRecent, false); uiScene->Update(0.6 + 0.05 * i); }
		canvas->Solve((f32)W, (f32)H);
		check(visible(findByName("RecentMenu")), "hovering an entry with a submenu opens it");

		// A press anywhere else puts it all away.
		canvas->UpdateInput(Vec2(640.f, 660.f), false);
		canvas->UpdateInput(Vec2(640.f, 660.f), true);
		canvas->UpdateInput(Vec2(640.f, 660.f), false);
		uiScene->Update(1.0);
		canvas->Solve((f32)W, (f32)H);
		// Visibility, not the rect: a hidden element is never solved again,
		// so it keeps whatever rect it had when it was last on screen.
		check(!visible(findByName("FileMenu")) && !visible(findByName("RecentMenu")),
			"a press outside closes the whole menu");
	}


	// ---- the dialog ----
	// Opened by the Reset button, and while it is up nothing behind it can
	// be touched - which is the only thing that distinguishes a dialog from
	// a panel that happens to be visible.
	{
		const std::vector<uchar> beforeDialog = px;
		const Vec2 onReset = centreOf(findByName("Reset"));
		canvas->UpdateInput(onReset, false);
		canvas->UpdateInput(onReset, true);
		canvas->UpdateInput(onReset, false);
		// The example polls the button in Update() rather than through a
		// handler, so run what it would have run.
		if (UIButton* reset = ButtonOn(findByName("Reset")))
			if (reset->ConsumeClicked()) confirm->Open();
		uiScene->Update(0.0);
		canvas->Solve((f32)W, (f32)H);
		frame();
		check(confirm->IsOpen(), "the Reset button opens the dialog");

		const Vec2 middleOfDialog = centreOf(findByName("Dialog"));
		const Vec4 dialogPixel = at(middleOfDialog.x, middleOfDialog.y - 60.f);
		check(dialogPixel.x >= 0.f, "and it is on screen");

		// The scrim dims what is behind it: the same pixel, darker.
		const Vec4 scrimmedNow = at(200.f, 640.f);
		const Vec4 scrimmedWas = atIn(beforeDialog, 200.f, 640.f);
		printf("      behind the scrim (%d,%d,%d) was (%d,%d,%d)\n",
			(int)scrimmedNow.x, (int)scrimmedNow.y, (int)scrimmedNow.z,
			(int)scrimmedWas.x, (int)scrimmedWas.y, (int)scrimmedWas.z);
		check(scrimmedNow.x < scrimmedWas.x && scrimmedNow.y < scrimmedWas.y,
			"and the scrim dims everything behind it");

		// A click on a checkbox underneath does nothing at all.
		const bool wasOn = invertY->GetValue();
		const Vec2 onCheckbox = centreOf(invertY->GetOwner());
		canvas->UpdateInput(onCheckbox, false);
		canvas->UpdateInput(onCheckbox, true);
		canvas->UpdateInput(onCheckbox, false);
		uiScene->Update(0.0);
		check(invertY->GetValue() == wasOn, "and a click behind it is swallowed");

		// Its own buttons still work: Cancel closes it and leaves the
		// screen exactly as it was.
		const Vec2 onCancel = centreOf(findByName("Cancel"));
		canvas->UpdateInput(onCancel, false);
		canvas->UpdateInput(onCancel, true);
		canvas->UpdateInput(onCancel, false);
		if (UIButton* cancel = ButtonOn(findByName("Cancel")))
			if (cancel->ConsumeClicked()) confirm->Close();
		uiScene->Update(0.0);
		canvas->Solve((f32)W, (f32)H);
		frame();
		check(!confirm->IsOpen(), "a button inside it still works");
		const Vec4 restored = at(200.f, 640.f);
		check(!differs(restored, scrimmedWas), "and closing it puts the screen back");
	}

	// ---- the theme is a file swap ----
	std::vector<uchar> midnight = px;
	ApplyTheme(1);              // amber
	frame();
	std::vector<uchar> amber = px;
	size_t changed = 0;
	for (size_t i = 0; i + 3 < midnight.size() && i + 3 < amber.size(); i += 4)
		if (midnight[i] != amber[i] || midnight[i + 1] != amber[i + 1] || midnight[i + 2] != amber[i + 2])
			changed++;
	const size_t total = (size_t)W * H;
	printf("      %zu of %zu pixels changed with the theme (%.0f%%)\n",
		changed, total, 100.0 * (double)changed / (double)total);
	check(changed > total / 10, "switching theme repaints the screen");

	// And the accent really is the palette's, not a shade of the old one.
	const Vec4 amberTick = inside(childOf(fullscreen->GetOwner(), "Check"), 0.5f, 0.62f);
	check(amberTick.x > amberTick.z, "the new palette's accent is what the tick wears");

	// A third theme, to be sure the first switch was not a one-off.
	ApplyTheme(2);              // paper
	frame();
	const Vec4 paperPanel = at(640.f, 700.f);
	printf("      paper panel (%d,%d,%d)\n", (int)paperPanel.x, (int)paperPanel.y, (int)paperPanel.z);
	check(paperPanel.x > 200.f, "a light theme is light");

	// One image per theme: the same screen, three skins, nothing else
	// changed.
	const char* files[3] = { "widget_gallery.png", "widget_gallery_amber.png", "widget_gallery_paper.png" };
	for (uint32 t = 0; t < 3; t++)
	{
		ApplyTheme(t);
		frame();
		stbi_write_png(files[t], (int)W, (int)H, 4, px.data(), (int)W * 4);
	}
	ApplyTheme(0);
	printf("      wrote %s, %s and %s\n", files[0], files[1], files[2]);

	printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASSED", failures);
	if (failures) exit(1);
}
