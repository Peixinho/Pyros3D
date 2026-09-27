//============================================================================
// Name        : NetworkSession.cpp
// Author      : Duarte Peixinho
// Description : See NetworkSession.h.
//============================================================================

#include <Pyros3D/Network/NetworkSession.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Core/Logs/Log.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace p3d {

	namespace {
		namespace Msg
		{
			enum : uint8
			{
				// server -> client, reliable
				Welcome = 1, Spawn = 2, Despawn = 3, VarName = 4, Rpc = 5,
				// server -> client, snapshot channel
				Snapshot = 10,
				// client -> server, unsequenced
				Ack = 20, Commands = 21
			};
		}

		namespace Mask
		{
			// Predict: the owner's own predicted object - the authoritative
			// transform after the last command the server processed.
			enum : uint8 { Position = 1, Rotation = 2, Vars = 4, Predict = 8 };
		}

		// Kept in flight per client until acknowledged or too old.
		const uint32 kMaxInflightTicks = 64;
		// Commands resent with each new one, so a lost packet costs nothing.
		const size_t kCommandRedundancy = 4;
		// Commands an owner keeps unacknowledged before giving up on old ones.
		const size_t kMaxPendingCommands = 240;

		void WriteValue(NetWriter &w, const NetValue &v);
		NetValue ReadValue(NetReader &r);

		Quaternion ToQuat(const Vec3 &euler)
		{
			Quaternion q;
			q.SetRotationFromEuler(euler);
			return q;
		}

		void WriteValue(NetWriter &w, const NetValue &v)
		{
			w.U8((uint8)v.type);
			switch (v.type)
			{
			case NetValue::Number: { uint64 u; memcpy(&u, &v.number, 8); w.U64(u); } break;
			case NetValue::Bool: w.Bool(v.boolean); break;
			case NetValue::String: w.String(v.text); break;
			case NetValue::Vector: w.Vec3Raw(v.vector); break;
			default: break;
			}
		}

		NetValue ReadValue(NetReader &r)
		{
			NetValue v;
			v.type = (NetValue::Type)r.U8();
			switch (v.type)
			{
			case NetValue::Number: { const uint64 u = r.U64(); memcpy(&v.number, &u, 8); } break;
			case NetValue::Bool: v.boolean = r.Bool(); break;
			case NetValue::String: v.text = r.String(); break;
			case NetValue::Vector: v.vector = r.Vec3Raw(); break;
			case NetValue::None: break;
			default: v.type = NetValue::None; break;
			}
			return v;
		}

		bool Moved(const Vec3 &a, const Vec3 &b) { return (a - b).magnitude() > 0.0005f; }
		bool Turned(const Quaternion &a, const Quaternion &b)
		{
			return std::fabs(a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z) < 0.999999f;
		}

		GameObject* FindByPath(SceneGraph* scene, const std::string &path)
		{
			std::vector<GameObject*> all;
			scene->CollectGameObjectsRecursive(all);
			for (size_t i = 0; i < all.size(); i++)
				if (all[i] && NetworkIdentity::ScenePath(all[i]) == path) return all[i];
			return NULL;
		}

		NetworkIdentity* IdentityOf(GameObject* go)
		{
			if (!go) return NULL;
			const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
			for (size_t i = 0; i < comps.size(); i++)
				if (NetworkIdentity* id = dynamic_cast<NetworkIdentity*>(comps[i].get())) return id;
			return NULL;
		}
	}

	struct NetworkSession::Entity
	{
		uint32 netId = 0;
		GameObject* go = NULL;
		NetworkIdentity* identity = NULL;
		std::shared_ptr<GameObject> spawned;	// held when this session built it
		std::shared_ptr<LoadedSceneAssets> assets;
		std::string prefab;			// spawned from, or empty
		std::string scenePath;		// bound by, or empty
	};

	struct NetworkSession::ClientState
	{
		PeerId peer = 0;
		Vec3 viewer;
		bool heardFrom = false;
		struct Known
		{
			bool acked = false;
			Vec3 position;
			Quaternion rotation;
			std::map<std::string, uint32> varVersions;
			f32 accumulator = 0.f;
		};
		std::map<uint32, Known> known;
		struct Sent { uint32 netId; uint8 mask; Vec3 position; Quaternion rotation; std::map<std::string, uint32> varVersions; };
		std::map<uint32, std::vector<Sent> > inflight;	// tick -> what that snapshot carried
		std::map<std::string, uint32> varIds;			// names this client has been told
		uint32 nextVarId = 1;
		// Prediction: last command applied per object, what the owner was
		// told last, the view tick of its latest command, and how many more
		// commands it may send this tick (so a client cannot run faster).
		std::map<uint32, uint32> lastSeq, sentSeq;
		f64 viewTick = 0.0;
		f32 commandCredit = 0.f;
	};

	NetworkSession::NetworkSession(SceneGraph* scene, const std::string &scenePath, IPhysics* physics, sol::state* lua)
		: scene(scene), scenePath(scenePath), physics(physics), lua(lua) {}

	NetworkSession::~NetworkSession()
	{
		Shutdown();
	}

	bool NetworkSession::Host(const uint16 port, const Settings &s)
	{
		if (role != Offline) return false;
		settings = s;
		if (!transport.Host(port, s.maxClients)) return false;
		role = Server;
		localPeer = 0;
		welcomed = true;
		serverTick = 0.0;
		return true;
	}

	bool NetworkSession::Connect(const std::string &address, const uint16 port, const Settings &s)
	{
		if (role != Offline) return false;
		settings = s;
		if (!transport.Connect(address, port)) return false;
		role = Client;
		welcomed = false;
		return true;
	}

	void NetworkSession::Shutdown()
	{
		transport.Shutdown();
		std::vector<uint32> ids;
		for (std::map<uint32, std::unique_ptr<Entity> >::iterator it = entities.begin(); it != entities.end(); ++it) ids.push_back(it->first);
		for (size_t i = 0; i < ids.size(); i++) RemoveEntity(ids[i]);
		clients.clear();
		role = Offline;
		welcomed = false;
	}

	bool NetworkSession::IsReady() const
	{
		return role == Server || (role == Client && welcomed);
	}

	GameObject* NetworkSession::Find(const uint32 netId) const
	{
		std::map<uint32, std::unique_ptr<Entity> >::const_iterator it = entities.find(netId);
		return it == entities.end() ? NULL : it->second->go;
	}

	std::shared_ptr<GameObject> NetworkSession::LoadPrefab(const std::string &prefab)
	{
		if (prefabLoader) return prefabLoader(prefab);
		std::map<std::string, std::string>::iterator cached = prefabText.find(prefab);
		if (cached == prefabText.end())
		{
			// Resolved the way scene asset paths are: relative to the project
			// the scene lives in.
			std::filesystem::path sp(scenePath);
			std::filesystem::path root = sp.parent_path().filename() == "scenes" ? sp.parent_path().parent_path() : sp.parent_path();
			std::ifstream in((root / prefab).string().c_str());
			if (!in.is_open()) in.open(prefab.c_str());
			if (!in.is_open()) { echo("ERROR: NetworkSession - prefab not found: " + prefab); return nullptr; }
			std::stringstream ss;
			ss << in.rdbuf();
			cached = prefabText.insert(std::make_pair(prefab, ss.str())).first;
		}
		return SceneSerializer::DeserializeSubtree(cached->second, scenePath, physics, lua, NULL);
	}

	std::shared_ptr<GameObject> NetworkSession::Spawn(const std::string &prefab, const Vec3 &position, const Vec3 &rotation, const PeerId owner)
	{
		if (role != Server) return nullptr;
		std::shared_ptr<GameObject> go = LoadPrefab(prefab);
		if (!go) return nullptr;
		NetworkIdentity* id = IdentityOf(go.get());
		if (!id)
		{
			std::shared_ptr<NetworkIdentity> added = std::make_shared<NetworkIdentity>();
			go->AddComponent(added);
			id = added.get();
		}
		id->prefab = prefab;
		id->owner = owner;
		id->netId = nextNetId++;
		go->SetPosition(position);
		go->SetRotation(rotation);
		std::unique_ptr<Entity> e(new Entity());
		e->netId = id->netId;
		e->go = go.get();
		e->identity = id;
		e->spawned = go;
		e->prefab = prefab;
		entities[e->netId] = std::move(e);
		scene->Add(go);
		return go;
	}

	void NetworkSession::Destroy(GameObject* go)
	{
		if (role != Server || !go) return;
		NetworkIdentity* id = IdentityOf(go);
		if (!id || !id->netId) return;
		const uint32 netId = id->netId;
		for (std::map<PeerId, std::unique_ptr<ClientState> >::iterator c = clients.begin(); c != clients.end(); ++c)
			if (c->second->known.count(netId)) SendDespawn(*c->second, netId);
		RemoveEntity(netId);
	}

	void NetworkSession::RemoveEntity(const uint32 netId)
	{
		std::map<uint32, std::unique_ptr<Entity> >::iterator it = entities.find(netId);
		if (it == entities.end()) return;
		Entity &e = *it->second;
		if (e.spawned && scene) scene->Remove(e.spawned);
		entities.erase(it);
	}

	void NetworkSession::RegisterNewIdentities()
	{
		// Scene-authored objects - and whatever a streamed cell brings - are
		// found as they register, and bound by their path.
		const std::vector<NetworkIdentity*> &all = NetworkIdentity::All();
		for (size_t i = 0; i < all.size(); i++)
		{
			NetworkIdentity* id = all[i];
			if (id->netId != 0 || id->replica) continue;
			GameObject* go = id->GetOwner();
			if (!go) continue;
			id->netId = nextNetId++;
			std::unique_ptr<Entity> e(new Entity());
			e->netId = id->netId;
			e->go = go;
			e->identity = id;
			e->scenePath = NetworkIdentity::ScenePath(go);
			entities[e->netId] = std::move(e);
		}
		// Bound objects that left the scene (a streamed cell unloading).
		std::vector<uint32> gone;
		for (std::map<uint32, std::unique_ptr<Entity> >::iterator it = entities.begin(); it != entities.end(); ++it)
		{
			const std::vector<NetworkIdentity*>::const_iterator f = std::find(all.begin(), all.end(), it->second->identity);
			if (!it->second->spawned && f == all.end()) gone.push_back(it->first);
		}
		for (size_t i = 0; i < gone.size(); i++)
		{
			for (std::map<PeerId, std::unique_ptr<ClientState> >::iterator c = clients.begin(); c != clients.end(); ++c)
				if (c->second->known.count(gone[i])) SendDespawn(*c->second, gone[i]);
			RemoveEntity(gone[i]);
		}
	}

	uint32 NetworkSession::VarId(ClientState &c, const std::string &name)
	{
		std::map<std::string, uint32>::iterator it = c.varIds.find(name);
		if (it != c.varIds.end()) return it->second;
		const uint32 id = c.nextVarId++;
		c.varIds[name] = id;
		NetWriter w;
		w.U8(Msg::VarName); w.VarU32(id); w.String(name);
		transport.Send(c.peer, NetChannel::Reliable, w.data.data(), w.Size());
		return id;
	}

	void NetworkSession::SendSpawn(ClientState &c, Entity &e)
	{
		NetWriter w;
		w.U8(Msg::Spawn);
		w.VarU32(e.netId);
		w.VarU32(e.identity->owner);
		w.Bool(!e.prefab.empty());
		w.String(!e.prefab.empty() ? e.prefab : e.scenePath);
		w.Position(e.go->GetPosition(), settings.quantization);
		w.Rotation(ToQuat(e.go->GetRotation()));
		const std::map<std::string, NetValue> &vars = e.identity->GetVars();
		w.VarU32((uint32)vars.size());
		for (std::map<std::string, NetValue>::const_iterator v = vars.begin(); v != vars.end(); ++v)
		{
			w.VarU32(VarId(c, v->first));
			WriteValue(w, v->second);
		}
		transport.Send(c.peer, NetChannel::Reliable, w.data.data(), w.Size());
		// A spawn carries the full state, reliably: that is its ack.
		ClientState::Known k;
		k.acked = true;
		k.position = e.go->GetPosition();
		k.rotation = ToQuat(e.go->GetRotation());
		k.varVersions = e.identity->varVersion;
		c.known[e.netId] = k;
	}

	void NetworkSession::SendDespawn(ClientState &c, const uint32 netId)
	{
		NetWriter w;
		w.U8(Msg::Despawn);
		w.VarU32(netId);
		transport.Send(c.peer, NetChannel::Reliable, w.data.data(), w.Size());
		c.known.erase(netId);
	}

	void NetworkSession::Rpc(const PeerId target, const std::string &name, const std::vector<NetValue> &args)
	{
		NetWriter w;
		w.U8(Msg::Rpc);
		w.String(name);
		w.VarU32((uint32)args.size());
		for (size_t i = 0; i < args.size(); i++) WriteValue(w, args[i]);
		if (role == Client) transport.Send(1, NetChannel::Reliable, w.data.data(), w.Size());
		else if (role == Server)
		{
			if (target == 0) transport.Broadcast(NetChannel::Reliable, w.data.data(), w.Size());
			else transport.Send(target, NetChannel::Reliable, w.data.data(), w.Size());
		}
	}

	void NetworkSession::Update(const f64 dt)
	{
		if (role == Offline) return;
		std::vector<NetEvent> events;
		transport.Poll(events);
		for (size_t i = 0; i < events.size(); i++) HandleMessage(events[i]);

		const f64 tickLength = 1.0 / std::max(settings.tickRate, 1.f);
		if (role == Server)
		{
			tickAccumulator += dt;
			// At most a few ticks to catch up after a stall - the same rule
			// as physics, and for the same reason.
			int ran = 0;
			while (tickAccumulator >= tickLength && ran < 3)
			{
				tickAccumulator -= tickLength;
				serverTick += 1.0;
				RunServerTick();
				ran++;
			}
			if (ran == 3) tickAccumulator = 0.0;
		}
		else if (role == Client && welcomed)
		{
			serverTick += dt * settings.tickRate;
			RunClientCommands(dt);
			tickAccumulator += dt;
			if (tickAccumulator >= tickLength)
			{
				tickAccumulator = std::fmod(tickAccumulator, tickLength);
				RunClientTick();
			}
			PoseReplicas();
		}
		transport.Flush();
		stats.replicated = (uint32)entities.size();
	}

	void NetworkSession::RunServerTick()
	{
		RegisterNewIdentities();
		const uint32 tick = (uint32)serverTick;

		// Where everything was, for rewound hit tests.
		const size_t keep = (size_t)std::max(2.f, settings.historySeconds * settings.tickRate) + 1;
		for (std::map<uint32, std::unique_ptr<Entity> >::iterator ei = entities.begin(); ei != entities.end(); ++ei)
		{
			std::vector<NetworkIdentity::Past> &h = ei->second->identity->history;
			NetworkIdentity::Past past;
			past.tick = tick;
			past.position = ei->second->go->GetWorldPosition();
			h.push_back(past);
			if (h.size() > keep) h.erase(h.begin(), h.begin() + (h.size() - keep));
		}
		// Each tick earns a client the commands a tick is worth, plus slack
		// for jitter; a client sending faster than that is dropped to it.
		const f32 perTick = settings.commandRate / std::max(settings.tickRate, 1.f);
		for (std::map<PeerId, std::unique_ptr<ClientState> >::iterator ci = clients.begin(); ci != clients.end(); ++ci)
			ci->second->commandCredit = std::min(ci->second->commandCredit + perTick, perTick * 4.f + 2.f);
		stats.lastSnapshotBytes = 0;
		stats.lastSnapshotEntities = 0;

		for (std::map<PeerId, std::unique_ptr<ClientState> >::iterator ci = clients.begin(); ci != clients.end(); ++ci)
		{
			ClientState &c = *ci->second;
			if (!c.heardFrom) continue;	// no viewer yet: wait for its first ack

			// Relevance.
			for (std::map<uint32, std::unique_ptr<Entity> >::iterator ei = entities.begin(); ei != entities.end(); ++ei)
			{
				Entity &e = *ei->second;
				const f32 radius = e.identity->relevance > 0.f ? e.identity->relevance : settings.defaultRelevance;
				const f32 d = e.go->GetWorldPosition().distance(c.viewer);
				const bool known = c.known.count(e.netId) != 0;
				const bool mine = e.identity->owner == c.peer;
				if (!known && (mine || d <= radius)) SendSpawn(c, e);
				else if (known && !mine && d > radius + settings.relevanceHysteresis) SendDespawn(c, e.netId);
			}

			// What changed since what this client acknowledged.
			struct Candidate { uint32 netId; f32 score; };
			std::vector<Candidate> candidates;
			for (std::map<uint32, ClientState::Known>::iterator k = c.known.begin(); k != c.known.end(); ++k)
			{
				std::map<uint32, std::unique_ptr<Entity> >::iterator ei = entities.find(k->first);
				if (ei == entities.end()) continue;
				Entity &e = *ei->second;
				// The owner moves its own object, so its transform is never
				// echoed back - except a predicted one's authoritative state,
				// once per command the server processed.
				const bool mine = e.identity->owner == c.peer;
				const Vec3 pos = e.go->GetPosition();
				const Quaternion rot = ToQuat(e.go->GetRotation());
				bool dirty = !k->second.acked && !mine;
				if (!dirty && e.identity->syncTransform && !mine) dirty = Moved(pos, k->second.position) || Turned(rot, k->second.rotation);
				if (!dirty && mine && e.identity->predicted) dirty = c.lastSeq[e.netId] != c.sentSeq[e.netId];
				if (!dirty)
					for (std::map<std::string, uint32>::const_iterator v = e.identity->varVersion.begin(); v != e.identity->varVersion.end() && !dirty; ++v)
					{
						std::map<std::string, uint32>::const_iterator a = k->second.varVersions.find(v->first);
						dirty = a == k->second.varVersions.end() || a->second < v->second;
					}
				if (!dirty) continue;
				const f32 radius = e.identity->relevance > 0.f ? e.identity->relevance : settings.defaultRelevance;
				const f32 near = 1.f - 0.9f * std::min(1.f, e.go->GetWorldPosition().distance(c.viewer) / radius);
				k->second.accumulator += e.identity->priority * near;
				Candidate cd; cd.netId = k->first; cd.score = k->second.accumulator;
				candidates.push_back(cd);
			}
			std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) { return a.score > b.score; });

			NetWriter w;
			w.U8(Msg::Snapshot);
			w.VarU32(tick);
			NetWriter body;
			uint32 count = 0;
			std::vector<ClientState::Sent> sent;
			for (size_t i = 0; i < candidates.size(); i++)
			{
				Entity &e = *entities[candidates[i].netId];
				ClientState::Known &k = c.known[e.netId];
				NetWriter entry;
				entry.VarU32(e.netId);
				const Vec3 pos = e.go->GetPosition();
				const Quaternion rot = ToQuat(e.go->GetRotation());
				uint8 mask = 0;
				const bool mine = e.identity->owner == c.peer;
				if (!mine && e.identity->syncTransform && (!k.acked || Moved(pos, k.position))) mask |= Mask::Position;
				if (!mine && e.identity->syncTransform && (!k.acked || Turned(rot, k.rotation))) mask |= Mask::Rotation;
				if (mine && e.identity->predicted && c.lastSeq[e.netId] != c.sentSeq[e.netId]) mask |= Mask::Predict;
				std::vector<std::string> changedVars;
				for (std::map<std::string, uint32>::const_iterator v = e.identity->varVersion.begin(); v != e.identity->varVersion.end(); ++v)
				{
					std::map<std::string, uint32>::const_iterator a = k.varVersions.find(v->first);
					if (a == k.varVersions.end() || a->second < v->second) changedVars.push_back(v->first);
				}
				if (!changedVars.empty()) mask |= Mask::Vars;
				if (mask == 0 && changedVars.empty()) continue;
				entry.U8(mask);
				if (mask & Mask::Position) entry.Position(pos, settings.quantization);
				if (mask & Mask::Rotation) entry.Rotation(rot);
				if (mask & Mask::Predict)
				{
					entry.VarU32(c.lastSeq[e.netId]);
					entry.Position(pos, settings.quantization);
					entry.Rotation(rot);
				}
				if (mask & Mask::Vars)
				{
					entry.VarU32((uint32)changedVars.size());
					for (size_t v = 0; v < changedVars.size(); v++)
					{
						NetValue value;
						e.identity->GetVar(changedVars[v], value);
						entry.VarU32(VarId(c, changedVars[v]));
						WriteValue(entry, value);
					}
				}
				// Over budget: this one waits, and gets more priority for it.
				if (w.Size() + body.Size() + entry.Size() + 5 > settings.bytesPerTick) break;
				body.Bytes(entry.data.data(), entry.Size());
				count++;
				k.accumulator = 0.f;
				// Unreliable, but the next processed command resends it anyway.
				if (mask & Mask::Predict) c.sentSeq[e.netId] = c.lastSeq[e.netId];
				ClientState::Sent s;
				s.netId = e.netId; s.mask = mask; s.position = pos; s.rotation = rot;
				for (size_t v = 0; v < changedVars.size(); v++) s.varVersions[changedVars[v]] = e.identity->varVersion[changedVars[v]];
				sent.push_back(s);
			}
			if (count == 0) continue;
			w.VarU32(count);
			w.Bytes(body.data.data(), body.Size());
			transport.Send(c.peer, NetChannel::Snapshot, w.data.data(), w.Size());
			c.inflight[tick] = sent;
			while (!c.inflight.empty() && c.inflight.begin()->first + kMaxInflightTicks < tick) c.inflight.erase(c.inflight.begin());
			stats.lastSnapshotBytes = std::max(stats.lastSnapshotBytes, (uint32)w.Size());
			stats.lastSnapshotEntities = std::max(stats.lastSnapshotEntities, count);
		}
	}

	void NetworkSession::RunClientTick()
	{
		NetWriter w;
		w.U8(Msg::Ack);
		w.VarU32((uint32)latestSnapshotTick);
		w.Position(viewer, settings.quantization);
		std::vector<Entity*> owned;
		for (std::map<uint32, std::unique_ptr<Entity> >::iterator it = entities.begin(); it != entities.end(); ++it)
			if (it->second->identity && it->second->identity->owner == localPeer && !it->second->identity->predicted) owned.push_back(it->second.get());
		w.VarU32((uint32)owned.size());
		for (size_t i = 0; i < owned.size(); i++)
		{
			w.VarU32(owned[i]->netId);
			w.Position(owned[i]->go->GetPosition(), settings.quantization);
			w.Rotation(ToQuat(owned[i]->go->GetRotation()));
		}
		transport.Send(1, NetChannel::Unsequenced, w.data.data(), w.Size());
	}

	void NetworkSession::HandleMessage(const NetEvent &e)
	{
		if (e.type == NetEvent::Connected)
		{
			if (role == Server)
			{
				std::unique_ptr<ClientState> c(new ClientState());
				c->peer = e.peer;
				clients[e.peer] = std::move(c);
				NetWriter w;
				w.U8(Msg::Welcome);
				w.VarU32(e.peer);
				w.F32(settings.tickRate);
				w.VarU32((uint32)serverTick);
				transport.Send(e.peer, NetChannel::Reliable, w.data.data(), w.Size());
				if (onPeerJoined) onPeerJoined(e.peer);
			}
			return;
		}
		if (e.type == NetEvent::Disconnected)
		{
			if (role == Server)
			{
				clients.erase(e.peer);
				if (onPeerLeft) onPeerLeft(e.peer);
			}
			else
			{
				welcomed = false;
				if (onPeerLeft) onPeerLeft(e.peer);
			}
			return;
		}
		NetReader r(e.data);
		const uint8 type = r.U8();
		if (role == Server) HandleServerMessage(e.peer, r, type);
		else HandleClientMessage(r, type);
	}

	void NetworkSession::HandleServerMessage(const PeerId from, NetReader &r, const uint8 type)
	{
		std::map<PeerId, std::unique_ptr<ClientState> >::iterator ci = clients.find(from);
		if (ci == clients.end()) return;
		ClientState &c = *ci->second;
		if (type == Msg::Ack)
		{
			const uint32 tick = r.VarU32();
			const Vec3 v = r.Position(settings.quantization);
			if (!r.Ok()) return;
			c.viewer = v;
			c.heardFrom = true;
			// Everything that snapshot carried is now known to have arrived.
			std::map<uint32, std::vector<ClientState::Sent> >::iterator it = c.inflight.find(tick);
			if (it != c.inflight.end())
				for (size_t i = 0; i < it->second.size(); i++)
				{
					const ClientState::Sent &s = it->second[i];
					std::map<uint32, ClientState::Known>::iterator k = c.known.find(s.netId);
					if (k == c.known.end()) continue;
					k->second.acked = true;
					if (s.mask & Mask::Position) k->second.position = s.position;
					if (s.mask & Mask::Rotation) k->second.rotation = s.rotation;
					for (std::map<std::string, uint32>::const_iterator v2 = s.varVersions.begin(); v2 != s.varVersions.end(); ++v2)
						k->second.varVersions[v2->first] = std::max(k->second.varVersions[v2->first], v2->second);
				}
			while (!c.inflight.empty() && c.inflight.begin()->first <= tick) c.inflight.erase(c.inflight.begin());
			// Objects the client owns: it moves them (client authority).
			const uint32 n = r.VarU32();
			for (uint32 i = 0; i < n && r.Ok(); i++)
			{
				const uint32 netId = r.VarU32();
				const Vec3 p = r.Position(settings.quantization);
				Quaternion q = r.Rotation();
				if (!r.Ok()) break;
				std::map<uint32, std::unique_ptr<Entity> >::iterator ei = entities.find(netId);
				if (ei == entities.end() || ei->second->identity->owner != from || ei->second->identity->predicted) continue;
				ei->second->go->SetPosition(p);
				ei->second->go->SetRotation(q.GetEulerFromQuaternion());
			}
		}
		else if (type == Msg::Rpc)
		{
			const std::string name = r.String();
			const uint32 n = r.VarU32();
			std::vector<NetValue> args;
			for (uint32 i = 0; i < n && r.Ok() && i < 64; i++) args.push_back(ReadValue(r));
			if (!r.Ok()) return;
			std::map<std::string, RpcHandler>::iterator h = rpcHandlers.find(name);
			if (h != rpcHandlers.end()) h->second(from, args);
		}
		else if (type == Msg::Commands)
		{
			const uint32 netId = r.VarU32();
			const f64 view = r.U32() / 16.0;
			const uint32 n = r.U8();
			std::map<uint32, std::unique_ptr<Entity> >::iterator ei = entities.find(netId);
			if (ei == entities.end() || ei->second->identity->owner != from || !ei->second->identity->predicted) return;
			c.viewTick = view;
			const f32 dt = 1.f / std::max(settings.commandRate, 1.f);
			for (uint32 i = 0; i < n && r.Ok(); i++)
			{
				const uint32 seq = r.VarU32();
				const uint32 argc = r.U8();
				std::vector<NetValue> input;
				for (uint32 a = 0; a < argc && r.Ok(); a++) input.push_back(ReadValue(r));
				if (!r.Ok()) break;
				// Redundant copies of what was already applied are skipped;
				// what is new is applied in order, as far as credit allows.
				if (seq <= c.lastSeq[netId]) continue;
				if (c.commandCredit < 1.f) break;
				c.commandCredit -= 1.f;
				if (simulate) simulate(ei->second->go, input, dt);
				c.lastSeq[netId] = seq;
			}
		}
	}

	void NetworkSession::HandleClientMessage(NetReader &r, const uint8 type)
	{
		switch (type)
		{
		case Msg::Welcome:
		{
			localPeer = r.VarU32();
			settings.tickRate = r.F32();
			const uint32 tick = r.VarU32();
			if (!r.Ok()) return;
			NetPeerStats st;
			transport.GetStats(1, st);
			serverTick = tick + (st.roundTripMs * 0.0005) * settings.tickRate;
			welcomed = true;
			if (onPeerJoined) onPeerJoined(localPeer);
		}
		break;
		case Msg::VarName:
		{
			const uint32 id = r.VarU32();
			const std::string name = r.String();
			if (r.Ok()) varNames[id] = name;
		}
		break;
		case Msg::Spawn:
		{
			const uint32 netId = r.VarU32();
			const PeerId owner = r.VarU32();
			const bool isPrefab = r.Bool();
			const std::string source = r.String();
			const Vec3 pos = r.Position(settings.quantization);
			Quaternion rot = r.Rotation();
			if (!r.Ok() || entities.count(netId)) return;
			std::unique_ptr<Entity> e(new Entity());
			e->netId = netId;
			if (isPrefab)
			{
				e->spawned = LoadPrefab(source);
				e->go = e->spawned.get();
				e->prefab = source;
			}
			else
			{
				e->go = FindByPath(scene, source);
				e->scenePath = source;
			}
			if (!e->go) { echo("WARNING: NetworkSession - could not build " + source); return; }
			e->identity = IdentityOf(e->go);
			if (!e->identity)
			{
				std::shared_ptr<NetworkIdentity> added = std::make_shared<NetworkIdentity>();
				e->go->AddComponent(added);
				e->identity = added.get();
			}
			e->identity->netId = netId;
			e->identity->owner = owner;
			// The owner moves its own object; everyone else's copy follows
			// the server.
			e->identity->replica = (owner != localPeer);
			e->go->SetPosition(pos);
			e->go->SetRotation(rot.GetEulerFromQuaternion());
			const uint32 nv = r.VarU32();
			for (uint32 i = 0; i < nv && r.Ok(); i++)
			{
				const uint32 id = r.VarU32();
				const NetValue v = ReadValue(r);
				std::map<uint32, std::string>::const_iterator name = varNames.find(id);
				if (name != varNames.end()) e->identity->vars[name->second] = v;
			}
			if (e->spawned) scene->Add(e->spawned);
			entities[netId] = std::move(e);
		}
		break;
		case Msg::Despawn:
		{
			const uint32 netId = r.VarU32();
			if (r.Ok()) RemoveEntity(netId);
		}
		break;
		case Msg::Rpc:
		{
			const std::string name = r.String();
			const uint32 n = r.VarU32();
			std::vector<NetValue> args;
			for (uint32 i = 0; i < n && r.Ok() && i < 64; i++) args.push_back(ReadValue(r));
			if (!r.Ok()) return;
			std::map<std::string, RpcHandler>::iterator h = rpcHandlers.find(name);
			if (h != rpcHandlers.end()) h->second(0, args);
		}
		break;
		case Msg::Snapshot:
		{
			const uint32 tick = r.VarU32();
			const uint32 count = r.VarU32();
			if (!r.Ok()) return;
			if (tick > latestSnapshotTick) latestSnapshotTick = tick;
			// Keep our estimate of the server's clock honest: this tick left
			// the server half a round trip ago.
			NetPeerStats st;
			transport.GetStats(1, st);
			const f64 target = tick + (st.roundTripMs * 0.0005) * settings.tickRate;
			const f64 offset = target - serverTick;
			stats.clockOffsetTicks = offset;
			if (std::fabs(offset) > 5.0) serverTick = target;
			else serverTick += offset * 0.1;

			for (uint32 i = 0; i < count && r.Ok(); i++)
			{
				const uint32 netId = r.VarU32();
				const uint8 mask = r.U8();
				Vec3 pos;
				Quaternion rot;
				if (mask & Mask::Position) pos = r.Position(settings.quantization);
				if (mask & Mask::Rotation) rot = r.Rotation();
				uint32 ackSeq = 0;
				Vec3 predictPos;
				Quaternion predictRot;
				if (mask & Mask::Predict)
				{
					ackSeq = r.VarU32();
					predictPos = r.Position(settings.quantization);
					predictRot = r.Rotation();
				}
				std::vector<std::pair<uint32, NetValue> > vars;
				if (mask & Mask::Vars)
				{
					const uint32 nv = r.VarU32();
					for (uint32 v = 0; v < nv && r.Ok(); v++) { const uint32 id = r.VarU32(); vars.push_back(std::make_pair(id, ReadValue(r))); }
				}
				if (!r.Ok()) break;
				std::map<uint32, std::unique_ptr<Entity> >::iterator ei = entities.find(netId);
				if (ei == entities.end()) continue;	// its spawn has not arrived yet
				NetworkIdentity* id = ei->second->identity;
				for (size_t v = 0; v < vars.size(); v++)
				{
					std::map<uint32, std::string>::const_iterator name = varNames.find(vars[v].first);
					if (name != varNames.end()) id->vars[name->second] = vars[v].second;
				}
				if ((mask & Mask::Predict) && id->predicted && id->owner == localPeer)
					Reconcile(id, ei->second->go, ackSeq, predictPos, predictRot);
				if (!id->replica || !(mask & (Mask::Position | Mask::Rotation))) continue;
				// A sample: whatever this snapshot did not carry is unchanged.
				NetworkIdentity::Sample s;
				s.tick = tick;
				s.position = (mask & Mask::Position) ? pos : (id->samples.empty() ? ei->second->go->GetPosition() : id->samples.back().position);
				s.rotation = (mask & Mask::Rotation) ? rot : (id->samples.empty() ? ToQuat(ei->second->go->GetRotation()) : id->samples.back().rotation);
				// In order; a late one (reordered) slots in where it belongs.
				std::vector<NetworkIdentity::Sample>::iterator at = id->samples.end();
				while (at != id->samples.begin() && (at - 1)->tick > s.tick) --at;
				if (at != id->samples.begin() && (at - 1)->tick == s.tick) continue;
				id->samples.insert(at, s);
			}
		}
		break;
		default:
			break;
		}
	}

	void NetworkSession::RunClientCommands(const f64 dt)
	{
		std::vector<Entity*> predicted;
		for (std::map<uint32, std::unique_ptr<Entity> >::iterator it = entities.begin(); it != entities.end(); ++it)
			if (it->second->identity->predicted && it->second->identity->owner == localPeer) predicted.push_back(it->second.get());
		if (predicted.empty() || !simulate) { commandAccumulator = 0.0; return; }

		const f64 step = 1.0 / std::max(settings.commandRate, 1.f);
		commandAccumulator += dt;
		int made = 0;
		while (commandAccumulator >= step && made < 8)
		{
			commandAccumulator -= step;
			made++;
			const uint32 seq = nextCommandSeq++;
			for (size_t i = 0; i < predicted.size(); i++)
			{
				NetworkIdentity* id = predicted[i]->identity;
				// Ahead of the server: the player sees the move now.
				simulate(predicted[i]->go, currentInput, (f32)step);
				NetworkIdentity::Command cmd;
				cmd.seq = seq;
				cmd.input = currentInput;
				id->pending.push_back(cmd);
				if (id->pending.size() > kMaxPendingCommands) id->pending.erase(id->pending.begin());

				NetWriter w;
				w.U8(Msg::Commands);
				w.VarU32(predicted[i]->netId);
				// What this client is looking at, for rewound hit tests.
				const f64 view = ViewTick();
				w.U32((uint32)(view * 16.0));
				const size_t first = id->pending.size() > kCommandRedundancy ? id->pending.size() - kCommandRedundancy : 0;
				w.U8((uint8)(id->pending.size() - first));
				for (size_t c = first; c < id->pending.size(); c++)
				{
					w.VarU32(id->pending[c].seq);
					w.U8((uint8)std::min<size_t>(id->pending[c].input.size(), 255));
					for (size_t a = 0; a < id->pending[c].input.size() && a < 255; a++) WriteValue(w, id->pending[c].input[a]);
				}
				transport.Send(1, NetChannel::Unsequenced, w.data.data(), w.Size());
			}
		}
		if (made == 8) commandAccumulator = 0.0;
	}

	void NetworkSession::Reconcile(NetworkIdentity* id, GameObject* go, const uint32 ackSeq, const Vec3 &position, const Quaternion &rotation)
	{
		// The server's result for command ackSeq; the ones after it are
		// replayed on top, so the prediction stays ahead from the right
		// starting point. When client and server agree - the usual case -
		// this lands exactly where the object already was.
		while (!id->pending.empty() && id->pending.front().seq <= ackSeq) id->pending.erase(id->pending.begin());
		Quaternion q = rotation;
		go->SetPosition(position);
		go->SetRotation(q.GetEulerFromQuaternion());
		const f32 dt = 1.f / std::max(settings.commandRate, 1.f);
		for (size_t i = 0; i < id->pending.size(); i++)
			if (simulate) simulate(go, id->pending[i].input, dt);
	}

	f64 NetworkSession::ViewTickOf(const PeerId peer) const
	{
		std::map<PeerId, std::unique_ptr<ClientState> >::const_iterator it = clients.find(peer);
		return it == clients.end() ? serverTick : it->second->viewTick;
	}

	std::vector<Vec3> NetworkSession::ClientViewers() const
	{
		std::vector<Vec3> v;
		for (std::map<PeerId, std::unique_ptr<ClientState> >::const_iterator it = clients.begin(); it != clients.end(); ++it)
			if (it->second->heardFrom) v.push_back(it->second->viewer);
		return v;
	}

	f64 NetworkSession::ViewTick() const
	{
		return std::max(0.0, serverTick - settings.interpolationDelay * settings.tickRate);
	}

	NetworkSession::RayHit NetworkSession::RaycastRewound(const PeerId shooter, const Vec3 &origin, const Vec3 &direction, const f32 maxDistance,
		const f64 viewTick) const
	{
		RayHit best;
		const f32 len = direction.magnitude();
		if (len <= 0.f) return best;
		const Vec3 d = direction * (1.f / len);
		f64 view = viewTick >= 0.0 ? viewTick : (shooter == 0 ? serverTick : ViewTickOf(shooter));
		// A client may not claim to see further back than history, nor the
		// future.
		view = std::min(serverTick, std::max(view, serverTick - settings.historySeconds * settings.tickRate));
		f32 bestT = maxDistance;
		for (std::map<uint32, std::unique_ptr<Entity> >::const_iterator it = entities.begin(); it != entities.end(); ++it)
		{
			const NetworkIdentity* id = it->second->identity;
			if (shooter != 0 && id->owner == shooter) continue;
			if (id->hitRadius <= 0.f) continue;
			// Where it was at the view tick, between the two recorded ticks
			// around it; the present if history does not reach that far.
			Vec3 p = it->second->go->GetWorldPosition();
			const std::vector<NetworkIdentity::Past> &h = id->history;
			for (size_t i = 0; i + 1 < h.size(); i++)
				if (h[i].tick <= view && h[i + 1].tick >= view)
				{
					const f32 t = h[i + 1].tick > h[i].tick ? (f32)((view - h[i].tick) / (f64)(h[i + 1].tick - h[i].tick)) : 0.f;
					p = h[i].position + (h[i + 1].position - h[i].position) * t;
					break;
				}
			// Closest point on the capsule's axis to the ray, then a sphere
			// of hitRadius there: exact for a sphere, close for a capsule.
			const Vec3 a = p, b = p + Vec3(0.f, id->hitHeight, 0.f);
			const Vec3 ab = b - a;
			const f32 abLen2 = ab.dotProduct(ab);
			f32 s = 0.f;
			if (abLen2 > 0.f)
			{
				// Segment parameter of the closest approach between the ray
				// (origin + d*t) and the axis (a + ab*s).
				const Vec3 w0 = origin - a;
				const f32 bdot = d.dotProduct(ab), dw = d.dotProduct(w0), abw = ab.dotProduct(w0);
				const f32 denom = abLen2 - bdot * bdot;
				s = denom > 1e-6f ? (abw - bdot * dw) / denom : 0.f;
				s = std::min(std::max(s, 0.f), 1.f);
			}
			const Vec3 c = a + ab * s;
			const Vec3 oc = origin - c;
			const f32 bq = oc.dotProduct(d);
			const f32 cq = oc.dotProduct(oc) - id->hitRadius * id->hitRadius;
			const f32 disc = bq * bq - cq;
			if (disc < 0.f) continue;
			const f32 t = -bq - std::sqrt(disc);
			if (t < 0.f || t > bestT) continue;
			bestT = t;
			best.netId = it->first;
			best.distance = t;
			best.point = origin + d * t;
		}
		return best;
	}

	void NetworkSession::PoseReplicas()
	{
		const f64 renderTick = serverTick - settings.interpolationDelay * settings.tickRate;
		for (std::map<uint32, std::unique_ptr<Entity> >::iterator it = entities.begin(); it != entities.end(); ++it)
		{
			NetworkIdentity* id = it->second->identity;
			if (!id || !id->replica || id->samples.empty()) continue;
			std::vector<NetworkIdentity::Sample> &s = id->samples;
			// Drop what is behind the pair we need.
			while (s.size() >= 2 && s[1].tick <= renderTick) s.erase(s.begin());
			Vec3 p;
			Quaternion q;
			if (s.size() >= 2 && s[0].tick <= renderTick)
			{
				const f32 t = (f32)((renderTick - s[0].tick) / (s[1].tick - s[0].tick));
				p = s[0].position + (s[1].position - s[0].position) * t;
				q = s[0].rotation.Slerp(s[1].rotation, t);
			}
			else
			{
				// Only newer samples, or only one: hold the nearest.
				p = s[0].position;
				q = s[0].rotation;
			}
			it->second->go->SetPosition(p);
			it->second->go->SetRotation(q.GetEulerFromQuaternion());
		}
	}

}
