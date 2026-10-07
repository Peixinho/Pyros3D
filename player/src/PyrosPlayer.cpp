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
#include <Pyros3D/Assets/Renderable/Terrains/TerrainEditor.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <Pyros3D/Assets/AssetPreload.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include "PyrosPlayer.h"
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
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include "PrefabResolver.h"
// UI style/palette files, resolved by the same header the editor uses
// (shared/UIStyleResolver.h) so a themed UI looks identical in both.
#include "UIStyleResolver.h"

#include <SDL2/SDL.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <Pyros3D/Rendering/Components/Layer2D/Layer2D.h>
#include <Pyros3D/Rendering/Components/Occluder2D/Occluder2D.h>
#ifdef LUA_BINDINGS
#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
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

		// The engine loads its shaders from "shaders/PyrosShader.glsl" -
		// relative to the *working directory*, not to the executable - and
		// scene files reference their assets project-relative. Making the
		// game folder the working directory is what lets both resolve when
		// the game is launched from anywhere (Finder, a shortcut, a
		// launcher), which is every way a player will actually start it.
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
		m.serverPublicKey = j.value("serverPublicKey", std::string());
		m.deferred = (j.value("renderer", std::string("forward")) == "deferred");
		if (j.contains("antiAliasing") && !AntiAliasing::FromString(j.value("antiAliasing", std::string()), m.antiAliasing))
			echo("WARNING: game.json has an unknown \"antiAliasing\" - running without anti-aliasing");
		m.width = j.value("width", m.width);
		m.height = j.value("height", m.height);
		m.fullscreen = j.value("fullscreen", false);
		m.renderScale = j.value("renderScale", 1.f);
		if (j.contains("quality") && j["quality"].is_object())
		{
			const json &q = j["quality"];
			m.qSmallCull = q.value("smallObjectCull", -1.f);
			m.qShadowEvery = q.value("shadowUpdateInterval", -1);
			if (q.contains("ssaoHalfResolution")) m.qSsaoHalf = q.value("ssaoHalfResolution", false) ? 1 : 0;
			m.qRenderScale = q.value("renderScale", -1.f);
			// "upscaleSharpness": 0..1, how sharp a frame rendered below the window's
			// size is brought up to it; negative for a plain stretch
			m.qUpscaleSharpness = q.value("upscaleSharpness", 0.85f);
			// "upscaler": "fsr1" (AMD FSR 1) or "sharp" (the built-in filter: one pass, far cheaper)
			m.qUpscaler = q.value("upscaler", std::string("fsr1"));
			// "upscaleQuality": "native" | "quality" | "balanced" | "performance" | "auto".
			// Left out: renderScale / autoRenderScaleFps below say it, as before.
			m.qUpscaleQuality = q.value("upscaleQuality", std::string());
			m.qAutoFps = q.value("autoRenderScaleFps", -1.f);
			m.qAutoMin = q.value("autoRenderScaleMin", 0.42f);
			m.qFrameLimit = q.value("frameRateLimit", -2.f);
		}
		if (j.contains("preload") && j["preload"].is_array())
			for (const auto &e : j["preload"]) if (e.is_string()) m.preload.push_back(e.get<std::string>());
		if (j.contains("background") && j["background"].is_array() && j["background"].size() >= 3)
			m.background = Vec4(j["background"][0].get<f32>(), j["background"][1].get<f32>(),
				j["background"][2].get<f32>(), j["background"].size() > 3 ? j["background"][3].get<f32>() : 1.f);

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
	renderer = NULL;
	uiRenderer = NULL;
	audio = NULL;
	gbufferFBO = NULL;
	gbufferDepth = gbufferAlbedo = gbufferSpecular = gbufferNormal = gbufferMatRough = NULL;
	activeCamera = NULL;
	resizePending = false;
	pendingResizeWidth = pendingResizeHeight = 0;
	effectsManager = NULL;
	antiAliasingMode = PlayerManifestInstance().antiAliasing;
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

// The deferred renderer renders into a G-buffer the caller owns - it does
// not build one. Same five attachments, same formats, as the editor's
// viewport (SceneEditor::BuildGBuffer): the two RGBA16F targets carry the
// additive ambient+emissive term in their alpha, which an 8-bit UNORM would
// clamp at 1.0 and silently flatten every emissive material.
void PyrosPlayer::BuildGBuffer(uint32 width, uint32 height)
{
	gbufferDepth = new Texture();
	gbufferDepth->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, width, height, false);
	gbufferDepth->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	// Nearest, not CreateEmptyTexture's default Linear. This is the
	// depth every secondpass*.glsl samples as tDepth, and a
	// Linear-filtered depth texture is not filterable: WebGL2 calls the
	// texture incomplete and every sample returns 0, Apple's GL calls it
	// "unloadable" and substitutes the zero texture. tDepth then reads 0
	// everywhere, `if (tDepth >= 1.0) discard` never rejects the sky, and
	// the ambient pass plus every light shade the whole screen - the
	// white/washed-out deferred viewport in the browser build. Same rule
	// as PostEffectsManager::Init() and VelocityRenderer, which document
	// it from the effect side.
	gbufferDepth->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);

	gbufferAlbedo = new Texture();
	gbufferAlbedo->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, width, height, false);
	gbufferAlbedo->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

	gbufferSpecular = new Texture();
	gbufferSpecular->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, width, height, false);
	gbufferSpecular->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

	gbufferNormal = new Texture();
	gbufferNormal->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, width, height, false);
	gbufferNormal->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

	gbufferMatRough = new Texture();
	gbufferMatRough->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, width, height, false);
	gbufferMatRough->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

	gbufferFBO = new FrameBuffer();
	gbufferFBO->SetDebugName("Player G-buffer");
	gbufferFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, gbufferDepth);
	gbufferFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, gbufferAlbedo);
	gbufferFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment1, TextureType::Texture, gbufferSpecular);
	gbufferFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment2, TextureType::Texture, gbufferNormal);
	gbufferFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment3, TextureType::Texture, gbufferMatRough);
}

