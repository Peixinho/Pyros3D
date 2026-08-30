//============================================================================
// Name        : UIMenu
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Menus and submenus, as a tree of items
//============================================================================

#include <Pyros3D/Rendering/Components/UI/UIMenu.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/GameObjects/GameObject.h>

namespace p3d {

	std::vector<IComponent*> UIMenuItem::Components;
	std::vector<IComponent*> UIMenu::Components;

	namespace {

		UIRect* RectOn(GameObject* go)
		{
			if (!go) return NULL;
			const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
			for (size_t i = 0; i < cs.size(); i++)
				if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
					return static_cast<UIRect*>(cs[i].get());
			return NULL;
		}

		UIMenuItem* ItemOn(GameObject* go)
		{
			if (!go) return NULL;
			const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
			for (size_t i = 0; i < cs.size(); i++)
				if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIMenuItem)
					return static_cast<UIMenuItem*>(cs[i].get());
			return NULL;
		}

		// Every entry below this node, however deep - a submenu's entries
		// are children of the submenu element, not of the entry that opens
		// it, so this has to walk rather than look one level down.
		void CollectItems(GameObject* node, std::vector<UIMenuItem*> &out)
		{
			if (!node) return;
			if (UIMenuItem* item = ItemOn(node)) out.push_back(item);
			const std::vector<std::shared_ptr<GameObject> > &kids = node->GetChildren();
			for (size_t i = 0; i < kids.size(); i++) CollectItems(kids[i].get(), out);
		}

