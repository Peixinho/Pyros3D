// The `network` table's variadic calls - network.rpc and network.input -
// carrying Lua values into NetValues. Written after MSVC refused the
// implicit sol::stack_proxy -> sol::object conversion they relied on: the
// fix made the conversion explicit, and this proves the values still
// arrive intact (number, string, boolean, vector), over a real loopback
// session.
//
//   c++ -std=c++17 -DLUA_BINDINGS -DPYROS_NETWORKING -I include -I src/Pyros3D/Ext/box3d/include \
//       -I /opt/homebrew/opt/lua@5.4/include/lua tools/tests/lua_network.cpp \
//       -o /tmp/lua_network -L build_ed_vk -lPyrosEngine -L /opt/homebrew/opt/lua@5.4/lib -llua \
//       -Wl,-rpath,$PWD/build_ed_vk

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Network/NetworkSession.h>
#include <Pyros3D/Ext/sol/sol.hpp>
#include <Pyros3D/Utils/Bindings/PyrosBindings.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaNetwork.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const std::string &what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what.c_str());
	fflush(stdout);
	if (!cond) failures++;
}

int main()
{
	const std::string scenePath = (std::filesystem::temp_directory_path() / "lua_network_test.json").string();
	SceneGraph serverScene, clientScene;
	NetworkSession server(&serverScene, scenePath), client(&clientScene, scenePath);

	sol::state lua;
	lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
	GenerateBindings(&lua);
	RegisterLuaNetwork(&lua, [&client]() { return &client; });

	std::vector<NetValue> got;
	PeerId from = 0;
	server.OnRpc("hello", [&](const PeerId sender, const std::vector<NetValue> &args) { from = sender; got = args; });

	const uint16 port = 47321;
	check(server.Host(port), "server hosts");
	lua.script("assert(network.connect('127.0.0.1', " + std::to_string(port) + "))");
	auto frames = [&](const double seconds, const std::function<bool()> &until) {
		for (int i = 0; i < (int)(seconds * 60); i++)
		{
			server.Update(1.0 / 60.0);
			client.Update(1.0 / 60.0);
			if (until && until()) return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
		}
		return until ? until() : true;
	};
	check(frames(3.0, [&] { return client.IsReady(); }), "the Lua client joins");

	lua.script("network.rpc(0, 'hello', 42.5, 'text', true, Vec3.new(1, 2, 3))");
	check(frames(3.0, [&] { return !got.empty(); }), "an RPC called from Lua arrives");
	check(from == client.LocalPeer(), "from that client");
	check(got.size() == 4, "with all four arguments");
	if (got.size() == 4)
	{
		check(got[0].type == NetValue::Number && got[0].number == 42.5, "a number");
		check(got[1].type == NetValue::String && got[1].text == "text", "a string");
		check(got[2].type == NetValue::Bool && got[2].boolean, "a boolean");
		check(got[3].type == NetValue::Vector && got[3].vector.x == 1.f && got[3].vector.y == 2.f && got[3].vector.z == 3.f, "a vector");
	}
	// network.input is the other variadic entry point; it has no reply to
	// wait for, so this proves it converts and does not throw.
	bool inputOk = true;
	try { lua.script("network.input(1, 0.5, false)"); }
	catch (const std::exception &e) { inputOk = false; printf("%s\n", e.what()); }
	check(inputOk, "network.input takes mixed values");

	client.Shutdown();
	server.Shutdown();
	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures ? 1 : 0;
}
