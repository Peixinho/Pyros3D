//============================================================================
// Name        : UIDispatch.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Canvas events -> the named handlers on the elements.
//
//               Outside the engine, next to the prefab and style
//               resolvers, and for the same reason: the engine reports that
//               a slider changed, and has no idea what a Lua function is.
//               Which handler a given event calls, and what it is passed,
//               is a decision about scripting - so it lives here, shared by
//               the player and by the editor's play mode, which have to
//               agree or a UI would behave differently once it shipped.
//============================================================================

#ifndef UIDISPATCH_H
#define UIDISPATCH_H

#ifdef LUA_BINDINGS

#include <Pyros3D/Ext/sol/sol.hpp>
#include <Pyros3D/Rendering/Components/UI/UICanvas.h>
#include <Pyros3D/Rendering/Components/UI/UIButton.h>
#include <Pyros3D/Rendering/Components/UI/UIToggle.h>
#include <Pyros3D/Rendering/Components/UI/UISlider.h>
#include <Pyros3D/Rendering/Components/UI/UIInput.h>
#include <Pyros3D/Rendering/Components/UI/UIList.h>
#include <Pyros3D/Rendering/Components/UI/UIDropdown.h>
#include <Pyros3D/Rendering/Components/UI/UIPopup.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <string>
#include <vector>

namespace uidispatch {

	using namespace p3d;

	// Where a complaint goes. The player and the editor log differently, and
	// a test wants to collect them.
	typedef void (*ReportFn)(const std::string &message, void* user);

	// The handler an element names for what just happened to it. A submit
	// falls back to the change handler when there is no submit one, so a
	// field with a single handler still hears Enter.
	inline std::string HandlerFor(UIWidget* widget, const uint32 flags)
	{
		if (!widget) return std::string();

		if (flags & UIEventFlag::Submitted)
		{
			if (UIInput* in = dynamic_cast<UIInput*>(widget)) { if (!in->GetOnSubmit().empty()) return in->GetOnSubmit(); }
			else if (UIList* l = dynamic_cast<UIList*>(widget)) { if (!l->GetOnSubmit().empty()) return l->GetOnSubmit(); }
			else if (UIPopup* p = dynamic_cast<UIPopup*>(widget)) { if (!p->GetOnClose().empty()) return p->GetOnClose(); }
		}
		if (flags & (UIEventFlag::Changed | UIEventFlag::Submitted))
		{
			if (!widget->GetOnChange().empty()) return widget->GetOnChange();
		}
		// A click with nothing else to say is a button press, and a button's
		// handler is its onClick.
		if (flags & UIEventFlag::Clicked)
		{
			if (UIButton* b = dynamic_cast<UIButton*>(widget)) return b->GetOnClick();
		}
		return std::string();
	}

	// Calls `handler` with the element's name and whatever value it has -
	// which is what a handler wants and would otherwise have to go and ask
	// for. Returns false if the handler was missing or errored.
	inline bool Call(sol::state &lua, const std::string &handler, GameObject* node, UIWidget* widget,
		ReportFn report, void* user)
	{
		sol::protected_function fn = lua[handler];
		if (!fn.valid())
		{
			if (report) report("WARNING: UI element '" + (node ? node->GetName() : std::string("?"))
				+ "' wants '" + handler + "', which is not a global function", user);
			return false;
		}

		const std::string name = node ? node->GetName() : std::string();
		sol::protected_function_result res;
		if (UISlider* s = dynamic_cast<UISlider*>(widget)) res = fn(name, s->GetValue());
		else if (UIToggle* t = dynamic_cast<UIToggle*>(widget)) res = fn(name, t->GetValue());
		else if (UIInput* in = dynamic_cast<UIInput*>(widget)) res = fn(name, in->GetText());
		else if (UIList* l = dynamic_cast<UIList*>(widget)) res = fn(name, l->GetSelectedItem(), l->GetSelected());
		else if (UIDropdown* d = dynamic_cast<UIDropdown*>(widget)) res = fn(name, d->GetSelectedOption(), d->GetSelected());
		else res = fn(name);

		if (!res.valid())
		{
			sol::error err = res;
			if (report) report(std::string("ERROR: UI handler '") + handler + "' - " + err.what(), user);
			return false;
		}
		return true;
	}

	// Everything a canvas reported since the last input call. Returns how
	// many handlers ran.
	//
	// The events are copied first: a handler is free to open a dialog, load
	// a scene or otherwise disturb the canvas it was called from, and
	// iterating the live list while that happens is how a UI ends up
	// crashing on the frame something interesting finally happened.
	inline int Dispatch(UICanvas* canvas, sol::state &lua, ReportFn report = NULL, void* user = NULL)
	{
		if (!canvas) return 0;
		const std::vector<UICanvas::WidgetEvent> events = canvas->GetEvents();
		int called = 0;
		for (size_t i = 0; i < events.size(); i++)
		{
			const std::string handler = HandlerFor(events[i].widget, events[i].flags);
			if (handler.empty()) continue;
			if (Call(lua, handler, events[i].node, events[i].widget, report, user)) called++;
		}
		return called;
	}

}

#endif /* LUA_BINDINGS */

#endif /* UIDISPATCH_H */
