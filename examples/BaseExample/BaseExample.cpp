//============================================================================
// Name        : BaseExample.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Parallax Mapping
//============================================================================

#include "BaseExample.h"
#include <iostream>
#include <SDL2/SDL.h>
#include <cstdio>
#include <Pyros3D/Other/PyrosGL.h>

using namespace p3d;

BaseExample::BaseExample(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType) : ClassName(width, height, title, windowType)
{
	imguiInitialized = false;
	mouseCaptured = true;
	ignoreNextMouseDelta = false;

	// Shutdown() guards each of these with `if (ptr)` before deleting; they
	// must start null rather than uninitialized, since not every example
	// sets Light/dLight, and Init() must have run before Shutdown() can
	// safely dereference the rest.
	Scene = nullptr;
	Renderer = nullptr;
	FPSCamera.reset();
	Light.reset();
	dLight.reset();
}

void BaseExample::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	// mouseCenter used to be computed once in Init() and never updated, so
	// after any resize LookTo()'s recenter kept warping the cursor to the
	// *old* window's center - which a shrunk window can put outside itself
	// entirely (measured: cursor parked at 640,360 with the window down to
	// 460x881). Recompute it, and move the cursor there now so the pointer
	// doesn't sit somewhere unrelated to where the camera thinks it is.
	//
	// Integer division, deliberately - NOT width * .5f. SetMousePosition()
	// takes uint32, so the cursor can only ever land on a whole pixel; an
	// odd dimension put mouseCenter on a half pixel (503 wide -> 251.5)
	// while the warp actually landed the cursor at 251. mouseCenter could
	// then never equal GetMousePosition(), so LookTo()'s guard passed on
	// every event and fed it a phantom (-0.5,-0.5) delta forever - the
	// camera rotating on its own for as long as the app ran, and only when
	// a dimension happened to be odd, which is why it looked intermittent.
	// Confirmed by logging the real values, not derived.
	mouseCenter = Vec2((f32)(width / 2), (f32)(height / 2));

	// Recentering also has to not register as a look. This runs during
	// event processing and a tiling WM delivers resizes in bursts, so
	// motion events queued against the *previous* geometry can still be
	// waiting when LookTo() next runs; without the flag each one is read as
	// a real movement of (new center - old position) and turned into
	// rotation - one jump per resize in the burst.
	if (mouseCaptured)
	{
		SetMousePosition((uint32)mouseCenter.x, (uint32)mouseCenter.y);
		mouseLastPosition = mouseCenter;
		ignoreNextMouseDelta = true;
	}
}

void BaseExample::Init()
{
	// Initialization
	ClassName::Init();

	// Scene
	Scene = new SceneGraph();

	// Create Camera
	FPSCamera = std::make_shared<GameObject>();
	FPSCamera->SetPosition(Vec3(0, 0, 80));

	Scene->Add(FPSCamera);
	
	// Input
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::W, this, &BaseExample::MoveFrontPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::S, this, &BaseExample::MoveBackPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::A, this, &BaseExample::StrafeLeftPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::D, this, &BaseExample::StrafeRightPress);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::W, this, &BaseExample::MoveFrontRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::S, this, &BaseExample::MoveBackRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::A, this, &BaseExample::StrafeLeftRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::D, this, &BaseExample::StrafeRightRelease);
	InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &BaseExample::LookTo);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::Escape, this, &BaseExample::Exit);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::Tab, this, &BaseExample::OnTabPress);

	_strafeLeft = _strafeRight = _moveBack = _moveFront = false;
	
	// Integer division - see OnResize()'s comment for why .5f here is a
	// real bug (this is where it originally came from).
	SetMousePosition(Width / 2, Height / 2);
	mouseCenter = Vec2((f32)(Width / 2), (f32)(Height / 2));
	mouseLastPosition = mouseCenter;
	counterX = counterY = 0.f;

	// Initialize ImGui - will be done after specific example setup
	//InitImGui();
}

void BaseExample::Update()
{
	// Only process camera movement if mouse is captured
	if (mouseCaptured) {
		Vec3 finalPosition;
		Vec3 direction = FPSCamera->GetDirection();
		float dt = (float)GetTime() - lastTime;
		lastTime = GetTime();
		float speed = dt * 20.f;
		
		if (_moveFront)
		{
			finalPosition -= direction*speed;
		}
		if (_moveBack)
		{
			finalPosition += direction*speed;
		}
		if (_strafeLeft)
		{
			finalPosition += direction.cross(Vec3(0, 1, 0)).normalize()*speed;
		}
		if (_strafeRight)
		{
			finalPosition -= direction.cross(Vec3(0, 1, 0)).normalize()*speed;
		}

		FPSCamera->SetPosition(FPSCamera->GetPosition() + finalPosition);
	}
}

