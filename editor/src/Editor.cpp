//============================================================================
// Name        : Editor.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : ImGui Example
//============================================================================

#include "Editor.h"
#include <glad/glad.h>

using namespace p3d;

Editor* Editor::instance= NULL;

Editor* Editor::getInstance()
{
	if (instance == 0) {
		instance = new Editor();
	}
	return instance;
}

void Editor::cleanupInstance()
{
	if (instance != NULL) {
		delete instance;
		instance = NULL;
	}
}

Editor::Editor() : ClassName(1024,768,"PyrosBuilder",WindowType::Close | WindowType::Resize) 
{
	resetLayout = false;
}

void Editor::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	sceneView->OnResize(width, height);
}

void Editor::Init()
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Disable Multi-Viewport for input focus stability
	//io.ConfigViewportsNoAutoMerge = true;
	//io.ConfigViewportsNoTaskBarIcon = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// Platform + renderer backends per graphics API. Vulkan and Metal are
	// wrapped by their render devices rather than calling ImGui_Impl* here -
	// they share the engine's already-loaded volk pointers / MTLDevice, which
	// a copy compiled into this binary could not (see VulkanImGuiBackend.cpp).
#if defined(_SDL2VULKAN)
	ImGui_ImplSDL2_InitForVulkan(GetSDLWindow());
	static_cast<VulkanRenderDevice&>(GetActiveRenderDevice()).InitImGuiVulkanBackend();
#elif defined(_SDL2METAL)
	ImGui_ImplSDL2_InitForMetal(GetSDLWindow());
	static_cast<MetalRenderDevice&>(GetActiveRenderDevice()).InitImGuiMetalBackend();
#else
	ImGui_ImplSDL2_InitForOpenGL(rview, mainGLContext);
	// Must match the context the engine asks SDL for (a GL core profile, or
	// GLES3): "#version 130" is compatibility-profile-only and the driver
	// rejects it outright on macOS, which used to abort in the backend's
	// first NewFrame(). Same choice BaseExample makes.
	#if defined(GLES3) || defined(EMSCRIPTEN)
		ImGui_ImplOpenGL3_Init("#version 300 es");
	#else
		ImGui_ImplOpenGL3_Init("#version 330");
	#endif
#endif

	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::Space, this, &Editor::MouseMove);

	UISettings();

	// Pre Render to Load docs correctly
#if defined(_SDL2VULKAN)
	static_cast<VulkanRenderDevice&>(GetActiveRenderDevice()).NewImGuiVulkanFrame();
#elif defined(_SDL2METAL)
	static_cast<MetalRenderDevice&>(GetActiveRenderDevice()).NewImGuiMetalFrame();
#else
	ImGui_ImplOpenGL3_NewFrame();
#endif
	ImGui_ImplSDL2_NewFrame();
	
	tabLog = new TabLog("Log", &showingLog);
	tabProperties = new PropertiesTab(&showingTabProperties);
	tabTools = new ToolsTab(&showingTabTools);
	sceneView = new SceneEditor(&showingSceneView, &showingSceneTree);
	sceneView->Init(Width, Height);

	tabProperties->SetActive(sceneView);
	tabTools->SetActive(sceneView);

	showingLog = showingSceneTree = showingSceneView = showingTabProperties = showingTabTools = showingMaterialEditor = true;
}

void Editor::LoadDefaultLayout()
{
	showingLog = showingSceneTree = showingSceneView = showingTabProperties = showingTabTools = showingMaterialEditor = true;
	// The dock nodes can only be rebuilt while a frame is in flight, so
	// just arm it here and let DrawUI() do the work.
	resetLayout = true;
}

// Scene Tree | Scene View | Tools over Properties, Log across the bottom -
// the arrangement the editor was originally shipped with, previously only
// reachable by restoring a hand-made imgui.ini next to the binary.
void Editor::BuildDefaultLayout(const ImGuiID dockspaceID, const ImVec2 &size)
{
	ImGui::DockBuilderRemoveNode(dockspaceID);
	ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceID, size);

	ImGuiID center = dockspaceID;
	ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, NULL, &center);
	ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, NULL, &center);
	ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, NULL, &center);
	ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.5f, NULL, &right);

	ImGui::DockBuilderDockWindow("Scene Tree", left);
	ImGui::DockBuilderDockWindow("Scene View", center);
	ImGui::DockBuilderDockWindow("Tools", right);
	ImGui::DockBuilderDockWindow("Properties", rightBottom);
	ImGui::DockBuilderDockWindow("Log", bottom);

	ImGui::DockBuilderFinish(dockspaceID);
}

void Editor::Update() 
{
	sceneView->Update(GetTime());
	DrawUI();
}

