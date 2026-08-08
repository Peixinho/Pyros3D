//============================================================================
// Name        : DemoLauncher.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See DemoLauncher.h.
//============================================================================

#include "DemoLauncher.h"

#include <Pyros3D/Other/PyrosGL.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <SDL2/SDL.h>
#include <cstring>
#include <fstream>
#include <cstdlib>

using namespace p3d;

namespace {

sol::object JsonToSol(sol::state &lua, const json &j)
{
	if (j.is_null()) return sol::make_object(lua, sol::lua_nil);
	if (j.is_boolean()) return sol::make_object(lua, j.get<bool>());
	if (j.is_number()) return sol::make_object(lua, j.get<double>());
	if (j.is_string()) return sol::make_object(lua, j.get<std::string>());
	if (j.is_array())
	{
		sol::table t = lua.create_table((int)j.size(), 0);
		for (size_t i = 0; i < j.size(); ++i)
			t[i + 1] = JsonToSol(lua, j[i]);
		return t;
	}
	if (j.is_object())
	{
		sol::table t = lua.create_table(0, (int)j.size());
		for (auto it = j.begin(); it != j.end(); ++it)
			t[it.key()] = JsonToSol(lua, it.value());
		return t;
	}
	return sol::make_object(lua, sol::lua_nil);
}

} // namespace

DemoLauncher::DemoLauncher() : ClassName(1280, 720, "Pyros3D - Demos", WindowType::Close | WindowType::Resize)
{
	Scene = nullptr;
	physics = nullptr;
	activeDemo = -1;
	activeDemoHasPhysics = false;
	imguiInitialized = false;
}

DemoLauncher::~DemoLauncher() {}

void DemoLauncher::OnResize(const uint32 width, const uint32 height)
{
	ClassName::OnResize(width, height);

	sol::table host = lua["RenderHost"];
	if (host.valid())
	{
		sol::function resize = host["resize"];
		if (resize.valid()) resize((int)width, (int)height);
	}
}

void DemoLauncher::Init()
{
	ClassName::Init();

	Scene = new SceneGraph();

	physics = new Physics();
	physics->InitPhysics();

	GenerateBindings(&lua);
	lua["physics"] = static_cast<IPhysics*>(physics);
	lua.require_file("class", STR(EXAMPLES_PATH) "/assets/middleclass.lua");
	lua.require_file("RenderHost", STR(EXAMPLES_PATH) "/assets/demos/scripts/render_host.lua");

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

	sol::table imgui = lua.create_table();
	imgui["text"] = [](const std::string &s) { ImGui::TextUnformatted(s.c_str()); };
	imgui["separator"] = []() { ImGui::Separator(); };
	imgui["sliderFloat"] = [](const std::string &label, float value, float minV, float maxV) {
		ImGui::SliderFloat(label.c_str(), &value, minV, maxV);
		return value;
	};
	imgui["checkbox"] = [](const std::string &label, bool value) {
		ImGui::Checkbox(label.c_str(), &value);
		return value;
	};
	imgui["drawCrosshair"] = []() {
		ImGuiIO &io = ImGui::GetIO();
		ImVec2 aim = (SDL_GetRelativeMouseMode() == SDL_TRUE)
			? ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f)
			: io.MousePos;
		ImDrawList *dl = ImGui::GetForegroundDrawList();
		const float arm = 10.0f, gap = 3.0f;
		const ImU32 col = IM_COL32(255, 255, 255, 220);
		const ImU32 outline = IM_COL32(0, 0, 0, 180);
		auto line = [&](float x0, float y0, float x1, float y1) {
			dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), outline, 3.0f);
			dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), col, 1.5f);
		};
		line(aim.x - arm, aim.y, aim.x - gap, aim.y);
		line(aim.x + gap, aim.y, aim.x + arm, aim.y);
		line(aim.x, aim.y - arm, aim.x, aim.y - gap);
		line(aim.x, aim.y + gap, aim.x, aim.y + arm);
		dl->AddCircle(aim, 2.5f, outline, 12, 2.0f);
		dl->AddCircle(aim, 2.5f, col, 12, 1.0f);
	};
	lua["imgui"] = imgui;

	lua["scene"] = Scene;
	lua["projection"] = &projection;
	lua["EXAMPLES_PATH"] = STR(EXAMPLES_PATH);
	lua["ASSETS_PATH"] = STR(EXAMPLES_PATH) "/assets/";

	LoadManifest();
	if (!demos.empty())
	{
		int start = 0;
		if (const char *env = std::getenv("PYROS_DEMO_INDEX"))
			start = atoi(env);
		if (start < 0 || start >= (int)demos.size()) start = 0;
		SwitchDemo(start);
	}

	InitImGui();
}

