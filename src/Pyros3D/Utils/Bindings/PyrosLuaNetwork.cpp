//============================================================================
// Name        : PyrosLuaNetwork.cpp
// Description : See PyrosLuaNetwork.h.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaNetwork.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/Network/NetworkSession.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Core/Logs/Log.h>

namespace p3d {

	namespace {
		NetworkIdentity* IdentityOf(GameObject* go)
		{
			if (!go) return NULL;
			const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
			for (size_t i = 0; i < comps.size(); i++)
				if (NetworkIdentity* id = dynamic_cast<NetworkIdentity*>(comps[i].get())) return id;
			return NULL;
		}

		NetValue FromLua(const sol::object &o)
		{
			switch (o.get_type())
			{
			case sol::type::number: return NetValue::FromNumber(o.as<f64>());
			case sol::type::boolean: return NetValue::FromBool(o.as<bool>());
			case sol::type::string: return NetValue::FromString(o.as<std::string>());
			case sol::type::userdata:
				if (o.is<Vec3>()) return NetValue::FromVector(o.as<Vec3>());
				break;
			default: break;
			}
			return NetValue();
		}

		sol::object ToLua(sol::state_view lua, const NetValue &v)
		{
			switch (v.type)
			{
			case NetValue::Number: return sol::make_object(lua, v.number);
			case NetValue::Bool: return sol::make_object(lua, v.boolean);
			case NetValue::String: return sol::make_object(lua, v.text);
			case NetValue::Vector: return sol::make_object(lua, v.vector);
			default: return sol::make_object(lua, sol::lua_nil);
			}
		}

		NetworkSettings SettingsFrom(sol::optional<sol::table> t)
		{
			NetworkSettings s;
			if (!t) return s;
			const sol::table &x = *t;
			s.tickRate = x.get_or("tickRate", s.tickRate);
			s.defaultRelevance = x.get_or("relevance", s.defaultRelevance);
			s.relevanceHysteresis = x.get_or("hysteresis", s.relevanceHysteresis);
			s.bytesPerTick = x.get_or("bytesPerTick", s.bytesPerTick);
			s.interpolationDelay = x.get_or("interpolationDelay", s.interpolationDelay);
			s.maxClients = x.get_or("maxClients", s.maxClients);
			s.password = x.get_or("password", s.password);
			s.reconnectGrace = x.get_or("reconnectGrace", s.reconnectGrace);
			s.autoReconnect = x.get_or("autoReconnect", s.autoReconnect);
			s.maxClientSpeed = x.get_or("maxClientSpeed", s.maxClientSpeed);
			return s;
		}

		// A script error inside a callback is reported, never thrown into
		// the session's message loop.
		void Call(const sol::protected_function &fn, const char* what, const PeerId peer, const std::vector<NetValue>* args, sol::state* lua)
		{
			sol::protected_function_result r;
			if (args)
			{
				std::vector<sol::object> luaArgs;
				for (size_t i = 0; i < args->size(); i++) luaArgs.push_back(ToLua(*lua, (*args)[i]));
				r = fn(peer, sol::as_args(luaArgs));
			}
			else r = fn(peer);
			if (!r.valid())
			{
				sol::error err = r;
				echo(std::string("ERROR: network ") + what + " - " + err.what());
			}
		}
	}

