//============================================================================
// Name        : NetTransport.cpp
// Author      : Duarte Peixinho
// Description : See NetTransport.h.
//============================================================================

#include <Pyros3D/Network/NetTransport.h>
#include <Pyros3D/Network/NetRendezvous.h>
#include <Pyros3D/Core/Logs/Log.h>

#include <algorithm>
#include <cstring>
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

		// ---- rendezvous datagrams (see NetRendezvous.h) ----
		// Raw UDP on the game's own socket, told apart from ENet's by this
		// prefix; one type byte follows.
		const char kRvMagic[8] = { 'P', 'Y', 'R', 'O', 'S', 'R', 'V', '1' };
		namespace Rv {
			enum {
				Register = 'R',	// host -> service: name
				Ack = 'A',		// service -> host
				Taken = 'T',	// service -> host: another host holds that name
				Query = 'Q',	// client -> service: name
				HostAt = 'H',	// service -> client: the host's address
				NoHost = 'N',	// service -> client: nobody by that name
				Punch = 'P',	// service -> host: a client's address, send toward it
				Hole = 'X'		// host -> client: carries nothing; opens the host's router
			};
		}
		const size_t kRvHeader = sizeof(kRvMagic) + 1;
		const size_t kRvNameMax = 64;

		bool IsRv(const uchar* data, const size_t n) { return n >= kRvHeader && std::memcmp(data, kRvMagic, sizeof(kRvMagic)) == 0; }

		void RvSend(const ENetSocket socket, const ENetAddress &to, const uint8 type, const void* payload = NULL, const size_t n = 0)
		{
			uchar packet[kRvHeader + kRvNameMax];
			if (n > kRvNameMax) return;
			std::memcpy(packet, kRvMagic, sizeof(kRvMagic));
			packet[sizeof(kRvMagic)] = type;
			if (n) std::memcpy(packet + kRvHeader, payload, n);
			ENetBuffer b;
			b.data = packet;
			b.dataLength = kRvHeader + n;
			enet_socket_send(socket, &to, &b, 1);
		}

		// An address as six bytes: ENet keeps the host in network order
		// already, the port goes low byte first.
		void RvPutAddress(uchar out[6], const ENetAddress &a)
		{
			std::memcpy(out, &a.host, 4);
			out[4] = (uchar)(a.port & 0xff);
			out[5] = (uchar)(a.port >> 8);
		}
		ENetAddress RvGetAddress(const uchar in[6])
		{
			ENetAddress a;
			std::memcpy(&a.host, in, 4);
			a.port = (enet_uint16)(in[4] | (in[5] << 8));
			return a;
		}
		bool SameAddress(const ENetAddress &a, const ENetAddress &b) { return a.host == b.host && a.port == b.port; }

		// ENet's intercept callback is given the host and nothing else.
		std::mutex g_hostsMutex;
		std::map<ENetHost*, void*> g_hosts;
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
		NetTransport::Tap tap;

		// Simulated conditions - see SetSimulatedConditions().
		struct Delayed { uint64 due; PeerId peer; uint32 channel; std::vector<uchar> data; };
		std::deque<Delayed> delayed;
		uint32 latencyMs = 0, jitterMs = 0;
		f32 loss = 0.f;
		std::mt19937 rng{ 12345u };

		// Rendezvous - see NetRendezvous.h.
		struct RvPacket { ENetAddress from; std::vector<uchar> data; };
		std::vector<RvPacket> rvInbox;		// filled by Intercept, inside enet_host_service
		bool rvActive = false, rvRegistered = false, rvAsking = false, rvWarned = false;
		ENetAddress rvService;
		std::string rvName;
		uint64 rvNextSend = 0, rvDeadline = 0;
		struct Punch { ENetAddress to; int remaining; uint64 next; };
		std::vector<Punch> punches;

		static int ENET_CALLBACK Intercept(ENetHost* host, ENetEvent*)
		{
			if (!IsRv(host->receivedData, host->receivedDataLength)) return 0;
			std::lock_guard<std::mutex> lock(g_hostsMutex);
			std::map<ENetHost*, void*>::iterator it = g_hosts.find(host);
			if (it != g_hosts.end())
			{
				Impl* self = (Impl*)it->second;
				// Bounded: these are a handful a second at most when honest.
				if (self->rvInbox.size() < 64)
				{
					RvPacket pkt;
					pkt.from = host->receivedAddress;
					pkt.data.assign(host->receivedData, host->receivedData + host->receivedDataLength);
					self->rvInbox.push_back(pkt);
				}
			}
			return 1;	// never ENet's to parse
		}

		void Watch()
		{
			std::lock_guard<std::mutex> lock(g_hostsMutex);
			g_hosts[host] = this;
			host->intercept = &Impl::Intercept;
		}

		void Unwatch()
		{
			std::lock_guard<std::mutex> lock(g_hostsMutex);
			g_hosts.erase(host);
		}

		void Rendezvous(std::vector<NetEvent> &out)
		{
			if (!rvActive) { rvInbox.clear(); return; }
			const uint64 now = NowMs();
			std::vector<RvPacket> inbox;
			inbox.swap(rvInbox);
			for (size_t i = 0; i < inbox.size(); i++)
			{
				const RvPacket &pkt = inbox[i];
				// Only the service is listened to: anyone else saying "send
				// toward this address" would be using this host to knock on
				// a stranger's door.
				if (!SameAddress(pkt.from, rvService)) continue;
				const uint8 type = pkt.data[sizeof(kRvMagic)];
				const uchar* body = pkt.data.data() + kRvHeader;
				const size_t n = pkt.data.size() - kRvHeader;
				if (server)
				{
					if (type == Rv::Ack) rvRegistered = true;
					else if (type == Rv::Taken && !rvWarned)
					{
						rvWarned = true;
						echo("WARNING: NetTransport - the rendezvous name '" + rvName + "' is held by another host");
					}
					else if (type == Rv::Punch && n >= 6 && punches.size() < 64)
					{
						Punch p;
						p.to = RvGetAddress(body);
						p.remaining = 4;
						p.next = now;
						punches.push_back(p);
					}
				}
				else if (rvAsking)
				{
					if (type == Rv::HostAt && n >= 6)
					{
						rvAsking = false;
						const ENetAddress at = RvGetAddress(body);
						// ENet's own connect attempts, repeated, are this
						// side's half of the punch.
						ENetPeer* p = enet_host_connect(host, &at, NetChannel::Count, 0);
						if (p) { p->data = (void*)(uintptr_t)1; peers[1] = p; }
						else rvDeadline = now;	// fall through to "could not"
					}
					else if (type == Rv::NoHost) { rvAsking = false; rvDeadline = now; }
				}
			}
			if (server)
			{
				// Announce - and, by doing so, keep the router's door to
				// the service open: every 5 s once acknowledged.
				if (now >= rvNextSend)
				{
					RvSend(host->socket, rvService, Rv::Register, rvName.data(), rvName.size());
					rvNextSend = now + (rvRegistered ? 5000 : 1000);
				}
				for (size_t i = 0; i < punches.size();)
				{
					if (now >= punches[i].next)
					{
						RvSend(host->socket, punches[i].to, Rv::Hole);
						punches[i].next = now + 150;
						if (--punches[i].remaining <= 0) { punches.erase(punches.begin() + i); continue; }
					}
					++i;
				}
			}
			else if (rvAsking)
			{
				if (now >= rvDeadline) rvAsking = false;
				else if (now >= rvNextSend)
				{
					RvSend(host->socket, rvService, Rv::Query, rvName.data(), rvName.size());
					rvNextSend = now + 500;
				}
			}
			// The service never answered, knew no such host, or the
			// connect could not start: to whoever is waiting, a connection
			// that did not happen.
			if (!server && !rvAsking && rvDeadline != 0 && !PeerOf(1))
			{
				rvDeadline = 0;
				NetEvent e;
				e.type = NetEvent::Disconnected;
				e.peer = 1;
				out.push_back(e);
			}
			else if (!server && !rvAsking && PeerOf(1)) rvDeadline = 0;
		}

		static bool Resolve(const std::string &name, const uint16 port, ENetAddress &out)
		{
			if (enet_address_set_host(&out, name.c_str()) != 0) return false;
			out.port = port;
			return true;
		}

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
			if (tap) tap(id, (const uchar*)data, length, true);
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
				if (impl->tap) impl->tap(e.peer, ev.packet->data, ev.packet->dataLength, false);
				enet_packet_destroy(ev.packet);
				out.push_back(e);
				break;
			default:
				break;
			}
		}
		impl->Rendezvous(out);
	}

	bool NetTransport::Register(const std::string &rendezvousAddress, const uint16 rendezvousPort, const std::string &name)
	{
		if (!impl->host || !impl->server || name.empty() || name.size() > kRvNameMax) return false;
		if (!Impl::Resolve(rendezvousAddress, rendezvousPort, impl->rvService))
		{
			echo("ERROR: NetTransport - could not resolve the rendezvous service " + rendezvousAddress);
			return false;
		}
		impl->rvName = name;
		impl->rvActive = true;
		impl->rvRegistered = impl->rvWarned = false;
		impl->rvNextSend = 0;
		impl->Watch();
		return true;
	}

	bool NetTransport::ConnectVia(const std::string &rendezvousAddress, const uint16 rendezvousPort, const std::string &name)
	{
		if (!impl->initialised || impl->host || name.empty() || name.size() > kRvNameMax) return false;
		ENetAddress service;
		if (!Impl::Resolve(rendezvousAddress, rendezvousPort, service))
		{
			echo("ERROR: NetTransport - could not resolve the rendezvous service " + rendezvousAddress);
			return false;
		}
		impl->host = enet_host_create(NULL, 1, NetChannel::Count, 0, 0);
		if (!impl->host) return false;
		impl->server = false;
		impl->rvService = service;
		impl->rvName = name;
		impl->rvActive = impl->rvAsking = true;
		impl->rvNextSend = 0;
		impl->rvDeadline = NowMs() + 5000;
		impl->Watch();
		return true;
	}

	bool NetTransport::IsRegistered() const { return impl->rvActive && impl->rvRegistered; }

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
		impl->Unwatch();
		enet_host_destroy(impl->host);
		impl->host = NULL;
		impl->rvActive = impl->rvRegistered = impl->rvAsking = false;
		impl->rvDeadline = 0;
		impl->rvInbox.clear();
		impl->punches.clear();
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

	void NetTransport::SetTap(const Tap &tap) { impl->tap = tap; }

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

	// ---- the service ----

	struct NetRendezvous::Impl
	{
		bool initialised = false, open = false;
		ENetSocket socket = ENET_SOCKET_NULL;
		struct Entry { ENetAddress at; uint64 heard; };
		std::map<std::string, Entry> hosts;
	};

	NetRendezvous::NetRendezvous() : impl(new Impl()) { impl->initialised = AcquireENet(); }

	NetRendezvous::~NetRendezvous()
	{
		Stop();
		if (impl->initialised) ReleaseENet();
		delete impl;
	}

	bool NetRendezvous::Start(const uint16 port)
	{
		if (!impl->initialised || impl->open) return false;
		impl->socket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
		if (impl->socket == ENET_SOCKET_NULL) return false;
		ENetAddress any;
		any.host = ENET_HOST_ANY;
		any.port = port;
		if (enet_socket_bind(impl->socket, &any) != 0)
		{
			enet_socket_destroy(impl->socket);
			impl->socket = ENET_SOCKET_NULL;
			return false;
		}
		enet_socket_set_option(impl->socket, ENET_SOCKOPT_NONBLOCK, 1);
		impl->open = true;
		return true;
	}

	void NetRendezvous::Stop()
	{
		if (!impl->open) return;
		enet_socket_destroy(impl->socket);
		impl->socket = ENET_SOCKET_NULL;
		impl->open = false;
		impl->hosts.clear();
	}

	bool NetRendezvous::Running() const { return impl->open; }
	uint32 NetRendezvous::HostCount() const { return (uint32)impl->hosts.size(); }

	void NetRendezvous::Update()
	{
		if (!impl->open) return;
		const uint64 now = NowMs();
		uchar packet[256];
		// Bounded per call, so a flood cannot hold the caller here.
		for (int i = 0; i < 512; i++)
		{
			ENetAddress from;
			ENetBuffer b;
			b.data = packet;
			b.dataLength = sizeof(packet);
			const int n = enet_socket_receive(impl->socket, &from, &b, 1);
			if (n <= 0) break;
			if (!IsRv(packet, (size_t)n)) continue;
			const uint8 type = packet[sizeof(kRvMagic)];
			const std::string name((const char*)packet + kRvHeader, (size_t)n - kRvHeader);
			if (name.empty() || name.size() > kRvNameMax) continue;
			std::map<std::string, Impl::Entry>::iterator it = impl->hosts.find(name);
			const bool live = it != impl->hosts.end() && now - it->second.heard < 60000;
			if (type == Rv::Register)
			{
				// First come, first served, until it goes quiet.
				if (live && !SameAddress(it->second.at, from)) { RvSend(impl->socket, from, Rv::Taken); continue; }
				if (it == impl->hosts.end() && impl->hosts.size() >= 4096) continue;
				Impl::Entry e;
				e.at = from;
				e.heard = now;
				impl->hosts[name] = e;
				RvSend(impl->socket, from, Rv::Ack);
			}
			else if (type == Rv::Query)
			{
				if (!live) { RvSend(impl->socket, from, Rv::NoHost); continue; }
				// Each is told where the other is, as this service saw it.
				uchar address[6];
				RvPutAddress(address, it->second.at);
				RvSend(impl->socket, from, Rv::HostAt, address, 6);
				RvPutAddress(address, from);
				RvSend(impl->socket, it->second.at, Rv::Punch, address, 6);
			}
		}
		for (std::map<std::string, Impl::Entry>::iterator it = impl->hosts.begin(); it != impl->hosts.end();)
		{
			if (now - it->second.heard >= 60000) it = impl->hosts.erase(it);
			else ++it;
		}
	}

#else	// no UDP sockets here (the web build)

	struct NetRendezvous::Impl {};
	NetRendezvous::NetRendezvous() : impl(new Impl()) {}
	NetRendezvous::~NetRendezvous() { delete impl; }
	bool NetRendezvous::Start(const uint16) { return false; }
	void NetRendezvous::Stop() {}
	void NetRendezvous::Update() {}
	bool NetRendezvous::Running() const { return false; }
	uint32 NetRendezvous::HostCount() const { return 0; }
	bool NetTransport::Register(const std::string &, const uint16, const std::string &) { return false; }
	bool NetTransport::ConnectVia(const std::string &, const uint16, const std::string &) { return false; }
	bool NetTransport::IsRegistered() const { return false; }

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
	void NetTransport::SetTap(const Tap &) {}
	void NetTransport::Shutdown() {}
	bool NetTransport::IsServer() const { return false; }
	bool NetTransport::IsConnected() const { return false; }
	uint32 NetTransport::PeerCount() const { return 0; }
	bool NetTransport::GetStats(const PeerId, NetPeerStats &) const { return false; }
	void NetTransport::SetSimulatedConditions(const uint32, const uint32, const f32) {}

#endif

}