void BaseExample::Shutdown()
{
	// Shutdown ImGui
	ShutdownImGui();
	
	// Clean up scene
	if (Scene) {
		delete Scene;
		Scene = nullptr;
	}
	
	// Clean up camera / light - shared_ptr drops last ref
	FPSCamera.reset();
	Light.reset();
	dLight.reset();
	
	// Clean up renderer
	if (Renderer) {
		delete Renderer;
		Renderer = nullptr;
	}
	
	// Call parent shutdown
	ClassName::Shutdown();
}

BaseExample::~BaseExample() {}

void BaseExample::Exit(Event::Input::Info e) {
  this->Close();
}

void BaseExample::MoveFrontPress(Event::Input::Info e)
{
	_moveFront = true;
}
void BaseExample::MoveBackPress(Event::Input::Info e)
{
	_moveBack = true;
}
void BaseExample::StrafeLeftPress(Event::Input::Info e)
{
	_strafeLeft = true;
}
void BaseExample::StrafeRightPress(Event::Input::Info e)
{
	_strafeRight = true;
}
void BaseExample::MoveFrontRelease(Event::Input::Info e)
{
	_moveFront = false;
}
void BaseExample::MoveBackRelease(Event::Input::Info e)
{
	_moveBack = false;
}
void BaseExample::StrafeLeftRelease(Event::Input::Info e)
{
	_strafeLeft = false;
}
void BaseExample::StrafeRightRelease(Event::Input::Info e)
{
	_strafeRight = false;
}
void BaseExample::LookTo(Event::Input::Info e)
{
	// Only process mouse input for camera control if mouse is captured
	if (mouseCaptured) {
		// A resize just recentered the cursor - see OnResize()'s comment.
		// Any motion event still queued from before that recenter refers
		// to the old geometry; treat the first one as "this is where the
		// cursor is now", not as a movement the user made.
		if (ignoreNextMouseDelta)
		{
			ignoreNextMouseDelta = false;
			mouseLastPosition = InputManager::GetMousePosition();
			return;
		}
		if (mouseCenter != GetMousePosition())
		{
			mousePosition = InputManager::GetMousePosition();
			Vec2 mouseDelta = (mousePosition - mouseLastPosition);
			if (mouseDelta.x != 0 || mouseDelta.y != 0)
			{
				counterX -= mouseDelta.x / 10.f;
				counterY -= mouseDelta.y / 10.f;
				if (counterY<-80.f) counterY = -80.f;
				if (counterY>80.f) counterY = 80.f;
				Quaternion qX, qY;
				qX.AxisToQuaternion(Vec3(1.f, 0.f, 0.f), DEGTORAD(counterY));
				qY.AxisToQuaternion(Vec3(0.f, 1.f, 0.f), DEGTORAD(counterX));
				//                Matrix rotX, rotY;
				//                rotX.RotationX(DEGTORAD(counterY));
				//                rotY.RotationY(DEGTORAD(counterX));
				FPSCamera->SetRotation((qY*qX).GetEulerFromQuaternion());
				SetMousePosition((int)(mouseCenter.x), (int)(mouseCenter.y));
				mouseLastPosition = mouseCenter;
			}
		}
	}
}

