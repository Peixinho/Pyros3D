//============================================================================
// Name        : UIButton
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : An interactive canvas element with visual states
//============================================================================

#ifndef UIBUTTON_H
#define	UIBUTTON_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Rendering/Components/UI/UIWidget.h>
#include <Pyros3D/Other/Export.h>
#include <string>
#include <vector>

namespace p3d {

	namespace UIState
	{
		enum {
			Normal = 0,
			Hover,
			Pressed,
			Disabled,
			// Keyboard/gamepad focus, kept distinct from Hover so a menu can
			// be driven by a stick and a mouse at once without the two
			// fighting over one highlight. Nothing sets it yet; it exists
			// here because retrofitting a state later means touching every
			// style, every inspector and the dispatch path.
			Focused,
			Count
		};
	}

	// What one state looks like. Deliberately not a material or a texture
	// swap: everything here is a value already on UIImage/UIText, so a state
	// change is an assignment rather than a rebuild, and a style asset is
	// just five of these.
	struct PYROS3D_API UIStateStyle
	{
		UIStateStyle() : hasTint(false), hasTextColor(false), tint(1.f, 1.f, 1.f, 1.f),
			textColor(1.f, 1.f, 1.f, 1.f), offset(0.f, 0.f) {}

		// Unset means "inherit Normal", so a hover state that only brightens
		// the fill does not also have to restate the text colour.
		bool hasTint;
		bool hasTextColor;
		Vec4 tint;
		Vec4 textColor;
		// In canvas units, added to the element's rect - the press nudge.
		Vec2 offset;
	};

	class PYROS3D_API UIButton : public UIWidget {

	public:

		UIButton();
		virtual ~UIButton();

		virtual void Register(SceneGraph* Scene);
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene);

		virtual uint32 GetComponentType() const { return ComponentType::UIButton; }

		static std::vector<IComponent*> &GetComponents();

		virtual void SetInteractable(const bool on);

		UIStateStyle &State(const uint32 state) { return states[state < UIState::Count ? state : 0]; }
		const UIStateStyle &GetState(const uint32 state) const { return states[state < UIState::Count ? state : 0]; }

		// Seconds to cross-fade between states. 0 snaps.
		void SetTransition(const f32 seconds) { transition = seconds > 0.f ? seconds : 0.f; }
		f32 GetTransition() const { return transition; }

		uint32 GetCurrentState() const { return current; }

		// The handler this button asks for when clicked. Dispatch is the
		// host's business - the engine does not know what a Lua function is -
		// so this is a name, and whoever owns scripting looks it up.
		void SetOnClick(const std::string &handler) { onClick = handler; }
		const std::string &GetOnClick() const { return onClick; }

		// Driven by UICanvas::UpdateInput(). `inside` is whether the pointer
		// is over this button's rect; `down` whether the pointer button is
		// held. Returns true on the frame a click completes - press and
		// release both inside, which is what makes a drag off the button
		// cancel it the way every other UI does.
		bool OnPointer(bool inside, bool down);

		// The canvas's generic entry point (see UIWidget). A button needs
		// neither the pointer position nor its own rect - whether the
		// pointer is inside is the whole of it.
		virtual uint32 OnPointer(const bool inside, const bool down,
			const Vec2 &point, const UIRectValue &rect)
		{
			return OnPointer(inside, down) ? UIEventFlag::Clicked : UIEventFlag::None;
		}

		// Set when OnPointer() returns true and cleared once read, so a host
		// polling once a frame cannot miss a click or see it twice.
		bool ConsumeClicked();

		// Keyboard/gamepad focus, set by the canvas. Kept separate from
		// hover so a menu driven by a stick and a mouse at once does not
		// have the two fighting over one highlight: the pointer wins while
		// it is actually over something, and focus shows through otherwise.
		void SetFocused(bool on);
		bool IsFocused() const { return focused; }
		virtual void SetWidgetFocused(const bool on) { SetFocused(on); }

		// Presses this button as if clicked, for a host driving the UI from
		// a key or a pad button rather than a pointer. Returns true so the
		// caller can dispatch the handler exactly as it does for a click.
		virtual uint32 Activate();

	private:

		void ApplyState(const f64 time);

		UIStateStyle states[UIState::Count];
		f32 transition;
		uint32 current;
		bool pressedInside;
		bool focused;
		bool wasDown;
		bool clicked;
		std::string onClick;

		// Where the cross-fade currently is, so a state change part-way
		// through another one starts from what is on screen rather than
		// from the state it was heading to.
		Vec4 blendTint, blendTextColor;
		Vec2 blendOffset;
		bool blendInitialized;
		f64 lastTime = 0.0;

		static std::vector<IComponent*> Components;
	};

};

#endif	/* UIBUTTON_H */