	void RegisterLuaNetwork(sol::state* lua, const std::function<NetworkSession*()> &session)
	{
		sol::table net = lua->create_named_table("network");
		net.set_function("available", []() { return NetTransport::Available(); });
		net.set_function("host", [session](const uint16 port, sol::optional<sol::table> settings) {
			NetworkSession* s = session();
			return s && s->Host(port, SettingsFrom(settings));
		});
		net.set_function("connect", [session](const std::string &address, const uint16 port, sol::optional<sol::table> settings) {
			NetworkSession* s = session();
			return s && s->Connect(address, port, SettingsFrom(settings));
		});
		net.set_function("shutdown", [session]() { if (NetworkSession* s = session()) s->Shutdown(); });
		net.set_function("isServer", [session]() { NetworkSession* s = session(); return s && s->GetRole() == NetworkSession::Server; });
		net.set_function("isClient", [session]() { NetworkSession* s = session(); return s && s->GetRole() == NetworkSession::Client; });
		net.set_function("isReady", [session]() { NetworkSession* s = session(); return s && s->IsReady(); });
		net.set_function("localPeer", [session]() { NetworkSession* s = session(); return s ? s->LocalPeer() : 0u; });

		net.set_function("spawn", [session](const std::string &prefab, const Vec3 &position, sol::optional<Vec3> rotation, sol::optional<uint32> owner) -> GameObject* {
			NetworkSession* s = session();
			if (!s) return NULL;
			std::shared_ptr<GameObject> go = s->Spawn(prefab, position, rotation.value_or(Vec3()), owner.value_or(0));
			return go.get();
		});
		net.set_function("destroy", [session](GameObject* go) { if (NetworkSession* s = session()) s->Destroy(go); });
		net.set_function("find", [session](const uint32 netId) -> GameObject* { NetworkSession* s = session(); return s ? s->Find(netId) : NULL; });
		net.set_function("netId", [](GameObject* go) { NetworkIdentity* id = IdentityOf(go); return id ? id->GetNetId() : 0u; });
		net.set_function("owner", [](GameObject* go) { NetworkIdentity* id = IdentityOf(go); return id ? id->GetOwnerPeer() : 0u; });
		net.set_function("isMine", [session](GameObject* go) {
			NetworkSession* s = session();
			NetworkIdentity* id = IdentityOf(go);
			return s && id && id->GetOwnerPeer() == s->LocalPeer();
		});
		net.set_function("setVar", [](GameObject* go, const std::string &name, sol::object value) {
			if (NetworkIdentity* id = IdentityOf(go)) id->SetVar(name, FromLua(value));
		});
		net.set_function("getVar", [lua](GameObject* go, const std::string &name) -> sol::object {
			NetworkIdentity* id = IdentityOf(go);
			NetValue v;
			if (!id || !id->GetVar(name, v)) return sol::make_object(*lua, sol::lua_nil);
			return ToLua(*lua, v);
		});
		net.set_function("setViewer", [session](const Vec3 &p) { if (NetworkSession* s = session()) s->SetViewer(p); });
		net.set_function("rpc", [session](const uint32 target, const std::string &name, sol::variadic_args va) {
			NetworkSession* s = session();
			if (!s) return;
			std::vector<NetValue> args;
			for (auto v : va) args.push_back(FromLua(v.as<sol::object>()));	// explicit: MSVC will not convert a stack_proxy implicitly
			s->Rpc(target, name, args);
		});
		net.set_function("on", [session, lua](const std::string &name, sol::protected_function fn) {
			NetworkSession* s = session();
			if (!s) return;
			s->OnRpc(name, [fn, lua, name](const PeerId sender, const std::vector<NetValue> &args) {
				Call(fn, ("rpc '" + name + "'").c_str(), sender, &args, lua);
			});
		});
		net.set_function("onPeerJoined", [session, lua](sol::protected_function fn) {
			if (NetworkSession* s = session()) s->onPeerJoined = [fn, lua](const PeerId p) { Call(fn, "onPeerJoined", p, NULL, lua); };
		});
		net.set_function("onPeerLeft", [session, lua](sol::protected_function fn) {
			if (NetworkSession* s = session()) s->onPeerLeft = [fn, lua](const PeerId p) { Call(fn, "onPeerLeft", p, NULL, lua); };
		});
		// Reconnection and refusal - see NetworkSession. A script error in
		// one is reported, like the others.
		net.set_function("onPeerDropped", [session, lua](sol::protected_function fn) {
			if (NetworkSession* s = session()) s->onPeerDropped = [fn, lua](const PeerId p) { Call(fn, "onPeerDropped", p, NULL, lua); };
		});
		net.set_function("onPeerRejoined", [session](sol::protected_function fn) {
			if (NetworkSession* s = session()) s->onPeerRejoined = [fn](const PeerId now, const PeerId was) {
				sol::protected_function_result r = fn(now, was);
				if (!r.valid()) { sol::error e = r; echo(std::string("ERROR: network onPeerRejoined - ") + e.what()); }
			};
		});
		net.set_function("onRejected", [session](sol::protected_function fn) {
			if (NetworkSession* s = session()) s->onRejected = [fn](const std::string &reason) {
				sol::protected_function_result r = fn(reason);
				if (!r.valid()) { sol::error e = r; echo(std::string("ERROR: network onRejected - ") + e.what()); }
			};
		});
		net.set_function("onSuspicious", [session](sol::protected_function fn) {
			if (NetworkSession* s = session()) s->onSuspicious = [fn](const PeerId p, const uint32 netId) {
				sol::protected_function_result r = fn(p, netId);
				if (!r.valid()) { sol::error e = r; echo(std::string("ERROR: network onSuspicious - ") + e.what()); }
			};
		});
		net.set_function("kick", [session](const PeerId p, sol::optional<std::string> reason) {
			if (NetworkSession* s = session()) s->Kick(p, reason.value_or("kicked"));
		});
		net.set_function("ban", [session](const PeerId p, sol::optional<std::string> reason) {
			if (NetworkSession* s = session()) s->Ban(p, reason.value_or("banned"));
		});
		net.set_function("unban", [session](const std::string &address) { if (NetworkSession* s = session()) s->Unban(address); });
		net.set_function("peerAddress", [session](const PeerId p) { NetworkSession* s = session(); return s ? s->PeerAddress(p) : std::string(); });
		net.set_function("lastError", [session]() { NetworkSession* s = session(); return s ? s->LastError() : std::string(); });
		net.set_function("isReconnecting", [session]() { NetworkSession* s = session(); return s && s->IsReconnecting(); });
		// Prediction: fn(go, input) with input a table of the values given
		// to network.input, and dt.
		net.set_function("setSimulate", [session, lua](sol::protected_function fn) {
			NetworkSession* s = session();
			if (!s) return;
			s->SetSimulate([fn, lua](GameObject* go, const std::vector<NetValue> &input, const f32 dt) {
				sol::table t = lua->create_table();
				for (size_t i = 0; i < input.size(); i++) t[i + 1] = ToLua(*lua, input[i]);
				sol::protected_function_result r = fn(go, t, dt);
				if (!r.valid()) { sol::error err = r; echo(std::string("ERROR: network simulate - ") + err.what()); }
			});
		});
		net.set_function("input", [session](sol::variadic_args va) {
			NetworkSession* s = session();
			if (!s) return;
			std::vector<NetValue> input;
			for (auto v : va) input.push_back(FromLua(v.as<sol::object>()));
			s->SetInput(input);
		});
		// Server: what `shooter` hit, as they saw the world. Returns the
		// object (or nil), the distance and the point.
		net.set_function("raycast", [session](const uint32 shooter, const Vec3 &origin, const Vec3 &direction, const f32 maxDistance,
			sol::optional<f64> viewTick) -> std::tuple<GameObject*, f32, Vec3> {
			NetworkSession* s = session();
			if (!s) return std::make_tuple((GameObject*)NULL, 0.f, Vec3());
			const NetworkSession::RayHit hit = s->RaycastRewound(shooter, origin, direction, maxDistance, viewTick.value_or(-1.0));
			return std::make_tuple(hit.netId ? s->Find(hit.netId) : (GameObject*)NULL, hit.distance, hit.point);
		});
		// Client: the tick on screen now - send it with a fire event. Server:
		// a client's, from its latest command.
		net.set_function("viewTick", [session](sol::optional<uint32> peer) {
			NetworkSession* s = session();
			if (!s) return 0.0;
			return s->GetRole() == NetworkSession::Client ? s->ViewTick() : s->ViewTickOf(peer.value_or(0));
		});

		net.set_function("stats", [session, lua]() {
			sol::table t = lua->create_table();
			NetworkSession* s = session();
			if (!s) return t;
			t["replicated"] = s->GetStats().replicated;
			t["snapshotBytes"] = s->GetStats().lastSnapshotBytes;
			t["tick"] = s->ServerTick();
			NetPeerStats ps;
			if (s->GetRole() == NetworkSession::Client && s->Transport().GetStats(1, ps))
			{
				t["rtt"] = ps.roundTripMs;
				t["loss"] = ps.packetLoss;
			}
			return t;
		});
	}

}

#endif
