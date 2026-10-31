#include <sys/stat.h>
#include <Pyros3D/Core/InputManager/TouchControls.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/Rendering/GI/ReflectionProbes.h>
#include <Pyros3D/Utils/Localization/Localization.h>
#include <cstring>
#include <Pyros3D/Rendering/GI/EnvironmentMap.h>
#include <Pyros3D/Utils/View/ViewFovScale.h>
#include <vector>
//============================================================================
// Name        : PyrosPlayer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See PyrosPlayer.h.
//============================================================================

#include <thread>
#include <chrono>
#include <Pyros3D/Rendering/Terrain/TerrainHorizon.h>
#include <Pyros3D/Rendering/Terrain/TerrainOcclusion.h>
#include <Pyros3D/Assets/Renderable/Terrains/TerrainEditor.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <Pyros3D/Assets/AssetPreload.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include "PyrosPlayer.h"
#include <Pyros3D/Rendering/ColourSpace.h>
#include <functional>
#include <Pyros3D/Rendering/Components/Terrain/TerrainComponent.h>
#include <Pyros3D/Utils/Streaming/AssetStreamer.h>
#include <cstdio>
#include <cstdlib>
#include <Pyros3D/Rendering/Components/Foliage/Foliage.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaNetwork.h>
#include "../../examples/WindowManagers/TextInputHook.h"
#include "UIDispatch.h"

#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Audio/AudioManager.h>
#include <Pyros3D/Audio/AudioSource.h>
#include <Pyros3D/Rendering/Components/Particles/ParticleSystem.h>
#include <Pyros3D/Physics/PhysicsEngines/Box3D/Box3DPhysics.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include "PrefabResolver.h"
// UI style/palette files, resolved by the same header the editor uses
// (shared/UIStyleResolver.h) so a themed UI looks identical in both.
#include "UIStyleResolver.h"
#include "QualitySettings.h"

#include <SDL2/SDL.h>
#include <Pyros3D/Utils/LoadProgress/LoadProgress.h>
#include <Pyros3D/Utils/CrashHandler/CrashHandler.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <Pyros3D/Rendering/Components/Layer2D/Layer2D.h>
#include <Pyros3D/Rendering/Components/Occluder2D/Occluder2D.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaView2D.h>
#ifdef LUA_BINDINGS
#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>

// PYROS_BENCH_CAMERA=x,z,yaw,pitch (metres, degrees): the active camera held there, 1.7 m over the ground,
// looking that way, every frame - and the world streamed, the terrain and the foliage thinned, round it.
// A benchmark's view the same from run to run (a match is not: where its player lands is not).
// x,z,yaw,pitch,speed,turn: walked along its heading at speed m/s, turning turn degrees a second, from
// the scene time it started at - a moving benchmark (streaming, levels of detail, lists rebuilt), the
// same path every run.
static void HoldBenchCamera(p3d::GameObject* camera, p3d::SceneGraph* scene)
{
	using namespace p3d;
	static const char* spec = std::getenv("PYROS_BENCH_CAMERA");
	if (spec == NULL || camera == NULL) return;
	f32 x = 0.f, z = 0.f, yaw = 0.f, pitch = 0.f, speed = 0.f, turn = 0.f;
	if (std::sscanf(spec, "%f,%f,%f,%f,%f,%f", &x, &z, &yaw, &pitch, &speed, &turn) < 2) return;
	if (speed != 0.f || turn != 0.f)
	{
		static f64 start = -1.0;
		const f64 now = scene->GetUpdateTime();
		if (start < 0.0) start = now;
		const f32 t = (f32)(now - start);
		// the way it looks is -(sin yaw, cos yaw); turning r radians a second at v, an arc of radius v / r
		const f32 r = turn * (f32)M_PI / 180.f, y0 = yaw * (f32)M_PI / 180.f;
		if (fabsf(r) < 1e-5f) { x -= sinf(y0) * speed * t; z -= cosf(y0) * speed * t; }
		else
		{
			x -= speed / r * (cosf(y0) - cosf(y0 + r * t));
			z -= speed / r * (sinf(y0 + r * t) - sinf(y0));
		}
		yaw += turn * t;
	}
	f32 ground = 0.f;
	TerrainEditor::HeightAt(scene, x, z, ground);
	const f32 cy = cosf(yaw * (f32)M_PI / 180.f), sy = sinf(yaw * (f32)M_PI / 180.f);
	const f32 cp = cosf(pitch * (f32)M_PI / 180.f), sp = sinf(pitch * (f32)M_PI / 180.f);
	Matrix world;
	world.identity();
	// (yaw about y, then pitch about x: columns of Ry * Rx)
	world.m[0] = cy;      world.m[1] = 0.f; world.m[2] = -sy;
	world.m[4] = sy * sp; world.m[5] = cp;  world.m[6] = cy * sp;
	world.m[8] = sy * cp; world.m[9] = -sp; world.m[10] = cy * cp;
	world.m[12] = x; world.m[13] = ground + 1.7f; world.m[14] = z;
	GameObject* parent = camera->GetParent();
	camera->SetTransformationMatrix(parent ? parent->GetWorldTransformation().Inverse() * world : world);
	camera->RefreshTransformation();
}
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================== manifest ===============================

namespace {

	// Where the game lives. The manifest sits next to the executable in a
	// built game, but running the player from a project folder during
	// development is useful enough to be worth supporting - so the current
	// directory is tried first, and the executable's own directory second.
	std::string FindManifestDir()
	{
		std::error_code ec;
		if (fs::exists("game.json", ec)) return ".";

		// SDL knows where the binary is even when the working directory is
		// somewhere else entirely (double-clicked .app, shortcut, launcher).
		char* base = SDL_GetBasePath();
		if (base)
		{
			const std::string dir(base);
			SDL_free(base);
			if (fs::exists(fs::path(dir) / "game.json", ec)) return dir;
			// macOS .app bundles put the binary in Contents/MacOS/ and the
			// game data in Contents/Resources/.
			const fs::path resources = fs::path(dir) / ".." / "Resources";
			if (fs::exists(resources / "game.json", ec))
				return fs::weakly_canonical(resources, ec).string();
		}
		return std::string();
	}

	PlayerManifest LoadManifest()
	{
		PlayerManifest m;
		m.root = FindManifestDir();
		if (m.root.empty())
		{
			echo("ERROR: game.json not found - the player must run from a built game folder");
			return m;
		}

		// Scene files reference their assets project-relative - relative to
		// the *working directory*, not to the executable. Making the game
		// folder the working directory is what lets them resolve when the
		// game is launched from anywhere (Finder, a shortcut, a launcher),
		// which is every way a player will actually start it.
		std::error_code chdirEc;
		m.root = fs::weakly_canonical(fs::path(m.root), chdirEc).string();
		fs::current_path(m.root, chdirEc);

		std::ifstream in((fs::path(m.root) / "game.json").string().c_str());
		json j;
		try { in >> j; }
		catch (const std::exception& e)
		{
			echo(std::string("ERROR: game.json is not valid JSON - ") + e.what());
			return m;
		}

		m.title = j.value("title", m.title);
		m.startupScene = j.value("startupScene", std::string());
		m.touchControls = j.value("touchControls", std::string());
		m.serverPublicKey = j.value("serverPublicKey", std::string());
		m.width = j.value("width", m.width);
		m.height = j.value("height", m.height);
		m.fullscreen = j.value("fullscreen", false);
		m.language = j.value("language", std::string("auto"));
		m.devConsole = j.value("devConsole", false) || getenv("PYROS_CONSOLE") != NULL;
		m.fallbackLanguage = j.value("fallbackLanguage", std::string("en"));
		if (j.contains("quality") && j["quality"].is_object())
		{
			const json &q = j["quality"];
			m.quality = q;
			// "frameRateLimit": no more frames a second than that (left out, -1 or 0: as the display refreshes)
			m.qFrameLimit = q.value("frameRateLimit", -2.f);
		}
		if (j.contains("preload") && j["preload"].is_array())
			for (const auto &e : j["preload"]) if (e.is_string()) m.preload.push_back(e.get<std::string>());
		if (j.contains("background") && j["background"].is_array() && j["background"].size() >= 3)
			m.background = Vec4(j["background"][0].get<f32>(), j["background"][1].get<f32>(),
				j["background"][2].get<f32>(), j["background"].size() > 3 ? j["background"][3].get<f32>() : 1.f);
		m.loadingLook.title = m.title;
		if (j.contains("loadingScreen"))
		{
			const json &ls = j["loadingScreen"];
			if (ls.is_boolean()) m.loadingScreen = ls.get<bool>();
			else if (ls.is_object())
			{
				const auto colour = [&ls](const char* key, Vec4 &out) {
					if (ls.contains(key) && ls[key].is_array() && ls[key].size() >= 3)
						out = Vec4(ls[key][0].get<f32>(), ls[key][1].get<f32>(), ls[key][2].get<f32>(), ls[key].size() > 3 ? ls[key][3].get<f32>() : 1.f);
				};
				m.loadingScreen = ls.value("enabled", true);
				m.loadingScene = ls.value("scene", std::string());
				m.loadingLook.title = ls.value("title", m.loadingLook.title);
				m.loadingLook.image = ls.value("image", std::string());
				m.loadingLook.showPercent = ls.value("percent", true);
				colour("background", m.loadingLook.background);
				colour("accent", m.loadingLook.accent);
				colour("text", m.loadingLook.text);
			}
		}

		if (m.startupScene.empty())
		{
			echo("ERROR: game.json has no \"startupScene\"");
			return m;
		}
		m.valid = true;
		return m;
	}

} // namespace

const PlayerManifest& PlayerManifestInstance()
{
	// Function-local static, not a global: this is read from the
	// constructor's initialiser list (the window's size and title come from
	// it), and a global would be racing static init order to get there.
	static PlayerManifest manifest = LoadManifest();
	return manifest;
}

// ================================ player ================================

void PyrosPlayer::SetPointerCaptured(const bool captured)
{
	mouseCaptured = captured;
	if (captured)
	{
		SDL_WarpMouseInWindow(GetSDLWindow(), (int)(Width / 2), (int)(Height / 2));
		SDL_SetRelativeMouseMode(SDL_TRUE);
		SDL_ShowCursor(SDL_DISABLE);
	}
	else
	{
		SDL_SetRelativeMouseMode(SDL_FALSE);
		SDL_ShowCursor(SDL_ENABLE);
	}
	// (with the on-screen controls live, a finger is theirs - not also the pointer, which SDL would
	// otherwise move with the first finger down; in a menu it is the pointer again, a tap a click)
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, (captured && TouchControls::Instance().IsEnabled()) ? "0" : "1");
}

void PyrosPlayer::SetLaunchArgs(int argc, char** argv)
{
	for (int i = 1; i < argc; i++)
	{
		const std::string a = argv[i];
		const bool hasValue = i + 1 < argc && argv[i + 1][0] != '-';
		if (a == "--scene" && hasValue) launchScene = argv[++i];
		else if (a == "--connect" && hasValue) launchConnect = argv[++i];
		else if (a == "--host") launchHostPort = hasValue ? std::atoi(argv[++i]) : 47400;
		else if (a == "--password" && hasValue) launchPassword = argv[++i];
		else if (a == "--server-key" && hasValue) launchServerKey = argv[++i];
		else if (a == "--rendezvous" && hasValue) launchRendezvous = argv[++i];
		else if (a == "--session" && hasValue) launchSession = argv[++i];
		else if (a == "--fill-shader-cache" && hasValue) fillShaderCacheDir = argv[++i];
	}
}

