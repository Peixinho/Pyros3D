//============================================================================
// Name        : UIDropdown.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A closed list that opens to be picked from
//============================================================================

#ifndef UIDROPDOWN_H
#define	UIDROPDOWN_H

#include <Pyros3D/Rendering/Components/UI/UIWidget.h>
#include <string>
#include <vector>

namespace p3d {

	class UIList;

	// Composition rather than a widget that draws a list of its own: the
	// popup is a child element containing a UIList, and the dropdown feeds
	// it options and reads back what was picked. A dropdown is a list you
	// cannot see most of the time, and having two implementations of a list
	// would mean two lists' worth of bugs.
	class PYROS3D_API UIDropdown : public UIWidget {

	public:

		UIDropdown();
		virtual ~UIDropdown();

		virtual void Register(SceneGraph* Scene);
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene);

		virtual uint32 GetComponentType() const { return ComponentType::UIDropdown; }

		static std::vector<IComponent*> &GetComponents();

		void SetOptions(const std::vector<std::string> &options);
		const std::vector<std::string> &GetOptions() const { return options; }
		void AddOption(const std::string &option);
		void ClearOptions();

		void SetSelected(const int32 index);
		int32 GetSelected() const { return selected; }
		const std::string &GetSelectedOption() const;

		void SetExpanded(const bool on);
		bool IsExpanded() const { return expanded; }

		// Child elements, by name: the popup that is shown while open (and
		// which contains the UIList), and the label showing what is
		// currently picked.
		void SetPopupElement(const std::string &name) { popupName = name; dirty = true; }
		const std::string &GetPopupElement() const { return popupName; }
		void SetLabelElement(const std::string &name) { labelName = name; dirty = true; }
		const std::string &GetLabelElement() const { return labelName; }

		// Shown in the label while nothing is picked.
		void SetPlaceholder(const std::string &t) { placeholder = t; dirty = true; }
		const std::string &GetPlaceholder() const { return placeholder; }

		virtual uint32 OnPointer(const bool inside, const bool down,
			const Vec2 &point, const UIRectValue &rect);
		virtual uint32 OnKey(const uint32 key, bool &claimed);
		virtual uint32 Activate();
		virtual bool TakesFocusOnPress() const { return true; }

	private:

		void Apply();
		UIList* FindList();

		std::vector<std::string> options;
		int32 selected;
		bool expanded;
		bool dirty;
		bool wasDown;
		// What the popup's list was showing last time this looked, so a
		// pick can be told from a value this dropdown set itself.
		int32 lastListSelection;
		std::string popupName, labelName, placeholder;

		static std::vector<IComponent*> Components;
	};

}

#endif	/* UIDROPDOWN_H */
