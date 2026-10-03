// The session's encryption. Two halves:
//
//   the cipher on its own - both ends arriving at one key, a message
//   surviving the trip, and every way a message must NOT open: altered,
//   played back, too old, from the wrong end, under another server's key;
//
//   a session over real sockets - a pinned server key accepted, a wrong
//   one and a missing one refused - and what an eavesdropper would read
//   off the wire: not the password, and not what the game says.
//
//   c++ -std=c++17 -DPYROS_NETWORKING -I include -I src/Pyros3D/Network -I src/Pyros3D/Ext/box3d/include \
//       tools/tests/net_crypto.cpp -o /tmp/net_crypto -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk

#include "NetCrypto.h"
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Network/NetworkSession.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
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

static bool Run(const std::vector<World*> &worlds, const std::function<bool()> &pred, const double seconds)
{
	for (int i = 0; i < (int)(seconds * 60); i++)
	{
		for (size_t w = 0; w < worlds.size(); w++) worlds[w]->session.Update(1.0 / 60.0);
		if (pred && pred()) return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
	return pred ? pred() : true;
}

static bool Contains(const std::vector<uchar> &hay, const std::string &needle)
{
	return std::search(hay.begin(), hay.end(), needle.begin(), needle.end()) != hay.end();
}

int main()
{
	// ---- the cipher ----
	{
		const NetKeyPair c = NetKeyPair::Generate(), s = NetKeyPair::Generate(), longTerm = NetKeyPair::Generate();
		check(c.Valid() && s.Valid() && std::memcmp(c.secret, s.secret, 32) != 0, "fresh key pairs are keys, and differ");
		NetKeyPair back;
		check(NetKeyPair::FromSecretHex(longTerm.SecretHex(), back) && back.PublicHex() == longTerm.PublicHex(), "a key survives being written as hex");
		check(!NetKeyPair::FromSecretHex("not a key", back), "nonsense is not a key");

		uint8 staticC[32], staticS[32];
		NetSharedSecret(c.secret, longTerm.pub, staticC);
		NetSharedSecret(longTerm.secret, c.pub, staticS);
		NetCipher client, server;
		client.Establish(c.secret, s.pub, staticC, c.pub, s.pub, false);
		server.Establish(s.secret, c.pub, staticS, c.pub, s.pub, true);

		const std::string text = "the quick brown fox";
		std::vector<uchar> sealed, opened;
		client.Seal((const uchar*)text.data(), text.size(), sealed);
		check(sealed.size() == text.size() + NetCipher::Overhead && !Contains(sealed, "quick"), "a sealed message is 24 bytes longer and unreadable");
		check(server.Open(sealed.data(), sealed.size(), opened) && std::string(opened.begin(), opened.end()) == text, "the other end opens it");
		check(!server.Open(sealed.data(), sealed.size(), opened), "played back, it does not open twice");

		std::vector<uchar> altered;
		client.Seal((const uchar*)text.data(), text.size(), altered);
		altered[altered.size() - 3] ^= 1;
		check(!server.Open(altered.data(), altered.size(), opened), "altered by one bit, it does not open");
		altered[altered.size() - 3] ^= 1;
		check(server.Open(altered.data(), altered.size(), opened), "put back, it does - a forgery did not move the replay window");

		std::vector<uchar> own;
		client.Seal((const uchar*)text.data(), text.size(), own);
		check(!client.Open(own.data(), own.size(), opened), "an end cannot be fed its own message");

		// Unreliable delivery: out of order within the window is fine, far
		// too late is not.
		std::vector<std::vector<uchar> > many(80);
		for (size_t i = 0; i < many.size(); i++) client.Seal((const uchar*)text.data(), text.size(), many[i]);
		check(server.Open(many[5].data(), many[5].size(), opened) && server.Open(many[3].data(), many[3].size(), opened), "messages overtaking each other open");
		check(server.Open(many[79].data(), many[79].size(), opened), "so does the newest");
		check(!server.Open(many[2].data(), many[2].size(), opened), "one more than 64 behind the newest does not");
		check(server.Open(many[40].data(), many[40].size(), opened) && !server.Open(many[40].data(), many[40].size(), opened), "one inside the window opens, once");

		// A server without the long-term secret arrives at another key.
		const NetKeyPair impostor = NetKeyPair::Generate();
		uint8 staticBad[32];
		NetSharedSecret(impostor.secret, c.pub, staticBad);
		NetCipher wrong;
		wrong.Establish(s.secret, c.pub, staticBad, c.pub, s.pub, true);
		std::vector<uchar> fresh;
		client.Seal((const uchar*)text.data(), text.size(), fresh);
		check(!wrong.Open(fresh.data(), fresh.size(), opened), "without the long-term secret the session key is a different one");
	}

	// ---- a session ----
	const std::string scenePath = (std::filesystem::temp_directory_path() / "pyros_net_crypto_test.json").string();
	const std::string secret = NetworkSession::GenerateSecretKey();
	const std::string pub = NetworkSession::PublicKeyOf(secret);
	check(secret.size() == 64 && pub.size() == 64 && secret != pub, "a server key is 64 hex characters, with a public half");
	const uint16 port = 47341;
	{
		World server(scenePath);
		NetworkSettings host;
		host.password = "open-sesame-password";
		host.serverSecretKey = secret;
		check(server.session.Host(port, host), "a server hosts with a long-term key and a password");
		std::string heard;
		server.session.OnRpc("say", [&](const PeerId, const std::vector<NetValue> &args) { if (!args.empty()) heard = args[0].text; });

		// What crosses the wire, both ways, as the client's transport sees it.
		World client(scenePath);
		std::vector<uchar> wire;
		client.session.Transport().SetTap([&](const PeerId, const uchar* data, const size_t n, const bool) { wire.insert(wire.end(), data, data + n); });
		NetworkSettings join;
		join.password = "open-sesame-password";
		join.serverPublicKey = pub;
		client.session.Connect("127.0.0.1", port, join);
		check(Run({ &server, &client }, [&] { return client.session.IsReady(); }, 3.0), "a client pinning the right key joins");
		client.session.Rpc(0, "say", { NetValue::FromString("MARKER-the-eagle-has-landed") });
		server.session.Rpc(0, "back", { NetValue::FromString("MARKER-from-the-server") });
		check(Run({ &server, &client }, [&] { return heard == "MARKER-the-eagle-has-landed"; }, 3.0), "what it says arrives");
		Run({ &server, &client }, nullptr, 0.3);
		check(wire.size() > 100, "and there was traffic to look at");
		check(!Contains(wire, "open-sesame-password"), "the password is not on the wire");
		check(!Contains(wire, "MARKER-the-eagle") && !Contains(wire, "MARKER-from-the-server") && !Contains(wire, "say"), "nor is what either end said");

		World wrongPin(scenePath);
		std::string reason;
		wrongPin.session.onRejected = [&](const std::string &r) { reason = r; };
		NetworkSettings bad = join;
		bad.serverPublicKey = NetworkSession::PublicKeyOf(NetworkSession::GenerateSecretKey());
		wrongPin.session.Connect("127.0.0.1", port, bad);
		check(Run({ &server, &wrongPin }, [&] { return !reason.empty(); }, 3.0) && reason == "server key mismatch" && !wrongPin.session.IsReady(),
			"a client pinning another key refuses the server");
		client.session.Shutdown();
		server.session.Shutdown();
	}
	{
		// A server with no long-term key: fine for a client that pins
		// nothing, refused by one that does.
		World server(scenePath);
		check(server.session.Host(port + 1), "a server hosts without a long-term key");
		World plain(scenePath), pinned(scenePath);
		plain.session.Connect("127.0.0.1", port + 1);
		check(Run({ &server, &plain }, [&] { return plain.session.IsReady(); }, 3.0), "an unpinned client joins it");
		std::string reason;
		pinned.session.onRejected = [&](const std::string &r) { reason = r; };
		NetworkSettings pin;
		pin.serverPublicKey = pub;
		pinned.session.Connect("127.0.0.1", port + 1, pin);
		check(Run({ &server, &pinned }, [&] { return !reason.empty(); }, 3.0) && reason == "server key mismatch", "a pinning client does not");
		plain.session.Shutdown();
		server.session.Shutdown();
	}

	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures ? 1 : 0;
}
