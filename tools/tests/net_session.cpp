// NetworkSession: a server and a client in one process, each with its own
// scene, over real loopback UDP. Spawn by relevance, smooth interpolation,
// variables, bound scene objects, RPCs both ways, client-owned objects,
// despawn, a bandwidth budget with priority, and convergence through loss.
// No render device - the replicated objects are bare GameObjects.
//
//   c++ -std=c++17 -DPYROS_NETWORKING -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/net_session.cpp -o /tmp/net_session \
//       -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/net_session
#include <Pyros3D/Network/NetworkSession.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <thread>

using namespace p3d;
namespace fs = std::filesystem;

static int failures = 0;
static void check(bool cond, const std::string &what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what.c_str());
	fflush(stdout);
	if (!cond) failures++;
}

struct World
{
	SceneGraph scene;
	NetworkSession session;
	f64 t = 0.0;
	explicit World(const std::string &scenePath) : session(&scene, scenePath) {}
	void Frame(const f64 dt) { t += dt; session.Update(dt); scene.Update(t); }
};

static f64 g_dt = 1.0 / 60.0;
// Runs both worlds in real time (the transport is real) until pred or timeout.
static bool Run(World &s, World &c, const std::function<bool()> &pred, const f64 seconds, const std::function<void()> &each = nullptr)
{
	const int frames = (int)(seconds / g_dt);
	for (int i = 0; i < frames; i++)
	{
		if (each) each();
		s.Frame(g_dt);
		c.Frame(g_dt);
		if (pred && pred()) return true;
		std::this_thread::sleep_for(std::chrono::microseconds((int)(g_dt * 1e6)));
	}
	return pred ? pred() : true;
}

static std::shared_ptr<GameObject> Named(const std::string &name, bool identity)
{
	std::shared_ptr<GameObject> go = std::make_shared<GameObject>();
	go->SetName(name);
	if (identity) go->AddComponent(std::make_shared<NetworkIdentity>());
	return go;
}

static NetworkIdentity* Id(GameObject* go)
{
	for (auto &c : go->GetComponents()) if (auto* id = dynamic_cast<NetworkIdentity*>(c.get())) return id;
	return NULL;
}

