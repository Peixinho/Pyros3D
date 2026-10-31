//============================================================================
// Name        : DevConsole.cpp
// Description : The developer console: the log, and a line to type commands into.
//============================================================================

#include <Pyros3D/Rendering/Components/UI/DevConsole.h>
#include <Pyros3D/Rendering/Components/UI/UIText.h>
#include <Pyros3D/Core/InputManager/InputManager.h>
#include <Pyros3D/Assets/Font/Font.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <chrono>
#include <cmath>

namespace p3d {

	using namespace LOG;

	namespace {
		using json = nlohmann::json;
		const int kRows = 22;
		const f32 kRowH = 22.f;

		json V4(const Vec4 &v) { return json::array({ v.x, v.y, v.z, v.w }); }
		json Rect(f32 ax0, f32 ay0, f32 ax1, f32 ay1, f32 ox0, f32 oy0, f32 ox1, f32 oy1)
		{
			return { { "type", "UIRect" }, { "anchorMin", { ax0, ay0 } }, { "anchorMax", { ax1, ay1 } },
					 { "offsetMin", { ox0, oy0 } }, { "offsetMax", { ox1, oy1 } }, { "pivot", { 0.5f, 0.5f } } };
		}
		json Node(const std::string &name, const json &components, const json &children = json::array())
		{
			return { { "name", name }, { "position", { 0, 0, 0 } }, { "rotation", { 0, 0, 0 } }, { "scale", { 1, 1, 1 } },
					 { "static", false }, { "tags", json::array() }, { "components", components }, { "children", children } };
		}
		json TextNode(const std::string &text, const f32 size, const Vec4 &color)
		{
			return { { "type", "UIText" }, { "text", text }, { "font", Font::BuiltinDefault() }, { "fontSDF", true }, { "fontSize", 48.f },
					 { "size", size }, { "color", V4(color) }, { "align", "Left" }, { "verticalAlign", "Middle" } };
		}
		Vec4 LevelColour(const int level)
		{
			switch (level)
			{
			case Level::Error: return Vec4(1.f, 0.45f, 0.4f, 1.f);
			case Level::Warning: return Vec4(1.f, 0.8f, 0.35f, 1.f);
			case Level::Success: return Vec4(0.55f, 0.9f, 0.6f, 1.f);
			default: return Vec4(0.85f, 0.87f, 0.9f, 1.f);
			}
		}
		f64 Now() { return std::chrono::duration<f64>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
		template <class T> T* ComponentOf(GameObject* go, const uint32 type)
		{
			if (!go) return NULL;
			const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
			for (size_t i = 0; i < cs.size(); i++) if (cs[i] && cs[i]->GetComponentType() == type) return static_cast<T*>(cs[i].get());
			return NULL;
		}
		GameObject* Find(GameObject* go, const std::string &name)
		{
			if (!go) return NULL;
			if (go->GetName() == name) return go;
			for (const std::shared_ptr<GameObject> &k : go->GetChildren()) if (GameObject* f = Find(k.get(), name)) return f;
			return NULL;
		}

		// What the console shows: the log's lines as they arrive, and its own, in one list.
		struct Line { std::string text; int level; };
		std::vector<Line> &Lines() { static std::vector<Line>* l = new std::vector<Line>(); return *l; }
		// One row a line: a message with newlines in it (a script's traceback) is several, and a tab
		// is spaces (the font has no glyph for it).
		void Add(const std::string &text, const int level)
		{
			size_t start = 0;
			for (;;)
			{
				const size_t nl = text.find('\n', start);
				std::string piece = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start), out;
				for (size_t i = 0; i < piece.size(); i++) { if (piece[i] == '\t') out += "    "; else if (piece[i] != '\r') out += piece[i]; }
				Lines().push_back(Line{ out, level });
				if (nl == std::string::npos) break;
				start = nl + 1;
			}
		}
		unsigned long long &Seen() { static unsigned long long s = 0; return s; }
		void Pull()
		{
			const unsigned long long total = _LOG::RecordedCount();
			const unsigned int have = _LOG::MessageCount();
			unsigned long long fresh = total - Seen();
			if (fresh > have) fresh = have;
			for (unsigned long long i = 0; i < fresh; i++)
			{
				const Entry e = _LOG::MessageAt((unsigned int)(have - fresh + i));
				Add(e.text, e.level);
			}
			Seen() = total;
			if (Lines().size() > 2000) Lines().erase(Lines().begin(), Lines().begin() + (Lines().size() - 2000));
		}
	}

