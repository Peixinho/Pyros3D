//============================================================================
// Name        : PyrosLuaBindings.h
// Description : Split Lua binding modules (parity with PyrosEmbind*).
//============================================================================
#ifdef LUA_BINDINGS
#ifndef PYROSLUABINDINGS_H
#define PYROSLUABINDINGS_H

#include <Pyros3D/Utils/Bindings/PyrosBindings.h>
#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <Pyros3D/Rendering/Components/Layer2D/Layer2D.h>

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

	// Garbage collection where a frame can afford it. Left to itself Lua
	// collects whenever enough has been allocated - in the middle of whatever
	// function happens to allocate next - and a step can be several
	// milliseconds: a game whose scripts make thousands of small objects a
	// frame stuttered a dozen times a minute, the time landing in functions
	// that do almost nothing. Call this once a frame, after the scripts have
	// run: the first call stops the automatic collector for that state, and
	// each call then collects for about budgetMs (more when the heap is
	// growing faster than that keeps up with). PYROS_LUA_GC=auto leaves Lua's
	// own collector alone.
	PYROS3D_API void LuaCollectWithinBudget(sol::state* lua, const f64 budgetMs = 1.0);
	// Drops every task scripts have running (`tasks`): when the scene they
	// were working on goes.
	PYROS3D_API void LuaClearTasks(sol::state* lua);
	void RegisterLuaUI(sol::state* lua);

}

#endif
#endif
