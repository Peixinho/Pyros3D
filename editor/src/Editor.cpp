//============================================================================
// Name        : Editor.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : ImGui Example
//============================================================================

#include "Editor.h"
#include "editor/UI/OpenDir.h"
#include "editor/FileDropQueue.h"
#include <Pyros3D/Audio/AudioManager.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <FileDropHook.h>
#include <CloseHook.h>
#include <glad/glad.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>

using namespace p3d;
namespace fs = std::filesystem;

static void EditorOnOsFileDrop(const char* path)
{
	FileDropQueue::Push(path);
}

bool Editor::EditorOnWindowClose()
{
	Editor* ed = Editor::getInstance();
	return ed ? ed->EditorAllowWindowClose() : true;
}

void Editor::HostCloseProject()
{
	Editor* ed = Editor::getInstance();
	if (ed) ed->CloseProjectImmediate();
}

void Editor::HostQuitApp()
{
	Editor* ed = Editor::getInstance();
	if (!ed) return;
	ed->FinishQuitIfClean();
}

void Editor::HostQuitDiscardingUnsaved()
{
	Editor* ed = Editor::getInstance();
	if (!ed) return;
	for (size_t i = 0; i < ed->sceneDocs.size(); ++i)
	{
		if (ed->sceneDocs[i])
			ed->sceneDocs[i]->ClearSceneDirty();
	}
	for (size_t i = 0; i < ed->scriptDocs.size(); ++i)
	{
		if (ed->scriptDocs[i])
			ed->scriptDocs[i]->dirty = false;
	}
	if (ed->project.IsOpen())
		ed->project.ClearDirty();
	ed->Close();
}

void Editor::HostNewProject()
{
	Editor* ed = Editor::getInstance();
	if (!ed) return;
	ed->openNewProjectModal = true;
	ed->projectDialogError.clear();
}

void Editor::HostOpenProject()
{
	Editor* ed = Editor::getInstance();
	if (!ed) return;
	ed->openOpenProjectModal = true;
	ed->projectDialogError.clear();
}

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
	showingAssets = true;
	assetsWindowHovered = false;
	openNewProjectModal = openOpenProjectModal = false;
	openProjectSettingsModal = false;
	newProjectBrowseDir = openProjectBrowse = false;
	openDeleteAssetModal = false;
	openNewScriptModal = false;
	newScriptName.clear();
	newScriptError.clear();
	sceneView = NULL;
	nextSceneDocId = 1;
	sharedAudio = NULL;
#ifdef LUA_BINDINGS
	luaReady = false;
#endif
	activeScriptDoc = NULL;
	nextScriptDocId = 1;
	pendingSelectScriptId = 0;
	dockCenterId = 0;
}

#ifdef LUA_BINDINGS
void Editor::InitLuaHost()
{
	try {
		GenerateBindings(&lua);
		const std::string middleclass = std::string(STR_EX(PYROS_EXAMPLES_PATH)) + "/assets/middleclass.lua";
		// require_file caches the module as "class" but does NOT set _G.class.
		// Behavior scripts call class('Name') as a global (same as DemoLauncher
		// scenes) - assign the return value or every attach/load fails/wedges.
		sol::object classMod = lua.require_file("class", middleclass);
		lua["class"] = classMod;

		// Route Lua print(...) into the editor Log panel (not the OS terminal).
		lua.set_function("__pyros_log", [](const std::string& msg) {
			echo(msg);
		});
		lua.script(R"LUA(
function print(...)
	local n = select("#", ...)
	local parts = {}
	for i = 1, n do
		parts[i] = tostring(select(i, ...))
	end
	__pyros_log(table.concat(parts, "\t"))
end
)LUA");

		lua.set_function("setMouseCaptured", [this](bool captured) {
			if (captured)
			{
				SDL_WarpMouseInWindow(GetSDLWindow(), (int)(Width / 2), (int)(Height / 2));
				SDL_SetRelativeMouseMode(SDL_TRUE);
				SDL_ShowCursor(SDL_DISABLE);
				if (ImGui::GetCurrentContext() != NULL)
					ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
			}
			else
			{
				SDL_SetRelativeMouseMode(SDL_FALSE);
				SDL_ShowCursor(SDL_ENABLE);
				if (ImGui::GetCurrentContext() != NULL)
					ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
			}
		});
		lua.set_function("isMouseCaptured", []() {
			return SDL_GetRelativeMouseMode() == SDL_TRUE;
		});
		lua.set_function("warpMouseToCenter", [this]() {
			SDL_WarpMouseInWindow(GetSDLWindow(), (int)(Width / 2), (int)(Height / 2));
		});
		lua.set_function("getWindowSize", [this]() {
			return std::make_tuple((int)Width, (int)Height);
		});

		LuaComponent::SetUpdatesEnabled(false);
		luaReady = true;
		echo("SUCCESS: Lua host ready");
	}
	catch (const std::exception& e) {
		luaReady = false;
		echo(std::string("ERROR: Lua host init failed: ") + e.what());
	}
	catch (...) {
		luaReady = false;
		echo("ERROR: Lua host init failed");
	}
}
#endif

void Editor::OnResize(const uint32 width, const uint32 height)
{
	ClassName::OnResize(width, height);
	for (size_t i = 0; i < sceneDocs.size(); ++i)
		sceneDocs[i]->OnResize(width, height);
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

	ImFontConfig defaultCfg;
	defaultCfg.SizePixels = 15.0f;
	io.Fonts->AddFontDefault(&defaultCfg);
	{
		ImFontConfig cfg;
		cfg.MergeMode = true;
		static const ImWchar iconRanges[] = { 0xf000, 0xf8ff, 0 };
		io.Fonts->AddFontFromFileTTF("assets/fonts/fa-solid-900.ttf", 15.0f, &cfg, iconRanges);
	}
	//ImGui::StyleColorsClassic();

	// Platform + renderer backends per graphics API. Vulkan and Metal are
	// wrapped by their render devices rather than calling ImGui_Impl* here -
	// they share the engine's already-loaded volk pointers / MTLDevice, which
	// a copy compiled into this binary could not (see VulkanImGuiBackend.cpp).
	// Scene brightness: VK/Metal keep UNORM swapchains (ImGui UI stays correct);
	// the editor viewport is display-encoded via PostEffectsManager::GetViewportColor().
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
	ImGui_ImplSDL2_NewFrame();
#if defined(_SDL2VULKAN)
	// After SDL NewFrame so Sync can override stale drawable scale - see
	// VulkanRenderDevice::NewImGuiVulkanFrame().
	static_cast<VulkanRenderDevice&>(GetActiveRenderDevice()).NewImGuiVulkanFrame();
#elif defined(_SDL2METAL)
	static_cast<MetalRenderDevice&>(GetActiveRenderDevice()).NewImGuiMetalFrame();
#else
	ImGui_ImplOpenGL3_NewFrame();
#endif
	
	tabLog = new TabLog("Log", &showingLog);
	tabProperties = new PropertiesTab(&showingTabProperties);
	tabTools = new ToolsTab(&showingTabTools);
	// Editor Log panel is the only sink — no OS terminal spam. Everything
	// includes Info so Lua print() shows up.
	p3d::LOG::_LOG::SetMirrorStdout(false);
	p3d::LOG::_LOG::SetLevel(p3d::LOG::Level::Info);
	sharedAudio = new AudioManager();
#ifdef LUA_BINDINGS
	InitLuaHost();
#endif
	sceneView = CreateSceneDocument();
	showingLog = showingSceneTree = showingSceneView = showingTabProperties = showingTabTools = showingMaterialEditor = true;
	showingAssets = true;
	PyrosFileDrop::SetHandler(&EditorOnOsFileDrop);
	PyrosWindowClose::SetHandler(&Editor::EditorOnWindowClose);
	if (sceneView)
	{
		sceneView->SetHostCallbacks(&Editor::HostCloseProject, &Editor::HostQuitApp,
			&Editor::HostNewProject, &Editor::HostOpenProject,
			&Editor::HostQuitDiscardingUnsaved);
	}
	LoadRecentProjects();
	// Rebuild dock layout once so fixed project-shell panel names replace any
	// leftover per-scene Hierarchy:/Scene: dock entries from older builds.
	resetLayout = true;
	if (const char* autoOpen = std::getenv("PYROS_OPEN_PROJECT"))
	{
		if (autoOpen[0])
			OpenProjectFromPath(autoOpen);
	}
}

