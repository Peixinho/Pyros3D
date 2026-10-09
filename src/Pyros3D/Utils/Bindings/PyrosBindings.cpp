#include <cstdlib>
//============================================================================
// Name        : PyrosBindings.cpp
// Description : Lua bindings entry — modules mirror PyrosEmbind*.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>

namespace p3d {

	bool LuaComponent::s_updatesEnabled = true;

	void RegisterLuaJson(sol::state* lua);		// PyrosLuaJson.cpp

	void GenerateBindings(sol::state* lua)
	{
		lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::coroutine, sol::lib::table, sol::lib::string);
		// (PYROS_LUA_DEBUG=1: Lua's debug library too, for looking into what the scripts hold - not for a game to use)
		if (std::getenv("PYROS_LUA_DEBUG") != NULL) lua->open_libraries(sol::lib::debug);

		// Order matters for sol usertype bases — keep interleaved registration.
		RegisterLuaEnums(lua);
		RegisterLuaMath(lua);
		RegisterLuaCore(lua);
		RegisterLuaRenderEarly(lua);
		RegisterLuaPhysicsEarly(lua);
		RegisterLuaRenderMid(lua);
		RegisterLuaAssetsEarly(lua);
		RegisterLuaRenderLate(lua);
		RegisterLuaAssetsMid(lua);
		RegisterLuaPhysicsLate(lua);
		RegisterLuaPostFX(lua);
		RegisterLuaAssetsLate(lua);
		RegisterLuaAudio(lua);
		RegisterLuaMisc(lua);
		RegisterLuaJson(lua);
		RegisterLuaUI(lua);
	}

} // namespace p3d

#endif