void DemoLauncher::LoadManifest()
{
	std::ifstream in((STR(EXAMPLES_PATH) "/assets/demos/manifest.json"));
	if (!in.is_open())
	{
		echo("ERROR: DemoLauncher - couldn't open manifest.json");
		return;
	}
	json root;
	try { in >> root; }
	catch (const std::exception&)
	{
		echo("ERROR: DemoLauncher - invalid JSON in manifest.json");
		return;
	}
	for (auto &e : root)
	{
		DemoEntry entry;
		entry.name = e.value("name", std::string());
		entry.description = e.value("description", std::string());
		entry.scene = e.value("scene", std::string());
		if (e.find("render") != e.end())
			entry.render = e["render"];
		else
		{
			entry.render = json::object();
			entry.render["type"] = "deferred";
			if (e.find("effects") != e.end())
				entry.render["effects"] = e["effects"];
		}
		demos.push_back(entry);
	}
}

void DemoLauncher::SwitchDemo(int index)
{
	if (index < 0 || index >= (int)demos.size()) return;

	for (const std::shared_ptr<GameObject> &go : currentAssets.gameObjects)
	{
		for (const std::shared_ptr<IComponent> &c : go->GetComponents())
		{
			if (c && c->GetComponentType() == ComponentType::LuaComponent)
				c->Destroy();
		}
	}

	if (!currentAssets.gameObjects.empty())
		SceneSerializer::UnloadScene(Scene, currentAssets);
	currentAssets = LoadedSceneAssets();
	activeDemoHasPhysics = false;
	lua["camera"] = sol::lua_nil;

	const DemoEntry &entry = demos[index];
	activeDemo = index;

	{
		sol::table host = lua["RenderHost"];
		sol::function setup = host.valid() ? host["setup"] : sol::function();
		if (setup.valid())
			setup(JsonToSol(lua, entry.render), (int)Width, (int)Height);
		else
			echo("ERROR: DemoLauncher - RenderHost.setup missing");
	}

	SceneSerializer::LoadScene(Scene, STR(EXAMPLES_PATH) + std::string("/assets/demos/") + entry.scene, physics, &lua, &currentAssets);

	// Camera scripts (and other scene Lua) register globals like `camera`
	// during init - run before the first Update/draw.
	{
		std::ofstream dbg("/tmp/pyros_demolauncher_switch.log", std::ios::app);
		if (dbg)
		{
			dbg << "  pre-Init globals: scene_type=" << (int)lua["scene"].get_type()
				<< " assets=" << lua.get_or<std::string>("ASSETS_PATH", "<nil>")
				<< " renderer_type=" << (int)lua["renderer"].get_type()
				<< std::endl;
		}
	}
	for (const std::shared_ptr<GameObject> &go : currentAssets.gameObjects)
	{
		for (const std::shared_ptr<IComponent> &c : go->GetComponents())
		{
			if (c && c->GetComponentType() == ComponentType::LuaComponent)
			{
				auto lc = std::static_pointer_cast<LuaComponent>(c);
				const bool hasInit = lc->on_init != nullptr;
				const bool hasData = lc->data.valid();
				{
					std::ofstream dbg("/tmp/pyros_demolauncher_switch.log", std::ios::app);
					if (dbg) dbg << "  LuaComponent go='" << go->GetName() << "' script='" << lc->scriptFile
						<< "' data=" << (hasData ? "yes" : "no") << " on_init=" << (hasInit ? "yes" : "no") << std::endl;
				}
				try {
					c->Init();
				} catch (const sol::error &e) {
					const std::string msg = std::string("ERROR: DemoLauncher LuaComponent::Init - ") + e.what();
					echo(msg);
					std::ofstream dbg("/tmp/pyros_demolauncher_switch.log", std::ios::app);
					if (dbg) dbg << msg << std::endl;
				} catch (const std::exception &e) {
					const std::string msg = std::string("ERROR: DemoLauncher LuaComponent::Init - ") + e.what();
					echo(msg);
					std::ofstream dbg("/tmp/pyros_demolauncher_switch.log", std::ios::app);
					if (dbg) dbg << msg << std::endl;
				} catch (...) {
					echo("ERROR: DemoLauncher LuaComponent::Init - unknown exception");
					std::ofstream dbg("/tmp/pyros_demolauncher_switch.log", std::ios::app);
					if (dbg) dbg << "ERROR: unknown exception" << std::endl;
				}
			}
		}
	}

	// One Update so components spawned during Init() register and
	// transforms settle before the next draw.
	Scene->Update(GetTime());

	{
		const size_t meshCount = Scene->GetRenderingMeshes().size();
		const size_t lightCount = Scene->GetLights().size();
		const size_t goCount = Scene->GetAllGameObjectList().size();
		const bool hasCamera = lua["camera"].valid() && lua["camera"].get_type() != sol::type::lua_nil;
		const std::string line = std::string("DemoLauncher SwitchDemo '") + entry.name + "': meshes=" + std::to_string(meshCount)
			+ " lights=" + std::to_string(lightCount)
			+ " gos=" + std::to_string(goCount)
			+ " camera=" + (hasCamera ? "ok" : "MISSING");
		echo(line);
		std::ofstream dbg("/tmp/pyros_demolauncher_switch.log", std::ios::app);
		if (dbg) dbg << line << std::endl;
	}
	for (const std::shared_ptr<GameObject> &go : Scene->GetAllGameObjectList())
	{
		if (!go) continue;
		for (const std::shared_ptr<IComponent> &c : go->GetComponents())
		{
			if (!c) continue;
			if (c->GetComponentType() == ComponentType::Physics || c->GetComponentType() == ComponentType::Vehicle)
				activeDemoHasPhysics = true;
		}
	}
}

