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
	IRenderer* renderer;
	AudioManager* audio;
	Projection projection;

	// Deferred only - NULL under the forward renderer.
	FrameBuffer* gbufferFBO;
	Texture *gbufferDepth, *gbufferAlbedo, *gbufferSpecular, *gbufferNormal, *gbufferMatRough;

	// Owned by the player, used only when the scene has no camera of its own.
	std::shared_ptr<GameObject> fallbackCamera;
	GameObject* activeCamera;
	f32 cameraFov, cameraNear, cameraFar;

	// Everything the current scene allocated, so a scene switch frees
	// exactly that (see SceneSerializer::UnloadScene).
	LoadedSceneAssets sceneAssets;
	std::string currentSceneRel;
	bool sceneLoaded;
};

#endif /* PYROSPLAYER_H */
