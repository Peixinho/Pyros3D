//============================================================================
// Name        : PyrosLuaUI.cpp
// Description : Screen-space UI - canvases, rects, images, text, buttons.
//
//               Deliberately narrow. A script's business with a UI is
//               finding an element by name and changing what it says or how
//               it looks; laying one out belongs in the editor, where the
//               result is visible. So there is a find, a handful of
//               setters, and no way to build a hierarchy from Lua at all.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>
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
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/UIRenderer/UIRenderer.h>

namespace p3d {

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

		// Every canvas in the scene, not just one: a game has a HUD and a
		// menu, and a script asking for "ScoreLabel" should not have to know
		// which of them it lives on.
		GameObject* UI_Find(SceneGraph* scene, const std::string &name)
		{
			if (!scene) return NULL;
			std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(scene);
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

		// Show/hide is per-component-active rather than removing the object
		// from the scene: the layout stays solved, so showing it again costs
		// nothing and cannot lose its place.
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
		// This is the single most common thing a HUD script does, and doing
		// it by hand means knowing that offsetMax is an inset.
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
		// ---- the rest of the widget set ----
		// A script's business with a widget is its value: what the slider is
		// set to, what the player typed, which row they picked. Layout and
		// look stay in the editor, same as everything above.

		// Toggles and sliders both answer to a value, because a script
		// asking "what is this control set to" should not have to know which
		// kind it is. A toggle reads back 0 or 1.
		f32 UI_GetValue(GameObject* go)
		{
			if (UISlider* s = FindComponent<UISlider>(go, ComponentType::UISlider)) return s->GetValue();
			if (UIToggle* t = FindComponent<UIToggle>(go, ComponentType::UIToggle)) return t->GetValue() ? 1.f : 0.f;
			return 0.f;
		}
		void UI_SetValue(GameObject* go, const f32 value)
		{
			if (UISlider* s = FindComponent<UISlider>(go, ComponentType::UISlider)) { s->SetValue(value); return; }
			if (UIToggle* t = FindComponent<UIToggle>(go, ComponentType::UIToggle)) t->SetValue(value != 0.f);
		}
		void UI_SetRange(GameObject* go, const f32 minimum, const f32 maximum)
		{
			if (UISlider* s = FindComponent<UISlider>(go, ComponentType::UISlider)) s->SetRange(minimum, maximum);
		}

		// Fields, lists and dropdowns all have a selection or a value that
		// reads as text - again, so a script does not have to branch on the
		// kind of thing it found.
		std::string UI_GetValueText(GameObject* go)
		{
			if (UIInput* in = FindComponent<UIInput>(go, ComponentType::UIInput)) return in->GetText();
			if (UIList* l = FindComponent<UIList>(go, ComponentType::UIList)) return l->GetSelectedItem();
			if (UIDropdown* d = FindComponent<UIDropdown>(go, ComponentType::UIDropdown)) return d->GetSelectedOption();
			return std::string();
		}
		void UI_SetValueText(GameObject* go, const std::string &text)
		{
			if (UIInput* in = FindComponent<UIInput>(go, ComponentType::UIInput)) in->SetText(text);
		}

		int32 UI_GetSelected(GameObject* go)
		{
			if (UIList* l = FindComponent<UIList>(go, ComponentType::UIList)) return l->GetSelected();
			if (UIDropdown* d = FindComponent<UIDropdown>(go, ComponentType::UIDropdown)) return d->GetSelected();
			return -1;
		}
		void UI_SetSelected(GameObject* go, const int32 index)
		{
			if (UIList* l = FindComponent<UIList>(go, ComponentType::UIList)) { l->SetSelected(index); return; }
			if (UIDropdown* d = FindComponent<UIDropdown>(go, ComponentType::UIDropdown)) d->SetSelected(index);
		}

		// The contents of a list or a dropdown, as a Lua array. This is the
		// one place a script legitimately builds part of a UI: a list of
		// save games or servers is not something the editor can author.
		void UI_SetItems(GameObject* go, sol::table items)
		{
			std::vector<std::string> values;
			for (size_t i = 1; i <= items.size(); i++)
			{
				sol::optional<std::string> v = items[i];
				if (v) values.push_back(*v);
			}
			if (UIList* l = FindComponent<UIList>(go, ComponentType::UIList)) { l->SetItems(values); return; }
			if (UIDropdown* d = FindComponent<UIDropdown>(go, ComponentType::UIDropdown)) d->SetOptions(values);
		}
		sol::table UI_GetItems(GameObject* go, sol::this_state ts)
		{
			sol::state_view lua(ts);
			sol::table out = lua.create_table();
			const std::vector<std::string>* values = NULL;
			if (UIList* l = FindComponent<UIList>(go, ComponentType::UIList)) values = &l->GetItems();
			else if (UIDropdown* d = FindComponent<UIDropdown>(go, ComponentType::UIDropdown)) values = &d->GetOptions();
			if (values)
				for (size_t i = 0; i < values->size(); i++) out[i + 1] = (*values)[i];
			return out;
		}

		void UI_SetExpanded(GameObject* go, const bool on)
		{
			if (UIDropdown* d = FindComponent<UIDropdown>(go, ComponentType::UIDropdown)) d->SetExpanded(on);
		}

		// Text typed by the player, and the keys a field or a list wants.
		// Both go to whatever has focus, which is the canvas's business.
		void UI_Type(SceneGraph* scene, const std::string &utf8)
		{
			if (!scene) return;
			std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(scene);
			for (size_t i = canvases.size(); i > 0; i--)
				if (canvases[i - 1]->GetFocused()) { canvases[i - 1]->UpdateText(utf8); return; }
		}
		bool UI_Key(SceneGraph* scene, const uint32 key)
		{
			if (!scene) return false;
			std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(scene);
			for (size_t i = canvases.size(); i > 0; i--)
				if (canvases[i - 1]->GetFocused()) return canvases[i - 1]->UpdateKey(key);
			return false;
		}
		void UI_Scroll(SceneGraph* scene, const f32 x, const f32 y, const f32 delta)
		{
			if (!scene) return;
			std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(scene);
			for (size_t i = canvases.size(); i > 0; i--)
			{
				const UIRectValue &r = canvases[i - 1]->GetCanvasRect();
				if (r.width <= 0.f || r.height <= 0.f) continue;
				canvases[i - 1]->UpdateScroll(Vec2(x, y), delta);
				return;
			}
		}

		// What changed this frame, as {name, event} pairs - the polling half
		// of the onChange handlers, for a script that would rather ask than
		// be called. Names, not pointers, for the same reason UI_UpdateInput
		// returns one.
		sol::table UI_Events(SceneGraph* scene, sol::this_state ts)
		{
			sol::state_view lua(ts);
			sol::table out = lua.create_table();
			if (!scene) return out;
			std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(scene);
			uint32 n = 1;
			for (size_t i = 0; i < canvases.size(); i++)
			{
				const std::vector<UICanvas::WidgetEvent> &events = canvases[i]->GetEvents();
				for (size_t e = 0; e < events.size(); e++)
				{
					if (!events[e].node) continue;
					sol::table entry = lua.create_table();
					entry["element"] = events[e].node->GetName();
					entry["changed"] = (events[e].flags & UIEventFlag::Changed) != 0;
					entry["clicked"] = (events[e].flags & UIEventFlag::Clicked) != 0;
					entry["submitted"] = (events[e].flags & UIEventFlag::Submitted) != 0;
					out[n++] = entry;
				}
			}
			return out;
		}

		bool UI_WasClicked(GameObject* go)
		{
			UIButton* b = FindComponent<UIButton>(go, ComponentType::UIButton);
			return b ? b->ConsumeClicked() : false;
		}
	}

