//============================================================================
// Name        : NeonPulse.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Neon Pulse - a game written entirely in Lua
//============================================================================

#ifndef NEONPULSE_H
#define	NEONPULSE_H

#define _STR(path) #path
#define STR(path) _STR(path)

#if defined(_SDL)
#include "../WindowManagers/SDL/SDLContext.h"
#define ClassName SDLContext
#elif defined(_SDL2VULKAN)
#include "../WindowManagers/SDL2Vulkan/SDL2VulkanContext.h"
#define ClassName SDL2VulkanContext
#else
#include "../WindowManagers/SDL2/SDL2Context.h"
#define ClassName SDL2Context
#endif

#include <Pyros3D/Ext/sol/sol.hpp>
#include <Pyros3D/Utils/Bindings/PyrosBindings.h>

using namespace p3d;

// Deliberately thin: this class owns a window, a sol::state and nothing
// else. Every line of the actual game - scene construction, materials,
// lighting, collision, scoring, HUD - lives in assets/neonpulse/*.lua and
// is reachable only through the bindings in PyrosBindings.cpp. If a
// feature the game needs isn't bound to Lua, the right fix is a binding,
// not C++ game code here.
class NeonPulse : public ClassName
{

public:

	NeonPulse();
	virtual ~NeonPulse();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

private:

	// Runs a .lua file through sol's protected script path so a syntax or
	// runtime error prints a real message instead of aborting.
	void RunScript(const std::string &file);

	sol::state lua;

};

#endif	/* NEONPULSE_H */
