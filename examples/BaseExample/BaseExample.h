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
#elif defined(_SDL2VULKAN)
#include "../WindowManagers/SDL2Vulkan/SDL2VulkanContext.h"
#define ClassName SDL2VulkanContext
#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>
#elif defined(_SDL2METAL)
#include "../WindowManagers/SDL2Metal/SDL2MetalContext.h"
#define ClassName SDL2MetalContext
#include <Pyros3D/Rendering/Device/MetalRenderDevice.h>
#elif defined(_SDL2)
#include "../WindowManagers/SDL2/SDL2Context.h"
#define ClassName SDL2Context
#else
#include "../WindowManagers/SFML/SFMLContext.h"
#define ClassName SFMLContext
#endif

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Utils/DeltaTime/DeltaTime.h>
#include <Pyros3D/Utils/FPS/FPS.h>
#include <memory>

// ImGui includes - resolved via the IMGUI_INCLUDE_DIRS include path (see
// root CMakeLists.txt), not a relative path - ImGui core now lives at
// src/Pyros3D/Ext/imgui (engine-owned), not examples/imgui.
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

using namespace p3d;

class BaseExample : public ClassName {

public:

	BaseExample(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType);
	virtual ~BaseExample();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void DrawUI(); // Override to add ImGui controls

protected:

	// Scene
	SceneGraph* Scene;
	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// Camera - Its a regular GameObject
	std::shared_ptr<GameObject> FPSCamera;
	// Light
	std::shared_ptr<GameObject> Light;
	std::shared_ptr<DirectionalLight> dLight;

	// ImGui support
	bool imguiInitialized;
	bool mouseCaptured;
	
	// ImGui methods
	void InitImGui();
	void ShutdownImGui();
	void BeginImGuiFrame();
	// Begin+DrawUI+Render - call before Renderer->RenderScene() for a
	// subclass that wants real Vulkan ImGui (paired with EndImGuiFrame()
	// after). See its .cpp definition's comment.
	void PrepareImGuiFrame();
	void EndImGuiFrame();
	void RenderImGui();
	void DrawBaseUI(); // Base UI with FPS and mouse controls
	
	// Mouse capture methods
	void ToggleMouseCapture();
	void OnTabPress(Event::Input::Info p);
	void DisableImGuiMouseInput();
	void EnableImGuiMouseInput();

	// Input handling
	void MoveFrontPress(Event::Input::Info e);
	void MoveBackPress(Event::Input::Info e);
	void StrafeLeftPress(Event::Input::Info e);
	void StrafeRightPress(Event::Input::Info e);
	void MoveFrontRelease(Event::Input::Info e);
	void MoveBackRelease(Event::Input::Info e);
	void StrafeLeftRelease(Event::Input::Info e);
	void StrafeRightRelease(Event::Input::Info e);
	void LookTo(Event::Input::Info e);
	void Exit(Event::Input::Info e);

	// FPS look yaw (counterX) / pitch (counterY) in degrees. Protected so
	// demos that mirror the camera (Island water reflection) can use the
	// same angles the mouse look wrote, instead of GetRotation() Euler
	// which swims after quaternion→euler round-trips.
	float counterX, counterY;

private:

	Vec2 mouseCenter, mouseLastPosition, mousePosition;
	// Set by OnResize() after it recenters the cursor; makes LookTo()
	// resync its reference point instead of turning the resulting jump
	// into camera rotation - see both call sites' comments.
	bool ignoreNextMouseDelta;
	bool _moveFront, _moveBack, _strafeLeft, _strafeRight;
	float lastTime;
};

#endif	/* BaseExample_H */

