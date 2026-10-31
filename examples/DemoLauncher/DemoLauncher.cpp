//============================================================================
// Name        : DemoLauncher.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See DemoLauncher.h.
//============================================================================

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#include "DemoLauncher.h"
#include <Pyros3D/Core/Math/Random.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/Rendering/Renderer/SceneView.h>
#include "imgui_internal.h"   // DockBuilder* - see Math.h on why order used to matter

#include <Pyros3D/Core/Logs/Log.h>
#include <SDL2/SDL.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <cstdlib>

using namespace p3d;


DemoLauncher::DemoLauncher() : ClassName(1280, 720, "Pyros3D - Demos", WindowType::Close | WindowType::Resize)
{
	Scene = nullptr;
	physics = nullptr;
	activeDemo = -1;
	showRenderTargets = false;
	renderTargetThumbSize = 200.0f;
	showLog = false;
	logErrorsOnly = false;
	logSeen = 0;
	logAutoOpened = false;
	activeDemoHasPhysics = false;
	imguiInitialized = false;
}

DemoLauncher::~DemoLauncher() {}

void DemoLauncher::OnResize(const uint32 width, const uint32 height)
{
	ClassName::OnResize(width, height);
	// (in the check's log: a demo framed differently from its baseline was often a window resized under it)
	if (std::getenv("PYROS_DEMO_FIXED_CLOCK") != NULL) { fprintf(stdout, "[check] resized to %ux%u\n", width, height); fflush(stdout); }
}

// (PYROS_DEMO_NO_UI: no keyboard, mouse or text input reaches the demo either. The window the check
// opens takes the focus, and what was being typed elsewhere then flew the camera off - the capture
// was of another view, and the demo failed.)
static int SDLCALL DropInputEvents(void*, SDL_Event* e)
{
	switch (e->type)
	{
	case SDL_KEYDOWN: case SDL_KEYUP: case SDL_TEXTINPUT: case SDL_TEXTEDITING:
	case SDL_MOUSEMOTION: case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: case SDL_MOUSEWHEEL:
		return 0;
	default:
		return 1;
	}
}

void DemoLauncher::Init()
{
	ClassName::Init();
	if (std::getenv("PYROS_DEMO_NO_UI") != NULL) SDL_SetEventFilter(DropInputEvents, NULL);
	// (no device on this machine - no Metal, Vulkan driver or WebGPU adapter: said, and out, not a crash)
	if (!GetDevice())
	{
		echo("ERROR: DemoLauncher - nothing to draw with: no graphics device could be made on this machine");
		std::exit(1);
	}
	renderer.reset(new p3d::SceneRenderer(GetDevice()));
	p3d::SceneView::SetMainRenderer(renderer.get());     // (the demos' own second views are drawn beside it)
	echo("DemoLauncher: the renderer on " + GetDevice()->Caps().name);

	Scene = new SceneGraph();

	physics = new Physics();
	physics->InitPhysics();

	GenerateBindings(&lua);
	lua["physics"] = static_cast<IPhysics*>(physics);
	{
		sol::object classMod = lua.require_file("class", STR(EXAMPLES_PATH) "/assets/middleclass.lua");
		lua["class"] = classMod;
	}

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

	// Foreground draw-list primitives, in screen pixels. text()/separator()
	// above only work inside the launcher's own ImGui window, which is the
	// wrong surface for a game HUD; these draw over everything and need no
	// Begin/End, so a demo can lay out a real overlay at absolute
	// coordinates. Colours are 0..1 floats to match the rest of the Lua
	// bindings rather than ImGui's 0..255.
	imgui["displaySize"] = []() {
		ImGuiIO &io = ImGui::GetIO();
		return std::make_tuple(io.DisplaySize.x, io.DisplaySize.y);
	};
	imgui["rectFilled"] = [](float x0, float y0, float x1, float y1,
		float r, float g, float b, float a) {
		ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1),
			IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255)));
	};
	imgui["rect"] = [](float x0, float y0, float x1, float y1,
		float r, float g, float b, float a, float thickness) {
		ImGui::GetForegroundDrawList()->AddRect(ImVec2(x0, y0), ImVec2(x1, y1),
			IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255)),
			0.0f, 0, thickness);
	};
	// Drawn twice, offset by a pixel, because a HUD over a dark station
	// still has to stay readable when it crosses a muzzle flash.
	imgui["textAt"] = [](float x, float y, float r, float g, float b, float a,
		const std::string &s) {
		ImDrawList *dl = ImGui::GetForegroundDrawList();
		dl->AddText(ImVec2(x + 1, y + 1), IM_COL32(0, 0, 0, (int)(a * 160)), s.c_str());
		dl->AddText(ImVec2(x, y),
			IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255)),
			s.c_str());
	};
	imgui["textSize"] = [](const std::string &s) {
		ImVec2 sz = ImGui::CalcTextSize(s.c_str());
		return std::make_tuple(sz.x, sz.y);
	};

	lua["imgui"] = imgui;

	// The demos were tuned before the engine lit in linear light. Linear(r, g, b[, a]): a colour as a demo
	// was tuned with it - the number taken as light - re-encoded so the engine's sRGB decode
	// (ColourSpace.h) gives that number back. LinearFalloffIntensity(intensity, distance, radius): a point
	// or spot light's intensity for the inverse-square falloff (p3d_lighting.glsl's p3d_Falloff) that
	// gives, at `distance`, what the old straight-line falloff gave there.
	lua.set_function("Linear", [](const f32 r, const f32 g, const f32 b, sol::optional<f32> a) {
		auto enc = [](const f32 c) { const f32 v = c < 0.f ? 0.f : c; return v <= 0.0031308f ? v * 12.92f : 1.055f * powf(v, 1.f / 2.4f) - 0.055f; };
		return Vec4(enc(r), enc(g), enc(b), a ? *a : 1.f);
	});
	lua.set_function("LinearFalloffIntensity", [](const f32 intensity, const f32 distance, const f32 radius) {
		const f32 d = distance < 0.1f ? 0.1f : distance;
		const f32 was = radius > 0.f ? std::max(0.f, 1.f - d / radius) : 1.f;
		const f32 x = radius > 0.f ? d / radius : 0.f, w = std::max(0.f, std::min(1.f, 1.f - x * x * x * x));
		const f32 now = w * w / std::max(d * d, 0.01f);
		return now > 0.f ? intensity * was / now : intensity;
	});

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

