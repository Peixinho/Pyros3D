//============================================================================
// Name        : Localization.cpp
// Description : What the game says, in the player's language.
//============================================================================

#include <Pyros3D/Utils/Localization/Localization.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace p3d {

	namespace {
		typedef std::map<std::string, std::string> Table;
		struct State {
			std::string directory = "assets/strings";
			std::string language;                 // (none until set: keys come from the fallback)
			std::string fallback = "en";
			std::map<std::string, Table> tables;  // loaded, by code
			uint32 revision = 1;
			std::vector<std::pair<const void*, std::function<void()> > > listeners;
		};
		// (never freed: a text component destroyed during exit still unregisters itself)
		State &S() { static State* s = new State(); return *s; }

		void Flatten(const nlohmann::json &j, const std::string &prefix, Table &out)
		{
			for (nlohmann::json::const_iterator it = j.begin(); it != j.end(); ++it)
			{
				const std::string key = prefix.empty() ? it.key() : prefix + "." + it.key();
				if (it->is_object()) Flatten(*it, key, out);
				else if (it->is_string()) out[key] = it->get<std::string>();
				else if (it->is_number() || it->is_boolean()) out[key] = it->dump();
			}
		}

		// (the table for a code, read the first time it is asked for; an empty one when there is none)
		const Table* TableFor(const std::string &code)
		{
			if (code.empty()) return NULL;
			std::map<std::string, Table>::iterator it = S().tables.find(code);
			if (it != S().tables.end()) return &it->second;
			const std::string path = S().directory + "/" + code + ".json";
			std::ifstream in(path.c_str(), std::ios::binary);
			if (!in.is_open()) return NULL;
			std::stringstream ss; ss << in.rdbuf();
			Table t;
			try
			{
				nlohmann::json j = nlohmann::json::parse(ss.str());
				if (j.is_object()) Flatten(j, std::string(), t);
			}
			catch (const std::exception &e)
			{
				fprintf(stderr, "WARNING: localization: %s does not read as JSON (%s)\n", path.c_str(), e.what());
				return NULL;
			}
			return &(S().tables[code] = t);
		}

		void Notify()
		{
			S().revision++;
			// (a copy: a listener may add or remove one)
			std::vector<std::pair<const void*, std::function<void()> > > l = S().listeners;
			for (size_t i = 0; i < l.size(); i++) if (l[i].second) l[i].second();
		}
	}

	void Localization::SetDirectory(const std::string &dir)
	{
		if (dir == S().directory) return;
		S().directory = dir;
		S().tables.clear();
		Notify();
	}
	const std::string &Localization::GetDirectory() { return S().directory; }

	bool Localization::SetLanguage(const std::string &code)
	{
		if (!TableFor(code)) return false;
		if (code == S().language) return true;
		S().language = code;
		Notify();
		return true;
	}
	const std::string &Localization::GetLanguage() { return S().language.empty() ? S().fallback : S().language; }

	void Localization::SetFallback(const std::string &code) { if (code != S().fallback) { S().fallback = code; Notify(); } }
	const std::string &Localization::GetFallback() { return S().fallback; }

	std::vector<std::string> Localization::GetLanguages()
	{
		std::vector<std::string> out;
		std::error_code ec;
		for (std::filesystem::directory_iterator it(S().directory, ec), end; !ec && it != end; it.increment(ec))
			if (it->path().extension() == ".json") out.push_back(it->path().stem().string());
		std::sort(out.begin(), out.end());
		return out;
	}

	std::string Localization::PickLanguage(const std::vector<std::string> &preferred)
	{
		const std::vector<std::string> have = GetLanguages();
		for (size_t i = 0; i < preferred.size(); i++)
		{
			std::string p = preferred[i];
			std::replace(p.begin(), p.end(), '_', '-');
			// (whole: pt-BR; then the language alone: pt)
			if (std::find(have.begin(), have.end(), p) != have.end()) return p;
			const std::string lang = p.substr(0, p.find('-'));
			if (std::find(have.begin(), have.end(), lang) != have.end()) return lang;
		}
		return S().fallback;
	}

	bool Localization::Has(const std::string &key)
	{
		const Table* t = TableFor(S().language);
		if (t && t->count(key)) return true;
		const Table* f = TableFor(S().fallback);
		return f && f->count(key);
	}

	std::string Localization::Get(const std::string &key)
	{
		const Table* t = TableFor(S().language);
		if (t) { Table::const_iterator it = t->find(key); if (it != t->end()) return it->second; }
		const Table* f = TableFor(S().fallback);
		if (f) { Table::const_iterator it = f->find(key); if (it != f->end()) return it->second; }
		return key;
	}

	std::string Localization::Format(const std::string &key, const std::map<std::string, std::string> &args)
	{
		std::string s = Get(key), out;
		out.reserve(s.size());
		for (size_t i = 0; i < s.size(); i++)
		{
			if (s[i] == '{')
			{
				const size_t close = s.find('}', i + 1);
				if (close != std::string::npos)
				{
					std::map<std::string, std::string>::const_iterator a = args.find(s.substr(i + 1, close - i - 1));
					if (a != args.end()) { out += a->second; i = close; continue; }
				}
			}
			out += s[i];
		}
		return out;
	}

	std::string Localization::Resolve(const std::string &text)
	{
		if (text.size() < 2 || text[0] != '@') return text;
		if (text[1] == '@') return text.substr(1);
		return Get(text.substr(1));
	}

	uint32 Localization::GetRevision() { return S().revision; }

	void Localization::Reload()
	{
		S().tables.clear();
		Notify();
	}

	void Localization::AddListener(const void* owner, const std::function<void()> &onChange)
	{
		for (size_t i = 0; i < S().listeners.size(); i++)
			if (S().listeners[i].first == owner) { S().listeners[i].second = onChange; return; }
		S().listeners.push_back(std::make_pair(owner, onChange));
	}

	void Localization::RemoveListener(const void* owner)
	{
		std::vector<std::pair<const void*, std::function<void()> > > &l = S().listeners;
		for (size_t i = 0; i < l.size(); i++) if (l[i].first == owner) { l.erase(l.begin() + i); return; }
	}
}