	namespace {
		// Pointer-in, name-out. Lua has no use for a GameObject* it cannot do
		// anything with, and the name is what a script already thinks in.
		std::string UI_UpdateInput(SceneGraph* scene, const f32 x, const f32 y, const bool down)
		{
			if (!scene) return std::string();
			std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(scene);
			// Topmost canvas first, so a menu over a HUD swallows the click
			// rather than both acting on it.
			for (size_t i = canvases.size(); i > 0; i--)
			{
				const UIRectValue &r = canvases[i - 1]->GetCanvasRect();
				if (r.width <= 0.f || r.height <= 0.f) continue;
				if (GameObject* hit = canvases[i - 1]->UpdateInput(Vec2(x, y), down))
					return hit->GetName();
			}
			return std::string();
		}

		// Direction in canvas space: up is (0, -1). Returns the newly focused
		// element's name, or "" when nothing moved.
		std::string UI_MoveFocus(SceneGraph* scene, const f32 dx, const f32 dy)
		{
			if (!scene) return std::string();
			std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(scene);
			// Topmost canvas takes navigation, for the same reason it takes
			// the pointer: a pause menu over a HUD owns the input.
			for (size_t i = canvases.size(); i > 0; i--)
				if (GameObject* go = canvases[i - 1]->MoveFocus(Vec2(dx, dy)))
					return go->GetName();
			return std::string();
		}