void DemoLauncher::Update()
{
	FrameProfiler &prof = FrameProfiler::Instance();

	{
		static bool f3WasDown = false;
		const Uint8 *keys = SDL_GetKeyboardState(NULL);
		const bool f3Down = keys && keys[SDL_SCANCODE_F3];
		if (f3Down && !f3WasDown)
			prof.Toggle();
		f3WasDown = f3Down;
	}

	f64 time = GetTime();
	f64 dt = GetTimeInterval();

	if (activeDemoHasPhysics)
		physics->Update(dt, 10);

	Scene->Update(time);

	if (imguiInitialized)
	{
		PYROS_PROFILE_SCOPE("ImGui.Prepare");
		PrepareImGuiFrame();
	}

	{
		PYROS_PROFILE_SCOPE("RenderHost.Draw");
		sol::table host = lua["RenderHost"];
		if (host.valid())
		{
			sol::protected_function draw = host["draw"];
			if (draw.valid())
			{
				sol::protected_function_result r = draw(lua["camera"], lua["scene"], lua["projection"]);
				if (!r.valid())
				{
					sol::error err = r;
					const std::string msg = std::string("ERROR: RenderHost.draw - ") + err.what();
					echo(msg);
					std::ofstream dbg("/tmp/pyros_demolauncher_draw.log", std::ios::app);
					if (dbg) dbg << msg << std::endl;
				}
			}
		}
	}

	if (imguiInitialized)
	{
		PYROS_PROFILE_SCOPE("ImGui.End");
		EndImGuiFrame();
	}
}

void DemoLauncher::DrawUI()
{
	ImGui::Begin("Pyros3D Demos");

	ImGui::Text("FPS: %.1u", (uint32)fps.getFPS());
#if defined(_SDL2METAL)
	ImGui::Text("Backend: Metal");
#else
	ImGui::Text("Backend: %s", GetActiveRenderDevice().IsVulkan() ? "Vulkan" : "OpenGL");
#endif
	ImGui::Text("Mouse: %s (TAB to toggle)", SDL_GetRelativeMouseMode() ? "Captured" : "Free");
	ImGui::Text("Profiler: F3");
	ImGui::Separator();

	FrameProfiler::Instance().DrawImGui();

	for (size_t i = 0; i < demos.size(); i++)
	{
		bool selected = (activeDemo == (int)i);
		if (ImGui::Selectable(demos[i].name.c_str(), selected))
		{
			if (!selected) SwitchDemo((int)i);
		}
		if (ImGui::IsItemHovered() && !demos[i].description.empty())
			ImGui::SetTooltip("%s", demos[i].description.c_str());
	}

	bool drewScriptUI = false;
	auto callMethod = [&](const std::shared_ptr<LuaComponent> &lc, const char *name) {
		if (!lc || !lc->data.valid()) return;
		sol::function f = lc->data[name];
		if (!f.valid()) return;
		if (strcmp(name, "drawUI") == 0 && !drewScriptUI) { ImGui::Separator(); drewScriptUI = true; }
		f(lc->data);
	};
	for (const std::shared_ptr<GameObject> &go : currentAssets.gameObjects)
	{
		for (const std::shared_ptr<IComponent> &c : go->GetComponents())
		{
			if (c && c->GetComponentType() == ComponentType::LuaComponent)
				callMethod(std::static_pointer_cast<LuaComponent>(c), "drawUI");
		}
	}

	ImGui::End();

	for (const std::shared_ptr<GameObject> &go : currentAssets.gameObjects)
	{
		for (const std::shared_ptr<IComponent> &c : go->GetComponents())
		{
			if (c && c->GetComponentType() == ComponentType::LuaComponent)
				callMethod(std::static_pointer_cast<LuaComponent>(c), "drawOverlay");
		}
	}
}

