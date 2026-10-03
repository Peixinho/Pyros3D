// Joining a session and what can go wrong on the way in: the hello, a
// password, being kicked or banned, dropping and coming back within the
// server's grace (and owning again what was owned), a grace that runs
// out, and a client trying to move its object faster than it may.
// Loopback, real sockets, real time.
//
//   c++ -std=c++17 -DPYROS_NETWORKING -I include -I src/Pyros3D/Ext/box3d/include \
//       tools/tests/net_join.cpp -o /tmp/net_join -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Network/NetworkSession.h>
#include <Pyros3D/Network/NetworkIdentity.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

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
	explicit World(const std::string &scenePath) : session(&scene, scenePath) {}
	void Frame(const f64 dt) { session.Update(dt); scene.Update(0.0); }
};

static const f64 kDt = 1.0 / 60.0;
// Runs every world in real time until pred, or the time is up.
static bool Run(const std::vector<World*> &worlds, const std::function<bool()> &pred, const f64 seconds)
{
	const int frames = (int)(seconds / kDt);
	for (int i = 0; i < frames; i++)
	{
		for (size_t w = 0; w < worlds.size(); w++) worlds[w]->Frame(kDt);
		if (pred && pred()) return true;
		std::this_thread::sleep_for(std::chrono::microseconds((int)(kDt * 1e6)));
	}
	return pred ? pred() : true;
}

static NetworkIdentity* Id(GameObject* go)
{
	if (!go) return NULL;
	for (auto &c : go->GetComponents()) if (auto* id = dynamic_cast<NetworkIdentity*>(c.get())) return id;
	return NULL;
}

