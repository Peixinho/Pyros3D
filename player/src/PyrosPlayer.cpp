//============================================================================
// Name        : PyrosPlayer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See PyrosPlayer.h.
//============================================================================

#include "PyrosPlayer.h"
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
		m.deferred = (j.value("renderer", std::string("forward")) == "deferred");
		m.width = j.value("width", m.width);
		m.height = j.value("height", m.height);
		m.fullscreen = j.value("fullscreen", false);
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

PyrosPlayer::PyrosPlayer()
	: ClassName(PlayerManifestInstance().width, PlayerManifestInstance().height,
		PlayerManifestInstance().title,
		PlayerManifestInstance().fullscreen
			? (WindowType::Fullscreen | WindowType::Close)
			: (WindowType::Close | WindowType::Resize))
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

	if (m.deferred && !sceneWantsForward)
	{
		BuildGBuffer(Width, Height);
		renderer = new DeferredRenderer(Width, Height, gbufferFBO);
	}
	else
		renderer = new ForwardRenderer(Width, Height);

	// Independent of that choice: it composites over whatever the frame
	// already drew.
	uiRenderer = new UIRenderer(Width, Height);

	renderer->SetViewPort(0, 0, Width, Height);
	// Asserted explicitly, never left to the default. The clear colour is
	// device-global state that outlives any one renderer (see
	// IRenderer::DrawBackground), so "whatever it happened to be" is not a
	// value - it is whichever renderer wrote it last. The editor sets its own
	// 0.2 grey for exactly this reason; a game gets black unless game.json
	// says otherwise.
	renderer->SetBackground(m.background);

#ifdef LUA_BINDINGS
	GenerateBindings(&lua);
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
	lua.set_function("warpMouseToCenter", [this]() {
		SDL_WarpMouseInWindow(GetSDLWindow(), (int)(Width / 2), (int)(Height / 2));
	});
	lua.set_function("getWindowSize", [this]() { return std::make_tuple((int)Width, (int)Height); });
	lua.set_function("quitGame", [this]() { Close(); });

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

	if (!LoadGameScene(m.startupScene))
	{
		echo("ERROR: could not load startup scene " + m.startupScene);
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

bool PyrosPlayer::HavePostEffects() const
{
	return effectsManager != NULL && effectsManager->GetNumberEffects() > 0;
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
		return;
	}
	if (effectsManager == NULL)
		effectsManager = new PostEffectsManager(Width, Height);
	PostEffectChain::Build(*effectsManager, meta.postEffects, Width, Height,
		&PyrosPlayer::ReadPostEffectAsset, this);
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
	ResolveCamera(abs);
	ApplyProjection();

#ifdef LUA_BINDINGS
	PushLuaHostGlobals();
#endif

	// Sounds and emitters authored in the scene start with the scene, the
	// same way entering play mode starts them in the editor. Anything a
	// script wants to control instead can stop it on its first update.
	int soundsStarted = 0, soundsMissing = 0;
	std::vector<std::shared_ptr<GameObject>>& all = scene->GetAllGameObjectList();
	for (size_t i = 0; i < all.size(); ++i)
	{
		const std::vector<std::shared_ptr<IComponent>>& comps = all[i]->GetComponents();
		for (size_t c = 0; c < comps.size(); ++c)
		{
			if (AudioSource* asrc = dynamic_cast<AudioSource*>(comps[c].get()))
			{
				if (!asrc->EnsureLoaded()) { ++soundsMissing; continue; }
				asrc->ResetVelocityTracking();
				asrc->Play();
				++soundsStarted;
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

void PyrosPlayer::UnloadGameScene()
{
	if (!sceneLoaded) return;
#ifdef LUA_BINDINGS
	sceneMainScript.reset();
#endif
	activeCamera = NULL;
	SceneSerializer::UnloadScene(scene, sceneAssets);
	sceneAssets = LoadedSceneAssets();
	sceneLoaded = false;
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
	lua["scene"] = scene;
	lua["camera"] = activeCamera;
	// Accepted and ignored: in the editor this redirects the *viewport* to a
	// different camera, and a script written against play mode will call it.
	// Here the render camera is whatever the scene says, so honouring it is
	// exactly right - it is the same thing.
	lua["setRenderCamera"] = [this](GameObject* go) { if (go) activeCamera = go; };
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

	if (!LoadGameScene(rel))
		echo("ERROR: loadScene(\"" + requested + "\") failed");
}
#endif

void PyrosPlayer::Update()
{
	if (!sceneLoaded) return;

	// At the top of the frame, before anything renders - see OnResize().
	ApplyPendingResizeIfAny();

	const f64 time = GetTime();
	const f64 dt = GetTimeInterval();

	physics->Update(dt, 10);
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
	// Layer parallax is deliberately NOT applied here. It is three lines of
	// Lua against Layer2D's factor, and doing it in the engine meant it
	// worked in a built game but not in the editor's play mode, and that a
	// script moving a layer would be fighting the engine for the same
	// transform. The engine's job is the layer; what it does is the game's.
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
		try { sceneMainScript->Update(time); }
		catch (const std::exception& e) { echo(std::string("ERROR: scene main script update - ") + e.what()); }
	}
#endif

	// The scene's own 2D view, after the script has had its say. A script
	// that moves the followed object, or writes view.center itself, has run by
	// now; doing this before it would render one frame behind whatever it did.
	UpdateView2DCamera((f32)dt);

	if (AudioManager* audio = AudioManager::GetActive())
		if (activeCamera) audio->SetListenerFromGameObject(activeCamera, dt);

	renderer->ResetViewPort();
	renderer->SetViewPort(0, 0, Width, Height);
	renderer->PreRender(activeCamera, scene);
	renderer->ApplyBackgroundClearColor();
	// A 2D scene renders through the *normal* pass, not a suppressed one. Its
	// content is ordinary world geometry - sprites are quads with materials -
	// seen through an orthographic camera, so it wants everything the pass
	// already does: transforms, culling, lights, sorting. An earlier version
	// pointed the renderer at RenderLayer::None here on the assumption that a
	// 2D scene was canvas-only; that is what UI is for, and it would have
	// drawn nothing at all for a real 2D game.
	// Only wrapped when there is a chain: with none, capturing would render
	// the scene into an FBO that nothing presents, i.e. a black window for
	// every game that has no post effects.
	const bool postFX = HavePostEffects();
	if (postFX) effectsManager->CaptureFrame();

	renderer->RenderScene(projection, activeCamera, scene);

	if (postFX)
	{
		effectsManager->EndCapture();
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
		uiRenderer->Resize(Width, Height);
		uiRenderer->RenderUI(scene);
		// The overlay composites over the finished frame, including over the
		// base scene's own UI - it is meant to be a pause menu or a dialog,
		// so it belongs on top of the HUD, not under it.
		if (overlayScene)
			uiRenderer->RenderUI(overlayScene);
		DispatchUIInput();
	}

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

	const uint32 w = pendingResizeWidth, h = pendingResizeHeight;
	if (w == 0 || h == 0) return; // minimised

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
