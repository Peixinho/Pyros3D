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
//                             [--admin-port <n>] [--admin-bind 127.0.0.1]
//                             [--admin-password <p>] [--admin-dir assets/admin]
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
//               Managing it from a browser: --admin-port serves the files in
//               --admin-dir (a page the game ships) and, under /api/, whatever
//               the game's scripts answer:
//
//                 admin.on("status", function(req) return { players = 3 } end)
//                     -- GET or POST /api/status; req.data is the JSON sent
//
//               /api/login takes {"password": ...} and gives the browser a
//               session cookie; every other /api/ call needs it. The password
//               is --admin-password, else $PYROS_ADMIN_PASSWORD, else game.json's
//               server.adminPassword - and without one the page is only served
//               on 127.0.0.1. It is plain HTTP: over the internet, reach it
//               through an SSH tunnel or a reverse proxy that does TLS.
//               store.read(name) / store.write(name, text) keep small files in
//               data/ beside game.json, for settings that outlive the process.
//
//               Scripts see HEADLESS = true. A scene script that calls
//               network.host() itself decides the port; otherwise the server
//               hosts on --port once the scene has started.
//============================================================================

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Physics/PhysicsEngines/Box3D/Box3DPhysics.h>
#include <Pyros3D/Rendering/Device/NullRenderDevice.h>
#include <Pyros3D/Rendering/Components/Foliage/Foliage.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Utils/Streaming/WorldStreamer.h>
#include <Pyros3D/Utils/Streaming/AssetStreamer.h>
#include <Pyros3D/Rendering/Components/Terrain/TerrainComponent.h>
#include <Pyros3D/Utils/Streaming/AssetStreamer.h>
#include <Pyros3D/Network/NetworkSession.h>
#include <Pyros3D/Network/NetRendezvous.h>
#include <Pyros3D/Network/HttpService.h>
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

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <thread>