PyrosPlayer::PyrosPlayer()
	: ClassName(PlayerManifestInstance().width, PlayerManifestInstance().height,
		PlayerManifestInstance().title,
		// (a game that starts full screen takes the desktop's own size, the way
		// Alt+Enter and setFullscreen do - below. An exclusive mode at the
		// window's size was asked for here, and on macOS that came out as a
		// plain window of that size.)
		WindowType::Close | WindowType::Resize)
{

	scene = NULL;
	overlayScene = NULL;
	physics2D = new Physics2DWorld();
	pendingOverlayHide = false;
	sceneIs2D = false;
	physics = NULL;
	wheelDelta = 0.f;
	audio = NULL;
	activeCamera = NULL;
	resizePending = false;
	pendingResizeWidth = pendingResizeHeight = 0;
	cameraFov = 70.f;
	cameraNear = 0.1f;
	cameraFar = 2000.f;
	cameraOrthographic = false;
	cameraOrthoSize = 10.f;
	sceneLoaded = false;
}

PyrosPlayer::~PyrosPlayer() {}

std::string PyrosPlayer::ResolvePath(const std::string& relative) const
{
	const PlayerManifest& m = PlayerManifestInstance();
	if (relative.empty()) return relative;
	if (fs::path(relative).is_absolute()) return relative;
	return (fs::path(m.root) / relative).string();
}

PyrosPlayer* PyrosPlayer::activePlayer = NULL;

void PyrosPlayer::Init()
{
	ClassName::Init();

	// Wheel notches and typed characters both arrive as events rather than
	// as state that can be polled, so they are collected as they come.
	activePlayer = this;
	InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Wheel, this, &PyrosPlayer::OnMouseWheel);
	PyrosTextInput::SetHandler(&PyrosPlayer::OnTextTyped);

	const PlayerManifest& m = PlayerManifestInstance();
	if (!m.valid)
	{
		// Nothing to run. Closing immediately beats a black window that
		// looks like a hung game - the log above says why.
		Close();
		return;
	}

	scene = new SceneGraph();

	physics = new Physics();
	physics->InitPhysics();
	// The editor leaves simulation off while editing and turns it on when
	// entering play mode; a player is only ever "in play mode".
	static_cast<Box3DPhysics*>(static_cast<IPhysics*>(physics))->SetSimulationEnabled(true);

	// Audio has to exist before the scene loads: AudioSource::EnsureLoaded()
	// checks for an active AudioManager and quietly loads nothing without
	// one, so a game built with sound would come up silent.
	audio = new AudioManager();
	if (audio->IsInitialized())
		AudioManager::MakeActive(audio);
	else
		echo("WARNING: no audio device - the game will run silent");
	// What stands between a sound and the ears (for sounds with occlusion on): a ray through the
	// world, a hit near the sound itself not counted - a car's own body around its engine.
	audio->SetOcclusionTest([this](const Vec3 &from, const Vec3 &to) -> f32 {
		if (physics == NULL) return 0.f;
		const f32 length = (to - from).magnitude();
		if (length < 3.f) return 0.f;
		const RayCastHit h = physics->RayCast(from, to);
		return (h.hasHit && h.distance < length - 2.5f) ? 1.f : 0.f;
	});

	// The renderer, on the window's device: Metal, Vulkan or WebGPU (none on this machine: said, and out, not a crash)
	if (!GetDevice())
	{
		echo("ERROR: PyrosPlayer - nothing to draw with: no graphics device could be made on this machine");
		std::exit(1);
	}
	renderer.reset(new p3d::SceneRenderer(GetDevice()));
	// (the game's surface cache, where its folder has one: a native run writes the materials' surfaces there, SPIR-V
	// and WGSL, and the browser build - which compiles none - reads them)
	{
		struct stat st;
		if (stat("surfaces", &st) == 0 && (st.st_mode & S_IFDIR)) renderer->SetSurfaceCache("surfaces");
	}
	renderer->SetBackground(SrgbToLinear(Vec3(m.background.x, m.background.y, m.background.z)));
	echo("Renderer on " + GetDevice()->Caps().name);

#ifdef LUA_BINDINGS
	GenerateBindings(&lua);
	p3d::SetScriptRenderer(lua, renderer.get());
	// `network`: the session belongs to the loaded scene and is made the
	// first time a script hosts or joins.
	RegisterLuaNetwork(&lua, [this]() -> NetworkSession* {
		if (!network && sceneLoaded)
			network.reset(new NetworkSession(scene, ResolvePath(currentSceneRel), physics, &lua));
		return network.get();
	});
	// Behaviour scripts call class('Name') as a global. require_file caches
	// the module but does not set _G.class, so the assignment matters - same
	// as Editor::InitLuaHost and DemoLauncher.
	try
	{
		sol::object classMod = lua.require_file("class", ResolvePath("lua/middleclass.lua"));
		lua["class"] = classMod;
	}
	catch (const std::exception& e)
	{
		echo(std::string("ERROR: could not load lua/middleclass.lua - scripts will not run: ") + e.what());
	}

	// print() goes to the engine log, which on a player build is the
	// console/stdout - a shipped game has no log panel to route it to.
	lua.set_function("__pyros_log", [](const std::string& msg) { echo(msg); });
	lua.script(R"LUA(
function print(...)
	local n = select("#", ...)
	local parts = {}
	for i = 1, n do parts[i] = tostring(select(i, ...)) end
	__pyros_log(table.concat(parts, "\t"))
end
)LUA");

	// The mouse-capture trio every first-person scene script expects. Same
	// names and behaviour as DemoLauncher's, minus the ImGui bookkeeping -
	// there is no ImGui in a player build to keep in sync.
	lua.set_function("setMouseCaptured", [this](bool captured) { SetPointerCaptured(captured); });
	lua.set_function("isMouseCaptured", [this]() { return mouseCaptured; });
	// The on-screen controls of a touch screen (TouchControls): a game's options can offer to move them.
	//   touchControls.edit(true)    drag them about, pinch to size; Done keeps it
	//   touchControls.reset()       the game's layout again
	//   touchControls.setEnabled(b) / isEnabled() / isEditing()
	//   touchControls.load(path)    a layout of the game's (project-relative), in place of the one shown
	//   touchControls.layout()      the layout shown, as JSON text; setLayout(text) another
	{
		sol::table touch = lua.create_named_table("touchControls");
		touch.set_function("edit", [](const bool on) { TouchControls::Instance().SetEditing(on); });
		touch.set_function("isEditing", []() { return TouchControls::Instance().IsEditing(); });
		touch.set_function("reset", []() { TouchControls::Instance().ResetLayout(); });
		touch.set_function("setEnabled", [](const bool on) { TouchControls::Instance().SetEnabled(on); });
		touch.set_function("isEnabled", []() { return TouchControls::Instance().IsEnabled(); });
		touch.set_function("load", [](const std::string &path) { return TouchControls::Instance().LoadLayoutFile(path); });
		touch.set_function("layout", []() { return TouchControls::Instance().LayoutText(); });
		touch.set_function("setLayout", [](const std::string &text) { return TouchControls::Instance().LoadLayoutText(text); });
	}
	// How far the mouse moved since this was last asked (x, y in device counts), for a
	// camera that holds the pointer: ask once a frame. Asked just after capturing, the
	// first answer holds the jump to the middle - throw it away.
	lua.set_function("getMouseDelta", []() { const Vec2 d = InputManager::ConsumeMouseDelta(); return std::make_tuple(d.x, d.y); });
	// isKeyDown(Key.W): whether the key is held at this moment
	lua.set_function("isKeyDown", [](const uint32 key) { return InputManager::IsKeyDown(key); });
	lua.set_function("warpMouseToCenter", [this]() {
		SDL_WarpMouseInWindow(GetSDLWindow(), (int)(Width / 2), (int)(Height / 2));
	});
	lua.set_function("getWindowSize", [this]() { return std::make_tuple((int)Width, (int)Height); });
	// setTerrainOcclusion(false): what is behind a hill is drawn again (to compare)
	lua.set_function("setTerrainOcclusion", [](const bool on) { TerrainOcclusion::SetEnabled(on); });
	lua.set_function("getTerrainOcclusion", []() { return TerrainOcclusion::GetEnabled(); });
	// setAutoLOD(false): models are drawn whole however far off (those loaded already included)
	lua.set_function("setAutoLOD", [](const bool on) { RenderingComponent::SetAutoLODInUse(on); });
	lua.set_function("setParallelAnimation", [](const bool on) { RenderingComponent::SetParallelAnimation(on); });
	// setFoliageThinning(false): every plant of a field drawn, however far (see FoliageLayerSpec::thinDensity)
	lua.set_function("setFoliageThinning", [](const bool on) { FoliageComponent::SetThinning(on); });
	// Quality for the machine the game is on - scenes are authored at their best:
	// setLODScale(k): every level of detail reaches k times as far; setFoliageQuality(reach, amount):
	// how far grass and the like are drawn (a multiple of each layer's distances) and how much of it.
	lua.set_function("setLODScale", [](const f32 k) { RenderingComponent::SetLODDistanceScale(k); });
	lua.set_function("setFoliageQuality", [](const f32 reach, const f32 amount) { FoliageComponent::SetQuality(reach, amount); });
	lua.set_function("getFoliageThinning", []() { return FoliageComponent::GetThinning(); });
	// setFrameRateLimit(fps): no more frames a second than that (0 or -1: as many as the display shows - the device
	// presents on its refresh). getDisplayRefreshRate(): what the screen the window is on can show, or 0 when it will
	// not say.
	lua.set_function("setFrameRateLimit", [this](const f32 fps) { frameRateLimit = fps < 0.f ? 0.f : fps; });
	lua.set_function("getDisplayRefreshRate", [this]() {
		SDL_DisplayMode mode;
		const int display = GetSDLWindow() ? SDL_GetWindowDisplayIndex(GetSDLWindow()) : 0;
		if (SDL_GetCurrentDisplayMode(display < 0 ? 0 : display, &mode) != 0) return 0;
		return mode.refresh_rate;
	});
	lua.set_function("getRenderSize", [this]() { return std::make_tuple((int)Width, (int)Height); });
	lua.set_function("quitGame", [this]() { Close(); });
	// The whole screen, or a window: the desktop's own resolution, so nothing about the
	// display changes and Alt+Tab is instant. Alt+Enter and F11 do the same from the
	// keyboard in any game (Update()).
	lua.set_function("setFullscreen", [this](bool on) {
		return SDL_SetWindowFullscreen(GetSDLWindow(), on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) == 0;
	});
	lua.set_function("isFullscreen", [this]() {
		return (SDL_GetWindowFlags(GetSDLWindow()) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
	});
	// The developer console runs what it does not know as Lua, in the game's own state: an
	// expression's value is shown (2 + 2 -> 4), a statement is run.
	console.SetExecutor([this](const std::string &text) -> std::string {
		sol::protected_function_result r = lua.safe_script("return " + text, sol::script_pass_on_error);
		if (!r.valid()) r = lua.safe_script(text, sol::script_pass_on_error);
		if (!r.valid()) { sol::error e = r; return std::string("ERROR: ") + e.what(); }
		std::string out;
		sol::protected_function tostr = lua["tostring"];
		for (int i = 0; i < r.return_count(); i++)
		{
			sol::object o = r.get<sol::object>(i);
			std::string piece = "nil";
			if (tostr.valid()) { sol::protected_function_result t = tostr(o); if (t.valid()) piece = t.get<std::string>(); }
			out += (i ? "\t" : "") + piece;
		}
		return out;
	});
	// Scripts can drive it too (a test, an automated check): devconsole.open() / run(line) / close().
	{
		sol::table dc = lua.create_named_table("devconsole");
		dc.set_function("open", [this]() { console.Open((uint32)Width, (uint32)Height); });
		dc.set_function("close", [this]() { console.Close(); });
		dc.set_function("isOpen", [this]() { return console.IsOpen(); });
		dc.set_function("run", [this](const std::string &text) { if (!console.IsOpen()) return; console.Type(text); console.Press(DevConsole::Enter); });
	}
	console.AddCommand("quit", "close the game", [this](const std::string &) { Close(); return std::string("bye"); });
	console.AddCommand("fps", "frames a second, now", [this](const std::string &) {
		char b[64]; snprintf(b, sizeof(b), "%.1f fps", GetTimeInterval() > 0.0 ? 1.0 / GetTimeInterval() : 0.0); return std::string(b); });
	// The player's language, before anything says anything (Localization; the tables are
	// assets/strings/<code>.json beside the game).
	{
		const PlayerManifest &pm = PlayerManifestInstance();
		Localization::SetDirectory("assets/strings");
		Localization::SetFallback(pm.fallbackLanguage);
		std::string want = pm.language;
		if (want.empty() || want == "auto")
		{
			std::vector<std::string> preferred;
#if SDL_VERSION_ATLEAST(2, 0, 14)
			if (SDL_Locale* locales = SDL_GetPreferredLocales())
			{
				for (SDL_Locale* l = locales; l->language != NULL; l++)
					preferred.push_back(l->country ? std::string(l->language) + "-" + l->country : std::string(l->language));
				SDL_free(locales);
			}
#endif
			want = Localization::PickLanguage(preferred);
		}
		if (!Localization::SetLanguage(want) && !Localization::GetLanguages().empty())
			echo("WARNING: no strings for language '" + want + "' - using " + Localization::GetFallback());
	}
	// Settings that outlive the game: a folder of this player's own, per game.
	{
		const std::string title = PlayerManifestInstance().title.empty() ? std::string("Game") : PlayerManifestInstance().title;
		char* pref = SDL_GetPrefPath("Pyros3D", title.c_str());
		GenerateStoreBindings(&lua, pref ? std::string(pref) : std::string("."));
		// The on-screen controls' layout: the player's own (kept beside the settings) over the game's.
		TouchControls &touch = TouchControls::Instance();
		touch.SetUserLayoutFile((pref ? std::string(pref) : std::string("./")) + "touch_layout.json");
		if (pref) SDL_free(pref);
		if (!PlayerManifestInstance().touchControls.empty()) touch.SetGameLayoutFile((fs::path(PlayerManifestInstance().root) / PlayerManifestInstance().touchControls).string());
		touch.LoadPreferredLayout();
		TouchControls::Emitter e;
		e.keyDown = [this](const uint32 k) { SetKeyPressed(k); };
		e.keyUp = [this](const uint32 k) { SetKeyReleased(k); };
		e.mouseDown = [this](const uint32 b) { SetMouseButtonPressed(b); };
		e.mouseUp = [this](const uint32 b) { SetMouseButtonReleased(b); };
		// (a look drag as a captured mouse moves: the raw amount, and the pointer that far from the
		// middle - where a game that warps it back each move expects it)
		e.look = [this](const f32 dx, const f32 dy) {
			InputManager::AddMouseDelta(dx, dy);
			SetMouseMove((f32)(Width / 2) + dx, (f32)(Height / 2) + dy);
		};
		touch.SetEmitter(e);
	}

	// Runtime spawning. Registered here rather than in the engine's
	// bindings because a prefab is a tooling concept - what the engine
	// offers is DeserializeSubtree(), and this is that pointed at a
	// .prefab file:
	//
	//   local e = Prefab.instantiate("assets/prefabs/Enemy.prefab")
	//   e:setPosition(Vec3.new(x, 0, z))
	//   scene:add(e)
	//
	// The returned object is not in the scene yet (same contract as
	// GameObject.new()) - keep it and scene:add() it, or it is collected.
	{
		sol::table prefabTable = lua.create_table();
		PyrosPlayer* self = this;
		prefabTable["instantiate"] = [self](const std::string& path) -> std::shared_ptr<LUA_GameObject> {
			const json j = prefab::ReadPrefabFile(self->ResolvePath(path));
			if (!j.is_object())
			{
				echo("ERROR: Prefab.instantiate - could not read " + path);
				return nullptr;
			}
			// Its own materials each time: sharing them across spawns would
			// mean holding engine resources alive in a cache across scene
			// unloads, which is not a trade worth making for something that
			// happens a few times a second.
			return std::static_pointer_cast<LUA_GameObject>(
				SceneSerializer::DeserializeSubtree(j.dump(), self->SceneAnchorPath(),
					self->physics, &self->lua, NULL));
		};
		lua["Prefab"] = prefabTable;
	}

	LuaComponent::SetUpdatesEnabled(true);
#endif

	if (std::getenv("PYROS_JOB_BENCH")) JobSystem::Instance().Benchmark();

	// The project's quality settings (game.json "quality"), before any script
	// runs: a script may still change them.
	// (what is the engine's: the same function the editor's Play applies the project's with)
	quality::ApplyToEngine(m.quality);
	// "frameRateLimit": that many frames a second at most; left out, -1 or 0, as the display refreshes
	frameRateLimit = m.qFrameLimit > 0.f ? m.qFrameLimit : 0.f;

	// What the project asked to have ready before anything is played (its
	// "preload" list): read here, kept for as long as the game runs.
	for (size_t i = 0; i < m.preload.size(); i++)
		AssetPreload::Add((fs::path(m.root) / m.preload[i]).string());

	const std::string firstScene = launchScene.empty() ? m.startupScene : launchScene;
#ifdef LUA_BINDINGS
	for (const auto &kv : lua.globals())
		if (kv.first.is<std::string>()) engineGlobals.insert(kv.first.as<std::string>());
#endif
	if (!LoadGameScene(firstScene))
	{
		echo("ERROR: could not load startup scene " + firstScene);
		Close();
		return;
	}
}

