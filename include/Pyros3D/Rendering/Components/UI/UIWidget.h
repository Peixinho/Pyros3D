//============================================================================
// Name        : UIWidget.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : What every interactive canvas element has in common
//============================================================================

#ifndef UIWIDGET_H
#define	UIWIDGET_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Other/Export.h>
#include <string>

namespace p3d {

	// What a widget did with the input it was handed. A bitmask because one
	// event can be two things at once: typing Enter into a text field both
	// changes it and submits it.
	namespace UIEventFlag
	{
		enum {
			None = 0,
			// The value changed and anything mirroring it should re-read.
			Changed = 1,
			// A click, or its keyboard equivalent, completed on this widget.
			Clicked = 2,
			// The user is done: Enter in a field, a list item double-clicked.
			Submitted = 4
		};
	}

	// The interaction half of a UI element. UIImage/UIText draw, UIRect
	// lays out, and a widget is the part that reacts - so a checkbox is a
	// UIRect plus a UIImage plus a UIToggle, exactly as a button is.
	//
	// The canvas drives every one of these rather than each polling input,
	// because only the canvas knows what is on top: two overlapping widgets
	// must not both light up, and the one underneath must not get the click.
	class PYROS3D_API UIWidget : public IComponent {

	public:

		UIWidget() : interactable(true), widgetFocused(false) {}
		virtual ~UIWidget() {}

		// Pointer state for this frame, in canvas units. `inside` is whether
		// the pointer is over this widget's rect and no other widget is on
		// top of it; `down` whether its button is held. `rect` is the rect
		// this widget solved to, so a slider can work out where along its
		// track the pointer is without hunting for its own layout.
		virtual uint32 OnPointer(const bool inside, const bool down,
			const Vec2 &point, const UIRectValue &rect) { return UIEventFlag::None; }

		// Wheel notches, positive away from the user. Only reaches the
		// widget under the pointer, innermost first.
		virtual uint32 OnScroll(const f32 delta) { return UIEventFlag::None; }

		// UTF-8 the platform decoded from the keyboard - a character, not a
		// key. Only ever reaches the focused widget.
		virtual uint32 OnText(const std::string &utf8) { return UIEventFlag::None; }

		// A non-printing key (see UIKey), for the focused widget. Returning
		// a flag or claiming the key stops the canvas using it for
		// navigation, which is what keeps arrow keys inside a text field
		// instead of walking focus out of it.
		virtual uint32 OnKey(const uint32 key, bool &claimed) { claimed = false; return UIEventFlag::None; }

		// Off means "draw as disabled and take no input". The canvas skips
		// these entirely, including for focus navigation.
		virtual void SetInteractable(const bool on) { interactable = on; }
		bool IsInteractable() const { return interactable; }

		// Whether keyboard navigation should be able to land here. A label
		// -like widget can opt out without becoming non-interactive.
		virtual bool IsFocusable() const { return interactable; }

		// Whether clicking this widget should also focus it. False by
		// default: a button works without focus, and taking it on every
		// click leaves a keyboard highlight the user never asked for -
		// which is exactly the pointer-and-pad fight UIState::Focused
		// exists to avoid. A text field, a list or a dropdown says true,
		// because typing has to go somewhere.
		virtual bool TakesFocusOnPress() const { return false; }

		// Whether this widget should be offered keys even when something
		// else has focus. False for almost everything - keys belong to the
		// focused element. An open menu is the exception: it is on top of
		// whatever had focus, and Escape has to shut it rather than being
		// swallowed by a text field underneath.
		virtual bool WantsKeysWhileUnfocused() const { return false; }

		// Whether this widget is currently blocking everything outside its
		// own subtree - a dialog that is open. The canvas dispatches only
		// inside the topmost one of these, which is what makes a modal
		// modal rather than merely on top.
		virtual bool IsModalActive() const { return false; }

		virtual void SetWidgetFocused(const bool on) { widgetFocused = on; }
		bool IsWidgetFocused() const { return widgetFocused; }

		// Activated by keyboard/pad rather than pointer - Enter or A on
		// whatever has focus.
		virtual uint32 Activate() { return UIEventFlag::None; }

		// The handler this widget asks for when it fires. Dispatch is the
		// host's business - the engine does not know what a Lua function is -
		// so this is a name, and whoever owns scripting looks it up.
		void SetOnChange(const std::string &handler) { onChange = handler; }
		const std::string &GetOnChange() const { return onChange; }

	protected:

		bool interactable;
		bool widgetFocused;
		std::string onChange;
	};

	// Whether a child element's name is the one a widget is looking for,
	// ignoring the "(2)" the editor appends to keep names unique across a
	// scene. Two checkboxes both want a child called Check, and the second
	// one gets Check(1) - so an exact match would find nothing, and the
	// second checkbox would be the one that quietly does not work.
	inline bool UINameMatches(const std::string &actual, const std::string &wanted)
	{
		if (actual == wanted) return true;
		if (actual.size() <= wanted.size() + 2) return false;
		if (actual.compare(0, wanted.size(), wanted) != 0) return false;
		if (actual[wanted.size()] != '(' || actual[actual.size() - 1] != ')') return false;
		for (size_t i = wanted.size() + 1; i + 1 < actual.size(); i++)
			if (actual[i] < '0' || actual[i] > '9') return false;
		return actual.size() > wanted.size() + 2;
	}

	// Keys a widget can be sent. Deliberately a short list of the ones text
	// editing and list navigation need, mapped by the host from whatever its
	// window layer calls them, so the engine carries no keyboard enum of its
	// own into the UI.
	namespace UIKey
	{
		enum {
			None = 0,
			Backspace,
			Delete,
			Left,
			Right,
			Up,
			Down,
			Home,
			End,
			Enter,
			Escape,
			Tab
		};
	}

}

#endif	/* UIWIDGET_H */