		std::string UI_FocusFirst(SceneGraph* scene)
		{
			if (!scene) return std::string();
			std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(scene);
			for (size_t i = canvases.size(); i > 0; i--)
				if (GameObject* go = canvases[i - 1]->FocusFirst())
					return go->GetName();
			return std::string();
		}

		std::string UI_ActivateFocused(SceneGraph* scene)
		{
			if (!scene) return std::string();
			std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(scene);
			for (size_t i = canvases.size(); i > 0; i--)
				if (GameObject* go = canvases[i - 1]->ActivateFocused())
					return go->GetName();
			return std::string();
		}

		void UI_ClearFocus(SceneGraph* scene)
		{
			if (!scene) return;
			std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(scene);
			for (size_t i = 0; i < canvases.size(); i++) canvases[i]->ClearFocus();
		}

		Vec2 UI_ScreenToCanvas(SceneGraph* scene, const f32 x, const f32 y,
			const f32 screenWidth, const f32 screenHeight)
		{
			if (!scene || screenWidth <= 0.f || screenHeight <= 0.f) return Vec2(x, y);
			std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(scene);
			if (canvases.empty()) return Vec2(x, y);
			const UIRectValue &r = canvases.back()->GetCanvasRect();
			return Vec2(x / screenWidth * r.width, y / screenHeight * r.height);
		}
	}

	void RegisterLuaUI(sol::state* lua)
	{
		// The pass itself, so a Lua-driven host (the DemoLauncher's
		// render_host) can composite UI the same way the editor and the
		// player do. Nothing else in the engine needs to be told about it -
		// it draws whatever canvases the scene has.
		{
			sol::constructors<sol::types<uint32, uint32>> con;
			lua->new_usertype<UIRenderer>("UIRenderer",
				con,
				"resize", &UIRenderer::Resize,
				"renderUI", &UIRenderer::RenderUI);
		}

		sol::table ui = lua->create_named_table("ui");
		ui.set_function("updateInput", &UI_UpdateInput);
		ui.set_function("screenToCanvas", &UI_ScreenToCanvas);
		ui.set_function("moveFocus", &UI_MoveFocus);
		ui.set_function("focusFirst", &UI_FocusFirst);
		ui.set_function("activateFocused", &UI_ActivateFocused);
		ui.set_function("clearFocus", &UI_ClearFocus);
		ui.set_function("find", &UI_Find);
		ui.set_function("setText", &UI_SetText);
		ui.set_function("getText", &UI_GetText);
		ui.set_function("setTextColor", &UI_SetTextColor);
		ui.set_function("setTint", &UI_SetTint);
		ui.set_function("setVisible", &UI_SetVisible);
		ui.set_function("setFill", &UI_SetFill);
		ui.set_function("setInteractable", &UI_SetInteractable);
		// Polled as an alternative to the button's onClick handler, for a
		// script that would rather ask than be called.
		ui.set_function("wasClicked", &UI_WasClicked);

		// The rest of the widget set. Values first, because that is what a
		// script actually wants from a control.
		ui.set_function("getValue", &UI_GetValue);
		ui.set_function("setValue", &UI_SetValue);
		ui.set_function("setRange", &UI_SetRange);
		ui.set_function("getValueText", &UI_GetValueText);
		ui.set_function("setValueText", &UI_SetValueText);
		ui.set_function("getSelected", &UI_GetSelected);
		ui.set_function("setSelected", &UI_SetSelected);
		ui.set_function("getItems", &UI_GetItems);
		ui.set_function("setItems", &UI_SetItems);
		ui.set_function("setExpanded", &UI_SetExpanded);
		ui.set_function("type", &UI_Type);
		ui.set_function("key", &UI_Key);
		ui.set_function("scroll", &UI_Scroll);
		ui.set_function("events", &UI_Events);

		// The keys a widget understands, so a script naming one does not
		// have to know what number the engine gave it.
		sol::table keys = lua->create_table();
		keys["backspace"] = (uint32)UIKey::Backspace;
		keys["del"] = (uint32)UIKey::Delete;
		keys["left"] = (uint32)UIKey::Left;
		keys["right"] = (uint32)UIKey::Right;
		keys["up"] = (uint32)UIKey::Up;
		keys["down"] = (uint32)UIKey::Down;
		keys["home"] = (uint32)UIKey::Home;
		keys["last"] = (uint32)UIKey::End;
		keys["enter"] = (uint32)UIKey::Enter;
		keys["escape"] = (uint32)UIKey::Escape;
		keys["tab"] = (uint32)UIKey::Tab;
		ui["keys"] = keys;
	}

};

#endif
