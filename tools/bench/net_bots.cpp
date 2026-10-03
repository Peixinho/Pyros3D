// Players for a real server: N client sessions in one process that join a
// running PyrosServer (password, pinned key, by address or by rendezvous
// name) and each walk the object the server spawned for them. What
// net_scale does inside one process, against the built game instead -
// read the server's own cost from `PyrosServer --stats`.
//
//   c++ -std=c++17 -O2 -DPYROS_NETWORKING -I include -I src/Pyros3D/Ext/box3d/include \
//       tools/bench/net_bots.cpp -o /tmp/net_bots -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/net_bots --game <built game folder> --connect 127.0.0.1:47400 [--count 100] [--seconds 30]
//                 [--speed 5] [--password p] [--server-key hex] [--rendezvous host:port --session name]
//
// The server's game is expected to spawn each joining peer an object it
// owns (network.spawn(prefab, position, rotation, peer) in onPeerJoined).

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Network/NetworkSession.h>
#include <Pyros3D/Utils/Json/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace p3d;
namespace fs = std::filesystem;

struct Bot
{
	SceneGraph scene;
	NetworkSession session;
	GameObject* body = NULL;
	Vec3 heading;
	std::string refused;
	explicit Bot(const std::string &scenePath) : session(&scene, scenePath) {}
};

static std::string Arg(int argc, char** argv, const char* name, const std::string &fallback)
{
	for (int i = 1; i + 1 < argc; i++) if (std::string(argv[i]) == name) return argv[i + 1];
	return fallback;
}

int main(int argc, char** argv)
{
	const fs::path game = Arg(argc, argv, "--game", ".");
	std::string connect = Arg(argc, argv, "--connect", "127.0.0.1:47400");
	const int count = std::atoi(Arg(argc, argv, "--count", "100").c_str());
	const double seconds = std::atof(Arg(argc, argv, "--seconds", "30").c_str());
	const float speed = (float)std::atof(Arg(argc, argv, "--speed", "5").c_str());

	nlohmann::json manifest;
	{
		std::ifstream in((game / "game.json").string().c_str());
		if (!in) { fprintf(stderr, "net_bots: no game.json in %s\n", game.string().c_str()); return 1; }
		manifest = nlohmann::json::parse(in, NULL, false);
	}
	const std::string scenePath = (game / manifest.value("startupScene", std::string())).string();

	NetworkSettings settings;
	settings.password = Arg(argc, argv, "--password", "");
	settings.serverPublicKey = Arg(argc, argv, "--server-key", manifest.value("serverPublicKey", std::string()));
	settings.rendezvous = Arg(argc, argv, "--rendezvous", "");
	settings.sessionName = Arg(argc, argv, "--session", "");
	int port = 47400;
	const size_t colon = connect.rfind(':');
	if (colon != std::string::npos) { port = std::atoi(connect.c_str() + colon + 1); connect = connect.substr(0, colon); }

	std::vector<std::unique_ptr<Bot> > bots;
	uint32 rng = 11u;
	auto rnd = [&rng]() { rng = rng * 1664525u + 1013904223u; return (float)(rng >> 8) / 16777216.f; };
	for (int i = 0; i < count; i++)
	{
		bots.emplace_back(new Bot(scenePath));
		Bot* b = bots.back().get();
		b->session.onRejected = [b](const std::string &why) { b->refused = why; };
		const float a = rnd() * 6.2831853f;
		b->heading = Vec3(std::cos(a), 0.f, std::sin(a)) * speed;
		if (!b->session.Connect(connect, (uint16)port, settings)) b->refused = "could not start";
	}

	const double dt = 1.0 / 60.0;
	const auto start = std::chrono::steady_clock::now();
	auto elapsed = [&]() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(); };
	double nextReport = 5.0, joinedAt = -1.0, worstFrame = 0.0;
	std::vector<uint64> receivedAtJoin(bots.size(), 0);
	while (elapsed() < seconds)
	{
		const auto f0 = std::chrono::steady_clock::now();
		int ready = 0, owning = 0;
		for (size_t i = 0; i < bots.size(); i++)
		{
			Bot &b = *bots[i];
			b.session.Update(dt);
			b.scene.Update(0.0);
			if (!b.session.IsReady()) { b.body = NULL; continue; }
			ready++;
			if (!b.body)
			{
				const std::vector<NetworkSession::EntityInfo> all = b.session.Entities();
				for (size_t e = 0; e < all.size(); e++) if (all[e].owner == b.session.LocalPeer() && all[e].object) { b.body = all[e].object; break; }
			}
			if (b.body)
			{
				owning++;
				b.body->SetPosition(b.body->GetPosition() + b.heading * (float)dt);
				b.session.SetViewer(b.body->GetPosition());
			}
		}
		if (joinedAt < 0.0 && ready == count)
		{
			joinedAt = elapsed();
			for (size_t i = 0; i < bots.size(); i++)
			{
				NetPeerStats st;
				if (bots[i]->session.Transport().GetStats(1, st)) receivedAtJoin[i] = st.bytesReceived;
			}
			printf("all %d joined after %.1f s\n", count, joinedAt);
			fflush(stdout);
		}
		if (elapsed() >= nextReport)
		{
			nextReport += 5.0;
			double rtt = 0, known = 0;
			for (size_t i = 0; i < bots.size(); i++)
			{
				NetPeerStats st;
				if (bots[i]->session.Transport().GetStats(1, st)) rtt += st.roundTripMs;
				known += bots[i]->session.GetStats().replicated;
			}
			printf("%5.0f s: %d/%d joined, %d walking | avg round trip %.1f ms | each knows %.1f objects | bots' worst frame %.1f ms\n",
				elapsed(), ready, count, owning, ready ? rtt / ready : 0.0, ready ? known / ready : 0.0, worstFrame);
			fflush(stdout);
			worstFrame = 0.0;
		}
		const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - f0).count();
		worstFrame = std::max(worstFrame, took * 1000.0);
		if (took < dt) std::this_thread::sleep_for(std::chrono::duration<double>(dt - took));
	}

	int ready = 0, refused = 0;
	double received = 0;
	std::string why;
	for (size_t i = 0; i < bots.size(); i++)
	{
		if (bots[i]->session.IsReady()) ready++;
		if (!bots[i]->refused.empty()) { refused++; why = bots[i]->refused; }
		NetPeerStats st;
		if (bots[i]->session.Transport().GetStats(1, st)) received += (double)(st.bytesReceived - receivedAtJoin[i]);
	}
	const double measured = joinedAt >= 0.0 ? seconds - joinedAt : seconds;
	printf("end: %d/%d still joined, %d refused%s%s | %.0f kbit/s down per bot\n", ready, count, refused,
		why.empty() ? "" : " - ", why.c_str(), ready ? received * 8.0 / measured / ready / 1000.0 : 0.0);
	for (size_t i = 0; i < bots.size(); i++) bots[i]->session.Shutdown();
	return ready == count ? 0 : 1;
}
