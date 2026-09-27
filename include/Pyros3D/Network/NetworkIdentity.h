//============================================================================
// Name        : NetworkIdentity.h
// Author      : Duarte Peixinho
// Description : Marks a GameObject as replicated. The server owns the truth:
//               it gives the object a network id, tells each client about it
//               while it is relevant to them (within `relevance` metres of
//               their viewer), and streams its transform and variables. On a
//               client the object is a replica - moved by snapshots,
//               interpolated a little behind the server so it moves smoothly
//               through jitter and loss.
//
//               Two ways to exist on a client:
//                 spawned  the server names a prefab; the client builds it
//                 bound    the object is in the scene on both ends already
//                          (a door, a pickup); the server names it by its
//                          path in the hierarchy and the client finds its
//                          own copy
//
//               Variables are named values (health, ammo, team) the server
//               sets and every client that knows the object receives -
//               reliably, eventually, and only when they change.
//============================================================================

#ifndef NETWORKIDENTITY_H
#define NETWORKIDENTITY_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Network/NetTransport.h>
#include <Pyros3D/Other/Export.h>
#include <map>
#include <string>
#include <vector>

namespace p3d {

	class GameObject;

	struct PYROS3D_API NetValue
	{
		enum Type { None, Number, Bool, String, Vector };
		Type type = None;
		f64 number = 0.0;
		bool boolean = false;
		std::string text;
		Vec3 vector;

		static NetValue FromNumber(const f64 v) { NetValue n; n.type = Number; n.number = v; return n; }
		static NetValue FromBool(const bool v) { NetValue n; n.type = Bool; n.boolean = v; return n; }
		static NetValue FromString(const std::string &v) { NetValue n; n.type = String; n.text = v; return n; }
		static NetValue FromVector(const Vec3 &v) { NetValue n; n.type = Vector; n.vector = v; return n; }
		bool operator==(const NetValue &o) const;
		bool operator!=(const NetValue &o) const { return !(*this == o); }
	};

	class PYROS3D_API NetworkIdentity : public IComponent
	{
	public:
		NetworkIdentity() {}
		virtual ~NetworkIdentity();

		// Authored settings.
		std::string prefab;			// what a client builds to spawn this; empty = bound by scene path
		f32 relevance = 0.f;		// metres; 0 = the session's default
		f32 priority = 1.f;			// share of the bandwidth budget relative to others
		bool syncTransform = true;

		// 0 until the server has registered it.
		uint32 GetNetId() const { return netId; }
		// The peer that controls it; 0 = the server. (GetOwner() is the
		// GameObject, as for every component.)
		PeerId GetOwnerPeer() const { return owner; }

		// Server: set a replicated variable. Client: ignored (the server's
		// next value wins) - a client changes things by asking the server,
		// through an RPC.
		void SetVar(const std::string &name, const NetValue &value);
		bool GetVar(const std::string &name, NetValue &out) const;
		const std::map<std::string, NetValue> &GetVars() const { return vars; }

		// "Root/Child/Grandchild" - how a bound object is found on both ends.
		static std::string ScenePath(GameObject* go);

		// Every live identity, for the session to find new ones (a streamed
		// cell arriving brings its own). Main thread.
		static const std::vector<NetworkIdentity*> &All();

		virtual void Register(SceneGraph* Scene);
		virtual void Unregister(SceneGraph* Scene);
		virtual void Init() {}
		virtual void Update(const f64 time = 0) {}
		virtual void Destroy() {}

	private:
		friend class NetworkSession;
		uint32 netId = 0;
		PeerId owner = 0;
		std::map<std::string, NetValue> vars;
		std::map<std::string, uint32> varVersion;	// bumped on every change, for delta
		uint32 versionCounter = 0;
		bool replica = false;		// true on a client: driven by the server

		// Client interpolation: server tick -> transform.
		struct Sample { f64 tick; Vec3 position; Quaternion rotation; };
		std::vector<Sample> samples;
	};

}

#endif /* NETWORKIDENTITY_H */