// What the renderer takes of a demo's render config: the ambient (globalLight, linear), the background, the sky, the
// exposure and the projection
void DemoLauncher::ConfigureRenderer(const DemoEntry &entry)
{
	// The scripts' `renderer` (RendererLua.h)
	p3d::SetScriptRenderer(lua, renderer.get());
	const json &r = entry.render;
	Vec3 ambient(0.03f, 0.03f, 0.035f);
	if (r.contains("globalLight") && r["globalLight"].is_array() && r["globalLight"].size() >= 3)
		ambient = Vec3(r["globalLight"][0].get<f32>(), r["globalLight"][1].get<f32>(), r["globalLight"][2].get<f32>());
	renderer->SetAmbient(ambient);
	Vec3 background(0.f, 0.f, 0.f);
	if (r.contains("background") && r["background"].is_array() && r["background"].size() >= 3)
		background = SrgbToLinear(Vec3(r["background"][0].get<f32>(), r["background"][1].get<f32>(), r["background"][2].get<f32>()));
	renderer->SetBackground(background);
	// (the physical sky: asked for by name - "sky": true, or the old renderer's Sky effect)
	bool sky = r.value("sky", false);
	if (r.contains("effects") && r["effects"].is_array())
		for (const json &e : r["effects"])
			if ((e.is_string() && e.get<std::string>() == "Sky") || (e.is_object() && e.value("name", std::string()) == "Sky")) sky = true;
	renderer->SetSky(sky);
	// (exposure: the config's Output effect - uAuto 0 a fixed exposure of 2^uExposure, otherwise auto exposure
	// compensated by uExposure stops; a demo that says nothing gets auto exposure)
	bool autoExposure = true; f32 ev = 0.f;
	if (r.contains("effects") && r["effects"].is_array())
		for (const json &e : r["effects"])
			if (e.is_object() && e.value("name", std::string()) == "Output" && e.contains("params"))
			{
				autoExposure = e["params"].value("uAuto", 1.f) > 0.5f;
				ev = e["params"].value("uExposure", 0.f);
			}
	renderer->SetAutoExposure(autoExposure, ev);
	// (the lens and shutter, where the demo asks: SunShafts, DepthOfField, MotionBlur - as a scene's effects say them)
	f32 shafts = 0.f, focus = 10.f, range = 4.f, blur = 0.f, shutter = 0.f, sunAngle = 0.0093f;
	bool rayTraced = false, rtReflections = false;
	if (r.contains("effects") && r["effects"].is_array())
		for (const json &e : r["effects"])
		{
			if (!e.is_object()) continue;
			const std::string name = e.value("name", std::string());
			const json p = e.contains("params") ? e["params"] : json::object();
			if (name == "SunShafts") shafts = p.value("uStrength", 0.6f);
			if (name == "DepthOfField") { focus = p.value("uFocus", focus); range = p.value("uRange", range); blur = p.value("uBlur", 8.f); }
			if (name == "MotionBlur") shutter = p.value("uShutter", 0.5f);
			if (name == "RayTracedReflections") rtReflections = true;
			if (name == "RayTracedShadows") { rayTraced = true; sunAngle = p.value("uSunAngle", 0.53f) * 0.017453293f; }
		}
	renderer->SetRayTracedShadows(rayTraced, sunAngle);
	renderer->SetRayTracedReflections(rtReflections);
	renderer->SetSunShafts(shafts); renderer->SetDepthOfField(focus, range, blur); renderer->SetMotionBlur(shutter);
	viewFov = 70.f; viewNear = 0.1f;
	if (r.contains("projection") && r["projection"].is_object())
	{
		viewFov = r["projection"].value("fov", 70.f);
		viewNear = r["projection"].value("near", 0.1f);
	}
}

