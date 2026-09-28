// NetTransport over loopback (a server and a client in one process) and
// NetBuffer round trips. Simulated latency and loss check that the reliable
// channel stays complete and ordered while the unreliable one may not.
//
//   c++ -std=c++17 -DPYROS_NETWORKING -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/net_transport.cpp -o /tmp/net_transport \
//       -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/net_transport
#include <Pyros3D/Network/NetTransport.h>
#include <Pyros3D/Network/NetBuffer.h>

#include <chrono>
#include <cmath>
#include <cstdio>
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

// Pumps both ends until pred() or timeout.
template <class F>
static bool pump(NetTransport &a, NetTransport &b, std::vector<NetEvent> &ea, std::vector<NetEvent> &eb, F pred, int ms = 3000)
{
	for (int i = 0; i < ms / 2; i++)
	{
		a.Poll(ea); b.Poll(eb);
		if (pred()) return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	return pred();
}

int main()
{
	// ---- NetBuffer ----
	{
		NetWriter w;
		w.U8(7); w.U16(65000); w.U32(0xDEADBEEF); w.F32(3.25f); w.VarU32(5); w.VarU32(300000); w.VarI32(-3);
		w.String("pyros"); w.Position(Vec3(1234.5678f, -12.3456f, -4000.001f));
		Quaternion q; q.SetRotationFromEuler(Vec3(0.3f, 1.2f, -0.7f)); w.Rotation(q);
		check(w.Size() == 1 + 2 + 4 + 4 + 1 + 3 + 1 + 6 + 9 + 6, "sizes: varints are short, a position is 9 bytes, a rotation 6");
		NetReader r(w.data);
		check(r.U8() == 7 && r.U16() == 65000 && r.U32() == 0xDEADBEEF && r.F32() == 3.25f, "fixed-width round trip");
		check(r.VarU32() == 5 && r.VarU32() == 300000 && r.VarI32() == -3, "varints round trip");
		check(r.String() == "pyros", "string round trip");
		const Vec3 p = r.Position();
		check(std::fabs(p.x - 1234.5678f) < 0.001f && std::fabs(p.y + 12.3456f) < 0.0002f && std::fabs(p.z + 4000.001f) < 0.001f,
			"position within a millimetre");
		const Quaternion q2 = r.Rotation();
		const f32 dot = std::fabs(q.w * q2.w + q.x * q2.x + q.y * q2.y + q.z * q2.z);
		check(dot > 0.99999f, "rotation within ~0.3 degrees");
		check(r.Ok() && r.AtEnd(), "consumed exactly");
		check(r.U8() == 0 && !r.Ok(), "reading past the end fails, not overruns");

		NetWriter bad; bad.VarU32(1000);	// a string length with no bytes behind it
		NetReader rb(bad.data);
		check(rb.String().empty() && !rb.Ok(), "a truncated string is rejected");
	}

	if (!NetTransport::Available()) { printf("no transport on this platform\n"); return failures ? 1 : 0; }

	// ---- loopback ----
	const uint16 port = 47291;
	NetTransport server, client;
	check(server.Host(port, 8), "server listens");
	check(client.Connect("127.0.0.1", port), "client starts connecting");
	std::vector<NetEvent> se, ce;
	auto has = [](const std::vector<NetEvent> &v, NetEvent::Type t) { for (auto &e : v) if (e.type == t) return true; return false; };
	check(pump(server, client, se, ce, [&] { return has(se, NetEvent::Connected) && has(ce, NetEvent::Connected); }), "both ends see the connection");
	check(client.IsConnected() && server.PeerCount() == 1, "connected state");
	PeerId clientId = 0;
	for (auto &e : se) if (e.type == NetEvent::Connected) clientId = e.peer;
	se.clear(); ce.clear();

	const char hello[] = "hello";
	client.Send(1, NetChannel::Reliable, hello, sizeof(hello));
	server.Send(clientId, NetChannel::Snapshot, "snap", 5);
	check(pump(server, client, se, ce, [&] { return has(se, NetEvent::Message) && has(ce, NetEvent::Message); }), "messages both ways");
	check(!se.empty() && std::string((const char*)se.back().data.data()) == "hello" && se.back().channel == NetChannel::Reliable,
		"payload and channel arrive intact");
	se.clear(); ce.clear();

	// Simulated 60 ms +- 20 ms, 30% loss, from the client.
	client.SetSimulatedConditions(60, 20, 0.3f);
	const int N = 200;
	for (int i = 0; i < N; i++)
	{
		NetWriter w; w.U32((uint32)i);
		client.Send(1, NetChannel::Reliable, w.data.data(), w.data.size());
		client.Send(1, NetChannel::Unsequenced, w.data.data(), w.data.size());
	}
	const auto t0 = std::chrono::steady_clock::now();
	int reliable = 0, unreliable = 0;
	bool ordered = true;
	int firstAfterMs = -1;
	pump(server, client, se, ce, [&] {
		for (auto &e : se)
		{
			if (e.type != NetEvent::Message) continue;
			NetReader r(e.data);
			const uint32 v = r.U32();
			if (firstAfterMs < 0) firstAfterMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
			if (e.channel == NetChannel::Reliable) { if ((int)v != reliable) ordered = false; reliable++; }
			else unreliable++;
		}
		se.clear();
		return false;	// run the full window: jittered unreliable packets lag the reliable ones
	}, 600);
	printf("reliable %d/%d, unreliable %d/%d, first after %d ms\n", reliable, N, unreliable, N, firstAfterMs);
	check(reliable == N && ordered, "reliable: all of them, in order, through simulated loss");
	check(unreliable > N * 0.55 && unreliable < N * 0.85, "unreliable: about 70% arrive at 30% loss");
	check(firstAfterMs >= 40, "simulated latency delays delivery");
	client.SetSimulatedConditions(0, 0, 0.f);

	NetPeerStats st;
	check(server.GetStats(clientId, st) && st.bytesReceived > 0, "stats report traffic");
	// Per peer, exactly the payload: a server with many clients reads each
	// one's own traffic, not the whole host's.
	{
		const uint64 before = st.bytesSent;
		std::vector<uchar> blob(1000, 7);
		server.Send(clientId, NetChannel::Reliable, blob.data(), blob.size());
		server.Flush();
		NetPeerStats after;
		check(server.GetStats(clientId, after) && after.bytesSent - before == 1000, "bytesSent counts that peer's payload exactly");
		NetPeerStats none;
		check(!server.GetStats(clientId + 999, none), "no stats for a peer that is not there");
	}

	client.Disconnect(1);
	se.clear(); ce.clear();
	check(pump(server, client, se, ce, [&] { return has(se, NetEvent::Disconnected); }), "a disconnect reaches the server");
	check(server.PeerCount() == 0, "and the peer is gone");

	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures ? 1 : 0;
}
