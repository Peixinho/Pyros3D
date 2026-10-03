// Meeting by name through a rendezvous service (NetRendezvous.h): a host
// announces itself, a client that knows only the service and the name ends
// up in the host's session; a name nobody holds fails quickly; a second
// host cannot take a live name.
//
// Loopback: this proves the protocol - the service's table, the address
// exchange, both ends sending toward each other on the game's own socket.
// It cannot prove a real router lets the packets through; that needs two
// networks (see docs/multiplayer-field-test.md).
//
//   c++ -std=c++17 -DPYROS_NETWORKING -I include -I src/Pyros3D/Ext/box3d/include \
//       tools/tests/net_rendezvous.cpp -o /tmp/net_rendezvous -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Network/NetworkSession.h>
#include <Pyros3D/Network/NetRendezvous.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>
#include <vector>

using namespace p3d;

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
};

static NetRendezvous service;
static bool Run(const std::vector<World*> &worlds, const std::function<bool()> &pred, const double seconds)
{
	for (int i = 0; i < (int)(seconds * 60); i++)
	{
		service.Update();
		for (size_t w = 0; w < worlds.size(); w++) worlds[w]->session.Update(1.0 / 60.0);
		if (pred && pred()) return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
	return pred ? pred() : true;
}

int main()
{
	const std::string scenePath = (std::filesystem::temp_directory_path() / "pyros_net_rendezvous_test.json").string();
	const uint16 servicePort = 47361;
	const std::string at = "127.0.0.1:" + std::to_string(servicePort);
	check(service.Start(servicePort), "the rendezvous service starts");

	World host(scenePath);
	NetworkSettings hs;
	hs.rendezvous = at;
	hs.sessionName = "duarte's game";
	hs.password = "sesame";
	check(host.session.Host(47362, hs), "a host hosts, announcing a name");
	check(Run({ &host }, [&] { return host.session.Transport().IsRegistered(); }, 3.0) && service.HostCount() == 1, "the service knows it");

	// The client is given no address at all - only the service and the name.
	World client(scenePath);
	NetworkSettings cs = hs;
	client.session.Connect("", 0, cs);
	check(Run({ &host, &client }, [&] { return client.session.IsReady(); }, 5.0), "a client that knows only the name joins");
	std::string heard;
	host.session.OnRpc("say", [&](const PeerId, const std::vector<NetValue> &a) { if (!a.empty()) heard = a[0].text; });
	client.session.Rpc(0, "say", { NetValue::FromString("through") });
	check(Run({ &host, &client }, [&] { return heard == "through"; }, 3.0), "and the session works - password, encryption and all");

	// The announcements go on while it plays: still there later.
	Run({ &host, &client }, nullptr, 1.5);
	check(host.session.Transport().IsRegistered() && client.session.IsReady(), "the announcement does not disturb the game");

	// Nobody by that name.
	{
		World lost(scenePath);
		PeerId left = 99;
		lost.session.onPeerLeft = [&](const PeerId p) { left = p; };
		NetworkSettings s = cs;
		s.sessionName = "no such game";
		lost.session.Connect("", 0, s);
		const auto t0 = std::chrono::steady_clock::now();
		const bool failed = Run({ &host, &lost }, [&] { return !lost.session.LastError().empty(); }, 4.0);
		const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		printf("unknown name: '%s' after %.2f s\n", lost.session.LastError().c_str(), took);
		check(failed && lost.session.LastError() == "could not connect" && took < 2.0, "a name nobody holds fails, and quickly");
		lost.session.Shutdown();
	}

	// A service that is not there.
	{
		World lost(scenePath);
		NetworkSettings s = cs;
		s.rendezvous = "127.0.0.1:47369";
		lost.session.Connect("", 0, s);
		check(Run({ &lost }, [&] { return !lost.session.LastError().empty(); }, 7.0), "a service that never answers is a failed connection, not a hang");
		lost.session.Shutdown();
	}

	// The name is the first host's while it lives.
	{
		World other(scenePath);
		check(other.session.Host(47363, hs), "a second host may host under the same name");
		Run({ &host, &other, &client }, nullptr, 2.0);
		check(!other.session.Transport().IsRegistered() && host.session.Transport().IsRegistered(), "but the name stays the first one's");
		World second(scenePath);
		second.session.Connect("", 0, cs);
		check(Run({ &host, &other, &client, &second }, [&] { return second.session.IsReady(); }, 5.0) && host.session.Peers().size() == 2,
			"and whoever asks for it still reaches the first");
		second.session.Shutdown();
		other.session.Shutdown();
	}

	client.session.Shutdown();
	host.session.Shutdown();
	service.Stop();
	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures ? 1 : 0;
}
