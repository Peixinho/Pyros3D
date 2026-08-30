//============================================================================
// Name        : UIMenu.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Menus and submenus, as a tree of items
//============================================================================

#ifndef UIMENU_H
#define	UIMENU_H

#include <Pyros3D/Rendering/Components/UI/UIButton.h>
#include <string>
#include <vector>

namespace p3d {

	class UIMenu;

	// One entry. A UIButton, for the same reason a checkbox is: hover,
	// press, disabled and the state cross-fade are all identical, and an
	// entry that behaved differently under the pointer would be a bug. What
	// it adds is a submenu - the name of a child element shown while this
	// entry is open, which is what makes menus nest without a second kind
	// of thing to author.
	class PYROS3D_API UIMenuItem : public UIButton {

	public:

		UIMenuItem();
		virtual ~UIMenuItem();

		virtual void Register(SceneGraph* Scene);
		virtual void Unregister(SceneGraph* Scene);

		virtual uint32 GetComponentType() const { return ComponentType::UIMenuItem; }

		static std::vector<IComponent*> &GetComponents();

		// Empty means a leaf: clicking it does whatever its onClick says and
		// closes the whole menu.
		void SetSubmenu(const std::string &name) { submenu = name; }
		const std::string &GetSubmenu() const { return submenu; }
		bool HasSubmenu() const { return !submenu.empty(); }

		// The element this entry opens, or NULL.
		GameObject* SubmenuNode() const;
		// Shows or hides it, and closes anything open inside it - a submenu
		// that reopened with its own children still expanded would be a
		// menu remembering something nobody asked it to.
		void SetOpen(const bool on);
		bool IsOpen() const;

		// The menu this entry belongs to: the nearest ancestor carrying a
		// UIMenu. Entries do not own the open chain, because a submenu's
		// entries have to close their cousins in the parent menu.
		UIMenu* Menu() const;

		virtual uint32 OnPointer(const bool inside, const bool down,
			const Vec2 &point, const UIRectValue &rect);
		virtual uint32 Activate();

	private:

		std::string submenu;
		// Whether the pointer was over this entry last frame, so opening on
		// hover happens once rather than every frame it stays there.
		bool wasInside;

		static std::vector<IComponent*> Components;
	};

	// The root of a menu tree - a menu bar, or a standalone popup. It owns
	// what is open, because closing "File" when you move to "Edit" is a
	// decision about siblings, and no single entry can see its siblings'
	// siblings.
	class PYROS3D_API UIMenu : public UIWidget {

	public:

		UIMenu();
		virtual ~UIMenu();

		virtual void Register(SceneGraph* Scene);
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene);

		virtual uint32 GetComponentType() const { return ComponentType::UIMenu; }

		static std::vector<IComponent*> &GetComponents();

		// Armed. A menu bar opens on a click and then follows the pointer:
		// once File is open, moving across to Edit opens Edit without
		// another click, and moving away from both closes neither until
		// something is picked or the menu is dismissed. That is the whole
		// difference between a menu bar and a row of dropdowns.
		bool IsActive() const { return active; }
		void SetActive(const bool on);

		// Opens `item` and closes whatever else was open at its level (and
		// inside those). An entry with no submenu still closes its siblings
		// - moving from "File" to a plain entry beside it has to shut File.
		void OpenItem(UIMenuItem* item);
		// Everything, and disarms.
		void CloseAll();
		// Whether a canvas point is over this menu or any part of it that is
		// currently open - which is what "clicked outside" has to mean when
		// the thing you clicked is a submenu three levels down.
		bool ContainsPoint(const Vec2 &point) const;

		// The deepest entry currently open, or NULL.
		UIMenuItem* DeepestOpen() const;

		virtual uint32 OnPointer(const bool inside, const bool down,
			const Vec2 &point, const UIRectValue &rect);
		virtual uint32 OnKey(const uint32 key, bool &claimed);
		// A menu is not somewhere focus should land: its entries are.
		virtual bool IsFocusable() const { return false; }
		// While it is open. Escape has to reach the menu even though the
		// focus is on whatever was under it.
		virtual bool WantsKeysWhileUnfocused() const { return active; }

	private:

		bool active;
		bool wasDown;

		static std::vector<IComponent*> Components;
	};

}

#endif	/* UIMENU_H */