	DevConsole::DevConsole() : scene(NULL), input(NULL), historyAt(-1), scroll(0), blinkStart(0.0)
	{
		AddCommand("help", "what the console knows", [this](const std::string &) {
			std::string s = "Commands:";
			for (std::map<std::string, Command>::const_iterator c = commands.begin(); c != commands.end(); ++c) s += "\n  " + c->first + " - " + c->second.help;
			return s + "\nAnything else is run as script (an expression's value is shown).";
		});
		AddCommand("clear", "empty the console", [](const std::string &) { Lines().clear(); return std::string(); });
		AddCommand("loglevel", "0 errors .. 4 trace: how much of the log is kept", [](const std::string &arg) {
			if (!arg.empty()) _LOG::SetLevel(atoi(arg.c_str()));
			return "log level " + std::to_string(_LOG::GetLevel());
		});
	}

	DevConsole::~DevConsole() { Close(); }

	void DevConsole::AddCommand(const std::string &name, const std::string &help, const std::function<std::string(const std::string &)> &run)
	{
		Command c; c.help = help; c.run = run;
		commands[name] = c;
	}

	void DevConsole::Open(const uint32 width, const uint32 height)
	{
		if (scene) return;
		// The top half of the screen: a dark panel, the log rows over it from the top, the line at its foot.
		json rowsJ = json::array();
		for (int i = 0; i < kRows; i++)
			rowsJ.push_back(Node("Console_Row" + std::to_string(i), { Rect(0.f, 0.f, 1.f, 0.f, 16.f, 10.f + i * kRowH, -16.f, 10.f + (i + 1) * kRowH), TextNode("", 17.f, Vec4(1, 1, 1, 1)) }));
		json kids = json::array();
		kids.push_back(Node("Console_Panel", { Rect(0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 20.f + (kRows + 1) * kRowH + 14.f),
			{ { "type", "UIImage" }, { "tint", V4(Vec4(0.05f, 0.06f, 0.08f, 0.88f)) }, { "border", { 0, 0, 0, 0 } } } }, rowsJ));
		kids.push_back(Node("Console_Input", { Rect(0.f, 0.f, 1.f, 0.f, 16.f, 16.f + kRows * kRowH, -16.f, 16.f + (kRows + 1) * kRowH), TextNode("> ", 19.f, Vec4(1.f, 1.f, 1.f, 1.f)) }));
		json canvas = Node("DevConsole", { { { "type", "UICanvas" }, { "referenceWidth", 1280.f }, { "referenceHeight", 720.f },
			{ "scaleMode", "MatchWidth" }, { "sortOrder", 1000 } } }, kids);
		json sceneJ = { { "version", 1 }, { "materials", json::array() }, { "roots", json::array({ canvas }) } };
		scene = new SceneGraph();
		if (!SceneSerializer::LoadSceneFromText(scene, sceneJ.dump(), "devconsole.json", NULL, NULL, &assets, NULL))
		{
			SceneSerializer::UnloadScene(scene, assets);
			delete scene; scene = NULL;
			return;
		}
		scene->Update(0.0);
		GameObject* root = NULL;
		for (const std::shared_ptr<GameObject> &g : scene->GetAllGameObjectList()) if (g && g->GetName() == "DevConsole") root = g.get();
		rows.clear();
		for (int i = 0; i < kRows; i++) rows.push_back(ComponentOf<UIText>(Find(root, "Console_Row" + std::to_string(i)), ComponentType::UIText));
		input = ComponentOf<UIText>(Find(root, "Console_Input"), ComponentType::UIText);
		(void)width; (void)height;
		scroll = 0; historyAt = -1;
		blinkStart = Now();
		InputManager::SetGameInputWithheld(true);
	}