int main()
{
	const fs::path root = fs::temp_directory_path() / "pyros_net_session_test";
	fs::remove_all(root);
	fs::create_directories(root / "scenes");
	fs::create_directories(root / "assets" / "prefabs");
	std::ofstream(root / "assets" / "prefabs" / "Crate.prefab") << "{\"prefabVersion\":1,\"root\":{\"name\":\"Crate\"}}";
	std::ofstream(root / "assets" / "prefabs" / "Player.prefab") << "{\"prefabVersion\":1,\"root\":{\"name\":\"Player\","
		"\"components\":[{\"type\":\"NetworkIdentity\",\"predicted\":true}]}}";
	const std::string scenePath = (root / "scenes" / "Test.json").string();

	World server(scenePath), client(scenePath);
	// A scene-authored object both ends have: bound, not spawned.
	server.scene.Add(Named("Door", true));
	client.scene.Add(Named("Door", true));

	NetworkSettings settings;
	settings.defaultRelevance = 500.f;
	settings.relevanceHysteresis = 50.f;
	const uint16 port = 47311;
	check(server.session.Host(port, settings), "server hosts");
	check(client.session.Connect("127.0.0.1", port, settings), "client connects");
	check(Run(server, client, [&] { return client.session.IsReady(); }, 3.0), "client is welcomed");
	const PeerId me = client.session.LocalPeer();
	check(me != 0, "and given a peer id");

	std::shared_ptr<GameObject> a = server.session.Spawn("assets/prefabs/Crate.prefab", Vec3(10, 0, 0), Vec3(0, 0.5f, 0));
	std::shared_ptr<GameObject> b = server.session.Spawn("assets/prefabs/Crate.prefab", Vec3(2000, 0, 0), Vec3());
	const uint32 aId = Id(a.get())->GetNetId(), bId = Id(b.get())->GetNetId();
	check(Run(server, client, [&] { return client.session.Find(aId) != NULL; }, 3.0), "a nearby spawn reaches the client");
	Run(server, client, nullptr, 0.3);
	check(client.session.Find(bId) == NULL, "one 2 km away does not");
	check(client.session.Find(aId)->GetName() == "Crate", "built from its prefab");

	// Bound door.
	GameObject* serverDoor = NULL;
	for (auto &g : server.scene.GetAllGameObjectList()) if (g->GetName() == "Door") serverDoor = g.get();
	Id(serverDoor)->SetVar("open", NetValue::FromBool(true));
	GameObject* clientDoor = NULL;
	for (auto &g : client.scene.GetAllGameObjectList()) if (g->GetName() == "Door") clientDoor = g.get();
	check(Run(server, client, [&] { NetValue v; return Id(clientDoor)->GetVar("open", v) && v.boolean; }, 3.0),
		"a scene object is bound by path and receives its variables");

	// Motion: A slides along +x at 5 m/s; the replica follows, smoothly,
	// about interpolationDelay behind.
	std::vector<f32> seen;
	f32 worstLag = 0.f;
	Run(server, client, nullptr, 2.0, [&] {
		a->SetPosition(a->GetPosition() + Vec3(5.f * (f32)g_dt, 0, 0));
		GameObject* r = client.session.Find(aId);
		if (r) seen.push_back(r->GetPosition().x);
	});
	bool smooth = true;
	for (size_t i = 60; i + 1 < seen.size(); i++) if (seen[i + 1] < seen[i] - 1e-4f || seen[i + 1] - seen[i] > 0.3f) smooth = false;
	const f32 lag = a->GetPosition().x - client.session.Find(aId)->GetPosition().x;
	printf("server x %.2f, replica x %.2f (lag %.2f m = %.0f ms at 5 m/s)\n", a->GetPosition().x, client.session.Find(aId)->GetPosition().x, lag, lag / 5.f * 1000.f);
	check(smooth, "the replica moves forward every frame, without jumps");
	check(lag > 0.2f && lag < 1.5f, "about the interpolation delay (plus latency) behind");

	Id(a.get())->SetVar("health", NetValue::FromNumber(42));
	check(Run(server, client, [&] { NetValue v; return Id(client.session.Find(aId))->GetVar("health", v) && v.number == 42; }, 3.0),
		"variables arrive");

	// RPCs.
	PeerId rpcFrom = 0; std::string rpcText; f64 rpcNum = 0;
	server.session.OnRpc("hello", [&](const PeerId from, const std::vector<NetValue> &args) {
		rpcFrom = from; if (args.size() == 2) { rpcNum = args[0].number; rpcText = args[1].text; }
	});
	client.session.Rpc(1, "hello", { NetValue::FromNumber(7), NetValue::FromString("hi") });
	check(Run(server, client, [&] { return rpcFrom == me; }, 3.0) && rpcNum == 7 && rpcText == "hi", "client -> server RPC with arguments");
	bool gotBack = false;
	client.session.OnRpc("pong", [&](const PeerId from, const std::vector<NetValue> &) { gotBack = from == 0; });
	server.session.Rpc(0, "pong", {});
	check(Run(server, client, [&] { return gotBack; }, 3.0), "server -> client RPC");

	// A client-owned object: the client moves it, the server follows.
	std::shared_ptr<GameObject> mine = server.session.Spawn("assets/prefabs/Crate.prefab", Vec3(0, 0, 5), Vec3(), me);
	const uint32 mineId = Id(mine.get())->GetNetId();
	check(Run(server, client, [&] { return client.session.Find(mineId) != NULL; }, 3.0), "an owned object reaches its owner");
	client.session.Find(mineId)->SetPosition(Vec3(3, 1, 4));
	check(Run(server, client, [&] { return (mine->GetPosition() - Vec3(3, 1, 4)).magnitude() < 0.01f; }, 3.0), "the owner moves it on the server");

	// Relevance: walk the viewer to B, then away again.
	client.session.SetViewer(Vec3(1900, 0, 0));
	check(Run(server, client, [&] { return client.session.Find(bId) != NULL; }, 3.0), "B spawns when the viewer comes near");
	check(Run(server, client, [&] { return client.session.Find(aId) == NULL; }, 3.0), "and A despawns once the viewer is far");
	client.session.SetViewer(Vec3(0, 0, 0));
	Run(server, client, [&] { return client.session.Find(aId) != NULL; }, 3.0);

	server.session.Destroy(a.get());
	check(Run(server, client, [&] { return client.session.Find(aId) == NULL; }, 3.0), "destroy despawns everywhere");

	// Budget: 400 moving crates nearby, 1500 bytes a tick.
	std::vector<std::shared_ptr<GameObject> > many;
	for (int i = 0; i < 400; i++)
		many.push_back(server.session.Spawn("assets/prefabs/Crate.prefab", Vec3((f32)(i % 20), 0, (f32)(i / 20)), Vec3()));
	Run(server, client, [&] { return client.session.Find(Id(many.back().get())->GetNetId()) != NULL; }, 5.0);
	uint32 maxBytes = 0;
	Run(server, client, nullptr, 2.0, [&] {
		for (size_t i = 0; i < many.size(); i++) many[i]->SetPosition(many[i]->GetPosition() + Vec3(0, 0.01f, 0));
		maxBytes = std::max(maxBytes, server.session.GetStats().lastSnapshotBytes);
	});
	printf("largest snapshot with 400 movers: %u bytes, %u entities\n", maxBytes, server.session.GetStats().lastSnapshotEntities);
	check(maxBytes <= settings.bytesPerTick, "snapshots stay inside the byte budget");
	// Everyone gets updated eventually, not just the first few.
	Run(server, client, nullptr, 1.5);
	int stale = 0;
	for (size_t i = 0; i < many.size(); i++)
	{
		GameObject* r = client.session.Find(Id(many[i].get())->GetNetId());
		if (!r || std::fabs(r->GetPosition().y - many[i]->GetPosition().y) > 0.05f) stale++;
	}
	check(stale == 0, "priority rotation reaches every object (" + std::to_string(stale) + " stale)");

	// Loss: 25% of snapshots dropped; a variable still converges.
	server.session.Transport().SetSimulatedConditions(30, 10, 0.25f);
	Id(many[5].get())->SetVar("team", NetValue::FromString("red"));
	check(Run(server, client, [&] { NetValue v; return Id(client.session.Find(Id(many[5].get())->GetNetId()))->GetVar("team", v) && v.text == "red"; }, 5.0),
		"a variable converges through 25% snapshot loss");

	// ---- prediction ----
	server.session.Transport().SetSimulatedConditions(0, 0, 0.f);
	for (size_t i = 0; i < many.size(); i++) server.session.Destroy(many[i].get());
	// The game's movement code, run by both ends: 5 m/s along the input.
	const NetworkSession::Simulate walk = [](GameObject* go, const std::vector<NetValue> &in, const f32 dt) {
		if (in.size() < 2) return;
		go->SetPosition(go->GetPosition() + Vec3((f32)in[0].number, 0.f, (f32)in[1].number) * (5.f * dt));
	};
	server.session.SetSimulate(walk);
	client.session.SetSimulate(walk);
	std::shared_ptr<GameObject> hero = server.session.Spawn("assets/prefabs/Player.prefab", Vec3(0, 0, 0), Vec3(), me);
	const uint32 heroId = Id(hero.get())->GetNetId();
	check(Run(server, client, [&] { return client.session.Find(heroId) != NULL; }, 3.0), "a predicted player reaches its owner");
	GameObject* local = client.session.Find(heroId);
	check(Id(local)->predicted, "flagged predicted by its prefab");
	server.session.Transport().SetSimulatedConditions(60, 0, 0.f);
	client.session.Transport().SetSimulatedConditions(60, 0, 0.f);
	Run(server, client, nullptr, 0.3);

	client.session.SetInput({ NetValue::FromNumber(1), NetValue::FromNumber(0) });
	Run(server, client, nullptr, 0.1);
	printf("after 0.1 s: owner x %.3f, server x %.3f\n", local->GetPosition().x, hero->GetPosition().x);
	check(local->GetPosition().x > hero->GetPosition().x + 0.3f, "the owner moves at once, well ahead of what the server has heard");
	Run(server, client, nullptr, 0.9);
	client.session.SetInput({ NetValue::FromNumber(0), NetValue::FromNumber(0) });
	Run(server, client, nullptr, 1.0);
	printf("stopped: owner x %.4f, server x %.4f\n", local->GetPosition().x, hero->GetPosition().x);
	check(std::fabs(local->GetPosition().x - hero->GetPosition().x) < 0.002f, "after reconciling, owner and server agree");
	check(std::fabs(hero->GetPosition().x - 5.f) < 0.3f, "and moved 5 m in the second of input");

	// A client making commands twice as fast (a sped-up clock) cannot move
	// the server faster, and is pulled back to it.
	const f32 start = hero->GetPosition().x;
	client.session.SetInput({ NetValue::FromNumber(1), NetValue::FromNumber(0) });
	for (int i = 0; i < 60; i++)
	{
		server.Frame(g_dt);
		client.Frame(g_dt * 2.0);
		std::this_thread::sleep_for(std::chrono::microseconds((int)(g_dt * 1e6)));
	}
	client.session.SetInput({ NetValue::FromNumber(0), NetValue::FromNumber(0) });
	Run(server, client, nullptr, 1.0);
	const f32 moved = hero->GetPosition().x - start;
	printf("sped-up client: server moved %.2f m (honest would be 5.0, cheating 10.0), owner at %.2f\n", moved, local->GetPosition().x - start);
	check(moved < 7.5f, "the server caps a client's command rate");
	check(std::fabs(local->GetPosition().x - hero->GetPosition().x) < 0.002f, "and the cheater is reconciled back to it");

	// ---- lag compensation ----
	std::shared_ptr<GameObject> target = server.session.Spawn("assets/prefabs/Crate.prefab", Vec3(0, 0, 20), Vec3());
	const uint32 targetId = Id(target.get())->GetNetId();
	Run(server, client, [&] { return client.session.Find(targetId) != NULL; }, 3.0);
	// Keep the client's view tick flowing: it rides on its commands.
	client.session.SetInput({ NetValue::FromNumber(0), NetValue::FromNumber(0) });
	Run(server, client, nullptr, 1.0, [&] { target->SetPosition(target->GetPosition() + Vec3(10.f * (f32)g_dt, 0, 0)); });
	// The client fires: it sends where it aimed and the tick it was seeing.
	const Vec3 sighted = client.session.Find(targetId)->GetPosition();
	const f64 firedAt = client.session.ViewTick();
	const Vec3 origin = sighted + Vec3(0.f, 0.9f, -10.f);
	const NetworkSession::RayHit rewound = server.session.RaycastRewound(me, origin, Vec3(0, 0, 1), 50.f, firedAt);
	const NetworkSession::RayHit present = server.session.RaycastRewound(0, origin, Vec3(0, 0, 1), 50.f);
	printf("target on the client at x %.2f, on the server at x %.2f\n", sighted.x, target->GetPosition().x);
	check(rewound.netId == targetId && std::fabs(rewound.distance - (10.f - 0.4f)) < 0.2f, "a shot where the client saw the target hits, rewound");
	check(present.netId == 0, "the same shot against the present misses");

	client.session.Shutdown();
	check(Run(server, client, [&] { return server.session.Transport().PeerCount() == 0; }, 3.0), "the server sees the client leave");
	server.session.Shutdown();
	fs::remove_all(root);
	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures ? 1 : 0;
}