void Editor::DrawUI()
{
	// Update - Game Loop
#if defined(_SDL2VULKAN)
	static_cast<VulkanRenderDevice&>(GetActiveRenderDevice()).NewImGuiVulkanFrame();
#elif defined(_SDL2METAL)
	static_cast<MetalRenderDevice&>(GetActiveRenderDevice()).NewImGuiMetalFrame();
#else
	ImGui_ImplOpenGL3_NewFrame();
#endif
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

    // Menu bar
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            // Project-level, as distinct from the Scene menu's scene files.
            // Disabled rather than silently doing nothing: there is no
            // project concept in the editor yet - no project file, no notion
            // of which scenes/assets belong to one.
            ImGui::MenuItem("New Project", "CTRL+N", false, false);
            ImGui::MenuItem("Open Project...", "CTRL+O", false, false);
            ImGui::MenuItem("Save Project", "CTRL+S", false, false);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Projects are not implemented yet.\nUse the Scene menu to open and save scenes.");
            ImGui::EndMenu();
        }

        sceneView->ShowMenubarOptions();

        if (ImGui::BeginMenu("View", ""))
        {
            if (ImGui::BeginMenu("Windows", "")) {
                if (ImGui::MenuItem("Log", "", &showingLog)) {}
                if (ImGui::MenuItem("Properties", "", &showingTabProperties)) {}
                if (ImGui::MenuItem("Tools", "", &showingTabTools)) {}
                if (ImGui::MenuItem("Scene View", "", &showingSceneView)) {}
                if (ImGui::MenuItem("Scene Tree", "", &showingSceneTree)) {}
                if (ImGui::MenuItem("Material Editor", "", &showingMaterialEditor)) {}
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Default Layout", "")) { LoadDefaultLayout(); }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

	// "Main" is a host window for the dockspace and nothing else: it fills
	// the viewport below the menu bar and every panel docks into it. The
	// panels used to be submitted *inside* this Begin()/End() pair, which
	// nests top-level windows inside another window - docking then has two
	// competing parents for them and ImGui's error recovery trips.
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Main", NULL,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoDocking);
	ImGui::PopStyleVar(3);

	ImGuiID dockspaceID = ImGui::GetID("MainDockSpace");
	// Rebuild on an explicit reset, and on the first run of a fresh build
	// dir (no imgui.ini yet, so no node exists) - otherwise leave whatever
	// arrangement the user saved last time alone.
	if (resetLayout || ImGui::DockBuilderGetNode(dockspaceID) == NULL)
	{
		BuildDefaultLayout(dockspaceID, viewport->WorkSize);
		resetLayout = false;
	}
	ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();

	// Panels are submitted at top level so they can dock into the space above.
	sceneView->DrawSceneFileDialog();

	if (showingLog)
		tabLog->Show();

	if (showingSceneView)
		sceneView->Show();

	if (showingTabTools)
		tabTools->Show();

	if (showingTabProperties)
		tabProperties->Show();

    ImGui::EndFrame();
    ImGui::Render();
	// On Vulkan and Metal the draw data is consumed inside the device's
	// EndFrame() through its UIRenderHook, so there is nothing to issue here
	// and no backbuffer of ours to clear - the render pass does it.
#if !defined(_SDL2VULKAN) && !defined(_SDL2METAL)
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif

    // Multi-viewport disabled
}

// The editor never adds a post effect, so PostEffectsManager::
// ProcessPostEffects() returns at its effects.empty() guard - and that is the
// only thing in the engine that brackets a swapchain frame for a scene drawn
// entirely offscreen. On GL that is invisible (SDL_GL_SwapWindow presents
// regardless), but on Vulkan and Metal nothing acquired or presented a frame
// at all: a black window, and the ImGui draw never ran because the backends
// record it inside EndFrame() via UIRenderHook. Bracket the frame here, after
// DrawUI() has already called ImGui::Render().
void Editor::Draw()
{
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
	GetActiveRenderDevice().BeginFrame();
	GetActiveRenderDevice().EndFrame();
#endif
	ClassName::Draw();
}

void Editor::MouseMove(Event::Input::Info e)
{
	//gizmo->OnMouseMove(mPos.x, mPos.y);
}

void Editor::Shutdown()
{
    // All your Shutdown Code Here

	delete tabProperties;
	delete tabTools;
	delete sceneView;
	delete tabLog;

#if defined(_SDL2VULKAN)
	static_cast<VulkanRenderDevice&>(GetActiveRenderDevice()).ShutdownImGuiVulkanBackend();
#elif defined(_SDL2METAL)
	static_cast<MetalRenderDevice&>(GetActiveRenderDevice()).ShutdownImGuiMetalBackend();
#else
	ImGui_ImplOpenGL3_Shutdown();
#endif
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

Editor::~Editor() 
{
	// Clean up any remaining resources
	if (instance == this) {
		instance = NULL;
	}
}
