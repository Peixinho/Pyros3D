//============================================================================
// Name        : NeonPulse.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Neon Pulse - a game written entirely in Lua
//============================================================================

#include "NeonPulse.h"

using namespace p3d;

NeonPulse::NeonPulse() : ClassName(1280, 800, "Pyros3D - Neon Pulse", WindowType::Close | WindowType::Resize) {}

void NeonPulse::OnResize(const uint32 width, const uint32 height)
{
	ClassName::OnResize(width, height);

	lua["SCREEN_W"] = width;
	lua["SCREEN_H"] = height;

	sol::protected_function resize = lua["resize"];
	if (resize.valid()) resize(width, height);
}

void NeonPulse::Init()
{
	// GenerateBindings() opens base/math/table/string/coroutine itself;
	// `package` is deliberately not opened anywhere, so the game's own
	// modules are loaded with loadfile() off GAME_PATH rather than
	// require() - see assets/neonpulse/main.lua's import().
	GenerateBindings(&lua);

	lua.script(R"(
		function got_problems(error_msg)
			print("[lua] " .. error_msg)
		end)");
	sol::protected_function::set_default_handler(lua["got_problems"]);

	// The only state the host hands the game: where its files are and how
	// big the window is. Everything else the game builds for itself.
	lua["GAME_PATH"] = STR(EXAMPLES_PATH) "/assets/neonpulse/";
	lua["ASSETS_PATH"] = STR(EXAMPLES_PATH) "/assets/";
	lua["SCREEN_W"] = Width;
	lua["SCREEN_H"] = Height;

	RunScript(STR(EXAMPLES_PATH) "/assets/neonpulse/main.lua");

	sol::protected_function init = lua["init"];
	init();
}

void NeonPulse::RunScript(const std::string &file)
{
	// safe_script_file() loads and runs through sol's own protected path -
	// same "[lua] " prefix as got_problems (Init()'s default handler for
	// ordinary protected_function calls like init()/update()) so a syntax
	// error here reads identically to a runtime one instead of failing
	// silently.
	lua.safe_script_file(file, [](lua_State*, sol::protected_function_result pfr) {
		sol::error err = pfr;
		fprintf(stderr, "[lua] %s\n", err.what());
		return pfr;
	});
}

void NeonPulse::Update()
{
	sol::protected_function update = lua["update"];
	update(GetTime(), GetTimeInterval());
}

void NeonPulse::Shutdown() {}

NeonPulse::~NeonPulse() {}
