//============================================================================
// Name        : NetTransport.h
// Author      : Duarte Peixinho
// Description : UDP connections for a networked game - a server hosting
//               clients, or a client joined to a server - over ENet, which
//               stays out of every public header.
//
//               Three channels, because a game sends three kinds of thing:
//                 Reliable     ordered and guaranteed - spawns, despawns,
//                              RPCs, chat. Late is fine, lost is not.
//                 Snapshot     unreliable, and an older one arriving after
//                              a newer one is dropped: state that the next
//                              packet replaces anyway.
//                 Unsequenced  unreliable and unordered - input, which the
//                              client sends redundantly so a lost packet
//                              costs nothing.
//
//               Nothing blocks. Poll() once a frame pumps the socket and
//               hands back what arrived; Send() queues, and the queue goes
//               out on the next Poll() or Flush().
//
//               Available() is false where there are no UDP sockets (the
//               web build) and every call there fails harmlessly.
//============================================================================

#ifndef NETTRANSPORT_H
#define NETTRANSPORT_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Other/Global.h>
#include <Pyros3D/Core/Math/Math.h>
#include <functional>
#include <string>
#include <vector>

namespace p3d {

	// 0 is never a peer. On a client the server is peer 1.
	typedef uint32 PeerId;

	namespace NetChannel
	{
		enum { Reliable = 0, Snapshot = 1, Unsequenced = 2, Count = 3 };
	}

	struct NetEvent
	{
		enum Type { Connected, Disconnected, Message };
		Type type = Message;
		PeerId peer = 0;
		uint32 channel = 0;
		std::vector<uchar> data;
	};

	struct NetPeerStats
	{
		uint32 roundTripMs = 0;
		uint32 roundTripVarianceMs = 0;
		f32 packetLoss = 0.f;		// 0..1
		// Application payload to and from this peer since it connected
		// (ENet's headers and resends not included).
		uint64 bytesSent = 0, bytesReceived = 0;
	};

	class PYROS3D_API NetTransport
	{
	public:
		NetTransport();
		~NetTransport();
		NetTransport(const NetTransport &) = delete;
		NetTransport &operator=(const NetTransport &) = delete;

		static bool Available();

		// Server: listen on `port` (0.0.0.0) for up to maxClients.
		bool Host(const uint16 port, const uint32 maxClients);
		// Client: start connecting; a Connected event (peer 1) follows on a
		// later Poll(), or a Disconnected one if it fails.
		bool Connect(const std::string &address, const uint16 port);

		// Arrived events, oldest first. waitMs > 0 blocks up to that long
		// for the first one - for a server with nothing else to do.
		void Poll(std::vector<NetEvent> &out, const uint32 waitMs = 0);

		bool Send(const PeerId peer, const uint32 channel, const void* data, const size_t length);
		void Broadcast(const uint32 channel, const void* data, const size_t length);
		void Flush();

		// A graceful goodbye: the peer gets a Disconnected event.
		void Disconnect(const PeerId peer);
		// Everything closed, at once.
		void Shutdown();

		bool IsServer() const;
		bool IsConnected() const;		// client: joined; server: hosting
		uint32 PeerCount() const;
		bool GetStats(const PeerId peer, NetPeerStats &out) const;
		// Every payload as it goes out (true) and comes in (false), exactly
		// as it is on the wire - for a traffic meter, or a test that wants
		// to know what an eavesdropper would read. Not for game logic.
		typedef std::function<void(const PeerId peer, const uchar* data, const size_t length, const bool outgoing)> Tap;
		void SetTap(const Tap &tap);
		// The peer's address ("203.0.113.7"), empty when unknown - what a
		// ban is recorded against.
		std::string PeerAddress(const PeerId peer) const;

		// Simulated conditions for testing: every packet sent from here is
		// delayed by latencyMs +- jitterMs, and dropped with probability
		// loss. 0/0/0 is off.
		void SetSimulatedConditions(const uint32 latencyMs, const uint32 jitterMs, const f32 loss);

	private:
		struct Impl;
		Impl* impl;
	};

}

#endif /* NETTRANSPORT_H */