void DemoLauncher::DrawFrame()
{
	sol::object cam = lua["camera"];
	if (!cam.valid() || !cam.is<GameObject*>()) return;
	GameObject* camera = cam.as<GameObject*>();
	if (!camera) return;
	p3d::View view;
	view.cameraWorld = camera->GetDrawWorld();
	view.fovY = viewFov; view.nearZ = viewNear;
	view.width = Width; view.height = Height;
	// (PYROS_DEMO_ORTHO=<height>: seen orthographically, that many units top to bottom - a test of the renderer's)
	if (const char* ortho = std::getenv("PYROS_DEMO_ORTHO")) { view.orthographic = true; view.orthoHeight = (f32)atof(ortho); view.farZ = 2000.f; }
	// One frame: the scene, then the panels over it (ImGui on the device), then presented
	gfx::Device* device = GetDevice();
	view.hostFrame = imgui != nullptr;
	if (view.hostFrame) device->BeginFrame();
#ifdef LUA_BINDINGS
	// (the scripts' own views first - a second camera into a texture: drawn ahead of this one, which shows it)
	LuaComponent::PreRenderScene(Scene);
#endif
	// (PYROS_DEMO_READBACK=<file.png>: the 120th frame drawn into a texture of its own - View::target, as an editor's
	// viewport is - and read back (Device::ReadTexture) into the file: a test of both)
	static uint32 drawn = 0;
	static const char* readback = std::getenv("PYROS_DEMO_READBACK");
	gfx::Texture offscreen;
	if (readback && view.hostFrame && ++drawn == 120)
	{
		gfx::TextureDesc d; d.width = Width; d.height = Height; d.format = device->BackbufferFormat();
		d.usage = gfx::TexColorTarget | gfx::TexSampled | gfx::TexCopySrc; d.debugName = "DemoLauncher.Readback";
		offscreen = device->CreateTexture(d);
		view.target = offscreen;
	}
	renderer->Render(Scene, view);
	if (offscreen)
	{
		std::vector<uint8_t> px;
		const gfx::Format f = device->BackbufferFormat();
		if (device->ReadTexture(offscreen, px) && px.size() == (size_t)Width * Height * 4)
		{
			if (f == gfx::Format::BGRA8 || f == gfx::Format::BGRA8_sRGB) for (size_t k = 0; k < px.size(); k += 4) std::swap(px[k], px[k + 2]);
			for (size_t k = 3; k < px.size(); k += 4) px[k] = 255;
			gfx::WritePng(readback, px.data(), Width, Height);
			echo(std::string("DemoLauncher: frame read back into ") + readback);
		}
		else echo("DemoLauncher: the read back failed");
		device->Destroy(offscreen);
	}
	if (imgui)
	{
		ImGui_ImplSDL2_NewFrame();
		// (the window's pixels to its points: the backbuffer is the drawable, not the window's size)
		int w = 0, h = 0;
		SDL_GetWindowSize(GetSDLWindow(), &w, &h);
		if (w > 0 && h > 0) ImGui::GetIO().DisplayFramebufferScale = ImVec2((float)Width / (float)w, (float)Height / (float)h);
		ImGui::NewFrame();
		static const bool noUI = std::getenv("PYROS_DEMO_NO_UI") != NULL;
		if (!noUI) DrawUI();
		ImGui::Render();
		gfx::CommandList* cl = device->NewCommandList("ImGui");
		imgui->Render(ImGui::GetDrawData(), cl, gfx::Texture(), false);
		device->Submit(&cl, 1);
		device->Present();
		device->EndFrame();
	}
	// (every 120 frames: what the frame cost - the CPU's whole Render, the GPU's passes)
	static uint32 frames = 0;
	if (++frames % 120 == 0)
	{
		const gfx::FrameStats &s = renderer->Stats();
		char line[256];
		snprintf(line, sizeof(line), "[renderer] frame %u: CPU %.3f ms (+ %.3f waiting for the display), GPU %.3f ms, draws %u, pipelines %u, bind groups %u, skipped %u",
			frames, renderer->CpuMilliseconds(), renderer->WaitMilliseconds(), s.gpuMs, s.draws, s.pipelineChanges, s.bindGroupChanges, s.skippedDraws);
		fprintf(stdout, "%s\n", line);
		std::string passes;
		for (const gfx::FrameStats::Pass &p : s.passes) { char b[96]; snprintf(b, sizeof(b), " %s=%.3f", p.name.c_str(), p.gpuMs); passes += b; }
		fprintf(stdout, "[renderer] passes:%s\n", passes.c_str());
		const p3d::FrameGraph &g = renderer->Graph();
		std::string culled;
		for (const std::string &c : g.culled) culled += " " + c;
		fprintf(stdout, "[renderer] graph: %u passes, %u recorded, %u barriers; culled:%s\n", g.passesAdded, g.passesRecorded, g.barriersMade, culled.empty() ? " none" : culled.c_str());
		fflush(stdout);
	}
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
		// (its look: "globalLight", "background", "sky", "projection", and "effects" for an older entry's Sky / Output)
		entry.render = e.find("render") != e.end() ? e["render"] : json::object();
		if (e.find("effects") != e.end() && entry.render.find("effects") == entry.render.end())
			entry.render["effects"] = e["effects"];
		demos.push_back(entry);
	}
}