		GameObject* FindChildNamed(GameObject* root, const std::string &name)
		{
			if (!root || name.empty()) return NULL;
			const std::vector<std::shared_ptr<GameObject> > &kids = root->GetChildren();
			for (size_t i = 0; i < kids.size(); i++)
			{
				if (!kids[i]) continue;
				if (UINameMatches(kids[i]->GetName(), name)) return kids[i].get();
				if (GameObject* found = FindChildNamed(kids[i].get(), name)) return found;
			}
			return NULL;
		}
	}

	// =====================================================================
	// UIMenuItem
	// =====================================================================

	UIMenuItem::UIMenuItem() : UIButton()
	{
		wasInside = false;
	}

	UIMenuItem::~UIMenuItem() {}

	void UIMenuItem::Register(SceneGraph* Scene)
	{
		if (!Registered)
		{
			Components.push_back(this);
			Registered = true;
		}
	}

	void UIMenuItem::Unregister(SceneGraph* Scene)
	{
		for (std::vector<IComponent*>::iterator i = Components.begin(); i != Components.end(); i++)
			if ((*i) == this) { Components.erase(i); break; }
		Registered = false;
	}

	std::vector<IComponent*> &UIMenuItem::GetComponents()
	{
		return Components;
	}

	GameObject* UIMenuItem::SubmenuNode() const
	{
		if (submenu.empty()) return NULL;
		return FindChildNamed(const_cast<UIMenuItem*>(this)->GetOwner(), submenu);
	}

	bool UIMenuItem::IsOpen() const
	{
		UIRect* r = RectOn(SubmenuNode());
		return r && r->IsVisible();
	}

	void UIMenuItem::SetOpen(const bool on)
	{
		GameObject* node = SubmenuNode();
		if (!node) return;
		if (UIRect* r = RectOn(node)) r->SetVisible(on);
		if (on) return;

		// Closing takes everything inside with it. A submenu that reopened
		// with its own children still expanded would be a menu remembering
		// something nobody asked it to.
		std::vector<UIMenuItem*> inner;
		CollectItems(node, inner);
		for (size_t i = 0; i < inner.size(); i++)
			if (inner[i] != this) inner[i]->SetOpen(false);
	}

	UIMenu* UIMenuItem::Menu() const
	{
		for (GameObject* go = const_cast<UIMenuItem*>(this)->GetOwner(); go != NULL; go = go->GetParent())
		{
			const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
			for (size_t i = 0; i < cs.size(); i++)
				if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIMenu)
					return static_cast<UIMenu*>(cs[i].get());
		}
		return NULL;
	}

	uint32 UIMenuItem::OnPointer(const bool inside, const bool down,
		const Vec2 &point, const UIRectValue &rect)
	{
		UIMenu* menu = Menu();
		const bool clicked = UIButton::OnPointer(inside, down);

		if (menu && interactable)
		{
			// Hover follows the pointer once the menu is armed, and only on
			// the frame the pointer arrives - reopening every frame would
			// fight a click trying to close the same entry.
			if (inside && !wasInside && menu->IsActive()) menu->OpenItem(this);
			wasInside = inside;
		}

		if (!clicked) return UIEventFlag::None;

		if (!menu)
		{
			// An entry outside a menu is just a button.
			return UIEventFlag::Clicked;
		}

		if (HasSubmenu())
		{
			// Clicking a branch arms the menu and opens it, or shuts it if
			// it was already open - which is how a menu bar's title behaves
			// and the only way to dismiss one with the mouse alone.
			if (IsOpen()) { menu->CloseAll(); }
			else { menu->SetActive(true); menu->OpenItem(this); }
			return UIEventFlag::Clicked;
		}

		// A leaf: it did something, so the menu is finished.
		menu->CloseAll();
		return UIEventFlag::Clicked;
	}

	uint32 UIMenuItem::Activate()
	{
		if (!interactable) return UIEventFlag::None;
		UIMenu* menu = Menu();
		if (menu && HasSubmenu())
		{
			menu->SetActive(true);
			menu->OpenItem(this);
			return UIEventFlag::Clicked;
		}
		if (menu) menu->CloseAll();
		return UIButton::Activate();
	}

	// =====================================================================
	// UIMenu
	// =====================================================================

	UIMenu::UIMenu() : UIWidget()
	{
		active = false;
		wasDown = true;
	}

	UIMenu::~UIMenu() {}

	void UIMenu::Register(SceneGraph* Scene)
	{
		if (!Registered)
		{
			Components.push_back(this);
			Registered = true;
		}
	}

	void UIMenu::Unregister(SceneGraph* Scene)
	{
		for (std::vector<IComponent*>::iterator i = Components.begin(); i != Components.end(); i++)
			if ((*i) == this) { Components.erase(i); break; }
		Registered = false;
	}

	std::vector<IComponent*> &UIMenu::GetComponents()
	{
		return Components;
	}

	// Nothing per-frame: what is open is changed by input, not by time, and
	// a menu that recomputed itself every frame would fight the click that
	// just closed it.
	void UIMenu::Update(const f64 time) {}

	void UIMenu::SetActive(const bool on)
	{
		active = on;
		if (!on) CloseAll();
	}

	void UIMenu::OpenItem(UIMenuItem* item)
	{
		if (!item || !item->GetOwner()) return;
		GameObject* parent = item->GetOwner()->GetParent();
		if (!parent) return;

		// Siblings first: opening Edit has to close File, and a plain entry
		// beside them has to close both.
		const std::vector<std::shared_ptr<GameObject> > &siblings = parent->GetChildren();
		for (size_t i = 0; i < siblings.size(); i++)
		{
			if (!siblings[i] || siblings[i].get() == item->GetOwner()) continue;
			if (UIMenuItem* other = ItemOn(siblings[i].get())) other->SetOpen(false);
		}
		item->SetOpen(true);
	}

	void UIMenu::CloseAll()
	{
		active = false;
		if (!GetOwner()) return;
		std::vector<UIMenuItem*> items;
		CollectItems(GetOwner(), items);
		for (size_t i = 0; i < items.size(); i++) items[i]->SetOpen(false);
	}

	UIMenuItem* UIMenu::DeepestOpen() const
	{
		GameObject* owner = const_cast<UIMenu*>(this)->GetOwner();
		if (!owner) return NULL;
		std::vector<UIMenuItem*> items;
		CollectItems(owner, items);
		// Depth is how many menu ancestors an entry has, and CollectItems
		// walks parents before children, so the last open one is deepest.
		UIMenuItem* deepest = NULL;
		for (size_t i = 0; i < items.size(); i++)
			if (items[i]->IsOpen()) deepest = items[i];
		return deepest;
	}

	bool UIMenu::ContainsPoint(const Vec2 &point) const
	{
		GameObject* owner = const_cast<UIMenu*>(this)->GetOwner();
		if (!owner) return false;
		if (UIRect* own = RectOn(owner))
			if (own->IsVisible() && own->GetRect().Contains(point)) return true;

		// Any open submenu, at any depth: a click three levels down is not
		// a click outside the menu, even though it is well outside the bar.
		std::vector<UIMenuItem*> items;
		CollectItems(owner, items);
		for (size_t i = 0; i < items.size(); i++)
		{
			if (!items[i]->IsOpen()) continue;
			if (UIRect* r = RectOn(items[i]->SubmenuNode()))
				if (r->GetRect().Contains(point)) return true;
		}
		return false;
	}

	uint32 UIMenu::OnPointer(const bool inside, const bool down,
		const Vec2 &point, const UIRectValue &rect)
	{
		const bool pressed = down && !wasDown;
		wasDown = down;
		// A press anywhere that is not this menu or one of its open parts
		// dismisses it. Menus are the one thing on a canvas that has to
		// react to being ignored.
		if (pressed && active && !ContainsPoint(point)) CloseAll();
		return UIEventFlag::None;
	}

	uint32 UIMenu::OnKey(const uint32 key, bool &claimed)
	{
		claimed = false;
		if (key != UIKey::Escape || !active) return UIEventFlag::None;
		claimed = true;
		// Escape closes one level at a time, which is what lets you back out
		// of a submenu without losing the menu you opened it from.
		if (UIMenuItem* deepest = DeepestOpen())
		{
			deepest->SetOpen(false);
			if (DeepestOpen() == NULL) active = false;
		}
		else CloseAll();
		return UIEventFlag::None;
	}

};
