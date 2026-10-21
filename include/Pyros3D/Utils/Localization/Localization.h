//============================================================================
// Name        : Localization.h
// Description : What the game says, in the player's language.
//============================================================================
#ifndef LOCALIZATION_H
#define LOCALIZATION_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace p3d {

	// String tables, one a language: <directory>/<code>.json (assets/strings/pt.json), each a JSON
	// object of key -> text; nested objects are keys joined with '.' ({"menu": {"play": "Jogar"}}
	// is "menu.play"). A key the language lacks comes from the fallback language (en), and one
	// neither has comes out as the key itself - visibly missing, not blank.
	//
	// Text components take "@key" for their text (a UIText reading "@menu.play"), show it in
	// the current language, and change with it. A literal leading @ is written "@@".
	// Lua: tr("menu.play"), tr("hud.ammo", { count = 30 }) for "{count} rounds",
	// localization.setLanguage("pt").
	class PYROS3D_API Localization {
	public:
		static void SetDirectory(const std::string &dir);
		static const std::string &GetDirectory();
		// Loads that language's table and makes it current (false, and nothing changes, when
		// there is no such table). Everything showing "@key" text is redrawn in it.
		static bool SetLanguage(const std::string &code);
		static const std::string &GetLanguage();
		static void SetFallback(const std::string &code);
		static const std::string &GetFallback();
		// The languages there are tables for, by code.
		static std::vector<std::string> GetLanguages();
		// Of the codes given (the system's preferred languages, best first: "pt-PT", "en-US"),
		// the first there is a table for - whole code or language alone - else the fallback.
		static std::string PickLanguage(const std::vector<std::string> &preferred);

		static std::string Get(const std::string &key);
		static bool Has(const std::string &key);
		// Get(key) with each {name} replaced by args[name].
		static std::string Format(const std::string &key, const std::map<std::string, std::string> &args);
		// "@key" -> Get(key); "@@text" -> "@text"; anything else as it is.
		static std::string Resolve(const std::string &text);
		static bool IsKey(const std::string &text) { return text.size() > 1 && text[0] == '@' && text[1] != '@'; }
		// Bumped by every change of language or table.
		static uint32 GetRevision();
		// Re-reads the tables (the files changed: the editor saved one).
		static void Reload();

		// Told when the language changes (a text component showing a key redraws itself).
		static void AddListener(const void* owner, const std::function<void()> &onChange);
		static void RemoveListener(const void* owner);
	};
}

#endif /* LOCALIZATION_H */
