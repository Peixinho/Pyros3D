//============================================================================
// Name        : Editor.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : ImGui Example
//============================================================================

#include "Editor.h"
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
// The agent bridge reaches into an animation document's live rig (bone
// names, current pose) - AnimationEditorDocument only forward-declares it.
#include "editor/AnimationPreview.h"
#include "editor/UI/OpenDir.h"
#include "editor/FileDropQueue.h"
#include "editor/AssetCommands.h"
#include <Pyros3D/Audio/AudioManager.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <FileDropHook.h>
#include <CloseHook.h>
#include <glad/glad.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <cfloat>
#include <set>
#include <sstream>

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
	ed->projectDialogError.clear();
	// A recent-project pick that was queued behind the unsaved-work prompt
	// resumes as that same pick, not as a file browser.
	if (!ed->pendingRecentProjectPath.empty())
	{
		const std::string path = ed->pendingRecentProjectPath;
		ed->pendingRecentProjectPath.clear();
		ed->OpenProjectFromPath(path);
		return;
	}
	ed->openOpenProjectModal = true;
}

// The asset New/Import entries, shared by the Assets menu in the menu bar and
// by both Assets-panel context menus. They were three different lists before,
// and only the panel ones were discoverable at all.
void Editor::ShowAssetCreateMenuItems()
{
	// No icons: fa-solid-900 has no glyph for the old u8"\uf0f6" and drew a
	// tofu box, and nothing else in any editor menu is iconised anyway.
	if (ImGui::MenuItem("New Script..."))
	{
		openNewScriptModal = true;
		newScriptName = "NewScript";
		newScriptError.clear();
	}
	if (ImGui::MenuItem("New Material..."))
	{
		openNewMaterialModal = true;
		newMaterialName = "NewMaterial";
		newMaterialError.clear();
	}
	if (ImGui::MenuItem("New Animation..."))
		openNewAnimationModal = true;
	ImGui::Separator();
	if (ImGui::MenuItem("Import Animation..."))
	{
		// Reuses the same browse-and-import path a dropped file takes; the
		// queue is drained in ProcessPendingFileDrops.
		openImportAnimationModal = true;
		importAnimationSource.clear();
		importAnimationName.clear();
		importAnimationError.clear();
	}
}

// The editor's palette. These are the same values DrawWelcomeScreen() has
// always used for the splash - the warm slate ground, the ember accent, the
// brown headers - lifted out so the whole editor wears them instead of the
// splash being the one warm screen in a stock-blue application.
namespace
{
	inline ImVec4 PyrosCol(int r, int g, int b, float a = 1.f)
	{
		return ImVec4((float)r / 255.f, (float)g / 255.f, (float)b / 255.f, a);
	}
}

void Editor::ApplyPyrosTheme()
{
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* c = style.Colors;

	// Grounds, darkest to lightest. The welcome screen's gradient runs
	// (42,38,40) -> (58,36,30); windows sit on the first, raised surfaces
	// (frames, headers) on warmer steps above it.
	const ImVec4 bgDeep    = PyrosCol(30, 27, 28);
	const ImVec4 bgWindow  = PyrosCol(42, 38, 40);
	const ImVec4 bgPanel   = PyrosCol(56, 48, 46);
	const ImVec4 bgRaised  = PyrosCol(72, 60, 55);

	// Ember accent, straight off the splash's primary button.
	const ImVec4 accent       = PyrosCol(224, 82, 41);
	const ImVec4 accentHover  = PyrosCol(245, 107, 51);
	const ImVec4 accentActive = PyrosCol(191, 61, 31);

	// Brown header ramp, straight off the splash's recent-projects list.
	const ImVec4 header       = PyrosCol(92, 61, 46);
	const ImVec4 headerHover  = PyrosCol(140, 77, 46);
	const ImVec4 headerActive = PyrosCol(179, 87, 41);

	const ImVec4 text     = PyrosCol(250, 240, 230);
	const ImVec4 textDim  = PyrosCol(158, 138, 128);
	const ImVec4 border   = PyrosCol(120, 84, 66, 0.60f);

	c[ImGuiCol_Text]                  = text;
	c[ImGuiCol_TextDisabled]          = textDim;
	c[ImGuiCol_WindowBg]              = bgWindow;
	c[ImGuiCol_ChildBg]               = ImVec4(0, 0, 0, 0);
	c[ImGuiCol_PopupBg]               = PyrosCol(38, 33, 34, 0.98f);
	c[ImGuiCol_Border]                = border;
	c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
	c[ImGuiCol_FrameBg]               = bgPanel;
	c[ImGuiCol_FrameBgHovered]        = bgRaised;
	c[ImGuiCol_FrameBgActive]         = header;
	c[ImGuiCol_TitleBg]               = bgDeep;
	c[ImGuiCol_TitleBgActive]         = header;
	c[ImGuiCol_TitleBgCollapsed]      = PyrosCol(30, 27, 28, 0.75f);
	c[ImGuiCol_MenuBarBg]             = bgDeep;
	c[ImGuiCol_ScrollbarBg]           = PyrosCol(24, 21, 22, 0.60f);
	c[ImGuiCol_ScrollbarGrab]         = bgRaised;
	c[ImGuiCol_ScrollbarGrabHovered]  = header;
	c[ImGuiCol_ScrollbarGrabActive]   = headerHover;
	c[ImGuiCol_CheckMark]             = accentHover;
	c[ImGuiCol_SliderGrab]            = accent;
	c[ImGuiCol_SliderGrabActive]      = accentHover;
	c[ImGuiCol_Button]                = bgRaised;
	c[ImGuiCol_ButtonHovered]         = headerHover;
	c[ImGuiCol_ButtonActive]          = accentActive;
	c[ImGuiCol_Header]                = header;
	c[ImGuiCol_HeaderHovered]         = headerHover;
	c[ImGuiCol_HeaderActive]          = headerActive;
	c[ImGuiCol_Separator]             = border;
	c[ImGuiCol_SeparatorHovered]      = headerHover;
	c[ImGuiCol_SeparatorActive]       = accent;
	c[ImGuiCol_ResizeGrip]            = PyrosCol(120, 84, 66, 0.40f);
	c[ImGuiCol_ResizeGripHovered]     = headerHover;
	c[ImGuiCol_ResizeGripActive]      = accent;
	c[ImGuiCol_Tab]                   = PyrosCol(48, 41, 40);
	c[ImGuiCol_TabHovered]            = headerHover;
	c[ImGuiCol_TabSelected]           = header;
	c[ImGuiCol_TabSelectedOverline]   = accent;
	c[ImGuiCol_TabDimmed]             = bgDeep;
	c[ImGuiCol_TabDimmedSelected]     = PyrosCol(64, 46, 38);
	c[ImGuiCol_TabDimmedSelectedOverline] = PyrosCol(120, 84, 66, 0.60f);
	c[ImGuiCol_DockingPreview]        = ImVec4(accent.x, accent.y, accent.z, 0.55f);
	c[ImGuiCol_DockingEmptyBg]        = bgDeep;
	c[ImGuiCol_PlotLines]             = PyrosCol(214, 175, 150);
	c[ImGuiCol_PlotLinesHovered]      = accentHover;
	c[ImGuiCol_PlotHistogram]         = accent;
	c[ImGuiCol_PlotHistogramHovered]  = accentHover;
	c[ImGuiCol_TableHeaderBg]         = header;
	c[ImGuiCol_TableBorderStrong]     = PyrosCol(96, 70, 58);
	c[ImGuiCol_TableBorderLight]      = PyrosCol(64, 52, 48);
	c[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
	c[ImGuiCol_TableRowBgAlt]         = ImVec4(1.f, 1.f, 1.f, 0.03f);
	c[ImGuiCol_TextSelectedBg]        = ImVec4(accent.x, accent.y, accent.z, 0.40f);
	c[ImGuiCol_DragDropTarget]        = accentHover;
	c[ImGuiCol_NavCursor]             = accentHover;
	c[ImGuiCol_NavWindowingHighlight] = text;
	c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.f, 0.f, 0.f, 0.55f);
	c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.f, 0.f, 0.f, 0.55f);

	// Geometry to match: the splash rounds its buttons and its recent list,
	// so the rest of the editor should not be all hard corners.
	style.WindowRounding    = 4.f;
	style.ChildRounding     = 4.f;
	style.FrameRounding     = 4.f;
	style.PopupRounding     = 4.f;
	style.ScrollbarRounding = 6.f;
	style.GrabRounding      = 4.f;
	style.TabRounding       = 4.f;
	style.WindowBorderSize  = 1.f;
	style.FrameBorderSize   = 0.f;
	style.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
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
	openNewMaterialModal = false;
	newMaterialName.clear();
	newMaterialError.clear();
	newMaterialKindCombo = 1; // default to Custom Shader - most materials made here are custom-shaded
	newMaterialCustomModeCombo = 0; // default to Node Graph
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
	activeMaterialDoc = NULL;
	nextMaterialDocId = 1;
	pendingSelectMaterialDocId = 0;
	pendingCloseScriptId = 0;
	pendingCloseMaterialId = 0;
	pendingCloseAnimationId = 0;
	activeAnimationDoc = NULL;
	nextAnimationDocId = 1;
	pendingSelectAnimationDocId = 0;
	openSaveAnimationAsModal = false;
	saveAnimationAsDocId = 0;
	saveAnimationAsThenClose = false;
	openNewAnimationModal = false;
	openImportAnimationModal = false;
	openImportAnimationBrowse = false;
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

		// Prefab spawning, for scripts running in play mode. Registered by
		// the host rather than by the engine's bindings, because a prefab is
		// a tooling concept - what the engine offers is DeserializeSubtree(),
		// and this is that pointed at a .prefab file. Deliberately identical
		// in name and behaviour to the binding PyrosPlayer registers: a
		// script that spawns enemies in play mode has to do the same thing
		// in a build, and two different APIs for it is how that stops being
		// true.
		{
			sol::table prefabTable = lua.create_table();
			Editor* self = this;
			prefabTable["instantiate"] = [self](const std::string& path) -> std::shared_ptr<LUA_GameObject> {
				const std::string abs = (self->project.IsOpen() && !path.empty()
					&& path.find(':') == std::string::npos && path[0] != '/')
					? self->project.AbsolutePath(path) : path;
				const nlohmann::json j = prefab::ReadPrefabFile(abs);
				if (!j.is_object())
				{
					echo("ERROR: Prefab.instantiate - could not read " + path);
					return nullptr;
				}
				SceneEditor* view = self->sceneView;
				return std::static_pointer_cast<LUA_GameObject>(
					SceneSerializer::DeserializeSubtree(j.dump(),
						view ? view->GetScenePath() : std::string(),
						view ? view->GetPhysics() : NULL, &self->lua, NULL));
			};
			lua["Prefab"] = prefabTable;
		}

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
	ApplyPyrosTheme();

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
	// Material properties take the panel over from the scene selection
	// whenever a material document has focus - see
	// DrawActiveMaterialProperties().
	tabProperties->SetOverrideDrawer([this]() { return DrawActiveMaterialProperties(); });
	tabTools = new ToolsTab(&showingTabTools);
	tabAI = new AIAssistantTab(&showingTabAI);
	// The model gets a live picture of the project, and its tool calls run
	// through the same dispatcher the MCP bridge uses - HandleAgentCommand
	// (pumped on the main thread by AIAssistantTab::Update()).
	tabAI->SetContextProvider([this]() { return BuildAIContext(); });
	tabAI->SetToolExecutor([this](const std::string& name, const nlohmann::json& args) -> nlohmann::json {
		nlohmann::json cmd;
		cmd["cmd"] = name;
		cmd["args"] = args.is_null() ? nlohmann::json::object() : args;
		return HandleAgentCommand(cmd);
	});
	// Editor Log panel is the only sink — no OS terminal spam. Everything
	// includes Info so Lua print() shows up.
	p3d::LOG::_LOG::SetMirrorStdout(false);
	p3d::LOG::_LOG::SetLevel(p3d::LOG::Level::Info);
	sharedAudio = new AudioManager();
#ifdef LUA_BINDINGS
	InitLuaHost();
#endif
	sceneView = CreateSceneDocument();
	showingLog = showingSceneTree = showingSceneView = showingTabProperties = showingTabTools = true;
	showingTabAI = true;
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

	// Local command server for external agents (MCP bridge). Loopback-only,
	// token-authenticated; commands are executed on the main thread from
	// Update() so the SceneGraph/render device are safe.
	agentServer.Start([this](const nlohmann::json& cmd) {
		return HandleAgentCommand(cmd);
	});
}

void Editor::LoadDefaultLayout()
{
	showingLog = showingSceneTree = showingSceneView = showingTabProperties = showingTabTools = true;
	showingTabAI = true;
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
	ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.34f, NULL, &center);
	// The right column splits into Tools/Properties (left of the column)
	// and the AI Assistant (right of Properties, as requested).
	ImGuiID rightTools = ImGui::DockBuilderSplitNode(right, ImGuiDir_Left, 0.55f, NULL, &right);
	ImGuiID rightBottom = ImGui::DockBuilderSplitNode(rightTools, ImGuiDir_Down, 0.5f, NULL, &rightTools);

	ImGui::DockBuilderDockWindow("Scene Tree", left);
	ImGui::DockBuilderDockWindow("Scene View", center);
	ImGui::DockBuilderDockWindow("Assets", bottom);
	ImGui::DockBuilderDockWindow("Log", bottom);
	ImGui::DockBuilderDockWindow("Tools", rightTools);
	ImGui::DockBuilderDockWindow("Properties", rightBottom);
	ImGui::DockBuilderDockWindow("AI Assistant", right);
	dockCenterId = center;

	ImGui::DockBuilderFinish(dockspaceID);
}

void Editor::Update()
{
	// Drain agent commands first so the result is visible in this frame's draw.
	agentServer.Process();
	// Executes the AI Assistant's queued tool calls on this (main) thread -
	// HandleAgentCommand touches the SceneGraph, so it must not run on the
	// client's worker thread.
	if (tabAI)
	{
		tabAI->Update(GetTime());
		// AI Assistant settings (provider/model/key/...) are per-project -
		// persist them into project.json when the user changes them.
		if (tabAI->ConsumeDirtySettings() && project.IsOpen())
		{
			ProjectSettings s = project.GetSettings();
			s.aiAssistant = tabAI->ToJson();
			project.SetSettings(s); // marks the project dirty
			project.Save();
		}
	}
	ProcessPendingFileDrops();
	FlushPendingSceneDocumentCloses();
	for (size_t i = 0; i < sceneDocs.size(); ++i)
		sceneDocs[i]->Update(GetTime());
	// DrawUI / viewport CaptureFrame / thumbnails run in Draw() after
	// BeginFrame so Vulkan offscreen binds skip WaitAllFrameFences
	// (that wait was freezing the editor: ShowViewport ran with
	// !frameInProgress every tick and parked on MoltenVK fence waits).
}

std::string Editor::BuildAIContext()
{
	if (!project.IsOpen())
		return "";
	std::ostringstream os;
	os << "Project: " << project.GetProjectName() << "\n";
	if (sceneView)
	{
		os << "Active scene: " << sceneView->GetSceneDisplayName()
		   << (sceneView->IsSceneDirty() ? " (unsaved changes)" : "") << "\n";
		// An outline, not the scene. This used to inline up to 12 KB of
		// AgentSceneState() JSON into the context of *every* message, which
		// is most of a small model's budget spent restating something the
		// assistant can ask for - and it went stale the moment it edited
		// anything. Names are what it needs to address an object; scene_state
		// and get_object give it the rest on demand.
		try
		{
			const nlohmann::json state = sceneView->AgentSceneState();
			if (state.contains("objects") && state["objects"].is_array())
			{
				const nlohmann::json& objs = state["objects"];
				os << "Objects (" << objs.size() << "): ";
				const size_t kMaxNamed = 60;
				for (size_t i = 0; i < objs.size() && i < kMaxNamed; ++i)
				{
					if (i) os << ", ";
					os << objs[i].value("name", std::string("?"));
				}
				if (objs.size() > kMaxNamed)
					os << ", … (" << (objs.size() - kMaxNamed) << " more)";
				os << "\n";
			}
		}
		catch (...)
		{
			// The outline is a bonus - never fail the chat over it.
		}
		os << "Call scene_state for the full graph, or get_object for one object.\n";
	}
	return os.str();
}

std::string Editor::AgentScenePathArg(const std::string& pathArg) const
{
	// Scene paths come back out of this API project-relative, so they have to
	// go back in that way too - an agent that reads "scenes/Level1.json" from
	// save_scene and feeds it to load_scene should not get "file not found".
	// Absolute is still accepted (a scene outside the project has no other
	// form), matching the convention AgentOpenMaterial already uses.
	if (pathArg.empty() || !project.IsOpen()) return pathArg;
	if (pathArg[0] == '/' || (pathArg.size() >= 2 && pathArg[1] == ':')) return pathArg;
	return project.AbsolutePath(pathArg);
}

MaterialEditorDocument* Editor::AgentOpenMaterial(const std::string& pathArg, std::string& errOut)
{
	if (pathArg.empty()) { errOut = "'path' is required"; return NULL; }
	if (!project.IsOpen()) { errOut = "no project open"; return NULL; }
	// Treat as project-relative unless it already looks like an absolute
	// filesystem path (matches ResolveTexturePath's convention elsewhere).
	std::string abs = pathArg;
	if (pathArg[0] != '/' && !(pathArg.size() >= 2 && pathArg[1] == ':'))
		abs = project.AbsolutePath(pathArg);
	if (!OpenMaterialDocument(abs))
	{
		errOut = "could not open material: " + pathArg;
		return NULL;
	}
	return FindMaterialDocumentByPath(abs);
}

MaterialEditorDocument* Editor::LoadMaterialQuietly(const std::string& pathArg, std::string& errOut)
{
	if (pathArg.empty()) { errOut = "'path' is required"; return NULL; }
	if (!project.IsOpen()) { errOut = "no project open"; return NULL; }
	std::string abs = pathArg;
	if (pathArg[0] != '/' && !(pathArg.size() >= 2 && pathArg[1] == ':'))
		abs = project.AbsolutePath(pathArg);

	// Already loaded somewhere (an open tab, or a previous quiet load) -
	// reuse it as-is, touching neither its tab visibility nor focus.
	if (MaterialEditorDocument* existing = FindMaterialDocumentByPath(abs))
		return existing;

	MaterialEditorDocument* doc = new MaterialEditorDocument();
	doc->id = nextMaterialDocId++;
	BindUndoRouting(doc);
	doc->hiddenFromTabs = true;
	if (!MaterialEditor::LoadFromFile(*doc, abs, project.GetProjectPath(), UseDeferredGBuffer()))
	{
		delete doc;
		errOut = "could not open material: " + pathArg;
		return NULL;
	}
	// Kept in materialDocs (not drawn - see DrawMaterialEditorWindows'
	// hiddenFromTabs check) purely so it stays alive for as long as
	// whatever gets assigned this material still points at its compiled
	// Shader (CustomShaderMaterial::SetShader doesn't take ownership - the
	// doc's compiledShader unique_ptr is the real owner).
	materialDocs.push_back(doc);
	return doc;
}

