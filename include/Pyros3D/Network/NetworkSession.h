//============================================================================
// Name        : NetworkSession.h
// Author      : Duarte Peixinho
// Description : A game's network session: a server that owns the world and
//               streams it to up to maxClients players, or a client that
//               joins one and shows a replica of it.
//
//               Server, every tick (tickRate a second), per client:
//                 relevance  objects within their relevance radius of the
//                            client's viewer (with hysteresis) are spawned
//                            on that client; objects that leave it, or are
//                            destroyed, are despawned. A client's own
//                            objects are always relevant to it.
//                 snapshot   what changed since the last snapshot that
//                            client ACKNOWLEDGED - so a lost packet is simply
//                            covered by the next one, and an object that
//                            stood still costs nothing - highest priority
//                            first (priority grows each tick an object waits,
//                            faster when it is close), until the per-client
//                            byte budget is spent.
//
//               Client, every frame: applies snapshots, keeps the server's
//               clock, and poses each replica by interpolating its samples
//               `interpolationDelay` seconds in the past. Every tick it
//               acknowledges the newest snapshot and reports its viewer
//               (the camera) - what relevance is measured from - and the
//               transforms of objects it owns, which the server accepts
//               (client authority; server-side prediction comes later).
//
//               RPCs are named, reliable, ordered calls with a few simple
//               arguments, in either direction.
//
//               Main thread only. Update() once a frame, before the scene's.
//============================================================================

#ifndef NETWORKSESSION_H
#define NETWORKSESSION_H

#include <Pyros3D/Network/NetTransport.h>
#include <Pyros3D/Network/NetBuffer.h>
#include <Pyros3D/Network/NetworkIdentity.h>
#include <Pyros3D/Other/Export.h>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace sol { class state; }

namespace p3d {

	class SceneGraph;
	class GameObject;
	class IPhysics;
	struct LoadedSceneAssets;

	struct PYROS3D_API NetworkSettings
	{
		f32 tickRate = 30.f;
		f32 defaultRelevance = 500.f;		// metres
		f32 relevanceHysteresis = 50.f;		// metres past the radius before despawn
		uint32 bytesPerTick = 4000;			// per client; 4000 x 30 Hz ~ 1 Mbit/s
		f32 interpolationDelay = 0.1f;		// seconds
		f32 commandRate = 60.f;				// predicted input commands a second
		f32 historySeconds = 1.f;			// how far back hit tests may rewind
		uint32 maxClients = 100;
		NetQuantization quantization;
	};

	class PYROS3D_API NetworkSession
	{
	public:
		enum Role { Offline, Server, Client };

		typedef NetworkSettings Settings;

		// scenePath is where prefab paths resolve (the project's scenes/
		// directory's scene), as for scene loading.
		NetworkSession(SceneGraph* scene, const std::string &scenePath, IPhysics* physics = NULL, sol::state* lua = NULL);
		~NetworkSession();

		bool Host(const uint16 port, const Settings &settings = Settings());
		bool Connect(const std::string &address, const uint16 port, const Settings &settings = Settings());
		void Shutdown();

		void Update(const f64 dt);

		Role GetRole() const { return role; }
		// The server is 0; a client learns its id when the server welcomes it.
		PeerId LocalPeer() const { return localPeer; }
		bool IsReady() const;	// server hosting, or client welcomed
		NetTransport &Transport() { return transport; }
		const Settings &GetSettings() const { return settings; }
		uint32 ServerTick() const { return (uint32)serverTick; }

		// Server: builds `prefab` in the scene and replicates it; `owner` is
		// the peer that controls it (0 = the server).
		std::shared_ptr<GameObject> Spawn(const std::string &prefab, const Vec3 &position, const Vec3 &rotation, const PeerId owner = 0);
		// Server: removes a replicated object everywhere.
		void Destroy(GameObject* go);
		// The object with this network id, NULL if none here.
		GameObject* Find(const uint32 netId) const;

		// Client: where the player looks from - relevance is measured from
		// it. Server: its own viewer, for listen-server play.
		void SetViewer(const Vec3 &position) { viewer = position; }

		// RPCs. Arguments are NetValues; target on the server is a peer or
		// 0 for every client, on a client it is always the server.
		void Rpc(const PeerId target, const std::string &name, const std::vector<NetValue> &args);
		typedef std::function<void(const PeerId sender, const std::vector<NetValue> &args)> RpcHandler;
		void OnRpc(const std::string &name, const RpcHandler &handler) { rpcHandlers[name] = handler; }