void Editor::LoadDefaultLayout()
{
	showingLog = showingSceneTree = showingSceneView = showingTabProperties = showingTabTools = showingMaterialEditor = true;
	showingAssets = true;
	// The dock nodes can only be rebuilt while a frame is in flight, so
	// just arm it here and let DrawUI() do the work.
	resetLayout = true;
}

// Project shell: Scene Tree | Scene View | Tools/Properties, Assets+Log at bottom.
// Scene documents share these fixed panels; only the active tab fills Tree/View.
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
	ImGui::DockBuilderDockWindow("Assets", bottom);
	ImGui::DockBuilderDockWindow("Log", bottom);
	ImGui::DockBuilderDockWindow("Tools", right);
	ImGui::DockBuilderDockWindow("Properties", rightBottom);
	dockCenterId = center;

	ImGui::DockBuilderFinish(dockspaceID);
}

void Editor::Update() 
{
	ProcessPendingFileDrops();
	FlushPendingSceneDocumentCloses();
	for (size_t i = 0; i < sceneDocs.size(); ++i)
		sceneDocs[i]->Update(GetTime());
	// DrawUI / viewport CaptureFrame / thumbnails run in Draw() after
	// BeginFrame so Vulkan offscreen binds skip WaitAllFrameFences
	// (that wait was freezing the editor: ShowViewport ran with
	// !frameInProgress every tick and parked on MoltenVK fence waits).
}

void Editor::ProcessPendingFileDrops()
{
	std::vector<std::string> dropped;
	FileDropQueue::Drain(dropped);
	if (dropped.empty()) return;

	if (!project.IsOpen())
	{
		lastDropStatus = "Open a project before dropping files";
		echo("ERROR: " + lastDropStatus);
		return;
	}

	// Prefer drops aimed at the Assets window; still accept window-wide drops
	// into the project so Finder/Explorer drops are useful either way.
	int okCount = 0;
	for (size_t i = 0; i < dropped.size(); ++i)
	{
		std::string out;
		std::string err;
		if (project.ImportAssetFile(dropped[i], out, &err))
		{
			++okCount;
			const std::string rel = project.RelativePath(out);
			echo("Imported: " + (rel.empty() ? out : rel));
			if (sceneView && ProjectManager::IsP3dm(out))
			{
				sceneView->QueueModelThumbnail(out, true);
				std::map<std::string, Texture*>::iterator pit = assetPreviewCache.find(out);
				if (pit != assetPreviewCache.end())
				{
					if (pit->second)
						deferredDestroyPreviews.push_back(pit->second);
					assetPreviewCache.erase(pit);
				}
			}
		}
		else
			echo("ERROR: drop import failed (" + dropped[i] + "): " + err);
	}
	if (okCount > 0)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "Imported %d file%s", okCount, okCount == 1 ? "" : "s");
		lastDropStatus = buf;
		showingAssets = true;
	}
}