bool Editor::AssignMaterialAsset(const std::string& objectName, int submeshIndex, const std::string& materialPath, std::string& errOut)
{
	// Quiet load - picking a material from the Properties panel's list (or
	// the assign_material agent command) should just attach it, not pop
	// open a Material Editor tab the way explicitly opening one for editing
	// does (see LoadMaterialQuietly).
	MaterialEditorDocument* doc = LoadMaterialQuietly(materialPath, errOut);
	if (!doc) return false;
	if (!doc->currentMaterial) { errOut = "material has no constructed instance yet"; return false; }
	if (!sceneView) { errOut = "no scene open"; return false; }
	return sceneView->AgentAssignMaterial(objectName, submeshIndex, doc->currentMaterial, errOut);
}

std::string Editor::HostAssignMaterialAsset(const std::string& objectName, int submeshIndex, const std::string& materialPath)
{
	Editor* ed = Editor::getInstance();
	if (!ed) return "editor not available";
	std::string err;
	if (!ed->AssignMaterialAsset(objectName, submeshIndex, materialPath, err))
		return err;
	return std::string();
}

nlohmann::json Editor::HandleAgentCommand(const nlohmann::json& cmd)
{
	const std::string name = cmd.value("cmd", std::string());
	const nlohmann::json& a = cmd.contains("args") ? cmd["args"] : nlohmann::json::object();

	auto A = [&](const char* k) -> std::string { return a.is_object() ? a.value(k, std::string()) : std::string(); };
	auto AV = [&](const char* k) -> std::vector<f32> {
		std::vector<f32> out;
		if (a.is_object() && a.contains(k) && a[k].is_array())
			for (auto& v : a[k])
				if (v.is_number()) out.push_back((f32)v.get<double>());
		return out;
	};

	if (name == "ping")
	{
		nlohmann::json r;
		r["pong"] = true;
		r["server"] = "PyrosBuilder";
		r["port"] = (int)agentServer.GetPort();
		return r;
	}

	if (name == "status")
	{
		nlohmann::json r;
		r["editor"] = "PyrosBuilder";
		r["agentServer"] = agentServer.IsRunning();
		r["port"] = (int)agentServer.GetPort();
		r["projectOpen"] = project.IsOpen();
		// projectPath is the one absolute path in the agent API, and it is
		// deliberate: it is the anchor every other path is relative to, and
		// an out-of-process caller (the MCP bridge) has no other way to turn
		// "scenes/Level1.json" into a file on disk. It is never shown in the
		// editor's own UI. Everything else below is project-relative.
		if (project.IsOpen())
			r["projectPath"] = project.GetProjectPath();
		if (sceneView)
		{
			r["scene"] = sceneView->GetSceneDisplayName();
			r["scenePath"] = project.DisplayPath(sceneView->GetScenePath());
			r["dirty"] = sceneView->IsSceneDirty();
			r["playing"] = sceneView->IsPlaying();
		}
		return r;
	}

	if (name == "log")
	{
		int lines = 50;
		if (a.is_object() && a.contains("lines") && a["lines"].is_number())
			lines = (int)a["lines"].get<int>();
		nlohmann::json r;
		r["log"] = SceneEditor::AgentLogTail(lines);
		return r;
	}

	if (name == "list_scenes")
	{
		if (!project.IsOpen())
			throw std::runtime_error("no project open");
		std::vector<std::string> scenes;
		project.ListScenes(scenes);
		nlohmann::json r;
		r["scenes"] = scenes;
		r["active"] = project.GetActiveSceneRel();
		return r;
	}

	if (name == "list_assets")
	{
		if (!project.IsOpen())
			throw std::runtime_error("no project open");
		const std::string under = A("under");
		const bool recursive = a.is_object() ? a.value("recursive", true) : true;
		const std::string ext = A("extension");
		std::vector<ProjectAssetEntry> entries;
		project.ListAssets(under, entries, recursive);
		nlohmann::json arr = nlohmann::json::array();
		for (const ProjectAssetEntry& e : entries)
		{
			if (e.isDirectory) continue;
			if (!ext.empty())
			{
				if (e.relativePath.size() < ext.size()) continue;
				if (e.relativePath.compare(e.relativePath.size() - ext.size(), ext.size(), ext) != 0) continue;
			}
			arr.push_back(e.relativePath);
		}
		nlohmann::json r;
		r["assets"] = std::move(arr);
		return r;
	}

	// ---- Animation Editor ------------------------------------------------
	// Every command below drives the same document/AnimationEditor
	// operations the panel's own buttons do, so an agent and a human editing
	// the same clip cannot diverge. Each resolves its target document from
	// an optional "path" (project-relative or absolute .p3da) and otherwise
	// falls back to whichever animation window last had focus.
	if (name.rfind("animation", 0) == 0 || name == "list_animations"
		|| name == "import_animation" || name == "open_animation" || name == "save_animation"
		|| name == "key_animation_pose" || name == "set_animation_keyframe"
		|| name == "delete_animation_keyframe" || name == "set_animation_pose"
		|| name == "add_animation_clip" || name == "remove_animation_clip"
		|| name == "rename_animation_clip" || name == "set_animation_clip_duration"
		|| name == "select_animation_bone" || name == "undo_animation" || name == "redo_animation"
		|| name == "animation_blend" || name == "animation_ik" || name == "animation_joint_limit")
	{
		if (name == "list_animations")
		{
			if (!project.IsOpen()) throw std::runtime_error("no project open");
			std::vector<ProjectAssetEntry> entries;
			project.ListAssets("assets", entries, true);
			nlohmann::json arr = nlohmann::json::array();
			for (const ProjectAssetEntry& e : entries)
			{
				if (e.isDirectory || !ProjectManager::IsAnimationExtension(e.relativePath)) continue;
				nlohmann::json j;
				j["path"] = e.relativePath;
				const std::string abs = project.AbsolutePath(e.relativePath);
				const std::string bound = LookupAnimationMeshBinding(abs);
				if (!bound.empty()) j["mesh"] = project.DisplayPath(bound);
				// Whether an Animation Editor tab currently holds this file.
				// An out-of-process tool has to know before it edits the file
				// on disk, because this editor would write its own in-memory
				// copy over that edit on the next save - and asking must not
				// itself open anything, which is why this lives here rather
				// than being probed with animation_state (that one opens the
				// document on demand, by design).
				j["open"] = (FindAnimationDocumentByPath(abs) != NULL);
				arr.push_back(j);
			}
			nlohmann::json r;
			r["animations"] = std::move(arr);
			// Open documents including ones never saved (no path yet), which
			// the asset walk above cannot see at all.
			nlohmann::json open = nlohmann::json::array();
			for (size_t i = 0; i < animationDocs.size(); i++)
			{
				if (!animationDocs[i]) continue;
				nlohmann::json d;
				d["name"] = animationDocs[i]->displayName;
				d["path"] = animationDocs[i]->absolutePath.empty()
					? std::string() : project.DisplayPath(animationDocs[i]->absolutePath);
				d["absolutePath"] = animationDocs[i]->absolutePath;
				d["dirty"] = animationDocs[i]->dirty;
				open.push_back(d);
			}
			r["openDocuments"] = std::move(open);
			return r;
		}

		if (name == "import_animation")
		{
			if (!project.IsOpen()) throw std::runtime_error("no project open");
			const std::string src = A("source");
			if (src.empty()) throw std::runtime_error("source required");
			std::string out, err;
			if (!project.ImportAnimation(src, A("name"), out, &err))
				throw std::runtime_error(err.empty() ? "import failed" : err);
			project.Save();
			nlohmann::json r;
			r["path"] = project.DisplayPath(out);
			if (a.value("open", false)) r["opened"] = OpenAnimationDocument(out);
			return r;
		}

		// Resolve the target document for everything else.
		AnimationEditorDocument* doc = NULL;
		const std::string pathArg = A("path");
		if (!pathArg.empty())
		{
			const std::string abs = (pathArg.find('/') == 0 || pathArg.find(':') != std::string::npos)
				? pathArg : project.AbsolutePath(pathArg);
			doc = FindAnimationDocumentByPath(abs);
			if (!doc && name == "open_animation")
			{
				if (!OpenAnimationDocument(abs))
					throw std::runtime_error("could not open " + pathArg);
				doc = FindAnimationDocumentByPath(abs);
				// A .p3dm opens an unsaved document, which has no path to
				// find it by - it is the newest document instead.
				if (!doc && !animationDocs.empty()) doc = animationDocs.back();
			}
			if (!doc)
			{
				// Opening on demand is what an agent means by naming a file.
				if (!OpenAnimationDocument(abs))
					throw std::runtime_error("could not open " + pathArg);
				doc = FindAnimationDocumentByPath(abs);
			}
		}
		else doc = activeAnimationDoc;

		if (!doc)
			throw std::runtime_error("no animation document open (pass \"path\")");

		// Everything below needs the rig loaded for bone lookups, and the
		// panel only builds the preview when it draws. A background window
		// would otherwise report "unknown bone" for every bone it has.
		AnimationEditor::EnsurePreview(*doc);
		AnimationPreview* pv = doc->preview.get();

		auto clipIndexArg = [&](const char* key) -> int {
			if (!a.is_object() || !a.contains(key)) return doc->activeClip;
			if (a[key].is_number()) return (int)a[key].get<int>();
			if (a[key].is_string())
			{
				const std::string want = a[key].get<std::string>();
				for (size_t i = 0; i < doc->clips.size(); i++)
					if (doc->clips[i].AnimationName == want) return (int)i;
				throw std::runtime_error("no clip named '" + want + "'");
			}
			return doc->activeClip;
		};

		auto describe = [&]() -> nlohmann::json {
			nlohmann::json r;
			r["path"] = doc->absolutePath.empty() ? std::string("(unsaved)") : project.DisplayPath(doc->absolutePath);
			r["name"] = doc->displayName;
			r["dirty"] = doc->dirty;
			r["mesh"] = doc->meshPath.empty() ? std::string() : project.DisplayPath(doc->meshPath);
			r["activeClip"] = doc->activeClip;
			r["playhead"] = doc->playhead;
			r["playing"] = doc->playing;
			r["selectedBone"] = doc->selectedBoneName;
			r["canUndo"] = doc->undo.CanUndo();
			r["canRedo"] = doc->undo.CanRedo();
			nlohmann::json clips = nlohmann::json::array();
			for (size_t i = 0; i < doc->clips.size(); i++)
			{
				nlohmann::json c;
				c["id"] = (int)i;
				c["name"] = doc->clips[i].AnimationName;
				c["duration"] = doc->clips[i].Duration;
				c["channels"] = (int)doc->clips[i].Channels.size();
				int keys = 0;
				for (size_t ch = 0; ch < doc->clips[i].Channels.size(); ch++)
					keys += (int)(doc->clips[i].Channels[ch].positions.size()
						+ doc->clips[i].Channels[ch].rotations.size()
						+ doc->clips[i].Channels[ch].scales.size());
				c["keys"] = keys;
				clips.push_back(c);
			}
			r["clips"] = std::move(clips);
			if (pv && pv->instance) r["bones"] = (int)pv->instance->GetNumberBones();
			if (pv && !pv->loadError.empty()) r["meshError"] = pv->loadError;
			return r;
		};

		if (name == "open_animation" || name == "animation_state")
		{
			// Selecting the clip the timeline shows - the UI's clip combo,
			// reachable from an agent.
			if (a.is_object() && a.contains("clip"))
			{
				const int want = clipIndexArg("clip");
				if (want >= 0 && want < (int)doc->clips.size())
				{
					doc->activeClip = want;
					doc->selectedKeys.clear();
					AnimationEditor::SetPlayhead(*doc, 0.f);
				}
			}
			// Binding a rig is part of opening as far as an agent is
			// concerned - it has no other way to say "preview this on that".
			const std::string meshArg = A("mesh");
			if (!meshArg.empty())
			{
				const std::string meshAbs = (meshArg.find('/') == 0) ? meshArg : project.AbsolutePath(meshArg);
				doc->meshPath = meshAbs;
				AnimationEditor::EnsurePreview(*doc);
				pv = doc->preview.get();
				if (!doc->absolutePath.empty()) StoreAnimationMeshBinding(doc->absolutePath, meshAbs);
			}
			return describe();
		}

		if (name == "animation_skeleton")
		{
			if (!pv || !pv->instance) throw std::runtime_error("no rig bound - pass \"mesh\"");
			nlohmann::json arr = nlohmann::json::array();
			const std::vector<Bone>& bones = pv->instance->GetSkeletonBones();
			for (size_t i = 0; i < bones.size(); i++)
			{
				nlohmann::json b;
				b["id"] = bones[i].self;
				b["name"] = bones[i].name;
				b["parent"] = bones[i].parent;
				b["animated"] = doc->FindChannel(doc->activeClip, bones[i].name) >= 0;
				const Vec3 wp = pv->instance->GetBoneGlobalTransform(bones[i].self).GetTranslation();
				b["position"] = { wp.x, wp.y, wp.z };
				// Where this joint last projected inside the viewport image,
				// in pixels from its top-left. Exposed so a caller driving the
				// editor (the agent harness, a UI test) can click a specific
				// joint instead of guessing at coordinates off a screenshot.
				// z <= 0 means the joint is behind the camera and not
				// clickable this frame.
				if (bones[i].self >= 0 && bones[i].self < (int)pv->boneScreenPos.size())
				{
					const Vec3 sp = pv->boneScreenPos[bones[i].self];
					b["screen"] = { sp.x, sp.y, sp.z };
				}
				arr.push_back(b);
			}
			nlohmann::json r;
			r["bones"] = std::move(arr);
			r["viewport"] = { pv->width, pv->height };
			// Desktop position of the viewport image's top-left corner; add a
			// bone's "screen" to it to get a clickable point.
			r["viewportScreenOrigin"] = { pv->imageScreenX, pv->imageScreenY };
			return r;
		}

		if (name == "animation_keys")
		{
			const int clip = clipIndexArg("clip");
			if (clip < 0 || clip >= (int)doc->clips.size()) throw std::runtime_error("clip out of range");
			const std::string boneArg = A("bone");
			nlohmann::json arr = nlohmann::json::array();
			for (size_t ch = 0; ch < doc->clips[clip].Channels.size(); ch++)
			{
				const Channel& c = doc->clips[clip].Channels[ch];
				if (!boneArg.empty() && c.NodeName != boneArg) continue;
				std::vector<float> times;
				doc->CollectKeyTimes(clip, (int)ch, times);
				nlohmann::json j;
				j["bone"] = c.NodeName;
				j["times"] = times;
				j["positionKeys"] = (int)c.positions.size();
				j["rotationKeys"] = (int)c.rotations.size();
				j["scaleKeys"] = (int)c.scales.size();
				arr.push_back(j);
			}
			nlohmann::json r;
			r["clip"] = clip;
			r["channels"] = std::move(arr);
			return r;
		}

		if (name == "save_animation")
		{
			std::string target = A("as");
			if (!target.empty())
			{
				if (target.size() < 5 || target.compare(target.size() - 5, 5, ".p3da") != 0) target += ".p3da";
				if (target.find('/') == std::string::npos) target = "assets/animations/" + target;
				target = project.AbsolutePath(target);
			}
			else target = doc->absolutePath;
			if (target.empty())
				throw std::runtime_error("this animation has never been saved - pass \"as\"");
			if (!SaveAnimationDocument(doc, target))
				throw std::runtime_error("save failed - see the editor log");
			nlohmann::json r;
			r["saved"] = project.DisplayPath(target);
			return r;
		}

		if (name == "add_animation_clip")
		{
			const std::string clipName = A("name");
			const float dur = a.is_object() && a.contains("duration") && a["duration"].is_number()
				? (float)a["duration"].get<double>() : 1.f;
			int created = -1;
			doc->PushSnapshotEdit("Add clip", [&]() {
				created = doc->AddClip(clipName.empty() ? "Clip" : clipName, dur);
			});
			doc->activeClip = created;
			nlohmann::json r;
			r["clip"] = created;
			r["name"] = doc->clips[created].AnimationName;
			return r;
		}

		if (name == "remove_animation_clip")
		{
			const int clip = clipIndexArg("clip");
			if (clip < 0 || clip >= (int)doc->clips.size()) throw std::runtime_error("clip out of range");
			doc->PushSnapshotEdit("Delete clip", [&]() { doc->RemoveClip(clip); });
			nlohmann::json r;
			r["removed"] = clip;
			r["remaining"] = (int)doc->clips.size();
			// Removing a clip renumbers the ones after it, and those ids are
			// what scenes Play(). Say so rather than leaving it implicit.
			r["warning"] = "clip ids after the removed one shifted down by 1";
			return r;
		}

		if (name == "rename_animation_clip")
		{
			const int clip = clipIndexArg("clip");
			const std::string newName = A("name");
			if (newName.empty()) throw std::runtime_error("name required");
			if (clip < 0 || clip >= (int)doc->clips.size()) throw std::runtime_error("clip out of range");
			doc->PushSnapshotEdit("Rename clip", [&]() { doc->RenameClip(clip, newName); });
			nlohmann::json r;
			r["clip"] = clip;
			r["name"] = newName;
			return r;
		}

		if (name == "set_animation_clip_duration")
		{
			const int clip = clipIndexArg("clip");
			if (clip < 0 || clip >= (int)doc->clips.size()) throw std::runtime_error("clip out of range");
			if (!a.is_object() || !a.contains("duration") || !a["duration"].is_number())
				throw std::runtime_error("duration required");
			const float dur = (float)a["duration"].get<double>();
			doc->PushSnapshotEdit("Change clip length", [&]() { doc->SetClipDuration(clip, dur); });
			nlohmann::json r;
			r["clip"] = clip;
			r["duration"] = doc->clips[clip].Duration;
			return r;
		}

		if (name == "set_animation_pose")
		{
			if (!pv || !pv->instance) throw std::runtime_error("no rig bound - pass \"mesh\"");
			const std::string boneName = A("bone");
			const int boneId = pv->FindBone(boneName);
			if (boneId < 0) throw std::runtime_error("unknown bone '" + boneName + "'");

			// Start from what the bone is showing right now, so an agent can
			// set rotation alone without flattening the position it already
			// has (and vice versa).
			// Rotations cross this API in DEGREES - that is what the tool
			// documentation promises and what anyone typing an angle
			// means - while the engine's Euler conversions
			// (SetRotationFromEuler / GetEulerFromRotationMatrix) are
			// radians throughout. Converting at the boundary is the whole
			// of the difference; getting it wrong is silent, since 75
			// radians is a perfectly valid rotation (it just isn't 75
			// degrees - it wraps to about -23).
			Matrix local = pv->instance->GetBoneLocalTransform(boneId);
			Vec3 pos = local.GetTranslation();
			const Vec3 eulerRad = local.GetEulerFromRotationMatrix();
			Vec3 eulerDeg((f32)RADTODEG(eulerRad.x), (f32)RADTODEG(eulerRad.y), (f32)RADTODEG(eulerRad.z));
			const std::vector<f32> pArg = AV("position");
			const std::vector<f32> rArg = AV("rotation");
			if (pArg.size() == 3) pos = Vec3(pArg[0], pArg[1], pArg[2]);
			if (rArg.size() == 3) eulerDeg = Vec3(rArg[0], rArg[1], rArg[2]);

			Quaternion q;
			q.SetRotationFromEuler(Vec3((f32)DEGTORAD(eulerDeg.x), (f32)DEGTORAD(eulerDeg.y), (f32)DEGTORAD(eulerDeg.z)));
			Matrix m = q.ConvertToMatrix();
			m.Translate(pos);

			pv->poseOverrides[boneId] = m;
			pv->instance->SetBoneLocalTransform(boneId, m);
			pv->instance->RefreshSkinning();
			doc->selectedBone = boneId;
			doc->selectedBoneName = boneName;

			nlohmann::json r;
			r["bone"] = boneName;
			r["position"] = { pos.x, pos.y, pos.z };
			r["rotation"] = { eulerDeg.x, eulerDeg.y, eulerDeg.z };
			r["keyed"] = false;
			if (a.value("key", false))
			{
				const float t = doc->SnapTime(doc->playhead);
				doc->PushSnapshotEdit("Key bone '" + boneName + "'", [&]() {
					AnimationEditor::KeyBoneAtTime(*doc, boneId, t);
				});
				pv->poseOverrides.erase(boneId);
				r["keyed"] = true;
				r["time"] = t;
			}
			return r;
		}

		if (name == "set_animation_keyframe")
		{
			if (!pv || !pv->instance) throw std::runtime_error("no rig bound - pass \"mesh\"");
			const int clip = clipIndexArg("clip");
			if (clip < 0 || clip >= (int)doc->clips.size()) throw std::runtime_error("clip out of range");
			const std::string boneName = A("bone");
			if (pv->FindBone(boneName) < 0) throw std::runtime_error("unknown bone '" + boneName + "'");
			const float time = a.is_object() && a.contains("time") && a["time"].is_number()
				? (float)a["time"].get<double>() : doc->playhead;

			// Values default to whatever the bone is posed at, so
			// "key this bone here" needs no numbers at all.
			const int boneId = pv->FindBone(boneName);
			Matrix local = pv->instance->GetBoneLocalTransform(boneId);
			Vec3 pos = local.GetTranslation();
			const Vec3 eulerRad = local.GetEulerFromRotationMatrix();
			// Degrees across the API, radians inside - see set_animation_pose.
			Vec3 eulerDeg((f32)RADTODEG(eulerRad.x), (f32)RADTODEG(eulerRad.y), (f32)RADTODEG(eulerRad.z));
			const std::vector<f32> pArg = AV("position");
			const std::vector<f32> rArg = AV("rotation");
			if (pArg.size() == 3) pos = Vec3(pArg[0], pArg[1], pArg[2]);
			if (rArg.size() == 3) eulerDeg = Vec3(rArg[0], rArg[1], rArg[2]);
			Quaternion q;
			q.SetRotationFromEuler(Vec3((f32)DEGTORAD(eulerDeg.x), (f32)DEGTORAD(eulerDeg.y), (f32)DEGTORAD(eulerDeg.z)));

			const bool doPos = a.value("keyPosition", true);
			const bool doRot = a.value("keyRotation", true);
			doc->PushSnapshotEdit("Key bone '" + boneName + "'", [&]() {
				const int ch = doc->FindOrCreateChannel(clip, boneName);
				doc->SetKey(clip, ch, time, pos, q, Vec3(1, 1, 1), doPos, doRot, false);
			});
			nlohmann::json r;
			r["clip"] = clip;
			r["bone"] = boneName;
			r["time"] = time;
			return r;
		}

		if (name == "delete_animation_keyframe")
		{
			const int clip = clipIndexArg("clip");
			if (clip < 0 || clip >= (int)doc->clips.size()) throw std::runtime_error("clip out of range");
			const std::string boneName = A("bone");
			const int ch = doc->FindChannel(clip, boneName);
			if (ch < 0) throw std::runtime_error("clip has no keys for bone '" + boneName + "'");
			if (!a.is_object() || !a.contains("time") || !a["time"].is_number())
				throw std::runtime_error("time required");
			const float time = (float)a["time"].get<double>();
			bool removed = false;
			doc->PushSnapshotEdit("Delete key", [&]() {
				removed = doc->DeleteKeysAtTime(clip, ch, time);
				doc->PruneEmptyChannels(clip);
			});
			if (!removed) throw std::runtime_error("no key at that time");
			nlohmann::json r;
			r["removed"] = true;
			return r;
		}

		if (name == "key_animation_pose")
		{
			const float time = a.is_object() && a.contains("time") && a["time"].is_number()
				? (float)a["time"].get<double>() : doc->playhead;
			int keyed = 0;
			if (a.value("allBones", false)) keyed = AnimationEditor::KeyWholeSkeleton(*doc, time);
			else
			{
				// Keep the pending pose across the move - it is the thing
				// being keyed.
				AnimationEditor::SetPlayhead(*doc, time, /*keepPendingPose=*/true);
				keyed = AnimationEditor::KeyPendingPose(*doc);
			}
			nlohmann::json r;
			r["keyed"] = keyed;
			r["time"] = doc->SnapTime(time);
			return r;
		}

		if (name == "animation_playback")
		{
			const std::string action = A("action");
			if (action == "play") doc->playing = true;
			else if (action == "pause") doc->playing = false;
			else if (action == "stop") { doc->playing = false; AnimationEditor::SetPlayhead(*doc, 0.f); }
			else if (action == "scrub" || action.empty())
			{
				if (a.is_object() && a.contains("time") && a["time"].is_number())
					AnimationEditor::SetPlayhead(*doc, (float)a["time"].get<double>());
			}
			else throw std::runtime_error("action must be play, pause, stop or scrub");
			if (a.is_object() && a.contains("loop") && a["loop"].is_boolean())
				doc->looping = a["loop"].get<bool>();
			if (a.is_object() && a.contains("speed") && a["speed"].is_number())
				doc->playSpeed = (float)a["speed"].get<double>();
			nlohmann::json r;
			r["playing"] = doc->playing;
			r["playhead"] = doc->playhead;
			return r;
		}

		if (name == "animation_blend")
		{
			const std::string action = A("action");
			if (action == "mode")
			{
				doc->blendMode = a.value("enabled", true);
				doc->TouchBlend();
			}
			else if (action == "add")
			{
				// Every mutating action here goes through PushBlendEdit for
				// the same reason the panel's do: an agent edit that cannot
				// be undone is worse than one made by hand, since there was
				// nobody watching it happen.
				AnimationBlendEntry e;
				e.clip = clipIndexArg("clip");
				if (a.is_object() && a.contains("weight") && a["weight"].is_number())
					e.weight = (float)a["weight"].get<double>();
				if (a.is_object() && a.contains("speed") && a["speed"].is_number())
					e.speed = (float)a["speed"].get<double>();
				e.layer = A("layer");
				doc->PushBlendEdit("Add clip to blend", [&]() {
					doc->blendEntries.push_back(e);
					doc->blendMode = true;
				});
			}
			else if (action == "set")
			{
				const int idx = a.value("index", 0);
				if (idx < 0 || idx >= (int)doc->blendEntries.size())
					throw std::runtime_error("blend entry index out of range");
				doc->PushBlendEdit("Edit blend entry", [&]() {
					if (a.contains("weight") && a["weight"].is_number())
						doc->blendEntries[idx].weight = (float)a["weight"].get<double>();
					if (a.contains("speed") && a["speed"].is_number())
						doc->blendEntries[idx].speed = (float)a["speed"].get<double>();
					if (a.contains("layer")) doc->blendEntries[idx].layer = A("layer");
				});
			}
			else if (action == "remove")
			{
				const int idx = a.value("index", 0);
				if (idx < 0 || idx >= (int)doc->blendEntries.size())
					throw std::runtime_error("blend entry index out of range");
				doc->PushBlendEdit("Remove clip from blend", [&]() {
					doc->blendEntries.erase(doc->blendEntries.begin() + idx);
				});
			}
			else if (action == "clear")
			{
				doc->PushBlendEdit("Clear blend", [&]() {
					doc->blendEntries.clear();
					doc->blendLayers.clear();
				});
			}
			else if (action == "layer")
			{
				const std::string layerName = A("layer");
				if (layerName.empty()) throw std::runtime_error("layer name required");
				const std::string boneName = A("bone");
				// Resolved before the edit so a bad bone name throws without
				// having already created the layer as a side effect.
				int boneId = -1;
				if (!boneName.empty())
				{
					if (!pv || !pv->instance) throw std::runtime_error("no rig bound");
					boneId = pv->FindBone(boneName);
					if (boneId < 0) throw std::runtime_error("unknown bone '" + boneName + "'");
				}
				const bool withChildren = a.value("children", false);
				doc->PushBlendEdit("Edit layer '" + layerName + "'", [&]() {
					AnimationBlendLayer& layer = doc->EnsureBlendLayer(layerName);
					if (boneId < 0) return;
					const std::vector<Bone>& bones = pv->instance->GetSkeletonBones();
					for (size_t b = 0; b < bones.size(); b++)
					{
						bool take = ((int)bones[b].self == boneId);
						if (!take && withChildren)
						{
							int p = bones[b].parent, guard = 0;
							while (p >= 0 && p < (int)bones.size() && guard++ < (int)bones.size())
							{
								if (p == boneId) { take = true; break; }
								p = bones[p].parent;
							}
						}
						if (!take) continue;
						if (std::find(layer.bones.begin(), layer.bones.end(), bones[b].name) == layer.bones.end())
							layer.bones.push_back(bones[b].name);
					}
				});
				doc->rigDirty = true;
			}
			else if (action == "lua")
			{
				nlohmann::json r;
				r["lua"] = doc->BuildBlendLuaSnippet(doc->displayName + ".p3da");
				return r;
			}
			else if (action == "tick")
			{
				// Advance the blend clock deterministically, for tests and for
				// agents that want a specific moment rather than whatever the
				// UI happens to have reached.
				if (a.contains("time") && a["time"].is_number())
					doc->blendClock = (float)a["time"].get<double>();
				doc->blendPlaying = a.value("playing", doc->blendPlaying);
			}
			else if (!action.empty() && action != "state")
				throw std::runtime_error("action must be mode, add, set, remove, clear, layer, lua, tick or state");

			StoreAnimationBlend(*doc);

			nlohmann::json r;
			r["blendMode"] = doc->blendMode;
			r["clock"] = doc->blendClock;
			r["playing"] = doc->blendPlaying;
			nlohmann::json entries = nlohmann::json::array();
			for (size_t i = 0; i < doc->blendEntries.size(); i++)
			{
				const AnimationBlendEntry& e = doc->blendEntries[i];
				nlohmann::json j;
				j["index"] = (int)i;
				j["clip"] = e.clip;
				j["clipName"] = (e.clip >= 0 && e.clip < (int)doc->clips.size())
					? doc->clips[e.clip].AnimationName : std::string();
				j["weight"] = e.weight;
				j["speed"] = e.speed;
				j["layer"] = e.layer;
				j["playOrder"] = e.playOrder;
				entries.push_back(j);
			}
			r["entries"] = std::move(entries);
			nlohmann::json layers = nlohmann::json::array();
			for (size_t i = 0; i < doc->blendLayers.size(); i++)
			{
				nlohmann::json j;
				j["name"] = doc->blendLayers[i].name;
				j["bones"] = doc->blendLayers[i].bones;
				layers.push_back(j);
			}
			r["layers"] = std::move(layers);
			return r;
		}

		if (name == "select_animation_bone")
		{
			if (!pv || !pv->instance) throw std::runtime_error("no rig bound - pass \"mesh\"");
			const std::string boneName = A("bone");
			const int boneId = pv->FindBone(boneName);
			if (boneId < 0) throw std::runtime_error("unknown bone '" + boneName + "'");
			doc->selectedBone = boneId;
			doc->selectedBoneName = boneName;
			nlohmann::json r;
			r["bone"] = boneName;
			r["id"] = boneId;
			// Model-space position of this bone and of every descendant, so a
			// caller (and the editor's own tests) can confirm that posing a
			// parent actually carries its children.
			nlohmann::json chain = nlohmann::json::array();
			const std::vector<Bone>& sb = pv->instance->GetSkeletonBones();
			for (size_t i = 0; i < sb.size(); i++)
			{
				int p = sb[i].parent, guard = 0;
				bool desc = ((int)sb[i].self == boneId);
				while (!desc && p >= 0 && p < (int)sb.size() && guard++ < (int)sb.size())
				{
					if (p == boneId) desc = true;
					p = sb[p].parent;
				}
				if (!desc) continue;
				const Vec3 wp = pv->instance->GetBoneGlobalTransform(sb[i].self).GetTranslation();
				nlohmann::json e;
				e["bone"] = sb[i].name;
				e["pos"] = { wp.x, wp.y, wp.z };
				chain.push_back(e);
			}
			r["chain"] = std::move(chain);
			return r;
		}

		if (name == "undo_animation" || name == "redo_animation")
		{
			if (name == "undo_animation") doc->undo.Undo(); else doc->undo.Redo();
			nlohmann::json r;
			r["canUndo"] = doc->undo.CanUndo();
			r["canRedo"] = doc->undo.CanRedo();
			r["description"] = doc->undo.UndoDescription();
			return r;
		}

		if (name == "animation_ik")
		{
			if (!pv || !pv->instance) throw std::runtime_error("no rig bound - pass \"mesh\" to open_animation");
			const std::string action = A("action").empty() ? std::string("list") : A("action");

			// Chains are addressed by name, matching the rig sidecar. Index
			// would be the obvious alternative and is exactly the mistake
			// clip ids used to make.
			const std::string chainName = A("name");
			int idx = -1;
			for (size_t i = 0; i < doc->ikHandles.size(); i++)
				if (doc->ikHandles[i].name == chainName) { idx = (int)i; break; }

			auto describe = [&]() {
				nlohmann::json r;
				nlohmann::json arr = nlohmann::json::array();
				for (size_t i = 0; i < doc->ikHandles.size(); i++)
				{
					const AnimationEditorDocument::IKHandle& h = doc->ikHandles[i];
					nlohmann::json c;
					c["name"] = h.name;
					c["root"] = h.rootBone;
					c["effector"] = h.effectorBone;
					c["target"] = { h.target.x, h.target.y, h.target.z };
					c["usePole"] = h.usePole;
					if (h.usePole) c["pole"] = { h.pole.x, h.pole.y, h.pole.z };
					// Resolution is the one thing that can silently be wrong,
					// so it is always reported rather than left to be guessed.
					const int rootId = pv->BoneIdByName(h.rootBone);
					const int effId = pv->BoneIdByName(h.effectorBone);
					if (rootId < 0 || effId < 0) c["resolved"] = false;
					else
					{
						const std::vector<int32> chain = IKSolver::BuildChain(pv->instance, rootId, effId);
						c["resolved"] = !chain.empty();
						c["boneCount"] = (int)chain.size();
						c["solver"] = chain.size() == 3 ? "two-bone (exact)" : "FABRIK";
						nlohmann::json names = nlohmann::json::array();
						for (size_t k = 0; k < chain.size(); k++) names.push_back(pv->BoneName((int)chain[k]));
						c["bones"] = names;
					}
					arr.push_back(c);
				}
				r["chains"] = arr;
				r["rigPath"] = doc->rigPath;
				r["rigDirty"] = doc->rigDirty;
				r["canUndo"] = doc->undo.CanUndo();
				r["canRedo"] = doc->undo.CanRedo();
				return r;
			};

			if (action == "list") return describe();

			// Mirrors animation_blend's "mode": the IK target handle is only
			// live in IK mode, so anything driving the editor from outside
			// needs a way in without clicking the radio button.
			if (action == "mode")
			{
				doc->ikMode = a.value("enabled", true);
				if (doc->ikMode) doc->blendMode = false;
				nlohmann::json r = describe();
				r["ikMode"] = doc->ikMode;
				return r;
			}

			if (action == "add")
			{
				if (chainName.empty()) throw std::runtime_error("\"name\" is required");
				if (idx >= 0) throw std::runtime_error("a chain named '" + chainName + "' already exists");
				AnimationEditorDocument::IKHandle h;
				h.name = chainName;
				h.rootBone = A("root");
				h.effectorBone = A("effector");
				if (a.contains("target") && a["target"].is_array() && a["target"].size() == 3)
				{
					h.target = Vec3(a["target"][0].get<f32>(), a["target"][1].get<f32>(), a["target"][2].get<f32>());
					h.targetSet = true;
				}
				else if (!h.effectorBone.empty())
				{
					const int e = pv->BoneIdByName(h.effectorBone);
					if (e >= 0)
					{
						h.target = pv->instance->GetBoneGlobalTransform(e).GetTranslation();
						h.targetSet = true;
					}
				}
				if (a.contains("pole") && a["pole"].is_array() && a["pole"].size() == 3)
				{
					h.pole = Vec3(a["pole"][0].get<f32>(), a["pole"][1].get<f32>(), a["pole"][2].get<f32>());
					h.usePole = true;
				}
				doc->PushSnapshotEdit("Add IK chain '" + chainName + "'", [&]() {
					doc->ikHandles.push_back(h);
					doc->activeIK = (int)doc->ikHandles.size() - 1;
				});
				return describe();
			}

			if (idx < 0) throw std::runtime_error("no IK chain named '" + chainName + "'");

			if (action == "remove")
			{
				doc->PushSnapshotEdit("Remove IK chain '" + chainName + "'", [&]() {
					doc->ikHandles.erase(doc->ikHandles.begin() + idx);
					doc->activeIK = doc->ikHandles.empty() ? -1 : 0;
				});
				return describe();
			}

			if (action == "set")
			{
				doc->PushSnapshotEdit("Edit IK chain '" + chainName + "'", [&]() {
					AnimationEditorDocument::IKHandle& h = doc->ikHandles[idx];
					if (!A("root").empty()) h.rootBone = A("root");
					if (!A("effector").empty()) h.effectorBone = A("effector");
					if (a.contains("target") && a["target"].is_array() && a["target"].size() == 3)
					{
						h.target = Vec3(a["target"][0].get<f32>(), a["target"][1].get<f32>(), a["target"][2].get<f32>());
						h.targetSet = true;
					}
					if (a.contains("pole") && a["pole"].is_array() && a["pole"].size() == 3)
					{
						h.pole = Vec3(a["pole"][0].get<f32>(), a["pole"][1].get<f32>(), a["pole"][2].get<f32>());
						h.usePole = true;
					}
					if (a.contains("usePole")) h.usePole = a["usePole"].get<bool>();
				});
				doc->activeIK = idx;
				return describe();
			}

			if (action == "solve" || action == "key" || action == "bake")
			{
				doc->activeIK = idx;
				nlohmann::json r = describe();
				const AnimationEditorDocument::IKHandle& h = doc->ikHandles[idx];

				if (action == "bake")
				{
					const f32 from = a.value("from", 0.f);
					const f32 to = a.value("to", doc->HasActiveClip() ? doc->clips[doc->activeClip].Duration : 0.f);
					r["keyed"] = AnimationEditor::BakeIKOverRange(*doc, h, from, to);
					r["from"] = from;
					r["to"] = to;
				}
				else
				{
					if (const Animation* clip = doc->ActiveClip())
						pv->instance->ApplyAnimationAtTime(*clip, doc->playhead);
					if (!AnimationEditor::ApplyIK(*doc, h))
						throw std::runtime_error("chain '" + chainName + "' could not be solved - check root/effector");
					if (action == "key")
						doc->PushSnapshotEdit("Key IK '" + chainName + "'", [&]() {
							r["keyed"] = AnimationEditor::KeyIKChain(*doc, h, doc->SnapTime(doc->playhead));
						});
					// Where the effector actually landed, so a caller can
					// check the solve rather than trust it.
					const int e = pv->BoneIdByName(h.effectorBone);
					if (e >= 0)
					{
						const Vec3 got = pv->instance->GetBoneGlobalTransform(e).GetTranslation();
						r["effectorPosition"] = { got.x, got.y, got.z };
						r["targetError"] = (got - h.target).magnitude();
					}
				}
				r["canUndo"] = doc->undo.CanUndo();
				return r;
			}

			throw std::runtime_error("unknown animation_ik action '" + action + "' (list/mode/add/remove/set/solve/key/bake)");
		}

		if (name == "animation_joint_limit")
		{
			if (!pv || !pv->instance) throw std::runtime_error("no rig bound - pass \"mesh\" to open_animation");

			// Degrees at this boundary, like the rig file and the bone
			// inspector - radians are an engine-internal detail.
			const std::string bone = A("bone");
			if (!bone.empty() && (a.contains("minDeg") || a.contains("maxDeg") || a.contains("enabled")))
			{
				doc->PushSnapshotEdit("Set joint limit on '" + bone + "'", [&]() {
					JointLimit& lim = doc->rig.JointLimits[bone];
					lim.Enabled = a.value("enabled", true);
					if (a.contains("minDeg") && a["minDeg"].is_array() && a["minDeg"].size() == 3)
						lim.Min = Vec3((f32)DEGTORAD(a["minDeg"][0].get<f32>()),
							(f32)DEGTORAD(a["minDeg"][1].get<f32>()),
							(f32)DEGTORAD(a["minDeg"][2].get<f32>()));
					if (a.contains("maxDeg") && a["maxDeg"].is_array() && a["maxDeg"].size() == 3)
						lim.Max = Vec3((f32)DEGTORAD(a["maxDeg"][0].get<f32>()),
							(f32)DEGTORAD(a["maxDeg"][1].get<f32>()),
							(f32)DEGTORAD(a["maxDeg"][2].get<f32>()));
				});
				doc->rigDirty = true;
			}

			if (a.value("save", false) && !doc->SaveRig())
				throw std::runtime_error("could not write " + doc->rigPath);

			nlohmann::json r;
			nlohmann::json arr = nlohmann::json::array();
			for (std::map<std::string, JointLimit>::const_iterator it = doc->rig.JointLimits.begin();
				it != doc->rig.JointLimits.end(); ++it)
			{
				nlohmann::json l;
				l["bone"] = it->first;
				l["enabled"] = it->second.Enabled;
				l["minDeg"] = { (f32)RADTODEG(it->second.Min.x), (f32)RADTODEG(it->second.Min.y), (f32)RADTODEG(it->second.Min.z) };
				l["maxDeg"] = { (f32)RADTODEG(it->second.Max.x), (f32)RADTODEG(it->second.Max.y), (f32)RADTODEG(it->second.Max.z) };
				arr.push_back(l);
			}
			r["jointLimits"] = arr;
			r["rigPath"] = doc->rigPath;
			r["rigDirty"] = doc->rigDirty;
			r["canUndo"] = doc->undo.CanUndo();
			return r;
		}

		throw std::runtime_error("unknown animation command: " + name);
	}

	if (name == "read_script" || name == "write_script" || name == "create_script")
	{
		if (!project.IsOpen())
			throw std::runtime_error("no project open");

		if (name == "create_script")
		{
			const std::string scriptName = A("name");
			if (scriptName.empty())
				throw std::runtime_error("create_script requires 'name'");
			const std::string kindStr = a.is_object() ? a.value("kind", std::string("gameobject")) : std::string("gameobject");
			const LuaScriptKind kind = (kindStr == "scene") ? LuaScriptKind::Scene : LuaScriptKind::GameObject;
			std::string abs, cerr;
			if (!project.CreateLuaScript(scriptName, abs, &cerr, kind))
				throw std::runtime_error(cerr);
			nlohmann::json r;
			r["ok"] = true;
			r["path"] = project.RelativePath(abs);
			return r;
		}

		const std::string rel = A("path");
		if (rel.empty())
			throw std::runtime_error(name + " requires 'path' (project-relative, e.g. assets/lua/player.lua)");
		// Project-relative only: a script tool that accepts absolute paths is
		// a tool that can read and overwrite anything on the machine.
		const std::string abs = project.AbsolutePath(rel);
		if (project.RelativePath(abs).empty())
			throw std::runtime_error("path must stay inside the project: " + rel);

		if (name == "read_script")
		{
			std::ifstream in(abs);
			if (!in)
				throw std::runtime_error("could not read " + rel);
			std::ostringstream ss;
			ss << in.rdbuf();
			nlohmann::json r;
			r["path"] = rel;
			r["text"] = ss.str();
			return r;
		}

		if (!a.is_object() || !a.contains("text") || !a["text"].is_string())
			throw std::runtime_error("write_script requires 'text'");
		std::ofstream out(abs, std::ios::binary | std::ios::trunc);
		if (!out)
			throw std::runtime_error("could not write " + rel);
		out << a["text"].get<std::string>();
		out.close();
		// An open document would otherwise keep showing (and later re-save)
		// the pre-write text over the top of what was just written.
		ReloadScriptDocumentFromDisk(abs);
		nlohmann::json r;
		r["ok"] = true;
		r["path"] = rel;
		return r;
	}

	if (name == "open_project")
	{
		const std::string path = A("path");
		if (path.empty())
			throw std::runtime_error("open_project requires 'path'");
		projectDialogError.clear();
		if (!OpenProjectFromPath(path))
			throw std::runtime_error(projectDialogError.empty()
				? ("failed to open project: " + path) : projectDialogError);
		nlohmann::json r;
		r["ok"] = true;
		r["projectPath"] = project.GetProjectPath(); // the anchor - see "status"
		if (sceneView)
		{
			r["scenePath"] = project.DisplayPath(sceneView->GetScenePath());
			r["scene"] = sceneView->GetSceneDisplayName();
		}
		return r;
	}

	if (name == "create_material")
	{
		if (!project.IsOpen())
			throw std::runtime_error("no project open");
		const std::string matName = A("name");
		const std::string kindStr = (a.is_object() ? a.value("kind", std::string("generic")) : std::string("generic"));
		const MaterialAssetKind kind = (kindStr == "custom") ? MaterialAssetKind::Custom : MaterialAssetKind::Generic;
		std::string abs, cerr;
		if (!project.CreateMaterial(matName, kind, abs, &cerr))
			throw std::runtime_error(cerr);
		const std::string createdRel = project.RelativePath(abs);
		if (sceneView && !createdRel.empty())
			sceneView->PushUndoCommand(std::make_unique<CreateAssetCommand>(&project, createdRel, "Create Material '" + createdRel + "'"));
		project.Save();
		if (!OpenMaterialDocument(abs))
			throw std::runtime_error("material created but could not be opened: " + abs);
		nlohmann::json r;
		r["ok"] = true;
		r["path"] = createdRel;
		r["kind"] = kindStr;
		return r;
	}
	if (name == "delete_asset")
	{
		if (!project.IsOpen())
			throw std::runtime_error("no project open");
		const std::string rel = A("path");
		std::string derr, trashRel, movedFromRel;
		if (!project.DeleteAsset(rel, &derr, &trashRel, &movedFromRel))
			throw std::runtime_error(derr);
		if (sceneView && !trashRel.empty())
			sceneView->PushUndoCommand(std::make_unique<DeleteAssetCommand>(&project, movedFromRel, trashRel));
		project.Save();
		nlohmann::json r;
		r["ok"] = true;
		r["trashed"] = trashRel;
		return r;
	}
	if (name == "get_material_graph")
	{
		std::string aerr;
		MaterialEditorDocument* doc = AgentOpenMaterial(A("path"), aerr);
		if (!doc) throw std::runtime_error(aerr);
		return doc->AgentGetGraph();
	}
	if (name == "set_material_graph")
	{
		std::string aerr;
		MaterialEditorDocument* doc = AgentOpenMaterial(A("path"), aerr);
		if (!doc) throw std::runtime_error(aerr);
		if (doc->editKind != MaterialEditKind::Custom)
			throw std::runtime_error("material is Generic kind - node graphs only apply to Custom Shader materials");
		const MaterialEditorDocument::GraphSnapshot graphBefore = doc->CaptureGraphSnapshot();
		std::string gerr;
		if (!doc->AgentSetGraph(a.contains("nodes") ? a["nodes"] : nlohmann::json::array(),
			a.contains("connections") ? a["connections"] : nlohmann::json::array(), gerr))
			throw std::runtime_error(gerr);
		// Unifies this with the UI's own per-widget undo (GraphUndoCommit in
		// UI/MaterialEditor.cpp) - a bulk agent-driven graph replace is one
		// undo step, same as any other single edit.
		doc->undo.Push(std::make_unique<GraphEditCommand>(doc, graphBefore, doc->CaptureGraphSnapshot(), "Set Material Graph"));
		if (!MaterialEditor::SaveToFile(*doc, doc->absolutePath, project.GetProjectPath(), UseDeferredGBuffer()))
			throw std::runtime_error("failed to save material to " + doc->absolutePath);
		nlohmann::json r;
		r["ok"] = true;
		r["path"] = project.RelativePath(doc->absolutePath);
		if (!doc->lastApplyError.empty())
			r["applyWarning"] = doc->lastApplyError; // graph saved, but the compile that SaveToFile also runs failed
		return r;
	}
	if (name == "apply_material")
	{
		std::string aerr;
		MaterialEditorDocument* doc = AgentOpenMaterial(A("path"), aerr);
		if (!doc) throw std::runtime_error(aerr);
		std::string cerr;
		if (!MaterialEditor::ApplyGraphOrTextToLiveMaterial(*doc, project.GetProjectPath(), UseDeferredGBuffer(), &cerr))
			throw std::runtime_error(cerr);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "get_material_text")
	{
		std::string aerr;
		MaterialEditorDocument* doc = AgentOpenMaterial(A("path"), aerr);
		if (!doc) throw std::runtime_error(aerr);
		return doc->AgentGetText();
	}
	if (name == "set_material_text")
	{
		std::string aerr;
		MaterialEditorDocument* doc = AgentOpenMaterial(A("path"), aerr);
		if (!doc) throw std::runtime_error(aerr);
		if (!a.is_object() || !a.contains("text"))
			throw std::runtime_error("'text' (the GLSL snippet) is required");
		std::string terr;
		if (!doc->AgentSetText(a.value("text", std::string()),
			a.contains("textures") ? a["textures"] : nlohmann::json(), terr))
			throw std::runtime_error(terr);
		if (!MaterialEditor::SaveToFile(*doc, doc->absolutePath, project.GetProjectPath(), UseDeferredGBuffer()))
			throw std::runtime_error("failed to save material to " + doc->absolutePath);
		nlohmann::json r;
		r["ok"] = true;
		r["path"] = project.RelativePath(doc->absolutePath);
		if (!doc->lastApplyError.empty())
			r["applyWarning"] = doc->lastApplyError; // saved, but the compile failed
		return r;
	}

	if (!sceneView)
		throw std::runtime_error("no scene is open in the editor");

	std::string err;
	if (name == "scene_state")
		return sceneView->AgentSceneState();

	if (name == "get_object")
	{
		nlohmann::json obj;
		if (!sceneView->AgentGetObject(A("name"), obj, err))
			throw std::runtime_error(err);
		return obj;
	}

	if (name == "select_object")
	{
		if (!sceneView->AgentSelectObject(A("name"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}

	if (name == "add_object")
	{
		if (!sceneView->AgentAddObject(A("name"), A("parent"), AV("position"), AV("rotation"), AV("scale"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "add_primitive")
	{
		if (!sceneView->AgentAddPrimitive(A("name"), A("shape"), a, A("parent"), a.contains("color") ? a["color"] : nlohmann::json(), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "add_light")
	{
		if (!sceneView->AgentAddLight(A("name"), A("type"), a, A("parent"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "add_audio")
	{
		if (!sceneView->AgentAddAudio(A("name"), A("file"), a, A("parent"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "add_particles")
	{
		if (!sceneView->AgentAddParticles(A("name"), a, A("parent"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "add_physics")
	{
		if (!sceneView->AgentAddPhysics(A("name"), a, A("parent"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "add_model")
	{
		if (!sceneView->AgentAddModel(A("name"), A("file"), A("parent"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "add_camera")
	{
		const bool active = a.is_object() ? a.value("active", true) : true;
		const f32 fov = a.is_object() ? (f32)a.value("fov", 70.0) : 70.f;
		const f32 nearP = a.is_object() ? (f32)a.value("near", 0.1) : 0.1f;
		const f32 farP = a.is_object() ? (f32)a.value("far", 2000.0) : 2000.f;
		if (!sceneView->AgentAddCamera(A("name"), AV("position"), fov, nearP, farP, active, err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "set_transform")
	{
		if (!sceneView->AgentSetTransform(A("name"), a.contains("transform") ? a["transform"] : a, err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "set_tags")
	{
		if (!sceneView->AgentSetTags(A("name"), a.contains("add") ? a["add"] : nlohmann::json::array(), a.contains("remove") ? a["remove"] : nlohmann::json::array(), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "rename")
	{
		if (!sceneView->AgentRename(A("name"), A("newName"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "reparent")
	{
		if (!sceneView->AgentReparent(A("name"), A("newParent"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "duplicate")
	{
		if (!sceneView->AgentDuplicate(A("name"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "delete_object")
	{
		if (!sceneView->AgentDeleteObject(A("name"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "undo")
	{
		sceneView->Undo();
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "redo")
	{
		sceneView->Redo();
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "undo_material" || name == "redo_material")
	{
		std::string aerr;
		MaterialEditorDocument* doc = AgentOpenMaterial(A("path"), aerr);
		if (!doc) throw std::runtime_error(aerr);
		if (name == "undo_material") doc->undo.Undo(); else doc->undo.Redo();
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "attach_script")
	{
		if (!sceneView->AgentAttachScript(A("name"), A("scriptFile"), a.contains("data") ? a["data"] : nlohmann::json::object(), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "detach_component")
	{
		if (!sceneView->AgentDetachComponent(A("name"), A("componentType"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "set_material")
	{
		if (!sceneView->AgentSetMaterial(A("object"), a.contains("material") ? a["material"] : a, err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "assign_material")
	{
		const int submesh = (a.is_object() ? a.value("submesh", 0) : 0);
		if (!AssignMaterialAsset(A("object"), submesh, A("path"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "save_scene")
	{
		if (!sceneView->AgentSave(err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		r["path"] = project.DisplayPath(sceneView->GetScenePath());
		return r;
	}
	if (name == "save_scene_as")
	{
		if (!sceneView->AgentSaveAs(AgentScenePathArg(A("path")), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		r["path"] = project.DisplayPath(sceneView->GetScenePath());
		return r;
	}
	if (name == "load_scene")
	{
		if (!sceneView->AgentLoadScene(AgentScenePathArg(A("path")), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		r["path"] = project.DisplayPath(sceneView->GetScenePath());
		return r;
	}
	if (name == "create_prefab")
	{
		std::string rel;
		if (!sceneView->AgentCreatePrefab(A("name"), a.value("prefabName", std::string()), rel, err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		r["path"] = rel;
		return r;
	}
	if (name == "instantiate_prefab")
	{
		Vec3 pos(0.f, 0.f, 0.f);
		if (a.contains("position") && a["position"].is_array() && a["position"].size() >= 3)
			pos = Vec3(a["position"][0].get<f32>(), a["position"][1].get<f32>(), a["position"][2].get<f32>());
		std::string created;
		if (!sceneView->AgentInstantiatePrefab(A("path"), pos, created, err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		r["name"] = created;
		return r;
	}
	if (name == "apply_prefab")
	{
		if (!sceneView->AgentApplyPrefab(A("name"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "revert_prefab")
	{
		if (!sceneView->AgentRevertPrefab(A("name"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "unpack_prefab")
	{
		if (!sceneView->AgentUnpackPrefab(A("name"), err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["ok"] = true;
		return r;
	}
	if (name == "prefab_state")
	{
		nlohmann::json r;
		r["ok"] = true;
		r["instances"] = sceneView->AgentPrefabState();
		return r;
	}
	if (name == "build_game")
	{
		if (!project.IsOpen()) throw std::runtime_error("no project open");
		// Saved first, deliberately: the build copies files off disk, so an
		// unsaved edit would silently not be in what ships.
		if (!sceneView->GetScenePath().empty())
			sceneView->SaveSceneToFile(sceneView->GetScenePath());

		ProjectManager::BuildOptions opts;
		opts.outputDir = A("outputDir");
		opts.startupSceneRel = a.value("startupScene", std::string());
		opts.title = a.value("title", std::string());
		opts.width = a.value("width", 1280);
		opts.height = a.value("height", 720);
		opts.fullscreen = a.value("fullscreen", false);
		opts.deferred = (project.GetSettings().rendererType == ProjectRendererType::Deferred);

		ProjectManager::BuildResult br = project.BuildGame(opts);
		if (!br.ok) throw std::runtime_error(br.error);
		nlohmann::json r;
		r["ok"] = true;
		r["outputDir"] = br.outputDir;
		r["files"] = br.filesCopied;
		r["warnings"] = br.warnings;
		return r;
	}
	if (name == "set_renderer")
	{
		const std::string type = A("type");
		if (type != "forward" && type != "deferred")
			throw std::runtime_error("set_renderer requires type='forward' or 'deferred'");
		SwitchAllScenesRenderer(type == "deferred");
		nlohmann::json r;
		r["ok"] = true;
		r["rendererType"] = type;
		return r;
	}
	if (name == "play")
	{
		if (!sceneView->AgentPlay(err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["playing"] = true;
		return r;
	}
	if (name == "stop_play")
	{
		if (!sceneView->AgentStopPlay(err))
			throw std::runtime_error(err);
		nlohmann::json r;
		r["playing"] = false;
		return r;
	}
	if (name == "screenshot")
	{
		const std::string b64 = sceneView->AgentScreenshot();
		if (b64.empty())
			throw std::runtime_error("screenshot failed (viewport not ready?)");
		nlohmann::json r;
		r["pngBase64"] = b64;
		return r;
	}
	if (name == "reload")
	{
		const bool reloaded = sceneView->AgentReloadIfChanged();
		nlohmann::json r;
		r["reloaded"] = reloaded;
		return r;
	}

	throw std::runtime_error("unknown agent command '" + name + "'");
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
		std::string trashedExisting;
		if (project.ImportAssetFile(dropped[i], out, &err, &trashedExisting))
		{
			++okCount;
			const std::string rel = project.RelativePath(out);
			echo("Imported: " + (rel.empty() ? out : rel));
			if (sceneView && !trashedExisting.empty() && !rel.empty())
			{
				// A .p3dm import trashes the whole package folder (see
				// ImportModel), not the .p3dm file itself - `out` is the
				// .p3dm path either way, so the undo command needs the
				// package folder's relative path in that case.
				const std::string importedRel = ProjectManager::IsP3dm(out)
					? std::filesystem::path(rel).parent_path().string() : rel;
				if (!importedRel.empty())
					sceneView->PushUndoCommand(std::make_unique<ImportOverwriteCommand>(&project, importedRel, trashedExisting,
						"Import (overwrite) '" + importedRel + "'"));
			}
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

	// Ctrl+Z / Ctrl+Shift+Z (and the Windows-convention Ctrl+Y) act on
	// whichever document last had focus (see FocusedDocKind) - !WantTextInput
	// keeps this from firing while typing in an InputText, where ImGui's own
	// per-widget text-undo already owns Ctrl+Z. The Project Settings modal
	// takes priority over FocusedDocKind whenever it's open: it isn't a
	// document tab, and its Renderer combo (unlike Cancel) applies live, so
	// this is the only way to revert an accidental change while the modal
	// is still up.
	if (ImGui::GetIO().KeyCtrl && !ImGui::GetIO().WantTextInput && ImGui::IsPopupOpen("Project Settings"))
	{
		const bool shift = ImGui::GetIO().KeyShift;
		if (ImGui::IsKeyPressed(ImGuiKey_Z))
		{
			if (shift) projectUndo.Redo(); else projectUndo.Undo();
		}
		else if (!shift && ImGui::IsKeyPressed(ImGuiKey_Y))
			projectUndo.Redo();
	}
	else if (ImGui::GetIO().KeyCtrl && !ImGui::GetIO().WantTextInput)
	{
		const bool shift = ImGui::GetIO().KeyShift;
		if (lastFocusedDocKind == FocusedDocKind::Scene && sceneView)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Z))
			{
				if (shift) sceneView->Redo(); else sceneView->Undo();
			}
			else if (!shift && ImGui::IsKeyPressed(ImGuiKey_Y))
				sceneView->Redo();
		}
		else if (lastFocusedDocKind == FocusedDocKind::Material && activeMaterialDoc)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Z))
			{
				if (shift) activeMaterialDoc->undo.Redo(); else activeMaterialDoc->undo.Undo();
			}
			else if (!shift && ImGui::IsKeyPressed(ImGuiKey_Y))
				activeMaterialDoc->undo.Redo();
		}
		else if (lastFocusedDocKind == FocusedDocKind::Animation && activeAnimationDoc)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Z))
			{
				if (shift) activeAnimationDoc->undo.Redo(); else activeAnimationDoc->undo.Undo();
			}
			else if (!shift && ImGui::IsKeyPressed(ImGuiKey_Y))
				activeAnimationDoc->undo.Redo();
		}
	}

	// Menu bar. Each File action is requested here and performed once below,
	// so the accelerator and the menu item share a single code path - the
	// menu item alone cannot serve as that path, since its body only runs
	// while the menu is actually open.
	bool reqNewProject = false, reqOpenProject = false, reqSaveProject = false, reqQuit = false;
	{
		const bool ctrl = ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift
			&& !ImGui::GetIO().KeyAlt && !ImGui::GetIO().WantTextInput;
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N)) reqNewProject = true;
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) reqOpenProject = true;
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S) && project.IsOpen()) reqSaveProject = true;
	}

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Project...", "Ctrl+N"))
				reqNewProject = true;
            if (ImGui::MenuItem("Open Project...", "Ctrl+O"))
				reqOpenProject = true;
			// Recents were only reachable from the welcome splash, which is
			// gone the moment a project opens.
			if (ImGui::BeginMenu("Open Recent", !recentProjects.empty()))
			{
				int shown = 0;
				for (size_t i = 0; i < recentProjects.size(); ++i)
				{
					const std::string& p = recentProjects[i];
					std::error_code ec;
					if (!fs::exists(p, ec))
						continue;
					ImGui::PushID((int)i);
					if (ImGui::MenuItem(fs::path(p).filename().string().c_str()))
					{
						// Deferred through the same unsaved-work gate as
						// "Open Project..."; HostOpenProject picks the path
						// back up instead of raising the browse dialog.
						pendingRecentProjectPath = p;
						if (!sceneView || sceneView->ConfirmUnsavedThen(SceneEditor::UnsavedOpenProject))
						{
							pendingRecentProjectPath.clear();
							OpenProjectFromPath(p);
						}
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%s", p.c_str());
					ImGui::PopID();
					++shown;
				}
				if (shown == 0)
					ImGui::TextDisabled("No recent projects on disk");
				else
				{
					ImGui::Separator();
					if (ImGui::MenuItem("Clear Recent"))
					{
						recentProjects.clear();
						SaveRecentProjects();
					}
				}
				ImGui::EndMenu();
			}

			// Scene New/Open/Save used to live in a "Scene" menu of their
			// own, one menu away from the project's - so half the editor's
			// file operations were somewhere other than File.
			if (project.IsOpen() && sceneView)
			{
				ImGui::Separator();
				sceneView->ShowFileMenuItems();
			}

			ImGui::Separator();
            if (ImGui::MenuItem("Save Project", "Ctrl+S", false, project.IsOpen()))
				reqSaveProject = true;
			ImGui::Separator();
			if (ImGui::MenuItem("Close Project", NULL, false, project.IsOpen()))
				CloseProject();
			if (ImGui::MenuItem("Quit"))
				reqQuit = true;
            ImGui::EndMenu();
        }

		if (ImGui::BeginMenu("Edit"))
		{
			// Scoped to whichever document last had focus (see
			// FocusedDocKind) - greyed out otherwise, same convention as
			// every other conditionally-available item in this menu bar
			// (e.g. "Save Project" above).
			bool canUndo = false, canRedo = false;
			std::string undoDesc, redoDesc;
			if (lastFocusedDocKind == FocusedDocKind::Scene && sceneView)
			{
				canUndo = sceneView->CanUndo(); canRedo = sceneView->CanRedo();
				undoDesc = sceneView->UndoDescription(); redoDesc = sceneView->RedoDescription();
			}
			else if (lastFocusedDocKind == FocusedDocKind::Material && activeMaterialDoc)
			{
				canUndo = activeMaterialDoc->undo.CanUndo(); canRedo = activeMaterialDoc->undo.CanRedo();
				undoDesc = activeMaterialDoc->undo.UndoDescription(); redoDesc = activeMaterialDoc->undo.RedoDescription();
			}
			else if (lastFocusedDocKind == FocusedDocKind::Animation && activeAnimationDoc)
			{
				canUndo = activeAnimationDoc->undo.CanUndo(); canRedo = activeAnimationDoc->undo.CanRedo();
				undoDesc = activeAnimationDoc->undo.UndoDescription(); redoDesc = activeAnimationDoc->undo.RedoDescription();
			}
			const std::string undoLabel = canUndo ? ("Undo " + undoDesc) : "Undo";
			const std::string redoLabel = canRedo ? ("Redo " + redoDesc) : "Redo";
			if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, canUndo))
			{
				if (lastFocusedDocKind == FocusedDocKind::Scene && sceneView) sceneView->Undo();
				else if (lastFocusedDocKind == FocusedDocKind::Animation && activeAnimationDoc) activeAnimationDoc->undo.Undo();
				else if (activeMaterialDoc) activeMaterialDoc->undo.Undo();
			}
			if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Shift+Z", false, canRedo))
			{
				if (lastFocusedDocKind == FocusedDocKind::Scene && sceneView) sceneView->Redo();
				else if (lastFocusedDocKind == FocusedDocKind::Animation && activeAnimationDoc) activeAnimationDoc->undo.Redo();
				else if (activeMaterialDoc) activeMaterialDoc->undo.Redo();
			}

			// Same document scoping as Undo/Redo: only the scene has a
			// selection to act on. Both already had keyboard shortcuts that
			// no menu ever advertised.
			ImGui::Separator();
			const bool sceneSel = lastFocusedDocKind == FocusedDocKind::Scene
				&& sceneView && !sceneView->IsPlaying() && sceneView->HasSelection();
			if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, sceneSel))
				sceneView->DuplicateSelection();
			if (ImGui::MenuItem("Delete", "Del", false, sceneSel))
				sceneView->DeleteSelection();

			// Project-wide preferences, next to the other things that edit
			// state rather than open/save it.
			ImGui::Separator();
			if (ImGui::MenuItem("Project Settings...", NULL, false, project.IsOpen()))
			{
				openProjectSettingsModal = true;
				projectSettingsName = project.GetProjectName();
				projectDialogError.clear();
			}
			ImGui::EndMenu();
		}

		// Everything that creates or imports a project asset. New Animation /
		// Import Animation used to sit in File between the project entries,
		// and New Script / New Material were reachable only by right-clicking
		// inside the Assets panel.
		if (project.IsOpen() && ImGui::BeginMenu("Assets"))
		{
			ShowAssetCreateMenuItems();
			ImGui::EndMenu();
		}

        if (project.IsOpen() && sceneView)
			sceneView->ShowMenubarOptions();

        if (ImGui::BeginMenu("View", ""))
        {
            if (ImGui::BeginMenu("Windows", "")) {
				if (ImGui::MenuItem("Scene Tree", "", &showingSceneTree)) {}
				if (ImGui::MenuItem("Scene View", "", &showingSceneView)) {}
                if (ImGui::MenuItem("Properties", "", &showingTabProperties)) {}
                if (ImGui::MenuItem("Tools", "", &showingTabTools)) {}
                if (ImGui::MenuItem("Assets", "", &showingAssets)) {}
                if (ImGui::MenuItem("Log", "", &showingLog)) {}
                if (ImGui::MenuItem("AI Assistant", "", &showingTabAI)) {}
                ImGui::EndMenu();
            }
			// Viewport overlays: their own group. They used to trail off the
			// end of the menu directly under "Default Layout", which reads
			// like part of the layout controls.
			if (project.IsOpen() && sceneView)
			{
				ImGui::Separator();
				sceneView->ShowViewOptions();
			}
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout", "")) { LoadDefaultLayout(); }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

	if (reqNewProject && sceneView->ConfirmUnsavedThen(SceneEditor::UnsavedNewProject))
	{
		openNewProjectModal = true;
		projectDialogError.clear();
	}
	if (reqOpenProject)
	{
		pendingRecentProjectPath.clear();
		if (sceneView->ConfirmUnsavedThen(SceneEditor::UnsavedOpenProject))
		{
			openOpenProjectModal = true;
			projectDialogError.clear();
		}
	}
	if (reqSaveProject && project.IsOpen())
	{
		if (sceneView->IsSceneDirty())
			sceneView->TrySaveCurrentScene();
		std::string err;
		if (!project.Save(&err))
			echo("ERROR: " + err);
	}
	if (reqQuit && EditorAllowWindowClose())
		Close();

	// No project: skip the dock host entirely — empty dock nodes / the
	// fullscreen "Main" window were painting over the welcome splash.
	if (!project.IsOpen())
	{
		DrawProjectDialogs();
		DrawWelcomeScreen();
		ImGui::EndFrame();
		ImGui::Render();
#if !defined(_SDL2VULKAN) && !defined(_SDL2METAL)
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(0.16f, 0.14f, 0.15f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
		return;
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

	if (sceneView)
	{
		sceneView->DrawSceneFileDialog();
		sceneView->DrawUnsavedChangesModal();
	}

	// Run BEFORE the main viewport pass, same reasoning as ShowViewport()'s
	// own RenderCameraPreview call ("run it before the viewport pass so
	// PreRender below restores that state"): the Material Editor's live
	// preview is a second offscreen render sharing the same GlobalMatrices
	// UBO (and, when the project's renderer is Deferred, the same
	// composite-always-targets-framebuffer-0 quirk - see
	// DeferredRenderer.h's GetColorTexture() comment) as the main viewport.
	// Rendering it first means DrawSceneViewWindow()'s own
	// ResetViewPort()/SetViewPort()/PreRender() sequence unconditionally
	// restores the correct camera/viewport state afterward, instead of the
	// preview's leftover state bleeding into the main viewport, its gizmo,
	// or its grid for the rest of this frame.
	DrawMaterialEditorWindows();
	DrawAnimationEditorWindows();

	if (showingSceneTree)
		DrawSceneTreeWindow();

	if (showingSceneView)
		DrawSceneViewWindow();
	else if (sceneView)
		sceneView->NotifyViewportNotDrawn(); // panel closed entirely - same reasoning

	DrawScriptEditorWindows();

	if (showingLog)
		tabLog->Show();

	if (showingTabTools)
		tabTools->Show();

	if (showingTabProperties)
		tabProperties->Show();

	if (showingTabAI)
		tabAI->Show();

	if (showingAssets)
		DrawAssetsWindow();

	// After every document window, so the ids parked by their close buttons
	// this frame are resolved while those documents are still alive.
	DrawUnsavedDocumentModal();
	DrawSaveAnimationAsModal();
	DrawAnimationAssetModals();

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
	CloseAllMaterialDocuments();
	CloseAllAnimationDocuments();
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
	tabAI->SetProjectOpen(true);
	tabAI->LoadFrom(project.GetSettings().aiAssistant);
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
	CloseAllMaterialDocuments();
	CloseAllAnimationDocuments();
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
	tabAI->SetProjectOpen(true);
	tabAI->LoadFrom(project.GetSettings().aiAssistant);
	return true;
}

void Editor::CloseProjectImmediate()
{
	if (!project.IsOpen()) return;
	if (sceneView) sceneView->StopAssetSoundPreview();
	ClearAssetPreviews();
	selectedAssetRel.clear();
	CloseAllLuaScriptDocuments();
	CloseAllMaterialDocuments();
	CloseAllAnimationDocuments();
	CloseAllSceneDocuments();
	sceneView = CreateSceneDocument();
	project.Close();
	projectUndo.Clear(); // its entries reference the project just closed
	tabAI->SetProjectOpen(false);
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

void Editor::EnsureWelcomeLogo()
{
	if (welcomeLogo) return;
	Texture* tex = new Texture();
	if (!tex->LoadTexture("assets/pyros.png", TextureType::Texture, false))
	{
		delete tex;
		return;
	}
	welcomeLogo = tex;
}

void Editor::DrawWelcomeScreen()
{
	EnsureWelcomeLogo();

	const ImGuiViewport* vp = ImGui::GetMainViewport();
	const ImVec2 p0 = vp->WorkPos;
	const ImVec2 size = vp->WorkSize;
	const ImVec2 p1(p0.x + size.x, p0.y + size.y);

	ImGui::SetNextWindowPos(p0);
	ImGui::SetNextWindowSize(size);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
	ImGui::Begin("##pyros_welcome", NULL,
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoNavFocus);

	ImDrawList* dl = ImGui::GetWindowDrawList();

	// Readable warm slate — not near-black (logo sits on it cleanly).
	dl->AddRectFilledMultiColor(p0, p1,
		IM_COL32(42, 38, 40, 255), IM_COL32(42, 38, 40, 255),
		IM_COL32(58, 36, 30, 255), IM_COL32(58, 36, 30, 255));

	const ImVec2 hero(p0.x + size.x * 0.5f, p0.y + size.y * 0.34f);
	// Stronger heat bloom so the mark pops.
	for (int i = 6; i >= 1; --i)
	{
		const float r = 48.f + (float)i * 42.f;
		const int a = 10 + (7 - i) * 10;
		dl->AddCircleFilled(hero, r, IM_COL32(230, 90, 40, a), 64);
	}

	const float logoSz = std::min(220.f, std::max(140.f, size.y * 0.28f));
	if (welcomeLogo)
	{
		void* tid = GetActiveRenderDevice().GetImGuiTextureID(
			welcomeLogo->GetBindID(), welcomeLogo->GetTextureType());
		if (tid)
		{
			const ImVec2 logoMin(hero.x - logoSz * 0.5f, hero.y - logoSz * 0.62f);
			const ImVec2 logoMax(logoMin.x + logoSz, logoMin.y + logoSz);
			// Texture is authored top-down; do not Y-flip (GL path was inverted).
			dl->AddImage((ImTextureID)tid, logoMin, logoMax);
		}
	}

	const float contentW = std::min(440.f, size.x * 0.78f);
	float y = hero.y + logoSz * 0.48f;
	ImGui::SetCursorScreenPos(ImVec2(hero.x - contentW * 0.5f, y));
	ImGui::BeginChild("##welcome_body", ImVec2(contentW, size.y - (y - p0.y) - 36.f), false,
		ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);

	{
		const float titleSize = ImGui::GetFontSize() * 2.15f;
		ImFont* font = ImGui::GetFont();
		const char* title = "PyrosBuilder";
		const ImVec2 ts = font->CalcTextSizeA(titleSize, FLT_MAX, -1.f, title);
		const float tx = (contentW - ts.x) * 0.5f;
		ImGui::SetCursorPosX(tx);
		const ImVec2 tp = ImGui::GetCursorScreenPos();
		dl->AddText(font, titleSize, tp, IM_COL32(255, 250, 245, 255), title);
		ImGui::Dummy(ImVec2(ts.x, ts.y + 2.f));
	}

	{
		const char* tag = "Build worlds. Attach scripts. Press Play.";
		const ImVec2 ts = ImGui::CalcTextSize(tag);
		ImGui::SetCursorPosX((contentW - ts.x) * 0.5f);
		ImGui::TextColored(ImVec4(0.92f, 0.78f, 0.68f, 1.f), "%s", tag);
	}

	ImGui::Dummy(ImVec2(0, 18.f));

	const float btnW = contentW;
	const float btnH = 42.f;
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.f, 10.f));

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.88f, 0.32f, 0.16f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.96f, 0.42f, 0.20f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.24f, 0.12f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
	if (ImGui::Button("New Project", ImVec2(btnW, btnH)))
	{
		openNewProjectModal = true;
		projectDialogError.clear();
	}
	ImGui::PopStyleColor(4);

	ImGui::Dummy(ImVec2(0, 8.f));

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.32f, 0.28f, 0.28f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.42f, 0.34f, 0.30f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.22f, 0.22f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.94f, 0.90f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.85f, 0.45f, 0.28f, 0.95f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.4f);
	if (ImGui::Button("Open Project", ImVec2(btnW, btnH)))
	{
		openOpenProjectModal = true;
		projectDialogError.clear();
	}
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(5);
	ImGui::PopStyleVar(2);

	if (!recentProjects.empty())
	{
		ImGui::Dummy(ImVec2(0, 22.f));
		ImGui::SetCursorPosX(4.f);
		ImGui::TextColored(ImVec4(0.90f, 0.78f, 0.68f, 1.f), "RECENT");
		ImGui::Dummy(ImVec2(0, 4.f));

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.22f, 0.19f, 0.18f, 0.92f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.55f, 0.32f, 0.22f, 0.75f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.92f, 0.88f, 1.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 8.f));
		const float recentH = std::min(200.f, ImGui::GetContentRegionAvail().y - 8.f);
		ImGui::BeginChild("##recentprojs", ImVec2(btnW, recentH), true);

		int shown = 0;
		for (size_t i = 0; i < recentProjects.size(); ++i)
		{
			const std::string& p = recentProjects[i];
			std::error_code ec;
			if (!fs::exists(p, ec))
				continue;
			const std::string name = fs::path(p).filename().string();
			ImGui::PushID((int)i);
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.36f, 0.24f, 0.18f, 0.95f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.55f, 0.30f, 0.18f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.70f, 0.34f, 0.16f, 1.f));
			if (ImGui::Selectable(("  " + name).c_str(), false, 0, ImVec2(0, 28.f)))
				OpenProjectFromPath(p);
			ImGui::PopStyleColor(3);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", p.c_str());
			ImGui::PopID();
			++shown;
		}
		if (shown == 0)
			ImGui::TextColored(ImVec4(0.75f, 0.68f, 0.62f, 1.f), "No recent projects on disk");

		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(3);
	}

	ImGui::EndChild();
	ImGui::End();
	ImGui::PopStyleVar(3);
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
		ImGui::SetNextWindowFocus();
		ImGui::OpenPopup("New Project");
		openNewProjectModal = false;
	}
	if (openOpenProjectModal)
	{
		ImGui::SetNextWindowFocus();
		ImGui::OpenPopup("Open Project");
		openOpenProjectModal = false;
	}
	if (openProjectSettingsModal)
	{
		ImGui::SetNextWindowFocus();
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

		int rendererIdx = (project.GetSettings().rendererType == ProjectRendererType::Deferred) ? 1 : 0;
		if (ImGui::Combo("Renderer", &rendererIdx, "Forward\0Deferred\0"))
		{
			// Note: unlike Project Name below, this applies to the setting
			// immediately (matching the pre-existing behavior - Cancel does
			// NOT revert it, only skips the Name/Save step) - Ctrl+Z while
			// this modal is open is the only way to revert an accidental
			// change today.
			const ProjectRendererType before = project.GetSettings().rendererType;
			const ProjectRendererType after = (rendererIdx == 1) ? ProjectRendererType::Deferred : ProjectRendererType::Forward;
			project.GetSettingsMutable().rendererType = after;
			project.MarkDirty();
			projectUndo.Push(std::make_unique<ApplyClosureCommand>(
				[this, before]() { project.GetSettingsMutable().rendererType = before; project.MarkDirty(); },
				[this, after]() { project.GetSettingsMutable().rendererType = after; project.MarkDirty(); },
				"Set Renderer Type"));
		}

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
			const std::string oldName = project.GetProjectName();
			const std::string newName = projectSettingsName;
			project.SetProjectName(projectSettingsName);
			if (oldName != newName)
				projectUndo.Push(std::make_unique<ApplyClosureCommand>(
					[this, oldName]() { project.SetProjectName(oldName); },
					[this, newName]() { project.SetProjectName(newName); },
					"Set Project Name"));
			std::string err;
			if (!project.Save(&err))
			{
				projectDialogError = err;
				echo("ERROR: " + err);
			}
			else
			{
				// Apply live instead of only writing project.json.
				SwitchAllScenesRenderer(project.GetSettings().rendererType == ProjectRendererType::Deferred);
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

void Editor::SwitchAllScenesRenderer(bool useDeferred)
{
	for (size_t i = 0; i < sceneDocs.size(); ++i)
		sceneDocs[i]->SwitchRenderer(useDeferred);
	RecompileCustomMaterialsForRenderer(useDeferred);
}

void Editor::RecompileCustomMaterialsForRenderer(bool useDeferred)
{
	if (!project.IsOpen()) return;
	const std::string projectRoot = project.GetProjectPath();

	// Open editor tabs first: their doc owns compiledShader, so their
	// material must be recompiled through ApplyGraphOrTextToLiveMaterial
	// (which re-wires uniforms/samplers and updates the doc's tracking
	// fields) rather than the orphan path's RecompileFromDisk.
	std::set<IMaterial*> handled;
	for (size_t i = 0; i < materialDocs.size(); ++i)
	{
		MaterialEditorDocument* doc = materialDocs[i];
		if (!doc || doc->editKind != MaterialEditKind::Custom || !doc->currentMaterial) continue;
		handled.insert(doc->currentMaterial.get());
		// Already on the right branch (or never compiled) - nothing to do.
		if (!doc->hasCompiledShader || doc->compiledForDeferredGBuffer == useDeferred) continue;
		std::string err;
		if (!MaterialEditor::ApplyGraphOrTextToLiveMaterial(*doc, projectRoot, useDeferred, &err))
			doc->lastApplyError = err;
	}

	// Then every scene-assigned custom material NOT owned by an open tab
	// (the "orphaned" ones - e.g. a material loaded with the scene and
	// never opened in the editor).
	for (size_t i = 0; i < sceneDocs.size(); ++i)
		if (sceneDocs[i])
			sceneDocs[i]->RecompileOrphanedCustomMaterials(projectRoot, useDeferred, handled);
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
				RequestCloseScriptDocument(doc, closeIds);
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
				echo("SUCCESS: Saved " + project.DisplayPath(doc->absolutePath));
			}
			else
				echo("ERROR: Failed to save " + project.DisplayPath(doc->absolutePath));
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
		// Only the focused document reads the keyboard. An unfocused one used
		// to keep running the completion handler purely because its list was
		// still up, and ate the keys meant for whatever the user had clicked
		// into instead.
		if (focused)
			doc->HandleEditorInput();
		else if (doc->completionOpen)
			doc->CloseCompletion();

		doc->editor.SetHandleKeyboardInputs(doc->WantsEditorKeys(focused));
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		doc->editor.Render("##lua_editor", avail, true);
		doc->editor.SetHandleKeyboardInputs(true);
		doc->completionBlockEditorKeys = false;
		doc->AfterEditorRender();
		if (doc->completionOpen)
			doc->DrawCompletionPopup();
		ImGui::End();

		if (!open)
			RequestCloseScriptDocument(doc, closeIds);
	}
	for (size_t i = 0; i < closeIds.size(); ++i)
		CloseLuaScriptDocument(closeIds[i]);
}

void Editor::DrawMaterialEditorWindows()
{
	// Same live dock-node retargeting DrawScriptEditorWindows() does -
	// materials end up as tabs alongside scripts and Scene View.
	if (ImGuiWindow* sv = ImGui::FindWindowByName("Scene View"))
	{
		if (sv->DockId != 0)
			dockCenterId = sv->DockId;
	}

	const std::string projectRoot = project.GetProjectPath();
	std::vector<uint32_t> closeIds;
	for (size_t i = 0; i < materialDocs.size(); ++i)
	{
		MaterialEditorDocument* doc = materialDocs[i];
		if (!doc) continue;

		// Push a freshly applied shader out to the objects already using
		// this material. Polled rather than called from the Apply sites,
		// because Apply is reached from three of them (the toolbar's Save,
		// the debounced auto-apply in DrawWindow, and the agent's
		// apply_material) and none should have to know the scene exists -
		// same reason MaterialPreview polls applyGeneration. Runs before the
		// hiddenFromTabs skip below so a quietly-loaded document, which is
		// exactly what an agent-driven or Assign-material flow produces,
		// propagates too.
		if (doc->applyGeneration != doc->sceneSyncedApplyGeneration)
		{
			doc->sceneSyncedApplyGeneration = doc->applyGeneration;
			if (sceneView && !doc->generatedGlslPath.empty())
			{
				// The document's own material is already up to date - Apply
				// swapped its shader in place - and recompiling it here
				// would throw away doc.compiledShader's ownership.
				std::set<IMaterial*> skip;
				if (doc->currentMaterial) skip.insert(doc->currentMaterial.get());
				const int n = sceneView->RefreshMaterialsFromGeneratedGlsl(
					doc->generatedGlslPath, project.GetProjectPath(), UseDeferredGBuffer(), skip);
				if (n > 0)
					echo("Updated " + std::to_string(n) + " scene material(s) from " + doc->generatedGlslPath);
			}
		}

		if (doc->hiddenFromTabs) continue;

		char title[512];
		snprintf(title, sizeof(title), u8" %s###material_win_%u", doc->displayName.c_str(), doc->id);

		const bool forceDock = (pendingSelectMaterialDocId == doc->id);
		if (dockCenterId != 0)
		{
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
				RequestCloseMaterialDocument(doc, closeIds);
			continue;
		}

		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		{
			activeMaterialDoc = doc;
			lastFocusedDocKind = FocusedDocKind::Material;
		}
		if (pendingSelectMaterialDocId == doc->id)
			pendingSelectMaterialDocId = 0;

		MaterialEditor::DrawWindow(*doc, projectRoot, UseDeferredGBuffer());

		ImGui::End();

		if (!open)
			RequestCloseMaterialDocument(doc, closeIds);
	}
	for (size_t i = 0; i < closeIds.size(); ++i)
		CloseMaterialDocument(closeIds[i]);
}

// ---- Animation documents ---------------------------------------------------

AnimationEditorDocument* Editor::FindAnimationDocumentByPath(const std::string& absPath) const
{
	if (absPath.empty()) return NULL;
	for (size_t i = 0; i < animationDocs.size(); ++i)
		if (animationDocs[i] && animationDocs[i]->absolutePath == absPath) return animationDocs[i];
	return NULL;
}

std::string Editor::LookupAnimationMeshBinding(const std::string& animAbsPath) const
{
	if (!project.IsOpen() || animAbsPath.empty()) return std::string();
	const nlohmann::json& bindings = project.GetSettings().animationBindings;
	if (!bindings.is_object()) return std::string();
	const std::string rel = project.RelativePath(animAbsPath);
	if (rel.empty()) return std::string();
	auto it = bindings.find(rel);
	if (it == bindings.end() || !it->is_string()) return std::string();
	const std::string meshRel = it->get<std::string>();
	if (meshRel.empty()) return std::string();
	const std::string meshAbs = project.AbsolutePath(meshRel);
	// A binding to a model that has since been deleted or renamed should
	// fall back to guessing rather than handing the preview a path that
	// fails to load.
	std::error_code ec;
	if (!fs::exists(meshAbs, ec)) return std::string();
	return meshAbs;
}

void Editor::StoreAnimationMeshBinding(const std::string& animAbsPath, const std::string& meshAbsPath)
{
	if (!project.IsOpen() || animAbsPath.empty()) return;
	const std::string rel = project.RelativePath(animAbsPath);
	if (rel.empty()) return; // animation outside the project - nothing to key on

	nlohmann::json& bindings = project.GetSettingsMutable().animationBindings;
	if (!bindings.is_object()) bindings = nlohmann::json::object();
	const std::string meshRel = project.RelativePath(meshAbsPath);
	if (meshAbsPath.empty()) bindings.erase(rel);
	else bindings[rel] = meshRel.empty() ? meshAbsPath : meshRel;
	project.MarkDirty();
	project.Save();
}

void Editor::LoadAnimationBlend(AnimationEditorDocument& doc) const
{
	if (!project.IsOpen() || doc.absolutePath.empty()) return;
	const nlohmann::json& blends = project.GetSettings().animationBlends;
	if (!blends.is_object()) return;
	const std::string rel = project.RelativePath(doc.absolutePath);
	if (rel.empty()) return;
	auto it = blends.find(rel);
	if (it == blends.end() || !it->is_object()) return;
	doc.BlendFromJson(*it);
}

void Editor::StoreAnimationBlend(const AnimationEditorDocument& doc)
{
	if (!project.IsOpen() || doc.absolutePath.empty()) return;
	const std::string rel = project.RelativePath(doc.absolutePath);
	if (rel.empty()) return;

	nlohmann::json& blends = project.GetSettingsMutable().animationBlends;
	if (!blends.is_object()) blends = nlohmann::json::object();
	if (doc.blendEntries.empty() && doc.blendLayers.empty()) blends.erase(rel);
	else blends[rel] = doc.BlendToJson();
	project.MarkDirty();
	project.Save();
}

void Editor::BuildAnimationMeshChoices(std::vector<AnimationMeshChoice>& out) const
{
	out.clear();
	if (!project.IsOpen()) return;
	std::vector<ProjectAssetEntry> assets;
	project.ListAssets("assets/models", assets, true);
	for (size_t i = 0; i < assets.size(); ++i)
	{
		if (assets[i].isDirectory) continue;
		if (!ProjectManager::IsP3dm(assets[i].relativePath)) continue;
		if (ProjectManager::IsInternalAssetPath(assets[i].relativePath)) continue;
		out.push_back(AnimationMeshChoice(assets[i].relativePath, project.AbsolutePath(assets[i].relativePath)));
	}
}

std::string Editor::GuessAnimationMesh(const AnimationEditorDocument& doc) const
{
	// Score each candidate rig by how many of the clip's channel names it
	// actually has bones for. A clip authored for a given character matches
	// it near-perfectly and matches anything else hardly at all, so the top
	// score is unambiguous in practice - and requiring a real majority
	// (below) means "no good match" stays "no match" rather than binding a
	// random model.
	if (doc.clips.empty()) return std::string();

	std::set<std::string> channelNames;
	for (size_t c = 0; c < doc.clips.size(); ++c)
		for (size_t ch = 0; ch < doc.clips[c].Channels.size(); ++ch)
			channelNames.insert(doc.clips[c].Channels[ch].NodeName);
	if (channelNames.empty()) return std::string();

	std::vector<AnimationMeshChoice> meshes;
	BuildAnimationMeshChoices(meshes);

	std::string best;
	size_t bestHits = 0;
	for (size_t i = 0; i < meshes.size(); ++i)
	{
		// Loading a Model just to read its bone names is not cheap, but this
		// runs once per animation-document open, not per frame, and the
		// alternative (parsing .p3dm's binary skeleton block here) would be
		// a second reader of that format to keep in sync.
		std::shared_ptr<Renderable> mesh;
		try { mesh = std::make_shared<Model>(meshes[i].second, true); }
		catch (...) { continue; }
		if (!mesh) continue;
		RenderingComponent probe(mesh, ShaderUsage::Diffuse);
		if (!probe.HasBones()) continue;

		size_t hits = 0;
		const std::map<StringID, Bone>& skel = probe.GetSkeleton();
		for (std::map<StringID, Bone>::const_iterator b = skel.begin(); b != skel.end(); ++b)
			if (channelNames.count(b->second.name) > 0) hits++;

		if (hits > bestHits) { bestHits = hits; best = meshes[i].second; }
	}

	if (bestHits * 2 < channelNames.size()) return std::string();
	return best;
}

AnimationEditorDocument* Editor::NewAnimationDocument(const std::string& meshPath)
{
	AnimationEditorDocument* doc = new AnimationEditorDocument();
	doc->id = nextAnimationDocId++;
	BindUndoRouting(doc);
	doc->displayName = "NewAnimation";
	doc->meshPath = meshPath;
	// A brand new document with no clip has nothing to key into, so it
	// starts with one rather than making "+ Clip" a mandatory first step.
	doc->activeClip = doc->AddClip("Clip", 1.f);
	doc->dirty = true;
	animationDocs.push_back(doc);
	pendingSelectAnimationDocId = doc->id;
	activeAnimationDoc = doc;
	lastFocusedDocKind = FocusedDocKind::Animation;
	return doc;
}

bool Editor::OpenAnimationDocument(const std::string& absPath)
{
	if (absPath.empty()) return false;

	// A .p3dm opens a new, unsaved animation for that rig - "animate this
	// character" is the natural thing to want from a model, and there is no
	// clip file to open yet.
	if (ProjectManager::IsP3dm(absPath))
	{
		AnimationEditorDocument* doc = NewAnimationDocument(absPath);
		echo("New animation for " + project.DisplayPath(absPath));
		return doc != NULL;
	}

	if (!ProjectManager::IsAnimationExtension(absPath))
	{
		echo("ERROR: Not an animation file: " + project.DisplayPath(absPath));
		return false;
	}

	if (AnimationEditorDocument* existing = FindAnimationDocumentByPath(absPath))
	{
		pendingSelectAnimationDocId = existing->id;
		activeAnimationDoc = existing;
		lastFocusedDocKind = FocusedDocKind::Animation;
		return true;
	}

	AnimationEditorDocument* doc = new AnimationEditorDocument();
	doc->id = nextAnimationDocId++;
	BindUndoRouting(doc);
	std::string err;
	if (!doc->LoadFromFile(absPath, err))
	{
		echo("ERROR: " + err);
		delete doc;
		return false;
	}

	doc->meshPath = LookupAnimationMeshBinding(absPath);
	if (doc->meshPath.empty())
	{
		doc->meshPath = GuessAnimationMesh(*doc);
		if (!doc->meshPath.empty())
		{
			echo("Animation: matched rig " + project.DisplayPath(doc->meshPath) + " by bone names");
			StoreAnimationMeshBinding(absPath, doc->meshPath);
		}
	}

	LoadAnimationBlend(*doc);

	animationDocs.push_back(doc);
	pendingSelectAnimationDocId = doc->id;
	activeAnimationDoc = doc;
	lastFocusedDocKind = FocusedDocKind::Animation;
	echo("Opened animation " + project.DisplayPath(absPath) + " ("
		+ std::to_string(doc->clips.size()) + " clip(s))");
	return true;
}

bool Editor::SaveAnimationDocument(AnimationEditorDocument* doc, const std::string& absPath)
{
	if (!doc) return false;
	const std::string target = absPath.empty() ? doc->absolutePath : absPath;
	if (target.empty()) return false;

	std::error_code ec;
	fs::create_directories(fs::path(target).parent_path(), ec);

	std::string err;
	if (!doc->SaveToFile(target, err))
	{
		echo("ERROR: " + err);
		return false;
	}
	StoreAnimationMeshBinding(target, doc->meshPath);
	echo("SUCCESS: Saved " + project.DisplayPath(target) + " ("
		+ std::to_string(doc->clips.size()) + " clip(s))");
	return true;
}

void Editor::CloseAnimationDocument(uint32_t id)
{
	for (size_t i = 0; i < animationDocs.size(); ++i)
	{
		if (!animationDocs[i] || animationDocs[i]->id != id) continue;
		AnimationEditorDocument* doc = animationDocs[i];
		if (activeAnimationDoc == doc) activeAnimationDoc = NULL;
		animationDocs.erase(animationDocs.begin() + i);
		// The document owns an AnimationPreview holding an FBO-backed
		// renderer whose color texture this frame's ImGui draw list may
		// still reference - the same mid-frame-free hazard the material
		// previews are queued around. Deleting here is safe only because
		// document closes are processed at the END of
		// DrawAnimationEditorWindows(), after that window's ImGui::End(),
		// and the image it submitted is not sampled again until the next
		// frame's render... which is exactly the assumption that bit the
		// material path. Defer it the same way instead of re-litigating it.
		deferredDestroyAnimationDocs.push_back(doc);
		return;
	}
}

void Editor::CloseAllAnimationDocuments()
{
	for (size_t i = 0; i < animationDocs.size(); ++i)
		delete animationDocs[i];
	animationDocs.clear();
	activeAnimationDoc = NULL;
	pendingSelectAnimationDocId = 0;
	pendingCloseAnimationId = 0;
}

void Editor::RequestCloseAnimationDocument(AnimationEditorDocument* doc, std::vector<uint32_t>& closeIds)
{
	if (!doc) return;
	if (doc->dirty)
	{
		pendingCloseAnimationId = doc->id;
		ImGui::OpenPopup("Unsaved Document");
		return;
	}
	closeIds.push_back(doc->id);
}

void Editor::DrawAnimationEditorWindows()
{
	if (animationDocs.empty()) return;

	// Same live dock-node retargeting the script/material windows do.
	if (ImGuiWindow* sv = ImGui::FindWindowByName("Scene View"))
	{
		if (sv->DockId != 0) dockCenterId = sv->DockId;
	}

	std::vector<AnimationMeshChoice> meshes;
	BuildAnimationMeshChoices(meshes);

	const float dt = ImGui::GetIO().DeltaTime;
	std::vector<uint32_t> closeIds;

	for (size_t i = 0; i < animationDocs.size(); ++i)
	{
		AnimationEditorDocument* doc = animationDocs[i];
		if (!doc) continue;

		char title[512];
		snprintf(title, sizeof(title), u8" %s###animation_win_%u", doc->displayName.c_str(), doc->id);

		const bool forceDock = (pendingSelectAnimationDocId == doc->id);
		if (dockCenterId != 0)
			ImGui::SetNextWindowDockID(dockCenterId, forceDock ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
		if (forceDock) ImGui::SetNextWindowFocus();
		ImGui::SetNextWindowSize(ImVec2(1100, 720), ImGuiCond_FirstUseEver);

		bool open = true;
		ImGuiWindowFlags wflags = ImGuiWindowFlags_None;
		if (doc->dirty) wflags |= ImGuiWindowFlags_UnsavedDocument;

		if (!ImGui::Begin(title, &open, wflags))
		{
			ImGui::End();
			if (!open) RequestCloseAnimationDocument(doc, closeIds);
			continue;
		}

		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		{
			activeAnimationDoc = doc;
			lastFocusedDocKind = FocusedDocKind::Animation;
		}
		if (pendingSelectAnimationDocId == doc->id) pendingSelectAnimationDocId = 0;

		AnimationEditor::FrameRequests req;
		AnimationEditor::DrawWindow(*doc, meshes, dt, req);

		ImGui::End();

		if (req.meshChanged)
		{
			doc->meshPath = req.newMeshPath;
			doc->selectedBone = -1;
			if (!doc->absolutePath.empty())
				StoreAnimationMeshBinding(doc->absolutePath, doc->meshPath);
		}
		if (req.save)
		{
			if (doc->absolutePath.empty())
			{
				openSaveAnimationAsModal = true;
				saveAnimationAsDocId = doc->id;
				saveAnimationAsName = doc->displayName;
				saveAnimationAsError.clear();
				saveAnimationAsThenClose = false;
			}
			else SaveAnimationDocument(doc, doc->absolutePath);
		}
		if (req.saveAs)
		{
			openSaveAnimationAsModal = true;
			saveAnimationAsDocId = doc->id;
			saveAnimationAsName = doc->displayName;
			saveAnimationAsError.clear();
			saveAnimationAsThenClose = false;
		}
		if (req.blendChanged) StoreAnimationBlend(*doc);
		if (req.close) RequestCloseAnimationDocument(doc, closeIds);
		if (!open) RequestCloseAnimationDocument(doc, closeIds);
	}

	for (size_t i = 0; i < closeIds.size(); ++i)
		CloseAnimationDocument(closeIds[i]);
}

void Editor::DrawSaveAnimationAsModal()
{
	if (openSaveAnimationAsModal)
	{
		ImGui::OpenPopup("Save Animation As");
		openSaveAnimationAsModal = false;
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	if (vp) ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (!ImGui::BeginPopupModal("Save Animation As", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	AnimationEditorDocument* doc = NULL;
	for (size_t i = 0; i < animationDocs.size(); ++i)
		if (animationDocs[i] && animationDocs[i]->id == saveAnimationAsDocId) { doc = animationDocs[i]; break; }
	if (!doc)
	{
		// The document went away underneath the prompt (project close, quit).
		saveAnimationAsDocId = 0;
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	ImGui::TextUnformatted("Save into assets/animations/");
	ImGui::SetNextItemWidth(320.f);
	ImGui::InputText("##animname", &saveAnimationAsName);
	ImGui::SameLine();
	ImGui::TextDisabled(".p3da");
	if (!saveAnimationAsError.empty())
		ImGui::TextColored(ImVec4(1.f, 0.45f, 0.4f, 1.f), "%s", saveAnimationAsError.c_str());

	ImGui::Spacing();
	if (ImGui::Button("Save", ImVec2(120, 0)))
	{
		if (saveAnimationAsName.empty()) saveAnimationAsError = "Name required";
		else if (!project.IsOpen()) saveAnimationAsError = "No project open";
		else
		{
			std::string name = saveAnimationAsName;
			if (name.size() > 5 && name.compare(name.size() - 5, 5, ".p3da") == 0)
				name = name.substr(0, name.size() - 5);
			const std::string abs = project.AbsolutePath("assets/animations/" + name + ".p3da");
			if (SaveAnimationDocument(doc, abs))
			{
				const bool closeAfter = saveAnimationAsThenClose;
				const uint32_t docId = doc->id;
				saveAnimationAsDocId = 0;
				saveAnimationAsThenClose = false;
				ImGui::CloseCurrentPopup();
				if (closeAfter)
				{
					pendingCloseAnimationId = 0;
					CloseAnimationDocument(docId);
				}
			}
			else saveAnimationAsError = "Could not write the file - see the log";
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(120, 0)))
	{
		saveAnimationAsDocId = 0;
		saveAnimationAsThenClose = false;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void Editor::DrawAnimationAssetModals()
{
	if (openNewAnimationModal)
	{
		ImGui::OpenPopup("New Animation");
		openNewAnimationModal = false;
		newAnimationMeshPath.clear();
	}
	if (openImportAnimationModal)
	{
		ImGui::OpenPopup("Import Animation");
		openImportAnimationModal = false;
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	if (vp) ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("New Animation", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Rig to animate");
		std::vector<AnimationMeshChoice> meshes;
		BuildAnimationMeshChoices(meshes);

		std::string label = "(none)";
		for (size_t i = 0; i < meshes.size(); ++i)
			if (meshes[i].second == newAnimationMeshPath) { label = meshes[i].first; break; }
		ImGui::SetNextItemWidth(360.f);
		if (ImGui::BeginCombo("##newanimrig", label.c_str()))
		{
			if (ImGui::Selectable("(none)", newAnimationMeshPath.empty()))
				newAnimationMeshPath.clear();
			for (size_t i = 0; i < meshes.size(); ++i)
				if (ImGui::Selectable(meshes[i].first.c_str(), meshes[i].second == newAnimationMeshPath))
					newAnimationMeshPath = meshes[i].second;
			ImGui::EndCombo();
		}
		if (meshes.empty())
			ImGui::TextDisabled("No models in this project yet - import one first.");
		ImGui::TextDisabled("The file is written when you save.");

		ImGui::Spacing();
		if (ImGui::Button("Create", ImVec2(120, 0)))
		{
			NewAnimationDocument(newAnimationMeshPath);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	if (vp) ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Import Animation", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Source file (fbx, dae, gltf, blend, ... or an existing .p3da)");
		ImGui::SetNextItemWidth(420.f);
		ImGui::InputText("##animsrc", &importAnimationSource);
		ImGui::SameLine();
		if (ImGui::Button("Browse##animsrc"))
			ImGui::_priv::OpenLocation(importAnimationSource.empty() ? std::string() : fs::path(importAnimationSource).parent_path().string(),
				"p3da,fbx,dae,gltf,glb,blend,3ds,x,smd,md5anim", &openImportAnimationBrowse);
		if (openImportAnimationBrowse)
		{
			std::string picked;
			if (ImGui::FilePath("##animsrcbrowse", "", "p3da,fbx,dae,gltf,glb,blend,3ds,x,smd,md5anim",
				&picked, 1024, &openImportAnimationBrowse))
			{
				if (!picked.empty()) importAnimationSource = picked;
			}
		}

		ImGui::TextUnformatted("Save as");
		ImGui::SetNextItemWidth(300.f);
		ImGui::InputText("##animimportname", &importAnimationName);
		ImGui::SameLine();
		ImGui::TextDisabled(".p3da  (blank = source filename)");

		if (!importAnimationError.empty())
			ImGui::TextColored(ImVec4(1.f, 0.45f, 0.4f, 1.f), "%s", importAnimationError.c_str());

		ImGui::Spacing();
		if (ImGui::Button("Import", ImVec2(120, 0)))
		{
			std::string out, err, trashed;
			if (importAnimationSource.empty())
				importAnimationError = "Pick a source file";
			else if (project.ImportAnimation(importAnimationSource, importAnimationName, out, &err, &trashed))
			{
				const std::string rel = project.RelativePath(out);
				echo("Imported animation: " + (rel.empty() ? out : rel));
				if (sceneView && !trashed.empty() && !rel.empty())
					sceneView->PushUndoCommand(std::make_unique<ImportOverwriteCommand>(&project, rel, trashed,
						"Import (overwrite) '" + rel + "'"));
				selectedAssetRel = rel;
				project.Save();
				OpenAnimationDocument(out);
				importAnimationError.clear();
				ImGui::CloseCurrentPopup();
			}
			else
			{
				importAnimationError = err;
				echo("ERROR: " + err);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			importAnimationError.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void Editor::FlushDeferredAnimationDocs()
{
	for (size_t i = 0; i < deferredDestroyAnimationDocs.size(); ++i)
		delete deferredDestroyAnimationDocs[i];
	deferredDestroyAnimationDocs.clear();
}

Editor::AssetMaterialKind Editor::GetAssetMaterialKind(const std::string& absPath)
{
	long long mtime = 0;
	std::error_code ec;
	const fs::file_time_type ft = fs::last_write_time(absPath, ec);
	if (!ec)
		mtime = (long long)ft.time_since_epoch().count();

	std::map<std::string, AssetMaterialInfo>::iterator it = assetMaterialKinds.find(absPath);
	if (it != assetMaterialKinds.end() && it->second.mtime == mtime)
		return it->second.kind;

	AssetMaterialInfo info;
	info.mtime = mtime;
	info.kind = AssetMaterialKind::Unknown;
	try
	{
		std::ifstream f(absPath);
		if (f)
		{
			nlohmann::json j;
			f >> j;
			const std::string kind = j.value("kind", std::string());
			if (kind == "generic")
				info.kind = AssetMaterialKind::Generic;
			else if (kind == "custom")
				info.kind = (j.value("editMode", std::string()) == "text")
					? AssetMaterialKind::CustomText : AssetMaterialKind::CustomNodes;
		}
	}
	catch (...)
	{
		// A malformed or half-written .mat just shows the neutral icon -
		// the Assets panel is not the place to report parse errors.
	}
	assetMaterialKinds[absPath] = info;
	return info.kind;
}

void Editor::RequestCloseScriptDocument(CodeEditorDocument* doc, std::vector<uint32_t>& closeIds)
{
	if (!doc) return;
	if (!doc->dirty) { closeIds.push_back(doc->id); return; }
	pendingCloseScriptId = doc->id;
	ImGui::OpenPopup("Unsaved Document");
}

void Editor::RequestCloseMaterialDocument(MaterialEditorDocument* doc, std::vector<uint32_t>& closeIds)
{
	if (!doc) return;
	if (!doc->dirty) { closeIds.push_back(doc->id); return; }
	pendingCloseMaterialId = doc->id;
	ImGui::OpenPopup("Unsaved Document");
}

// Shared Save / Don't Save / Cancel prompt for a dirty script or material tab
// being closed. Modelled on SceneEditor::DrawUnsavedChangesModal(), which
// already guards the scene the same way - closing a *document* was the one
// path left that threw edits away without asking.
void Editor::DrawUnsavedDocumentModal()
{
	if (pendingCloseScriptId == 0 && pendingCloseMaterialId == 0 && pendingCloseAnimationId == 0)
		return;

	// Resolve the pending id to a live document every frame rather than
	// caching a pointer: this prompt spans frames, and anything that closes
	// documents underneath it (project close, quit) would leave that pointer
	// dangling. A document that vanished simply cancels the prompt.
	CodeEditorDocument* script = NULL;
	MaterialEditorDocument* material = NULL;
	AnimationEditorDocument* animation = NULL;
	if (pendingCloseScriptId != 0)
		for (size_t i = 0; i < scriptDocs.size(); ++i)
			if (scriptDocs[i] && scriptDocs[i]->id == pendingCloseScriptId) { script = scriptDocs[i]; break; }
	if (pendingCloseMaterialId != 0)
		for (size_t i = 0; i < materialDocs.size(); ++i)
			if (materialDocs[i] && materialDocs[i]->id == pendingCloseMaterialId) { material = materialDocs[i]; break; }
	if (pendingCloseAnimationId != 0)
		for (size_t i = 0; i < animationDocs.size(); ++i)
			if (animationDocs[i] && animationDocs[i]->id == pendingCloseAnimationId) { animation = animationDocs[i]; break; }
	if (!script && !material && !animation)
	{
		pendingCloseScriptId = 0;
		pendingCloseMaterialId = 0;
		pendingCloseAnimationId = 0;
		return;
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	if (vp)
		ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Unsaved Document", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const std::string name = script ? script->displayName
			: (material ? material->displayName : animation->displayName);
		ImGui::Text("Save changes to '%s' before closing?", name.c_str());
		ImGui::TextDisabled("%s", script ? "Script has unsaved edits."
			: (material ? "Material has unsaved edits." : "Animation has unsaved edits."));
		ImGui::Spacing();

		if (ImGui::Button("Save", ImVec2(110, 0)))
		{
			bool saved = false;
			if (animation)
			{
				// Never saved anywhere: hand it to the Save As prompt
				// instead of inventing a path, and let that prompt finish
				// the close (saveAnimationAsThenClose).
				if (animation->absolutePath.empty())
				{
					openSaveAnimationAsModal = true;
					saveAnimationAsDocId = animation->id;
					saveAnimationAsName = animation->displayName;
					saveAnimationAsError.clear();
					saveAnimationAsThenClose = true;
					pendingCloseAnimationId = 0;
					ImGui::CloseCurrentPopup();
					ImGui::EndPopup();
					return;
				}
				saved = SaveAnimationDocument(animation, animation->absolutePath);
			}
			else if (script)
			{
				saved = script->SaveToFile();
				if (saved)
				{
					script->dirty = false;
					echo("SUCCESS: Saved " + project.DisplayPath(script->absolutePath));
				}
				else
					echo("ERROR: Failed to save " + project.DisplayPath(script->absolutePath));
			}
			else
			{
				// Same fallback DrawToolbar's own Save button uses for a
				// material that has never been written anywhere yet.
				std::string path = material->absolutePath;
				if (path.empty())
					path = project.AbsolutePath("assets/materials/" + material->materialName + ".mat");
				saved = MaterialEditor::SaveToFile(*material, path, project.GetProjectPath(), UseDeferredGBuffer());
				if (!saved)
					material->lastApplyError = "Could not save to " + path;
			}

			// Only close once the edits are actually on disk - a failed save
			// leaves the document open rather than discarding what it holds.
			if (saved)
			{
				if (script) CloseLuaScriptDocument(pendingCloseScriptId);
				else if (material) CloseMaterialDocument(pendingCloseMaterialId);
				else CloseAnimationDocument(pendingCloseAnimationId);
				pendingCloseScriptId = 0;
				pendingCloseMaterialId = 0;
				pendingCloseAnimationId = 0;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Don't Save", ImVec2(110, 0)))
		{
			if (script) CloseLuaScriptDocument(pendingCloseScriptId);
			else if (material) CloseMaterialDocument(pendingCloseMaterialId);
			else CloseAnimationDocument(pendingCloseAnimationId);
			pendingCloseScriptId = 0;
			pendingCloseMaterialId = 0;
			pendingCloseAnimationId = 0;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(110, 0)))
		{
			pendingCloseScriptId = 0;
			pendingCloseMaterialId = 0;
			pendingCloseAnimationId = 0;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

bool Editor::DrawActiveMaterialProperties()
{
	// Only when a material document is the focused document. Clicking
	// inside Properties itself never changes lastFocusedDocKind (only the
	// Scene View / Scene Tree and the material windows set it), so editing
	// these controls does not pull the panel out from under itself.
	if (lastFocusedDocKind != FocusedDocKind::Material)
		return false;
	// activeMaterialDoc is cleared by CloseMaterialDocument()/
	// CloseAllMaterialDocuments(), so it cannot dangle here.
	MaterialEditorDocument* doc = activeMaterialDoc;
	if (!doc || doc->hiddenFromTabs || !doc->currentMaterial)
		return false;
	// Drawn after DrawMaterialEditorWindows() in DrawUI(), so
	// activeMaterialDoc already reflects this frame's focus.
	MaterialEditor::DrawProperties(*doc, project.GetProjectPath());
	return true;
}

void Editor::DrawSceneViewWindow()
{
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

	if (!ImGui::Begin("Scene View", &showingSceneView, flags))
	{
		// Collapsed, or a background tab (the Material Editor docks into
		// this same tab bar, so that happens constantly) - ShowViewport()
		// will not run this frame, so the cached viewport rect the raw-SDL
		// mouse handlers gate on is about to describe whatever window took
		// the viewport's place. See SceneEditor::NotifyViewportNotDrawn().
		if (sceneView)
			sceneView->NotifyViewportNotDrawn();
		ImGui::End();
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		lastFocusedDocKind = FocusedDocKind::Scene;

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
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		lastFocusedDocKind = FocusedDocKind::Scene;
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
		ImGui::SetNextWindowFocus();
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
				if (sceneView && !selectedAssetRel.empty())
					sceneView->PushUndoCommand(std::make_unique<CreateAssetCommand>(&project, selectedAssetRel, "Create Script '" + selectedAssetRel + "'"));
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

	if (openNewMaterialModal)
	{
		ImGui::SetNextWindowFocus();
		ImGui::OpenPopup("New Material");
		openNewMaterialModal = false;
	}
	if (ImGui::BeginPopupModal("New Material", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Create a material under assets/materials/");
		ImGui::SetNextItemWidth(280.f);
		ImGui::InputText("Name", &newMaterialName);
		static const char* kindLabels[] = { "Generic Shader", "Custom Shader" };
		ImGui::SetNextItemWidth(280.f);
		ImGui::Combo("Type", &newMaterialKindCombo, kindLabels, IM_ARRAYSIZE(kindLabels));
		if (newMaterialKindCombo == 1)
		{
			// Text and Node Graph are two separate, INCOMPATIBLE
			// representations of a Custom material - neither one converts
			// to the other, so this has to be picked up front rather than
			// defaulted and switched later (switching modes in the Material
			// Editor's own Type combo throws away whichever one you're
			// leaving - see MaterialEditor::CreateNewMaterial).
			static const char* customModeLabels[] = { "Node Graph", "Text (GLSL)" };
			ImGui::SetNextItemWidth(280.f);
			ImGui::Combo("Editing Mode", &newMaterialCustomModeCombo, customModeLabels, IM_ARRAYSIZE(customModeLabels));
		}
		if (!newMaterialError.empty())
			ImGui::TextColored(ImVec4(1.f, 0.45f, 0.4f, 1.f), "%s", newMaterialError.c_str());
		ImGui::Spacing();
		if (ImGui::Button("Create", ImVec2(120, 0)))
		{
			std::string abs;
			std::string err;
			MaterialAssetKind kind = (newMaterialKindCombo == 1) ? MaterialAssetKind::Custom : MaterialAssetKind::Generic;
			const bool useTextMode = (newMaterialKindCombo == 1) && (newMaterialCustomModeCombo == 1);
			if (project.CreateMaterial(newMaterialName, kind, abs, &err, useTextMode))
			{
				echo("SUCCESS: Created material " + abs);
				selectedAssetRel = project.RelativePath(abs);
				if (sceneView && !selectedAssetRel.empty())
					sceneView->PushUndoCommand(std::make_unique<CreateAssetCommand>(&project, selectedAssetRel, "Create Material '" + selectedAssetRel + "'"));
				OpenMaterialDocument(abs);
				project.Save();
				ImGui::CloseCurrentPopup();
			}
			else
			{
				newMaterialError = err;
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
		const bool isMat = ProjectManager::IsMaterialExtension(e.relativePath);
		const bool isAnim = ProjectManager::IsAnimationExtension(e.relativePath);
		const bool selected = (selectedAssetRel == e.relativePath);

		// Materials get an icon per kind, plus a corner badge below - the
		// glyph alone reads as "some material" at 28px, and which authoring
		// mode a material uses is fixed at creation and cannot be told apart
		// any other way without opening it.
		// One palette icon for every material - the kind is carried by the
		// badge below, not by the glyph, so materials stay visually one
		// family in the grid.
		const AssetMaterialKind matKind = isMat ? GetAssetMaterialKind(abs) : AssetMaterialKind::Unknown;
		const char* matBadge = NULL;
		if (isMat)
		{
			if (matKind == AssetMaterialKind::CustomText) matBadge = "GLSL";
			else if (matKind == AssetMaterialKind::CustomNodes) matBadge = "NODES";
			else if (matKind == AssetMaterialKind::Generic) matBadge = "GENERIC";
		}

		const char* typeIcon = isScene ? u8"\uf1c0"
			: (isModel ? u8"\uf1b2"
			: (isSound ? u8"\uf028"
			: (isTex ? u8"\uf03e"
			: (isLua ? u8"\uf121"
			: (ProjectManager::IsShaderExtension(e.relativePath) ? u8"\uf0eb"
			: (isMat ? u8"\uf53f"
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

			// Kind badge along the bottom of the thumb area. Spelled out
			// rather than relying on the glyph alone, which is the whole
			// point - telling a node-graph material from a text one at a
			// glance.
			if (matBadge)
			{
				const float badgeH = 12.f;
				const ImVec2 bsz = font->CalcTextSizeA(badgeH, FLT_MAX, 0.f, matBadge);
				const ImVec2 bpos(thumbMin.x + (thumbMax.x - thumbMin.x - bsz.x) * 0.5f,
					thumbMax.y - badgeH - 1.f);
				dl->AddRectFilled(ImVec2(bpos.x - 3.f, bpos.y - 1.f),
					ImVec2(bpos.x + bsz.x + 3.f, bpos.y + badgeH + 1.f),
					ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 0.55f)), 3.f);
				dl->AddText(font, badgeH, bpos,
					ImGui::GetColorU32(matKind == AssetMaterialKind::Generic
						? ImVec4(0.75f, 0.85f, 1.f, 1.f)
						: (matKind == AssetMaterialKind::CustomText
							? ImVec4(1.f, 0.85f, 0.5f, 1.f)
							: ImVec4(0.6f, 1.f, 0.75f, 1.f))),
					matBadge);
			}
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
			else if (isMat)
				OpenMaterialDocument(abs);
			else if (isAnim)
				OpenAnimationDocument(abs);
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
			// What you right-clicked comes first, then what you can do with
			// it, then the create-new entries that belong to the folder
			// rather than to this tile, and Delete last.
			if (isScene && ImGui::MenuItem("Open Scene"))
				OpenSceneDocument(abs);
			if (isLua && ImGui::MenuItem("Open Script"))
				OpenLuaScriptDocument(abs);
			if (isMat && ImGui::MenuItem("Open Material"))
				OpenMaterialDocument(abs);
			if (isAnim && ImGui::MenuItem("Open Animation"))
				OpenAnimationDocument(abs);
			// A model is the other way into the Animation Editor: it opens
			// a new, empty clip already bound to that rig.
			if (isModel && ImGui::MenuItem("Animate This Model"))
				OpenAnimationDocument(abs);
			if ((isModel || isSound) && ImGui::MenuItem("Place in Scene") && sceneView)
				sceneView->PlaceAssetInScene(abs);
			if (isScene || isLua || isMat || isAnim || isModel || isSound)
				ImGui::Separator();
			ShowAssetCreateMenuItems();
			ImGui::Separator();
			if (ImGui::MenuItem("Delete"))
			{
				pendingDeleteAssetRel = e.relativePath;
				openDeleteAssetModal = true;
			}
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
		ShowAssetCreateMenuItems();
		ImGui::EndPopup();
	}

	ImGui::EndChild();

	if (highlightDrop)
		ImGui::PopStyleColor();

	if (openDeleteAssetModal)
	{
		ImGui::SetNextWindowFocus();
		ImGui::OpenPopup("Delete Asset");
		openDeleteAssetModal = false;
	}
	if (ImGui::BeginPopupModal("Delete Asset", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped("Delete \"%s\" from the project? (Ctrl+Z to undo.)", pendingDeleteAssetRel.c_str());
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
			std::string trashRel, movedFromRel;
			if (project.DeleteAsset(pendingDeleteAssetRel, &err, &trashRel, &movedFromRel))
			{
				echo("SUCCESS: Deleted " + pendingDeleteAssetRel);
				if (selectedAssetRel == pendingDeleteAssetRel)
					selectedAssetRel.clear();
				if (sceneView && !trashRel.empty())
					sceneView->PushUndoCommand(std::make_unique<DeleteAssetCommand>(&project, movedFromRel, trashRel));
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
	FlushDeferredPreviewRenderers();
	FlushDeferredAnimationDocs();
}

void Editor::MouseMove(Event::Input::Info e)
{
	//gizmo->OnMouseMove(mPos.x, mPos.y);
}

void Editor::Shutdown()
{
    // All your Shutdown Code Here
	agentServer.Stop();
	PyrosFileDrop::SetHandler(NULL);
	PyrosWindowClose::SetHandler(NULL);
	ClearAssetPreviews();
	FlushDeferredPreviewDestroy();
	if (welcomeLogo)
	{
		delete welcomeLogo;
		welcomeLogo = NULL;
	}

	// Animation previews FIRST, before the scene documents go. Each one
	// owns a PostEffectsManager whose device pointer is *borrowed* from the
	// process-wide active render device (see ResolvePostEffectsDevice), and
	// ~PostEffectsManager's very first statement is device->WaitIdle().
	// Tearing down the scene documents clears/destroys that device - measured
	// directly: at this point after CloseAllSceneDocuments(),
	// IsActiveRenderDeviceSet() is already false - so destroying a preview
	// afterwards dereferences a freed device and segfaults on every clean
	// exit that had an animation tab open. Same latent hazard IRenderer's
	// ResolveInitialDevice() comment describes for renderers outliving their
	// device; the fix here is simply to go first, while it is still alive.
	CloseAllAnimationDocuments();
	FlushDeferredAnimationDocs();

	CloseAllSceneDocuments();
	CloseAllLuaScriptDocuments();
	CloseAllMaterialDocuments();
	// CloseAllMaterialDocuments() queued any live previews above - finish
	// them now (no more frames coming on the shutdown path). NOTE: a
	// MaterialPreview holds a PostEffectsManager too, so this has the same
	// dangling-device exposure the animation previews were moved above to
	// avoid; it just needs a material tab open at quit to show it.
	FlushDeferredPreviewRenderers();
	sceneView = NULL;

	delete sharedAudio;
	sharedAudio = NULL;
#ifdef LUA_BINDINGS
	luaReady = false;
#endif

	delete tabProperties;
	delete tabTools;
	delete tabAI;
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

void Editor::BindUndoRouting(SceneEditor* doc)
{
	if (!doc) return;
	doc->SetUndoPushHook([this]() { lastFocusedDocKind = FocusedDocKind::Scene; });
}

void Editor::BindUndoRouting(MaterialEditorDocument* doc)
{
	if (!doc) return;
	// Capturing doc is safe: the stack is a member of the document, so the
	// callback cannot outlive what it captures.
	doc->undo.onPush = [this, doc]() {
		lastFocusedDocKind = FocusedDocKind::Material;
		activeMaterialDoc = doc;
	};
}

void Editor::BindUndoRouting(AnimationEditorDocument* doc)
{
	if (!doc) return;
	doc->undo.onPush = [this, doc]() {
		lastFocusedDocKind = FocusedDocKind::Animation;
		activeAnimationDoc = doc;
	};
}

SceneEditor* Editor::CreateSceneDocument()
{
	SceneEditor* doc = new SceneEditor(nextSceneDocId++);
	BindUndoRouting(doc);
	doc->SetSharedAudioManager(sharedAudio);
#ifdef LUA_BINDINGS
	if (luaReady)
		doc->SetSharedLua(&lua);
#endif
	// Must be set before Init() - that's where the renderer actually gets
	// constructed (Forward or Deferred, see SceneEditor::Init()'s comment).
	doc->SetUseDeferredRenderer(UseDeferredGBuffer());
	doc->Init(Width, Height);
	doc->SetProjectManager(&project);
	doc->SetHostCallbacks(&Editor::HostCloseProject, &Editor::HostQuitApp,
		&Editor::HostNewProject, &Editor::HostOpenProject,
		&Editor::HostQuitDiscardingUnsaved);
	doc->SetHostDocumentCallbacks(&Editor::HostActivateSceneDocument,
		&Editor::HostRequestCloseSceneDocument,
		&Editor::HostNewSceneDocument,
		&Editor::HostOpenSceneDocument,
		&Editor::HostOpenLuaScript,
		&Editor::HostEditMaterialInline,
		&Editor::HostAssignMaterialAsset);
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
		echo("ERROR: Could not open script " + project.DisplayPath(absPath));
		delete doc;
		return false;
	}
	scriptDocs.push_back(doc);
	activeScriptDoc = doc;
	pendingSelectScriptId = doc->id;
	echo("SUCCESS: Opened script " + project.DisplayPath(absPath));
	return true;
}

void Editor::ReloadScriptDocumentFromDisk(const std::string& absPath)
{
	for (size_t i = 0; i < scriptDocs.size(); ++i)
	{
		CodeEditorDocument* doc = scriptDocs[i];
		if (!doc || doc->absolutePath != absPath) continue;
		std::ifstream in(absPath);
		if (!in) return;
		std::ostringstream ss;
		ss << in.rdbuf();
		doc->editor.SetText(ss.str());
		doc->dirty = false;
		doc->CloseCompletion();
		return;
	}
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

MaterialEditorDocument* Editor::FindMaterialDocumentByPath(const std::string& absPath) const
{
	if (absPath.empty()) return NULL;
	for (size_t i = 0; i < materialDocs.size(); ++i)
	{
		if (materialDocs[i] && materialDocs[i]->absolutePath == absPath)
			return materialDocs[i];
	}
	return NULL;
}

bool Editor::OpenMaterialDocument(const std::string& absPath)
{
	if (absPath.empty() || !project.IsOpen()) return false;
	if (MaterialEditorDocument* existing = FindMaterialDocumentByPath(absPath))
	{
		existing->hiddenFromTabs = false; // explicitly opening it now, even if it was only loaded silently for an assignment before
		activeMaterialDoc = existing;
		pendingSelectMaterialDocId = existing->id;
		return true;
	}

	MaterialEditorDocument* doc = new MaterialEditorDocument();
	doc->id = nextMaterialDocId++;
	BindUndoRouting(doc);
	if (!MaterialEditor::LoadFromFile(*doc, absPath, project.GetProjectPath(), UseDeferredGBuffer()))
	{
		echo("ERROR: Could not open material " + project.DisplayPath(absPath));
		delete doc;
		return false;
	}
	materialDocs.push_back(doc);
	activeMaterialDoc = doc;
	pendingSelectMaterialDocId = doc->id;
	echo("SUCCESS: Opened material " + project.DisplayPath(absPath));
	return true;
}

MaterialEditorDocument* Editor::EditMaterialInline(std::shared_ptr<IMaterial> mat, const std::string& ownerLabel)
{
	if (!mat) return NULL;
	// Keyed by the live IMaterial* pointer, not a path - there isn't one yet
	// for a material that only exists attached to a scene object.
	for (size_t i = 0; i < materialDocs.size(); ++i)
	{
		if (materialDocs[i] && materialDocs[i]->currentMaterial.get() == mat.get())
		{
			materialDocs[i]->hiddenFromTabs = false;
			activeMaterialDoc = materialDocs[i];
			pendingSelectMaterialDocId = materialDocs[i]->id;
			return materialDocs[i];
		}
	}

	MaterialEditorDocument* doc = new MaterialEditorDocument();
	doc->id = nextMaterialDocId++;
	BindUndoRouting(doc);
	doc->currentMaterial = mat;
	doc->materialName = ownerLabel;
	doc->displayName = ownerLabel;
	doc->dirty = false;
	if (dynamic_cast<GenericShaderMaterial*>(mat.get()))
	{
		doc->editKind = MaterialEditKind::Generic;
		doc->editMode = MaterialEditMode::Inspector;
	}
	else
	{
		doc->editKind = MaterialEditKind::Custom;
		doc->editMode = MaterialEditMode::Text;
		if (auto* cm = dynamic_cast<CustomShaderMaterial*>(mat.get()))
		{
			doc->generatedGlslPath = cm->GetShaderFile();
			doc->codeDoc = new CodeEditorDocument();
			doc->codeDoc->editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
			doc->codeDoc->editor.SetPalette(TextEditor::GetDarkPalette());
			doc->codeDoc->editor.SetShowWhitespaces(false);
			doc->codeDoc->editor.SetTabSize(4);
			if (cm->GetShaderObject())
				doc->codeDoc->editor.SetText(cm->GetShaderObject()->GetShaderText());
		}
	}
	materialDocs.push_back(doc);
	activeMaterialDoc = doc;
	pendingSelectMaterialDocId = doc->id;
	return doc;
}

// The doc owns its live material's Shader* (doc.compiledShader) while
// CustomShaderMaterial::shader is only a non-owning raw pointer to it. If
// the material outlives the doc (still assigned to a scene GameObject),
// move ownership into the material BEFORE `delete doc` frees it -
// otherwise the material's shader pointer dangles.
static void AdoptDocumentShaderIfShared(MaterialEditorDocument* doc)
{
	if (!doc || !doc->compiledShader || !doc->currentMaterial || doc->currentMaterial.use_count() <= 1)
		return;
	if (auto* cm = dynamic_cast<CustomShaderMaterial*>(doc->currentMaterial.get()))
		cm->AdoptShader(std::move(doc->compiledShader));
}

// The doc's sphere preview owns an FBO whose color texture this frame's
// ImGui draw list already sampled (its window was drawn earlier this frame)
// - freeing it mid-frame is the Metal/Vulkan crash class
// deferredDestroyPreviews guards against. Pull it out of the doc and
// destroy it after the frame's rasterization instead.
void Editor::QueuePreviewForDeferredDestroy(MaterialEditorDocument* doc)
{
	if (doc && doc->preview)
		deferredDestroyPreviewRenderers.push_back(doc->preview.release());
}

void Editor::CloseMaterialDocument(uint32_t id)
{
	for (size_t i = 0; i < materialDocs.size(); ++i)
	{
		if (!materialDocs[i] || materialDocs[i]->id != id) continue;
		MaterialEditorDocument* doc = materialDocs[i];
		if (activeMaterialDoc == doc)
			activeMaterialDoc = NULL;
		AdoptDocumentShaderIfShared(doc);
		QueuePreviewForDeferredDestroy(doc);
		delete doc;
		materialDocs.erase(materialDocs.begin() + (std::ptrdiff_t)i);
		break;
	}
}

void Editor::CloseAllMaterialDocuments()
{
	for (size_t i = 0; i < materialDocs.size(); ++i)
	{
		AdoptDocumentShaderIfShared(materialDocs[i]);
		QueuePreviewForDeferredDestroy(materialDocs[i]);
		delete materialDocs[i];
	}
	materialDocs.clear();
	activeMaterialDoc = NULL;
}

void Editor::FlushDeferredPreviewRenderers()
{
	for (size_t i = 0; i < deferredDestroyPreviewRenderers.size(); ++i)
		delete deferredDestroyPreviewRenderers[i];
	deferredDestroyPreviewRenderers.clear();
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
		echo("ERROR: failed to open scene: " + project.DisplayPath(absPath));
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
			echo("SUCCESS: Saved " + project.DisplayPath(doc->absolutePath));
		}
		else
		{
			allOk = false;
			echo("ERROR: Failed to save " + project.DisplayPath(doc->absolutePath));
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

void Editor::HostEditMaterialInline(std::shared_ptr<IMaterial> mat, const std::string& ownerLabel)
{
	Editor* ed = Editor::getInstance();
	if (ed) ed->EditMaterialInline(mat, ownerLabel);
}

Editor::~Editor()
{
	// Belt and braces for shutdown paths that delete the editor without a
	// full Shutdown() (queued material previews must not leak).
	FlushDeferredPreviewRenderers();
	FlushDeferredAnimationDocs();
	if (instance == this) {
		instance = NULL;
	}
}
