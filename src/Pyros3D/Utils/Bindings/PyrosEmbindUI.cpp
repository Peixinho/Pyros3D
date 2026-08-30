//============================================================================
// Name        : PyrosEmbindUI.cpp
// Description : Embind screen-space UI - the same surface the Lua bindings
//               expose (PyrosLuaUI.cpp), so a scene's UI behaves identically
//               whether the host is a native build or a browser.
//
//               Deliberately the same shape and the same narrowness: a
//               script's business with a UI is finding an element by name
//               and changing what it says or how it looks. Laying one out
//               belongs in the editor, where the result is visible, so
//               there is no way to build a hierarchy from here either.
//============================================================================

#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

#include <emscripten/bind.h>

#include <Pyros3D/Utils/Bindings/PyrosEmbindHelpers.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/UIRenderer/UIRenderer.h>
#include <Pyros3D/Rendering/Components/UI/UICanvas.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Rendering/Components/UI/UIImage.h>
#include <Pyros3D/Rendering/Components/UI/UIText.h>
#include <Pyros3D/Rendering/Components/UI/UIButton.h>

#include <memory>
#include <string>
#include <vector>

using namespace emscripten;
using namespace p3d;
using namespace p3d::Math;

namespace {

	template <typename T>
	T* FindComponent(GameObject* go, const uint32 type)
	{
		if (!go) return NULL;
		const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
		for (size_t i = 0; i < cs.size(); i++)
			if (cs[i] && cs[i]->GetComponentType() == type) return static_cast<T*>(cs[i].get());
		return NULL;
	}

	GameObject* FindByNameInSubtree(GameObject* node, const std::string &name)
	{
		if (!node) return NULL;
		if (node->GetName() == name) return node;
		const std::vector<std::shared_ptr<GameObject> > &kids = node->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
			if (GameObject* found = FindByNameInSubtree(kids[i].get(), name)) return found;
		return NULL;
	}

	// Every canvas in the scene, not just one: a game has a HUD and a menu,
	// and a script asking for "ScoreLabel" should not have to know which of
	// them it lives on.
	GameObject* UI_Find(SceneGraph &scene, const std::string &name)
	{
		std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(&scene);
		for (size_t i = 0; i < canvases.size(); i++)
		{
			GameObject* owner = canvases[i]->GetOwner();
			if (!owner) continue;
			const std::vector<std::shared_ptr<GameObject> > &kids = owner->GetChildren();
			for (size_t k = 0; k < kids.size(); k++)
				if (GameObject* found = FindByNameInSubtree(kids[k].get(), name)) return found;
		}
		return NULL;
	}

	void UI_SetText(GameObject* go, const std::string &text)
	{
		if (UIText* t = FindComponent<UIText>(go, ComponentType::UIText)) t->SetText(text);
	}
	std::string UI_GetText(GameObject* go)
	{
		UIText* t = FindComponent<UIText>(go, ComponentType::UIText);
		return t ? t->GetText() : std::string();
	}
	void UI_SetTextColor(GameObject* go, const Vec4 &c)
	{
		if (UIText* t = FindComponent<UIText>(go, ComponentType::UIText)) t->SetColor(c);
	}
	void UI_SetTint(GameObject* go, const Vec4 &c)
	{
		if (UIImage* i = FindComponent<UIImage>(go, ComponentType::UIImage)) i->SetTint(c);
	}

	// Per-component-active rather than removing the object from the scene:
	// the layout stays solved, so showing it again costs nothing and cannot
	// lose its place.
	void UI_SetVisible(GameObject* go, bool visible)
	{
		if (!go) return;
		const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
		for (size_t i = 0; i < cs.size(); i++)
		{
			if (!cs[i]) continue;
			const uint32 t = cs[i]->GetComponentType();
			if (t != ComponentType::UIImage && t != ComponentType::UIText) continue;
			if (visible) cs[i]->Enable(); else cs[i]->Disable();
		}
		const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
		for (size_t i = 0; i < kids.size(); i++) UI_SetVisible(kids[i].get(), visible);
	}

	// A bar or a fill: 0..1 of its parent's width, by moving one anchor.
	void UI_SetFill(GameObject* go, const f32 fraction)
	{
		UIRect* r = FindComponent<UIRect>(go, ComponentType::UIRect);
		if (!r) return;
		const f32 f = fraction < 0.f ? 0.f : (fraction > 1.f ? 1.f : fraction);
		r->SetAnchors(r->GetAnchorMin(), Vec2(f, r->GetAnchorMax().y));
	}

	void UI_SetInteractable(GameObject* go, bool on)
	{
		if (UIButton* b = FindComponent<UIButton>(go, ComponentType::UIButton)) b->SetInteractable(on);
	}
	bool UI_WasClicked(GameObject* go)
	{
		UIButton* b = FindComponent<UIButton>(go, ComponentType::UIButton);
		return b ? b->ConsumeClicked() : false;
	}