void Editor::DrawUI()
{
	// Update - Game Loop. SDL first so Vulkan can correct FramebufferScale
	// against the real swapchain extent (see NewImGuiVulkanFrame).
	ImGui_ImplSDL2_NewFrame();
#if defined(_SDL2VULKAN)
	static_cast<VulkanRenderDevice&>(GetActiveRenderDevice()).NewImGuiVulkanFrame();
#elif defined(_SDL2METAL)
	static_cast<MetalRenderDevice&>(GetActiveRenderDevice()).NewImGuiMetalFrame();
#else
	ImGui_ImplOpenGL3_NewFrame();
#endif
	ImGui::NewFrame();

	// Esc stops play from anywhere (Scene View tab may be unfocused / mouse captured).
	if (sceneView && sceneView->IsPlaying() && ImGui::IsKeyPressed(ImGuiKey_Escape))
		sceneView->StopPlayMode();

    // Menu bar
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Project", "CTRL+N"))
			{
				if (sceneView->ConfirmUnsavedThen(SceneEditor::UnsavedNewProject))
				{
					openNewProjectModal = true;
					projectDialogError.clear();
				}
			}
            if (ImGui::MenuItem("Open Project...", "CTRL+O"))
			{
				if (sceneView->ConfirmUnsavedThen(SceneEditor::UnsavedOpenProject))
				{
					openOpenProjectModal = true;
					projectDialogError.clear();
				}
			}
            if (ImGui::MenuItem("Save Project", "CTRL+S", false, project.IsOpen()))
			{
				if (sceneView->IsSceneDirty())
					sceneView->TrySaveCurrentScene();
				std::string err;
				if (!project.Save(&err))
					echo("ERROR: " + err);
			}
			if (ImGui::MenuItem("Project Settings...", NULL, false, project.IsOpen()))
			{
				openProjectSettingsModal = true;
				projectSettingsName = project.GetProjectName();
				projectDialogError.clear();
			}
			if (ImGui::MenuItem("Close Project", NULL, false, project.IsOpen()))
				CloseProject();
            ImGui::EndMenu();
        }

        if (project.IsOpen() && sceneView)
			sceneView->ShowMenubarOptions();

        if (ImGui::BeginMenu("View", ""))
        {
            if (ImGui::BeginMenu("Windows", "")) {
				if (ImGui::MenuItem("Scene Tree", "", &showingSceneTree)) {}
				if (ImGui::MenuItem("Scene View", "", &showingSceneView)) {}
                if (ImGui::MenuItem("Log", "", &showingLog)) {}
                if (ImGui::MenuItem("Properties", "", &showingTabProperties)) {}
                if (ImGui::MenuItem("Tools", "", &showingTabTools)) {}
                if (ImGui::MenuItem("Assets", "", &showingAssets)) {}
                if (ImGui::MenuItem("Material Editor", "", &showingMaterialEditor)) {}
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Default Layout", "")) { LoadDefaultLayout(); }
			if (project.IsOpen() && sceneView)
				sceneView->ShowViewOptions();
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
	DrawProjectDialogs();
	if (!project.IsOpen())
	{
		DrawWelcomeScreen();
		ImGui::EndFrame();
		ImGui::Render();
#if !defined(_SDL2VULKAN) && !defined(_SDL2METAL)
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
		return;
	}

	if (sceneView)
	{
		sceneView->DrawSceneFileDialog();
		sceneView->DrawUnsavedChangesModal();
	}

	if (showingSceneTree)
		DrawSceneTreeWindow();

	if (showingSceneView)
		DrawSceneViewWindow();

	DrawScriptEditorWindows();

	if (showingLog)
		tabLog->Show();

	if (showingTabTools)
		tabTools->Show();

	if (showingTabProperties)
		tabProperties->Show();

	if (showingAssets)
		DrawAssetsWindow();

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

bool Editor::CreateNewProject(const std::string& parentDir, const std::string& name)
{
	std::string err;
	if (!project.Create(parentDir, name, &err))
	{
		projectDialogError = err;
		echo("ERROR: " + err);
		return false;
	}
	ClearAssetPreviews();
	selectedAssetRel.clear();
	CloseAllLuaScriptDocuments();
	CloseAllSceneDocuments();
	sceneView = CreateSceneDocument();
	const std::string sceneAbs = project.AbsolutePath(project.GetActiveSceneRel());
	if (sceneView)
		sceneView->LoadSceneFromFile(sceneAbs);
	GetActiveRenderDevice().WaitIdle();
	if (sceneView)
		sceneView->QueueMissingProjectModelThumbnails();
	AddRecentProject(project.GetProjectPath());
	echo("SUCCESS: Created project: " + project.GetProjectPath());
	UpdateWindowTitle();
	return true;
}

bool Editor::OpenProjectFromPath(const std::string& path)
{
	std::string err;
	if (!project.Open(path, &err))
	{
		projectDialogError = err;
		echo("ERROR: " + err);
		return false;
	}
	ClearAssetPreviews();
	selectedAssetRel.clear();
	CloseAllLuaScriptDocuments();
	CloseAllSceneDocuments();
	sceneView = CreateSceneDocument();
	const std::string sceneAbs = project.AbsolutePath(project.GetActiveSceneRel());
	std::error_code ec;
	if (sceneView && fs::exists(sceneAbs, ec))
		sceneView->LoadSceneFromFile(sceneAbs);
	GetActiveRenderDevice().WaitIdle();
	if (sceneView)
		sceneView->QueueMissingProjectModelThumbnails();
#ifdef LUA_BINDINGS
	if (const char* attachPath = std::getenv("PYROS_ATTACH_SCRIPT"))
	{
		if (attachPath[0] && sceneView)
			sceneView->DebugAutoAttachScript(attachPath);
	}
#endif
	AddRecentProject(project.GetProjectPath());
	echo("Opened project: " + project.GetProjectPath());
	UpdateWindowTitle();
	return true;
}

void Editor::CloseProjectImmediate()
{
	if (!project.IsOpen()) return;
	if (sceneView) sceneView->StopAssetSoundPreview();
	ClearAssetPreviews();
	selectedAssetRel.clear();
	CloseAllLuaScriptDocuments();
	CloseAllSceneDocuments();
	sceneView = CreateSceneDocument();
	project.Close();
	UpdateWindowTitle();
}

void Editor::CloseProject()
{
	if (!project.IsOpen()) return;
	if (!sceneView)
	{
		CloseProjectImmediate();
		return;
	}
	if (sceneView->ConfirmUnsavedThen(SceneEditor::UnsavedCloseProject))
		CloseProjectImmediate();
}

bool Editor::EditorAllowWindowClose()
{
	if (!AnySceneHasUnsavedWork())
		return true;
	// Scripts alone: save them and allow close (no scene modal needed).
	if (!AnySceneDocumentHasUnsavedWork())
	{
		if (SaveAllDirtyScripts())
			return true;
	}
	PromptQuitWithUnsaved();
	return false;
}

void Editor::FinishQuitIfClean()
{
	// Always flush open scripts first — the scene modal never did, which left
	// AnySceneHasUnsavedWork() true and re-entered a no-op prompt (hang).
	SaveAllDirtyScripts();

	// Save dirty scenes that already have a path.
	for (size_t i = 0; i < sceneDocs.size(); ++i)
	{
		SceneEditor* doc = sceneDocs[i];
		if (!doc || !doc->IsSceneDirty()) continue;
		if (!doc->GetScenePath().empty())
			doc->TrySaveCurrentScene();
	}
	if (project.IsOpen() && project.IsDirty())
		project.Save();

	if (!AnySceneHasUnsavedWork())
	{
		Close();
		return;
	}
	PromptQuitWithUnsaved();
}

void Editor::PromptQuitWithUnsaved()
{
	SceneEditor* dirty = NULL;
	for (size_t i = 0; i < sceneDocs.size(); ++i)
	{
		if (sceneDocs[i] && sceneDocs[i]->HasUnsavedWork())
		{
			dirty = sceneDocs[i];
			break;
		}
	}
	if (!dirty)
	{
		// Only scripts / nothing left that needs a scene dialog.
		SaveAllDirtyScripts();
		if (project.IsOpen() && project.IsDirty())
			project.Save();
		if (!AnySceneHasUnsavedWork())
			Close();
		return;
	}
	SetActiveSceneDocument(dirty);
	if (dirty->ConfirmUnsavedThen(SceneEditor::UnsavedQuitApp))
	{
		// This document reported nothing to save — keep draining quit.
		FinishQuitIfClean();
	}
}

void Editor::ClearAssetPreviews()
{
	for (std::map<std::string, Texture*>::iterator it = assetPreviewCache.begin();
		it != assetPreviewCache.end(); ++it)
	{
		if (it->second)
			deferredDestroyPreviews.push_back(it->second);
	}
	assetPreviewCache.clear();
}

void Editor::FlushDeferredPreviewDestroy()
{
	for (size_t i = 0; i < deferredDestroyPreviews.size(); ++i)
		delete deferredDestroyPreviews[i];
	deferredDestroyPreviews.clear();
}

Texture* Editor::GetAssetPreviewTexture(const std::string& absPath)
{
	if (absPath.empty()) return NULL;

	std::map<std::string, Texture*>::iterator it = assetPreviewCache.find(absPath);
	if (it != assetPreviewCache.end())
		return it->second;

	if (ProjectManager::IsTextureExtension(absPath))
	{
		Texture* tex = new Texture();
		if (!tex->LoadTexture(absPath, TextureType::Texture, false))
		{
			delete tex;
			return NULL;
		}
		assetPreviewCache[absPath] = tex;
		return tex;
	}

	if (ProjectManager::IsP3dm(absPath) && sceneView)
	{
		std::string thumbPath = SceneEditor::ModelThumbnailPath(absPath);
		std::error_code ec;
		if (!fs::exists(thumbPath, ec))
		{
			sceneView->QueueModelThumbnail(absPath);
			const fs::path legacy = fs::path(absPath).parent_path() / "preview.png";
			if (fs::exists(legacy, ec))
				thumbPath = legacy.string();
			else
				return NULL;
		}

		if (!thumbPath.empty() && fs::exists(thumbPath, ec))
		{
			Texture* tex = new Texture();
			if (tex->LoadTexture(thumbPath, TextureType::Texture, false))
			{
				assetPreviewCache[absPath] = tex;
				return tex;
			}
			delete tex;
		}
	}

	return NULL;
}

void Editor::DrawWelcomeScreen()
{
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::Begin("Welcome", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);
	ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.2f, 1.0f), "PyrosBuilder");
	ImGui::Separator();
	ImGui::TextDisabled("Create or open a project to start");
	ImGui::Spacing();
	if (ImGui::Button("New Project", ImVec2(280, 0)))
	{
		openNewProjectModal = true;
		projectDialogError.clear();
	}
	if (ImGui::Button("Open Project", ImVec2(280, 0)))
	{
		openOpenProjectModal = true;
		projectDialogError.clear();
	}

	if (!recentProjects.empty())
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("Recent projects");
		ImGui::BeginChild("##recentprojs", ImVec2(280, 180), true);
		for (size_t i = 0; i < recentProjects.size(); ++i)
		{
			const std::string& p = recentProjects[i];
			std::error_code ec;
			if (!fs::exists(p, ec))
				continue;
			const std::string label = fs::path(p).filename().string();
			ImGui::PushID((int)i);
			if (ImGui::Selectable(label.c_str()))
				OpenProjectFromPath(p);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", p.c_str());
			ImGui::PopID();
		}
		ImGui::EndChild();
	}
	ImGui::End();
}