void DemoLauncher::SwitchDemo(int index)
{
	if (index < 0 || index >= (int)demos.size()) return;
	renderer->ForgetScene();     // (its caches are keyed on the scene's objects' addresses)

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

	ConfigureRenderer(entry);

	// (under the fixed clock, chance is fixed too: Lua seeds its generator differently every run, and a
	// demo whose lamps flicker at random - Metro - showed another moment of the flicker each time)
	if (std::getenv("PYROS_DEMO_FIXED_CLOCK") != NULL) { lua.script("math.randomseed(1)"); srand(1); Math::Random::SetDefaultSeed(1); }
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
	// PYROS_DEMO_FIXED_CLOCK=<seconds> (what tools/VisualCheck sets): the scene's clock moves 1/60 s a
	// frame, whatever the frame took, and stops at that many seconds - every run of a demo shows the
	// same moment of it, an orbiting camera or a spinning cube included. "[check] frozen" on stdout
	// says the moment has come.
	{
		static const char* fixedEnv = std::getenv("PYROS_DEMO_FIXED_CLOCK");
		static const f64 freezeAfter = fixedEnv ? std::atof(fixedEnv) : 0.0;
		if (freezeAfter > 0.0)
		{
			static f64 start = -1.0, sim = 0.0;
			static bool said = false;
			if (start < 0.0) { start = time; sim = time; }
			dt = (sim - start) < freezeAfter ? 1.0 / 60.0 : 0.0;
			sim += dt;
			time = sim;
			if (dt == 0.0 && !said) { said = true; fprintf(stdout, "[check] frozen\n"); fflush(stdout); }
		}
	}

	if (activeDemoHasPhysics)
		physics->Update(dt, 10);

	Scene->Update(time);
	// The scripts' garbage, within a budget, as the player and the editor collect it. Left to Lua's
	// own collector a demo whose script makes a Vec3 a frame for each water tile (Island, SSR) had
	// a 4 ms frame every 0.6 s - the collector's whole cycle at once.
	{
		PYROS_PROFILE_SCOPE("Demo.LuaGC");
		p3d::LuaCollectWithinBudget(&lua, 1.0);
	}

	{
		PYROS_PROFILE_SCOPE("Demo.Draw");
		DrawFrame();
	}
}


