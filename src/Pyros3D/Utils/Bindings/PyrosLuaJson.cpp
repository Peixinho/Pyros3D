//============================================================================
// Name        : PyrosLuaJson.cpp
// Description : json.encode / json.decode for scripts. A file of its own
//               because json.hpp cannot be included after the headers the
//               other binding files need (one defines isnan as a macro).
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Json/json.hpp>
#include <cmath>
#include <sstream>
#include <tuple>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Ext/sol/sol.hpp>

namespace p3d {

	namespace {
		// Lua <-> JSON. A table whose keys are exactly 1..n is an array (an
		// empty one too); anything else is an object, its keys turned to text.
		nlohmann::json LuaToJson(const sol::object &o, const int depth)
		{
			if (depth > 64) return nullptr;
			switch (o.get_type())
			{
			case sol::type::boolean: return o.as<bool>();
			case sol::type::number:
			{
				const f64 v = o.as<f64>();
				if (v == std::floor(v) && std::fabs(v) < 9.0e15) return (int64_t)v;
				return v;
			}
			case sol::type::string: return o.as<std::string>();
			case sol::type::table:
			{
				sol::table t = o.as<sol::table>();
				size_t count = 0;
				bool array = true;
				for (const auto &kv : t)
				{
					count++;
					if (kv.first.get_type() != sol::type::number) { array = false; continue; }
					const f64 k = kv.first.as<f64>();
					if (k != std::floor(k) || k < 1.0) array = false;
				}
				if (array && t.size() != count) array = false;
				if (array)
				{
					nlohmann::json out = nlohmann::json::array();
					for (size_t i = 1; i <= count; i++) out.push_back(LuaToJson(t[i], depth + 1));
					return out;
				}
				nlohmann::json out = nlohmann::json::object();
				for (const auto &kv : t)
				{
					std::string key;
					if (kv.first.get_type() == sol::type::string) key = kv.first.as<std::string>();
					else if (kv.first.get_type() == sol::type::number) { std::ostringstream s; s << kv.first.as<f64>(); key = s.str(); }
					else continue;
					out[key] = LuaToJson(kv.second, depth + 1);
				}
				return out;
			}
			default: return nullptr;
			}
		}

		sol::object JsonToLua(sol::state_view lua, const nlohmann::json &j)
		{
			if (j.is_boolean()) return sol::make_object(lua, j.get<bool>());
			if (j.is_number()) return sol::make_object(lua, j.get<f64>());
			if (j.is_string()) return sol::make_object(lua, j.get<std::string>());
			if (j.is_array())
			{
				sol::table t = lua.create_table();
				int n = 1;
				for (const auto &v : j) t[n++] = JsonToLua(lua, v);
				return t;
			}
			if (j.is_object())
			{
				sol::table t = lua.create_table();
				for (auto it = j.begin(); it != j.end(); ++it) t[it.key()] = JsonToLua(lua, it.value());
				return t;
			}
			return sol::make_object(lua, sol::lua_nil);
		}
	}

	void RegisterLuaJson(sol::state* lua)
	{
		{
			// json.encode(value) -> text;  json.decode(text) -> value, or nil and
			// what was wrong with it. For anything a script sends or keeps as
			// text: a message to another machine, a saved setting, a web page.
			sol::table js = lua->create_named_table("json");
			js.set_function("encode", [](const sol::object &value, sol::optional<bool> pretty) {
				return LuaToJson(value, 0).dump(pretty.value_or(false) ? 2 : -1, ' ', false, nlohmann::json::error_handler_t::replace);
			});
			js.set_function("decode", [](const std::string &text, sol::this_state ts) -> std::tuple<sol::object, sol::object> {
				sol::state_view L(ts);
				try { return std::make_tuple(JsonToLua(L, nlohmann::json::parse(text)), sol::make_object(L, sol::lua_nil)); }
				catch (const std::exception &e) { return std::make_tuple(sol::make_object(L, sol::lua_nil), sol::make_object(L, std::string(e.what()))); }
			});
		}
	}

} // namespace p3d

#endif
