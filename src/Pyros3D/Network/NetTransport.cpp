//============================================================================
// Name        : NetTransport.cpp
// Author      : Duarte Peixinho
// Description : See NetTransport.h.
//============================================================================

#include <Pyros3D/Network/NetTransport.h>
#include <Pyros3D/Core/Logs/Log.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <map>
#include <mutex>
#include <random>

#if defined(PYROS_NETWORKING)
#include <enet/enet.h>
#endif

namespace p3d {

#if defined(PYROS_NETWORKING)

	namespace {
		// enet_initialize/deinitialize are process-wide; the transports
		// share one reference count.
		std::mutex g_initMutex;
		int g_initCount = 0;

		bool AcquireENet()
		{
			std::lock_guard<std::mutex> lock(g_initMutex);
			if (g_initCount == 0 && enet_initialize() != 0)
			{
				echo("ERROR: NetTransport - enet_initialize failed");
				return false;
			}
			g_initCount++;
			return true;
		}

		void ReleaseENet()
		{
			std::lock_guard<std::mutex> lock(g_initMutex);
			if (g_initCount > 0 && --g_initCount == 0) enet_deinitialize();
		}

		uint64 NowMs()
		{
			return (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count();
		}
	}

	struct NetTransport::Impl
	{
		ENetHost* host = NULL;
		bool server = false;
		bool initialised = false;
		PeerId nextId = 1;
		std::map<PeerId, ENetPeer*> peers;
		// Application bytes to and from each peer (ENet's own counters are
		// host-wide, or reset by its throttle).
		struct Bytes { uint64 sent = 0, received = 0; };
		std::map<PeerId, Bytes> bytes;

		// Simulated conditions - see SetSimulatedConditions().
		struct Delayed { uint64 due; PeerId peer; uint32 channel; std::vector<uchar> data; };
		std::deque<Delayed> delayed;
		uint32 latencyMs = 0, jitterMs = 0;
		f32 loss = 0.f;
		std::mt19937 rng{ 12345u };

		static PeerId IdOf(ENetPeer* p) { return (PeerId)(uintptr_t)p->data; }

		ENetPeer* PeerOf(const PeerId id) const
		{
			std::map<PeerId, ENetPeer*>::const_iterator it = peers.find(id);
			return it == peers.end() ? NULL : it->second;
		}

		bool SendNow(const PeerId id, const uint32 channel, const void* data, const size_t length)
		{
			ENetPeer* p = PeerOf(id);
			if (!p) return false;
			enet_uint32 flags = 0;
			if (channel == NetChannel::Reliable) flags = ENET_PACKET_FLAG_RELIABLE;
			else if (channel == NetChannel::Unsequenced) flags = ENET_PACKET_FLAG_UNSEQUENCED;
			ENetPacket* packet = enet_packet_create(data, length, flags);
			if (!packet) return false;
			if (enet_peer_send(p, (enet_uint8)channel, packet) != 0)
			{
				enet_packet_destroy(packet);
				return false;
			}
			bytes[id].sent += length;
			return true;
		}

		void ReleaseDelayed(const bool all)
		{
			const uint64 now = NowMs();
			// Not strictly ordered by due time once jitter is on - unreliable
			// packets may overtake each other, which is the point.
			for (std::deque<Delayed>::iterator it = delayed.begin(); it != delayed.end();)
			{
				if (all || it->due <= now)
				{
					SendNow(it->peer, it->channel, it->data.empty() ? NULL : &it->data[0], it->data.size());
					it = delayed.erase(it);
				}
				else ++it;
			}
		}
	};

	bool NetTransport::Available() { return true; }

	NetTransport::NetTransport() : impl(new Impl())
	{
		impl->initialised = AcquireENet();
	}

	NetTransport::~NetTransport()
	{
		Shutdown();
		if (impl->initialised) ReleaseENet();
		delete impl;
	}

	bool NetTransport::Host(const uint16 port, const uint32 maxClients)
	{
		if (!impl->initialised || impl->host) return false;
		ENetAddress address;
		address.host = ENET_HOST_ANY;
		address.port = port;
		impl->host = enet_host_create(&address, maxClients, NetChannel::Count, 0, 0);
		if (!impl->host)
		{
			echo("ERROR: NetTransport - could not listen on port " + std::to_string(port));
			return false;
		}
		impl->server = true;
		return true;
	}