std::string PyrosPlayer::ExpandSceneFile(const std::string& absPath)
{
	std::ifstream in(absPath.c_str());
	if (!in.is_open()) return std::string();
	std::stringstream buffer;
	buffer << in.rdbuf();
	in.close();

	// a scene that names no prefab is handed over as it is, unparsed
	// (empty: "nothing to put in - read the file yourself")
	if (buffer.str().find("\"prefab\"") == std::string::npos) return std::string();

	json sceneJson;
	try { sceneJson = json::parse(buffer.str()); }
	catch (const std::exception&) { return std::string(); } // the engine reports it

	std::vector<prefab::Link> links;
	std::vector<std::string> errors;
	PyrosPlayer* self = this;
	prefab::ExpandScene(sceneJson,
		[self](const std::string& rel) { return prefab::ReadPrefabFile(self->ResolvePath(rel)); },
		links, errors);

	for (size_t i = 0; i < errors.size(); ++i)
		echo("ERROR: prefab not found, its objects are missing from this scene: " + errors[i]);

	if (links.empty()) return std::string();
	return sceneJson.dump();
}

namespace {
	// PYROS_LOADING_SCREEN=0: loads as before, black - to tell what the screen itself costs
	bool LoadingScreenOff() { const char* e = std::getenv("PYROS_LOADING_SCREEN"); return e && e[0] == '0'; }
	// whether a scene's light is solved at load (ambient mode 3): it then takes its own share of the bar
	bool SceneSolvesLight(const std::string &text)
	{
		const size_t k = text.find("\"ambientMode\"");
		if (k == std::string::npos) return false;
		size_t i = text.find(':', k);
		if (i == std::string::npos) return false;
		while (++i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' || text[i] == '\r')) {}
		return i < text.size() && text[i] == '3';
	}
}

void PyrosPlayer::OpenLoadingScreen()
{
	const PlayerManifest &m = PlayerManifestInstance();
	if (!m.loadingScreen || LoadingScreenOff()) return;
#ifdef EMSCRIPTEN
	// (a page shows nothing until the code running returns to the browser, which a load does not
	// do before it ends: frames drawn in it would only be load time. The page has its own bar.)
	return;
#endif
	std::string text, at;
	if (!m.loadingScene.empty())
	{
		at = ResolvePath(m.loadingScene);
		std::ifstream f(at, std::ios::binary);
		if (f) { std::stringstream ss; ss << f.rdbuf(); text = ss.str(); }
		else echo("WARNING: game.json's loading screen " + m.loadingScene + " is not there - showing the default");
	}
	if (text.empty())
	{
		text = LoadingScreen::DefaultSceneJson(m.loadingLook);
		at = ResolvePath("scenes/_loading.json");      // (where its picture's path is read from: the game's folder)
	}
	// Between its frames: the window's events kept moving (not handled - nothing of the game's runs
	// in the middle of a load) and the hang watchdog told the program is alive: a long load is not a hang.
	// And the window's size followed: one resized during the load (a tiling window manager places a new
	// window seconds after it opens) kept the old swapchain - the screen drawn small in a corner and the
	// rest of the window magenta. The swapchain is remade now; the game's own targets after the load.
	loadingScreen.Open(text, at, m.loadingLook, (uint32)Width, (uint32)Height, [this]() {
		SDL_PumpEvents(); FrameHeartbeat();
		int w = 0, h = 0;
		// (in pixels where SDL can say - 2.26 and later; the Linux CI's is older, and its windows are
		// not scaled: there points are pixels)
#if SDL_VERSION_ATLEAST(2, 26, 0)
		if (GetSDLWindow() != NULL) SDL_GetWindowSizeInPixels(GetSDLWindow(), &w, &h);
#else
		if (GetSDLWindow() != NULL) SDL_GetWindowSize(GetSDLWindow(), &w, &h);
#endif
		if (w > 0 && h > 0 && ((uint32)w != Width || (uint32)h != Height))
		{
			OnResize((uint32)w, (uint32)h);
			loadingScreen.Resize((uint32)w, (uint32)h);
		}
	});
	// (its frames drawn by the renderer: the screen's UI over its colour, a whole frame each)
	if (loadingScreen.IsOpen())
		loadingScreen.SetDrawer([this](SceneGraph* ui, const Vec4 &background, const uint32 w, const uint32 h) {
			renderer->SetBackground(SrgbToLinear(Vec3(background.x, background.y, background.z)));
			renderer->SetOverlayScenes(std::vector<SceneGraph*>());
			p3d::View view;
			view.width = w; view.height = h;
			renderer->Render(ui, view);
		});
	if (loadingScreen.IsOpen()) loadingScreen.Listen();
}

bool PyrosPlayer::LoadGameScene(const std::string& sceneRel)
{
	// What the player sees while this runs: a screen with the load's progress on it, closed at
	// every way out of here (what its frames draw with is let go when they are done: the device's Destroy).
	struct ScreenUp {
		LoadingScreen &s;
		~ScreenUp() { if (s.IsOpen()) { LoadProgress::Phase(1.f, 1.f, ""); s.Draw(); s.Close(); } }
	} screenUp{ loadingScreen };
	OpenLoadingScreen();
	LoadProgress::Phase(0.f, 0.02f, "Loading");

	UnloadGameScene();

	const std::string abs = ResolvePath(sceneRel);
	// Reset rather than shadowed: this is the player's own member now, and a
	// scene loaded after another must not inherit the previous one's view.
	meta = SceneMeta();
#ifdef LUA_BINDINGS
	sol::state* luaPtr = &lua;
#else
	sol::state* luaPtr = NULL;
#endif

	// Prefab references are resolved here rather than by the engine, which
	// knows nothing about them - the same pass the editor runs, from the
	// same header (shared/PrefabResolver.h). A built game therefore ships
	// scenes that still reference their prefabs, so a prefab edit reaches
	// every instance in the build exactly as it does in the editor.
	const std::string expanded = ExpandSceneFile(abs);
	// The bar's shares: reading the scene (its objects, their models and pictures) and, for a scene
	// whose light is solved here, the solve - which on a big scene is the longer of the two.
	bool solves = false;
	if (loadingScreen.IsOpen())
	{
		if (!expanded.empty()) solves = SceneSolvesLight(expanded);
		else { std::ifstream f(abs, std::ios::binary); std::stringstream ss; ss << f.rdbuf(); solves = SceneSolvesLight(ss.str()); }
	}
	LoadProgress::Phase(0.02f, solves ? 0.55f : 0.95f, "Loading");
	const bool loaded = expanded.empty()
		? SceneSerializer::LoadScene(scene, abs, physics, luaPtr, &sceneAssets, &meta)
		: SceneSerializer::LoadSceneFromText(scene, expanded, abs, physics, luaPtr, &sceneAssets, &meta);
	if (!loaded) return false;

	currentSceneRel = sceneRel;
	sceneLoaded = true;
	// See PyrosPlayer.h - a 2D scene defaults its camera to orthographic.
	sceneIs2D = meta.twoD;
	if (sceneIs2D) cameraOrthographic = true;

	// Styles are re-applied here rather than baked into the scene at build
	// time, so shipping a different theme.palette re-skins the whole game
	// without rebuilding a single scene.
	{
		std::string styleErr;
		const std::string root = PlayerManifestInstance().root;
		const int styled = uistyle::ApplyToScene(scene, root,
			ResolvePath("assets/ui/theme.palette"), styleErr);
		if (!styleErr.empty()) echo("WARNING: UI styles - " + styleErr);
		if (styled > 0)
		{
			char buf[64];
			snprintf(buf, sizeof(buf), "Applied UI styles to %d element(s)", styled);
			echo(buf);
		}
	}

	// Environment lighting, the same two values the editor's Scene panel
	// shows - so a build looks like what was authored. The scene's background
	// wins over game.json's: game.json carries a fallback for a scene that
	// predates the field, the scene carries the authored one.
	// The scene's settings, before anything renders with them: its ambient, background, sky, exposure
	ConfigureRenderer();

	// The terrain's shadow, where the scene asks for it baked: worked out here,
	// as part of loading the map (see TerrainHorizon).
	scene->SetTerrainHorizon(std::shared_ptr<TerrainHorizon>());
	// (A terrain grows its tiles on the scene's first updates, not while the
	// file is read: there is nothing to bake from yet. Update() does it as
	// soon as the tiles are there - it used to be asked for here, found no
	// terrain and quietly made nothing, so a built game drew its whole
	// terrain into the shadow map every frame after all.)
	terrainBakePending = meta.terrainShadowsBaked;
	// (said only when there is a bake to wait for: every scene without one printed it - an error in a browser's console)
	if (meta.terrainShadowsBaked) fprintf(stderr, "[terrain] scene %s: its shadow of itself to be baked\n", sceneRel.c_str());
	terrainBakeTiles = 0; terrainBakeStable = 0; terrainBakeWaited = 0;

	// A DDGI scene's light: its kept solve, else solved here on the CPU (and kept)
	renderer->SetGlobalIllumination(nullptr);
	if (meta.ambientMode == 3)
	{
		LoadProgress::Phase(0.55f, 0.97f, "Lighting");
		if (!ddgi) ddgi.reset(new DDGIVolume());
		if (SolveSceneGI(scene, GISettings(abs), *ddgi))
		{
			renderer->SetGlobalIllumination(ddgi.get());
			echo("Global illumination solved.");
			// (a dynamic one refreshed each frame - its light follows a light that moves - against the scene's rays)
			giRays.reset();
			if (meta.ddgiDynamic)
			{
				giRays.reset(new RayScene());
				if (giRays->BuildFromScene(scene)) giRays->Build(4); else giRays.reset();
			}
		}
		else echo("WARNING: global illumination could not be solved - the scene's flat ambient instead.");
	}
	LoadProgress::Phase(0.97f, 1.f, "Starting");
	ResolveCamera(abs);
	ApplyProjection();

#ifdef LUA_BINDINGS
	PushLuaHostGlobals();
#endif

	// Sounds and emitters authored in the scene start with the scene, the
	// same way entering play mode starts them in the editor. Anything a
	// script wants to control instead can stop it on its first update.
	{
		std::vector<std::shared_ptr<GameObject>>& all = scene->GetAllGameObjectList();
		std::vector<GameObject*> objects;
		for (size_t i = 0; i < all.size(); ++i) objects.push_back(all[i].get());
		StartSceneMedia(objects);
	}

	// A streamed world brings in the cells around the camera before the
	// first frame - the player must not spawn into an empty world - and
	// streams the rest from Update().
	worldStreamer.reset();
	if (meta.world.enabled)
	{
		// the cells name their prefab instances, as the scene does
		SceneSerializer::SetSubtreeFileFilter(prefab::ExpandSubtreeText);
		worldStreamer.reset(new WorldStreamer(scene, abs, meta.world, physics, luaPtr));
		worldStreamer->SetOnCellLoaded([this](const std::shared_ptr<GameObject> &root) {
			std::vector<GameObject*> objects(1, root.get());
			for (size_t i = 0; i < objects.size(); i++)
			{
				const std::vector<std::shared_ptr<GameObject> > &kids = objects[i]->GetChildren();
				for (size_t k = 0; k < kids.size(); k++) objects.push_back(kids[k].get());
			}
			StartSceneMedia(objects);
			RenderingComponent::StartAutoPlayIn(root.get());
		});
		const Vec3 focus = activeCamera ? activeCamera->GetWorldPosition() : Vec3();
		worldStreamer->LoadAround(focus);
		echo("Streamed world: " + std::to_string(worldStreamer->LoadedCount()) + " cell(s) loaded around the camera");
	}
	// Terrains stream their own tiles; the ground under the camera is in
	// before the first frame for the same reason.
	{
		const std::vector<Vec3> viewers(1, activeCamera ? activeCamera->GetWorldPosition() : Vec3());
		TerrainComponent::SetViewers(scene, viewers);
		const std::vector<TerrainComponent*> &terrains = TerrainComponent::Instances();
		for (size_t i = 0; i < terrains.size(); i++) terrains[i]->LoadAround(viewers);
	}

#ifdef LUA_BINDINGS
	// One update before the first frame so components spawned during load
	// register with the scene graph, matching DemoLauncher's own priming
	// update - without it a script that creates objects in its init sees
	// them appear a frame late.
	scene->Update(GetTime());

	// Clips marked autoplay start here, before the scene's own script, so the
	// script can stop or replace one instead of racing it.
	RenderingComponent::StartAutoPlayInScene(scene);

	if (!meta.mainScript.empty())
	{
		try
		{
			sceneMainScript = LuaComponent_FromFile(lua, meta.mainScript);
			// (what the scene set of the variables it offers)
			if (sceneMainScript) SceneSerializer::TakeScriptPropertiesText(*sceneMainScript, meta.mainScriptProperties);
			if (sceneMainScript) sceneMainScript->Init();
		}
		catch (const std::exception& e)
		{
			echo(std::string("ERROR: scene main script - ") + e.what());
		}
	}
#endif

	// The terrain's shadows of itself, baked while loading: the tiles around the camera are in by now
	// (LoadAround waits for them). It was baked in the frame - half a second held, as the match began.
	// (Only a terrain whose tiles are not in yet is left to the frame, once they have come.)
	if (terrainBakePending && scene && !TerrainEditor::FindTiles(scene).empty())
	{
		scene->SetTerrainHorizon(TerrainHorizon::Bake(scene, meta.terrainShadowsResolution, meta.terrainShadowsReach));
		terrainBakePending = false;
		fprintf(stderr, "[terrain] shadow of itself baked while loading: %s\n", scene->GetTerrainHorizon() ? "made" : "NOT made");
	}

	// Every model's levels of detail made before the game starts - the scene's, and those its script
	// made in its init (the guns of an armory): not in the frame one is first seen.
	{
		const auto t0 = std::chrono::steady_clock::now();
		SimplifiedModel::FinishPreparing();
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		if (ms > 50.0) fprintf(stderr, "Levels of detail: waited %.0f ms for the last of them\n", ms);
	}

	echo("SUCCESS: loaded " + sceneRel);
	return true;
}

void PyrosPlayer::StartSceneMedia(const std::vector<GameObject*> &objects)
{
	int soundsMissing = 0;
	for (size_t i = 0; i < objects.size(); ++i)
	{
		const std::vector<std::shared_ptr<IComponent>>& comps = objects[i]->GetComponents();
		for (size_t c = 0; c < comps.size(); ++c)
		{
			if (AudioSource* asrc = dynamic_cast<AudioSource*>(comps[c].get()))
			{
				if (!asrc->EnsureLoaded()) { ++soundsMissing; continue; }
				asrc->ResetVelocityTracking();
				asrc->Play();
			}
			else if (ParticleSystem* ps = dynamic_cast<ParticleSystem*>(comps[c].get()))
			{
				ps->Clear();
				ps->Play();
			}
		}
	}
	if (soundsMissing > 0)
		echo("WARNING: " + std::to_string(soundsMissing) + " sound(s) could not be loaded");
}

void PyrosPlayer::UnloadGameScene()
{
	if (!sceneLoaded) return;
#ifdef LUA_BINDINGS
	// The scripts of the scene being left are told first: destroy() is where
	// they put away what they made. Dropping the script without it - which is
	// what this did - left everything a script had spawned in the next scene:
	// a title screen's helicopter, figure and sky standing in the level it
	// had just loaded.
	{
		std::vector<GameObject*> all;
		scene->CollectGameObjectsRecursive(all);
		for (size_t i = 0; i < all.size(); i++)
		{
			if (!all[i]) continue;
			const std::vector<std::shared_ptr<IComponent> > &cs = all[i]->GetComponents();
			for (size_t c = 0; c < cs.size(); c++)
				if (LuaComponent* lc = dynamic_cast<LuaComponent*>(cs[c].get())) lc->ResetLifecycle();
		}
	}
	if (sceneMainScript) sceneMainScript->ResetLifecycle();
	sceneMainScript.reset();
	// ...and what they were listening for is theirs no longer. An Input a script
	// made keeps calling its handlers until Lua collects it - and a handler is
	// held by the engine, so anything it refers to (the script, and through it
	// often the Input itself) is never collected at all. The level left for the
	// menu went on hearing every key and the mouse: in the next match the old
	// player's handler took the mouse's motion before the new one asked for it,
	// and the view would not turn; the old guns fired; and a handler could call
	// into what the scene had already taken apart. (The editor does this on Stop.)
	LuaInputBridge::ClearAllCallbacks();
#endif
	// Its cells are in the scene; they go first, by the streamer's hand.
	worldStreamer.reset();
	// Replicas and spawned objects live in the scene too.
	network.reset();
	activeCamera = NULL;
	SceneSerializer::UnloadScene(scene, sceneAssets);
	// ...and whatever a script made and did not put away goes with the scene:
	// UnloadScene() removes only what the scene file itself had loaded.
	scene->RemoveAll();
	sceneAssets = LoadedSceneAssets();
	sceneLoaded = false;
#ifdef LUA_BINDINGS
	// What the scene's scripts made and let go of is a few bytes of userdata
	// to Lua and megabytes of models and textures to the engine: the
	// collector paces itself by the first, so a level left for the menu was
	// still in memory when the next one loaded. Twice - an object with a
	// finalizer is freed the cycle after the one that finds it.
	lua.collect_garbage();
	lua.collect_garbage();
#endif
}

void PyrosPlayer::ResolveCamera(const std::string& sceneAbsPath)
{
	activeCamera = NULL;
	cameraFov = 70.f;
	cameraNear = 0.1f;
	cameraFar = 2000.f;

	// A scene that frames itself (SceneMeta::View2D) needs no camera object.
	// The GameObject below still exists because RenderScene() wants something
	// to render *from* - but it is this player's own, driven from the scene's
	// view every frame, and nothing about it has to be authored.
	if (meta.view2D.enabled)
	{
		if (!fallbackCamera) fallbackCamera = std::make_shared<GameObject>();
		scene->Add(fallbackCamera);
		activeCamera = fallbackCamera.get();
		cameraOrthographic = true;
		cameraNear = SceneMeta::View2D::kNear;
		cameraFar = SceneMeta::View2D::kFar;
		UpdateView2DCamera(0.f);
		// Said out loud, the way the swapchain mode is: "why is my game
		// looking at the wrong place" is a question the log should answer, and
		// a scene framed by itself has no camera object to inspect instead.
		{
			char buf[256];
			snprintf(buf, sizeof(buf),
				"Scene framed by its own 2D view: centre (%.2f, %.2f), half-height %.2f%s%s",
				meta.view2D.center.x, meta.view2D.center.y, meta.view2D.halfHeight,
				meta.view2D.follow.empty() ? "" : ", following ",
				meta.view2D.follow.c_str());
			echo(buf);
		}
		return;
	}

	// Which GameObject is a camera, and which one is active, is recorded by
	// the editor in <scene>.json.editor.json - the scene file itself has no
	// notion of a camera, so without this sidecar a built game would have
	// nothing to render from. Build Game ships it for exactly this reason.
	// A camera's projection is part of what the editor records here, so it
	// outgrew the Vec3 this used to be.
	struct CameraSidecar { bool orthographic; f32 fov, orthoSize, nearPlane, farPlane; };
	std::string activeName;
	std::map<std::string, CameraSidecar> cameraSettings;
	std::ifstream in((sceneAbsPath + ".editor.json").c_str());
	if (in)
	{
		try
		{
			json j;
			in >> j;
			if (j.contains("activeCamera") && j["activeCamera"].is_string())
				activeName = j["activeCamera"].get<std::string>();
			if (j.contains("cameras") && j["cameras"].is_object())
				for (json::iterator it = j["cameras"].begin(); it != j["cameras"].end(); ++it)
				{
					CameraSidecar c;
					// Defaults match a sidecar written before orthographic
					// cameras existed - which is to say, perspective.
					c.orthographic = it.value().value("orthographic", false);
					c.fov = it.value().value("fov", 70.f);
					c.orthoSize = it.value().value("orthoSize", 10.f);
					c.nearPlane = it.value().value("near", 0.1f);
					c.farPlane = it.value().value("far", 2000.f);
					cameraSettings[it.key()] = c;
				}
		}
		catch (const std::exception&) { /* falls through to the defaults below */ }
	}

	std::vector<std::shared_ptr<GameObject>>& all = scene->GetAllGameObjectList();
	GameObject* firstKnownCamera = NULL;
	for (size_t i = 0; i < all.size(); ++i)
	{
		std::map<std::string, CameraSidecar>::const_iterator s = cameraSettings.find(all[i]->GetName());
		if (s == cameraSettings.end()) continue;
		if (!firstKnownCamera) firstKnownCamera = all[i].get();
		if (all[i]->GetName() == activeName)
		{
			activeCamera = all[i].get();
			cameraOrthographic = s->second.orthographic;
			cameraFov = s->second.fov;
			cameraOrthoSize = s->second.orthoSize;
			cameraNear = s->second.nearPlane;
			cameraFar = s->second.farPlane;
			return;
		}
	}

	if (firstKnownCamera)
	{
		activeCamera = firstKnownCamera;
		std::map<std::string, CameraSidecar>::const_iterator s = cameraSettings.find(activeCamera->GetName());
		if (s != cameraSettings.end())
		{
			cameraOrthographic = s->second.orthographic;
			cameraFov = s->second.fov;
			cameraOrthoSize = s->second.orthoSize;
			cameraNear = s->second.nearPlane;
			cameraFar = s->second.farPlane;
		}
		echo("WARNING: no active camera set for this scene - using '" + activeCamera->GetName() + "'");
		return;
	}

	// Nothing usable. A camera at the origin renders *something*, which is
	// far easier to diagnose from than a black window.
	if (!fallbackCamera) fallbackCamera = std::make_shared<GameObject>();
	fallbackCamera->SetPosition(Vec3(0.f, 2.f, 10.f));
	scene->Add(fallbackCamera);
	activeCamera = fallbackCamera.get();
	// Not a warning for a 2D scene: its content is UICanvas trees drawn in
	// screen space, the 3D pass is suppressed anyway, and a camera is exactly
	// the thing it is not supposed to need. The fallback above still runs -
	// RenderScene() wants a camera object even when it draws no world.
	if (!sceneIs2D)
		echo("WARNING: this scene has no camera - rendering from a default one at the origin."
			" Add a camera in the editor and set it active.");
}

void PyrosPlayer::UpdateView2DCamera(const f32 dt)
{
	if (!meta.view2D.enabled || !activeCamera) return;

	// Follow, zoom, bounds, shake and the automatic layers: one function,
	// the same one the editor's play mode calls.
	const f32 aspect = (Height > 0) ? ((f32)Width / (f32)Height) : 1.f;
	const f32 zoomBefore = meta.view2D.halfHeight;
	UpdateSceneView2D(meta.view2D, scene, dt, aspect);
	// A zoom in progress changes what the projection is built from.
	if (meta.view2D.halfHeight != zoomBefore) ApplyProjection();

	activeCamera->SetTransformationMatrix(meta.view2D.CameraMatrix());
	// SetTransformationMatrix only writes the LOCAL matrix; the world matrix
	// the frame is actually viewed through is rebuilt in scene->Update(),
	// which for the camera's own placement has to be forced here - the same
	// trap the editor's rig viewport documents.
	activeCamera->RefreshTransformation();
}

void PyrosPlayer::ApplyProjection()
{
	// A self-framing 2D scene owns its projection: height is authored, width
	// follows from the window, so the same scene fills any window.
	if (meta.view2D.enabled)
	{
		const f32 a = (Height > 0) ? ((f32)Width / (f32)Height) : 1.f;
		projection = meta.view2D.MakeProjection(a);
		return;
	}

	const f32 aspect = (Height > 0) ? ((f32)Width / (f32)Height) : 1.f;
	if (cameraOrthographic)
	{
		const f32 halfH = cameraOrthoSize;
		const f32 halfW = halfH * aspect;
		projection.Ortho(-halfW, halfW, -halfH, halfH, cameraNear, cameraFar);
	}
	else
	{
		appliedFovScale = GetViewFovScale();
		projection.Perspective(cameraFov * appliedFovScale, aspect, cameraNear, cameraFar);   // (a game's zoom: ViewFovScale.h)
	}
}

#ifdef LUA_BINDINGS
void PyrosPlayer::PushLuaHostGlobals()
{
	lua["physics"] = static_cast<IPhysics*>(physics);
	// Same name the editor publishes, so a scene script raycasts identically
	// in the preview and in the shipped game.
	lua["physics2d"] = physics2D;
	lua["scene"] = scene;
	lua["camera"] = activeCamera;
	// (the view's projection, as the DemoLauncher gives it: placeDecalAtCursor and friends turn a pixel into a ray with it)
	lua["projection"] = &projection;
	// Accepted and ignored: in the editor this redirects the *viewport* to a
	// different camera, and a script written against play mode will call it.
	// Here the render camera is whatever the scene says, so honouring it is
	// exactly right - it is the same thing.
	lua["setRenderCamera"] = [this](GameObject* go) {
		if (!go) return;
		// (a cut: what the temporal anti-aliasing kept of the last view is not this one's)
		if (go != activeCamera) renderer->ResetHistory();
		activeCamera = go;
	};

	lua["loadScene"] = [this](const std::string& name) { pendingLoadSceneName = name; };

	// The scene's own 2D view: the `view` table, bound by the engine so the
	// editor's play mode hands scripts exactly the same one.
	{
		LuaView2DHost host;
		host.view = [this]() { return &meta.view2D; };
		// The projection is rebuilt from the view, and nothing else will ask
		// for it until the window resizes.
		host.changed = [this]() { ApplyProjection(); };
		host.viewportSize = [this]() { return Vec2((f32)Width, (f32)Height); };
		host.mouseInViewport = []() { return InputManager::GetMousePosition(); };
		LuaBindView2D(lua, host);
	}
	// Show a 2D scene over whatever is running, and take it down again. The
	// scene shown is an ordinary scene file - usually one marked twoD - so a
	// pause menu can also be opened on its own with loadScene().
	lua["showOverlay"] = [this](const std::string& name) { ShowOverlayScene(name); };
	lua["hideOverlay"] = [this]() { HideOverlayScene(); };
	lua["overlayScene"] = [this]() { return fs::path(overlaySceneRel).stem().string(); };
	lua["currentScene"] = [this]() { return fs::path(currentSceneRel).stem().string(); };
	lua["echo"] = [](const std::string& msg) { p3d::LOG::_LOG::_echo(msg); };
	// A script's own scopes in the frame profile (perf.log, the editor's Profiler):
	// profileBegin("Bots") ... profileEnd().
	lua["profileBegin"] = [](const std::string &name) {
		// (the profiler keeps the name's address: one copy of each name, for good)
		static std::set<std::string> names;
		p3d::FrameProfiler::Instance().Begin(names.insert(name).first->c_str());
	};
	lua["profileEnd"] = []() { p3d::FrameProfiler::Instance().End(); };
	lua["profileCount"] = [](const std::string &name, const f64 value) {
		static std::set<std::string> names;
		p3d::FrameProfiler::Instance().Counter(names.insert(name).first->c_str(), value);
	};
	lua["ASSETS_PATH"] = (fs::path(PlayerManifestInstance().root) / "assets").string() + "/";
}

bool PyrosPlayer::LoadOverlayScene(const std::string& sceneRel)
{
	const std::string abs = (fs::path(PlayerManifestInstance().root) / sceneRel).string();
	if (!fs::exists(abs))
	{
		echo("ERROR: overlay scene not found: " + sceneRel);
		return false;
	}

	// Its own graph, never merged into the running scene - see the header.
	// Physics is deliberately NULL: an overlay is UI, and giving it bodies
	// would step them against the level's world.
	delete overlayScene;
	overlayScene = new SceneGraph();

#ifdef LUA_BINDINGS
	sol::state* luaPtr = &lua;
#else
	sol::state* luaPtr = NULL;
#endif
	SceneMeta overlayMeta;
	const std::string expanded = ExpandSceneFile(abs);
	const bool loaded = expanded.empty()
		? SceneSerializer::LoadScene(overlayScene, abs, NULL, luaPtr, &sceneAssets, &overlayMeta)
		: SceneSerializer::LoadSceneFromText(overlayScene, expanded, abs, NULL, luaPtr, &sceneAssets, &overlayMeta);
	if (!loaded)
	{
		delete overlayScene;
		overlayScene = NULL;
		echo("ERROR: could not load overlay scene: " + sceneRel);
		return false;
	}
	// Same reason LoadGameScene updates immediately after loading: components
	// are not registered with the graph, and canvases therefore not findable,
	// until an Update has run.
	overlayScene->Update(GetTime());
	overlaySceneRel = sceneRel;
	return true;
}

void PyrosPlayer::ShowOverlayScene(const std::string& sceneRel)
{
	pendingOverlayName = sceneRel;
	pendingOverlayHide = false;
}

void PyrosPlayer::HideOverlayScene()
{
	pendingOverlayHide = true;
	pendingOverlayName.clear();
}

void PyrosPlayer::ApplyPendingOverlayIfAny()
{
	if (pendingOverlayHide)
	{
		pendingOverlayHide = false;
		delete overlayScene;
		overlayScene = NULL;
		overlaySceneRel.clear();
	}
	if (pendingOverlayName.empty()) return;
	std::string rel = pendingOverlayName;
	pendingOverlayName.clear();
	// Bare name or explicit project-relative path, same rule as loadScene().
	if (rel.find('/') == std::string::npos && rel.find('\\') == std::string::npos)
		rel = "scenes/" + rel + ".json";
	LoadOverlayScene(rel);
}

// What a DDGI scene's light is solved with: its counts, rays, passes and sky, and where its solve is kept (<scene>.gi,
// keyed by the scene's text and these) - for either renderer
SceneGISettings PyrosPlayer::GISettings(const std::string &abs) const
{
	SceneGISettings gi;
	gi.enabled = true;
	for (uint32 i = 0; i < 3; i++) gi.counts[i] = meta.ddgiCounts[i];
	gi.raysPerProbe = meta.ddgiRaysPerProbe;
	gi.passes = meta.ddgiPasses;
	gi.skyColor = Vec3(meta.ddgiSky.x, meta.ddgiSky.y, meta.ddgiSky.z);
	if (meta.ddgiHasBounds) { gi.hasBounds = true; gi.boundsMin = meta.ddgiMin; gi.boundsMax = meta.ddgiMax; }
	// a scene with an HDR environment: its rays see that sky, as bright as the scene's ambient
	if (EnvironmentMap::IsLoaded())
	{
		gi.skyFromSH = true;
		gi.skySH = EnvironmentMap::GetSH();
		gi.skySH.Scale(meta.ambientIntensity);
	}
	// Relative to the game directory, where build_game puts the
	// shaders next to the binary - the same place the player's own
	// materials come from.
	// Light that does not change once solved is kept in <scene>.gi, keyed by the scene's text and
	// these settings: the next load reads it instead of tracing (a long street: a minute saved).
	// A package ships the file, so the first load on another machine reads it too.
	if (!meta.ddgiDynamic && std::getenv("PYROS_GI_NO_CACHE") == NULL)
	{
		std::ifstream f(abs, std::ios::binary);
		std::stringstream ss; ss << f.rdbuf();
		uint64 h = 1469598103934665603ull;
		auto mix = [&h](const void* p, size_t n) { const uint8* b = (const uint8*)p; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; } };
		const std::string text = ss.str();
		mix(text.data(), text.size());
		mix(gi.counts, sizeof(gi.counts)); mix(&gi.raysPerProbe, sizeof(gi.raysPerProbe)); mix(&gi.passes, sizeof(gi.passes));
		mix(&gi.boundsMin, sizeof(Vec3)); mix(&gi.boundsMax, sizeof(Vec3));
		for (uint32 c = 0; c < SphericalHarmonicsL2::kCoefficientCount; c++) mix(&gi.skySH.coefficients[c], sizeof(Vec3));
		const char version[] = "gi-cache-1";
		mix(version, sizeof(version));
		gi.cachePath = abs + ".gi";
		gi.cacheKey = h;
	}
	return gi;
}

