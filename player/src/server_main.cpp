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
//                             [--password <p>] [--reconnect-grace <s>]
//                             [--max-speed <m/s>] [--stats <seconds>]
//
//               Defaults come from game.json's "server" block (what Build
//               Game's dialog wrote); a flag overrides. Banned addresses
//               are kept in bans.txt beside game.json, one a line.
//
//               Players hosting from home (PyrosPlayer --host) sit behind
//               routers; --rendezvous-service [port] runs, instead of a
//               game, the small service they meet through (see
//               NetRendezvous.h) - on a machine with a public address.
//               A dedicated server on a public address does not need it,
//               though --rendezvous <host[:port]> --session <name> lets
//               players find one by name too.
//
//               The server's long-term key lives in server.key beside
//               game.json - made on the first run, never shipped to
//               players. Its public half is printed at start (and by
//               --print-key, which then exits): put that in Build Game's
//               "Server public key" and clients will refuse any server
//               that is not this one.
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
#include <Pyros3D/Rendering/Components/Terrain/TerrainComponent.h>
#include <Pyros3D/Utils/Streaming/AssetStreamer.h>
#include <Pyros3D/Network/NetworkSession.h>
#include <Pyros3D/Network/NetRendezvous.h>
#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>
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
#include <set>
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

	// --rendezvous-service [port]: no game - only the meeting point.
	for (int i = 1; i < argc; i++)
	{
		if (std::string(argv[i]) != "--rendezvous-service") continue;
		const int servicePort = (i + 1 < argc && argv[i + 1][0] != '-') ? std::atoi(argv[i + 1]) : 47400;
		NetRendezvous service;
		if (!service.Start((uint16)servicePort))
		{
			fprintf(stderr, "PyrosServer: could not open UDP port %d for the rendezvous service\n", servicePort);
			return 1;
		}
		fprintf(stderr, "PyrosServer: rendezvous service on UDP port %d\n", servicePort);
		uint32 shown = 0;
		while (g_running)
		{
			service.Update();
			if (service.HostCount() != shown) { shown = service.HostCount(); fprintf(stderr, "PyrosServer: %u host(s) announced\n", shown); }
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		return 0;
	}

	// Nothing here is drawn: terrain tiles keep their heights (physics
	// stands on those) and build no render geometry - measured at 6.8 MB a
	// cell, which with players spread over an 8 km map was 6 GB.
	HeightfieldMesh::SetHeadless(true);

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
	// game.json's "server" block, then the flags over it.
	const json sv = manifest.contains("server") && manifest["server"].is_object() ? manifest["server"] : json::object();
	const uint16 port = (uint16)std::stoi(Arg(argc, argv, "--port", std::to_string(sv.value("port", 47400))));
	NetworkSettings settings;
	settings.maxClients = (uint32)std::stoi(Arg(argc, argv, "--max-clients", std::to_string(sv.value("maxClients", 100))));
	settings.tickRate = std::stof(Arg(argc, argv, "--tick", std::to_string(sv.value("tickRate", 30.f))));
	settings.password = Arg(argc, argv, "--password", sv.value("password", std::string()));
	settings.reconnectGrace = std::stof(Arg(argc, argv, "--reconnect-grace", std::to_string(sv.value("reconnectGrace", 30.f))));
	settings.maxClientSpeed = std::stof(Arg(argc, argv, "--max-speed", std::to_string(sv.value("maxClientSpeed", 0.f))));
	settings.rendezvous = Arg(argc, argv, "--rendezvous", sv.value("rendezvous", std::string()));
	settings.sessionName = Arg(argc, argv, "--session", sv.value("sessionName", std::string()));
	const f64 statsEvery = std::stod(Arg(argc, argv, "--stats", "0"));
	{
		const fs::path keyFile = game / "server.key";
		std::string secret;
		{ std::ifstream in(keyFile.string().c_str()); std::getline(in, secret); }
		if (NetworkSession::PublicKeyOf(secret).empty())
		{
			secret = NetworkSession::GenerateSecretKey();
			std::ofstream out(keyFile.string().c_str(), std::ios::trunc);
			out << secret << "\n";
			out.close();
			std::error_code kec;
			fs::permissions(keyFile, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, kec);
		}
		settings.serverSecretKey = secret;
		fprintf(stderr, "PyrosServer: public key %s\n", NetworkSession::PublicKeyOf(secret).c_str());
		for (int i = 1; i < argc; i++) if (std::string(argv[i]) == "--print-key") { printf("%s\n", NetworkSession::PublicKeyOf(secret).c_str()); return 0; }
	}
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
	// Bans outlive the process.
	const fs::path bansFile = game / "bans.txt";
	{
		std::ifstream in(bansFile.string().c_str());
		std::string line;
		while (std::getline(in, line)) if (!line.empty()) session->BanAddress(line);
	}
	size_t bansSaved = session->Bans().size();
	const auto saveBans = [&]() {
		std::ofstream out(bansFile.string().c_str(), std::ios::trunc);
		for (std::set<std::string>::const_iterator b = session->Bans().begin(); b != session->Bans().end(); ++b) out << *b << "\n";
		bansSaved = session->Bans().size();
	};
	echo("PyrosServer: " + sceneRel + " on port " + std::to_string(session->GetRole() == NetworkSession::Server ? port : 0)
		+ ", " + std::to_string(settings.maxClients) + " players, " + std::to_string((int)settings.tickRate) + " Hz");

	uint32 statFrames = 0;
	f64 statMs = 0.0, statWorst = 0.0, statClock = 0.0;
	uint64 statSent = 0;
	const std::chrono::duration<f64> frame(1.0 / std::max(frameRate, 1.0));
	std::chrono::steady_clock::time_point next = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point last = next;
	while (g_running)
	{
		const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		const f64 dt = std::min(std::chrono::duration<f64>(now - last).count(), 0.25);
		last = now;
		t += dt;

		const std::chrono::steady_clock::time_point work0 = std::chrono::steady_clock::now();
		session->Update(dt);
		// Cells and terrain tiles around every player; the world's streamer
		// pumps the loader both use.
		TerrainComponent::SetViewers(scene, session->ClientViewers());
		if (world) world->Update(session->ClientViewers());
		else AssetStreamer::Instance().Pump(4.0);
		physics->Update(dt, 10);
		scene->Update(t);
#ifdef LUA_BINDINGS
		if (mainScript)
		{
			try { mainScript->Update(t); }
			catch (const std::exception &e) { echo(std::string("ERROR: scene main script update - ") + e.what()); }
		}
#endif
		if (session->Bans().size() != bansSaved) saveBans();
		// --stats: what a frame costs and what goes out, every few seconds.
		if (statsEvery > 0.0)
		{
			const f64 ms = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - work0).count();
			statFrames++;
			statMs += ms;
			statWorst = std::max(statWorst, ms);
			statClock += dt;
			if (statClock >= statsEvery)
			{
				uint64 sent = 0;
				const std::vector<NetworkSession::PeerInfo> peers = session->Peers();
				for (size_t i = 0; i < peers.size(); i++) sent += peers[i].transport.bytesSent;
				fprintf(stderr, "PyrosServer: %zu players, %u objects, frame avg %.2f ms worst %.2f ms, %.1f kbit/s out per player\n",
					peers.size(), session->GetStats().replicated, statMs / std::max(1u, statFrames), statWorst,
					peers.empty() ? 0.0 : (f64)(sent - statSent) * 8.0 / 1000.0 / statClock / (f64)peers.size());
				statSent = sent;
				statFrames = 0; statMs = 0.0; statWorst = 0.0; statClock = 0.0;
			}
		}
		next += std::chrono::duration_cast<std::chrono::steady_clock::duration>(frame);
		if (next < std::chrono::steady_clock::now()) next = std::chrono::steady_clock::now();	// fell behind: do not spiral
		std::this_thread::sleep_until(next);
	}

	echo("PyrosServer: shutting down");
	if (session->Bans().size() != bansSaved) saveBans();
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