// The engine's log has no idea this exists - it records into a ring buffer
// and this reads it. Keeping the dependency pointing this way is what lets
// the same messages also reach a console, a file, or an editor panel later,
// and is why anything logged during device or script init is still here to
// be read once the UI finally comes up.
void DemoLauncher::DrawLogWindow()
{
	if (!showLog)
	{
		// Not open: just remember where we are, so reopening does not claim
		// everything is new.
		logSeen = p3d::LOG::_LOG::MessageCount();
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(760, 260), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Log", &showLog))
	{
		if (ImGui::Button("Clear")) { p3d::LOG::_LOG::ClearMessages(); logSeen = 0; }
		ImGui::SameLine();
		ImGui::Checkbox("Errors only", &logErrorsOnly);
		ImGui::SameLine();
		// Raising this is retroactive only for what arrives next - anything
		// already dropped was never recorded. Errors and warnings are always
		// kept, which is the point of the default.
		{
			int level = p3d::LOG::_LOG::GetLevel();
			ImGui::SetNextItemWidth(150.0f);
			if (ImGui::Combo("Level", &level, "Errors\0+ Warnings\0+ Success\0Everything\0"))
				p3d::LOG::_LOG::SetLevel(level);
		}
		ImGui::SameLine();
		ImGui::Text("%u line(s)", p3d::LOG::_LOG::MessageCount());
		ImGui::Separator();

		ImGui::BeginChild("loglines", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
		const unsigned int count = p3d::LOG::_LOG::MessageCount();
		for (unsigned int i = 0; i < count; i++)
		{
			const p3d::LOG::Entry &e = p3d::LOG::_LOG::MessageAt(i);
			if (logErrorsOnly && !e.error) continue;
			if (e.error)
				ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", e.text.c_str());
			else
				ImGui::TextUnformatted(e.text.c_str());
		}
		// Only stick to the bottom while the user is already there, so
		// scrolling back to read something is not fought by new messages.
		if (count != logSeen && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);
		ImGui::EndChild();
		logSeen = count;
	}
	ImGui::End();
}



// Reads `src` back on the CPU and keeps a small RGBA8 copy ImGui can draw.
// This is the fallback for everything GetImGuiTextureID() cannot hand over
// directly - depth, and cube faces.
//
// Deliberately throttled and only called for sections the user has expanded.
// A 2048x2048 shadow cube is 16MB of floats per face, and reading one back
// stalls the pipeline; at a few frames a second on a panel you opened on
// purpose that is a fair trade, every frame would not be.
//
// Depth is normalised between the min and max actually present rather than
// shown raw. A raw depth buffer is almost entirely values near 1.0 and reads
// as a white rectangle - technically correct and completely useless, which
// The renderer's own targets, as pictures (SceneRenderer::DebugTargets: the HDR picture, the AO, the motion, the bloom,
// the sky's tables, the clouds, the probes' atlas)
void DemoLauncher::DrawRenderTargetViewer()
{
	if (!showRenderTargets) return;
	ImGui::SetNextWindowSize(ImVec2(560, 620), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Render Targets", &showRenderTargets))
	{
		ImGui::SliderFloat("Size", &renderTargetThumbSize, 64.f, 512.f, "%.0f px");
		const std::vector<p3d::SceneRenderer::DebugTarget> targets = renderer->DebugTargets();
		for (size_t k = 0; k < targets.size(); k++)
		{
			const p3d::SceneRenderer::DebugTarget &t = targets[k];
			ImGui::Text("%s  %ux%u", t.name, t.width, t.height);
			const float aspect = t.height ? (float)t.width / (float)t.height : 1.f;
			ImGui::Image((ImTextureID)p3d::ImGuiRenderer::Id(t.texture), ImVec2(renderTargetThumbSize * aspect, renderTargetThumbSize));
		}
	}
	ImGui::End();
}

void DemoLauncher::DrawUI()
{
	// PassthruCentralNode: the centre of the dockspace stays transparent and
	// does not eat mouse input, so the 3D scene drawn underneath is still
	// visible and still clickable. Without it the dockspace host window
	// would cover the whole viewport with a solid background.
	const ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(0, NULL, ImGuiDockNodeFlags_PassthruCentralNode);

	if (buildDefaultDockLayout)
	{
		buildDefaultDockLayout = false;
		ImGui::DockBuilderRemoveNode(dockspaceId);
		ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

		// Each split takes a fraction of what is left, so `centre` shrinks as
		// we go and whatever remains is the passthru region the scene shows
		// through.
		ImGuiID centre = dockspaceId;
#if TARGET_OS_IPHONE
		// (a phone's screen is narrow: the list wide enough for its names)
		const ImGuiID left = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left, 0.30f, NULL, &centre);
#else
		const ImGuiID left = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left, 0.22f, NULL, &centre);
#endif
		const ImGuiID bottom = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, 0.30f, NULL, &centre);
		const ImGuiID right = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, 0.26f, NULL, &centre);

		ImGui::DockBuilderDockWindow("Pyros3D Demos", left);
		// Same node for both, so they share one tab bar along the bottom
		// rather than each taking half of it - both want the full width.
		ImGui::DockBuilderDockWindow("Profiler", bottom);
		ImGui::DockBuilderDockWindow("Log", bottom);
		ImGui::DockBuilderDockWindow("Point Shadow Cubemap", right);
		ImGui::DockBuilderFinish(dockspaceId);
	}

	ImGui::Begin("Pyros3D Demos");

	ImGui::Text("FPS: %.1u", (uint32)fps.getFPS());
	ImGui::Text("Backend: %s", GetDevice()->Caps().name.c_str());
	ImGui::Text("Mouse: %s (TAB to toggle)", SDL_GetRelativeMouseMode() ? "Captured" : "Free");