		// Prediction. `simulate` is the game's movement code for predicted
		// objects: it reads one command's input and moves the object by dt.
		// The server runs it on every command it receives, in order - the
		// authoritative result - and the owning client runs it the moment
		// the command is made, then, when the server's state for that
		// command arrives, snaps to it and replays the commands still in
		// flight. It must be deterministic given the object's transform and
		// variables, and kinematic: rigid bodies cannot be rewound.
		typedef std::function<void(GameObject* go, const std::vector<NetValue> &input, const f32 dt)> Simulate;
		void SetSimulate(const Simulate &fn) { simulate = fn; }
		// Client: this frame's input for the objects it owns; sampled into
		// commands at commandRate (the latest input holds until replaced).
		void SetInput(const std::vector<NetValue> &input) { currentInput = input; }

		// Server: a ray against every replicated object's hit capsule as
		// `shooter` SAW them - rewound to that client's view time (its
		// interpolated past) - so a hit on screen is a hit on the server.
		// The shooter's own objects are skipped. netId 0 when nothing.
		// viewTick: the tick the shooter was looking at when it fired - a
		// client sends ViewTick() with its fire event; < 0 uses the view
		// tick of its latest command, which is up to one latency stale.
		struct RayHit { uint32 netId = 0; f32 distance = 0.f; Vec3 point; };
		RayHit RaycastRewound(const PeerId shooter, const Vec3 &origin, const Vec3 &direction, const f32 maxDistance,
			const f64 viewTick = -1.0) const;
		// Server: every client's viewer - where to keep the world loaded.
		std::vector<Vec3> ClientViewers() const;

		// Client: the server tick its replicas are showing right now.
		f64 ViewTick() const;
		// Server: the view tick of a client's latest command.
		f64 ViewTickOf(const PeerId peer) const;

		// Server: a client joined / left. Client: joined the server (peer is
		// the id it was given) / lost it.
		std::function<void(const PeerId peer)> onPeerJoined, onPeerLeft;

		// How a client builds a spawned object. The default loads the prefab
		// file as a scene subtree; a host that expands nested prefabs (the
		// player, the editor) may supply its own.
		typedef std::function<std::shared_ptr<GameObject>(const std::string &prefab)> PrefabLoader;
		void SetPrefabLoader(const PrefabLoader &loader) { prefabLoader = loader; }

		struct Stats
		{
			uint32 replicated = 0;			// identities this side knows
			uint32 lastSnapshotBytes = 0;	// server: largest to one client last tick
			uint32 lastSnapshotEntities = 0;
			f64 clockOffsetTicks = 0.0;		// client: server tick - local estimate, last correction
		};
		const Stats &GetStats() const { return stats; }

	private:
		struct ClientState;
		struct Entity;

		void RunServerTick();
		void RunClientTick();
		void HandleMessage(const NetEvent &e);
		void HandleServerMessage(const PeerId from, NetReader &r, const uint8 type);
		void HandleClientMessage(NetReader &r, const uint8 type);
		void RegisterNewIdentities();
		void SendSpawn(ClientState &c, Entity &e);
		void SendDespawn(ClientState &c, const uint32 netId);
		void WriteVarDelta(NetWriter &w, NetworkIdentity* id, std::map<std::string, uint32> &acked, ClientState &c);
		uint32 VarId(ClientState &c, const std::string &name);
		void PoseReplicas();
		void RunClientCommands(const f64 dt);
		void Reconcile(NetworkIdentity* id, GameObject* go, const uint32 ackSeq, const Vec3 &position, const Quaternion &rotation);
		std::shared_ptr<GameObject> LoadPrefab(const std::string &prefab);
		void RemoveEntity(const uint32 netId);

		SceneGraph* scene;
		std::string scenePath;
		IPhysics* physics;
		sol::state* lua;
		NetTransport transport;
		Settings settings;
		Role role = Offline;
		PeerId localPeer = 0;
		bool welcomed = false;
		f64 tickAccumulator = 0.0;
		f64 serverTick = 0.0;		// server: ticks run; client: estimate of the server's
		f64 latestSnapshotTick = 0.0;
		Vec3 viewer;

		std::map<uint32, std::unique_ptr<Entity> > entities;
		uint32 nextNetId = 1;
		std::map<PeerId, std::unique_ptr<ClientState> > clients;
		std::map<std::string, RpcHandler> rpcHandlers;
		PrefabLoader prefabLoader;
		std::map<std::string, std::string> prefabText;	// cache of prefab files
		std::map<uint32, std::string> varNames;			// client: id -> name the server sent
		Stats stats;
		Simulate simulate;
		std::vector<NetValue> currentInput;
		f64 commandAccumulator = 0.0;
		uint32 nextCommandSeq = 1;
	};

}

#endif /* NETWORKSESSION_H */
