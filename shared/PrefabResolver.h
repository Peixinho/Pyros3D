//============================================================================
// Name        : PrefabResolver.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Prefab references in a scene file, resolved entirely outside
//               the engine.
//============================================================================
//
// A prefab is a tooling concept, not an engine one. The engine's job is to
// turn a scene file into a scene graph; making SceneSerializer::LoadScene()
// depend on resolving further files would put a whole new class of failure
// (a missing asset that is itself a scene fragment) inside the engine, for
// something that only exists to make authoring pleasant.
//
// So the engine knows nothing about any of this. It reads and writes scenes
// whose roots are written out in full, and everything here is a pass over
// that JSON *before* it is handed to the engine, or *after* the engine has
// produced it:
//
//   load:  file → ExpandScene()  → SceneSerializer::LoadScene(text)
//   save:  SceneSerializer::SaveScene(file) → CollapseScene() → file
//
// Both the editor and the player run the same two passes, which is why this
// lives in shared/ rather than in either of them. Header-only so that using
// it costs nothing but an include path - it is JSON manipulation and file
// reads, with no engine types in its interface at all.
//
// ---------------------------------------------------------------------------
// The file format
//
// A .prefab is exactly what SceneSerializer::SerializeSubtree() produces -
// {"root": <gameobject>, "materials": [...]} - plus "prefabVersion". Nothing
// here invents a format; a prefab is a scene subtree in a file.
//
// A scene that references one stores this in place of the whole subtree:
//
//   { "prefab": "assets/prefabs/Enemy.prefab", "name": "Enemy_03",
//     "position": [...], "rotation": [...], "scale": [...], "tags": [...] }
//
// Those four fields are the entire override set (kOverrideFields): the ones
// an instance can differ in and still be re-derivable from its source. An
// instance edited any other way no longer matches, and CollapseScene leaves
// it written out in full rather than either discarding the edit or pushing
// it into every other instance.
//
// An entry whose prefab file cannot be read is deliberately left alone
// rather than dropped. The engine ignores JSON keys it does not know, so it
// deserializes as a bare GameObject carrying the instance's name and
// transform - the scene still loads, the object is still there and still
// says what it was, and repairing the file restores it on the next load.
//
// ---------------------------------------------------------------------------
// Material pools
//
// The one genuinely fiddly part. Scene JSON has a single top-level
// "materials" array and components reference entries by index; a .prefab has
// its own, numbered from zero. Expanding therefore has to merge the prefab's
// pool into the scene's and renumber every reference inside the subtree, and
// collapsing has to compare a subtree that numbers into the scene's pool
// against one that numbers into the prefab's.
//
// Canonical() is what makes that comparable: it inlines each referenced
// material in place of its index, so two subtrees are the same prefab if and
// only if their canonical forms are equal - whatever pool their materials
// happened to live in, and in whatever order.
//
// Merging deduplicates by content, so a hundred instances of one prefab
// share one pool entry and therefore one material at load: the renderer
// groups draw calls by material, so this is the difference between one group
// and a hundred.

#ifndef PREFABRESOLVER_H
#define PREFABRESOLVER_H

#include <Pyros3D/Utils/Json/json.hpp>