void PyrosPlayer::ApplyPendingSceneLoadIfAny()
{
	if (pendingLoadSceneName.empty()) return;
#if defined(__EMSCRIPTEN__)
	// (a web game in two parts: its first scene's files first, the rest after it has started - player/web's page says
	// so with this file while the rest is still coming. Another scene waits for all of them: what it reads is there)
	{
		struct stat st;
		if (stat("/.pyros-files-coming", &st) == 0)
		{
			static bool said = false;
			if (!said) { said = true; echo("Scene " + pendingLoadSceneName + " waits for the game's files still downloading"); }
			return;
		}
	}
#endif
	const std::string requested = pendingLoadSceneName;
	pendingLoadSceneName.clear();

	// A bare name ("Level2") or an explicit project-relative path, same as
	// the editor's loadScene().
	std::string rel = requested;
	if (rel.find('/') == std::string::npos && rel.find('\\') == std::string::npos)
		rel = "scenes/" + rel + ".json";

#ifdef LUA_BINDINGS
	p3d::LuaClearTasks(&lua);
#endif
	renderer->ForgetScene();     // (what it kept by the old scene's objects' addresses)
	if (!LoadGameScene(rel))
		echo("ERROR: loadScene(\"" + requested + "\") failed");
}
#endif

void PyrosPlayer::UpdateConsole()
{
	if (!PlayerManifestInstance().devConsole) return;
	// (on the press of the key, not while it is held; the physical key left of 1, on any layout)
	static bool wasDown[8] = { false, false, false, false, false, false, false, false };
	const Uint8* k = SDL_GetKeyboardState(NULL);
	const int scan[8] = { SDL_SCANCODE_GRAVE, SDL_SCANCODE_RETURN, SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
		SDL_SCANCODE_PAGEUP, SDL_SCANCODE_PAGEDOWN, SDL_SCANCODE_ESCAPE };
	bool pressed[8];
	for (int i = 0; i < 8; i++) { const bool d = k[scan[i]] != 0 || (i == 1 && k[SDL_SCANCODE_KP_ENTER]); pressed[i] = d && !wasDown[i]; wasDown[i] = d; }
	if (pressed[0])
	{
		if (console.IsOpen()) console.Close();
		else
		{
			console.Open((uint32)Width, (uint32)Height);
			SDL_StartTextInput();
		}
		return;
	}
	if (!console.IsOpen()) return;
	if (pressed[1]) console.Press(DevConsole::Enter);
	if (pressed[2]) console.Press(DevConsole::Backspace);
	if (pressed[3]) console.Press(DevConsole::Up);
	if (pressed[4]) console.Press(DevConsole::Down);
	if (pressed[5]) console.Press(DevConsole::PageUp);
	if (pressed[6]) console.Press(DevConsole::PageDown);
	if (pressed[7]) console.Press(DevConsole::Escape);
}

