//============================================================================
// Name        : UIDropdown
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A closed list that opens to be picked from
//============================================================================

#include <Pyros3D/Rendering/Components/UI/UIDropdown.h>
#include <Pyros3D/Rendering/Components/UI/UIList.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Rendering/Components/UI/UIText.h>
#include <Pyros3D/GameObjects/GameObject.h>

namespace p3d {

	std::vector<IComponent*> UIDropdown::Components;

	namespace {
		const std::string kNoOption;

		GameObject* FindChild(GameObject* root, const std::string &name)
		{
			if (!root || name.empty()) return NULL;
			const std::vector<std::shared_ptr<GameObject> > &kids = root->GetChildren();
			for (size_t i = 0; i < kids.size(); i++)
			{
				if (!kids[i]) continue;
				if (kids[i]->GetName() == name) return kids[i].get();
				if (GameObject* found = FindChild(kids[i].get(), name)) return found;
			}
			return NULL;
		}

		template <class T> T* ComponentIn(GameObject* go, const uint32 type)
		{
			if (!go) return NULL;
			const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
			for (size_t i = 0; i < cs.size(); i++)
				if (cs[i] && cs[i]->GetComponentType() == type) return static_cast<T*>(cs[i].get());
			return NULL;
		}

		UIList* ListIn(GameObject* go)
		{
			if (!go) return NULL;
			if (UIList* l = ComponentIn<UIList>(go, ComponentType::UIList)) return l;
			const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
			for (size_t i = 0; i < kids.size(); i++)
			{
				if (!kids[i]) continue;
				if (UIList* l = ListIn(kids[i].get())) return l;
			}
			return NULL;
		}
	}

	UIDropdown::UIDropdown() : UIWidget()
	{
		selected = -1;
		expanded = false;
		dirty = true;
		wasDown = true;
		lastListSelection = -1;
		popupName = "Popup";
		labelName = "Label";
	}

	UIDropdown::~UIDropdown() {}

	void UIDropdown::Register(SceneGraph* Scene)
	{
		if (!Registered)
		{
			Components.push_back(this);
			Registered = true;
		}
	}

	void UIDropdown::Unregister(SceneGraph* Scene)
	{
		for (std::vector<IComponent*>::iterator i = Components.begin(); i != Components.end(); i++)
			if ((*i) == this) { Components.erase(i); break; }
		Registered = false;
	}

	std::vector<IComponent*> &UIDropdown::GetComponents()
	{
		return Components;
	}

	UIList* UIDropdown::FindList()
	{
		if (!GetOwner()) return NULL;
		return ListIn(FindChild(GetOwner(), popupName));
	}

	void UIDropdown::SetOptions(const std::vector<std::string> &v)
	{
		options = v;
		if (selected >= (int32)options.size()) selected = -1;
		dirty = true;
	}

	void UIDropdown::AddOption(const std::string &option)
	{
		options.push_back(option);
		dirty = true;
	}

	void UIDropdown::ClearOptions()
	{
		options.clear();
		selected = -1;
		dirty = true;
	}

	void UIDropdown::SetSelected(const int32 index)
	{
		const int32 clamped = (index < 0 || index >= (int32)options.size()) ? -1 : index;
		if (clamped == selected) return;
		selected = clamped;
		dirty = true;
	}

	const std::string &UIDropdown::GetSelectedOption() const
	{
		if (selected < 0 || selected >= (int32)options.size()) return kNoOption;
		return options[selected];
	}

	void UIDropdown::SetExpanded(const bool on)
	{
		if (expanded == on) return;
		expanded = on;
		dirty = true;
	}

	uint32 UIDropdown::OnPointer(const bool inside, const bool down,
		const Vec2 &point, const UIRectValue &rect)
	{
		if (!interactable) { wasDown = down; return UIEventFlag::None; }

		const bool pressed = down && !wasDown;
		wasDown = down;
		if (!pressed) return UIEventFlag::None;

		if (inside)
		{
			SetExpanded(!expanded);
			return UIEventFlag::Clicked;
		}
		// A press anywhere else closes it. The popup is not inside this
		// element's rect - it hangs below it - so a press on a row arrives
		// here as "outside", and closing on the same frame the list records
		// the pick is exactly right: both see the press, and the popup only
		// disappears at the next solve.
		if (expanded) SetExpanded(false);
		return UIEventFlag::None;
	}

	uint32 UIDropdown::OnKey(const uint32 key, bool &claimed)
	{
		claimed = false;
		if (!interactable) return UIEventFlag::None;

		if (key == UIKey::Escape && expanded)
		{
			claimed = true;
			SetExpanded(false);
			return UIEventFlag::None;
		}
		if (key == UIKey::Enter)
		{
			claimed = true;
			SetExpanded(!expanded);
			return UIEventFlag::Clicked;
		}
		// Closed, the arrows step through the options without opening it,
		// which is how every settings screen expects to be driven by a pad.
		if (!expanded && (key == UIKey::Up || key == UIKey::Down))
		{
			if (options.empty()) return UIEventFlag::None;
			claimed = true;
			int32 target = selected + (key == UIKey::Down ? 1 : -1);
			if (target < 0) target = 0;
			if (target >= (int32)options.size()) target = (int32)options.size() - 1;
			if (target == selected) return UIEventFlag::None;
			SetSelected(target);
			return UIEventFlag::Changed;
		}
		return UIEventFlag::None;
	}

	uint32 UIDropdown::Activate()
	{
		if (!interactable) return UIEventFlag::None;
		SetExpanded(!expanded);
		return UIEventFlag::Clicked;
	}

	void UIDropdown::Update(const f64 time)
	{
		// Polled rather than wired: the canvas dispatches to the list and to
		// this independently, and nothing guarantees an order between them.
		// Reading the list once a frame is both simpler and immune to it.
		UIList* list = FindList();
		if (list && list->GetSelected() != lastListSelection)
		{
			lastListSelection = list->GetSelected();
			if (lastListSelection >= 0)
			{
				SetSelected(lastListSelection);
				SetExpanded(false);
			}
		}

		if (dirty) Apply();
	}

	void UIDropdown::Apply()
	{
		if (!GetOwner()) return;

		if (GameObject* popup = FindChild(GetOwner(), popupName))
		{
			if (UIRect* r = ComponentIn<UIRect>(popup, ComponentType::UIRect))
				r->SetVisible(expanded);
		}

		if (GameObject* label = FindChild(GetOwner(), labelName))
		{
			if (UIText* t = ComponentIn<UIText>(label, ComponentType::UIText))
				t->SetText(selected >= 0 ? options[selected] : placeholder);
		}

		if (UIList* list = FindList())
		{
			if (list->GetItems() != options) list->SetItems(options);
			list->SetSelected(selected);
			lastListSelection = list->GetSelected();
		}

		dirty = false;
	}

};