std::string Editor::RecentProjectsFilePath()
{
	const char* home = std::getenv("HOME");
	if (home && home[0])
	{
		fs::path dir = fs::path(home) / "Library/Application Support/PyrosBuilder";
		std::error_code ec;
		fs::create_directories(dir, ec);
		return (dir / "recent_projects.txt").string();
	}
	return "recent_projects.txt";
}

void Editor::LoadRecentProjects()
{
	recentProjects.clear();
	std::ifstream in(RecentProjectsFilePath().c_str());
	if (!in) return;
	std::string line;
	while (std::getline(in, line))
	{
		while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
			line.pop_back();
		if (line.empty()) continue;
		std::error_code ec;
		if (!fs::exists(line, ec)) continue;
		recentProjects.push_back(line);
		if (recentProjects.size() >= (size_t)kMaxRecentProjects)
			break;
	}
}

void Editor::SaveRecentProjects()
{
	std::ofstream out(RecentProjectsFilePath().c_str(), std::ios::trunc);
	if (!out) return;
	for (size_t i = 0; i < recentProjects.size() && i < (size_t)kMaxRecentProjects; ++i)
		out << recentProjects[i] << "\n";
}

void Editor::AddRecentProject(const std::string& projectPathOrJson)
{
	if (projectPathOrJson.empty()) return;
	std::error_code ec;
	fs::path p = fs::weakly_canonical(projectPathOrJson, ec);
	if (ec) p = fs::path(projectPathOrJson);
	// Prefer the project folder (parent of project.json).
	if (p.filename() == "project.json")
		p = p.parent_path();
	const std::string key = p.string();
	if (key.empty()) return;

	for (std::vector<std::string>::iterator it = recentProjects.begin(); it != recentProjects.end(); )
	{
		if (*it == key) it = recentProjects.erase(it);
		else ++it;
	}
	recentProjects.insert(recentProjects.begin(), key);
	while (recentProjects.size() > (size_t)kMaxRecentProjects)
		recentProjects.pop_back();
	SaveRecentProjects();
}