	void DevConsole::Close()
	{
		if (!scene) return;
		rows.clear(); input = NULL;
		SceneSerializer::UnloadScene(scene, assets);
		delete scene; scene = NULL;
		InputManager::SetGameInputWithheld(false);
	}

	void DevConsole::Print(const std::string &text)
	{
		Add(text, text.compare(0, 6, "ERROR:") == 0 ? Level::Error : Level::Info);
	}

	void DevConsole::Run(const std::string &text)
	{
		Pull();                                   // (what was logged before this line comes before it)
		Add("> " + text, Level::Success);
		const size_t sp = text.find(' ');
		const std::string word = text.substr(0, sp), rest = sp == std::string::npos ? std::string() : text.substr(sp + 1);
		std::map<std::string, Command>::iterator c = commands.find(word);
		std::string out;
		// (what it prints is kept while it runs, whatever the log's level: a print typed here is
		// meant to be seen - the default keeps only warnings and errors)
		const int level = _LOG::GetLevel();
		if (level < Level::Info) _LOG::SetLevel(Level::Info);
		if (c != commands.end()) out = c->second.run(rest);
		else if (executor) out = executor(text);
		else out = "no such command (help)";
		if (c == commands.end() || word != "loglevel") _LOG::SetLevel(level);
		Pull();                                   // (what running it logged)
		if (!out.empty()) Print(out);
		scroll = 0;
	}

	void DevConsole::Type(const std::string &utf8)
	{
		if (!scene) return;
		for (size_t i = 0; i < utf8.size(); i++) if (utf8[i] == '`' || utf8[i] == '~') return;   // (the key that opens it)
		line += utf8;
		blinkStart = Now();
	}

	void DevConsole::Press(const Key key)
	{
		if (!scene) return;
		switch (key)
		{
		case Enter:
			if (!line.empty())
			{
				if (history.empty() || history.back() != line) history.push_back(line);
				const std::string run = line;
				line.clear();
				Run(run);
			}
			historyAt = -1;
			break;
		case Backspace:
			// (a whole UTF-8 character: its continuation bytes go with it)
			while (!line.empty() && (line.back() & 0xC0) == 0x80) line.pop_back();
			if (!line.empty()) line.pop_back();
			break;
		case Up:
			if (history.empty()) break;
			historyAt = historyAt < 0 ? (int32)history.size() - 1 : (historyAt > 0 ? historyAt - 1 : 0);
			line = history[historyAt];
			break;
		case Down:
			if (historyAt < 0) break;
			if (historyAt + 1 < (int32)history.size()) { historyAt++; line = history[historyAt]; }
			else { historyAt = -1; line.clear(); }
			break;
		case PageUp: scroll += kRows - 2; break;
		case PageDown: scroll = scroll > kRows - 2 ? scroll - (kRows - 2) : 0; break;
		case Escape: Close(); return;
		}
		blinkStart = Now();
	}

	SceneGraph* DevConsole::Prepare(const f64 time)
	{
		if (!scene) return NULL;
		Pull();
		const int32 n = (int32)Lines().size();
		const int32 maxScroll = n > kRows ? n - kRows : 0;
		if (scroll > maxScroll) scroll = maxScroll;
		const int32 first = n - kRows - scroll;
		for (int32 i = 0; i < kRows; i++)
		{
			UIText* t = rows[i];
			if (!t) continue;
			const int32 k = first + i;
			if (k >= 0 && k < n)
			{
				std::string s = Lines()[k].text;
				if (s.size() > 220) s = s.substr(0, 217) + "...";
				t->SetText(s);
				t->SetColor(LevelColour(Lines()[k].level));
			}
			else t->SetText("");
		}
		if (input)
		{
			const bool caret = fmod(Now() - blinkStart, 1.0) < 0.55;
			input->SetText(std::string("> ") + line + (caret ? "_" : " ") + (scroll > 0 ? "      [scrolled: Page Down]" : ""));
		}
		scene->Update(time);
		return scene;
	}

	void DevConsole::Resize(const uint32 width, const uint32 height) { (void)width; (void)height; }
}
