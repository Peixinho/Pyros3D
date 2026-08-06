//============================================================================
// Name        : PyrosLuaMisc.cpp
// Description : Input bridge, File, placeDecalAtCursor.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>

namespace p3d {

	void RegisterLuaMisc(sol::state* lua)
	{
		{
			// Input - real keyboard/mouse enums plus the LuaInputBridge
			// registration API (see PyrosBindings.h's LuaInputBridge
			// class comment for why a bridge object is needed instead of
			// binding InputManager directly).
			lua->new_enum("Key",
				"A", Event::Input::Keyboard::A, "B", Event::Input::Keyboard::B, "C", Event::Input::Keyboard::C,
				"D", Event::Input::Keyboard::D, "E", Event::Input::Keyboard::E, "F", Event::Input::Keyboard::F,
				"G", Event::Input::Keyboard::G, "H", Event::Input::Keyboard::H, "I", Event::Input::Keyboard::I,
				"J", Event::Input::Keyboard::J, "K", Event::Input::Keyboard::K, "L", Event::Input::Keyboard::L,
				"M", Event::Input::Keyboard::M, "N", Event::Input::Keyboard::N, "O", Event::Input::Keyboard::O,
				"P", Event::Input::Keyboard::P, "Q", Event::Input::Keyboard::Q, "R", Event::Input::Keyboard::R,
				"S", Event::Input::Keyboard::S, "T", Event::Input::Keyboard::T, "U", Event::Input::Keyboard::U,
				"V", Event::Input::Keyboard::V, "W", Event::Input::Keyboard::W, "X", Event::Input::Keyboard::X,
				"Y", Event::Input::Keyboard::Y, "Z", Event::Input::Keyboard::Z,
				"Num0", Event::Input::Keyboard::Num0, "Num1", Event::Input::Keyboard::Num1,
				"Num2", Event::Input::Keyboard::Num2, "Num3", Event::Input::Keyboard::Num3,
				"Num4", Event::Input::Keyboard::Num4, "Num5", Event::Input::Keyboard::Num5,
				"Num6", Event::Input::Keyboard::Num6, "Num7", Event::Input::Keyboard::Num7,
				"Num8", Event::Input::Keyboard::Num8, "Num9", Event::Input::Keyboard::Num9,
				"Escape", Event::Input::Keyboard::Escape,
				"LControl", Event::Input::Keyboard::LControl, "LShift", Event::Input::Keyboard::LShift,
				"LAlt", Event::Input::Keyboard::LAlt, "RControl", Event::Input::Keyboard::RControl,
				"RShift", Event::Input::Keyboard::RShift, "RAlt", Event::Input::Keyboard::RAlt,
				"Space", Event::Input::Keyboard::Space, "Return", Event::Input::Keyboard::Return,
				"Back", Event::Input::Keyboard::Back, "Tab", Event::Input::Keyboard::Tab,
				"Left", Event::Input::Keyboard::Left, "Right", Event::Input::Keyboard::Right,
				"Up", Event::Input::Keyboard::Up, "Down", Event::Input::Keyboard::Down,
				"F1", Event::Input::Keyboard::F1, "F2", Event::Input::Keyboard::F2,
				"F3", Event::Input::Keyboard::F3, "F4", Event::Input::Keyboard::F4,
				"F5", Event::Input::Keyboard::F5, "F6", Event::Input::Keyboard::F6,
				"F7", Event::Input::Keyboard::F7, "F8", Event::Input::Keyboard::F8,
				"F9", Event::Input::Keyboard::F9, "F10", Event::Input::Keyboard::F10,
				"F11", Event::Input::Keyboard::F11, "F12", Event::Input::Keyboard::F12
			);
			lua->new_enum("MouseButton",
				"Left", Event::Input::Mouse::Left,
				"Middle", Event::Input::Mouse::Middle,
				"Right", Event::Input::Mouse::Right
			);

			sol::constructors<sol::types<>> con;
			lua->new_usertype<LuaInputBridge>("Input",
				con,
				"onKeyPressed", &LuaInputBridge::OnKeyPressed,
				"onKeyReleased", &LuaInputBridge::OnKeyReleased,
				"onMouseButtonPressed", &LuaInputBridge::OnMouseButtonPressed,
				"onMouseButtonReleased", &LuaInputBridge::OnMouseButtonReleased,
				"onMouseMoved", &LuaInputBridge::OnMouseMoved,
				"onMouseWheelMoved", &LuaInputBridge::OnMouseWheelMoved
				);
		}

		{
			//File
			sol::constructors<sol::types<>> con;
			lua->new_usertype<File>("File",
				con,
				"open", &File::Open,
				"write", &File::Write,
				"read", &File::Read,
				"rewind", &File::Rewind,
				"close", &File::Close,
				"size", &File::Size,
				"getData", &File::GetData
				);
		}

		lua->set_function("getMousePosition", []() {
			Vec2 p = InputManager::GetMousePosition();
			return std::make_tuple(p.x, p.y);
		});
		lua->set_function("placeDecalAtCursor", &PlaceDecalAtCursor);
	}

} // namespace p3d

#endif
