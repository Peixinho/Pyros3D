//============================================================================
// Name        : NetRendezvous.h
// Author      : Duarte Peixinho
// Description : Meeting through NAT. A player hosting from home sits behind
//               a router that lets nothing in unasked; another player's
//               router does the same. Both can reach a rendezvous service
//               on a public address, though, and it can tell each the
//               address the other's router showed it - after which the two
//               send to each other at once, and each router, having seen
//               its own side speak first, lets the other's packets in (UDP
//               hole punching).
//
//                 host    NetTransport::Host + Register(service, name):
//                         announces the name every few seconds, from the
//                         very socket the game uses.
//                 client  NetTransport::ConnectVia(service, name): asks
//                         for the name, is told the host's public address,
//                         and connects to it while the host - told the
//                         client's - sends toward it.
//                 service NetRendezvous: this class. A table of names and
//                         where they were last heard from; nothing of the
//                         game passes through it.
//
//               It does not work through a "symmetric" NAT (some mobile
//               and corporate networks), which gives every destination a
//               different port; those need a relay, which this is not.
//               Names are first come, first served and unauthenticated -
//               the session's own password and pinned key (see
//               NetworkSession) are what keep a stranger out.
//
//               A dedicated server on a public address needs none of this:
//               players connect out to it.
//============================================================================

#ifndef NETRENDEZVOUS_H
#define NETRENDEZVOUS_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Other/Global.h>
#include <Pyros3D/Core/Math/Math.h>
#include <string>

namespace p3d {

	class PYROS3D_API NetRendezvous
	{
	public:
		NetRendezvous();
		~NetRendezvous();

		bool Start(const uint16 port);
		void Stop();
		// Answers what has arrived; forgets hosts not heard from for a
		// minute. Call often (it never blocks).
		void Update();
		bool Running() const;
		// Hosts announced right now.
		uint32 HostCount() const;

	private:
		struct Impl;
		Impl* impl;
	};

}

#endif /* NETRENDEZVOUS_H */
