//============================================================================
// Name        : InputManager.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Input Manager
//============================================================================

#include <unordered_set>
#include <Pyros3D/Core/InputManager/InputManager.h>

namespace p3d {

	// (file statics, not members: MSVC will not have them declared in an exported class)
	static f32 mouseDX = 0.f;
	static f32 mouseDY = 0.f;
	uint32 InputManager::mouseX = 0;
	uint32 InputManager::mouseY = 0;

	std::map<uint32, Gallant::Signal1<Event::Input::Info> > InputManager::EventsMapPressed;
	std::map<uint32, Gallant::Signal1<Event::Input::Info> > InputManager::EventsMapReleased;
	Gallant::Signal1<Event::Input::Info> InputManager::MouseWheelEvents;
	Gallant::Signal1<Event::Input::Info> InputManager::MouseMoveEvents;
	Gallant::Signal1<Event::Input::Info> InputManager::CharacterEnteredPress;
	Gallant::Signal1<Event::Input::Info> InputManager::CharacterEnteredRelease;
	uchar InputManager::Code;

	// Joypad
	std::map <uint32, std::map<uint32, Gallant::Signal1<Event::Input::Info> > > InputManager::JoyPadPressed;
	std::map <uint32, std::map<uint32, Gallant::Signal1<Event::Input::Info> > > InputManager::JoyPadReleased;
	std::map <uint32, std::map<uint32, Gallant::Signal1<Event::Input::Info> > > InputManager::JoyPadMove;

	namespace {
		// What each pad is doing right now, for whoever asks rather than
		// listens. The events below have always existed; nothing kept the
		// state they describe.
		struct PadState {
			bool connected = false;
			std::string name;
			uint32 buttons = 0;         // a bit a button: 32 are plenty
			f32 axes[8] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
		};
		PadState &Pad(const uint32 id) { static PadState pads[8]; return pads[id < 8 ? id : 7]; }
		std::unordered_set<uint32> &MouseDown() { static std::unordered_set<uint32> b; return b; }
		const std::string kNoPadName;
	}

	namespace { bool g_gameInputWithheld = false; }
	void InputManager::SetGameInputWithheld(const bool withheld) { g_gameInputWithheld = withheld; }
	bool InputManager::IsGameInputWithheld() { return g_gameInputWithheld; }

	bool InputManager::IsMouseButtonDown(const uint32 e) { return !g_gameInputWithheld && MouseDown().count(e) != 0; }
	bool InputManager::IsJoypadConnected(const uint32 joypadID) { return joypadID < 8 && Pad(joypadID).connected; }
	const std::string &InputManager::GetJoypadName(const uint32 joypadID) { return joypadID < 8 ? Pad(joypadID).name : kNoPadName; }
	bool InputManager::IsJoypadButtonDown(const uint32 joypadID, const uint32 button)
	{
		return !g_gameInputWithheld && joypadID < 8 && button < 32 && (Pad(joypadID).buttons & (1u << button)) != 0;
	}
	f32 InputManager::GetJoypadAxis(const uint32 joypadID, const uint32 axis)
	{
		return (!g_gameInputWithheld && joypadID < 8 && axis < 8) ? Pad(joypadID).axes[axis] : 0.f;
	}
	void InputManager::SetJoypadConnected(const uint32 joypadID, const bool connected, const std::string &name)
	{
		if (joypadID >= 8) return;
		PadState &p = Pad(joypadID);
		p.connected = connected;
		p.name = name;
		p.buttons = 0;
		for (int i = 0; i < 8; i++) p.axes[i] = 0.f;
	}

	void InputManager::JoypadButtonPressed(const uint32 joypadID, const uint32 button)
	{
		if (joypadID < 8 && button < 32) Pad(joypadID).buttons |= (1u << button);
		Event::Input::Info m;
		m.Type = Event::Type::OnPress;
		m.Input = button;
		m.Value = button;
		JoyPadPressed[joypadID][button](m);
	}
	void InputManager::JoypadButtonReleased(const uint32 joypadID, const uint32 button)
	{
		if (joypadID < 8 && button < 32) Pad(joypadID).buttons &= ~(1u << button);
		Event::Input::Info m;
		m.Type = Event::Type::OnRelease;
		m.Input = button;
		m.Value = button;
		JoyPadReleased[joypadID][button](m);
	}
	void InputManager::JoypadMove(const uint32 joypadID, const uint32 axis, const f32 value)
	{
		if (joypadID < 8 && axis < 8) Pad(joypadID).axes[axis] = value;
		Event::Input::Info m;
		m.Type = Event::Type::OnMove;
		m.Input = axis;
		m.Value = value;
		JoyPadMove[joypadID][axis](m);
	}