	// Pointer and navigation. Names out rather than pointers, because a name
	// is what a script already thinks in - and because handing raw
	// GameObject* to JS is a lifetime problem nobody asked for.
	std::string UI_UpdateInput(SceneGraph &scene, const f32 x, const f32 y, const bool down)
	{
		std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(&scene);
		// Topmost canvas first, so a menu over a HUD swallows the click.
		for (size_t i = canvases.size(); i > 0; i--)
		{
			const UIRectValue &r = canvases[i - 1]->GetCanvasRect();
			if (r.width <= 0.f || r.height <= 0.f) continue;
			if (GameObject* hit = canvases[i - 1]->UpdateInput(Vec2(x, y), down))
				return hit->GetName();
		}
		return std::string();
	}

	std::string UI_MoveFocus(SceneGraph &scene, const f32 dx, const f32 dy)
	{
		std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(&scene);
		for (size_t i = canvases.size(); i > 0; i--)
			if (GameObject* go = canvases[i - 1]->MoveFocus(Vec2(dx, dy)))
				return go->GetName();
		return std::string();
	}
	std::string UI_FocusFirst(SceneGraph &scene)
	{
		std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(&scene);
		for (size_t i = canvases.size(); i > 0; i--)
			if (GameObject* go = canvases[i - 1]->FocusFirst()) return go->GetName();
		return std::string();
	}
	std::string UI_ActivateFocused(SceneGraph &scene)
	{
		std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(&scene);
		for (size_t i = canvases.size(); i > 0; i--)
			if (GameObject* go = canvases[i - 1]->ActivateFocused()) return go->GetName();
		return std::string();
	}
	void UI_ClearFocus(SceneGraph &scene)
	{
		std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(&scene);
		for (size_t i = 0; i < canvases.size(); i++) canvases[i]->ClearFocus();
	}

	// Canvas units for a pointer given in CSS pixels - the browser's
	// coordinates are never the canvas's.
	Vec2 UI_ScreenToCanvas(SceneGraph &scene, const f32 x, const f32 y,
		const f32 screenWidth, const f32 screenHeight)
	{
		if (screenWidth <= 0.f || screenHeight <= 0.f) return Vec2(x, y);
		std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(&scene);
		if (canvases.empty()) return Vec2(x, y);
		const UIRectValue &r = canvases.back()->GetCanvasRect();
		return Vec2(x / screenWidth * r.width, y / screenHeight * r.height);
	}

	void UIRenderer_RenderUI(UIRenderer &r, SceneGraph &scene) { r.RenderUI(&scene); }

} // namespace

namespace p3d {
	void PyrosEmbindUIForceLink() {}
}

EMSCRIPTEN_BINDINGS(pyros3d_ui)
{
	// A plain constructor, matching how ForwardRenderer/DeferredRenderer are
	// bound. Wrapping it in a shared_ptr factory looked tidier and produced a
	// "function signature mismatch" at call time: a class with no
	// smart_ptr-based .constructor cannot be handed one.
	class_<UIRenderer>("UIRenderer")
		.constructor<const uint32, const uint32>()
		.function("resize", &UIRenderer::Resize)
		.function("renderUI", &UIRenderer_RenderUI);

	// Free functions rather than a namespace object: Embind has no namespaces,
	// and uiFind/uiSetText reads the same as ui.find/ui.setText does in Lua.
	emscripten::function("uiFind", &UI_Find, allow_raw_pointers());
	emscripten::function("uiSetText", &UI_SetText, allow_raw_pointers());
	emscripten::function("uiGetText", &UI_GetText, allow_raw_pointers());
	emscripten::function("uiSetTextColor", &UI_SetTextColor, allow_raw_pointers());
	emscripten::function("uiSetTint", &UI_SetTint, allow_raw_pointers());
	emscripten::function("uiSetVisible", &UI_SetVisible, allow_raw_pointers());
	emscripten::function("uiSetFill", &UI_SetFill, allow_raw_pointers());
	emscripten::function("uiSetInteractable", &UI_SetInteractable, allow_raw_pointers());
	emscripten::function("uiWasClicked", &UI_WasClicked, allow_raw_pointers());
	emscripten::function("uiUpdateInput", &UI_UpdateInput);
	emscripten::function("uiScreenToCanvas", &UI_ScreenToCanvas);
	emscripten::function("uiMoveFocus", &UI_MoveFocus);
	emscripten::function("uiFocusFirst", &UI_FocusFirst);
	emscripten::function("uiActivateFocused", &UI_ActivateFocused);
	emscripten::function("uiClearFocus", &UI_ClearFocus);
}

#endif
