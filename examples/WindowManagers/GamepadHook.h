//============================================================================
// Name        : GamepadHook.h
// Description : Game controllers, for every SDL2 window manager.
//
//               The engine has had joypad events in InputManager from the
//               start and no window manager ever fed them, so a controller
//               plugged into a Pyros3D game did nothing at all. This is the
//               missing half, written once and called from each SDL2 context
//               (GL, Vulkan, Metal) rather than three times.
//
//               SDL's GameController layer rather than its raw joystick one:
//               it maps every recognised pad onto one layout, so "the bottom
//               face button" is button 0 on an Xbox pad, a DualSense and a
//               Switch Pro alike, and a game is written against one set of
//               names (GamepadButton / GamepadAxis in Lua).
//============================================================================

#ifndef PYROS_GAMEPADHOOK_H
#define PYROS_GAMEPADHOOK_H

#include <Pyros3D/Core/InputManager/InputManager.h>
#include <SDL2/SDL.h>

namespace PyrosGamepad {

	// SDL identifies an open pad by an instance id that goes up for ever; a
	// game wants "player one's pad" as a small number that stays put. Eight
	// slots, a pad taking the lowest free one when it is plugged in and
	// giving it back when it is pulled.
	struct Slots {
		SDL_GameController* pad[8];
		SDL_JoystickID instance[8];
		bool started;
	};
	inline Slots &State()
	{
		static Slots s = { { 0, 0, 0, 0, 0, 0, 0, 0 }, { -1, -1, -1, -1, -1, -1, -1, -1 }, false };
		return s;
	}

	// After SDL_Init. Its own subsystem, so a build that fails to bring it
	// up (no HID access, a headless box) still has a window and a keyboard.
	inline void Init()
	{
		Slots &s = State();
		if (s.started) return;
		s.started = true;
		if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) return;
		SDL_GameControllerEventState(SDL_ENABLE);
	}

	inline int SlotOf(const SDL_JoystickID id)
	{
		const Slots &s = State();
		for (int i = 0; i < 8; i++) if (s.pad[i] != NULL && s.instance[i] == id) return i;
		return -1;
	}

	// Every SDL event goes through here; anything that is not a controller's
	// is ignored. True when it was one.
	template <typename Pressed, typename Released, typename Moved>
	inline bool HandleEvent(const SDL_Event &e, Pressed pressed, Released released, Moved moved)
	{
		Slots &s = State();
		switch (e.type)
		{
		case SDL_CONTROLLERDEVICEADDED:
		{
			// `which` is a device INDEX here, an instance id everywhere else.
			SDL_GameController* c = SDL_GameControllerOpen(e.cdevice.which);
			if (c == NULL) return true;
			const SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c));
			if (SlotOf(id) >= 0) { return true; }   // reported twice at startup on some platforms
			for (int i = 0; i < 8; i++)
				if (s.pad[i] == NULL)
				{
					s.pad[i] = c;
					s.instance[i] = id;
					const char* name = SDL_GameControllerName(c);
					p3d::InputManager::SetJoypadConnected((p3d::uint32)i, true, name ? name : "Gamepad");
					return true;
				}
			SDL_GameControllerClose(c);     // a ninth pad: no slot for it
			return true;
		}
		case SDL_CONTROLLERDEVICEREMOVED:
		{
			const int i = SlotOf(e.cdevice.which);
			if (i < 0) return true;
			SDL_GameControllerClose(s.pad[i]);
			s.pad[i] = NULL;
			s.instance[i] = -1;
			// Everything it was holding is let go: a pad pulled with a
			// button down would otherwise hold that button for ever.
			p3d::InputManager::SetJoypadConnected((p3d::uint32)i, false, "");
			return true;
		}
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
		{
			const int i = SlotOf(e.cbutton.which);
			if (i < 0) return true;
			if (e.type == SDL_CONTROLLERBUTTONDOWN) pressed((p3d::uint32)i, (p3d::uint32)e.cbutton.button);
			else released((p3d::uint32)i, (p3d::uint32)e.cbutton.button);
			return true;
		}
		case SDL_CONTROLLERAXISMOTION:
		{
			const int i = SlotOf(e.caxis.which);
			if (i < 0) return true;
			// -32768..32767 to -1..1, and the sticks' vertical axes turned
			// over: SDL has DOWN as positive, and a 2D game's up is +y. A
			// stick pushed up reading +1 is one sign fewer to get wrong in
			// every controller script ever written.
			p3d::f32 v = e.caxis.value < 0 ? (p3d::f32)e.caxis.value / 32768.f : (p3d::f32)e.caxis.value / 32767.f;
			if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY || e.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY) v = -v;
			moved((p3d::uint32)i, (p3d::uint32)e.caxis.axis, v);
			return true;
		}
		default:
			return false;
		}
	}

}

#endif /* PYROS_GAMEPADHOOK_H */
