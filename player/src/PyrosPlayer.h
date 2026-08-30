//============================================================================
// Name        : PyrosPlayer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Standalone runtime for a built game - no editor, no demo
//               browser. Reads game.json next to itself, opens a window,
//               loads the startup scene and runs it.
//============================================================================

#ifndef PYROSPLAYER_H
#define PYROSPLAYER_H

#if defined(_SDL2VULKAN)
#include "SDL2Vulkan/SDL2VulkanContext.h"
#define ClassName SDL2VulkanContext
#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>
#elif defined(_SDL2METAL)
#include "SDL2Metal/SDL2MetalContext.h"
#define ClassName SDL2MetalContext
#include <Pyros3D/Rendering/Device/MetalRenderDevice.h>
#else
#include "SDL2/SDL2Context.h"
#define ClassName SDL2Context
#endif

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Physics/Physics.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/UIRenderer/UIRenderer.h>
#include <Pyros3D/Rendering/Components/UI/UICanvas.h>
#include <Pyros3D/Rendering/Components/UI/UIButton.h>
#include <Pyros3D/Rendering/Components/UI/UIToggle.h>
#include <Pyros3D/Rendering/Components/UI/UISlider.h>
#include <Pyros3D/Rendering/Components/UI/UIInput.h>
#include <Pyros3D/Rendering/Components/UI/UIList.h>
#include <Pyros3D/Rendering/Components/UI/UIDropdown.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/Audio/AudioManager.h>

#ifdef LUA_BINDINGS
#include <Pyros3D/Ext/sol/sol.hpp>
#include <Pyros3D/Utils/Bindings/PyrosBindings.h>
#endif

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace p3d;

// game.json, written by the editor's Build Game. Every field has a usable
// default so a hand-written manifest can be three lines long, and a missing
// file is a clear error rather than a silent black window.
struct PlayerManifest
{
	std::string title = "Pyros3D";
	std::string startupScene;              // project-relative, e.g. "scenes/Level1.json"
	bool deferred = false;                 // matches ProjectSettings::rendererType
	int width = 1280;
	int height = 720;
	bool fullscreen = false;
	// What the frame clears to. Black unless game.json overrides it - the
	// editor viewport's own grey is editor chrome, not scene content, so a
	// build deliberately does not inherit it.
	Vec4 background = Vec4(0.f, 0.f, 0.f, 1.f);
	// Directory game.json was found in - everything else resolves against
	// it, so the game folder can be moved or renamed freely.
	std::string root;
	bool valid = false;
};

// Loaded once, before the window exists, because the window's size and
// title come from it. Safe to call repeatedly.
const PlayerManifest& PlayerManifestInstance();

class PyrosPlayer : public ClassName {

public:

	PyrosPlayer();
	virtual ~PyrosPlayer();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

private:

	// Loads `sceneRel` (project-relative), replacing whatever is loaded.
	// Runs the same start-up sequence the editor's play mode does: physics
	// synced, audio sources and particle systems started, scripts allowed
	// to run, scene main script initialised.
	bool LoadGameScene(const std::string& sceneRel);
	// Peeks the startup scene's twoD flag before the renderer is built.
	static bool StartupSceneIsTwoD(const PlayerManifest& m);

	// --- 2D scenes and overlays -------------------------------------------
	// A scene marked twoD (SceneMeta::twoD) is an ordinary scene seen through
	// an orthographic camera - sprites are quads with materials, and the
	// normal pass gives them transforms, culling, lights and sorting. It is
	// NOT canvas content: UI (UICanvas + UIRenderer) is the separate,
	// screen-space, batched thing that draws on top of whatever is beneath
	// it, and stays pinned. The two share only the shape of their projection.
	// Two shapes, same scene file either way:
	//
	//   * played on its own (a 2D game, or a menu built as UI): loadScene()
	//     it and `sceneIs2D` defaults the camera to orthographic;
	//   * shown *over* a running 3D scene on demand: showOverlay("Pause"),
	//     which loads it into its own SceneGraph and composites it on top
	//     without disturbing the scene underneath.
	//
	// The overlay is a second graph rather than objects merged into the main
	// one so that hiding it cannot perturb the scene it was drawn over, and
	// so one overlay scene can be authored once and shown above any level.
	bool LoadOverlayScene(const std::string& sceneRel);
	void ShowOverlayScene(const std::string& sceneRel);
	void HideOverlayScene();
	void ApplyPendingOverlayIfAny();
	// Repositions every Layer2D root for the current camera - see Layer2D.h.
	void ApplyLayerParallax();
	static void SetSubtreeRenderingEnabled(GameObject* go, const bool on);