	bool NetTransport::Connect(const std::string &addressName, const uint16 port)
	{
		if (!impl->initialised || impl->host) return false;
		impl->host = enet_host_create(NULL, 1, NetChannel::Count, 0, 0);
		if (!impl->host) return false;
		ENetAddress address;
		if (enet_address_set_host(&address, addressName.c_str()) != 0)
		{
			echo("ERROR: NetTransport - could not resolve " + addressName);
			enet_host_destroy(impl->host);
			impl->host = NULL;
			return false;
		}
		address.port = port;
		ENetPeer* p = enet_host_connect(impl->host, &address, NetChannel::Count, 0);
		if (!p) return false;
		// The server is always peer 1 on a client.
		p->data = (void*)(uintptr_t)1;
		impl->peers[1] = p;
		impl->server = false;
		return true;
	}

	void NetTransport::Poll(std::vector<NetEvent> &out, const uint32 waitMs)
	{
		if (!impl->host) return;
		impl->ReleaseDelayed(false);
		ENetEvent ev;
		uint32 wait = waitMs;
		while (enet_host_service(impl->host, &ev, wait) > 0)
		{
			wait = 0;
			NetEvent e;
			switch (ev.type)
			{
			case ENET_EVENT_TYPE_CONNECT:
				if (impl->server)
				{
					const PeerId id = impl->nextId++;
					ev.peer->data = (void*)(uintptr_t)id;
					impl->peers[id] = ev.peer;
				}
				e.type = NetEvent::Connected;
				e.peer = Impl::IdOf(ev.peer);
				out.push_back(e);
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				e.type = NetEvent::Disconnected;
				e.peer = Impl::IdOf(ev.peer);
				impl->peers.erase(e.peer);
				impl->bytes.erase(e.peer);
				ev.peer->data = NULL;
				out.push_back(e);
				break;
			case ENET_EVENT_TYPE_RECEIVE:
				e.type = NetEvent::Message;
				e.peer = Impl::IdOf(ev.peer);
				e.channel = ev.channelID;
				e.data.assign(ev.packet->data, ev.packet->data + ev.packet->dataLength);
				impl->bytes[e.peer].received += ev.packet->dataLength;
				enet_packet_destroy(ev.packet);
				out.push_back(e);
				break;
			default:
				break;
			}
		}
	}

	bool NetTransport::Send(const PeerId peer, const uint32 channel, const void* data, const size_t length)
	{
		if (!impl->host || channel >= NetChannel::Count) return false;
		if (impl->latencyMs == 0 && impl->jitterMs == 0 && impl->loss <= 0.f)
			return impl->SendNow(peer, channel, data, length);
		if (!impl->PeerOf(peer)) return false;
		// Simulated: loss only on the unreliable channels - dropping a
		// reliable packet before ENet ever sees it would lose it for good,
		// which the real network never does to one.
		const bool reliable = (channel == NetChannel::Reliable);
		std::uniform_real_distribution<f32> unit(0.f, 1.f);
		if (!reliable && unit(impl->rng) < impl->loss) return true;
		int64 delay = impl->latencyMs;
		if (!reliable && impl->jitterMs > 0)
			delay += (int64)(unit(impl->rng) * 2.f * impl->jitterMs) - impl->jitterMs;
		Impl::Delayed d;
		d.due = NowMs() + (uint64)std::max<int64>(delay, 0);
		d.peer = peer;
		d.channel = channel;
		d.data.assign((const uchar*)data, (const uchar*)data + length);
		impl->delayed.push_back(d);
		return true;
	}

	void NetTransport::Broadcast(const uint32 channel, const void* data, const size_t length)
	{
		for (std::map<PeerId, ENetPeer*>::const_iterator it = impl->peers.begin(); it != impl->peers.end(); ++it)
			Send(it->first, channel, data, length);
	}

	void NetTransport::Flush()
	{
		if (!impl->host) return;
		impl->ReleaseDelayed(false);
		enet_host_flush(impl->host);
	}

