//============================================================================
// Name        : UIToggle.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A checkbox: a button that remembers what it was set to
//============================================================================

#ifndef UITOGGLE_H
#define	UITOGGLE_H

#include <Pyros3D/Rendering/Components/UI/UIButton.h>

namespace p3d {

	// Deliberately a UIButton: hover, press, focus, the state cross-fade and
	// the press-and-drag-away rule are all the same, and a checkbox that
	// behaved differently from a button under the pointer would be a bug
	// rather than a feature. What it adds is a value, and a child element
	// that is shown or hidden to display it.
	class PYROS3D_API UIToggle : public UIButton {

	public:

		UIToggle();
		virtual ~UIToggle();

		virtual void Register(SceneGraph* Scene);
		virtual void Unregister(SceneGraph* Scene);
		virtual void Update(const f64 time = 0);

		virtual uint32 GetComponentType() const { return ComponentType::UIToggle; }

		static std::vector<IComponent*> &GetComponents();

		void SetValue(const bool on);
		bool GetValue() const { return value; }

		// The child element shown while on, by name. A name rather than a
		// pointer because that is what survives a save, a prefab and an
		// undo - the same reason UIRect carries a style reference as text.
		// Empty means the toggle drives nothing but its own states, which
		// is what a "pressed-in" style-only checkbox wants.
		void SetCheckElement(const std::string &name) { checkName = name; applied = false; }
		const std::string &GetCheckElement() const { return checkName; }

		// Toggles that share a group behave as radio buttons: turning one
		// on turns the others off. The group is a name, matched against
		// siblings under the same parent, so grouping is authored by
		// putting them together rather than by wiring up references.
		void SetGroup(const std::string &group) { this->group = group; }
		const std::string &GetGroup() const { return group; }

		virtual uint32 OnPointer(const bool inside, const bool down,
			const Vec2 &point, const UIRectValue &rect);
		virtual uint32 Activate();

	private:

		void ApplyValue();
		void ClearGroupSiblings();

		bool value;
		bool applied;
		std::string checkName;
		std::string group;

		static std::vector<IComponent*> Components;
	};

}

#endif	/* UITOGGLE_H */