	SceneGraph* overlayScene;
	std::string overlaySceneRel;
	// Deferred to a frame boundary, exactly like pendingLoadSceneName: the
	// script asking for it is running inside the frame that is drawing.
	std::string pendingOverlayName;
	bool pendingOverlayHide;
	bool sceneIs2D;
	// The scene file with its prefab references resolved, ready for the
	// engine. Empty when there was nothing to resolve (or nothing to read),
	// in which case the ordinary file-path load is used.
	std::string ExpandSceneFile(const std::string& absPath);
	void UnloadGameScene();

	// Which GameObject the frame is rendered from. Read from the scene's
	// .editor.json sidecar (that is where the editor records the active
	// camera and its fov/near/far); falls back to any camera in the file,
	// then to a camera of the player's own at the origin so a scene with
	// none still shows something rather than nothing.
	void ResolveCamera(const std::string& sceneAbsPath);
	void ApplyProjection();

	// DeferredRenderer renders into a G-buffer its caller owns.
	void BuildGBuffer(uint32 width, uint32 height);
	void DestroyGBuffer();

	// Window resizes are recorded by OnResize() and applied here, at the top
	// of a frame. See OnResize() for why reallocating GPU images from inside
	// the event callback is not safe.
	void ApplyPendingResizeIfAny();
	uint32 pendingResizeWidth, pendingResizeHeight;
	bool resizePending;

	// Scene switches requested by a script (loadScene("Level2")) are queued,
	// not immediate: the caller is running inside a component owned by the
	// scene graph the switch tears down. Applied between frames, exactly as
	// the editor does it.
	void ApplyPendingSceneLoadIfAny();

#ifdef LUA_BINDINGS
	// The globals a scene script expects to find. Deliberately the same set,
	// with the same names and meanings, as SceneEditor::PushLuaHostGlobals -
	// a script that runs in play mode has to run here unchanged, and every
	// name that exists in only one of the two is a way for that to stop
	// being true.
	void PushLuaHostGlobals();
	sol::state lua;
	std::shared_ptr<LuaComponent> sceneMainScript;
	std::string pendingLoadSceneName;
#endif

public:
	// Prefab.instantiate reaches these from inside a Lua callback.
	std::string ResolvePath(const std::string& relative) const;
	// The path asset references inside a spawned subtree resolve against -
	// the current scene, same as everything else the engine loads.
	std::string SceneAnchorPath() const { return ResolvePath(currentSceneRel); }
	Physics* physics;
private:

	SceneGraph* scene;
	void DispatchUIInput();
	void DispatchUIClick(GameObject* clicked);
	// Everything else a canvas reported this frame - a value changed, a
	// field submitted - to the handler named on the element.
	void DispatchUIEvents(UICanvas* canvas);
	// Typed characters arrive from the window layer, which knows nothing
	// about this instance - see TextInputHook.h.
	static void OnTextTyped(const char* utf8);
	static PyrosPlayer* activePlayer;
	// Wheel notches since the last frame. An event rather than a state the
	// way the mouse position is, so it has to be accumulated as it arrives
	// and spent once.
	void OnMouseWheel(Event::Input::Info e);
	f32 wheelDelta;
	// Edge detection for menu navigation - see DispatchUIInput.
	// One per entry in DispatchUIInput's key table - keys are edge-
	// triggered, or a held one would repeat at the polling rate.
	bool navKeyWasDown[10] = { false };
	bool navActivateWasDown = false;

	IRenderer* renderer;
	// Screen-space UI, composited over the finished 3D frame. Independent of
	// the Forward/Deferred choice above - it draws into whatever target the
	// frame is already using.
	UIRenderer* uiRenderer;
	AudioManager* audio;
	Projection projection;

	// Deferred only - NULL under the forward renderer.
	FrameBuffer* gbufferFBO;
	Texture *gbufferDepth, *gbufferAlbedo, *gbufferSpecular, *gbufferNormal, *gbufferMatRough;

	// Owned by the player, used only when the scene has no camera of its own.
	std::shared_ptr<GameObject> fallbackCamera;
	GameObject* activeCamera;
	f32 cameraFov, cameraNear, cameraFar;
	// See ApplyProjection(). Half-height of the ortho view volume, matching
	// the editor's own EditorCameraSettings::orthoSize.
	bool cameraOrthographic;
	f32 cameraOrthoSize;

	// Everything the current scene allocated, so a scene switch frees
	// exactly that (see SceneSerializer::UnloadScene).
	LoadedSceneAssets sceneAssets;
	std::string currentSceneRel;
	bool sceneLoaded;
};

#endif /* PYROSPLAYER_H */
