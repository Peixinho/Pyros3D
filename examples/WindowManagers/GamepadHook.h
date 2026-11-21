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

//============================================================================
// Raw joysticks and force feedback: wheels, pedals, shifters, button boxes.
//
// A wheel base or a pedal set is not a GameController (SDL's layer knows
// gamepads), so above it was invisible. Here every joystick SDL sees is opened
// raw into one of eight slots, its axes/buttons/hats passed on as the device
// numbers them (InputManager::GetJoystick*), and where it has force feedback a
// constant-force effect is kept running whose level follows
// InputManager::GetJoystickForce - set by the game each frame, played in Pump().
//============================================================================
namespace PyrosJoystick {

	struct Device {
		SDL_Joystick* stick = NULL;
		SDL_JoystickID instance = -1;
		SDL_Haptic* haptic = NULL;
		int effect = -1;
		SDL_HapticEffect def;
		Sint16 sent = 0;
		Uint32 lastRun = 0;
	};
	struct State {
		Device dev[p3d::InputManager::MaxJoysticks];
		bool started = false;
	};
	inline State &Get() { static State s; return s; }

	inline void Init()
	{
		State &s = Get();
		if (s.started) return;
		s.started = true;
		// (events while another window has the focus: a wheel is held while the
		// telemetry app or the menu bar is clicked)
		SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
		if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) return;
		SDL_InitSubSystem(SDL_INIT_HAPTIC);   // without it: input, no force
		SDL_JoystickEventState(SDL_ENABLE);
	}

	inline int SlotOf(const SDL_JoystickID id)
	{
		State &s = Get();
		for (int i = 0; i < (int)p3d::InputManager::MaxJoysticks; i++) if (s.dev[i].stick != NULL && s.dev[i].instance == id) return i;
		return -1;
	}

	inline void OpenForce(Device &d)
	{
		const char* jname = SDL_JoystickName(d.stick);
		const std::string who = std::string("[input] '") + (jname ? jname : "?") + "'";
		if (!SDL_JoystickIsHaptic(d.stick)) { echo(who + ": no force feedback (SDL: not a haptic device)"); return; }
		d.haptic = SDL_HapticOpenFromJoystick(d.stick);
		if (d.haptic == NULL) { echo(who + ": force feedback did not open: " + SDL_GetError()); return; }
		const unsigned int caps = SDL_HapticQuery(d.haptic);
		echo(who + ": force feedback opened, " + std::to_string(SDL_HapticNumAxes(d.haptic)) + " axes, effects 0x" + [](unsigned int c) { char b[16]; snprintf(b, sizeof(b), "%x", c); return std::string(b); }(caps)
			+ ((caps & SDL_HAPTIC_CONSTANT) ? " (constant force: yes)" : " (constant force: NO)"));
		if (!(caps & SDL_HAPTIC_CONSTANT)) { SDL_HapticClose(d.haptic); d.haptic = NULL; return; }
		// The game makes all of the force: no spring of the device's own pulling to centre.
		if (caps & SDL_HAPTIC_AUTOCENTER) SDL_HapticSetAutocenter(d.haptic, 0);
		if (caps & SDL_HAPTIC_GAIN) SDL_HapticSetGain(d.haptic, 100);
		SDL_memset(&d.def, 0, sizeof(d.def));
		d.def.type = SDL_HAPTIC_CONSTANT;
#ifdef SDL_HAPTIC_STEERING_AXIS
		d.def.constant.direction.type = SDL_HAPTIC_STEERING_AXIS;
#else
		d.def.constant.direction.type = SDL_HAPTIC_CARTESIAN;
#endif
		d.def.constant.direction.dir[0] = 1;
		d.def.constant.length = SDL_HAPTIC_INFINITY;
		d.def.constant.level = 0;
		d.effect = SDL_HapticNewEffect(d.haptic, &d.def);
#ifdef SDL_HAPTIC_STEERING_AXIS
		if (d.effect < 0)
		{
			// (older drivers: the plain first axis)
			d.def.constant.direction.type = SDL_HAPTIC_CARTESIAN;
			d.effect = SDL_HapticNewEffect(d.haptic, &d.def);
		}
#endif
		if (d.effect < 0) { echo(who + ": the constant force effect could not be made: " + SDL_GetError()); SDL_HapticClose(d.haptic); d.haptic = NULL; return; }
		if (SDL_HapticRunEffect(d.haptic, d.effect, 1) != 0) echo(who + ": the force effect did not start: " + SDL_GetError());
		else echo(who + ": force feedback running (constant force, " + (d.def.constant.direction.type == SDL_HAPTIC_CARTESIAN ? "cartesian x" : "steering axis") + ")");
		d.sent = 0;
		d.lastRun = SDL_GetTicks();
	}

	inline void Close(Device &d)
	{
		if (d.haptic) { if (d.effect >= 0) SDL_HapticStopEffect(d.haptic, d.effect); SDL_HapticClose(d.haptic); }
		if (d.stick) SDL_JoystickClose(d.stick);
		d = Device();
	}

	// True when the event was a joystick's (and is used up).
	inline bool HandleEvent(const SDL_Event &e)
	{
		State &s = Get();
		switch (e.type)
		{
		case SDL_JOYDEVICEADDED:
		{
			// `which` is a device INDEX here
			SDL_Joystick* j = SDL_JoystickOpen(e.jdevice.which);
			if (j == NULL) return true;
			const SDL_JoystickID id = SDL_JoystickInstanceID(j);
			if (SlotOf(id) >= 0) { SDL_JoystickClose(j); return true; }  // already open (SDL opens refcounted)
			for (int i = 0; i < (int)p3d::InputManager::MaxJoysticks; i++)
				if (s.dev[i].stick == NULL)
				{
					Device &d = s.dev[i];
					d.stick = j;
					d.instance = id;
					OpenForce(d);
					char guid[64];
					SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(j), guid, sizeof(guid));
					const char* name = SDL_JoystickName(j);
					echo(std::string("[input] joystick ") + std::to_string(i) + ": '" + (name ? name : "?") + "' " + std::to_string(SDL_JoystickNumAxes(j)) + " axes, "
						+ std::to_string(SDL_JoystickNumButtons(j)) + " buttons, " + std::to_string(SDL_JoystickNumHats(j)) + " hats, guid " + guid + (d.haptic ? ", force feedback" : ""));
					p3d::InputManager::SetJoystickConnected((p3d::uint32)i, true, name ? name : "Joystick", guid,
						(p3d::uint32)SDL_JoystickNumAxes(j), (p3d::uint32)SDL_JoystickNumButtons(j), (p3d::uint32)SDL_JoystickNumHats(j),
						d.haptic != NULL);
					// what the axes rest at now (a pedal at rest is not 0)
					for (int a = 0; a < SDL_JoystickNumAxes(j) && a < (int)p3d::InputManager::MaxJoystickAxes; a++)
					{
						const Sint16 v = SDL_JoystickGetAxis(j, a);
						p3d::InputManager::SetJoystickAxis((p3d::uint32)i, (p3d::uint32)a, v < 0 ? (p3d::f32)v / 32768.f : (p3d::f32)v / 32767.f);
					}
					return true;
				}
			SDL_JoystickClose(j);   // a ninth: no slot
			return true;
		}
		case SDL_JOYDEVICEREMOVED:
		{
			const int i = SlotOf(e.jdevice.which);
			if (i < 0) return true;
			Close(s.dev[i]);
			p3d::InputManager::SetJoystickConnected((p3d::uint32)i, false, "", "", 0, 0, 0, false);
			return true;
		}
		case SDL_JOYAXISMOTION:
		{
			const int i = SlotOf(e.jaxis.which);
			if (i >= 0) p3d::InputManager::SetJoystickAxis((p3d::uint32)i, e.jaxis.axis,
				e.jaxis.value < 0 ? (p3d::f32)e.jaxis.value / 32768.f : (p3d::f32)e.jaxis.value / 32767.f);
			return true;
		}
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
		{
			const int i = SlotOf(e.jbutton.which);
			if (i >= 0) p3d::InputManager::SetJoystickButton((p3d::uint32)i, e.jbutton.button, e.type == SDL_JOYBUTTONDOWN);
			return true;
		}
		case SDL_JOYHATMOTION:
		{
			const int i = SlotOf(e.jhat.which);
			if (i >= 0) p3d::InputManager::SetJoystickHat((p3d::uint32)i, e.jhat.hat, e.jhat.value);
			return true;
		}
		case SDL_JOYBALLMOTION:
			return true;
		default:
			return false;
		}
	}

	// Once a frame, after the events: the force each device was asked for.
	inline void Pump()
	{
		State &s = Get();
		for (int i = 0; i < (int)p3d::InputManager::MaxJoysticks; i++)
		{
			Device &d = s.dev[i];
			if (d.haptic == NULL || d.effect < 0) continue;
			const p3d::f32 f = p3d::InputManager::GetJoystickForce((p3d::uint32)i);
			const Sint16 level = (Sint16)(f * 32767.f);
			// (some DirectInput drivers drop a running effect - focus lost, the wheel's own software: start it again now and then)
			const Uint32 now = SDL_GetTicks();
			if (now - d.lastRun > 2000) { SDL_HapticRunEffect(d.haptic, d.effect, 1); d.lastRun = now; d.sent = (Sint16)(level + 1); }
			if (level == d.sent) continue;
			d.def.constant.level = level;
			if (SDL_HapticUpdateEffect(d.haptic, d.effect, &d.def) == 0) d.sent = level;
		}
	}

}

#endif /* PYROS_GAMEPADHOOK_H */