void BaseExample::InitImGui()
{
#if defined(_SDL2METAL)
	// Real ImGui-on-Metal backend - MetalRenderDevice wraps ImGui_ImplMetal_*
	// the same way VulkanRenderDevice wraps ImGui_ImplVulkan_* below (see
	// InitImGuiMetalBackend()'s header comment).
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();

	ImGui_ImplSDL2_InitForMetal(GetSDLWindow());
	imguiInitialized = static_cast<MetalRenderDevice&>(GetActiveRenderDevice()).InitImGuiMetalBackend();
#elif !defined(_SDL2VULKAN)
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
#if !defined(EMSCRIPTEN)
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport
#endif

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL(GetSDLWindow(), GetGLContext());
#if defined(GLES3) || defined(EMSCRIPTEN)
	ImGui_ImplOpenGL3_Init("#version 300 es");
#else
	ImGui_ImplOpenGL3_Init("#version 330");
#endif

	imguiInitialized = true;
#else
	// Real Vulkan ImGui backend (previously a stub - see
	// VULKAN_ROADMAP.md's Step D note and examples/DemoLauncher, the
	// first real consumer that needed this working). ImGui_ImplVulkan_*
	// itself is never called here - VulkanRenderDevice wraps it (see its
	// InitImGuiVulkanBackend() comment for why: sharing this library's
	// already-loaded volk function pointers, which a copy compiled into
	// this example binary couldn't). No viewports/docking on this path
	// (unlike the GL branch above) - not wired up, not needed by any
	// current Vulkan-built example.
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	ImGui_ImplSDL2_InitForVulkan(GetSDLWindow());
	imguiInitialized = static_cast<VulkanRenderDevice&>(GetActiveRenderDevice()).InitImGuiVulkanBackend();
#endif
}

void BaseExample::ShutdownImGui()
{
	if (imguiInitialized) {
#if defined(_SDL2METAL)
		static_cast<MetalRenderDevice&>(GetActiveRenderDevice()).ShutdownImGuiMetalBackend();
#elif !defined(_SDL2VULKAN)
		ImGui_ImplOpenGL3_Shutdown();
#else
		static_cast<VulkanRenderDevice&>(GetActiveRenderDevice()).ShutdownImGuiVulkanBackend();
#endif
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		imguiInitialized = false;
	}
}

void BaseExample::BeginImGuiFrame()
{
	if (imguiInitialized) {
#if defined(_SDL2METAL)
		static_cast<MetalRenderDevice&>(GetActiveRenderDevice()).NewImGuiMetalFrame();
#elif !defined(_SDL2VULKAN)
		ImGui_ImplOpenGL3_NewFrame();
#else
		static_cast<VulkanRenderDevice&>(GetActiveRenderDevice()).NewImGuiVulkanFrame();
#endif
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
	}
}

void BaseExample::PrepareImGuiFrame()
{
	// Begin+DrawUI+Render, meant to be called BEFORE Renderer->RenderScene()
	// on the Vulkan path - see VulkanRenderDevice::EndFrame()'s UIRenderHook:
	// it fires *inside* RenderScene(), so ImGui::Render() must have already
	// finalized this frame's draw data by the time RenderScene() runs, not
	// after. RenderImGui() (below) still does Begin+DrawUI+End all together
	// AFTER RenderScene() for GL subclasses, unchanged from before - no
	// existing subclass's Update() call order changes. A subclass that
	// wants real Vulkan ImGui needs to call PrepareImGuiFrame() before
	// RenderScene() and EndImGuiFrame() after, same as examples/DemoLauncher
	// does, instead of the combined RenderImGui().
	if (!imguiInitialized)
		return;
	BeginImGuiFrame();
	DrawUI();
	ImGui::Render();
}

void BaseExample::EndImGuiFrame()
{
	if (imguiInitialized) {
		// GL-only: Metal's ImGui draw call happens inside
		// MetalRenderDevice::EndFrame() via UIRenderHook (see its header
		// comment), and Vulkan's identical UIRenderHook mechanism already
		// handles that backend the same way - this whole block is a no-op
		// for both.
#if !defined(_SDL2METAL) && !defined(_SDL2VULKAN)
		// Save OpenGL state before ImGui rendering
		GLint last_program, last_texture, last_array_buffer, last_element_array_buffer, last_vertex_array;
		glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Update and Render additional Platform Windows
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
			SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
		}

		// Restore OpenGL state after ImGui rendering
		// Force restore the shader program that ImGui backend doesn't restore
		glUseProgram(last_program);
		glBindTexture(GL_TEXTURE_2D, last_texture);
		glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
		glBindVertexArray(last_vertex_array);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		// Additional state restoration to ensure proper rendering
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif
		// Vulkan: no-op via RenderImGui()'s call order (see
		// PrepareImGuiFrame()'s comment) - a subclass using the
		// PrepareImGuiFrame()/EndImGuiFrame() split instead gets a real
		// no-op here too, since VulkanRenderDevice::EndFrame() (called
		// from inside RenderScene(), which already ran) already recorded
		// the draw data via UIRenderHook.
	}
}

