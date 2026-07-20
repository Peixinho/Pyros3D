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

	// Shutdown() guards each of these with `if (ptr)` before deleting; they
	// must start null rather than uninitialized, since not every example
	// sets Light/dLight, and Init() must have run before Shutdown() can
	// safely dereference the rest.
	Scene = nullptr;
	Renderer = nullptr;
	FPSCamera = nullptr;
	Light = nullptr;
	dLight = nullptr;
}

void BaseExample::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);
}

void BaseExample::Init()
{
	// Initialization
	ClassName::Init();

	// Scene
	Scene = new SceneGraph();

	// Create Camera
	FPSCamera = new GameObject();
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
	
	SetMousePosition((uint32)(Width *.5f), (uint32)(Height *.5f));
	mouseCenter = Vec2((f32)Width *.5f, (f32)Height *.5f);
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
	
	// Clean up camera
	if (FPSCamera) {
		delete FPSCamera;
		FPSCamera = nullptr;
	}
	
	// Clean up light
	if (Light) {
		delete Light;
		Light = nullptr;
	}
	
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
#if !defined(_SDL2VULKAN)
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport

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
	ImGui_ImplOpenGL3_Init("#version 330");

	imguiInitialized = true;
#else
	// ImGui is hard-wired to imgui_impl_opengl3 (GetGLContext() above isn't
	// even declared on SDL2VulkanContext) - stubbed out for the Vulkan
	// backend rather than also standing up imgui_impl_vulkan, per
	// VULKAN_ROADMAP.md's Step D scope. imguiInitialized stays false, so
	// BeginImGuiFrame()/EndImGuiFrame()/ShutdownImGui() (all gated on it)
	// are natural no-ops - no other guards needed.
#endif
}

void BaseExample::ShutdownImGui()
{
	if (imguiInitialized) {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		imguiInitialized = false;
	}
}

void BaseExample::BeginImGuiFrame()
{
	if (imguiInitialized) {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
	}
}

void BaseExample::EndImGuiFrame()
{
	if (imguiInitialized) {
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
	if (!imguiInitialized)
		return;
	BeginImGuiFrame();
	DrawUI();
	EndImGuiFrame();
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
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
}

void BaseExample::EnableImGuiMouseInput()
{
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
}