void PyrosPlayer::DestroyGBuffer()
{
	delete gbufferFBO; gbufferFBO = NULL;
	delete gbufferDepth; gbufferDepth = NULL;
	delete gbufferAlbedo; gbufferAlbedo = NULL;
	delete gbufferSpecular; gbufferSpecular = NULL;
	delete gbufferNormal; gbufferNormal = NULL;
	delete gbufferMatRough; gbufferMatRough = NULL;
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

	// A 2D scene is always forward, whatever the manifest asked for.
	//
	// Not a preference - deferred cannot draw it. Sprites are alpha-blended
	// quads and a deferred renderer cannot blend into a G-buffer at all
	// (which is the entire reason ShaderUsage::AlphaTest exists), so a 2D
	// scene rendered deferred loses its blending. And ShaderUsage::Lighting2D
	// is a material flag compiled into the forward shader: the deferred
	// lighting passes shade whatever is in the G-buffer with no idea which
	// material wrote each pixel, so sprites would be lit with the N.L term
	// that Lighting2D exists to remove and would come back dark. There is no
	// spare G-buffer channel to mark them with either - FragData_pbr already
	// carries roughness, metallic, SSR and reflectivity.
	//
	// Read before the renderer is built, which is before the scene is loaded,
	// so this peeks at the startup scene's flag rather than waiting for it.
	bool sceneWantsForward = false;
	if (m.deferred && StartupSceneIsTwoD(m))
	{
		sceneWantsForward = true;
		echo("Startup scene is 2D - using the forward renderer (deferred cannot blend sprites)");
	}

	renderScale = m.renderScale < 0.25f ? 0.25f : (m.renderScale > 1.f ? 1.f : m.renderScale);
	if (m.deferred && !sceneWantsForward)
	{
		BuildGBuffer(RenderWidth(), RenderHeight());
		renderer = new DeferredRenderer(RenderWidth(), RenderHeight(), gbufferFBO);
	}
	else
		renderer = new ForwardRenderer(RenderWidth(), RenderHeight());

	// Independent of that choice: it composites over whatever the frame
	// already drew.
	uiRenderer = new UIRenderer(Width, Height);

	renderer->SetViewPort(0, 0, RenderWidth(), RenderHeight());
	// Asserted explicitly, never left to the default. The clear colour is
	// device-global state that outlives any one renderer (see
	// IRenderer::DrawBackground), so "whatever it happened to be" is not a
	// value - it is whichever renderer wrote it last. The editor sets its own
	// 0.2 grey for exactly this reason; a game gets black unless game.json
	// says otherwise.
	renderer->SetBackground(m.background);

#ifdef LUA_BINDINGS
	GenerateBindings(&lua);
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
	lua.set_function("setMouseCaptured", [this](bool captured) {
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
	});
	lua.set_function("isMouseCaptured", []() { return SDL_GetRelativeMouseMode() == SDL_TRUE; });
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
	// The fraction of the window's size the scene is rendered at, 0.25 to 1
	// (the UI is always at the window's own). A game's graphics setting, and
	// what keeps a deferred frame affordable on a high-resolution screen.
	lua.set_function("setRenderScale", [this](const f32 scale) { SetRenderScale(scale); });
	lua.set_function("getRenderScale", [this]() { return GetRenderScale(); });
	// setSunLaysAmbient(false): the ambient light in a lighting pass of its own again (to compare)
	// setNearestFirst(false): the G-buffer in the scene's own order again (to compare)
	lua.set_function("setNearestFirst", [](const bool on) { DeferredRenderer::SetNearestFirst(on); });
	// setFoliageThinning(false): every plant of a field drawn, however far (see FoliageLayerSpec::thinDensity)
	lua.set_function("setFoliageThinning", [](const bool on) { FoliageComponent::SetThinning(on); });
	lua.set_function("getFoliageThinning", []() { return FoliageComponent::GetThinning(); });
	lua.set_function("setSunLaysAmbient", [](const bool on) { DeferredRenderer::SetSunLaysAmbient(on); });
	// setUpscaleSharpness(0.6): how sharp a frame rendered below the window's size is
	// brought up to it (0..1); negative for a plain stretch
	// The upscaler and how far below the window the scene is rendered (see
	// Upscaling.h) - what a game's options menu is made of:
	//   getSupportedUpscalers()      names this machine can run, e.g. { "off", "sharp", "fsr1" }
	//   getUpscalerLabel(name)       what to show for one ("AMD FSR 1")
	//   setUpscaler(name)            asks for one; returns the name of what runs
	//   getUpscaler()                what runs;  getRequestedUpscaler()  what was asked for
	//   getUpscaleQualities(), getUpscaleQualityLabel(name), setUpscaleQuality(name), getUpscaleQuality()
	lua.set_function("getSupportedUpscalers", [this]() {
		std::vector<std::string> names;
		const std::vector<UpscalerMode> modes = Upscaling::Supported();
		for (size_t i = 0; i < modes.size(); i++) names.push_back(Upscaling::ToString(modes[i]));
		return sol::as_table(names);
	});
	lua.set_function("getUpscalerLabel", [](const std::string &name) {
		UpscalerMode mode = UpscalerMode::Sharp;
		return std::string(Upscaling::FromString(name, mode) ? Upscaling::DisplayName(mode) : name.c_str());
	});
	lua.set_function("setUpscaler", [this](const std::string &name) {
		UpscalerMode mode = upscaler;
		if (!Upscaling::FromString(name, mode)) echo("setUpscaler: no upscaler is called \"" + name + "\"");
		upscaler = mode;
		ApplyUpscaler();
		return Upscaling::ToString(Upscaling::Resolve(upscaler));
	});
	lua.set_function("getUpscaler", [this]() { return Upscaling::ToString(effectsManager ? effectsManager->GetEffectiveUpscaler() : Upscaling::Resolve(upscaler)); });
	lua.set_function("getRequestedUpscaler", [this]() { return Upscaling::ToString(upscaler); });
	lua.set_function("getUpscaleQualities", []() {
		std::vector<std::string> names;
		const std::vector<UpscaleQuality> &all = Upscaling::AllQualities();
		for (size_t i = 0; i < all.size(); i++) names.push_back(Upscaling::ToString(all[i]));
		return sol::as_table(names);
	});
	lua.set_function("getUpscaleQualityLabel", [](const std::string &name) {
		UpscaleQuality q = UpscaleQuality::Auto;
		return std::string(Upscaling::FromString(name, q) ? Upscaling::DisplayName(q) : name.c_str());
	});
	lua.set_function("setUpscaleQuality", [this](const std::string &name) {
		UpscaleQuality q = upscaleQuality;
		if (!Upscaling::FromString(name, q)) echo("setUpscaleQuality: no quality is called \"" + name + "\"");
		upscaleQuality = q;
		ApplyUpscaleQuality();
	});
	lua.set_function("getUpscaleQuality", [this]() { return Upscaling::ToString(upscaleQuality); });
	// setUpscaleSharpness(0.85): how sharp (0..1)
	lua.set_function("setUpscaleSharpness", [this](const f32 s) { if (s < 0.f) upscaler = UpscalerMode::Off; else upscaleSharpness = s; ApplyUpscaler(); });
	// setFrameRateLimit(fps): no more frames a second than that (0: as many
	// as there are). getDisplayRefreshRate(): what the screen the window is on
	// can show, or 0 when it will not say.
	lua.set_function("setFrameRateLimit", [this](const f32 fps) { frameRateLimit = fps < 0.f ? 0.f : fps; });
	lua.set_function("getDisplayRefreshRate", [this]() {
		SDL_DisplayMode mode;
		const int display = GetSDLWindow() ? SDL_GetWindowDisplayIndex(GetSDLWindow()) : 0;
		if (SDL_GetCurrentDisplayMode(display < 0 ? 0 : display, &mode) != 0) return 0;
		return mode.refresh_rate;
	});
	// setAutoRenderScale(fps, lowest, highest): the scale is moved by itself to
	// hold that frame rate where it is the GPU that cannot (0 fps turns it off
	// and leaves the scale where it is).
	lua.set_function("setAutoRenderScale", [this](const f32 fps, sol::optional<f32> lo, sol::optional<f32> hi) {
		SetAutoRenderScale(fps, lo ? *lo : 0.4f, hi ? *hi : 1.f);
	});
	lua.set_function("getRenderSize", [this]() { return std::make_tuple((int)RenderWidth(), (int)RenderHeight()); });
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
	// Settings that outlive the game: a folder of this player's own, per game.
	{
		const std::string title = PlayerManifestInstance().title.empty() ? std::string("Game") : PlayerManifestInstance().title;
		char* pref = SDL_GetPrefPath("Pyros3D", title.c_str());
		GenerateStoreBindings(&lua, pref ? std::string(pref) : std::string("."));
		if (pref) SDL_free(pref);
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
	if (m.qSmallCull >= 0.f) IRenderer::SetSmallObjectCull(m.qSmallCull);
	if (m.qShadowEvery >= 1) IRenderer::SetShadowUpdateInterval((uint32)m.qShadowEvery);
	if (m.qSsaoHalf >= 0) DeferredRenderer::SetSSAOHalfResolution(m.qSsaoHalf == 1);
	upscaleSharpness = m.qUpscaleSharpness < 0.f ? 0.85f : m.qUpscaleSharpness;
	if (!Upscaling::FromString(m.qUpscaler, upscaler)) upscaler = UpscalerMode::FSR1;
	if (m.qUpscaleSharpness < 0.f) upscaler = UpscalerMode::Off;        // (how "a plain stretch" used to be said)
	{
		const std::string why = Upscaling::FallbackReason(upscaler);
		if (!why.empty()) echo(why);
	}
	autoFpsWanted = m.qAutoFps > 0.f ? m.qAutoFps : 60.f;
	autoMinWanted = m.qAutoMin;
	if (!m.qUpscaleQuality.empty() && Upscaling::FromString(m.qUpscaleQuality, upscaleQuality))
		ApplyUpscaleQuality();
	else
	{
		// as a project said it before there were names for it
		if (m.qRenderScale > 0.f) SetRenderScale(m.qRenderScale);
		if (m.qAutoFps >= 0.f) SetAutoRenderScale(m.qAutoFps, m.qAutoMin, 1.f);
		upscaleQuality = m.qAutoFps > 0.f ? UpscaleQuality::Auto
			: (m.qRenderScale > 0.f && m.qRenderScale < 0.999f ? UpscaleQuality::Quality : UpscaleQuality::Native);
	}
	if (m.qFrameLimit > -1.5f)
	{
		f32 limit = m.qFrameLimit;
		if (limit < 0.f)
		{
			// the display's own rate, where that is an ordinary one
			SDL_DisplayMode mode;
			const int display = GetSDLWindow() ? SDL_GetWindowDisplayIndex(GetSDLWindow()) : 0;
			limit = (SDL_GetCurrentDisplayMode(display < 0 ? 0 : display, &mode) == 0 && mode.refresh_rate >= 50 && mode.refresh_rate <= 75) ? (f32)mode.refresh_rate : 0.f;
		}
		frameRateLimit = limit;
	}

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

// Just the twoD flag, without loading anything. The renderer has to be chosen
// before the scene is loaded, so the alternative would be building the wrong
// one and swapping it a moment later.
bool PyrosPlayer::StartupSceneIsTwoD(const PlayerManifest& m)
{
	if (m.startupScene.empty()) return false;
	const std::string abs = (fs::path(m.root) / m.startupScene).string();
	std::ifstream f(abs);
	if (!f.good()) return false;
	try
	{
		nlohmann::json j;
		f >> j;
		return j.value("twoD", false);
	}
	catch (...)
	{
		// A scene this cannot parse is one LoadGameScene is about to complain
		// about properly; defaulting to the manifest's choice is right here.
		return false;
	}
}

bool PyrosPlayer::ReadPostEffectAsset(const std::string& path, std::string& sourceOut, void* user)
{
	PyrosPlayer* self = (PyrosPlayer*)user;
	if (!self || path.empty()) return false;
	std::ifstream in(self->ResolvePath(path).c_str(), std::ios::binary);
	if (!in) return false;
	std::ostringstream ss;
	ss << in.rdbuf();
	sourceOut = ss.str();
	return true;
}

bool PyrosPlayer::HavePostEffects()
{
	if (antiAliasingMode != AntiAliasingMode::Off || renderScale < 0.999f)
		EnsureEffectsManager();
	if (effectsManager == NULL)
		return false;
	effectsManager->SetAntiAliasing(antiAliasingMode, gbufferFBO != NULL);
	// (rendered smaller than the window, something has to carry the frame to
	// it: the chain's last pass does, so there is always a chain to run)
	return effectsManager->NeedsCapture() || renderScale < 0.999f;
}

PostEffectsManager* PyrosPlayer::EnsureEffectsManager()
{
	if (effectsManager == NULL)
		effectsManager = new PostEffectsManager(RenderWidth(), RenderHeight());
	// (the chain works at the size the scene is rendered at, and its last pass
	// fills the window: see SetRenderScale)
	effectsManager->SetOutputSize(Width, Height);
	ApplyUpscaler();
	return effectsManager;
}

void PyrosPlayer::ApplyUpscaler()
{
	if (effectsManager == NULL) return;
	effectsManager->SetSharpUpscale(Upscaling::Resolve(upscaler) != UpscalerMode::Off, upscaleSharpness);
	effectsManager->SetUpscaler(upscaler);
}

void PyrosPlayer::ApplyUpscaleQuality()
{
	if (upscaleQuality == UpscaleQuality::Auto)
		SetAutoRenderScale(autoFpsWanted, autoMinWanted, 1.f);
	else
	{
		SetAutoRenderScale(0.f, autoMinWanted, 1.f);
		SetRenderScale(Upscaling::Scale(upscaleQuality));
	}
}

// Called after every scene load, including a mid-game loadScene(): the chain
// belongs to the scene, so switching scenes switches chains.
void PyrosPlayer::BuildPostEffectChain()
{
	if (meta.postEffects.empty())
	{
		// Nothing to run. Drop the chain rather than leave the previous
		// scene's effects on screen, but keep the manager - rebuilding it per
		// scene would throw away its capture textures for no reason.
		if (effectsManager) effectsManager->RemoveAllEffects();
		// SSAO on a deferred renderer is the renderer's, not the manager's.
		if (DeferredRenderer* deferred = dynamic_cast<DeferredRenderer*>(renderer))
			deferred->DisableSSAO();
		return;
	}
	EnsureEffectsManager();
	PostEffectChain::Build(*effectsManager, meta.postEffects, RenderWidth(), RenderHeight(),
		&PyrosPlayer::ReadPostEffectAsset, this, dynamic_cast<DeferredRenderer*>(renderer));
}

bool PyrosPlayer::LoadGameScene(const std::string& sceneRel)
{
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
	// The scene's chain, before anything renders with it.
	BuildPostEffectChain();
	// TAA's history belongs to the scene that just went away.
	if (effectsManager) effectsManager->ResetTemporalHistory();

	// The terrain's shadow, where the scene asks for it baked: worked out here,
	// as part of loading the map (see TerrainHorizon).
	scene->SetTerrainHorizon(std::shared_ptr<TerrainHorizon>());
	// (A terrain grows its tiles on the scene's first updates, not while the
	// file is read: there is nothing to bake from yet. Update() does it as
	// soon as the tiles are there - it used to be asked for here, found no
	// terrain and quietly made nothing, so a built game drew its whole
	// terrain into the shadow map every frame after all.)
	terrainBakePending = meta.terrainShadowsBaked;
	terrainBakeTiles = 0; terrainBakeStable = 0; terrainBakeWaited = 0;

	renderer->SetGlobalLight(Vec4(meta.ambientLight.x * meta.ambientIntensity,
								  meta.ambientLight.y * meta.ambientIntensity,
								  meta.ambientLight.z * meta.ambientIntensity,
								  meta.ambientLight.w));
	renderer->SetBackground(meta.background);
	renderer->SetAmbientMode(meta.ambientMode);
	{
		const f32 k = meta.ambientIntensity;
		renderer->SetAmbientGradient(Vec4(meta.ambientSky.x*k, meta.ambientSky.y*k, meta.ambientSky.z*k, 1.f),
									 Vec4(meta.ambientEquator.x*k, meta.ambientEquator.y*k, meta.ambientEquator.z*k, 1.f),
									 Vec4(meta.ambientGround.x*k, meta.ambientGround.y*k, meta.ambientGround.z*k, 1.f));
	}
	{
		// SH and the probe grid, which a build never applied before
		// this: a scene authored in Environment (SH) mode came up with
		// the mode set and no coefficients behind it, so it sampled as
		// black and looked like the ambient had been lost.
		//
		// Scaled by ambientIntensity like every other source, so the
		// slider means the same thing whichever one is selected. SH is
		// linear, so scaling coefficients scales the reconstruction.
		const f32 k = meta.ambientIntensity;
		SphericalHarmonicsL2 sh;
		for (uint32 i = 0; i < SphericalHarmonicsL2::kCoefficientCount; i++)
			sh.coefficients[i] = Vec3(meta.ambientSH[i].x * k, meta.ambientSH[i].y * k,
									  meta.ambientSH[i].z * k);
		renderer->SetAmbientSH(sh);
		renderer->SetAmbientProbeGrid(meta.ambientProbes.IsValid() ? &meta.ambientProbes : NULL);
	}

	// A DDGI scene carries its settings, not its solution - solve it
	// here, once, before the first frame. See SceneMeta::ddgiCounts.
	ddgiActive = false;
	if (meta.ambientMode == 3)
	{
		SceneGISettings gi;
		gi.enabled = true;
		for (uint32 i = 0; i < 3; i++) gi.counts[i] = meta.ddgiCounts[i];
		gi.raysPerProbe = meta.ddgiRaysPerProbe;
		gi.passes = meta.ddgiPasses;
		gi.skyColor = Vec3(meta.ddgiSky.x, meta.ddgiSky.y, meta.ddgiSky.z);
		// Relative to the game directory, where build_game puts the
		// shaders next to the binary - the same place the player's own
		// materials come from.
		gi.shaderRoot = "shaders";
		if (renderer->BakeGlobalIllumination(scene, gi))
		{
			ddgiActive = true;
			// Said out loud because the difference between GPU and CPU
			// tracing here is two orders of magnitude, and a build that
			// quietly fell back to the CPU path looks like a build that
			// is simply slow.
			echo(renderer->IsGlobalIlluminationOnGPU()
				? "Global illumination solved (GPU tracing)."
				: "Global illumination solved (CPU tracing - no compute on this backend).");
		}
		else
		{
			// Better a scene lit flatly than one lit by an ambient mode
			// with nothing behind it, which samples black everywhere.
			echo("WARNING: global illumination could not be solved - falling back to flat ambient.");
			renderer->SetAmbientMode(0);
		}
	}

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
			if (sceneMainScript) sceneMainScript->Init();
		}
		catch (const std::exception& e)
		{
			echo(std::string("ERROR: scene main script - ") + e.what());
		}
	}
#endif

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

	// Follow, then clamp, then place. In that order: clamping a position the
	// follow has not produced yet would fight it every frame.
	if (!meta.view2D.follow.empty() && scene)
	{
		std::vector<std::shared_ptr<GameObject>>& all = scene->GetAllGameObjectList();
		for (size_t i = 0; i < all.size(); ++i)
			if (all[i] && all[i]->GetName() == meta.view2D.follow)
			{
				meta.view2D.Track(all[i]->GetWorldPosition(), dt);
				break;
			}
	}
	const f32 aspect = (Height > 0) ? ((f32)Width / (f32)Height) : 1.f;
	meta.view2D.ClampCenter(aspect);

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
		projection.Perspective(cameraFov, aspect, cameraNear, cameraFar);
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
	// Accepted and ignored: in the editor this redirects the *viewport* to a
	// different camera, and a script written against play mode will call it.
	// Here the render camera is whatever the scene says, so honouring it is
	// exactly right - it is the same thing.
	lua["setRenderCamera"] = [this](GameObject* go) {
		if (!go) return;
		// A cut - see ResetTemporalHistory().
		if (go != activeCamera && effectsManager) effectsManager->ResetTemporalHistory();
		activeCamera = go;
	};

	// Anti-aliasing at run time, for an options menu. Takes effect next
	// frame. The mode is what was asked for; getEffectiveAntiAliasing() is
	// what runs after the renderer/device fallbacks, and
	// getSupportedAntiAliasing() is what a menu should offer.
	lua["setAntiAliasing"] = [this](const std::string& name) {
		AntiAliasingMode mode;
		if (!AntiAliasing::FromString(name, mode))
		{
			echo("ERROR: setAntiAliasing: unknown mode \"" + name + "\" (off, fxaa, smaa, taa, msaa2x, msaa4x, msaa8x)");
			return false;
		}
		antiAliasingMode = mode;
		return true;
	};
	lua["getAntiAliasing"] = [this]() { return AntiAliasing::ToString(antiAliasingMode); };
	lua["getEffectiveAntiAliasing"] = [this]() {
		return AntiAliasing::ToString(AntiAliasing::Resolve(antiAliasingMode, gbufferFBO != NULL,
			GetActiveRenderDevice().GetMaxSamples()));
	};
	lua["getSupportedAntiAliasing"] = [this](sol::this_state s) {
		sol::state_view view(s);
		sol::table t = view.create_table();
		const std::vector<AntiAliasingMode> modes = AntiAliasing::Supported(gbufferFBO != NULL, GetActiveRenderDevice().GetMaxSamples());
		for (size_t i = 0; i < modes.size(); i++)
			t[i + 1] = AntiAliasing::ToString(modes[i]);
		return t;
	};
	lua["loadScene"] = [this](const std::string& name) { pendingLoadSceneName = name; };

	// The scene's own 2D view, as a table of plain functions rather than a
	// usertype: it is a handful of numbers on the scene, not an object with a
	// lifetime, and a script wanting "move the camera left" should not have to
	// find out what owns it.
	//
	// Writing any of these turns the view on. A script that positions the view
	// has said, unambiguously, that the scene is framed by it.
	{
		sol::table v = lua.create_table();
		v["setCenter"] = [this](f32 x, f32 y) {
			meta.view2D.enabled = true;
			meta.view2D.center = Vec2(x, y);
		};
		v["center"] = [this]() {
			return std::make_tuple(meta.view2D.center.x, meta.view2D.center.y);
		};
		v["setZoom"] = [this](f32 halfHeight) {
			meta.view2D.enabled = true;
			if (halfHeight > 0.0001f) meta.view2D.halfHeight = halfHeight;
			// The projection is rebuilt from this, and nothing else will ask
			// for it until the window resizes.
			ApplyProjection();
		};
		v["zoom"] = [this]() { return meta.view2D.halfHeight; };
		// By NAME, matching how the editor stores it. An empty name is a
		// fixed view, which is how you stop following something.
		v["follow"] = [this](const std::string& name, sol::optional<f32> lag) {
			meta.view2D.enabled = true;
			meta.view2D.follow = name;
			if (lag) meta.view2D.followLag = *lag;
		};
		v["setFollowOffset"] = [this](f32 x, f32 y) { meta.view2D.followOffset = Vec2(x, y); };
		// Which axes the follow moves. followAxes(true, false) is the
		// side-scroller default: track the character across the level, leave
		// the horizon where it is.
		v["followAxes"] = [this](bool x, bool y) {
			meta.view2D.followX = x;
			meta.view2D.followY = y;
		};
		v["setBounds"] = [this](f32 minX, f32 minY, f32 maxX, f32 maxY) {
			meta.view2D.enabled = true;
			meta.view2D.clamp = true;
			meta.view2D.clampMin = Vec2(minX, minY);
			meta.view2D.clampMax = Vec2(maxX, maxY);
		};
		v["clearBounds"] = [this]() { meta.view2D.clamp = false; };
		lua["view"] = v;
	}
	// Show a 2D scene over whatever is running, and take it down again. The
	// scene shown is an ordinary scene file - usually one marked twoD - so a
	// pause menu can also be opened on its own with loadScene().
	lua["showOverlay"] = [this](const std::string& name) { ShowOverlayScene(name); };
	lua["hideOverlay"] = [this]() { HideOverlayScene(); };
	lua["overlayScene"] = [this]() { return fs::path(overlaySceneRel).stem().string(); };
	lua["currentScene"] = [this]() { return fs::path(currentSceneRel).stem().string(); };
	lua["echo"] = [](const std::string& msg) { p3d::LOG::_LOG::_echo(msg); };
	lua["editorRendererType"] = PlayerManifestInstance().deferred ? std::string("deferred") : std::string("forward");
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

void PyrosPlayer::ApplyPendingSceneLoadIfAny()
{
	if (pendingLoadSceneName.empty()) return;
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
	if (!LoadGameScene(rel))
		echo("ERROR: loadScene(\"" + requested + "\") failed");
}
#endif

void PyrosPlayer::Update()
{
	if (!sceneLoaded) return;

	if (terrainBakePending && scene)
	{
		// when the terrain's tiles have all arrived (the same number for a few
		// frames running), or it has been long enough
		const size_t tiles = TerrainEditor::FindTiles(scene).size();
		if (tiles > 0 && tiles == terrainBakeTiles) terrainBakeStable++;
		else { terrainBakeTiles = tiles; terrainBakeStable = 0; }
		if (tiles > 0 && (terrainBakeStable >= 3 || ++terrainBakeWaited > 240))
		{
			scene->SetTerrainHorizon(TerrainHorizon::Bake(scene, meta.terrainShadowsResolution, meta.terrainShadowsReach));
			terrainBakePending = false;
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

	{
		PYROS_PROFILE_SCOPE("Player.Physics");
		physics->Update(dt, 10);
	}
	// 2D bodies, after the 3D world and before the scene solves its
	// transforms - Step() writes positions onto GameObjects and they have to
	// be in place before anything reads them this frame.
	if (physics2D)
	{
		physics2D->Sync(scene);
		physics2D->Step(dt, scene);

	}
	// After the step, so a frame's shadows match the positions it draws the
	// casters at. Outside the physics guard on purpose - a scene with no
	// physics still has occluders.
	Occluder2D::PublishSceneOccluders(scene);

	// One frame of probe refresh, so indirect light follows a light
	// that moves. Budget 0 means "all of them", which is right on the
	// GPU and would cost tens of milliseconds a frame on the CPU - so
	// ask which one is doing the work rather than trusting the number.
	if (ddgiActive && meta.ddgiDynamic)
	{
		// 0 means "as many as fit": the whole volume on the GPU, and
		// on the CPU whatever the renderer's millisecond ceiling
		// allows. A fixed count cannot be right for both a desktop and
		// a phone, and this build runs on both.
		renderer->UpdateGlobalIllumination(scene, meta.ddgiProbeBudget, meta.ddgiHysteresis);
	}
	// Layer parallax is deliberately NOT applied here. It is three lines of
	// Lua against Layer2D's factor, and doing it in the engine meant it
	// worked in a built game but not in the editor's play mode, and that a
	// script moving a layer would be fighting the engine for the same
	// transform. The engine's job is the layer; what it does is the game's.
	// Cells in and out around the camera, before the scene solves this
	// frame's transforms so an arriving cell is drawn where it belongs.
	if (worldStreamer && activeCamera)
		worldStreamer->Update(activeCamera->GetWorldPosition());
	// Terrains follow the same camera. The world's streamer pumps the
	// loader they share; without one it is pumped here.
	if (activeCamera) TerrainComponent::SetViewers(scene, std::vector<Vec3>(1, activeCamera->GetWorldPosition()));
	if (!worldStreamer) AssetStreamer::Instance().Pump(4.0);
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
		network->Update(dt);
	}
	scene->Update(time);
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
	scene->SettleTransforms();

	// The scene's own 2D view, after the script has had its say. A script
	// that moves the followed object, or writes view.center itself, has run by
	// now; doing this before it would render one frame behind whatever it did.
	UpdateView2DCamera((f32)dt);

	if (AudioManager* audio = AudioManager::GetActive())
		if (activeCamera) audio->SetListenerFromGameObject(activeCamera, dt);

	// One frame for the scene, the post chain and the UI below. Left to
	// themselves the renderer presented the scene and UIRenderer then opened
	// and presented a second frame holding only the HUD over black - every
	// game with a HUD flickered.
	IRenderDevice &device = GetActiveRenderDevice();
	const bool ownFrame = device.GetCurrentRenderTarget() == 0 && !device.IsFrameInProgress();
	if (ownFrame)
	{
		PYROS_PROFILE_SCOPE("Player.BeginFrame");
		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		device.BeginFrame();
		autoScale.gpuWaitMs += std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count();
	}

	renderer->ResetViewPort();
	renderer->SetViewPort(0, 0, RenderWidth(), RenderHeight());
#ifdef LUA_BINDINGS
	if (sceneMainScript)
	{
		PYROS_PROFILE_SCOPE("Player.ScriptPreRender");
		sceneMainScript->PreRender();
	}
#endif
	renderer->PreRender(activeCamera, scene);
	renderer->ApplyBackgroundClearColor();
	// A 2D scene renders through the *normal* pass, not a suppressed one. Its
	// content is ordinary world geometry - sprites are quads with materials -
	// seen through an orthographic camera, so it wants everything the pass
	// already does: transforms, culling, lights, sorting. An earlier version
	// pointed the renderer at RenderLayer::None here on the assumption that a
	// 2D scene was canvas-only; that is what UI is for, and it would have
	// drawn nothing at all for a real 2D game.
	// Only wrapped when there is a chain or anti-aliasing: with neither,
	// capturing would render the scene into an FBO that nothing presents,
	// i.e. a black window for every game that has no post effects.
	const bool postFX = HavePostEffects();
	if (postFX)
	{
		effectsManager->CaptureFrame();
		// TAA's sub-pixel offset; zero for every other mode.
		renderer->SetProjectionJitter(effectsManager->GetProjectionJitter());
	}

	renderer->RenderScene(projection, activeCamera, scene);
	renderer->SetProjectionJitter(Vec2(0.f, 0.f));

	if (postFX)
	{
		effectsManager->EndCapture();
		// Under Deferred the scene's depth is the renderer's, not the capture's:
		// the capture's own depth was never drawn into and reads "nothing there"
		// in every pixel. An effect that tests against the scene - smoke that
		// must stop at a wall or a hill, ambient occlusion - saw straight through
		// the world in a built game (the editor has always made this copy).
		if (gbufferFBO != NULL && effectsManager->GetDepth() != NULL)
		{
			DeferredRenderer* dr = static_cast<DeferredRenderer*>(renderer);
			if (dr->GetDepthTexture() != NULL)
				GetActiveRenderDevice().CopyDepthTexture(dr->GetDepthTexture()->GetBindID(),
					effectsManager->GetDepth()->GetBindID(), RenderWidth(), RenderHeight());
		}
		// Under Deferred the capture does not hold the scene - the renderer's
		// final composite targets framebuffer 0 - so the chain is pointed at
		// its colour output instead. Same reasoning as the editor viewport.
		// gbufferFBO is the player's own deferred tell: it only exists under
		// the deferred renderer.
		effectsManager->SetSceneSourceTexture(gbufferFBO != NULL
			? static_cast<DeferredRenderer*>(renderer)->GetColorTexture()
			: NULL);
		// The last effect draws to the swapchain, which is what a game wants
		// and - on Vulkan - is also what presents the frame at all. So no
		// SetRenderLastToTexture() here, unlike the editor.
		// Per frame, for the same reason as the editor viewport: depth-based
		// effects need the view the frame was rendered with.
		if (activeCamera != NULL)
			effectsManager->SetViewMatrix(activeCamera->GetWorldTransformation().Inverse());
		// See the editor viewport - no-op unless the chain contains motion
		// blur, and fed the real frame rate rather than the target one.
		effectsManager->RenderVelocityPass(projection, activeCamera, scene,
			dt > 0.0 ? (f32)(1.0 / dt) : 60.f);
		effectsManager->ProcessPostEffects(&projection);
	}

	// UI last, over the finished frame, and input fed to it right before -
	// so a click is resolved against the layout the player is looking at,
	// not the one from the previous frame.
	if (uiRenderer)
	{
		PYROS_PROFILE_SCOPE("Player.UI");
		uiRenderer->Resize(Width, Height);
		uiRenderer->RenderUI(scene);
		// The overlay composites over the finished frame, including over the
		// base scene's own UI - it is meant to be a pause menu or a dialog,
		// so it belongs on top of the HUD, not under it.
		if (overlayScene)
			uiRenderer->RenderUI(overlayScene);
		DispatchUIInput();
	}

	if (ownFrame)
	{
		PYROS_PROFILE_SCOPE("Player.EndFrame");
		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		device.EndFrame();
		autoScale.presentWaitMs += std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count();
	}
	// No more frames than asked for (SetFrameRateLimit): what a display cannot
	// show is heat and nothing else - and a machine that slows itself down when
	// hot then cannot hold the rate it could have. Waited out here, at the end
	// of the frame; counted as idle for the render scale, which is what it is.
	if (frameRateLimit > 0.f)
	{
		const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		const std::chrono::duration<f64> period(1.0 / (f64)frameRateLimit);
		if (nextFrameAt.time_since_epoch().count() == 0 || now > nextFrameAt + period * 4) nextFrameAt = now;
		nextFrameAt += std::chrono::duration_cast<std::chrono::steady_clock::duration>(period);
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
			autoScale.presentWaitMs += waitMs;
		}
	}
	StepAutoRenderScale(dt);

#ifdef LUA_BINDINGS
	// Between frames, never inside one: the script that asked for the
	// switch is owned by the scene graph the switch tears down. The overlay
	// has the same problem - a button in the overlay calling hideOverlay()
	// is owned by the graph being deleted - so it is applied here too.
	ApplyPendingOverlayIfAny();
	ApplyPendingSceneLoadIfAny();
#endif
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

// The scene is rendered at a fraction of the window's size and the last pass
// of the post chain fills the window with it; the UI is drawn after, at the
// window's own size, so text stays sharp. What a 4K screen on an integrated
// GPU needs: every pass of a deferred frame costs by the pixel.
uint32 PyrosPlayer::ScaledSize(const uint32 full) const
{
	const uint32 v = (uint32)((f32)full * renderScale + 0.5f);
	// (even, and never nothing)
	return v < 16 ? 16 : (v & ~1u);
}
uint32 PyrosPlayer::RenderWidth() const { return ScaledSize((uint32)Width); }
uint32 PyrosPlayer::RenderHeight() const { return ScaledSize((uint32)Height); }

// The render scale kept where the frame rate asked for holds, by itself.
// What says the GPU is the limit is the wait for it at the top of a frame
// (the previous frame's fence); what says there is room is time spent idle in
// the present (a display that caps the rate) or, with no cap, frames coming
// faster than asked. Looked at every second and a half, moved a step at a
// time - changing the size reallocates every target, which is a hitch of its
// own - and never for a frame that was slow for some other reason: when the
// GPU is not being waited for, a smaller picture would not help.
void PyrosPlayer::SetAutoRenderScale(const f32 targetFps, const f32 minScale, const f32 maxScale)
{
	autoScale.targetFps = targetFps;
	autoScale.minScale = minScale < 0.25f ? 0.25f : minScale;
	autoScale.maxScale = maxScale > 1.f ? 1.f : maxScale;
	if (autoScale.maxScale < autoScale.minScale) autoScale.maxScale = autoScale.minScale;
	autoScale.time = 0.0; autoScale.frames = 0; autoScale.gpuWaitMs = autoScale.presentWaitMs = 0.0;
}

void PyrosPlayer::StepAutoRenderScale(const f64 dt)
{
	AutoScale &A = autoScale;
	FrameProfiler::Instance().Counter("Render.ScalePct", (f64)renderScale * 100.0);
	if (A.targetFps <= 0.f) { A.gpuWaitMs = A.presentWaitMs = 0.0; return; }
	A.time += dt; A.frames++;
	if (A.time < 1.5 || A.frames < 20) return;
	const f64 frameMs = A.time * 1000.0 / (f64)A.frames;
	const f64 gpuWait = A.gpuWaitMs / (f64)A.frames, idle = A.presentWaitMs / (f64)A.frames;
	const f64 want = 1000.0 / (f64)A.targetFps;
	A.time = 0.0; A.frames = 0; A.gpuWaitMs = A.presentWaitMs = 0.0;

	// Changing the scale is not free - every target is reallocated and the post
	// chain rebuilt, a hitch of tens of milliseconds - so it moves between a
	// few fixed rungs and seldom: down a rung after two looks running that are
	// short of the rate on the GPU's account (two rungs if far short), up a
	// rung only after six looks running with room AND when the frame the
	// bigger picture would cost - by the square of the scales - still fits.
	static const f32 kRungs[] = { 1.f, 0.86f, 0.74f, 0.64f, 0.55f, 0.47f, 0.40f, 0.34f, 0.29f, 0.25f };
	static const int kRungCount = (int)(sizeof(kRungs) / sizeof(kRungs[0]));
	int rung = 0;
	for (int r = 1; r < kRungCount; r++) if (fabsf(kRungs[r] - renderScale) < fabsf(kRungs[rung] - renderScale)) rung = r;
	f32 next = renderScale;
	// Short of the rate, whoever's fault it looks like: on a machine where the
	// GPU and the CPU share their heat, a frame that is slow on the CPU's
	// account is as often as not slow because of what the GPU is being asked
	// for. So a rung down is tried - and if a look later the frame is no
	// better for it, it is taken back and not tried again for a while (a
	// machine that really is held up by its CPU keeps its sharp picture).
	const bool shortOfIt = frameMs > want * 1.06;
	const bool room = (gpuWait < 0.3 && idle > want * 0.25) || frameMs < want * 0.90;
	A.sinceChange++;
	if (A.triedDownFrom > 0.0 && A.sinceChange == 2)
	{
		// the verdict on the last step down
		if (frameMs > A.triedDownFrom * 0.97 && rung > 0 && gpuWait < 1.0)
		{
			rung -= 1;
			A.downBlockedLooks = 20;       // half a minute
		}
		A.triedDownFrom = 0.0;
		A.shortLooks = A.roomLooks = 0;
	}
	else if (shortOfIt)
	{
		A.roomLooks = 0;
		if (A.downBlockedLooks > 0 && gpuWait < 1.0) A.downBlockedLooks--;
		else if (++A.shortLooks >= 2 || frameMs > want * 1.4)
		{
			A.triedDownFrom = frameMs;
			rung += (frameMs > want * 1.4) ? 2 : 1;
			A.shortLooks = 0;
			A.upBlockedLooks = 60;
		}
	}
	else if (room)
	{
		A.shortLooks = 0;
		// (Up is taken far more slowly than down: the heat of a bigger picture
		// arrives a minute after the picture does, and a machine that slows
		// itself down when hot then loses more than the step gained. Half a
		// minute of room, a minute and a half after any step down, and only
		// when the bigger picture should fit with a third to spare.)
		if (A.upBlockedLooks > 0) A.upBlockedLooks--;
		else if (++A.roomLooks >= 20 && rung > 0)
		{
			// what the frame costs now is the frame less what was waited out;
			// a rung up multiplies the GPU's part of it by the pixels (all of
			// it, to be on the safe side)
			const f64 busy = frameMs - idle;
			const f64 grow = (f64)(kRungs[rung - 1] * kRungs[rung - 1]) / (f64)(kRungs[rung] * kRungs[rung]);
			if (busy * grow < want * 0.68) rung -= 1;
			A.roomLooks = 0;
		}
	}
	else
	{
		A.shortLooks = 0;
		A.roomLooks = 0;
	}
	if (rung >= kRungCount) rung = kRungCount - 1;
	next = kRungs[rung];
	if (next < A.minScale) next = A.minScale;
	if (next > A.maxScale) next = A.maxScale;
	if (fabsf(next - renderScale) >= 0.004f) A.sinceChange = 0;
	SetRenderScale(next);
}

void PyrosPlayer::SetRenderScale(f32 scale)
{
	if (scale < 0.25f) scale = 0.25f;
	if (scale > 1.f) scale = 1.f;
	if (fabsf(scale - renderScale) < 0.004f) return;
	renderScale = scale;
	// applied where a window resize is: at the top of the next frame
	pendingResizeWidth = (uint32)Width;
	pendingResizeHeight = (uint32)Height;
	resizePending = true;
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

	const uint32 fullW = pendingResizeWidth, fullH = pendingResizeHeight;
	if (fullW == 0 || fullH == 0) return; // minimised
	// What is rendered is this big; the window is fullW x fullH (SetRenderScale).
	const uint32 w = ScaledSize(fullW), h = ScaledSize(fullH);

	// Guarded on a real size change, like SceneEditor's own G-buffer resize:
	// everything below is expensive and none of it has anything to do when
	// the extent is unchanged.
	if (gbufferFBO && (w != gbufferAlbedo->GetWidth() || h != gbufferAlbedo->GetHeight()))
	{
		// The one piece of synchronisation that makes the rest safe. Same
		// reason DeferredRenderer::Resize() and SceneSerializer::
		// UnloadScene() open with one: nothing else here waits, and the
		// resources about to be reallocated may still be referenced by an
		// in-flight command buffer.
		if (IsActiveRenderDeviceSet())
			GetActiveRenderDevice().WaitIdle();

		gbufferDepth->Resize(w, h);
		gbufferAlbedo->Resize(w, h);
		gbufferSpecular->Resize(w, h);
		gbufferNormal->Resize(w, h);
		gbufferMatRough->Resize(w, h);
		gbufferFBO->Resize(w, h);
	}

	if (renderer) renderer->Resize(w, h);
	// The chain's own targets are window-sized too, and an effect reading a
	// stale-sized capture samples the wrong part of it.
	if (effectsManager)
	{
		effectsManager->Resize(w, h);
		effectsManager->SetOutputSize(fullW, fullH);
		BuildPostEffectChain();
	}
	ApplyProjection();
}

void PyrosPlayer::Shutdown()
{
	PyrosTextInput::SetHandler(NULL);
	InputManager::RemoveEvent(Event::Type::OnMove, Event::Input::Mouse::Wheel, this, &PyrosPlayer::OnMouseWheel);
	activePlayer = NULL;

	// The last submitted frame is routinely still executing here - the same
	// reason SceneSerializer::UnloadScene waits before freeing anything.
	if (IsActiveRenderDeviceSet())
		GetActiveRenderDevice().WaitIdle();

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

	// Before the renderer: an effect owns GPU objects created against the
	// device the renderer publishes, and ~PostEffectsManager waits on it.
	delete effectsManager; effectsManager = NULL;
	delete uiRenderer; uiRenderer = NULL;
	delete renderer; renderer = NULL;
	DestroyGBuffer();
	delete physics; physics = NULL;
	delete physics2D; physics2D = NULL;
	delete overlayScene; overlayScene = NULL;
	delete scene; scene = NULL;
	delete audio; audio = NULL;

	ClassName::Shutdown();
}
