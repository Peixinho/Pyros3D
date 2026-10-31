//============================================================================
// Name        : TouchControls.cpp
// Author      : Duarte Peixinho
// Description : See TouchControls.h.
//============================================================================

#include <Pyros3D/Core/InputManager/TouchControls.h>
#include <Pyros3D/Core/InputManager/InputManager.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Rendering/Components/UI/UIImage.h>
#include <Pyros3D/Rendering/Components/UI/UIText.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Assets/Font/Font.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace p3d {

	using namespace LOG;

	namespace {
		using json = nlohmann::json;

		// The key names a layout file uses: Lua's Key table's (Key.W is "W").
		const std::map<std::string, uint32> &KeyNames()
		{
			static std::map<std::string, uint32> names;
			if (!names.empty()) return names;
			using namespace Event::Input;
			const uint32 letters[26] = { Keyboard::A, Keyboard::B, Keyboard::C, Keyboard::D, Keyboard::E, Keyboard::F, Keyboard::G, Keyboard::H,
				Keyboard::I, Keyboard::J, Keyboard::K, Keyboard::L, Keyboard::M, Keyboard::N, Keyboard::O, Keyboard::P, Keyboard::Q, Keyboard::R,
				Keyboard::S, Keyboard::T, Keyboard::U, Keyboard::V, Keyboard::W, Keyboard::X, Keyboard::Y, Keyboard::Z };
			for (int i = 0; i < 26; i++) names[std::string(1, (char)('A' + i))] = letters[i];
			const uint32 digits[10] = { Keyboard::Num0, Keyboard::Num1, Keyboard::Num2, Keyboard::Num3, Keyboard::Num4,
				Keyboard::Num5, Keyboard::Num6, Keyboard::Num7, Keyboard::Num8, Keyboard::Num9 };
			for (int i = 0; i < 10; i++) names["Num" + std::to_string(i)] = digits[i];
			names["Escape"] = Keyboard::Escape; names["Space"] = Keyboard::Space; names["Return"] = Keyboard::Return;
			names["Back"] = Keyboard::Back; names["Tab"] = Keyboard::Tab;
			names["LControl"] = Keyboard::LControl; names["LShift"] = Keyboard::LShift; names["LAlt"] = Keyboard::LAlt;
			names["RControl"] = Keyboard::RControl; names["RShift"] = Keyboard::RShift; names["RAlt"] = Keyboard::RAlt;
			names["Left"] = Keyboard::Left; names["Right"] = Keyboard::Right; names["Up"] = Keyboard::Up; names["Down"] = Keyboard::Down;
			const uint32 fkeys[12] = { Keyboard::F1, Keyboard::F2, Keyboard::F3, Keyboard::F4, Keyboard::F5, Keyboard::F6,
				Keyboard::F7, Keyboard::F8, Keyboard::F9, Keyboard::F10, Keyboard::F11, Keyboard::F12 };
			for (int i = 0; i < 12; i++) names["F" + std::to_string(i + 1)] = fkeys[i];
			return names;
		}
		int32 KeyOf(const json &j, const char* field)
		{
			if (!j.contains(field) || !j[field].is_string()) return -1;
			const std::map<std::string, uint32>::const_iterator k = KeyNames().find(j[field].get<std::string>());
			return k == KeyNames().end() ? -1 : (int32)k->second;
		}
		std::string NameOfKey(const int32 key)
		{
			for (const auto &k : KeyNames()) if ((int32)k.second == key) return k.first;
			return std::string();
		}
		int32 MouseOf(const std::string &name)
		{
			if (name == "Left") return Event::Input::Mouse::Left;
			if (name == "Right") return Event::Input::Mouse::Right;
			if (name == "Middle") return Event::Input::Mouse::Middle;
			return -1;
		}
		std::string NameOfMouse(const int32 b)
		{
			if (b == (int32)Event::Input::Mouse::Left) return "Left";
			if (b == (int32)Event::Input::Mouse::Right) return "Right";
			if (b == (int32)Event::Input::Mouse::Middle) return "Middle";
			return std::string();
		}

		// What a first-person game wants, where it has said nothing: a stick for W A S D (Shift at full
		// tilt) under the left thumb, the right half to look, and under the right thumb fire, aim, jump,
		// crouch, reload, use - and the inventory/menu key (the pointer freed: the controls step aside).
		const char* kDefaultLayout = R"({ "controls": [
			{ "id": "look", "type": "look", "x": 0.4, "y": 0.0, "w": 0.6, "h": 1.0, "sensitivity": 0.6 },
			{ "id": "move", "type": "stick", "x": 0.15, "y": 0.70, "size": 0.34, "up": "W", "down": "S", "left": "A", "right": "D", "run": "LShift" },
			{ "id": "fire", "type": "button", "x": 0.88, "y": 0.62, "size": 0.20, "mouse": "Left", "label": "FIRE" },
			{ "id": "aim", "type": "button", "x": 0.73, "y": 0.80, "size": 0.14, "mouse": "Right", "label": "AIM" },
			{ "id": "jump", "type": "button", "x": 0.92, "y": 0.86, "size": 0.14, "key": "Space", "label": "JUMP" },
			{ "id": "crouch", "type": "button", "x": 0.80, "y": 0.92, "size": 0.11, "key": "C", "label": "CROUCH" },
			{ "id": "reload", "type": "button", "x": 0.75, "y": 0.42, "size": 0.11, "key": "R", "label": "RELOAD" },
			{ "id": "use", "type": "button", "x": 0.88, "y": 0.36, "size": 0.11, "key": "F", "label": "USE" },
			{ "id": "menu", "type": "button", "x": 0.95, "y": 0.08, "size": 0.10, "key": "Tab", "label": "BAG" },
			{ "id": "layout", "type": "edit", "x": 0.05, "y": 0.08, "size": 0.09, "label": "LAYOUT" }
		] })";

		// A filled disc (or a ring) with a soft edge, white: tinted by the image that shows it.
		std::shared_ptr<Texture> MakeCircle(const bool ringOnly)
		{
			const int n = 128;
			std::vector<uchar> px((size_t)n * n * 4, 255);
			for (int y = 0; y < n; y++)
				for (int x = 0; x < n; x++)
				{
					const f32 dx = (x + 0.5f) / n * 2.f - 1.f, dy = (y + 0.5f) / n * 2.f - 1.f;
					const f32 r = sqrtf(dx * dx + dy * dy);
					f32 a = std::min(1.f, std::max(0.f, (1.f - r) * n * 0.5f));          // the outer edge, a pixel soft
					if (ringOnly) a *= std::min(1.f, std::max(0.f, (r - 0.86f) * n * 0.5f));   // and the inner, for a ring
					px[((size_t)y * n + x) * 4 + 3] = (uchar)(a * 255.f);
				}
			std::shared_ptr<Texture> t = std::make_shared<Texture>();
			t->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, n, n, false);
			t->UpdateData(px.data());
			return t;
		}

		json V4(const Vec4 &v) { return json::array({ v.x, v.y, v.z, v.w }); }
		json RectJ() { return { { "type", "UIRect" }, { "anchorMin", { 0.5f, 0.5f } }, { "anchorMax", { 0.5f, 0.5f } }, { "offsetMin", { 0, 0 } }, { "offsetMax", { 0, 0 } }, { "pivot", { 0.5f, 0.5f } } }; }
		json ImageJ(const Vec4 &tint) { return { { "type", "UIImage" }, { "tint", V4(tint) }, { "border", { 0, 0, 0, 0 } } }; }
		json TextJ(const std::string &text, const f32 size)
		{
			return { { "type", "UIText" }, { "text", text }, { "font", Font::BuiltinDefault() }, { "fontSDF", true }, { "fontSize", 48.f },
					 { "size", size }, { "color", V4(Vec4(1, 1, 1, 0.9f)) }, { "align", "Center" }, { "verticalAlign", "Middle" } };
		}
		json NodeJ(const std::string &name, const json &components, const json &children = json::array())
		{
			return { { "name", name }, { "position", { 0, 0, 0 } }, { "rotation", { 0, 0, 0 } }, { "scale", { 1, 1, 1 } },
					 { "static", false }, { "tags", json::array() }, { "components", components }, { "children", children } };
		}
		template <class T> T* ComponentOf(GameObject* go, const uint32 type)
		{
			if (!go) return NULL;
			const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
			for (size_t i = 0; i < cs.size(); i++) if (cs[i] && cs[i]->GetComponentType() == type) return static_cast<T*>(cs[i].get());
			return NULL;
		}
		GameObject* FindIn(GameObject* go, const std::string &name)
		{
			if (!go) return NULL;
			if (go->GetName() == name) return go;
			for (const std::shared_ptr<GameObject> &k : go->GetChildren()) if (GameObject* f = FindIn(k.get(), name)) return f;
			return NULL;
		}

		// The canvas is 720 units tall, as wide as the screen's shape makes it (MatchHeight).
		const f32 kCanvasH = 720.f;
		// Edit mode's two buttons, at the top middle: where, as fractions of the screen
		// (not at the very top: that strip is iOS's own, its swipe down from the edge)
		const Vec2 kDoneAt(0.42f, 0.16f), kResetAt(0.58f, 0.16f);
		const Vec2 kEditButton(0.13f, 0.08f);       // their width (of the screen's width) and height
	}

	TouchControls &TouchControls::Instance()
	{
		static TouchControls* instance = new TouchControls();     // (never destroyed: no textures freed after the device)
		return *instance;
	}

	TouchControls::TouchControls()
	{
#if defined(__APPLE__) && TARGET_OS_IPHONE
		enabled = true;
#elif defined(__ANDROID__)
		enabled = true;
#endif
		if (const char* e = getenv("PYROS_TOUCH")) enabled = e[0] == '1';
	}

	TouchControls::Control* TouchControls::Find(const std::string &id)
	{
		for (size_t i = 0; i < controls.size(); i++) if (controls[i].id == id) return &controls[i];
		return NULL;
	}

	bool TouchControls::LoadLayoutText(const std::string &jsonText)
	{
		json j;
		try { j = json::parse(jsonText); }
		catch (const std::exception &e) { echo(std::string("ERROR: touch controls layout is not valid JSON - ") + e.what()); return false; }
		if (!j.contains("controls") || !j["controls"].is_array()) { echo("ERROR: touch controls layout has no \"controls\" list"); return false; }
		std::vector<Control> next;
		for (const json &c : j["controls"])
		{
			Control k;
			k.id = c.value("id", std::string("control") + std::to_string(next.size()));
			k.type = c.value("type", std::string("button"));
			k.label = c.value("label", std::string());
			k.x = c.value("x", k.x); k.y = c.value("y", k.y); k.size = c.value("size", k.size);
			k.w = c.value("w", k.w); k.h = c.value("h", k.h);
			k.up = KeyOf(c, "up"); k.down = KeyOf(c, "down"); k.left = KeyOf(c, "left"); k.right = KeyOf(c, "right"); k.run = KeyOf(c, "run");
			k.key = KeyOf(c, "key");
			k.mouse = c.contains("mouse") && c["mouse"].is_string() ? MouseOf(c["mouse"].get<std::string>()) : -1;
			k.toggle = c.value("toggle", false);
			k.sensitivity = c.value("sensitivity", 1.f);
			k.visible = c.value("visible", true);
			if (k.type != "stick" && k.type != "button" && k.type != "look" && k.type != "edit") { echo("WARNING: touch control '" + k.id + "' has an unknown type '" + k.type + "'"); continue; }
			next.push_back(k);
		}
		ReleaseAll();
		controls = next;
		Teardown();
		return true;
	}

	bool TouchControls::LoadLayoutFile(const std::string &path)
	{
		std::ifstream in(path.c_str());
		if (!in.good()) return false;
		std::stringstream ss; ss << in.rdbuf();
		return LoadLayoutText(ss.str());
	}

	void TouchControls::LoadDefaultLayout() { LoadLayoutText(kDefaultLayout); }

	void TouchControls::LoadPreferredLayout()
	{
		if (!userLayoutPath.empty() && LoadLayoutFile(userLayoutPath)) return;
		if (!gameLayoutPath.empty() && LoadLayoutFile(gameLayoutPath)) return;
		LoadDefaultLayout();
	}

	void TouchControls::ResetLayout()
	{
		if (!userLayoutPath.empty()) std::remove(userLayoutPath.c_str());
		if (gameLayoutPath.empty() || !LoadLayoutFile(gameLayoutPath)) LoadDefaultLayout();
	}

	std::string TouchControls::LayoutText() const
	{
		json list = json::array();
		for (const Control &c : controls)
		{
			json k = { { "id", c.id }, { "type", c.type } };
			if (!c.label.empty()) k["label"] = c.label;
			if (c.type == "look") { k["x"] = c.x; k["y"] = c.y; k["w"] = c.w; k["h"] = c.h; k["sensitivity"] = c.sensitivity; }
			else { k["x"] = c.x; k["y"] = c.y; k["size"] = c.size; }
			const struct { const char* name; int32 key; } keys[] = { { "up", c.up }, { "down", c.down }, { "left", c.left }, { "right", c.right }, { "run", c.run }, { "key", c.key } };
			for (const auto &e : keys) if (e.key >= 0) k[e.name] = NameOfKey(e.key);
			if (c.mouse >= 0) k["mouse"] = NameOfMouse(c.mouse);
			if (c.toggle) k["toggle"] = true;
			if (!c.visible) k["visible"] = false;
			list.push_back(k);
		}
		return json({ { "controls", list } }).dump(1, '\t');
	}

	bool TouchControls::SaveUserLayout() const
	{
		if (userLayoutPath.empty()) return false;
		std::ofstream out(userLayoutPath.c_str());
		if (!out.good()) { echo("WARNING: touch controls layout not kept - " + userLayoutPath + " cannot be written"); return false; }
		out << LayoutText();
		return true;
	}

	void TouchControls::SetEnabled(const bool on)
	{
		if (on == enabled) return;
		enabled = on;
		if (!on) { ReleaseAll(); editing = false; }
	}

	void TouchControls::SetLive(const bool on)
	{
		if (on == live) return;
		live = on;
		if (!on) ReleaseAll();
	}

	void TouchControls::SetEditing(const bool on)
	{
		if (on == editing) return;
		ReleaseAll();
		editing = on;
		editHasFinger = pinchHas = false; editControl = -1;
		InputManager::SetGameInputWithheld(on);
		if (!on) SaveUserLayout();
	}

	void TouchControls::Press(const Control &c, const bool down)
	{
		if (c.key >= 0) { if (down) { if (emitter.keyDown) emitter.keyDown((uint32)c.key); } else if (emitter.keyUp) emitter.keyUp((uint32)c.key); }
		if (c.mouse >= 0) { if (down) { if (emitter.mouseDown) emitter.mouseDown((uint32)c.mouse); } else if (emitter.mouseUp) emitter.mouseUp((uint32)c.mouse); }
	}

	void TouchControls::ReleaseAll()
	{
		// (every key and button a control holds, let go - none may stay down behind a menu)
		for (std::map<int32, StickState>::iterator s = sticks.begin(); s != sticks.end(); ++s)
		{
			if (s->first < 0 || s->first >= (int32)controls.size()) continue;
			const Control &c = controls[s->first];
			const struct { bool on; int32 key; } ks[] = { { s->second.up, c.up }, { s->second.down, c.down }, { s->second.left, c.left }, { s->second.right, c.right }, { s->second.run, c.run } };
			for (const auto &k : ks) if (k.on && k.key >= 0 && emitter.keyUp) emitter.keyUp((uint32)k.key);
		}
		sticks.clear();
		for (std::map<int32, bool>::iterator b = buttonDown.begin(); b != buttonDown.end(); ++b)
			if (b->second && b->first >= 0 && b->first < (int32)controls.size()) Press(controls[b->first], false);
		buttonDown.clear();
		held.clear();
	}

	int32 TouchControls::HitControl(const Vec2 &p, const uint32 width, const uint32 height) const
	{
		// Buttons first (they sit on the look area), then sticks - each a little more forgiving than
		// it is drawn, a thumb being what it is - then the look areas.
		for (int pass = 0; pass < 3; pass++)
			for (int32 i = (int32)controls.size() - 1; i >= 0; i--)
			{
				const Control &c = controls[i];
				if (!c.visible) continue;
				if (pass == 0 && c.type != "button" && c.type != "edit") continue;
				if (pass == 1 && c.type != "stick") continue;
				if (pass == 2 && c.type != "look") continue;
				if (c.type == "look")
				{
					const f32 fx = p.x / (f32)width, fy = p.y / (f32)height;
					if (fx >= c.x && fx <= c.x + c.w && fy >= c.y && fy <= c.y + c.h) return i;
					continue;
				}
				const Vec2 centre(c.x * width, c.y * height);
				const f32 r = c.size * height * 0.5f * (c.type == "stick" ? 1.35f : 1.15f);
				if ((p - centre).magnitude() <= r) return i;
			}
		return -1;
	}

	void TouchControls::Update(const uint32 width, const uint32 height)
	{
		if (!enabled || width == 0 || height == 0) return;
		const std::vector<InputManager::Touch> &touches = InputManager::GetTouches();

		if (editing)
		{
			for (const InputManager::Touch &t : touches)
			{
				if (t.began)
				{
					const Vec2 f(t.position.x / width, t.position.y / height);
					// Done and Reset
					if (fabsf(f.x - kDoneAt.x) < kEditButton.x * 0.5f && fabsf(f.y - kDoneAt.y) < kEditButton.y * 0.5f) { SetEditing(false); return; }
					if (fabsf(f.x - kResetAt.x) < kEditButton.x * 0.5f && fabsf(f.y - kResetAt.y) < kEditButton.y * 0.5f) { ResetLayout(); continue; }
					if (editHasFinger && t.id == editFinger) { editHasFinger = false; pinchHas = false; editControl = -1; }   // (its lift was lost)
					if (!editHasFinger)
					{
						const int32 hit = HitControl(t.position, width, height);
						if (hit >= 0)
						{
							editHasFinger = true; editFinger = t.id; editControl = hit;
							editGrab = f - Vec2(controls[hit].x, controls[hit].y);
						}
					}
					else if (!pinchHas && editControl >= 0)
					{
						// a second finger: the distance between the two sizes the control
						pinchHas = true; pinchFinger = t.id;
						for (const InputManager::Touch &o : touches) if (o.id == editFinger) pinchStartDist = (o.position - t.position).magnitude();
						pinchStartSize = controls[editControl].size; pinchStartW = controls[editControl].w; pinchStartH = controls[editControl].h;
					}
				}
				if (t.ended)
				{
					if (editHasFinger && t.id == editFinger) { editHasFinger = false; pinchHas = false; editControl = -1; }
					else if (pinchHas && t.id == pinchFinger) pinchHas = false;
				}
			}
			if (editHasFinger && editControl >= 0)
			{
				Control &c = controls[editControl];
				Vec2 first, second; bool haveFirst = false, haveSecond = false;
				for (const InputManager::Touch &t : touches)
				{
					if (t.id == editFinger) { first = t.position; haveFirst = true; }
					if (pinchHas && t.id == pinchFinger) { second = t.position; haveSecond = true; }
				}
				if (haveFirst && !pinchHas)
				{
					const Vec2 f(first.x / width, first.y / height);
					c.x = std::min(1.f, std::max(0.f, f.x - editGrab.x));
					c.y = std::min(1.f, std::max(0.f, f.y - editGrab.y));
				}
				if (haveFirst && haveSecond && pinchStartDist > 1.f)
				{
					const f32 k = (first - second).magnitude() / pinchStartDist;
					if (c.type == "look") { c.w = std::min(1.f, std::max(0.1f, pinchStartW * k)); c.h = std::min(1.f, std::max(0.1f, pinchStartH * k)); }
					else c.size = std::min(0.6f, std::max(0.06f, pinchStartSize * k));
				}
			}
			return;
		}

		if (!live) { ReleaseAll(); return; }

		for (const InputManager::Touch &t : touches)
		{
			std::map<int64, Held>::iterator h = held.find(t.id);
			// (a new touch on a number still held: its lift was lost - let the old control go first)
			if (t.began && h != held.end())
			{
				const Control &old = controls[h->second.control];
				if (old.type == "button" && !old.toggle && buttonDown[h->second.control]) { buttonDown[h->second.control] = false; Press(old, false); }
				if (old.type == "stick")
				{
					StickState &s = sticks[h->second.control];
					const struct { bool on; int32 key; } ks[] = { { s.up, old.up }, { s.down, old.down }, { s.left, old.left }, { s.right, old.right }, { s.run, old.run } };
					for (const auto &k : ks) if (k.on && k.key >= 0 && emitter.keyUp) emitter.keyUp((uint32)k.key);
					sticks.erase(h->second.control);
				}
				held.erase(h);
				h = held.end();
			}
			if (t.began && h == held.end())
			{
				const int32 hit = HitControl(t.position, width, height);
				if (hit < 0) continue;
				Held nh; nh.control = hit; nh.start = t.position; nh.last = t.position;
				h = held.insert(std::make_pair(t.id, nh)).first;
				const Control &c = controls[hit];
				if (c.type == "edit") { held.erase(h); SetEditing(true); return; }
				if (c.type == "button")
				{
					bool &down = buttonDown[hit];
					if (c.toggle) { down = !down; Press(c, down); }
					else if (!down) { down = true; Press(c, true); }
				}
			}
			if (h == held.end()) continue;
			const Control &c = controls[h->second.control];
			if (c.type == "stick")
			{
				StickState &s = sticks[h->second.control];
				const Vec2 centre(c.x * width, c.y * height);
				const f32 radius = std::max(1.f, c.size * height * 0.5f);
				Vec2 d = (t.position - centre) / radius;
				const f32 len = d.magnitude();
				if (len > 1.f) d = d / len;
				s.knob = t.ended ? Vec2() : d;
				const f32 kOn = 0.3f;
				const struct { bool* on; bool want; int32 key; } ks[] = {
					{ &s.up, !t.ended && d.y < -kOn, c.up }, { &s.down, !t.ended && d.y > kOn, c.down },
					{ &s.left, !t.ended && d.x < -kOn, c.left }, { &s.right, !t.ended && d.x > kOn, c.right },
					{ &s.run, !t.ended && len > 0.92f, c.run } };
				for (const auto &k : ks)
				{
					if (k.key < 0 || *k.on == k.want) continue;
					*k.on = k.want;
					if (k.want) { if (emitter.keyDown) emitter.keyDown((uint32)k.key); }
					else if (emitter.keyUp) emitter.keyUp((uint32)k.key);
				}
			}
			else if (c.type == "look" && !t.ended)
			{
				const Vec2 d = t.position - h->second.last;
				if ((d.x != 0.f || d.y != 0.f) && emitter.look) emitter.look(d.x * c.sensitivity, d.y * c.sensitivity);
			}
			h->second.last = t.position;
			if (t.ended)
			{
				if (c.type == "button" && !c.toggle) { buttonDown[h->second.control] = false; Press(c, false); }
				if (c.type == "stick") sticks.erase(h->second.control);
				held.erase(h);
			}
		}
	}

	void TouchControls::Teardown()
	{
		shown.clear();
		doneRect = resetRect = hintRect = NULL; doneImage = resetImage = NULL;
		if (scene)
		{
			if (assets) SceneSerializer::UnloadScene(scene, *assets);
			delete scene; scene = NULL;
		}
		delete assets; assets = NULL;
		built = false;
	}

	void TouchControls::Build()
	{
		Teardown();
		if (!disc) { disc = MakeCircle(false); ring = MakeCircle(true); }
		json kids = json::array();
		for (size_t i = 0; i < controls.size(); i++)
		{
			const Control &c = controls[i];
			const std::string n = "Touch_" + std::to_string(i);
			json children = json::array();
			if (c.type == "stick") children.push_back(NodeJ(n + "_Knob", { RectJ(), ImageJ(Vec4(1, 1, 1, 0.35f)) }));
			if (!c.label.empty() || c.type == "look") children.push_back(NodeJ(n + "_Label", { RectJ(), TextJ(c.type == "look" ? std::string("LOOK") : c.label, 15.f) }));
			kids.push_back(NodeJ(n, { RectJ(), ImageJ(Vec4(1, 1, 1, 0.2f)) }, children));
		}
		kids.push_back(NodeJ("Touch_Done", { RectJ(), ImageJ(Vec4(0.2f, 0.55f, 0.3f, 0.85f)) }, json::array({ NodeJ("Touch_Done_Label", { RectJ(), TextJ("DONE", 18.f) }) })));
		kids.push_back(NodeJ("Touch_Reset", { RectJ(), ImageJ(Vec4(0.5f, 0.25f, 0.2f, 0.85f)) }, json::array({ NodeJ("Touch_Reset_Label", { RectJ(), TextJ("RESET", 18.f) }) })));
		kids.push_back(NodeJ("Touch_Hint", { RectJ(), TextJ("Drag a control to move it - a second finger sizes it", 15.f) }));
		json canvas = NodeJ("TouchControls", { { { "type", "UICanvas" }, { "referenceWidth", 1280.f }, { "referenceHeight", kCanvasH },
			{ "scaleMode", "MatchHeight" }, { "sortOrder", 900 } } }, kids);
		json sceneJ = { { "version", 1 }, { "materials", json::array() }, { "roots", json::array({ canvas }) } };
		scene = new SceneGraph();
		assets = new LoadedSceneAssets();
		if (!SceneSerializer::LoadSceneFromText(scene, sceneJ.dump(), "touchcontrols.json", NULL, NULL, assets, NULL))
		{
			Teardown();
			return;
		}
		scene->Update(0.0);
		GameObject* root = NULL;
		for (const std::shared_ptr<GameObject> &g : scene->GetAllGameObjectList()) if (g && g->GetName() == "TouchControls") root = g.get();
		for (size_t i = 0; i < controls.size(); i++)
		{
			const std::string n = "Touch_" + std::to_string(i);
			Shown s;
			GameObject* go = FindIn(root, n);
			s.rect = ComponentOf<UIRect>(go, ComponentType::UIRect);
			s.image = ComponentOf<UIImage>(go, ComponentType::UIImage);
			GameObject* knob = FindIn(root, n + "_Knob");
			s.knobRect = ComponentOf<UIRect>(knob, ComponentType::UIRect);
			s.knob = ComponentOf<UIImage>(knob, ComponentType::UIImage);
			GameObject* label = FindIn(root, n + "_Label");
			s.labelRect = ComponentOf<UIRect>(label, ComponentType::UIRect);
			s.text = ComponentOf<UIText>(label, ComponentType::UIText);
			if (s.image && controls[i].type != "look") s.image->SetTexture(controls[i].type == "stick" ? ring : disc);
			if (s.knob) s.knob->SetTexture(disc);
			shown.push_back(s);
		}
		doneRect = ComponentOf<UIRect>(FindIn(root, "Touch_Done"), ComponentType::UIRect);
		resetRect = ComponentOf<UIRect>(FindIn(root, "Touch_Reset"), ComponentType::UIRect);
		hintRect = ComponentOf<UIRect>(FindIn(root, "Touch_Hint"), ComponentType::UIRect);
		doneImage = ComponentOf<UIImage>(FindIn(root, "Touch_Done"), ComponentType::UIImage);
		resetImage = ComponentOf<UIImage>(FindIn(root, "Touch_Reset"), ComponentType::UIImage);
		built = true;
	}

	void TouchControls::Place(const uint32 width, const uint32 height)
	{
		const f32 aspect = height > 0 ? (f32)width / (f32)height : 16.f / 9.f;
		const f32 canvasW = kCanvasH * aspect;
		auto at = [&](UIRect* r, const Vec2 &centre, const Vec2 &size) {
			if (!r) return;
			r->SetAnchors(centre, centre);
			r->SetOffsets(Vec2(-size.x * 0.5f, -size.y * 0.5f), Vec2(size.x * 0.5f, size.y * 0.5f));
		};
		auto show = [](UIRect* r, const bool on) { if (r) r->SetVisible(on); };
		const bool drawControls = enabled && (live || editing);
		for (size_t i = 0; i < controls.size() && i < shown.size(); i++)
		{
			const Control &c = controls[i];
			const Shown &s = shown[i];
			const bool visible = drawControls && c.visible && (c.type != "look" || editing);
			show(s.rect, visible);
			if (!visible) continue;
			if (c.type == "look")
			{
				at(s.rect, Vec2(c.x + c.w * 0.5f, c.y + c.h * 0.5f), Vec2(c.w * canvasW, c.h * kCanvasH));
				if (s.image) s.image->SetTint(Vec4(0.3f, 0.5f, 1.f, editing && i == (size_t)editControl ? 0.25f : 0.12f));
			}
			else
			{
				const f32 d = c.size * kCanvasH;
				at(s.rect, Vec2(c.x, c.y), Vec2(d, d));
				bool pressed = false;
				std::map<int32, bool>::const_iterator b = buttonDown.find((int32)i);
				if (b != buttonDown.end()) pressed = b->second;
				const f32 a = editing ? (i == (size_t)editControl ? 0.7f : 0.45f) : (pressed ? 0.5f : 0.25f);
				if (s.image) s.image->SetTint(Vec4(1, 1, 1, a));
			}
			// (children are placed in their parent's rectangle: the knob and the label fill it, from its middle)
			if (s.knobRect)
			{
				Vec2 k;
				std::map<int32, StickState>::const_iterator st = sticks.find((int32)i);
				if (st != sticks.end()) k = st->second.knob;
				const f32 d = c.size * kCanvasH;
				s.knobRect->SetAnchors(Vec2(0.5f, 0.5f), Vec2(0.5f, 0.5f));
				const Vec2 o(k.x * d * 0.5f, k.y * d * 0.5f);
				s.knobRect->SetOffsets(o - Vec2(d * 0.22f, d * 0.22f), o + Vec2(d * 0.22f, d * 0.22f));
			}
			if (s.labelRect) { s.labelRect->SetAnchors(Vec2(0, 0), Vec2(1, 1)); s.labelRect->SetOffsets(Vec2(), Vec2()); }
		}
		show(doneRect, editing); show(resetRect, editing); show(hintRect, editing);
		if (editing)
		{
			at(doneRect, kDoneAt, Vec2(kEditButton.x * canvasW, kEditButton.y * kCanvasH));
			at(resetRect, kResetAt, Vec2(kEditButton.x * canvasW, kEditButton.y * kCanvasH));
			at(hintRect, Vec2(0.5f, 0.25f), Vec2(0.8f * canvasW, 30.f));
		}
	}

	SceneGraph* TouchControls::Prepare(const uint32 width, const uint32 height, const f64 time)
	{
		if (!enabled || controls.empty()) return NULL;
		if (!live && !editing) return NULL;
		if (!built) Build();
		if (!scene) return NULL;
		Place(lastW = width, lastH = height);
		scene->Update(time);
		return scene;
	}

}