	void InputManager::SetMousePosition(const uint32 mousex, const uint32 mousey)
	{
		mouseX = mousex;
		mouseY = mousey;

		Event::Input::Info m;
		m.Type = Event::Type::OnMove;
		m.Input = Event::Input::Mouse::Move;
		m.Value = Vec2((f32)mouseX, (f32)mouseY);
		MouseMoveEvents(m);
	}
	void InputManager::SetMouseWheel(const f32 delta)
	{
		Event::Input::Info m;
		m.Type = Event::Type::OnMove;
		m.Input = Event::Input::Mouse::Wheel;
		m.Value = delta;
		MouseWheelEvents(m);
	}
	void InputManager::AddMouseDelta(const f32 dx, const f32 dy)
	{
		mouseDX += dx;
		mouseDY += dy;
	}
	Vec2 InputManager::ConsumeMouseDelta()
	{
		const Vec2 d(mouseDX, mouseDY);
		mouseDX = mouseDY = 0.f;
		return g_gameInputWithheld ? Vec2(0.f, 0.f) : d;
	}
	namespace {
		std::vector<InputManager::Touch> &Touches()
		{
			static std::vector<InputManager::Touch> touches;
			return touches;
		}
	}
	const std::vector<InputManager::Touch> &InputManager::GetTouches() { return Touches(); }
	void InputManager::BeginTouchFrame()
	{
		std::vector<Touch> &t = Touches();
		for (size_t i = t.size(); i > 0; i--)
		{
			if (t[i - 1].ended) t.erase(t.begin() + (i - 1));
			else { t[i - 1].began = false; t[i - 1].previous = t[i - 1].position; }
		}
	}
	void InputManager::TouchBegan(const int64 id, const f32 x, const f32 y)
	{
		std::vector<Touch> &t = Touches();
		// (a finger that comes down while its number is still held here: the lift before it never came -
		// iOS cancels touches, and reuses their numbers. It is a new touch, not the old one moving.)
		for (size_t i = 0; i < t.size(); i++)
			if (t[i].id == id && !t[i].ended) { t[i].position = t[i].start = t[i].previous = Vec2(x, y); t[i].began = true; return; }
		Touch n;
		n.id = id;
		n.position = n.start = n.previous = Vec2(x, y);
		n.began = true;
		n.ended = false;
		t.push_back(n);
	}
	void InputManager::TouchMoved(const int64 id, const f32 x, const f32 y)
	{
		std::vector<Touch> &t = Touches();
		for (size_t i = 0; i < t.size(); i++)
			if (t[i].id == id && !t[i].ended) { t[i].position = Vec2(x, y); return; }
	}
	void InputManager::TouchEnded(const int64 id, const f32 x, const f32 y)
	{
		std::vector<Touch> &t = Touches();
		for (size_t i = 0; i < t.size(); i++)
			if (t[i].id == id && !t[i].ended) { t[i].position = Vec2(x, y); t[i].ended = true; return; }
	}
	Vec2 InputManager::GetMousePosition()
	{
		return Vec2((f32)mouseX, (f32)mouseY);
	}
	void InputManager::MousePressed(const uint32 e)
	{
		MouseDown().insert(e);
		if (g_gameInputWithheld) return;
		Event::Input::Info m;
		m.Type = Event::Type::OnPress;
		m.Input = e;
		m.Value = e;
		EventsMapPressed[e](m);
	}
	void InputManager::MouseReleased(const uint32 e)
	{
		MouseDown().erase(e);
		if (g_gameInputWithheld) return;
		Event::Input::Info m;
		m.Type = Event::Type::OnRelease;
		m.Input = e;
		m.Value = e;
		EventsMapReleased[e](m);
	}

	static std::unordered_set<uint32> &KeysDown() { static std::unordered_set<uint32> keys; return keys; }
	bool InputManager::IsKeyDown(const uint32 e) { return !g_gameInputWithheld && KeysDown().count(e) != 0; }

	void InputManager::KeyPressed(const uint32 e)
	{
		KeysDown().insert(e);
		if (g_gameInputWithheld) return;

		Event::Input::Info k;
		k.Type = Event::Type::OnPress;
		k.Input = e;
		k.Value = e;
		EventsMapPressed[e](k);

	}
	void InputManager::KeyReleased(const uint32 e)
	{
		KeysDown().erase(e);
		if (g_gameInputWithheld) return;

		Event::Input::Info k;
		k.Type = Event::Type::OnRelease;
		k.Input = e;
		k.Value = e;
		EventsMapReleased[e](k);
	}

	void InputManager::CharEntered(const uint32 e)
	{
		if (g_gameInputWithheld) return;
		// Could be on Pressed or Released, there is nothing different from them
		Code = e;
		Event::Input::Info kPress;
		kPress.Type = Event::Type::OnPress;
		kPress.Input = e;
		kPress.Value = e;
		EventsMapPressed[Event::Input::Keyboard::OtherKeyboardEvents::CharacterEnter](kPress);

		Event::Input::Info kRelease;
		kRelease.Type = Event::Type::OnRelease;
		kRelease.Input = e;
		kRelease.Value = e;
		EventsMapReleased[Event::Input::Keyboard::OtherKeyboardEvents::CharacterEnter](kRelease);
	}
};