#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace prefab {

	typedef nlohmann::json json;

	// Reads a .prefab. Loaders are injected rather than assumed so the editor
	// (project-relative) and the player (game-folder-relative) can each
	// resolve paths their own way.
	typedef std::function<json(const std::string& prefabRelPath)> LoadFn;

	inline const char* const* OverrideFields(size_t& count)
	{
		static const char* const fields[] = { "name", "position", "rotation", "scale", "tags" };
		count = sizeof(fields) / sizeof(fields[0]);
		return fields;
	}

	// ------------------------------ file IO ------------------------------

	inline json ReadPrefabFile(const std::string& absPath)
	{
		std::ifstream in(absPath.c_str());
		if (!in.is_open()) return json();
		try
		{
			json j;
			in >> j;
			if (!j.is_object() || j.find("root") == j.end()) return json();
			return j;
		}
		catch (const std::exception&) { return json(); }
	}

	inline bool WritePrefabFile(const json& subtree, const std::string& absPath)
	{
		if (!subtree.is_object() || subtree.find("root") == subtree.end()) return false;
		json out = subtree;
		// A prefab captures objects, not their provenance: a prefab made
		// from an instance of another prefab must not claim to be one.
		out["root"].erase("prefab");
		out["prefabVersion"] = 1;

		std::ofstream f(absPath.c_str());
		if (!f.is_open()) return false;
		f << out.dump(4);
		return true;
	}

	// --------------------------- material refs ---------------------------

	// "material" on a RenderingComponent is the only place scene JSON
	// references the pool by index (SceneSerializer::SerializeComponent).
	// Walking for it rather than special-casing component types means a new
	// component that references a material the same way needs no change here.
	inline void ForEachMaterialRef(json& node, const std::function<void(json&)>& fn)
	{
		if (node.is_object())
		{
			json::iterator it = node.find("material");
			if (it != node.end() && it->is_number_unsigned()) fn(*it);
			for (json::iterator i = node.begin(); i != node.end(); ++i)
				if (i.key() != "material") ForEachMaterialRef(*i, fn);
		}
		else if (node.is_array())
		{
			for (size_t i = 0; i < node.size(); ++i) ForEachMaterialRef(node[i], fn);
		}
	}

	// The comparable form of a subtree: material indices replaced by the
	// materials themselves, and the overridable fields stripped from the
	// root. See the material-pools note at the top.
	inline json Canonical(const json& root, const json& pool)
	{
		json copy = root;
		ForEachMaterialRef(copy, [&pool](json& ref) {
			const size_t id = ref.get<size_t>();
			ref = (id < pool.size()) ? pool[id] : json();
		});
		if (copy.is_object())
		{
			size_t n = 0;
			const char* const* fields = OverrideFields(n);
			for (size_t i = 0; i < n; ++i) copy.erase(fields[i]);
			copy.erase("prefab");
		}
		return copy;
	}

	inline bool MatchesPrefab(const json& sceneRoot, const json& scenePool, const json& prefabJson)
	{
		if (!prefabJson.is_object() || prefabJson.find("root") == prefabJson.end()) return false;
		return Canonical(sceneRoot, scenePool)
			== Canonical(prefabJson["root"], prefabJson.value("materials", json::array()));
	}

	// ------------------------------ expand -------------------------------

	// Which scene root came from which prefab, so the caller can re-link the
	// live objects after the engine has loaded them, and collapse them again
	// on the way out.
	struct Link {
		size_t rootIndex;
		std::string prefabPath;
	};

	namespace detail {

		// Appends `from` to `into`, reusing an identical entry when there is
		// one. Returns old index → new index.
		inline std::vector<size_t> MergePool(json& into, const json& from)
		{
			std::vector<size_t> remap(from.size(), 0);
			for (size_t i = 0; i < from.size(); ++i)
			{
				size_t found = into.size();
				for (size_t k = 0; k < into.size(); ++k)
					if (into[k] == from[i]) { found = k; break; }
				if (found == into.size()) into.push_back(from[i]);
				remap[i] = found;
			}
			return remap;
		}

		inline void ApplyOverrides(json& root, const json& entry)
		{
			if (entry.find("name") != entry.end()) root["name"] = entry["name"];
			if (entry.find("position") != entry.end()) root["position"] = entry["position"];
			if (entry.find("rotation") != entry.end()) root["rotation"] = entry["rotation"];
			if (entry.find("scale") != entry.end()) root["scale"] = entry["scale"];
			// Additive: a prefab's own tags are part of what it is (an enemy
			// prefab tagged "enemy" stays one), and the union is what gets
			// written back, so this round-trips.
			if (entry.find("tags") != entry.end() && entry["tags"].is_array())
			{
				json tags = root.value("tags", json::array());
				for (size_t i = 0; i < entry["tags"].size(); ++i)
				{
					bool present = false;
					for (size_t k = 0; k < tags.size(); ++k)
						if (tags[k] == entry["tags"][i]) { present = true; break; }
					if (!present) tags.push_back(entry["tags"][i]);
				}
				root["tags"] = tags;
			}
		}

		// Rebuilds the pool from what is actually still referenced.
		// Collapsing a root removes the only references to its materials, and
		// without this the pool would grow by a prefab's worth of dead
		// entries on every save.
		inline void CompactPool(json& scene)
		{
			json& pool = scene["materials"];
			if (!pool.is_array() || pool.empty()) return;

			std::vector<bool> used(pool.size(), false);
			ForEachMaterialRef(scene["roots"], [&used](json& ref) {
				const size_t id = ref.get<size_t>();
				if (id < used.size()) used[id] = true;
			});

			std::vector<size_t> remap(pool.size(), 0);
			json compacted = json::array();
			for (size_t i = 0; i < pool.size(); ++i)
			{
				if (!used[i]) continue;
				remap[i] = compacted.size();
				compacted.push_back(pool[i]);
			}
			if (compacted.size() == pool.size()) return;

			ForEachMaterialRef(scene["roots"], [&remap](json& ref) {
				const size_t id = ref.get<size_t>();
				if (id < remap.size()) ref = remap[id];
			});
			pool = compacted;
		}

	} // namespace detail

	// Replaces every prefab reference in `scene`'s roots with the subtree it
	// names. `outLinks` records what was expanded; `outErrors` collects
	// prefabs that could not be read (those entries are left untouched - see
	// the note at the top).
	//
	// Roots only, deliberately: an instance nested as a child of another
	// object would not render anyway (SceneGraph::Add registers only the
	// object passed to it, not its descendants), so supporting it here would
	// be machinery for a case the engine cannot honour.
	inline void ExpandScene(json& scene, const LoadFn& load,
		std::vector<Link>& outLinks, std::vector<std::string>& outErrors)
	{
		if (!scene.is_object() || scene.find("roots") == scene.end() || !scene["roots"].is_array())
			return;
		if (scene.find("materials") == scene.end() || !scene["materials"].is_array())
			scene["materials"] = json::array();

		json& roots = scene["roots"];
		for (size_t i = 0; i < roots.size(); ++i)
		{
			json& entry = roots[i];
			if (!entry.is_object()) continue;
			json::iterator ref = entry.find("prefab");
			if (ref == entry.end() || !ref->is_string()) continue;

			const std::string path = ref->get<std::string>();
			const json prefabJson = load(path);
			if (!prefabJson.is_object() || prefabJson.find("root") == prefabJson.end())
			{
				outErrors.push_back(path);
				continue;
			}

			json expanded = prefabJson["root"];
			const std::vector<size_t> remap =
				detail::MergePool(scene["materials"], prefabJson.value("materials", json::array()));
			ForEachMaterialRef(expanded, [&remap](json& m) {
				const size_t id = m.get<size_t>();
				if (id < remap.size()) m = remap[id];
			});

			detail::ApplyOverrides(expanded, entry);
			// Kept on the expanded object so a caller that only has the JSON
			// (the player) still knows the link. The engine ignores it.
			expanded["prefab"] = path;

			Link link;
			link.rootIndex = i;
			link.prefabPath = path;
			outLinks.push_back(link);

			roots[i] = expanded;
		}
	}

	// ------------------------------ collapse -----------------------------

	// `rootPrefabPaths[i]` is the prefab root i is an instance of, or empty.
	// A root that still matches its source is replaced by a reference; one
	// that no longer does is left in full and reported in `outModified`, so
	// the caller can say so rather than silently choosing for the user.
	inline void CollapseScene(json& scene, const std::vector<std::string>& rootPrefabPaths,
		const LoadFn& load, std::vector<std::string>& outModified, std::vector<std::string>& outMissing)
	{
		if (!scene.is_object() || scene.find("roots") == scene.end() || !scene["roots"].is_array())
			return;

		json& roots = scene["roots"];
		const json pool = scene.value("materials", json::array());
		// One read per distinct prefab, not per instance: a scene of two
		// hundred enemies would otherwise parse the same file two hundred
		// times just to ask "did this one change?".
		std::vector<std::pair<std::string, json>> cache;

		for (size_t i = 0; i < roots.size() && i < rootPrefabPaths.size(); ++i)
		{
			const std::string& path = rootPrefabPaths[i];
			if (path.empty()) continue;

			json prefabJson;
			bool cached = false;
			for (size_t k = 0; k < cache.size(); ++k)
				if (cache[k].first == path) { prefabJson = cache[k].second; cached = true; break; }
			if (!cached)
			{
				prefabJson = load(path);
				cache.push_back(std::make_pair(path, prefabJson));
			}

			const std::string name = roots[i].value("name", std::string());
			if (!prefabJson.is_object() || prefabJson.find("root") == prefabJson.end())
			{
				// Writing a reference to a prefab that cannot be read would
				// turn a working object into one that loads as nothing.
				outMissing.push_back(name + " → " + path);
				roots[i].erase("prefab");
				continue;
			}

			if (!MatchesPrefab(roots[i], pool, prefabJson))
			{
				outModified.push_back(name + " → " + path);
				// The link survives being written out in full, so the editor
				// can still offer Apply and Revert on it next session.
				roots[i]["prefab"] = path;
				continue;
			}

			json reference;
			reference["prefab"] = path;
			reference["name"] = roots[i].value("name", std::string());
			if (roots[i].find("position") != roots[i].end()) reference["position"] = roots[i]["position"];
			if (roots[i].find("rotation") != roots[i].end()) reference["rotation"] = roots[i]["rotation"];
			if (roots[i].find("scale") != roots[i].end()) reference["scale"] = roots[i]["scale"];
			if (roots[i].find("tags") != roots[i].end()) reference["tags"] = roots[i]["tags"];
			roots[i] = reference;
		}

		detail::CompactPool(scene);
	}

	// The prefab an already-expanded root says it came from, or empty. Lets
	// a caller working only from JSON (the player, or the editor re-reading
	// a file it just wrote) recover the links without tracking them.
	inline std::string LinkOf(const json& root)
	{
		if (!root.is_object()) return std::string();
		json::const_iterator it = root.find("prefab");
		return (it != root.end() && it->is_string()) ? it->get<std::string>() : std::string();
	}

} // namespace prefab

#endif /* PREFABRESOLVER_H */
