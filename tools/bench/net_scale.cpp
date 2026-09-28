// A 100-player server's cost: one NetworkSession hosting, 100 client
// sessions joined over loopback (real ENet sockets), 100 players the
// server moves every frame. Measures the server's Update alone - what a
// dedicated server spends per frame - and what it sends each client.
//
// Two layouts: everyone within 200 m (every player relevant to every
// client: the worst case, an endgame circle) and spread over an 8 x 8 km
// map (relevance 500 m does its job).
//
//   c++ -std=c++17 -O2 -DPYROS_NETWORKING -I include -I src/Pyros3D/Ext/box3d/include \
//       tools/bench/net_scale.cpp -o /tmp/net_scale -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/net_scale [players=100] [seconds=10]

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Network/NetworkSession.h>
#include <Pyros3D/Network/NetworkIdentity.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace p3d;
namespace fs = std::filesystem;

struct Client
{
	SceneGraph scene;
	NetworkSession session;
	explicit Client(const std::string &scenePath) : session(&scene, scenePath) {}
};

static void Run(const char* label, const int players, const double seconds, const float spread, const std::string &scenePath, const uint16 port)
{
	SceneGraph serverScene;
	NetworkSession server(&serverScene, scenePath);
	NetworkSettings settings;
	settings.maxClients = (uint32)players + 4;
	settings.defaultRelevance = 500.f;
	if (!server.Host(port, settings)) { printf("could not host\n"); return; }

	std::vector<std::unique_ptr<Client> > clients;
	for (int i = 0; i < players; i++)
	{
		clients.emplace_back(new Client(scenePath));
		clients.back()->session.Connect("127.0.0.1", port, settings);
	}
	const double dt = 1.0 / 60.0;
	auto frameAll = [&]() {
		server.Update(dt);
		serverScene.Update(0.0);
		for (size_t i = 0; i < clients.size(); i++) { clients[i]->session.Update(dt); clients[i]->scene.Update(0.0); }
	};
	for (int f = 0; f < 600; f++)
	{
		frameAll();
		bool all = true;
		for (size_t i = 0; i < clients.size() && all; i++) all = clients[i]->session.IsReady();
		if (all) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	int ready = 0;
	for (size_t i = 0; i < clients.size(); i++) if (clients[i]->session.IsReady()) ready++;

	// One player per client, placed by the layout, each walking its own way.
	uint32 rng = 7u;
	auto rnd = [&rng]() { rng = rng * 1664525u + 1013904223u; return (float)(rng >> 8) / 16777216.f; };
	std::vector<std::shared_ptr<GameObject> > bodies;
	std::vector<Vec3> heading;
	for (int i = 0; i < players; i++)
	{
		const Vec3 at = spread > 0.f ? Vec3(rnd() * spread, 0.f, rnd() * spread) : Vec3((rnd() - 0.5f) * 200.f, 0.f, (rnd() - 0.5f) * 200.f);
		// Server-owned: the server moves them and every client that can see
		// one receives it - a server-authoritative game.
		bodies.push_back(server.Spawn("assets/prefabs/Player.prefab", at, Vec3(), 0));
		const float a = rnd() * 6.2831853f;
		heading.push_back(Vec3(std::cos(a), 0.f, std::sin(a)) * 5.f);	// 5 m/s
	}

	std::vector<double> serverMs;
	uint64 sentBefore = 0;
	std::vector<PeerId> peers;
	for (size_t i = 0; i < clients.size(); i++) peers.push_back(clients[i]->session.LocalPeer());
	auto totalSent = [&]() {
		uint64 n = 0;
		for (size_t i = 0; i < peers.size(); i++) { NetPeerStats st; if (server.Transport().GetStats(peers[i], st)) n += st.bytesSent; }
		return n;
	};
	// Settle a second (initial spawns), then measure.
	uint32 largest = 0;
	const int warm = 60, frames = (int)(seconds * 60.0);
	for (int f = 0; f < warm + frames; f++)
	{
		if (f == warm) { sentBefore = totalSent(); serverMs.clear(); largest = 0; }
		for (size_t i = 0; i < bodies.size(); i++)
		{
			bodies[i]->SetPosition(bodies[i]->GetPosition() + heading[i] * (float)dt);
			// Each client watches from its own player.
			clients[i]->session.SetViewer(bodies[i]->GetPosition());
		}
		const auto t0 = std::chrono::steady_clock::now();
		server.Update(dt);
		serverScene.Update(0.0);
		serverMs.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
		largest = std::max(largest, server.GetStats().lastSnapshotBytes);
		for (size_t i = 0; i < clients.size(); i++) { clients[i]->session.Update(dt); clients[i]->scene.Update(0.0); }
	}
	const double sent = (double)(totalSent() - sentBefore);
	std::sort(serverMs.begin(), serverMs.end());
	double avg = 0; for (size_t i = 0; i < serverMs.size(); i++) avg += serverMs[i]; avg /= serverMs.size();
	uint32 seen = 0;
	for (size_t i = 0; i < clients.size(); i++) seen += clients[i]->session.GetStats().replicated;
	printf("%-9s %d/%d clients joined | server Update: avg %.3f ms, p99 %.3f ms, max %.3f ms per frame"
		" | %.1f KB/s per client (%.0f kbit/s), largest snapshot %u B | each client knows %.1f players\n",
		label, ready, players, avg, serverMs[serverMs.size() * 99 / 100], serverMs.back(),
		sent / seconds / players / 1024.0, sent * 8.0 / seconds / players / 1000.0, largest, (double)seen / players);

	for (size_t i = 0; i < clients.size(); i++) clients[i]->session.Shutdown();
	server.Shutdown();
}

int main(int argc, char** argv)
{
	const int players = argc > 1 ? std::atoi(argv[1]) : 100;
	const double seconds = argc > 2 ? std::atof(argv[2]) : 10.0;
	const fs::path root = fs::temp_directory_path() / "pyros_net_scale";
	fs::remove_all(root);
	fs::create_directories(root / "scenes");
	fs::create_directories(root / "assets" / "prefabs");
	std::ofstream(root / "assets" / "prefabs" / "Player.prefab") << "{\"prefabVersion\":1,\"root\":{\"name\":\"Player\","
		"\"components\":[{\"type\":\"NetworkIdentity\"}]}}";
	const std::string scenePath = (root / "scenes" / "Test.json").string();
	Run("clustered", players, seconds, 0.f, scenePath, 47501);
	Run("8 km", players, seconds, 8192.f, scenePath, 47502);
	fs::remove_all(root);
	return 0;
}
