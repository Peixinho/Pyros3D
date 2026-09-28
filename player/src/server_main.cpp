//============================================================================
// Name        : server_main.cpp
// Author      : Duarte Peixinho
// Description : PyrosServer - a built game run as a headless dedicated
//               server: no window, no GPU. It loads the game's scenes exactly
//               as the player does (through NullRenderDevice, which accepts
//               every mesh, texture and shader and draws nothing), runs
//               physics, scripts and the network session, and keeps the
//               streamed world loaded around every connected player.
//
//                 PyrosServer [--game <dir>] [--scene <scenes/x.json>]
//                             [--port 47400] [--max-clients 100]
//                             [--tick 30] [--frame-rate 60]
//
//               Scripts see HEADLESS = true. A scene script that calls
//               network.host() itself decides the port; otherwise the server
//               hosts on --port once the scene has started.
//============================================================================

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Physics/PhysicsEngines/Box3D/Box3DPhysics.h>
#include <Pyros3D/Rendering/Device/NullRenderDevice.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Utils/Streaming/WorldStreamer.h>
#include <Pyros3D/Utils/Streaming/AssetStreamer.h>
#include <Pyros3D/Network/NetworkSession.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/Utils/CrashHandler/CrashHandler.h>
#include <Pyros3D/Core/Logs/Log.h>
#ifdef LUA_BINDINGS
#include <Pyros3D/Ext/sol/sol.hpp>
#include <Pyros3D/Utils/Bindings/PyrosBindings.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaNetwork.h>
#endif
#include "PrefabResolver.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

using namespace p3d;
using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
	std::atomic<bool> g_running(true);
	void OnSignal(int) { g_running = false; }

	std::string Arg(int argc, char** argv, const char* name, const std::string &fallback)
	{
		for (int i = 1; i + 1 < argc; i++)
			if (std::string(argv[i]) == name) return argv[i + 1];
		return fallback;
	}
}