using namespace p3d;
using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
	std::atomic<bool> g_running(true);
	void OnSignal(int) { g_running = false; }
	// server.restart(): the loop ends as for a signal, and the process starts
	// itself again once everything is put away - onto whatever map the match's
	// settings now name.
	std::atomic<bool> g_restart(false);

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
	FoliageComponent::SetHeadless(true);

	// The game folder: --game, else wherever game.json is found from here.
	std::error_code ec;
	// (for restarting: the arguments are relative to where it was started)
	const fs::path startedIn = fs::current_path(ec);
	const fs::path startedAs = fs::absolute(fs::path(argv[0]), ec);
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
	// game.json's "server" block, then the flags over it.
	const json sv = manifest.contains("server") && manifest["server"].is_object() ? manifest["server"] : json::object();
	// Which map. A server has no scene of its own to be given: it runs the map
	// its match is set to - what the admin page last saved (data/match.json),
	// or the game's default for a server that has never been set up
	// ("server": { "map" }) - as scenes/<map>.json. --scene overrides, for
	// running one by hand; a game with no maps to choose between falls back to
	// its startup scene.
	std::string sceneRel = Arg(argc, argv, "--scene", std::string());
	if (sceneRel.empty())
	{
		std::string map;
		{
			std::ifstream in((game / "data" / "match.json").string().c_str());
			if (in.is_open())
			{
				json saved = json::parse(in, NULL, false);
				if (saved.is_object() && saved.contains("map") && saved["map"].is_string()) map = saved["map"].get<std::string>();
			}
		}
		if (map != "random" && (map.empty() || !fs::exists(game / "scenes" / (map + ".json"), ec))) map = sv.value("map", std::string());
		// "random": one of the game's maps ("server": { "maps": [...] }), a new
		// roll every time the server starts.
		if (map == "random")
		{
			std::vector<std::string> maps;
			if (sv.contains("maps") && sv["maps"].is_array())
				for (const json &m : sv["maps"])
					if (m.is_string() && fs::exists(game / "scenes" / (m.get<std::string>() + ".json"), ec)) maps.push_back(m.get<std::string>());
			map.clear();
			if (!maps.empty())
			{
				std::random_device rd;
				map = maps[rd() % maps.size()];
			}
		}
		if (!map.empty() && fs::exists(game / "scenes" / (map + ".json"), ec)) sceneRel = "scenes/" + map + ".json";
		else sceneRel = manifest.value("startupScene", std::string());
	}
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
	const uint16 adminPort = (uint16)std::stoi(Arg(argc, argv, "--admin-port", std::to_string(sv.value("adminPort", 0))));
	const std::string adminBind = Arg(argc, argv, "--admin-bind", sv.value("adminBind", std::string("127.0.0.1")));
	const std::string adminDir = Arg(argc, argv, "--admin-dir", sv.value("adminDir", std::string("assets/admin")));
	std::string adminPassword = Arg(argc, argv, "--admin-password", "");
	if (adminPassword.empty()) { const char* env = std::getenv("PYROS_ADMIN_PASSWORD"); if (env) adminPassword = env; }
	if (adminPassword.empty()) adminPassword = sv.value("adminPassword", std::string());

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
	lua["echo"] = [](const std::string &msg) { p3d::LOG::_LOG::_echo(msg); };		// as the player and the editor give scripts
	lua["profileBegin"] = [](const std::string &) {};      // (a server keeps no frame profile)
	lua["profileEnd"] = []() {};
	lua["profileCount"] = [](const std::string &, const f64) {};
	lua.script("function print(...) local t = {} for i = 1, select('#', ...) do t[i] = tostring(select(i, ...)) end __pyros_log(table.concat(t, '\\t')) end");
	lua["HEADLESS"] = true;
	lua["scene"] = scene;
	lua["physics"] = static_cast<IPhysics*>(physics);
	lua["ASSETS_PATH"] = (game / "assets").string() + "/";
	sol::state* luaPtr = &lua;

	// Places the server keeps loaded whether or not a player is near: a streamed
	// world and its terrain are otherwise only there round the players, and a
	// script that has to look at the ground before anyone has joined - to roll
	// where things spawn - finds none.  server.keepLoaded({ Vec3, ... })
	std::vector<Vec3> keepLoaded;
	{
		sol::table server = lua.create_named_table("server");
		// server.restart(): everyone is dropped and the server comes back as it
		// would from the command line - how a change of map takes effect.
		server.set_function("restart", []() { g_restart = true; g_running = false; });
		// which map this is: the scene's name
		server["map"] = fs::path(sceneRel).stem().string();
		// server.option("team-size"): what this server was launched with - the value
		// after --team-size on the command line, else "team-size" in game.json's
		// "server" section, else nil. For whatever a game's own script wants to be
		// told at launch; the engine does not look at any of it.
		std::vector<std::string> launchArgs(argv, argv + argc);
		server.set_function("option", [launchArgs, sv](const std::string &name, sol::this_state ts) -> sol::object {
			sol::state_view L(ts);
			const std::string flag = "--" + name;
			for (size_t i = 1; i + 1 < launchArgs.size(); i++)
				if (launchArgs[i] == flag) return sol::make_object(L, launchArgs[i + 1]);
			if (sv.is_object() && sv.contains(name))
			{
				const auto &v = sv[name];
				if (v.is_string()) return sol::make_object(L, v.get<std::string>());
				if (v.is_number()) return sol::make_object(L, v.get<double>());
				if (v.is_boolean()) return sol::make_object(L, v.get<bool>());
			}
			return sol::make_object(L, sol::lua_nil);
		});
		server.set_function("keepLoaded", [&keepLoaded](sol::optional<sol::table> points) {
			keepLoaded.clear();
			if (!points) return;
			for (const auto &kv : *points) if (kv.second.is<Vec3>()) keepLoaded.push_back(kv.second.as<Vec3>());
		});
	}

	// What the admin page can ask: admin.on(name, fn) answers /api/<name>.
	std::map<std::string, sol::protected_function> adminHandlers;
	{
		sol::table admin = lua.create_named_table("admin");
		admin.set_function("on", [&adminHandlers](const std::string &name, sol::protected_function fn) { adminHandlers[name] = fn; });
		// Small files that outlive the process, in data/ beside game.json.
		const fs::path dataDir = game / "data";
		const auto safe = [](const std::string &name) {
			if (name.empty() || name.size() > 64 || name[0] == '.') return false;
			for (size_t i = 0; i < name.size(); i++)
			{
				const char c = name[i];
				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
			}
			return true;
		};
		sol::table store = lua.create_named_table("store");
		store.set_function("read", [dataDir, safe](const std::string &name, sol::this_state ts) -> sol::object {
			sol::state_view L(ts);
			if (!safe(name)) return sol::make_object(L, sol::lua_nil);
			std::ifstream in((dataDir / name).string().c_str(), std::ios::binary);
			if (!in.is_open()) return sol::make_object(L, sol::lua_nil);
			std::stringstream ss;
			ss << in.rdbuf();
			return sol::make_object(L, ss.str());
		});
		store.set_function("write", [dataDir, safe](const std::string &name, const std::string &text) {
			if (!safe(name)) return false;
			std::error_code dec;
			fs::create_directories(dataDir, dec);
			std::ofstream out((dataDir / name).string().c_str(), std::ios::binary | std::ios::trunc);
			if (!out.is_open()) return false;
			out << text;
			return true;
		});
	}
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
	// the cells name their prefab instances, as the scene does
	SceneSerializer::SetSubtreeFileFilter(prefab::ExpandSubtreeText);
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
	// The admin page. Everything under /api/ but the login needs the cookie
	// the login gave out; a wrong password costs a wait that grows.
	HttpService web;
