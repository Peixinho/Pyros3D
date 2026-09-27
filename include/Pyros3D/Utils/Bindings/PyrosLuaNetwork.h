//============================================================================
// Name        : PyrosLuaNetwork.h
// Description : The `network` table for game scripts - see NetworkSession.
//               Registered by a host (the player, the editor's play mode),
//               not with the rest of the bindings: the session belongs to
//               the host's scene, and `session` hands back the current one,
//               creating it on first use.
//============================================================================

#ifndef PYROSLUANETWORK_H
#define PYROSLUANETWORK_H

#ifdef LUA_BINDINGS

#include <Pyros3D/Other/Export.h>
#include <functional>

namespace sol { class state; }

namespace p3d {

	class NetworkSession;

	PYROS3D_API void RegisterLuaNetwork(sol::state* lua, const std::function<NetworkSession*()> &session);

}

#endif

#endif /* PYROSLUANETWORK_H */