	void NetTransport::Disconnect(const PeerId peer)
	{
		// _later: what is already queued for it (a "why you were refused"
		// message) goes out first.
		if (ENetPeer* p = impl->PeerOf(peer)) enet_peer_disconnect_later(p, 0);
	}

	void NetTransport::Shutdown()
	{
		if (!impl->host) return;
		impl->ReleaseDelayed(true);
		for (std::map<PeerId, ENetPeer*>::const_iterator it = impl->peers.begin(); it != impl->peers.end(); ++it)
			enet_peer_disconnect_now(it->second, 0);
		enet_host_flush(impl->host);
		enet_host_destroy(impl->host);
		impl->host = NULL;
		impl->peers.clear();
		impl->bytes.clear();
		impl->delayed.clear();
	}

	bool NetTransport::IsServer() const { return impl->server && impl->host; }

	bool NetTransport::IsConnected() const
	{
		if (!impl->host) return false;
		if (impl->server) return true;
		ENetPeer* p = impl->PeerOf(1);
		return p && p->state == ENET_PEER_STATE_CONNECTED;
	}

	uint32 NetTransport::PeerCount() const
	{
		uint32 n = 0;
		for (std::map<PeerId, ENetPeer*>::const_iterator it = impl->peers.begin(); it != impl->peers.end(); ++it)
			if (it->second->state == ENET_PEER_STATE_CONNECTED) n++;
		return n;
	}

	bool NetTransport::GetStats(const PeerId peer, NetPeerStats &out) const
	{
		ENetPeer* p = impl->PeerOf(peer);
		if (!p) return false;
		out.roundTripMs = p->roundTripTime;
		out.roundTripVarianceMs = p->roundTripTimeVariance;
		out.packetLoss = (f32)p->packetLoss / (f32)ENET_PEER_PACKET_LOSS_SCALE;
		// This peer's, not the host's: every peer used to report the
		// whole host's traffic, so a server's per-client bandwidth read as
		// the sum over all its clients.
		std::map<PeerId, Impl::Bytes>::const_iterator b = impl->bytes.find(peer);
		out.bytesSent = b == impl->bytes.end() ? 0 : b->second.sent;
		out.bytesReceived = b == impl->bytes.end() ? 0 : b->second.received;
		return true;
	}

	std::string NetTransport::PeerAddress(const PeerId peer) const
	{
		ENetPeer* p = impl->PeerOf(peer);
		if (!p) return std::string();
		char ip[64] = { 0 };
		if (enet_address_get_host_ip(&p->address, ip, sizeof(ip)) != 0) return std::string();
		return ip;
	}

	void NetTransport::SetSimulatedConditions(const uint32 latencyMs, const uint32 jitterMs, const f32 loss)
	{
		impl->latencyMs = latencyMs;
		impl->jitterMs = jitterMs;
		impl->loss = std::min(std::max(loss, 0.f), 1.f);
	}

#else	// no UDP sockets here (the web build)

	struct NetTransport::Impl {};
	bool NetTransport::Available() { return false; }
	NetTransport::NetTransport() : impl(new Impl()) {}
	NetTransport::~NetTransport() { delete impl; }
	bool NetTransport::Host(const uint16, const uint32) { return false; }
	bool NetTransport::Connect(const std::string &, const uint16) { return false; }
	void NetTransport::Poll(std::vector<NetEvent> &, const uint32) {}
	bool NetTransport::Send(const PeerId, const uint32, const void*, const size_t) { return false; }
	void NetTransport::Broadcast(const uint32, const void*, const size_t) {}
	void NetTransport::Flush() {}
	void NetTransport::Disconnect(const PeerId) {}
	std::string NetTransport::PeerAddress(const PeerId) const { return std::string(); }
	void NetTransport::Shutdown() {}
	bool NetTransport::IsServer() const { return false; }
	bool NetTransport::IsConnected() const { return false; }
	uint32 NetTransport::PeerCount() const { return 0; }
	bool NetTransport::GetStats(const PeerId, NetPeerStats &) const { return false; }
	void NetTransport::SetSimulatedConditions(const uint32, const uint32, const f32) {}

#endif

}
