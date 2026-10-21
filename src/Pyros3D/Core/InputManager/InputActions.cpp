//============================================================================
// Name        : InputActions.cpp
// Description : What the player means, bound to what they press.
//============================================================================

#include <Pyros3D/Core/InputManager/InputActions.h>
#include <Pyros3D/Core/InputManager/InputManager.h>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace p3d {

	f32 InputActions::deadZone = 0.2f;
	bool InputActions::capturing = false;
	std::string InputActions::captured;
	std::vector<f32> InputActions::captureBaseAxes;

	namespace {
		struct Named { const char* name; uint32 code; };
		// (the keyboard's names as the Lua Keyboard enum has them)
		const Named kKeys[] = {
			{"A",Event::Input::Keyboard::A},{"B",Event::Input::Keyboard::B},{"C",Event::Input::Keyboard::C},{"D",Event::Input::Keyboard::D},
			{"E",Event::Input::Keyboard::E},{"F",Event::Input::Keyboard::F},{"G",Event::Input::Keyboard::G},{"H",Event::Input::Keyboard::H},
			{"I",Event::Input::Keyboard::I},{"J",Event::Input::Keyboard::J},{"K",Event::Input::Keyboard::K},{"L",Event::Input::Keyboard::L},
			{"M",Event::Input::Keyboard::M},{"N",Event::Input::Keyboard::N},{"O",Event::Input::Keyboard::O},{"P",Event::Input::Keyboard::P},
			{"Q",Event::Input::Keyboard::Q},{"R",Event::Input::Keyboard::R},{"S",Event::Input::Keyboard::S},{"T",Event::Input::Keyboard::T},
			{"U",Event::Input::Keyboard::U},{"V",Event::Input::Keyboard::V},{"W",Event::Input::Keyboard::W},{"X",Event::Input::Keyboard::X},
			{"Y",Event::Input::Keyboard::Y},{"Z",Event::Input::Keyboard::Z},
			{"Num0",Event::Input::Keyboard::Num0},{"Num1",Event::Input::Keyboard::Num1},{"Num2",Event::Input::Keyboard::Num2},{"Num3",Event::Input::Keyboard::Num3},
			{"Num4",Event::Input::Keyboard::Num4},{"Num5",Event::Input::Keyboard::Num5},{"Num6",Event::Input::Keyboard::Num6},{"Num7",Event::Input::Keyboard::Num7},
			{"Num8",Event::Input::Keyboard::Num8},{"Num9",Event::Input::Keyboard::Num9},
			{"Escape",Event::Input::Keyboard::Escape},{"LControl",Event::Input::Keyboard::LControl},{"LShift",Event::Input::Keyboard::LShift},
			{"LAlt",Event::Input::Keyboard::LAlt},{"RControl",Event::Input::Keyboard::RControl},{"RShift",Event::Input::Keyboard::RShift},
			{"RAlt",Event::Input::Keyboard::RAlt},{"Space",Event::Input::Keyboard::Space},{"Return",Event::Input::Keyboard::Return},
			{"Back",Event::Input::Keyboard::Back},{"Tab",Event::Input::Keyboard::Tab},{"Left",Event::Input::Keyboard::Left},
			{"Right",Event::Input::Keyboard::Right},{"Up",Event::Input::Keyboard::Up},{"Down",Event::Input::Keyboard::Down},
			{"F1",Event::Input::Keyboard::F1},{"F2",Event::Input::Keyboard::F2},{"F3",Event::Input::Keyboard::F3},{"F4",Event::Input::Keyboard::F4},
			{"F5",Event::Input::Keyboard::F5},{"F6",Event::Input::Keyboard::F6},{"F7",Event::Input::Keyboard::F7},{"F8",Event::Input::Keyboard::F8},
			{"F9",Event::Input::Keyboard::F9},{"F10",Event::Input::Keyboard::F10},{"F11",Event::Input::Keyboard::F11},{"F12",Event::Input::Keyboard::F12},
			{"Comma",Event::Input::Keyboard::Comma},{"Period",Event::Input::Keyboard::Period},{"Slash",Event::Input::Keyboard::Slash},
			{"Tilde",Event::Input::Keyboard::Tilde},{"Delete",Event::Input::Keyboard::Delete},{"Home",Event::Input::Keyboard::Home},{"End",Event::Input::Keyboard::End},
		};
		const Named kMouse[] = { {"Left",Event::Input::Mouse::Left},{"Middle",Event::Input::Mouse::Middle},{"Right",Event::Input::Mouse::Right} };
		// (the pad's layout as GamepadHook and Lua's GamepadButton / GamepadAxis have it)
		const Named kPad[] = { {"A",0},{"B",1},{"X",2},{"Y",3},{"Back",4},{"Guide",5},{"Start",6},{"LeftStick",7},{"RightStick",8},
			{"LeftShoulder",9},{"RightShoulder",10},{"DpadUp",11},{"DpadDown",12},{"DpadLeft",13},{"DpadRight",14} };
		const Named kAxes[] = { {"LeftX",0},{"LeftY",1},{"RightX",2},{"RightY",3},{"LeftTrigger",4},{"RightTrigger",5} };
		template <size_t N> bool Lookup(const Named (&t)[N], const std::string &n, uint32 &out) { for (size_t i = 0; i < N; i++) if (n == t[i].name) { out = t[i].code; return true; } return false; }
		template <size_t N> const char* NameOf(const Named (&t)[N], const uint32 c) { for (size_t i = 0; i < N; i++) if (t[i].code == c) return t[i].name; return "?"; }
		std::string Trim(const std::string &s) { size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t"); return a == std::string::npos ? std::string() : s.substr(a, b - a + 1); }
	}

	std::map<std::string, InputActions::Action> &InputActions::Actions() { static std::map<std::string, Action> a; return a; }

	bool InputActions::Parse(const std::string &bindingIn, Binding &out)
	{
		const std::string binding = Trim(bindingIn);
		const size_t colon = binding.find(':');
		if (colon == std::string::npos) return false;
		std::string kind = binding.substr(0, colon), name = binding.substr(colon + 1);
		out.pad = -1; out.sign = 1.f;
		if (kind == "key") { out.kind = Binding::Key; return Lookup(kKeys, name, out.code); }
		if (kind == "mouse") { out.kind = Binding::Mouse; return Lookup(kMouse, name, out.code); }
		if (kind.compare(0, 3, "pad") == 0)
		{
			const bool axis = kind.compare(0, 7, "padaxis") == 0;
			const std::string rest = kind.substr(axis ? 7 : 3);
			if (!rest.empty()) out.pad = atoi(rest.c_str());
			if (axis)
			{
				out.kind = Binding::PadAxis;
				if (!name.empty() && (name.back() == '+' || name.back() == '-')) { out.sign = name.back() == '-' ? -1.f : 1.f; name.pop_back(); }
				return Lookup(kAxes, name, out.code);
			}
			out.kind = Binding::PadButton;
			return Lookup(kPad, name, out.code);
		}
		return false;
	}

	std::string InputActions::Describe(const Binding &b)
	{
		const std::string pad = b.pad >= 0 ? std::to_string(b.pad) : std::string();
		switch (b.kind)
		{
		case Binding::Key: return std::string("key:") + NameOf(kKeys, b.code);
		case Binding::Mouse: return std::string("mouse:") + NameOf(kMouse, b.code);
		case Binding::PadButton: return "pad" + pad + ":" + NameOf(kPad, b.code);
		case Binding::PadAxis: return "padaxis" + pad + ":" + NameOf(kAxes, b.code) + (b.sign < 0.f ? "-" : "+");
		}
		return std::string();
	}

	void InputActions::Bind(const std::string &action, const std::string &binding)
	{
		Binding b;
		if (!Parse(binding, b)) return;
		Action &a = Actions()[action];
		const std::string d = Describe(b);
		for (size_t i = 0; i < a.text.size(); i++) if (a.text[i] == d) return;
		a.text.push_back(d); a.parsed.push_back(b);
	}

	void InputActions::Unbind(const std::string &action, const std::string &binding)
	{
		std::map<std::string, Action>::iterator it = Actions().find(action);
		if (it == Actions().end()) return;
		if (binding.empty()) { it->second.text.clear(); it->second.parsed.clear(); return; }
		Binding b;
		if (!Parse(binding, b)) return;
		const std::string d = Describe(b);
		for (size_t i = 0; i < it->second.text.size(); i++)
			if (it->second.text[i] == d) { it->second.text.erase(it->second.text.begin() + i); it->second.parsed.erase(it->second.parsed.begin() + i); return; }
	}

	std::vector<std::string> InputActions::GetBindings(const std::string &action)
	{
		std::map<std::string, Action>::iterator it = Actions().find(action);
		return it == Actions().end() ? std::vector<std::string>() : it->second.text;
	}

	std::vector<std::string> InputActions::GetActions()
	{
		std::vector<std::string> out;
		for (std::map<std::string, Action>::iterator it = Actions().begin(); it != Actions().end(); ++it) out.push_back(it->first);
		return out;
	}

	f32 InputActions::Evaluate(const Action &a)
	{
		f32 v = 0.f;
		for (const Binding &b : a.parsed)
		{
			f32 x = 0.f;
			switch (b.kind)
			{
			case Binding::Key: x = InputManager::IsKeyDown(b.code) ? 1.f : 0.f; break;
			case Binding::Mouse: x = InputManager::IsMouseButtonDown(b.code) ? 1.f : 0.f; break;
			case Binding::PadButton:
				for (uint32 p = 0; p < 8; p++)
					if ((b.pad < 0 || (int32)p == b.pad) && InputManager::IsJoypadButtonDown(p, b.code)) x = 1.f;
				break;
			case Binding::PadAxis:
				for (uint32 p = 0; p < 8; p++)
				{
					if (b.pad >= 0 && (int32)p != b.pad) continue;
					if (!InputManager::IsJoypadConnected(p)) continue;
					const f32 raw = InputManager::GetJoypadAxis(p, b.code) * b.sign;
					if (raw > deadZone) { const f32 k = (raw - deadZone) / (1.f - deadZone); if (k > x) x = k > 1.f ? 1.f : k; }
				}
				break;
			}
			if (x > v) v = x;
		}
		return v;
	}

	void InputActions::NewFrame()
	{
		for (std::map<std::string, Action>::iterator it = Actions().begin(); it != Actions().end(); ++it)
		{
			Action &a = it->second;
			a.was = a.down;
			a.value = Evaluate(a);
			a.down = a.value >= 0.5f;
		}
		if (capturing && captured.empty())
		{
			for (const Named &k : kKeys) if (InputManager::IsKeyDown(k.code)) { captured = std::string("key:") + k.name; break; }
			if (captured.empty()) for (const Named &m : kMouse) if (InputManager::IsMouseButtonDown(m.code)) { captured = std::string("mouse:") + m.name; break; }
			for (uint32 p = 0; p < 8 && captured.empty(); p++)
			{
				if (!InputManager::IsJoypadConnected(p)) continue;
				for (const Named &b : kPad) if (InputManager::IsJoypadButtonDown(p, b.code)) { captured = std::string("pad:") + b.name; break; }
				for (const Named &ax : kAxes)
				{
					if (!captured.empty()) break;
					const f32 v = InputManager::GetJoypadAxis(p, ax.code), base = captureBaseAxes.size() > p * 6 + ax.code ? captureBaseAxes[p * 6 + ax.code] : 0.f;
					// (pushed half way from where it rested when the capture began: a trigger rests at 0, a stick near it)
					if (fabsf(v - base) > 0.5f) captured = std::string("padaxis:") + ax.name + (v - base < 0.f ? "-" : "+");
				}
			}
		}
	}


	f32 InputActions::Value(const std::string &action) { std::map<std::string, Action>::iterator it = Actions().find(action); return it == Actions().end() ? 0.f : it->second.value; }
	bool InputActions::Down(const std::string &action) { std::map<std::string, Action>::iterator it = Actions().find(action); return it != Actions().end() && it->second.down; }
	bool InputActions::Pressed(const std::string &action) { std::map<std::string, Action>::iterator it = Actions().find(action); return it != Actions().end() && it->second.down && !it->second.was; }
	bool InputActions::Released(const std::string &action) { std::map<std::string, Action>::iterator it = Actions().find(action); return it != Actions().end() && !it->second.down && it->second.was; }
	f32 InputActions::Axis(const std::string &negative, const std::string &positive) { return Value(positive) - Value(negative); }

	void InputActions::BeginCapture()
	{
		capturing = true; captured.clear();
		captureBaseAxes.assign(8 * 6, 0.f);
		for (uint32 p = 0; p < 8; p++) for (uint32 a = 0; a < 6; a++) captureBaseAxes[p * 6 + a] = InputManager::GetJoypadAxis(p, a);
	}

	std::string InputActions::Capture()
	{
		if (!capturing || captured.empty()) return std::string();
		capturing = false;
		return captured;
	}

	std::string InputActions::Serialize()
	{
		std::string out;
		for (std::map<std::string, Action>::iterator it = Actions().begin(); it != Actions().end(); ++it)
		{
			if (!out.empty()) out += ";";
			out += it->first + "=";
			for (size_t i = 0; i < it->second.text.size(); i++) out += (i ? "," : "") + it->second.text[i];
		}
		return out;
	}

	bool InputActions::Deserialize(const std::string &text)
	{
		std::map<std::string, Action> fresh;
		std::stringstream entries(text);
		std::string entry;
		bool ok = true;
		while (std::getline(entries, entry, ';'))
		{
			const size_t eq = entry.find('=');
			if (eq == std::string::npos) { if (!Trim(entry).empty()) ok = false; continue; }
			const std::string name = Trim(entry.substr(0, eq));
			Action &a = fresh[name];
			std::stringstream bs(entry.substr(eq + 1));
			std::string one;
			while (std::getline(bs, one, ','))
			{
				Binding b;
				if (Parse(one, b)) { a.text.push_back(Describe(b)); a.parsed.push_back(b); }
				else if (!Trim(one).empty()) ok = false;
			}
		}
		Actions().swap(fresh);
		return ok;
	}
}
