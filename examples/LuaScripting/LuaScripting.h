//============================================================================
// Name        : LuaScripting.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Lua Scripting Example
//============================================================================

#ifndef LUA_SCRIPTING_H
#define	LUA_SCRIPTING_H

#define _STR(path) #path
#define STR(path) _STR(path)

#if defined(_SDL)
#include "../WindowManagers/SDL/SDLContext.h"
#define ClassName SDLContext
#elif defined(_SDL2VULKAN)
#include "../WindowManagers/SDL2Vulkan/SDL2VulkanContext.h"
#define ClassName SDL2VulkanContext
#else
// Default to SDL2 - was previously SFML here (a stale fallback with no
// real reason for this example to depend on it), which also meant this
// case never learned about _SDL2VULKAN and silently pulled in SFML for
// every Vulkan build.
#include "../WindowManagers/SDL2/SDL2Context.h"
#define ClassName SDL2Context
#endif

#include <Pyros3D/Ext/sol/sol.hpp>
#include <Pyros3D/Utils/Bindings/PyrosBindings.h>

using namespace p3d;

class LuaScripting : public ClassName
{

public:

	LuaScripting();
	virtual ~LuaScripting();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

	bool LoadScript(const std::string &file);

	sol::state lua;
	LUA_GameObject* camera;
	LUA_RenderingComponent* rCube;
	bool mainScript;

};

#endif	/* LUA_SCRIPTING_H */

