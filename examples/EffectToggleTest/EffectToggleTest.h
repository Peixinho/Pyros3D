//============================================================================
// Name        : EffectToggleTest
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Minimal repro harness for the Vulkan bug where changing
//                PostEffectsManager's effect *count* between 0 and 1 at
//                runtime blacks out the 3D scene while ImGui keeps drawing.
//                Deliberately the smallest possible setup - one static cube,
//                one directional light, DeferredRenderer + PostEffectsManager
//                and nothing else. No scene switching, no serialization, no
//                Lua, no physics: DemoLauncher (where this was first seen)
//                has all of those, and all of them were already ruled out, so
//                they only make the repro harder to reason about.
//
//                Driven entirely by env vars so one binary covers every case:
//                  P3D_MODE=none    - never add an effect
//                  P3D_MODE=always  - add the effect once at startup, keep it
//                  P3D_MODE=toggle  - add/remove every P3D_PERIOD frames
//                  P3D_MODE=addlate - start with none, add one at P3D_AT and
//                                     keep it (isolates 0->1 on its own)
//                  P3D_MODE=droplate- start with one, remove it at P3D_AT and
//                                     never re-add (isolates 1->0 on its own)
//                  P3D_PERIOD=<n>   - toggle period in frames (default 60)
//                  P3D_AT=<n>       - frame for the one-shot transition
//                  P3D_SHOT=<path>  - dump a PPM of the frame at P3D_SHOTF
//                  P3D_SHOTF=<n>    - which frame to capture (default 200)
//                  P3D_EXITAT=<n>   - quit after n frames (for GPU capture)
//                  P3D_LEGACYBRANCH - branch on the chain being empty, the way
//                                     callers had to before PostEffectsManager
//                                     grew its implicit passthrough. Still
//                                     reproduces the underlying Vulkan bug.
//
//                RenderDoc cannot be used here - it has no macOS capture
//                support. The equivalent is a Metal frame capture, which
//                MoltenVK can emit headlessly (needs Xcode.app to open):
//
//                  METAL_CAPTURE_ENABLED=1 \
//                  MVK_CONFIG_AUTO_GPU_CAPTURE_SCOPE=1 \
//                  MVK_CONFIG_AUTO_GPU_CAPTURE_OUTPUT_FILE=/tmp/broken.gputrace \
//                  P3D_LEGACYBRANCH=1 P3D_MODE=branchflip P3D_AT=1 P3D_EXITAT=4 \
//                  ./EffectToggleTest
//
//                Swap P3D_MODE=none for the working trace; the two differ only
//                in which render target the composite draws into.
//============================================================================

#ifndef EFFECTTOGGLETEST_H
#define EFFECTTOGGLETEST_H

#define _STR(path) #path
#define STR(path) _STR(path)

#if defined(_SDL2VULKAN)
#include "../WindowManagers/SDL2Vulkan/SDL2VulkanContext.h"
#define ClassName SDL2VulkanContext
#include <Pyros3D/Rendering/Device/VulkanRenderDevice.h>
#else
#include "../WindowManagers/SDL2/SDL2Context.h"
#define ClassName SDL2Context
#endif

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>

using namespace p3d;

class EffectToggleTest : public ClassName {

public:

	EffectToggleTest();
	virtual ~EffectToggleTest();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

private:

	void ApplyEffectChain(bool wanted);
	void CaptureIfRequested();

	SceneGraph* Scene;
	GameObject* Camera;
	GameObject* CubeObject;
	GameObject* LightObject;
	DirectionalLight* dLight;
	RenderingComponent* rCube;
	Renderable* cubeHandle;
	GenericShaderMaterial* Diffuse;

	Projection projection;
	DeferredRenderer* Renderer;
	PostEffectsManager* EffectManager;

	Texture *albedoTexture, *specularTexture, *depthTexture, *normalTexture, *metallicRoughnessTexture;
	FrameBuffer* deferredFBO;

	// Whether the chain currently holds an effect - mirrors
	// EffectManager->GetNumberEffects() != 0, kept separately so the
	// per-frame render branch reads the same flag the toggle wrote.
	bool haveEffect;
	int frame;

};

#endif /* EFFECTTOGGLETEST_H */
