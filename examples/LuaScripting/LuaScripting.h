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
#elif defined(_SDL2)
#include "../WindowManagers/SDL2/SDL2Context.h"
#define ClassName SDL2Context
#else
#include "../WindowManagers/SFML/SFMLContext.h"
#define ClassName SFMLContext
#endif

#include <Pyros3D/Ext/Sol/sol.hpp>
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

