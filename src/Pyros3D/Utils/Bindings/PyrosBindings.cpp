//============================================================================
// Name        : PyrosBindings.cpp
// Description : Lua bindings entry — modules mirror PyrosEmbind*.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>

namespace p3d {

	bool LuaComponent::s_updatesEnabled = true;

	void GenerateBindings(sol::state* lua)
	{
		lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::coroutine, sol::lib::table, sol::lib::string);

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
		RegisterLuaUI(lua);
	}

} // namespace p3d

#endif
