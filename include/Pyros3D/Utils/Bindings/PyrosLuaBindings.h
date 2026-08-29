//============================================================================
// Name        : PyrosLuaBindings.h
// Description : Split Lua binding modules (parity with PyrosEmbind*).
//============================================================================
#ifdef LUA_BINDINGS
#ifndef PYROSLUABINDINGS_H
#define PYROSLUABINDINGS_H

#include <Pyros3D/Utils/Bindings/PyrosBindings.h>

namespace p3d {

	void RegisterLuaEnums(sol::state* lua);
	void RegisterLuaMath(sol::state* lua);
	void RegisterLuaCore(sol::state* lua);
	void RegisterLuaRenderEarly(sol::state* lua);
	void RegisterLuaRenderMid(sol::state* lua);
	void RegisterLuaRenderLate(sol::state* lua);
	void RegisterLuaRender(sol::state* lua);
	void RegisterLuaPhysicsEarly(sol::state* lua);
	void RegisterLuaPhysicsLate(sol::state* lua);
	void RegisterLuaPhysics(sol::state* lua);
	void RegisterLuaAssetsEarly(sol::state* lua);
	void RegisterLuaAssetsMid(sol::state* lua);
	void RegisterLuaAssetsLate(sol::state* lua);
	void RegisterLuaAssets(sol::state* lua);
	void RegisterLuaPostFX(sol::state* lua);
	void RegisterLuaAudio(sol::state* lua);
	void RegisterLuaMisc(sol::state* lua);
	void RegisterLuaUI(sol::state* lua);

}

#endif
#endif
