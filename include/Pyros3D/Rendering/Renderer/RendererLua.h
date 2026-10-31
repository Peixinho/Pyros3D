//============================================================================
// Name        : RendererLua.h
// Author      : Duarte Peixinho
// Description : The renderer for scripts: the `Renderer` type, with what the renderer does - background, ambient,
//               levels of detail, frustum culling, the sky, exposure. A host that draws with the renderer gives its
//               scripts one as `renderer`; scripts that drove the old renderer's passes themselves find nothing of
//               them here and are ported to the renderer's features.
//============================================================================

#ifndef PYROS_RENDERER_LUA_H
#define PYROS_RENDERER_LUA_H

#if defined(LUA_BINDINGS)
#include <Pyros3D/Ext/sol/sol.hpp>
#include <Pyros3D/Other/Export.h>

namespace p3d {
	class SceneRenderer;
	PYROS3D_API void BindRenderer(sol::state &lua);
	PYROS3D_API void SetScriptRenderer(sol::state &lua, SceneRenderer* renderer);
}
#endif

#endif /* PYROS_RENDERER_LUA_H */