#ifdef LUA_BINDINGS
	std::map<std::string, f64> adminSessions;		// token -> when it stops being good
	f64 loginNotBefore = 0.0;
	uint32 loginFailures = 0;
	f64* clock = &t;
	if (adminPort != 0)
	{
		const bool local = adminBind == "127.0.0.1" || adminBind == "localhost";
		if (adminPassword.empty() && !local)
			fprintf(stderr, "PyrosServer: --admin-port ignored: no admin password, and %s is not this machine only\n", adminBind.c_str());
		else
		{
			web.SetStaticRoot((game / adminDir).string());
			web.SetHandler([&](const HttpService::Request &q, HttpService::Response &r) {
				const std::string name = q.path.substr(5);
				const auto fail = [&r](const int status, const char* why) { r.status = status; r.body = std::string("{\"error\":\"") + why + "\"}"; };
				// a page on another site cannot make the browser send this header
				if (q.method != "GET" && q.headers.find("x-requested-with") == q.headers.end()) { fail(403, "missing X-Requested-With"); return; }
				const std::string token = q.Cookie("pyros_admin");
				std::map<std::string, f64>::iterator s = token.empty() ? adminSessions.end() : adminSessions.find(token);
				if (s != adminSessions.end() && s->second < *clock) { adminSessions.erase(s); s = adminSessions.end(); }
				const bool in = adminPassword.empty() || s != adminSessions.end();

				if (name == "session") { r.body = std::string("{\"loggedIn\":") + (in ? "true" : "false") + ",\"passwordNeeded\":" + (adminPassword.empty() ? "false" : "true") + "}"; return; }
				if (name == "login")
				{
					if (*clock < loginNotBefore) { fail(429, "wait a moment before trying again"); return; }
					std::string given;
					try { given = json::parse(q.body).value("password", std::string()); } catch (const std::exception &) {}
					// compared in full whatever the first difference: no telling how much was right
					unsigned char diff = (unsigned char)(given.size() != adminPassword.size());
					for (size_t i = 0; i < given.size() && i < adminPassword.size(); i++) diff |= (unsigned char)(given[i] ^ adminPassword[i]);
					if (adminPassword.empty() || diff != 0)
					{
						loginFailures++;
						loginNotBefore = *clock + std::min(30.0, (f64)loginFailures);
						echo("PyrosServer: admin login refused from " + q.remote);
						fail(401, "wrong password");
						return;
					}
					loginFailures = 0;
					std::random_device rd;
					std::string fresh;
					const char* hex = "0123456789abcdef";
					for (int i = 0; i < 16; i++) { const unsigned v = rd(); for (int k = 0; k < 4; k++) fresh += hex[(v >> (k * 4)) & 15]; }
					adminSessions[fresh] = *clock + 12.0 * 3600.0;
					r.headers["Set-Cookie"] = "pyros_admin=" + fresh + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=43200";
					r.body = "{\"ok\":true}";
					echo("PyrosServer: admin logged in from " + q.remote);
					return;
				}
				if (!in) { fail(401, "log in first"); return; }
				if (name == "logout") { if (s != adminSessions.end()) adminSessions.erase(s); r.headers["Set-Cookie"] = "pyros_admin=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0"; r.body = "{\"ok\":true}"; return; }

				std::map<std::string, sol::protected_function>::iterator h = adminHandlers.find(name);
				if (h == adminHandlers.end()) { fail(404, "no such call"); return; }
				sol::table req = lua.create_table();
				req["method"] = q.method;
				req["name"] = name;
				req["query"] = q.query;
				req["body"] = q.body;
				req["remote"] = q.remote;
				if (!q.body.empty())
				{
					sol::protected_function_result decoded = lua["json"]["decode"](q.body);
					if (decoded.valid()) req["data"] = decoded.get<sol::object>(0);
				}
				sol::protected_function_result out = h->second(req);
				if (!out.valid())
				{
					sol::error e = out;
					echo(std::string("ERROR: admin.on('") + name + "') - " + e.what());
					fail(500, "the script failed; see the server log");
					return;
				}
				if (out.return_count() >= 2 && out.get<sol::object>(1).is<int>()) r.status = out.get<int>(1);
				sol::object value = out.return_count() >= 1 ? out.get<sol::object>(0) : sol::make_object(lua, sol::lua_nil);
				if (value.is<std::string>()) r.body = value.as<std::string>();
				else
				{
					sol::protected_function_result text = lua["json"]["encode"](value);
					r.body = text.valid() ? text.get<std::string>() : std::string("null");
				}
			});
			if (web.Start(adminPort, adminBind))
				echo("PyrosServer: admin page on http://" + adminBind + ":" + std::to_string(adminPort) + "/" + (adminPassword.empty() ? "  (no password: this machine only)" : ""));
			else
				fprintf(stderr, "PyrosServer: could not serve the admin page on %s:%u\n", adminBind.c_str(), (unsigned)adminPort);
		}
	}