void Editor::DrawProjectDialogs()
{
	if (openNewProjectModal)
	{
		ImGui::OpenPopup("New Project");
		openNewProjectModal = false;
	}
	if (openOpenProjectModal)
	{
		ImGui::OpenPopup("Open Project");
		openOpenProjectModal = false;
	}
	if (openProjectSettingsModal)
	{
		ImGui::OpenPopup("Project Settings");
		openProjectSettingsModal = false;
	}

	if (ImGui::BeginPopupModal("New Project", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("Directory", &newProjectDir);
		ImGui::SameLine();
		if (ImGui::Button("Browse##newprojdir"))
		{
			newProjectBrowseDir = !newProjectBrowseDir;
			if (newProjectDir.empty())
				newProjectDir = fs::current_path().string();
		}
		if (newProjectBrowseDir)
		{
			ImGui::BeginChild("##newprojdirs", ImVec2(520, 220), true);
			std::error_code ec;
			fs::path cur = newProjectDir.empty() ? fs::current_path() : fs::path(newProjectDir);
			if (!fs::is_directory(cur, ec))
				cur = fs::current_path();
			ImGui::TextUnformatted(cur.string().c_str());
			if (ImGui::Button("Up##newproj") && cur.has_parent_path())
				newProjectDir = cur.parent_path().string();
			ImGui::Separator();
			for (fs::directory_iterator it(cur, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
			{
				if (!it->is_directory(ec)) continue;
				const std::string name = it->path().filename().string();
				if (ImGui::Selectable(("[D] " + name).c_str()))
					newProjectDir = it->path().string();
			}
			if (ImGui::Button("Select##newprojdir"))
				newProjectBrowseDir = false;
			ImGui::EndChild();
		}
		ImGui::InputText("Project Name", &newProjectName);
		if (!projectDialogError.empty())
			ImGui::TextColored(ImVec4(1.f, 0.4f, 0.3f, 1.f), "%s", projectDialogError.c_str());
		if (ImGui::Button("Create Project"))
		{
			if (CreateNewProject(newProjectDir, newProjectName))
			{
				newProjectDir.clear();
				newProjectName.clear();
				newProjectBrowseDir = false;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel##newproj"))
		{
			newProjectBrowseDir = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Open Project", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("project.json", &openProjectPath);
		ImGui::SameLine();
		if (ImGui::Button("Browse##openproj"))
			ImGui::_priv::OpenLocation(openProjectPath.empty() ? std::string() : fs::path(openProjectPath).parent_path().string(), "json", &openProjectBrowse);
		if (openProjectBrowse)
		{
			std::string picked;
			if (ImGui::FilePath("##openprojbrowse", "", "json", &picked, 1024, &openProjectBrowse))
			{
				if (!picked.empty())
					openProjectPath = picked;
			}
		}
		if (!projectDialogError.empty())
			ImGui::TextColored(ImVec4(1.f, 0.4f, 0.3f, 1.f), "%s", projectDialogError.c_str());
		if (ImGui::Button("Open Project"))
		{
			if (OpenProjectFromPath(openProjectPath))
			{
				openProjectPath.clear();
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel##openproj"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Project Settings", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Global project options (stored in project.json).");
		ImGui::Separator();
		ImGui::InputText("Project Name", &projectSettingsName);
		ImGui::TextDisabled("Each scene has scenes/<SceneName>.lua — open via Scene menu, or click Scene in the tree → Properties.");
		if (sceneView && !sceneView->GetScenePath().empty())
		{
			if (ImGui::Button("Open Active Scene Script"))
			{
				sceneView->EnsureAndBindSceneCompanionScript();
				const std::string& p = sceneView->GetSceneMainScript();
				if (!p.empty())
					OpenLuaScriptDocument(p);
			}
		}
		if (!projectDialogError.empty())
			ImGui::TextColored(ImVec4(1.f, 0.4f, 0.3f, 1.f), "%s", projectDialogError.c_str());
		if (ImGui::Button("Apply", ImVec2(110, 0)))
		{
			project.SetProjectName(projectSettingsName);
			std::string err;
			if (!project.Save(&err))
			{
				projectDialogError = err;
				echo("ERROR: " + err);
			}
			else
			{
				UpdateWindowTitle();
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel##projsettings", ImVec2(110, 0)))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

void Editor::UpdateWindowTitle()
{
	std::string title = "PyrosBuilder";
	if (project.IsOpen())
	{
		title += " - ";
		title += project.GetProjectName().empty() ? "Project" : project.GetProjectName();
	}
	SDL_SetWindowTitle(GetSDLWindow(), title.c_str());
}

void Editor::DrawSceneTabBar()
{
	if (ImGui::BeginTabBar("##ProjectSceneTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll))
	{
		for (size_t i = 0; i < sceneDocs.size(); ++i)
		{
			SceneEditor* doc = sceneDocs[i];
			if (!doc) continue;

			bool open = true;
			ImGuiTabItemFlags flags = 0;
			if (doc->IsSceneDirty())
				flags |= ImGuiTabItemFlags_UnsavedDocument;

			char label[256];
			snprintf(label, sizeof(label), "%s###scene_tab_%u",
				doc->GetSceneDisplayName().c_str(), doc->GetDocumentId());

			if (ImGui::BeginTabItem(label, &open, flags))
			{
				if (sceneView != doc)
					SetActiveSceneDocument(doc);
				ImGui::EndTabItem();
			}
			if (!open)
				HostRequestCloseSceneDocument(doc);
		}

		if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip))
			OpenNewSceneDocument();

		ImGui::EndTabBar();
	}
}

void Editor::DrawScriptEditorWindows()
{
	// Keep docking target = Scene View's live dock node (same tab bar).
	if (ImGuiWindow* sv = ImGui::FindWindowByName("Scene View"))
	{
		if (sv->DockId != 0)
			dockCenterId = sv->DockId;
	}

	std::vector<uint32_t> closeIds;
	for (size_t i = 0; i < scriptDocs.size(); ++i)
	{
		CodeEditorDocument* doc = scriptDocs[i];
		if (!doc) continue;

		if (doc->editor.IsTextChanged())
			doc->dirty = true;

		char title[512];
		snprintf(title, sizeof(title), u8"\uf121 %s###script_win_%u",
			doc->GetDisplayName().c_str(), doc->id);

		const bool forceDock = (pendingSelectScriptId == doc->id);
		if (dockCenterId != 0)
		{
			// Always when opening/focusing so Assets/tree open lands next to Scene View
			// even if imgui.ini previously floated the window.
			ImGui::SetNextWindowDockID(dockCenterId,
				forceDock ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
		}
		if (forceDock)
			ImGui::SetNextWindowFocus();

		bool open = true;
		ImGuiWindowFlags wflags = ImGuiWindowFlags_None;
		if (doc->dirty)
			wflags |= ImGuiWindowFlags_UnsavedDocument;

		if (!ImGui::Begin(title, &open, wflags))
		{
			ImGui::End();
			if (!open)
				closeIds.push_back(doc->id);
			continue;
		}

		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
			activeScriptDoc = doc;
		if (pendingSelectScriptId == doc->id)
			pendingSelectScriptId = 0;

		if (ImGui::Button("Save") || (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)
			&& ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)))
		{
			if (doc->SaveToFile())
			{
				doc->editor.SetText(doc->editor.GetText());
				doc->dirty = false;
				echo("SUCCESS: Saved " + doc->absolutePath);
			}
			else
				echo("ERROR: Failed to save " + doc->absolutePath);
		}
		ImGui::SameLine();
		ImGui::Checkbox("Vim", &doc->vimEnabled);
		ImGui::SameLine();
		doc->DrawVimStatus();
		ImGui::SameLine();
		ImGui::TextDisabled("  F1 / Ctrl+N complete");
		if (doc->completionOpen || (doc->completionDebug && doc->completionDebug[0]))
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.45f, 0.85f, 1.f, 1.f), "  [%s]",
				doc->completionOpen ? (doc->completionDebug[0] ? doc->completionDebug : "open") : doc->completionDebug);
		}
		ImGui::Separator();

		const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (focused || doc->completionOpen)
			doc->HandleEditorInput();

		const bool editorKeys = focused && !doc->completionOpen && !doc->completionBlockEditorKeys
			&& (!doc->vimEnabled || doc->vimMode == CodeEditorDocument::VimMode::Insert);
		doc->editor.SetHandleKeyboardInputs(editorKeys);
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		doc->editor.Render("##lua_editor", avail, true);
		doc->editor.SetHandleKeyboardInputs(true);
		doc->completionBlockEditorKeys = false;
		doc->AfterEditorRender();
		if (doc->completionOpen)
			doc->DrawCompletionPopup();
		ImGui::End();

		if (!open)
			closeIds.push_back(doc->id);
	}
	for (size_t i = 0; i < closeIds.size(); ++i)
		CloseLuaScriptDocument(closeIds[i]);
}

void Editor::DrawSceneViewWindow()
{
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

	if (!ImGui::Begin("Scene View", &showingSceneView, flags))
	{
		ImGui::End();
		return;
	}

	DrawSceneTabBar();
	ImGui::Separator();
	if (sceneView)
		sceneView->ShowViewport();
	else
		ImGui::TextDisabled("No scene open");
	ImGui::End();
}

void Editor::DrawSceneTreeWindow()
{
	if (!ImGui::Begin("Scene Tree", &showingSceneTree))
	{
		ImGui::End();
		return;
	}
	ImGui::BeginChild("##scene_tree_scroll", ImVec2(0, 0), false);
	if (sceneView)
		sceneView->ShowHierarchy();
	else
		ImGui::TextDisabled("No scene open");
	ImGui::EndChild();
	ImGui::End();
}

void Editor::DrawAssetsWindow()
{
	if (!ImGui::Begin("Assets", &showingAssets))
	{
		assetsWindowHovered = false;
		ImGui::End();
		return;
	}

	assetsWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

	static int filter = 0;
	const char* filters[] = { "All", "Models", "Textures", "Sounds", "Shaders", "Lua", "Materials", "Scenes" };
	ImGui::SetNextItemWidth(160.f);
	ImGui::Combo("##assetfilter", &filter, filters, 8);
	if (!lastDropStatus.empty())
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.5f, 0.85f, 0.5f, 1.f), "%s", lastDropStatus.c_str());
	}
	ImGui::Separator();

	if (openNewScriptModal)
	{
		ImGui::OpenPopup("New Script");
		openNewScriptModal = false;
	}
	if (ImGui::BeginPopupModal("New Script", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Create a GameObject Lua script under assets/lua/");
		ImGui::SetNextItemWidth(280.f);
		ImGui::InputText("Name", &newScriptName);
		if (!newScriptError.empty())
			ImGui::TextColored(ImVec4(1.f, 0.45f, 0.4f, 1.f), "%s", newScriptError.c_str());
		ImGui::Spacing();
		if (ImGui::Button("Create", ImVec2(120, 0)))
		{
			std::string abs;
			std::string err;
			if (project.CreateLuaScript(newScriptName, abs, &err, LuaScriptKind::GameObject))
			{
				echo("SUCCESS: Created script " + abs);
				selectedAssetRel = project.RelativePath(abs);
				OpenLuaScriptDocument(abs);
				project.Save();
				ImGui::CloseCurrentPopup();
			}
			else
			{
				newScriptError = err;
				echo("ERROR: " + err);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	const bool highlightDrop = assetsWindowHovered;
	if (highlightDrop)
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.35f, 0.2f, 0.45f));

	ImGui::BeginChild("##assetlist", ImVec2(0, 0), false);

	std::vector<ProjectAssetEntry> assets;
	std::string under = "assets";
	switch (filter)
	{
	case 1: under = "assets/models"; break;
	case 2: under = "assets/textures"; break;
	case 3: under = "assets/sounds"; break;
	case 4: under = "assets/shaders"; break;
	case 5: under = "assets/lua"; break;
	case 6: under = "assets/materials"; break;
	case 7: under = "scenes"; break;
	default: under = "assets"; break;
	}
	if (filter == 7)
		project.ListAssets("scenes", assets, true);
	else if (filter == 0)
	{
		project.ListAssets("assets", assets, true);
		std::vector<ProjectAssetEntry> scenes;
		project.ListAssets("scenes", scenes, true);
		assets.insert(assets.end(), scenes.begin(), scenes.end());
	}
	else if (filter == 5)
	{
		project.ListAssets("assets/lua", assets, true);
		std::vector<ProjectAssetEntry> sceneScripts;
		project.ListAssets("scenes", sceneScripts, true);
		for (size_t si = 0; si < sceneScripts.size(); ++si)
		{
			if (ProjectManager::IsLuaExtension(sceneScripts[si].relativePath))
				assets.push_back(sceneScripts[si]);
		}
	}
	else
		project.ListAssets(under, assets, true);

	const float tileW = 92.f;
	const float thumbH = 72.f;
	const float labelH = 28.f;
	const float tileH = thumbH + labelH + 8.f;
	const float pad = 6.f;
	const float spacing = 8.f;
	const float avail = ImGui::GetContentRegionAvail().x;
	int columns = (int)((avail + spacing) / (tileW + spacing));
	if (columns < 1) columns = 1;

	int col = 0;
	for (size_t i = 0; i < assets.size(); ++i)
	{
		const ProjectAssetEntry& e = assets[i];
		if (e.isDirectory) continue;
		if (ProjectManager::IsInternalAssetPath(e.relativePath))
			continue;
		if (filter == 1 && !ProjectManager::IsP3dm(e.relativePath))
			continue;
		// Scenes filter: show .json and companion .lua
		if (filter == 7 && !ProjectManager::IsLuaExtension(e.relativePath)
			&& e.relativePath.find(".json") == std::string::npos)
			continue;

		const std::string abs = project.AbsolutePath(e.relativePath);
		const bool isLua = ProjectManager::IsLuaExtension(e.relativePath);
		const bool isScene = !isLua && ((e.relativePath.find("scenes/") == 0)
			|| (filter == 7 && e.relativePath.size() >= 5
				&& e.relativePath.compare(e.relativePath.size() - 5, 5, ".json") == 0));
		const bool isModel = ProjectManager::IsP3dm(e.relativePath);
		const bool isSound = ProjectManager::IsSoundExtension(e.relativePath);
		const bool isTex = ProjectManager::IsTextureExtension(e.relativePath);
		const bool selected = (selectedAssetRel == e.relativePath);

		const char* typeIcon = isScene ? u8"\uf1c0"
			: (isModel ? u8"\uf1b2"
			: (isSound ? u8"\uf028"
			: (isTex ? u8"\uf03e"
			: (isLua ? u8"\uf121"
			: (ProjectManager::IsShaderExtension(e.relativePath) ? u8"\uf0eb"
			: (ProjectManager::IsMaterialExtension(e.relativePath) ? u8"\uf53f"
			: u8"\uf15b"))))));

		if (col > 0)
			ImGui::SameLine(0.f, spacing);

		ImGui::PushID((int)i);
		ImGui::BeginGroup();

		const ImVec2 tileMin = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 tileMax(tileMin.x + tileW, tileMin.y + tileH);
		const ImU32 bg = selected
			? ImGui::GetColorU32(ImVec4(0.28f, 0.45f, 0.72f, 0.55f))
			: ImGui::GetColorU32(ImVec4(0.18f, 0.18f, 0.20f, 0.85f));
		const ImU32 border = selected
			? ImGui::GetColorU32(ImVec4(0.45f, 0.7f, 1.f, 1.f))
			: ImGui::GetColorU32(ImVec4(0.35f, 0.35f, 0.38f, 1.f));
		dl->AddRectFilled(tileMin, tileMax, bg, 4.f);
		dl->AddRect(tileMin, tileMax, border, 4.f);

		ImGui::SetNextItemAllowOverlap();
		ImGui::InvisibleButton("##tile", ImVec2(tileW, tileH));
		const bool hovered = ImGui::IsItemHovered();
		const bool clicked = ImGui::IsItemClicked();
		const bool dbl = hovered && ImGui::IsMouseDoubleClicked(0);

		if (hovered)
			dl->AddRect(tileMin, tileMax, ImGui::GetColorU32(ImVec4(0.6f, 0.75f, 1.f, 0.9f)), 4.f, 0, 1.5f);

		const ImVec2 thumbMin(tileMin.x + pad, tileMin.y + pad);
		const ImVec2 thumbMax(tileMin.x + tileW - pad, tileMin.y + pad + thumbH);
		Texture* preview = NULL;
		if (isTex || isModel)
			preview = GetAssetPreviewTexture(abs);
		void* tid = NULL;
		if (preview)
			tid = GetActiveRenderDevice().GetImGuiTextureID(preview->GetBindID(), preview->GetTextureType());

		if (tid)
		{
			const float tw = thumbMax.x - thumbMin.x;
			const float th = thumbMax.y - thumbMin.y;
			float iw = (float)preview->GetWidth();
			float ih = (float)preview->GetHeight();
			if (iw < 1.f) iw = 1.f;
			if (ih < 1.f) ih = 1.f;
			const float scale = (iw / ih > tw / th) ? (tw / iw) : (th / ih);
			const float dw = iw * scale;
			const float dh = ih * scale;
			const ImVec2 imgMin(thumbMin.x + (tw - dw) * 0.5f, thumbMin.y + (th - dh) * 0.5f);
			const ImVec2 imgMax(imgMin.x + dw, imgMin.y + dh);
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
			dl->AddImage((ImTextureID)tid, imgMin, imgMax);
#else
			dl->AddImage((ImTextureID)tid, imgMin, imgMax, ImVec2(0, 1), ImVec2(1, 0));
#endif
		}
		else
		{
			ImFont* font = ImGui::GetFont();
			const float iconSize = 28.f;
			ImVec2 isz = font->CalcTextSizeA(iconSize, FLT_MAX, 0.f, typeIcon);
			dl->AddText(font, iconSize,
				ImVec2(thumbMin.x + (thumbMax.x - thumbMin.x - isz.x) * 0.5f,
					thumbMin.y + (thumbMax.y - thumbMin.y - isz.y) * 0.5f),
				ImGui::GetColorU32(ImGuiCol_Text), typeIcon);
		}

		const char* name = e.name.c_str();
		{
			ImFont* font = ImGui::GetFont();
			const float fontSize = ImGui::GetFontSize();
			const ImVec2 labelMin(tileMin.x + pad, tileMin.y + pad + thumbH + 2.f);
			const ImVec2 labelMax(tileMin.x + tileW - pad, tileMax.y - 2.f);
			dl->PushClipRect(labelMin, labelMax, true);
			ImVec2 nsz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, name);
			dl->AddText(font, fontSize,
				ImVec2(tileMin.x + (tileW - ImMin(nsz.x, tileW - pad * 2.f)) * 0.5f, labelMin.y),
				ImGui::GetColorU32(ImGuiCol_Text), name);
			dl->PopClipRect();
		}

		if (isSound)
		{
			const bool playing = sceneView
				&& sceneView->IsAssetSoundPreviewPlaying()
				&& sceneView->GetAssetSoundPreviewPath() == abs;
			ImGui::SetCursorScreenPos(ImVec2(tileMax.x - 26.f, tileMin.y + 4.f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 3.f));
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.25f, 0.95f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.35f, 1.f));
			const bool playPressed = ImGui::SmallButton(playing ? u8"\uf04d##snd" : u8"\uf04b##snd");
			ImGui::PopStyleColor(2);
			ImGui::PopStyleVar();
			if (playPressed)
			{
				if (!sceneView)
					echo("WARNING: No scene open — cannot preview sound");
				else if (playing)
					sceneView->StopAssetSoundPreview();
				else
				{
					sceneView->SetAsActiveAudioDevice();
					sceneView->PreviewAssetSound(abs);
				}
			}
		}

		if (clicked)
			selectedAssetRel = e.relativePath;

		if (dbl)
		{
			selectedAssetRel = e.relativePath;
			if (isScene)
				OpenSceneDocument(abs);
			else if (isLua)
				OpenLuaScriptDocument(abs);
			else if (isSound)
			{
				// Double-click sound also previews (same as play button).
				if (sceneView)
				{
					sceneView->SetAsActiveAudioDevice();
					sceneView->PreviewAssetSound(abs);
				}
			}
			else if (sceneView)
				sceneView->PlaceAssetInScene(abs);
		}
		else if (clicked && isScene && !dbl)
			OpenSceneDocument(abs);
		else if (clicked && isLua && !dbl)
			OpenLuaScriptDocument(abs);

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
		{
			ImGui::SetDragDropPayload("ASSET_REL", e.relativePath.c_str(), e.relativePath.size() + 1);
			ImGui::TextUnformatted(e.name.c_str());
			if (tid)
			{
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
				ImGui::Image((ImTextureID)tid, ImVec2(64, isModel ? 48 : 64));
#else
				ImGui::Image((ImTextureID)tid, ImVec2(64, isModel ? 48 : 64), ImVec2(0, 1), ImVec2(1, 0));
#endif
			}
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginPopupContextItem("##assetctx"))
		{
			selectedAssetRel = e.relativePath;
			if (ImGui::MenuItem(u8"\uf0f6 New Script"))
			{
				openNewScriptModal = true;
				newScriptName = "NewScript";
				newScriptError.clear();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete"))
			{
				pendingDeleteAssetRel = e.relativePath;
				openDeleteAssetModal = true;
			}
			if (isScene && ImGui::MenuItem("Open Scene"))
				OpenSceneDocument(abs);
			if (isLua && ImGui::MenuItem("Open Script"))
				OpenLuaScriptDocument(abs);
			if ((isModel || isSound) && ImGui::MenuItem("Place in Scene") && sceneView)
				sceneView->PlaceAssetInScene(abs);
			ImGui::EndPopup();
		}

		ImGui::EndGroup();
		ImGui::PopID();

		col = (col + 1) % columns;
	}

	// Empty-area right-click (not over a tile).
	if (ImGui::BeginPopupContextWindow("##assets_bg_ctx",
		ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem(u8"\uf0f6 New Script"))
		{
			openNewScriptModal = true;
			newScriptName = "NewScript";
			newScriptError.clear();
		}
		ImGui::EndPopup();
	}

	ImGui::EndChild();

	if (highlightDrop)
		ImGui::PopStyleColor();

	if (openDeleteAssetModal)
	{
		ImGui::OpenPopup("Delete Asset");
		openDeleteAssetModal = false;
	}
	if (ImGui::BeginPopupModal("Delete Asset", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped("Delete \"%s\" from the project?\nThis cannot be undone.", pendingDeleteAssetRel.c_str());
		if (ProjectManager::IsP3dm(pendingDeleteAssetRel))
			ImGui::TextDisabled("Deletes the whole model package folder (textures included).");
		if (pendingDeleteAssetRel == project.GetActiveSceneRel())
			ImGui::TextColored(ImVec4(1.f, 0.6f, 0.2f, 1.f), "This is the active scene file.");
		ImGui::Spacing();
		if (ImGui::Button("Delete", ImVec2(120, 0)))
		{
			std::string err;
			const std::string abs = project.AbsolutePath(pendingDeleteAssetRel);
			if (assetPreviewCache.count(abs))
			{
				if (assetPreviewCache[abs])
					deferredDestroyPreviews.push_back(assetPreviewCache[abs]);
				assetPreviewCache.erase(abs);
			}
			if (project.DeleteAsset(pendingDeleteAssetRel, &err))
			{
				echo("SUCCESS: Deleted " + pendingDeleteAssetRel);
				if (selectedAssetRel == pendingDeleteAssetRel)
					selectedAssetRel.clear();
				project.Save();
			}
			else
				echo("ERROR: " + err);
			pendingDeleteAssetRel.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			pendingDeleteAssetRel.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::End();
}

// The editor never adds a post effect, so PostEffectsManager::
// ProcessPostEffects() returns at its effects.empty() guard - and that is the
// only thing in the engine that brackets a swapchain frame for a scene drawn
// entirely offscreen. On GL that is invisible (SDL_GL_SwapWindow presents
// regardless), but on Vulkan and Metal nothing acquired or presented a frame
// at all: a black window, and the ImGui draw never ran because the backends
// record it inside EndFrame() via UIRenderHook.
//
// BeginFrame must wrap DrawUI: viewport CaptureFrame and model thumbnails
// Bind offscreen FBOs. With !frameInProgress those binds call
// WaitAllFrameFences() (IslandDemo pre-frame sync) and the editor main
// thread spent whole seconds in vkWaitForFences every tick after load.
void Editor::Draw()
{
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
	GetActiveRenderDevice().BeginFrame();
#endif
	if (sceneView)
		sceneView->ProcessPendingModelThumbnails(1);
	DrawUI();
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
	GetActiveRenderDevice().EndFrame();
#endif
	ClassName::Draw();
	// After ImGui has consumed texture IDs from this frame's draw list.
	FlushDeferredPreviewDestroy();
}

void Editor::MouseMove(Event::Input::Info e)
{
	//gizmo->OnMouseMove(mPos.x, mPos.y);
}

void Editor::Shutdown()
{
    // All your Shutdown Code Here
	PyrosFileDrop::SetHandler(NULL);
	PyrosWindowClose::SetHandler(NULL);
	ClearAssetPreviews();
	FlushDeferredPreviewDestroy();

	CloseAllSceneDocuments();
	CloseAllLuaScriptDocuments();
	sceneView = NULL;

	delete sharedAudio;
	sharedAudio = NULL;
#ifdef LUA_BINDINGS
	luaReady = false;
#endif

	delete tabProperties;
	delete tabTools;
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

SceneEditor* Editor::CreateSceneDocument()
{
	SceneEditor* doc = new SceneEditor(nextSceneDocId++);
	doc->SetSharedAudioManager(sharedAudio);
#ifdef LUA_BINDINGS
	if (luaReady)
		doc->SetSharedLua(&lua);
#endif
	doc->Init(Width, Height);
	doc->SetProjectManager(&project);
	doc->SetHostCallbacks(&Editor::HostCloseProject, &Editor::HostQuitApp,
		&Editor::HostNewProject, &Editor::HostOpenProject,
		&Editor::HostQuitDiscardingUnsaved);
	doc->SetHostDocumentCallbacks(&Editor::HostActivateSceneDocument,
		&Editor::HostRequestCloseSceneDocument,
		&Editor::HostNewSceneDocument,
		&Editor::HostOpenSceneDocument,
		&Editor::HostOpenLuaScript);
	sceneDocs.push_back(doc);
	SetActiveSceneDocument(doc);
	return doc;
}

CodeEditorDocument* Editor::FindLuaScriptDocument(const std::string& absPath)
{
	if (absPath.empty()) return NULL;
	for (size_t i = 0; i < scriptDocs.size(); ++i)
	{
		if (scriptDocs[i] && scriptDocs[i]->absolutePath == absPath)
			return scriptDocs[i];
	}
	return NULL;
}

bool Editor::OpenLuaScriptDocument(const std::string& absPath)
{
	if (absPath.empty() || !project.IsOpen()) return false;
	if (CodeEditorDocument* existing = FindLuaScriptDocument(absPath))
	{
		activeScriptDoc = existing;
		pendingSelectScriptId = existing->id;
		return true;
	}

	CodeEditorDocument* doc = new CodeEditorDocument();
	doc->id = nextScriptDocId++;
	if (!doc->LoadFromFile(absPath))
	{
		echo("ERROR: Could not open script " + absPath);
		delete doc;
		return false;
	}
	scriptDocs.push_back(doc);
	activeScriptDoc = doc;
	pendingSelectScriptId = doc->id;
	echo("SUCCESS: Opened script " + absPath);
	return true;
}

void Editor::CloseLuaScriptDocument(uint32_t id)
{
	for (size_t i = 0; i < scriptDocs.size(); ++i)
	{
		if (!scriptDocs[i] || scriptDocs[i]->id != id) continue;
		CodeEditorDocument* doc = scriptDocs[i];
		if (activeScriptDoc == doc)
			activeScriptDoc = NULL;
		delete doc;
		scriptDocs.erase(scriptDocs.begin() + (std::ptrdiff_t)i);
		break;
	}
}

void Editor::CloseAllLuaScriptDocuments()
{
	for (size_t i = 0; i < scriptDocs.size(); ++i)
		delete scriptDocs[i];
	scriptDocs.clear();
	activeScriptDoc = NULL;
}

void Editor::DestroySceneDocument(SceneEditor* doc)
{
	if (!doc) return;
	const bool wasActive = (sceneView == doc);
	for (size_t i = 0; i < sceneDocs.size(); ++i)
	{
		if (sceneDocs[i] != doc) continue;
		doc->StopAssetSoundPreview();
		// ~SceneEditor calls Shutdown(); do not call it here or it runs twice.
		delete doc;
		sceneDocs.erase(sceneDocs.begin() + (std::ptrdiff_t)i);
		break;
	}
	if (wasActive)
		sceneView = sceneDocs.empty() ? NULL : sceneDocs.back();
	if (sceneView)
		SetActiveSceneDocument(sceneView);
	else
	{
		tabProperties->SetActive(NULL);
		tabTools->SetActive(NULL);
	}
}

void Editor::CloseAllSceneDocuments()
{
	pendingCloseSceneDocs.clear();
	while (!sceneDocs.empty())
		DestroySceneDocument(sceneDocs.back());
	sceneView = NULL;
}

void Editor::SetActiveSceneDocument(SceneEditor* doc)
{
	if (!doc) return;
	sceneView = doc;
	doc->SetAsActiveAudioDevice();
	tabProperties->SetActive(doc);
	tabTools->SetActive(doc);
	doc->BindSharedEditorHooks();
	if (project.IsOpen() && !doc->GetScenePath().empty())
	{
		const std::string rel = project.RelativePath(doc->GetScenePath());
		// Tab focus only — don't treat as unsaved project edits.
		if (!rel.empty())
			project.SetActiveSceneRel(rel, false);
	}
}

SceneEditor* Editor::FindSceneDocumentByPath(const std::string& absPath) const
{
	if (absPath.empty()) return NULL;
	for (size_t i = 0; i < sceneDocs.size(); ++i)
	{
		if (sceneDocs[i]->GetScenePath() == absPath)
			return sceneDocs[i];
	}
	return NULL;
}

bool Editor::OpenSceneDocument(const std::string& absPath)
{
	if (absPath.empty() || !project.IsOpen()) return false;
	if (SceneEditor* existing = FindSceneDocumentByPath(absPath))
	{
		SetActiveSceneDocument(existing);
		const std::string rel = project.RelativePath(absPath);
		if (!rel.empty())
		{
			project.SetActiveSceneRel(rel);
			project.Save();
		}
		return true;
	}
	SceneEditor* doc = CreateSceneDocument();
	if (!doc->LoadSceneFromFile(absPath))
	{
		DestroySceneDocument(doc);
		if (sceneDocs.empty())
			sceneView = CreateSceneDocument();
		echo("ERROR: failed to open scene: " + absPath);
		return false;
	}
	GetActiveRenderDevice().WaitIdle();
	const std::string rel = project.RelativePath(absPath);
	if (!rel.empty())
	{
		project.SetActiveSceneRel(rel);
		project.Save();
	}
	echo("Opened scene: " + (rel.empty() ? absPath : rel));
	return true;
}

bool Editor::OpenNewSceneDocument()
{
	if (!project.IsOpen()) return false;
	SceneEditor* doc = CreateSceneDocument();
	doc->NewScene();
	return true;
}

void Editor::FlushPendingSceneDocumentCloses()
{
	if (pendingCloseSceneDocs.empty()) return;
	std::vector<SceneEditor*> closing = pendingCloseSceneDocs;
	pendingCloseSceneDocs.clear();
	for (size_t i = 0; i < closing.size(); ++i)
	{
		SceneEditor* doc = closing[i];
		bool stillPresent = false;
		for (size_t j = 0; j < sceneDocs.size(); ++j)
			if (sceneDocs[j] == doc) { stillPresent = true; break; }
		if (!stillPresent) continue;
		if (doc->IsSceneDirty())
		{
			SetActiveSceneDocument(doc);
			echo("Save the scene before closing its tab (Scene → Save Scene).");
			continue;
		}
		DestroySceneDocument(doc);
		if (project.IsOpen() && sceneDocs.empty())
			sceneView = CreateSceneDocument();
	}
}

bool Editor::AnySceneDocumentHasUnsavedWork() const
{
	for (size_t i = 0; i < sceneDocs.size(); ++i)
		if (sceneDocs[i] && sceneDocs[i]->HasUnsavedWork())
			return true;
	return false;
}

bool Editor::SaveAllDirtyScripts()
{
	bool allOk = true;
	for (size_t i = 0; i < scriptDocs.size(); ++i)
	{
		CodeEditorDocument* doc = scriptDocs[i];
		if (!doc || !doc->dirty) continue;
		if (doc->SaveToFile())
		{
			doc->dirty = false;
			echo("SUCCESS: Saved " + doc->absolutePath);
		}
		else
		{
			allOk = false;
			echo("ERROR: Failed to save " + doc->absolutePath);
		}
	}
	return allOk;
}

bool Editor::AnySceneHasUnsavedWork() const
{
	if (AnySceneDocumentHasUnsavedWork())
		return true;
	for (size_t i = 0; i < scriptDocs.size(); ++i)
		if (scriptDocs[i] && scriptDocs[i]->dirty)
			return true;
	return false;
}

void Editor::HostActivateSceneDocument(SceneEditor* doc)
{
	Editor* ed = Editor::getInstance();
	if (ed) ed->SetActiveSceneDocument(doc);
}

void Editor::HostRequestCloseSceneDocument(SceneEditor* doc)
{
	Editor* ed = Editor::getInstance();
	if (!ed || !doc) return;
	for (size_t i = 0; i < ed->pendingCloseSceneDocs.size(); ++i)
		if (ed->pendingCloseSceneDocs[i] == doc) return;
	ed->pendingCloseSceneDocs.push_back(doc);
}

void Editor::HostNewSceneDocument()
{
	Editor* ed = Editor::getInstance();
	if (ed) ed->OpenNewSceneDocument();
}

void Editor::HostOpenSceneDocument(const std::string& absPath)
{
	Editor* ed = Editor::getInstance();
	if (ed) ed->OpenSceneDocument(absPath);
}

void Editor::HostOpenLuaScript(const std::string& absPath)
{
	Editor* ed = Editor::getInstance();
	if (ed) ed->OpenLuaScriptDocument(absPath);
}

Editor::~Editor() 
{
	if (instance == this) {
		instance = NULL;
	}
}
