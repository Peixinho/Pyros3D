//============================================================================
// Name        : DevConsole.h
// Description : The developer console: the log, and a line to type commands into.
//============================================================================
#ifndef DEVCONSOLE_H
#define DEVCONSOLE_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace p3d {

	class SceneGraph;
	class UIText;

	// Drops over the top half of a running game: the engine's log (the newest lines, coloured by
	// how bad they are, scrolled with Page Up / Page Down) and a line to type into. Enter runs what
	// was typed - a command the console knows (help, clear, ...), else whatever the application
	// does with it (the player runs it as Lua in the game's own state, and shows what it returns).
	// Up and Down walk back through what was typed before. Built from the engine's own UI, so it
	// is drawn the same way on every backend; while it is open the game hears no input
	// (InputManager::SetGameInputWithheld).
	class PYROS3D_API DevConsole {
	public:
		DevConsole();
		~DevConsole();

		void Open(const uint32 width, const uint32 height);
		void Close();
		bool IsOpen() const { return scene != NULL; }

		// What the application does with a line no command claims: returns what to print ("" for nothing).
		void SetExecutor(const std::function<std::string(const std::string &)> &run) { executor = run; }
		// A command of its own: the line's first word, then the rest of it handed over.
		void AddCommand(const std::string &name, const std::string &help, const std::function<std::string(const std::string &)> &run);

		// Typing (characters, as the platform decodes them) and the keys that edit the line.
		void Type(const std::string &utf8);
		enum Key { Enter, Backspace, Up, Down, PageUp, PageDown, Escape };
		void Press(const Key key);

		// Once a frame while open: the log and the line brought up to date - its UI scene, for the host's renderer to
		// draw over the frame (NULL while closed)
		SceneGraph* Prepare(const f64 time);
		void Resize(const uint32 width, const uint32 height);

		// The line being typed (tests, an agent driving it).
		const std::string &GetLine() const { return line; }

	private:
		void Run(const std::string &text);
		void Print(const std::string &text);
		SceneGraph* scene;
		LoadedSceneAssets assets;
		std::vector<UIText*> rows;
		UIText* input;
		std::string line;
		std::vector<std::string> history;
		int32 historyAt;
		int32 scroll;          // lines back from the newest
		f64 blinkStart;
		struct Command { std::string help; std::function<std::string(const std::string &)> run; };
		std::map<std::string, Command> commands;
		std::function<std::string(const std::string &)> executor;
	};
}

#endif /* DEVCONSOLE_H */