#endif

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
		web.Poll();
		// Cells and terrain tiles around every player; the world's streamer
		// pumps the loader both use.
		std::vector<Vec3> viewers = session->ClientViewers();
#ifdef LUA_BINDINGS
		viewers.insert(viewers.end(), keepLoaded.begin(), keepLoaded.end());
#endif
		TerrainComponent::SetViewers(scene, viewers);
		if (world) world->Update(viewers);
		else AssetStreamer::Instance().Pump(4.0);
		physics->Update(dt, 10);
		scene->Update(t);
#ifdef LUA_BINDINGS
		if (mainScript)
		{
			// the clock, not the delta: LuaComponent turns it into the frame's delta itself
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
	web.Stop();
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
	if (g_restart)
	{
		echo("PyrosServer: restarting");
		fflush(stdout); fflush(stderr);
		fs::current_path(startedIn, ec);
#ifdef _WIN32
		_execv(startedAs.string().c_str(), argv);
#else
		execv(startedAs.string().c_str(), argv);
#endif
		fprintf(stderr, "PyrosServer: could not restart itself - start it again by hand\n");
		return 1;
	}
	// Everything that has to be saved or closed has been, above. What is left
	// is the script state's own teardown, and that runs after the render
	// device is gone: a renderable a script was still holding (a sphere it had
	// made) destroyed its buffers through no device at all, and every clean
	// stop of the server ended in a segmentation fault instead of an exit
	// code of 0. There is nothing in it worth running: leave.
	fflush(stdout); fflush(stderr);
	std::_Exit(0);
}