void PyrosPlayer::Update()
{
	UpdateConsole();
	if (!sceneLoaded) return;

	// The game's zoom (setViewFovScale): the projection is otherwise only built on a load or a
	// resize, so a soft aim's narrower view was set and never shown.
	if (GetViewFovScale() != appliedFovScale) ApplyProjection();

	if (terrainBakePending && scene)
	{
		// when the terrain's tiles have all arrived (the same number for a few
		// frames running), or it has been long enough
		const size_t tiles = TerrainEditor::FindTiles(scene).size();
		if (tiles > 0 && tiles == terrainBakeTiles) terrainBakeStable++;
		else { terrainBakeTiles = tiles; terrainBakeStable = 0; }
		if (tiles > 0 && (terrainBakeStable >= 3 || ++terrainBakeWaited > 240))
		{
			PYROS_PROFILE_SCOPE("Player.TerrainBake");
			scene->SetTerrainHorizon(TerrainHorizon::Bake(scene, meta.terrainShadowsResolution, meta.terrainShadowsReach));
			terrainBakePending = false;
			fprintf(stderr, "[terrain] shadow of itself baked over %zu tiles: %s\n", tiles, scene->GetTerrainHorizon() ? "made" : "NOT made");
		}
	}

	// Alt+Enter or F11: the whole screen, and back. On the press, not while held.
	{
		static bool wasDown = false;
		const Uint8* k = SDL_GetKeyboardState(NULL);
		const bool down = k[SDL_SCANCODE_F11] || ((k[SDL_SCANCODE_LALT] || k[SDL_SCANCODE_RALT]) && k[SDL_SCANCODE_RETURN]);
		if (down && !wasDown)
		{
			const bool full = (SDL_GetWindowFlags(GetSDLWindow()) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
			SDL_SetWindowFullscreen(GetSDLWindow(), full ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
		}
		wasDown = down;
	}

	// (on the first frame, not when the window is made: asked for then, macOS
	// leaves it a window)
	{
		static bool startFullscreen = PlayerManifestInstance().fullscreen;
		if (startFullscreen && GetSDLWindow() != NULL)
		{
			startFullscreen = false;
			SDL_SetWindowFullscreen(GetSDLWindow(), SDL_WINDOW_FULLSCREEN_DESKTOP);
		}
	}

	// At the top of the frame, before anything renders - see OnResize().
	{
		PYROS_PROFILE_SCOPE("Player.Resize");
		ApplyPendingResizeIfAny();
	}

	const f64 time = GetTime();
	const f64 dt = GetTimeInterval();
	// The on-screen controls: live while the game holds the pointer; fingers into keys and movement
	TouchControls::Instance().SetLive(mouseCaptured && sceneLoaded);
	TouchControls::Instance().Update(Width, Height);

	{
		PYROS_PROFILE_SCOPE("Player.Physics");
		physics->Update(dt, 10);
	}
	// 2D bodies, after the 3D world and before the scene solves its
	// transforms - Step() writes positions onto GameObjects and they have to
	// be in place before anything reads them this frame.
	if (physics2D)
	{
		// The scene's own gravity. Compared rather than set every frame, so
		// a script that changes it (physics2d:setGravity) keeps its change
		// until the next scene is loaded.
		if (appliedGravity2D.x != meta.gravity2D.x || appliedGravity2D.y != meta.gravity2D.y)
		{
			appliedGravity2D = meta.gravity2D;
			physics2D->SetGravity(meta.gravity2D);
		}
		physics2D->Sync(scene);
		physics2D->Step(dt, scene);

	}
	// After the step, so a frame's shadows match the positions it draws the
	// casters at. Outside the physics guard on purpose - a scene with no
	// physics still has occluders.

	// One frame of a dynamic DDGI volume's refresh, so indirect light follows a light that moves: on the CPU, as
	// much of the volume as 2 ms a frame takes, then written into its textures
	if (ddgi && giRays && meta.ddgiDynamic)
	{
		PYROS_PROFILE_SCOPE("Player.GI");
		const RaySceneChange::Enum change = giRays->RefreshTransforms();
		if (change == RaySceneChange::NeedsRebuild) { if (giRays->BuildFromScene(scene)) giRays->Build(4); }
		else if (change == RaySceneChange::Moved) giRays->RefitBVH();
		ddgi->SetUpdateTimeBudget(2.f);
		UpdateSceneGI(scene, *giRays, *ddgi, meta.ddgiRaysPerProbe, giFrame++, meta.ddgiHysteresis, meta.ddgiProbeBudget);
		renderer->GlobalIlluminationChanged();
	}
	// Layer parallax is deliberately NOT applied here. It is three lines of
	// Lua against Layer2D's factor, and doing it in the engine meant it
	// worked in a built game but not in the editor's play mode, and that a
	// script moving a layer would be fighting the engine for the same
	// transform. The engine's job is the layer; what it does is the game's.
	// Cells in and out around the camera, before the scene solves this
	// frame's transforms so an arriving cell is drawn where it belongs.
	{
		PYROS_PROFILE_SCOPE("Player.Stream");
		HoldBenchCamera(activeCamera, scene);
		if (worldStreamer && activeCamera)
			worldStreamer->Update(activeCamera->GetWorldPosition());
	}
	// Terrains follow the same camera. The world's streamer pumps the
	// loader they share; without one it is pumped here.
	{
		PYROS_PROFILE_SCOPE("Player.Terrain");
		if (activeCamera) TerrainComponent::SetViewers(scene, std::vector<Vec3>(1, activeCamera->GetWorldPosition()));
		if (!worldStreamer) AssetStreamer::Instance().Pump(4.0);
	}
	// Foliage thins and fades against the camera the game is seen through.
	if (activeCamera) FoliageComponent::SetViewer(activeCamera->GetWorldPosition());
	// The network before the scene: snapshots pose replicas, and the scene
	// then solves their transforms for this frame. Relevance is measured
	// from the camera.
	// --connect / --host, once the scene's scripts have had their start -
	// and only if none of them has hosted or joined already.
	const bool launchByName = !launchRendezvous.empty() && !launchSession.empty();
	if (!launchNetDone && sceneLoaded && (!launchConnect.empty() || launchHostPort > 0 || launchByName))
	{
		launchNetDone = true;
		if (!network) network.reset(new NetworkSession(scene, ResolvePath(currentSceneRel), physics, &lua));
		// Said out loud unless the game's script has its own handler: a
		// refused connection otherwise looks like a game that does nothing.
		if (!network->onRejected)
			network->onRejected = [](const std::string &reason) { std::fprintf(stderr, "PyrosPlayer: the server refused the connection - %s\n", reason.c_str()); };
		if (network->GetRole() == NetworkSession::Offline)
		{
			bool ok;
			NetworkSettings launchSettings;
			launchSettings.password = launchPassword;
			launchSettings.serverPublicKey = !launchServerKey.empty() ? launchServerKey : PlayerManifestInstance().serverPublicKey;
			launchSettings.rendezvous = launchRendezvous;
			launchSettings.sessionName = launchSession;
			launchSettings.autoReconnect = true;	// a dropped connection is retried
			launchSettings.reconnectGrace = 30.f;
			if (launchHostPort > 0) ok = network->Host((uint16)launchHostPort, launchSettings);
			else
			{
				std::string host = launchConnect;
				int port = 47400;
				const size_t colon = host.rfind(':');
				if (colon != std::string::npos) { port = std::atoi(host.c_str() + colon + 1); host = host.substr(0, colon); }
				ok = network->Connect(host, (uint16)port, launchSettings);
			}
			std::fprintf(stderr, "PyrosPlayer: %s %s\n", launchHostPort > 0 ? "hosting on port" : "joining",
				launchHostPort > 0 ? std::to_string(launchHostPort).c_str() : (launchByName ? launchSession.c_str() : launchConnect.c_str()));
			if (!ok) std::fprintf(stderr, "PyrosPlayer: network start failed\n");
		}
	}
	if (network)
	{
		if (activeCamera) network->SetViewer(activeCamera->GetWorldPosition());
		PYROS_PROFILE_SCOPE("Player.Network");
		network->Update(dt);
	}
	{
		PYROS_PROFILE_SCOPE("Player.SceneUpdate");
		scene->Update(time);
	}
	// The overlay is a real SceneGraph and needs solving every frame like any
	// other - its UI layout, animations and component registration all happen
	// in Update(). Rendering it without this draws nothing at all:
	// UICanvas::GetCanvasesOnScene() finds its canvases only once the
	// components have registered, which is Update's job.
	if (overlayScene) overlayScene->Update(time);

#ifdef LUA_BINDINGS
	if (sceneMainScript)
	{
		PYROS_PROFILE_SCOPE("Player.Script");
		try { sceneMainScript->Update(time); }
		catch (const std::exception& e) { echo(std::string("ERROR: scene main script update - ") + e.what()); }
	}
	// (the scripts' garbage, collected here and within a budget rather than
	// wherever in the next frame Lua would have chosen)
	{
		PYROS_PROFILE_SCOPE("Player.LuaGC");
		p3d::LuaCollectWithinBudget(&lua, 1.0);
	}
#endif
	// what the script has just moved is drawn where it put it
	{
		PYROS_PROFILE_SCOPE("Player.Settle");
		scene->SettleTransforms();
	}

	// The scene's own 2D view, after the script has had its say. A script
	// that moves the followed object, or writes view.center itself, has run by
	// now; doing this before it would render one frame behind whatever it did.
	UpdateView2DCamera((f32)dt);

	if (AudioManager* audio = AudioManager::GetActive())
	{
		if (activeCamera) audio->SetListenerFromGameObject(activeCamera, dt);
		{
			// the scene's space, on its world bus - compared, so a scene loaded brings its own
			const f32 rv[6] = { meta.reverb ? 1.f : 0.f, meta.reverbRoom, meta.reverbDamping, meta.reverbWet, meta.reverbPreDelay, meta.reverbEarly };
			if (memcmp(rv, appliedReverb, sizeof(rv)) != 0)
			{
				memcpy(appliedReverb, rv, sizeof(rv));
				if (meta.reverb) audio->SetWorldReverb(meta.reverbRoom, meta.reverbDamping, meta.reverbWet, meta.reverbPreDelay, meta.reverbEarly);
				else audio->ClearWorldReverb();
			}
		}
	}

	// The frame: what the scripts place or pose last (hands on a gun, a scope's camera), the scene drawn with its
	// UI and the overlays over it, then the UI's input against the layout just drawn
#ifdef LUA_BINDINGS
	{
		PYROS_PROFILE_SCOPE("Player.ScriptPreRender");
		if (sceneMainScript) sceneMainScript->PreRender();
		LuaComponent::PreRenderScene(scene);
	}
#endif
	HoldBenchCamera(activeCamera, scene);
	// (the scene's 2D shadow casters, for its 2D lights - knowing what is on screen when the scene frames itself)
	if (meta.view2D.enabled)
	{
		const Vec4 seen = meta.view2D.VisibleRect((Height > 0) ? ((f32)Width / (f32)Height) : 1.f);
		Occluder2D::PublishSceneOccluders(scene, &seen);
	}
	else Occluder2D::PublishSceneOccluders(scene);
	{
		PYROS_PROFILE_SCOPE("Player.Draw");
		DrawFrame();
	}
	// (while the console is open the keys are its, not the game's UI's)
	if (!console.IsOpen()) DispatchUIInput();
	CloseFrame();
}

// What the renderer takes of the scene's settings: its ambient, its background, the sky (an enabled Sky effect), the
// exposure (an Output effect: uAuto 0 a fixed 2^uExposure, otherwise auto compensated by uExposure)
void PyrosPlayer::ConfigureRenderer()
{
	p3d::ApplySceneSettings(*renderer, meta.ambientLight, meta.ambientIntensity, meta.background, meta.postEffects, meta.twoD);
	p3d::ApplyAmbientEnvironment(*renderer, meta.ambientMode, meta.ambientSky, meta.ambientEquator, meta.ambientGround, meta.ambientSH, meta.ambientIntensity);
}

void PyrosPlayer::DrawFrame()
{
	GameObject::TakeDrawTransforms();
	p3d::View view;
	// (a scene without a camera - a menu of UI alone - is still drawn: its UI over the default view)
	if (activeCamera != NULL) view.cameraWorld = activeCamera->GetDrawWorld();
	// Over the scene's own UI: the overlay (a pause menu, a dialog), the on-screen controls, the console
	std::vector<SceneGraph*> overlays;
	if (overlayScene) overlays.push_back(overlayScene);
	if (SceneGraph* touch = TouchControls::Instance().Prepare(Width, Height, GetTime())) overlays.push_back(touch);
	if (console.IsOpen()) if (SceneGraph* c = console.Prepare(GetTime())) overlays.push_back(c);
	renderer->SetOverlayScenes(overlays);
	view.fovY = projection.Fov > 0.f ? projection.Fov : 70.f;
	view.nearZ = projection.Near > 0.f ? projection.Near : 0.1f;
	// (an orthographic camera - a 2D scene's own view above all - as it frames: its height, its depth range)
	if (cameraOrthographic)
	{
		view.orthographic = true;
		view.orthoHeight = meta.view2D.enabled ? meta.view2D.halfHeight * 2.f : std::max(projection.Top - projection.Bottom, 0.001f);
		view.nearZ = cameraNear; view.farZ = cameraFar;
	}
	view.width = Width; view.height = Height;
	renderer->Render(scene, view);
	// (PYROS_RENDER_STATS: every 120 frames, what the frame cost)
	static const bool stats = getenv("PYROS_RENDER_STATS") != NULL;
	static uint32 frames = 0;
	if (stats && ++frames % 120 == 0)
	{
		const gfx::FrameStats &s = renderer->Stats();
		std::string passes;
		for (const gfx::FrameStats::Pass &p : s.passes) { char b[96]; snprintf(b, sizeof(b), " %s=%.2f", p.name.c_str(), p.gpuMs); passes += b; }
		fprintf(stderr, "[renderer] CPU %.2f ms (gather %.2f, submit %.2f, present %.2f; +%.2f waiting), GPU %.2f ms, draws %u |%s\n", renderer->CpuMilliseconds(), renderer->GatherMilliseconds(), renderer->SubmitMilliseconds(), renderer->PresentMilliseconds(), renderer->WaitMilliseconds(), s.gpuMs, s.draws, passes.c_str());
	}
}

void PyrosPlayer::CloseFrame()
{
	// No more frames than asked for (SetFrameRateLimit): what a display cannot
	// show is heat and nothing else - and a machine that slows itself down when
	// hot then cannot hold the rate it could have. Waited out here, at the end
	// of the frame; counted as idle for the render scale, which is what it is.
	if (frameRateLimit > 0.f)
	{
		const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		const std::chrono::duration<f64> period(1.0 / (f64)frameRateLimit);
		if (nextFrameAt.time_since_epoch().count() == 0) nextFrameAt = now;
		nextFrameAt += std::chrono::duration_cast<std::chrono::steady_clock::duration>(period);
		// Behind by more than a period (a frame that ran long, a scene slower than the limit): the
		// schedule starts again from now, and this frame does not wait. (It was put back to now and
		// then a period added - once 4 periods behind, which a scene drawn at 21 fps is every other
		// frame - so the frame that had just run late slept a whole period more: a 17 ms sleep in
		// 331 of ChaseLab's 851 slow frames.) Behind by less, the next frame has that much less.
		if (nextFrameAt + period < now) nextFrameAt = now;
		if (nextFrameAt > now)
		{
			PYROS_PROFILE_SCOPE("Player.FrameLimit");
			const f64 waitMs = std::chrono::duration<f64, std::milli>(nextFrameAt - now).count();
			// (sleep to within a millisecond and a half of it - SDL's sleep
			// overshoots by as much as that - then, where the system's own
			// sleep is good to a fraction of a millisecond, sleep most of what
			// is left too; only the last of it is spun away. Spinning the
			// whole millisecond and a half was a tenth of a core, every frame,
			// on the machines this limit is there to keep cool.)
			if (waitMs > 2.0) SDL_Delay((Uint32)(waitMs - 1.5));
#ifndef _WIN32
			for (;;)
			{
				const f64 left = std::chrono::duration<f64, std::milli>(nextFrameAt - std::chrono::steady_clock::now()).count();
				if (left <= 0.3) break;
				std::this_thread::sleep_for(std::chrono::duration<f64, std::milli>(left - 0.25));
			}
#endif
			while (std::chrono::steady_clock::now() < nextFrameAt) {}
			// (idle, not the frame's work: FrameProfiler's slow frames and work graph leave it out)
			FrameProfiler::NoteDisplayWait(waitMs);
		}
	}
#ifdef LUA_BINDINGS
	// Between frames, never inside one: the script that asked for the
	// switch is owned by the scene graph the switch tears down. The overlay
	// has the same problem - a button in the overlay calling hideOverlay()
	// is owned by the graph being deleted - so it is applied here too.
	ApplyPendingOverlayIfAny();
	ApplyPendingSceneLoadIfAny();
#endif
	if (!fillShaderCacheDir.empty()) StepFillShaderCache();
}

void PyrosPlayer::StepFillShaderCache()
{
	// (between frames, where a script's loadScene is carried out too)
	static std::vector<std::string> scenes;
	static bool listed = false;
	static size_t next = 0;
	static uint32 frames = 0;
	if (!listed)
	{
		listed = true;
		std::error_code ec;
		for (fs::directory_iterator it(fs::path(PlayerManifestInstance().root) / "scenes", ec), end; it != end && !ec; it.increment(ec))
		{
			const std::string name = it->path().filename().string();
			if (it->path().extension() != ".json" || name.find(".editor.json") != std::string::npos || name[0] == '_') continue;
			scenes.push_back("scenes/" + name);
		}
		std::sort(scenes.begin(), scenes.end());
		fprintf(stderr, "[shader cache] %zu scenes, into %s\n", scenes.size(), fillShaderCacheDir.c_str());
	}
	// (a scene drawn for 90 frames: what it lists is made at load, what its shadows, batches and
	// probes draw with in its first frames)
	if (++frames < 90) return;
	frames = 0;
	if (next < scenes.size())
	{
		fprintf(stderr, "[shader cache] %s\n", scenes[next].c_str());
		if (!LoadGameScene(scenes[next])) fprintf(stderr, "[shader cache] %s could not be loaded - left out\n", scenes[next].c_str());
		next++;
		return;
	}
	fprintf(stderr, "[shader cache] done\n");
	Close();
}

// Feeds the pointer to every canvas and routes a completed click to the Lua
// handler the button names. The canvas decides what is under the pointer -
// only it knows the draw order - and this only has to turn the answer into a
// call.
// One place a button's handler is called from, because a click and a pad
// press must not be able to behave differently.
// Every event a canvas reported this frame, to whatever named handler the
// element carries - see shared/UIDispatch.h, which the editor's play mode
// uses too so that a UI behaves the same in both.
void PyrosPlayer::DispatchUIEvents(UICanvas* canvas)
{
#ifdef LUA_BINDINGS
	uidispatch::Dispatch(canvas, lua, [](const std::string &m, void*) { echo(m); }, NULL);
#else
	(void)canvas;
#endif
}

// Characters the platform decoded, to whichever canvas has focus. Static
// because the window layer's hook is a plain function pointer - see
// TextInputHook.h.
void PyrosPlayer::OnTextTyped(const char* utf8)
{
	if (activePlayer && utf8 && activePlayer->console.IsOpen()) { activePlayer->console.Type(utf8); return; }
	if (!activePlayer || !activePlayer->scene || !utf8) return;
	std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(activePlayer->scene);
	for (size_t i = canvases.size(); i > 0; i--)
	{
		if (!canvases[i - 1]->GetFocused()) continue;
		canvases[i - 1]->UpdateText(utf8);
		activePlayer->DispatchUIEvents(canvases[i - 1]);
		return;
	}
}

void PyrosPlayer::OnMouseWheel(Event::Input::Info e)
{
	wheelDelta += (f32)e.Value;
}

void PyrosPlayer::DispatchUIClick(GameObject* clicked)
{
#ifdef LUA_BINDINGS
	if (!clicked) return;
	const std::vector<std::shared_ptr<IComponent> > &cs = clicked->GetComponents();
	for (size_t j = 0; j < cs.size(); j++)
	{
		if (!cs[j] || cs[j]->GetComponentType() != ComponentType::UIButton) continue;
		const std::string &handler = static_cast<UIButton*>(cs[j].get())->GetOnClick();
		if (handler.empty()) return;
		sol::protected_function fn = lua[handler];
		if (!fn.valid())
		{
			echo("WARNING: UIButton on '" + clicked->GetName() + "' wants '" + handler + "', which is not a global function");
			return;
		}
		sol::protected_function_result res = fn(clicked->GetName());
		if (!res.valid())
		{
			sol::error err = res;
			echo(std::string("ERROR: UIButton handler '") + handler + "' - " + err.what());
		}
		return;
	}
#else
	(void)clicked;
#endif
}

// A GameObject has no visibility of its own - "active" lives on components -
// so hiding a layer means disabling the RenderingComponents under it. Walks
// children rather than only the root, because a layer's content is its subtree.
void PyrosPlayer::SetSubtreeRenderingEnabled(GameObject* go, const bool on)
{
	if (go == NULL) return;
	const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
	for (size_t i = 0; i < comps.size(); i++)
	{
		if (!comps[i]) continue;
		const uint32 t = comps[i]->GetComponentType();
		if (t != ComponentType::RenderingComponent && t != ComponentType::RenderingInstancedComponent) continue;
		if (on) comps[i]->Enable(); else comps[i]->Disable();
	}
	const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
	for (size_t i = 0; i < kids.size(); i++)
		SetSubtreeRenderingEnabled(kids[i].get(), on);
}

void PyrosPlayer::ApplyLayerParallax()
{
	if (scene == NULL || activeCamera == NULL) return;
	const Vec3 cam = activeCamera->GetWorldPosition();
	const Vec2 scroll(cam.x, cam.y);

	std::vector<std::shared_ptr<GameObject> > &all = scene->GetAllGameObjectList();
	for (size_t i = 0; i < all.size(); i++)
	{
		if (!all[i]) continue;
		const std::vector<std::shared_ptr<IComponent> > &comps = all[i]->GetComponents();
		for (size_t c = 0; c < comps.size(); c++)
		{
			if (!comps[c] || comps[c]->GetComponentType() != ComponentType::Layer2D) continue;
			Layer2D* layer = static_cast<Layer2D*>(comps[c].get());
			// Hidden layers still get positioned: a layer toggled back on
			// mid-scroll must appear where it belongs, not where it was when
			// it was hidden.
			SetSubtreeRenderingEnabled(all[i].get(), layer->IsVisible());
			layer->ApplyParallax(scroll);
		}
	}
}

void PyrosPlayer::DispatchUIInput()
{
	// While an overlay is up it takes input exclusively. A pause menu that
	// let clicks through to the level under it would not be a pause menu, and
	// this is also what keeps the base scene's focus/hover state frozen where
	// the player left it rather than tracking a pointer it can no longer act
	// on. Same reasoning as UICanvas modality within one scene, one level up.
	std::vector<UICanvas*> canvases =
		UICanvas::GetCanvasesOnScene(overlayScene ? overlayScene : scene);
	if (canvases.empty()) return;

	// Keyboard and gamepad navigation, edge-triggered: held keys must not
	// walk the menu at the polling rate. A game that would rather drive this
	// itself can - ui.moveFocus/activateFocused are bound - but a menu that
	// needs a script before the arrow keys work is a menu that ships broken.
	{
		const Uint8* keys = SDL_GetKeyboardState(NULL);
		std::vector<UICanvas*> navCanvases = UICanvas::GetCanvasesOnScene(scene);

		// The focused widget sees the key first and can claim it. That is
		// what keeps arrow keys inside a text field or a list instead of
		// walking the focus out of them mid-edit, and it is the widget that
		// decides - the player cannot know which is which.
		struct Key { int scan; uint32 key; f32 dx, dy; };
		static const Key keyMap[] = {
			{ SDL_SCANCODE_LEFT,      UIKey::Left,      -1.f,  0.f },
			{ SDL_SCANCODE_RIGHT,     UIKey::Right,      1.f,  0.f },
			{ SDL_SCANCODE_UP,        UIKey::Up,         0.f, -1.f },
			{ SDL_SCANCODE_DOWN,      UIKey::Down,       0.f,  1.f },
			{ SDL_SCANCODE_BACKSPACE, UIKey::Backspace,  0.f,  0.f },
			{ SDL_SCANCODE_DELETE,    UIKey::Delete,     0.f,  0.f },
			{ SDL_SCANCODE_HOME,      UIKey::Home,       0.f,  0.f },
			{ SDL_SCANCODE_END,       UIKey::End,        0.f,  0.f },
			{ SDL_SCANCODE_ESCAPE,    UIKey::Escape,     0.f,  0.f },
			{ SDL_SCANCODE_TAB,       UIKey::Tab,        0.f,  0.f },
		};
		for (size_t n = 0; n < sizeof(keyMap) / sizeof(keyMap[0]); n++)
		{
			const bool downNow = keys[keyMap[n].scan] != 0;
			if (downNow && !navKeyWasDown[n])
			{
				bool claimed = false;
				for (size_t i = navCanvases.size(); i > 0 && !claimed; i--)
				{
					claimed = navCanvases[i - 1]->UpdateKey(keyMap[n].key);
					DispatchUIEvents(navCanvases[i - 1]);
				}
				// Unclaimed and it points somewhere: walk the focus.
				if (!claimed && (keyMap[n].dx != 0.f || keyMap[n].dy != 0.f))
					for (size_t i = navCanvases.size(); i > 0; i--)
						if (navCanvases[i - 1]->MoveFocus(Vec2(keyMap[n].dx, keyMap[n].dy))) break;
				// Tab walks forward through whatever is focusable.
				if (!claimed && keyMap[n].key == UIKey::Tab)
					for (size_t i = navCanvases.size(); i > 0; i--)
						if (navCanvases[i - 1]->MoveFocus(Vec2(0.f, 1.f))) break;
			}
			navKeyWasDown[n] = downNow;
		}

		const bool activateNow = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER]
			|| keys[SDL_SCANCODE_SPACE];
		if (activateNow && !navActivateWasDown)
		{
			// Enter goes to the focused widget first, for the same reason:
			// in a text field it submits rather than pressing a button
			// somewhere else on the screen.
			bool claimed = false;
			for (size_t i = navCanvases.size(); i > 0 && !claimed; i--)
			{
				claimed = navCanvases[i - 1]->UpdateKey(UIKey::Enter);
				DispatchUIEvents(navCanvases[i - 1]);
			}
			if (!claimed)
				for (size_t i = navCanvases.size(); i > 0; i--)
					if (GameObject* go = navCanvases[i - 1]->ActivateFocused())
					{
						DispatchUIEvents(navCanvases[i - 1]);
						DispatchUIClick(go);
						break;
					}
		}
		navActivateWasDown = activateNow;
	}

	int mx = 0, my = 0;
	const Uint32 buttons = SDL_GetMouseState(&mx, &my);
	// (SDL's pointer is in the window's points, Width/Height the drawable's pixels: one to one on a
	// Mac's 1x window, three to one on an iPhone - where a tap on a menu landed a third of the way to it)
	{
		int pw = 0, ph = 0;
		SDL_GetWindowSize(GetSDLWindow(), &pw, &ph);
		if (pw > 0 && ph > 0) { mx = (int)((f32)mx * (f32)Width / (f32)pw); my = (int)((f32)my * (f32)Height / (f32)ph); }
	}
	const bool down = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
	const bool insideWindow = (mx >= 0 && my >= 0 && (uint32)mx < Width && (uint32)my < Height);

	// Topmost canvas first: a pause menu over a HUD must swallow the click
	// rather than let both act on it.
	// The wheel goes to whatever is under the pointer, before the click
	// does - a list scrolls without needing to be clicked first.
	if (wheelDelta != 0.f)
	{
		for (size_t i = canvases.size(); i > 0; i--)
		{
			UICanvas* c = canvases[i - 1];
			const UIRectValue &r = c->GetCanvasRect();
			if (r.width <= 0.f || r.height <= 0.f) continue;
			const Vec2 p((f32)mx / (f32)Width * r.width, (f32)my / (f32)Height * r.height);
			c->UpdateScroll(p, wheelDelta);
			DispatchUIEvents(c);
		}
		wheelDelta = 0.f;
	}

	for (size_t i = canvases.size(); i > 0; i--)
	{
		UICanvas* c = canvases[i - 1];
		const UIRectValue &r = c->GetCanvasRect();
		if (r.width <= 0.f || r.height <= 0.f) continue;
		const Vec2 p((f32)mx / (f32)Width * r.width, (f32)my / (f32)Height * r.height);
		GameObject* clicked = c->UpdateInput(p, down, insideWindow);
		// Every event, not just the click: a slider dragged and a row
		// picked are not clicks, and both have handlers.
		DispatchUIEvents(c);
		if (clicked) return;
	}
}

void PyrosPlayer::OnResize(const uint32 width, const uint32 height)
{
	ClassName::OnResize(width, height);
	// Recorded, not applied. This runs from SDL event handling, which is not
	// a safe place to destroy and recreate GPU images: the previous frame is
	// routinely still executing (the frame fence is only waited on at the
	// top of the next BeginFrame), and Texture::Resize()/FrameBuffer::
	// Resize() do no synchronisation of their own - they reallocate
	// immediately. Doing it here meant the GPU carried on reading G-buffer
	// attachments that had just been freed and reallocated underneath it,
	// which is exactly what the blocks of white and tiled colour noise after
	// a window resize were: undefined image memory, sampled.
	//
	// A drag-resize also delivers a stream of these events, so applying each
	// one would reallocate five images and an FBO per event rather than once
	// at the size the user settled on.
	pendingResizeWidth = width;
	pendingResizeHeight = height;
	resizePending = true;
}

void PyrosPlayer::ApplyPendingResizeIfAny()
{
	if (!resizePending) return;
	resizePending = false;

	// (the renderer's targets follow the size it is given each frame)
	if (pendingResizeWidth == 0 || pendingResizeHeight == 0) return; // minimised
	ApplyProjection();
}

void PyrosPlayer::Shutdown()
{
	PyrosTextInput::SetHandler(NULL);
	InputManager::RemoveEvent(Event::Type::OnMove, Event::Input::Mouse::Wheel, this, &PyrosPlayer::OnMouseWheel);
	activePlayer = NULL;

	UnloadGameScene();
#ifdef LUA_BINDINGS
	// The game's own globals go, and what they held with them, while the
	// device is still there: a game keeps its world in a global, and that
	// world's models and textures were otherwise never freed at all (1810
	// objects reported leaked by vkDestroyDevice on a 4 km map).
	if (!engineGlobals.empty())
	{
		std::vector<std::string> made;
		for (const auto &kv : lua.globals())
			if (kv.first.is<std::string>() && engineGlobals.count(kv.first.as<std::string>()) == 0)
				made.push_back(kv.first.as<std::string>());
		for (size_t i = 0; i < made.size(); i++)
			lua[made[i]] = sol::lua_nil;
		lua.collect_garbage();
		lua.collect_garbage();
	}
#endif

	delete physics; physics = NULL;
	delete physics2D; physics2D = NULL;
	delete overlayScene; overlayScene = NULL;
	delete scene; scene = NULL;
	delete audio; audio = NULL;

	// (the scene's light, then the renderer, before the window's device goes)
	ddgi.reset(); giRays.reset();
	renderer.reset();
	ClassName::Shutdown();
}
