//============================================================================
// Name        : LuaScripting.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "LuaScripting.h"

using namespace p3d;

LuaScripting::LuaScripting() : ClassName(1024, 768, "Pyros3D - Rotating Cube", WindowType::Close | WindowType::Resize) {}

void LuaScripting::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	sol::protected_function lua_resize = lua["resize"];
	lua_resize(width, height);


}

void LuaScripting::Init()
{
	lua.open_libraries(sol::lib::base);

	GenerateBindings(&lua);

	// Support for classes in Lua
	lua.require_file("class", STR(EXAMPLES_PATH)"/LuaScripting/middleclass.lua");

	// Error message when something goes wrong
	lua.script(R"(
					function got_problems( error_msg )
        			print("got_problems handler: " .. error_msg)
					end)");

	// Handler for error
	sol::protected_function::set_default_handler(lua["got_problems"]);

	// Main script
	mainScript = LoadScript(STR(EXAMPLES_PATH)"/LuaScripting/main.lua");

	// Run init function from main
	sol::protected_function lua_init = lua["init"];
	lua_init();
}

bool LuaScripting::LoadScript(const std::string &file)
{
    bool rt = false;
    std::unique_ptr<File> f = std::make_unique<File>();
    f->Open(file, false);

    std::string code; code.resize(f->Size()); memcpy(&code[0], &f->GetData()[0], f->Size());

    f->Close();

auto bad_code_result = lua.script(code.c_str(), [](lua_State*, sol::protected_function_result pfr) {
		// pfr will contain things that went wrong, for either loading or executing the script
		// Can throw your own custom error
		// You can also just return it, and let the call-site handle the error if necessary.
		return pfr;
	});
	return 0;
}


void LuaScripting::Update()
{
	sol::protected_function lua_update = lua["update"];
	lua_update(GetTime());
}

void LuaScripting::Shutdown()
{

}

LuaScripting::~LuaScripting() {}