#if TARGET_OS_IPHONE
	ImGui::Checkbox("Profiler", &FrameProfiler::Instance().WindowOpen());
#else
	ImGui::Text("Profiler: F3");
#endif
	{
		const unsigned int total = p3d::LOG::_LOG::MessageCount();
		unsigned int errors = 0;
		for (unsigned int i = 0; i < total; i++)
			if (p3d::LOG::_LOG::MessageAt(i).error) errors++;
		if (errors > 0)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%u error(s) logged", errors);
			if (!logAutoOpened) { logAutoOpened = true; showLog = true; }
		}
		ImGui::Checkbox("Log", &showLog);
		ImGui::Checkbox("Render targets", &showRenderTargets);
	}
	ImGui::Separator();

#if TARGET_OS_IPHONE
	FrameProfiler::Instance().DrawImGui(&FrameProfiler::Instance().WindowOpen());   // (shut until its box is ticked)
#else
	FrameProfiler::Instance().DrawImGui();
#endif
	DrawLogWindow();
	DrawRenderTargetViewer();

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

	lua["renderer"] = sol::lua_nil;
	lua["effectManager"] = sol::lua_nil;
	lua["camera"] = sol::lua_nil;
	lua["scene"] = sol::lua_nil;
	lua["projection"] = sol::lua_nil;
	lua["imgui"] = sol::lua_nil;
	lua["physics"] = sol::lua_nil;

	lua.script("collectgarbage(); collectgarbage()");
	lua = sol::state{};

	if (physics) { delete physics; physics = nullptr; }
	if (Scene) { delete Scene; Scene = nullptr; }

	renderer.reset();
	ClassName::Shutdown();
}

void DemoLauncher::InitImGui()
{
	// (the platform's side - mouse, keys, clipboard - is SDL's; the drawing the device's: ImGuiRenderer)
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	// Docking only: ViewportsEnable would want multi-viewport support from the platform and renderer backends.
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// (the default arrangement only when there is no saved one: a layout dragged into shape survives a restart)
	buildDefaultDockLayout = !std::ifstream(io.IniFilename ? io.IniFilename : "imgui.ini").good();
	// (no NavEnableKeyboard: ImGui would take Tab for widget focus, which camera_fly.lua's mouse capture is)
	ImGui::StyleColorsDark();
#if TARGET_OS_IPHONE
	// A phone: no layout saved from a desktop (the bundle holds one, and cannot be written to), and the profiler
	// shut until asked for - its F3 is a key a phone has not got.
	io.IniFilename = NULL;
	buildDefaultDockLayout = true;
	FrameProfiler::Instance().WindowOpen() = false;
#endif
	ImGui_ImplSDL2_InitForOther(GetSDLWindow());
	imgui.reset(new p3d::ImGuiRenderer(GetDevice()));
	imguiInitialized = true;
}

void DemoLauncher::ShutdownImGui()
{
	if (!imgui) return;
	imgui.reset();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	imguiInitialized = false;
}
