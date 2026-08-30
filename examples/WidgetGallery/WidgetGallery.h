//============================================================================
// Name        : WidgetGallery.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Every widget the UI has, under three interchangeable skins.
//
//                Buttons, checkboxes, a radio group, sliders, text fields, a
//                scrolling list and a dropdown - built from the same
//                UIRect/UIImage/UIText elements an author would use, and
//                skinned entirely from files: assets/ui/*.png for the art
//                and assets/ui/styles/*.uistyle for the look, resolved
//                against one of three palettes.
//
//                Nothing in this file chooses a colour. Every element names
//                a style, every style names palette entries, and switching
//                theme re-applies the same styles against a different
//                palette - which is the whole point of the split: a re-skin
//                is one file, not a hunt through the code that built the
//                screen.
//
//                Run with PYROS_UI_VERIFY=1 to render offscreen, assert
//                that every widget drew and responded and that switching
//                theme actually repainted it, write widget_gallery.png,
//                then exit.
//============================================================================

#ifndef WIDGETGALLERY_H
#define	WIDGETGALLERY_H

#include "../BaseExample/BaseExample.h"

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/UIRenderer/UIRenderer.h>
#include <Pyros3D/Rendering/Components/UI/UICanvas.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Rendering/Components/UI/UIImage.h>
#include <Pyros3D/Rendering/Components/UI/UIText.h>
#include <Pyros3D/Rendering/Components/UI/UIButton.h>
#include <Pyros3D/Rendering/Components/UI/UIToggle.h>
#include <Pyros3D/Rendering/Components/UI/UISlider.h>
#include <Pyros3D/Rendering/Components/UI/UIInput.h>
#include <Pyros3D/Rendering/Components/UI/UIList.h>
#include <Pyros3D/Rendering/Components/UI/UIDropdown.h>
#include <Pyros3D/Assets/Font/Font.h>
#include <memory>
#include <string>
#include <vector>

using namespace p3d;

class WidgetGallery : public BaseExample {

public:

	WidgetGallery();
	virtual ~WidgetGallery();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void DrawUI() {}

	// Characters arrive from the window layer rather than being decoded
	// here - see TextInputHook.h.
	static void OnTextTyped(const char* utf8);

private:

	// ---- building blocks ----
	// A styled element: a rect, a name, and the style it wears. Everything
	// on this screen is one of these, which is what keeps the layout code
	// about layout.
	std::shared_ptr<GameObject> Element(const std::shared_ptr<GameObject> &parent,
		const std::string &name, const Vec2 &anchorMin, const Vec2 &anchorMax,
		const Vec2 &offsetMin, const Vec2 &offsetMax, const std::string &style = std::string());
	std::shared_ptr<UIImage> Image(const std::shared_ptr<GameObject> &on);
	std::shared_ptr<UIText> Label(const std::shared_ptr<GameObject> &on, const std::string &text);

	// ---- the widgets ----
	std::shared_ptr<GameObject> Button(const std::shared_ptr<GameObject> &parent, const std::string &name,
		const Vec2 &offsetMin, const Vec2 &offsetMax, const std::string &text, bool primary);
	std::shared_ptr<UIToggle> Checkbox(const std::shared_ptr<GameObject> &parent, const std::string &name,
		const Vec2 &at, const std::string &text, const std::string &group);
	std::shared_ptr<UISlider> Slider(const std::shared_ptr<GameObject> &parent, const std::string &name,
		const Vec2 &offsetMin, const Vec2 &offsetMax, bool vertical);
	std::shared_ptr<UIInput> Field(const std::shared_ptr<GameObject> &parent, const std::string &name,
		const Vec2 &offsetMin, const Vec2 &offsetMax, const std::string &placeholder, bool password);
	std::shared_ptr<UIList> List(const std::shared_ptr<GameObject> &parent, const std::string &name,
		const Vec2 &offsetMin, const Vec2 &offsetMax, const uint32 rows, const f32 rowHeight);
	std::shared_ptr<UIDropdown> Dropdown(const std::shared_ptr<GameObject> &parent, const std::string &name,
		const Vec2 &offsetMin, const Vec2 &offsetMax);

	// Re-resolves every style against the named palette and applies it. The
	// only thing theme switching does.
	void ApplyTheme(const uint32 index);
	void RunVerification();
	// PYROS_UI_SHOW=1: a few frames of the widgets being used, written out
	// as images - for looking at, on a machine whose screen cannot be
	// captured.
	void RunShowcase();

	std::string AssetRoot() const;

	UIRenderer* uiRenderer;
	SceneGraph* uiScene;
	std::shared_ptr<GameObject> canvasGO;
	std::shared_ptr<UICanvas> canvas;
	std::shared_ptr<Font> font, fontSmall, fontTitle;

	// The widgets the demo reads back from, to show their values.
	std::shared_ptr<UISlider> volume, brightness, balance;
	std::shared_ptr<UIInput> nameField, passField;
	std::shared_ptr<UIList> levels;
	std::shared_ptr<UIDropdown> themePicker;
	std::shared_ptr<UIToggle> fullscreen, invertY;
	std::shared_ptr<UIText> volumeValue, brightnessValue, balanceValue, status;

	std::vector<std::string> themes;
	uint32 theme;
	bool verifyMode;
	bool verified;

	// The one canvas that is taking typed characters, so the static hook
	// has somewhere to put them.
	static WidgetGallery* active;
};

#endif	/* WIDGETGALLERY_H */