void BaseExample::RenderImGui()
{
	// DrawUI() (and every per-example override of it, e.g.
	// RotatingCube::DrawUI()) calls raw ImGui:: functions directly, not
	// gated on imguiInitialized itself - on the Vulkan backend
	// (InitImGui()'s _SDL2VULKAN stub, see its comment) no ImGui context
	// ever gets created, so calling DrawUI() unconditionally here crashed
	// on the first ImGui::Begin() once a Vulkan example's render loop
	// actually reached this call (previously unexercised - every earlier
	// "ran clean" check on this backend only verified Init() completed,
	// not a real frame; see VULKAN_ROADMAP.md). Gating the whole call the
	// same way BeginImGuiFrame()/EndImGuiFrame() already are makes this a
	// true no-op instead.
	//
	// Still GL-oriented even now that InitImGui() sets up a real Vulkan
	// backend too: this combined Begin+DrawUI+End order runs AFTER
	// RenderScene() (every subclass's existing Update() call site) - see
	// PrepareImGuiFrame()'s comment for why that's too late on Vulkan
	// specifically. No current subclass builds under CONTEXT=SDL2Vulkan,
	// so this is an existing, unchanged limitation, not a regression.
	if (!imguiInitialized)
		return;
	BeginImGuiFrame();
	DrawUI();
#if defined(_SDL2VULKAN)
	// EndImGuiFrame()'s Vulkan branch is a no-op that assumes
	// ImGui::Render() already ran earlier this frame via
	// PrepareImGuiFrame() (see its comment) - true for DemoLauncher-style
	// subclasses, not for this combined call, which runs after
	// RenderScene() already closed the frame's only render pass. Render()
	// must still be called here though, or the *next* frame's NewFrame()
	// (from BeginImGuiFrame()) trips ImGui's own
	// "Forgot to call Render()...?" sanity assertion - draw data just
	// ends up one frame late, consumed by the next EndFrame()'s
	// UIRenderHook instead of this one's (already past by now).
	ImGui::Render();
#else
	EndImGuiFrame();
#endif
}

void BaseExample::DrawUI()
{
	// Base UI with FPS and controls
	DrawBaseUI();
}

void BaseExample::DrawBaseUI()
{
	// Create a simple UI window
	ImGui::Begin("Pyros3D Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	
	ImGui::Text("Pyros3D Engine");
	ImGui::Separator();
	
	// FPS information
	ImGui::Text("FPS: %.1u", (uint32)fps.getFPS());
	ImGui::Text("Frame Time: %.3f ms", 1000.0f / fps.getFPS());
	
	ImGui::Separator();
	
	// Mouse capture status
	ImGui::Text("Mouse: %s", mouseCaptured ? "Captured" : "Free");
	ImGui::Text("Press TAB to toggle mouse capture");
	ImGui::Text("Press ESC to exit");
	
	ImGui::Separator();
	
	// Camera position
	Vec3 camPos = FPSCamera->GetPosition();
	ImGui::Text("Camera: (%.1f, %.1f, %.1f)", camPos.x, camPos.y, camPos.z);
	
	ImGui::End();
}

void BaseExample::ToggleMouseCapture()
{
	mouseCaptured = !mouseCaptured;
	
	if (mouseCaptured) {
		// Capture mouse for camera control
		SDL_SetRelativeMouseMode(SDL_TRUE);
		SDL_ShowCursor(SDL_DISABLE);
		DisableImGuiMouseInput(); // Disable ImGui mouse input when captured
	} else {
		// Release mouse for UI interaction
		SDL_SetRelativeMouseMode(SDL_FALSE);
		SDL_ShowCursor(SDL_ENABLE);
		EnableImGuiMouseInput(); // Enable ImGui mouse input when free
	}
}

void BaseExample::OnTabPress(Event::Input::Info p)
{
	printf("TAB pressed! Mouse was %s\n", mouseCaptured ? "captured" : "free");
	ToggleMouseCapture();
	printf("Mouse is now %s\n", mouseCaptured ? "captured" : "free");
}

void BaseExample::DisableImGuiMouseInput()
{
	// No ImGui context exists at all on _SDL2METAL (see InitImGui()'s
	// comment) - ImGui::GetIO() dereferences a null current-context
	// pointer in that case, crashing on the very first TAB press.
	if (!imguiInitialized) return;
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
}

void BaseExample::EnableImGuiMouseInput()
{
	if (!imguiInitialized) return;
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
}
