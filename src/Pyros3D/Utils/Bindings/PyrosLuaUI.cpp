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
		bool UI_WasClicked(GameObject* go)
		{
			UIButton* b = FindComponent<UIButton>(go, ComponentType::UIButton);
			return b ? b->ConsumeClicked() : false;
		}
	}

	void RegisterLuaUI(sol::state* lua)
	{
		sol::table ui = lua->create_named_table("ui");
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
	}

};

#endif