int main()
{
	const fs::path root = fs::temp_directory_path() / "pyros_net_join_test";
	fs::remove_all(root);
	fs::create_directories(root / "scenes");
	fs::create_directories(root / "assets" / "prefabs");
	std::ofstream(root / "assets" / "prefabs" / "Player.prefab") << "{\"prefabVersion\":1,\"root\":{\"name\":\"Player\","
		"\"components\":[{\"type\":\"NetworkIdentity\"}]}}";
	const std::string scenePath = (root / "scenes" / "Test.json").string();
	const uint16 port = 47331;

	World server(scenePath);
	NetworkSettings host;
	host.password = "sesame";
	host.reconnectGrace = 2.f;
	host.maxClientSpeed = 10.f;
	std::vector<PeerId> joined, left, dropped;
	PeerId rejoinedNew = 0, rejoinedOld = 0;
	server.session.onPeerJoined = [&](const PeerId p) { joined.push_back(p); };
	server.session.onPeerLeft = [&](const PeerId p) { left.push_back(p); };
	server.session.onPeerDropped = [&](const PeerId p) { dropped.push_back(p); };
	server.session.onPeerRejoined = [&](const PeerId now, const PeerId was) { rejoinedNew = now; rejoinedOld = was; };
	check(server.session.Host(port, host), "server hosts, with a password");

	// ---- password ----
	{
		World wrong(scenePath);
		std::string reason;
		wrong.session.onRejected = [&](const std::string &r) { reason = r; };
		NetworkSettings s;
		s.password = "guess";
		wrong.session.Connect("127.0.0.1", port, s);
		check(Run({ &server, &wrong }, [&] { return !reason.empty(); }, 3.0) && reason == "wrong password", "a wrong password is refused, and told why");
		check(!wrong.session.IsReady() && joined.empty(), "it never became a player");
		check(wrong.session.LastError() == "wrong password", "LastError says the same");
	}

	NetworkSettings join;
	join.password = "sesame";
	join.autoReconnect = true;
	join.reconnectGrace = 2.f;

	// ---- kick ----
	{
		World c(scenePath);
		std::string reason;
		c.session.onRejected = [&](const std::string &r) { reason = r; };
		c.session.Connect("127.0.0.1", port, join);
		check(Run({ &server, &c }, [&] { return c.session.IsReady(); }, 3.0) && joined.size() == 1, "the right password joins");
		const PeerId p = c.session.LocalPeer();
		server.session.Kick(p, "be nice");
		check(Run({ &server, &c }, [&] { return !reason.empty(); }, 3.0) && reason == "be nice", "a kicked client is told the reason");
		check(Run({ &server, &c }, [&] { return left.size() == 1; }, 3.0) && left[0] == p && dropped.empty(), "a kick is a leave, not a drop to wait for");
		Run({ &server, &c }, nullptr, 1.5);
		check(!c.session.IsReconnecting() && !c.session.IsReady(), "and it does not try to come back");
	}

	// ---- ban ----
	{
		World c(scenePath);
		c.session.Connect("127.0.0.1", port, join);
		Run({ &server, &c }, [&] { return c.session.IsReady(); }, 3.0);
		check(server.session.PeerAddress(c.session.LocalPeer()) == "127.0.0.1", "the server knows a peer's address");
		server.session.Ban(c.session.LocalPeer());
		Run({ &server, &c }, [&] { return !c.session.IsReady(); }, 3.0);
		check(server.session.Bans().count("127.0.0.1") == 1, "a ban is recorded against the address");
		World again(scenePath);
		std::string reason;
		again.session.onRejected = [&](const std::string &r) { reason = r; };
		again.session.Connect("127.0.0.1", port, join);
		check(Run({ &server, &again }, [&] { return !reason.empty(); }, 3.0) && reason == "banned", "that address is refused from then on");
		server.session.Unban("127.0.0.1");
	}
	joined.clear(); left.clear(); dropped.clear();

	// ---- drop and come back ----
	{
		World c(scenePath);
		c.session.Connect("127.0.0.1", port, join);
		check(Run({ &server, &c }, [&] { return c.session.IsReady(); }, 3.0), "unbanned, it joins again");
		const PeerId first = c.session.LocalPeer();
		std::shared_ptr<GameObject> hero = server.session.Spawn("assets/prefabs/Player.prefab", Vec3(5, 0, 0), Vec3(), first);
		const uint32 heroId = Id(hero.get())->GetNetId();
		check(Run({ &server, &c }, [&] { return c.session.Find(heroId) != NULL; }, 3.0), "its player is spawned on it");

		// The connection goes; neither side asked for it.
		c.session.Transport().Disconnect(1);
		check(Run({ &server, &c }, [&] { return dropped.size() == 1; }, 3.0) && dropped[0] == first && left.empty(),
			"the server sees a drop, not a leave");
		check(server.session.Find(heroId) == hero.get() && Id(hero.get())->GetOwnerPeer() == first, "and keeps the player, still its old owner's");
		check(Run({ &server, &c }, [&] { return c.session.IsReady(); }, 4.0), "the client reconnects by itself");
		const PeerId second = c.session.LocalPeer();
		check(rejoinedNew == second && rejoinedOld == first && second != first, "the server recognises it: rejoined, under a new peer id");
		check(Id(hero.get())->GetOwnerPeer() == second, "what it owned is its again");
		check(Run({ &server, &c }, [&] { GameObject* g = c.session.Find(heroId); return g && Id(g) && Id(g)->GetOwnerPeer() == second; }, 3.0),
			"and it is told the world afresh, its player its own");
		check(left.empty(), "nobody was reported as having left");

		// ---- the speed limit, on that client-moved player ----
		GameObject* mine = c.session.Find(heroId);
		const Vec3 start = hero->GetPosition();
		// An honest walk: 5 m/s for a second.
		for (int i = 0; i < 60; i++)
		{
			mine->SetPosition(mine->GetPosition() + Vec3(5.f * (f32)kDt, 0, 0));
			Run({ &server, &c }, nullptr, kDt);
		}
		Run({ &server, &c }, nullptr, 0.3);
		check(hero->GetPosition().x - start.x > 4.f && server.session.GetStats().rejectedMoves == 0, "an honest walk is applied on the server");
		// A teleport: 500 m in one step.
		const f32 before = hero->GetPosition().x;
		PeerId suspect = 0;
		server.session.onSuspicious = [&](const PeerId p, const uint32) { suspect = p; };
		mine->SetPosition(mine->GetPosition() + Vec3(500.f, 0, 0));
		Run({ &server, &c }, nullptr, 0.5);
		printf("teleported 500 m: the server moved it %.1f m, refused %u move(s)\n", hero->GetPosition().x - before, server.session.GetStats().rejectedMoves);
		check(hero->GetPosition().x - before < 20.f, "a teleport is not applied");
		check(server.session.GetStats().rejectedMoves > 0 && suspect == second, "and is reported");

		// ---- a grace that runs out ----
		dropped.clear();
		c.session.Shutdown();	// gone for good: nothing will reconnect
		check(Run({ &server }, [&] { return dropped.size() == 1; }, 3.0) && left.empty(), "a client that goes is first a drop");
		check(Run({ &server }, [&] { return left.size() == 1; }, 4.0) && left[0] == second, "and a leave once its grace has run out");
	}

	server.session.Shutdown();
	fs::remove_all(root);
	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures ? 1 : 0;
}
