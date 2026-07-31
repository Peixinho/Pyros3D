//============================================================================
// Name        : MSAATest.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Real multisample (MSAA) render-target verification - the
//                first example in the suite to exercise Vulkan's msaa
//                support at all. Renders a thin, sharply-angled cube into
//                a 4x multisample color+depth FBO, resolves it via the
//                new BlitFramebuffer() into a single-sample texture, and
//                displays that. A key toggle (M) re-renders the identical
//                scene into a plain single-sample FBO instead, with no
//                resolve step, for a direct A/B comparison of the same
//                edges with and without MSAA.
//============================================================================

#ifndef MSAATEST_H
#define	MSAATEST_H

#include "../BaseExample/BaseExample.h"

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Rendering/PostEffects/Effects/DisplayTextureEffect.h>

using namespace p3d;

class MSAATest : public BaseExample {

public:

	MSAATest();
	virtual ~MSAATest();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void DrawUI();

private:

	static const uint32 MSAA_SAMPLES = 4;

	ForwardRenderer* Renderer;
	Projection projection;

	GameObject* Light;
	DirectionalLight* dLight;

	Renderable* cubeMesh;
	GameObject* CubeObjects[6];
	RenderingComponent* rCubes[6];
	GenericShaderMaterial* CubeMaterial;

	// Multisample color+depth target - real MSAA path.
	Texture *msaaColor, *msaaDepth;
	FrameBuffer* msaaFBO;

	// Single-sample resolve target - BlitFramebuffer()'s destination,
	// and what actually gets displayed on an MSAA-on frame.
	Texture* resolvedColor;
	FrameBuffer* resolvedFBO;

	// Single-sample, no-MSAA target - the same scene, same camera, same
	// everything, just msaa=0. Toggled to for direct comparison.
	Texture *plainColor, *plainDepth;
	FrameBuffer* plainFBO;

	// Display via PostEffectsManager + the new DisplayTextureEffect -
	// IMaterial::extraUniforms (what a CustomShaderMaterial-based
	// full-screen quad, DeferredRenderer's own approach, would need) is
	// friend-only to IRenderer/ForwardRenderer/DeferredRenderer, not
	// reachable from example code. Two separate manager+effect pairs,
	// one per texture, rather than one with a swappable target - keeps
	// DisplayTextureEffect itself minimal (matches ResizeEffect's shape
	// exactly), no new setter needed.
	PostEffectsManager* ResolvedDisplay;
	PostEffectsManager* PlainDisplay;

	bool useMSAA;

};

#endif	/* MSAATEST_H */