void DemoLauncher::Shutdown()
{
	ShutdownImGui();

	for (const std::shared_ptr<GameObject> &go : currentAssets.gameObjects)
	{
		for (const std::shared_ptr<IComponent> &c : go->GetComponents())
		{
			if (c && c->GetComponentType() == ComponentType::LuaComponent)
				c->Destroy();
		}
	}

	if (!currentAssets.gameObjects.empty())
		SceneSerializer::UnloadScene(Scene, currentAssets);

	{
		sol::table host = lua["RenderHost"];
		if (host.valid())
		{
			sol::protected_function destroy = host["destroy"];
			if (destroy.valid())
			{
				sol::protected_function_result r = destroy();
				if (!r.valid())
				{
					sol::error err = r;
					echo(std::string("ERROR: RenderHost.destroy - ") + err.what());
				}
			}
		}
	}

	lua["renderer"] = sol::lua_nil;
	lua["effectManager"] = sol::lua_nil;
	lua["camera"] = sol::lua_nil;
	lua["scene"] = sol::lua_nil;
	lua["projection"] = sol::lua_nil;
	lua["imgui"] = sol::lua_nil;
	lua["RenderHost"] = sol::lua_nil;
	lua["physics"] = sol::lua_nil;

	lua.script("collectgarbage(); collectgarbage()");
	lua = sol::state{};

	if (physics) { delete physics; physics = nullptr; }
	if (Scene) { delete Scene; Scene = nullptr; }

	ClassName::Shutdown();
}

void DemoLauncher::InitImGui()
{
#if defined(_SDL2METAL)
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	// Do not enable NavEnableKeyboard - ImGui steals Tab for widget
	// focus, which breaks camera_fly.lua's Tab mouse-capture toggle.
	ImGui::StyleColorsDark();

	// SDL2's Metal init path is backend-agnostic - same InitForOther() a
	// bespoke non-GL/Vulkan renderer would use (ImGui_ImplSDL2.h's own
	// comment); it only wires up platform (mouse/keyboard/clipboard), no
	// GL context or Vulkan instance/device required.
	ImGui_ImplSDL2_InitForMetal(GetSDLWindow());
	imguiInitialized = static_cast<MetalRenderDevice&>(GetActiveRenderDevice()).InitImGuiMetalBackend();
#elif !defined(_SDL2VULKAN)
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	// Do not enable NavEnableKeyboard - ImGui steals Tab for widget
	// focus, which breaks camera_fly.lua's Tab mouse-capture toggle.

	ImGui::StyleColorsDark();

	ImGui_ImplSDL2_InitForOpenGL(GetSDLWindow(), GetGLContext());
#if defined(GLES3) || defined(EMSCRIPTEN)
	ImGui_ImplOpenGL3_Init("#version 300 es");
#else
	ImGui_ImplOpenGL3_Init("#version 330");
#endif

	imguiInitialized = true;
#else
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	// Do not enable NavEnableKeyboard - ImGui steals Tab for widget
	// focus, which breaks camera_fly.lua's Tab mouse-capture toggle.
	ImGui::StyleColorsDark();

	ImGui_ImplSDL2_InitForVulkan(GetSDLWindow());
	imguiInitialized = static_cast<VulkanRenderDevice&>(GetActiveRenderDevice()).InitImGuiVulkanBackend();
#endif
}

void DemoLauncher::ShutdownImGui()
{
	if (imguiInitialized)
	{
#if defined(_SDL2VULKAN)
		static_cast<VulkanRenderDevice&>(GetActiveRenderDevice()).ShutdownImGuiVulkanBackend();
#elif defined(_SDL2METAL)
		static_cast<MetalRenderDevice&>(GetActiveRenderDevice()).ShutdownImGuiMetalBackend();
#else
		ImGui_ImplOpenGL3_Shutdown();
#endif
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		imguiInitialized = false;
	}
}

void DemoLauncher::BeginImGuiFrame()
{
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

void DemoLauncher::PrepareImGuiFrame()
{
	if (!imguiInitialized) return;
	BeginImGuiFrame();
	DrawUI();
	ImGui::Render();
}

void DemoLauncher::EndImGuiFrame()
{
#if !defined(_SDL2VULKAN) && !defined(_SDL2METAL)
	GLint last_program, last_texture, last_array_buffer, last_element_array_buffer, last_vertex_array;
	glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	glUseProgram(last_program);
	glBindTexture(GL_TEXTURE_2D, last_texture);
	glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
	glBindVertexArray(last_vertex_array);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif
}
