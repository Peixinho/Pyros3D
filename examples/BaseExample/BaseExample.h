//============================================================================
// Name        : BaseExample.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Parallax Mapping Example
//============================================================================

#ifndef BaseExample_H
#define	BaseExample_H

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

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>

using namespace p3d;

class BaseExample : public ClassName {

public:

	BaseExample(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType = WindowType::Fullscreen);
	virtual ~BaseExample();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

protected:

	// Scene
	SceneGraph* Scene;
	// Camera - Its a regular GameObject
	GameObject* FPSCamera;

	// Events
	virtual void MoveFrontPress(Event::Input::Info e);
	virtual void MoveBackPress(Event::Input::Info e);
	virtual void StrafeLeftPress(Event::Input::Info e);
	virtual void StrafeRightPress(Event::Input::Info e);
	virtual void MoveFrontRelease(Event::Input::Info e);
	virtual void MoveBackRelease(Event::Input::Info e);
	virtual void StrafeLeftRelease(Event::Input::Info e);
	virtual void StrafeRightRelease(Event::Input::Info e);
	virtual void LookTo(Event::Input::Info e);

	// Navigation Variables
	float counterX, counterY;
	Vec2 mouseCenter, mouseLastPosition, mousePosition;
	bool _moveFront, _moveBack, _strafeLeft, _strafeRight;
	f64 lastTime;

};

#endif	/* BaseExample_H */