int main(int argc, char** argv)
{
	InstallCrashHandler();
	std::signal(SIGINT, OnSignal);
	std::signal(SIGTERM, OnSignal);
	// A server's log is its console; show what a player's would hide.
	LOG::_LOG::SetLevel(LOG::Level::Info);

	// The game folder: --game, else wherever game.json is found from here.
	std::error_code ec;
	fs::path game = Arg(argc, argv, "--game", fs::current_path(ec).string());
	if (!fs::exists(game / "game.json", ec))
	{
		const fs::path exe = fs::path(argv[0]).parent_path();
		if (fs::exists(exe / "game.json", ec)) game = exe;
	}
	if (!fs::exists(game / "game.json", ec))
	{
		fprintf(stderr, "PyrosServer: no game.json in %s (use --game <dir>)\n", game.string().c_str());
		return 1;
	}
	game = fs::weakly_canonical(game, ec);
	fs::current_path(game, ec);	// shaders and assets resolve from here, as in the player

	json manifest;
	{
		std::ifstream in((game / "game.json").string().c_str());
		try { in >> manifest; }
		catch (const std::exception &e) { fprintf(stderr, "PyrosServer: game.json - %s\n", e.what()); return 1; }
	}
	const std::string sceneRel = Arg(argc, argv, "--scene", manifest.value("startupScene", std::string()));
	const uint16 port = (uint16)std::stoi(Arg(argc, argv, "--port", "47400"));
	NetworkSettings settings;
	settings.maxClients = (uint32)std::stoi(Arg(argc, argv, "--max-clients", "100"));
	settings.tickRate = std::stof(Arg(argc, argv, "--tick", "30"));
	const f64 frameRate = std::stod(Arg(argc, argv, "--frame-rate", "60"));

	// Every GPU call the loaders make lands here and does nothing.
	std::shared_ptr<IRenderDevice> device = std::make_shared<NullRenderDevice>();
	SetActiveRenderDevice(device);

	SceneGraph* scene = new SceneGraph();
	Box3DPhysics* physics = new Box3DPhysics();
	physics->InitPhysics();

#ifdef LUA_BINDINGS
	sol::state lua;
	GenerateBindings(&lua);
	try { lua["class"] = lua.require_file("class", (game / "lua" / "middleclass.lua").string()); }
	catch (const std::exception &e) { echo(std::string("ERROR: lua/middleclass.lua - ") + e.what()); }
	lua.set_function("__pyros_log", [](const std::string &msg) { echo(msg); });
	lua.script("function print(...) local t = {} for i = 1, select('#', ...) do t[i] = tostring(select(i, ...)) end __pyros_log(table.concat(t, '\\t')) end");
	lua["HEADLESS"] = true;
	lua["scene"] = scene;
	lua["physics"] = static_cast<IPhysics*>(physics);
	lua["ASSETS_PATH"] = (game / "assets").string() + "/";
	sol::state* luaPtr = &lua;
#else
	sol::state* luaPtr = NULL;
#endif

	// The scene, prefabs expanded exactly as the player expands them.
	const std::string sceneAbs = (game / sceneRel).string();
	std::string text;
	{
		std::ifstream in(sceneAbs.c_str());
		if (!in.is_open()) { fprintf(stderr, "PyrosServer: cannot open %s\n", sceneAbs.c_str()); return 1; }
		std::stringstream ss;
		ss << in.rdbuf();
		text = ss.str();
	}
	try
	{
		json sceneJson = json::parse(text);
		std::vector<prefab::Link> links;
		std::vector<std::string> errors;
		prefab::ExpandScene(sceneJson, [&game](const std::string &rel) { return prefab::ReadPrefabFile((game / rel).string()); }, links, errors);
		for (size_t i = 0; i < errors.size(); i++) echo("ERROR: prefab not found: " + errors[i]);
		text = sceneJson.dump();
	}
	catch (const std::exception &) { /* the loader reports it */ }

	LoadedSceneAssets assets;
	SceneMeta meta;
	if (!SceneSerializer::LoadSceneFromText(scene, text, sceneAbs, physics, luaPtr, &assets, &meta))
	{
		fprintf(stderr, "PyrosServer: could not load %s\n", sceneAbs.c_str());
		return 1;
	}

	NetworkSession* session = new NetworkSession(scene, sceneAbs, physics, luaPtr);
#ifdef LUA_BINDINGS
	RegisterLuaNetwork(&lua, [session]() { return session; });
#endif

	// The whole streamed world would be the whole map; a server keeps
	// loaded what its players are near, which is where anything happens.
	// No far versions: nothing here is drawn.
	meta.world.farRadius = 0.f;
	WorldStreamer* world = meta.world.enabled ? new WorldStreamer(scene, sceneAbs, meta.world, physics, luaPtr) : NULL;

	f64 t = 0.0;
	scene->Update(t);	// components register; LuaComponents init
#ifdef LUA_BINDINGS
	std::shared_ptr<LuaComponent> mainScript;
	if (!meta.mainScript.empty())
	{
		try { mainScript = LuaComponent_FromFile(lua, meta.mainScript); if (mainScript) mainScript->Init(); }
		catch (const std::exception &e) { echo(std::string("ERROR: scene main script - ") + e.what()); }
	}
#endif
	if (session->GetRole() == NetworkSession::Offline && !session->Host(port, settings))
	{
		fprintf(stderr, "PyrosServer: could not host on port %u\n", (unsigned)port);
		return 1;
	}
	echo("PyrosServer: " + sceneRel + " on port " + std::to_string(session->GetRole() == NetworkSession::Server ? port : 0)
		+ ", " + std::to_string(settings.maxClients) + " players, " + std::to_string((int)settings.tickRate) + " Hz");

	const std::chrono::duration<f64> frame(1.0 / std::max(frameRate, 1.0));
	std::chrono::steady_clock::time_point next = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point last = next;
	while (g_running)
	{
		const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		const f64 dt = std::min(std::chrono::duration<f64>(now - last).count(), 0.25);
		last = now;
		t += dt;

		session->Update(dt);
		if (world) world->Update(session->ClientViewers());
		physics->Update(dt, 10);
		scene->Update(t);
#ifdef LUA_BINDINGS
		if (mainScript)
		{
			try { mainScript->Update(t); }
			catch (const std::exception &e) { echo(std::string("ERROR: scene main script update - ") + e.what()); }
		}
#endif
		next += std::chrono::duration_cast<std::chrono::steady_clock::duration>(frame);
		if (next < std::chrono::steady_clock::now()) next = std::chrono::steady_clock::now();	// fell behind: do not spiral
		std::this_thread::sleep_until(next);
	}

	echo("PyrosServer: shutting down");
	session->Shutdown();
	delete session;
	delete world;
#ifdef LUA_BINDINGS
	mainScript.reset();
#endif
	AssetStreamer::Instance().Shutdown();
	SceneSerializer::UnloadScene(scene, assets);
	delete physics;
	delete scene;
	SetActiveRenderDevice(NULL);
	return 0;
}
