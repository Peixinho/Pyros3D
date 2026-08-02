//============================================================================
// Name        : EffectToggleTest
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See EffectToggleTest.h.
//============================================================================

#include "EffectToggleTest.h"

#include <Pyros3D/Rendering/PostEffects/Effects/TonemapEffect.h>
#include <Pyros3D/Other/PyrosGL.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <cstdlib>
#include <cstdio>
#include <vector>

using namespace p3d;

static const char* ModeName() { return getenv("P3D_MODE") ? getenv("P3D_MODE") : "toggle"; }
static int EnvInt(const char* name, int fallback) { return getenv(name) ? atoi(getenv(name)) : fallback; }

EffectToggleTest::EffectToggleTest() : ClassName(1280, 720, "Pyros3D - Effect Toggle Test", WindowType::Close | WindowType::Resize)
{
	Scene = nullptr; Camera = nullptr; CubeObject = nullptr; LightObject = nullptr;
	dLight = nullptr; rCube = nullptr; cubeHandle = nullptr; Diffuse = nullptr;
	Renderer = nullptr; EffectManager = nullptr; deferredFBO = nullptr;
	albedoTexture = specularTexture = depthTexture = normalTexture = metallicRoughnessTexture = nullptr;
	haveEffect = false;
	frame = 0;
}

EffectToggleTest::~EffectToggleTest() {}

void EffectToggleTest::OnResize(const uint32 width, const uint32 height)
{
	ClassName::OnResize(width, height);
	Renderer->Resize(width, height);
	EffectManager->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 0.1f, 2000.f);
	albedoTexture->Resize(Width, Height);
	specularTexture->Resize(Width, Height);
	depthTexture->Resize(Width, Height);
	normalTexture->Resize(Width, Height);
	metallicRoughnessTexture->Resize(Width, Height);
}

void EffectToggleTest::Init()
{
	ClassName::Init();

	Scene = new SceneGraph();

	// Same 5-attachment G-buffer shape every other DeferredRenderer caller uses.
	albedoTexture = new Texture(); albedoTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	specularTexture = new Texture(); specularTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	depthTexture = new Texture(); depthTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
	normalTexture = new Texture(); normalTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, Width, Height, false);
	metallicRoughnessTexture = new Texture(); metallicRoughnessTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);

	albedoTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	specularTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	depthTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	normalTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	metallicRoughnessTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

	deferredFBO = new FrameBuffer();
	deferredFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, depthTexture);
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, albedoTexture);
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment1, TextureType::Texture, specularTexture);
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment2, TextureType::Texture, normalTexture);
	deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment3, TextureType::Texture, metallicRoughnessTexture);

	Renderer = new DeferredRenderer(Width, Height, deferredFBO);
	Renderer->SetGlobalLight(Vec4(0.2f, 0.2f, 0.2f, 1.f));
	EffectManager = new PostEffectsManager(Width, Height);

	projection.Perspective(70.f, (f32)Width / (f32)Height, 0.1f, 2000.f);

	// Added to the Scene, not free-standing: the camera's world transform is
	// what RenderScene() reads, and only SceneGraph::Update() refreshes it.
	Camera = new GameObject();
	Camera->SetPosition(Vec3(0.f, 0.f, 40.f));
	Scene->Add(Camera);

	// A single bright, unmissable cube - "is the scene there at all" is the
	// only measurement this harness makes. DeferredRenderer_Gbuffer is
	// mandatory: without it the material never writes the G-buffer and the
	// second pass has nothing to light, which looks exactly like the bug
	// being investigated.
	Diffuse = new GenericShaderMaterial(ShaderUsage::DeferredRenderer_Gbuffer | ShaderUsage::Color | ShaderUsage::Diffuse);
	Diffuse->SetColor(Vec4(1.f, 1.f, 1.f, 1.f));
	cubeHandle = new Cube(12, 12, 12);
	rCube = new RenderingComponent(cubeHandle, Diffuse);
	CubeObject = new GameObject();
	CubeObject->Add(rCube);
	Scene->Add(CubeObject);

	LightObject = new GameObject();
	dLight = new DirectionalLight(Vec4(1, 1, 1, 1), Vec3(-1, -1, -1));
	dLight->SetLightIntensity(3.14159f);
	LightObject->Add(dLight);
	Scene->Add(LightObject);

	const std::string mode = ModeName();
	if (mode == "always" || mode == "droplate" || mode == "rebuild" || mode == "branchflip" || mode == "warmup")
		ApplyEffectChain(true);

	echo("EffectToggleTest: mode=" + mode);
}

// The whole point of this harness: the only thing that ever changes at runtime.
void EffectToggleTest::ApplyEffectChain(bool wanted)
{
	if (wanted == haveEffect) return;
	EffectManager->RemoveAllEffects();
	if (wanted) EffectManager->AddEffect(new TonemapEffect(RTT::Color, Width, Height));
	haveEffect = wanted;
	fprintf(stderr, "[FX] frame=%d effects=%u\n", frame, (unsigned)EffectManager->GetNumberEffects());
	fflush(stderr);
}

void EffectToggleTest::Update()
{
	const std::string mode = ModeName();
	const int period = EnvInt("P3D_PERIOD", 60);
	const int at = EnvInt("P3D_AT", 120);

	if (mode == "toggle" && period > 0 && frame > 0 && frame % period == 0)
		ApplyEffectChain(!haveEffect);
	else if (mode == "addlate" && frame == at)
		ApplyEffectChain(true);
	else if (mode == "droplate" && frame == at)
		ApplyEffectChain(false);
	else if (mode == "rebuild" && frame == at)
	{
		// Destroy and recreate the effect, keeping the COUNT at 1 the whole
		// time. Separates "an IEffect object was torn down and rebuilt at
		// runtime" from "the chain length changed" - only one of those can be
		// the trigger.
		EffectManager->RemoveAllEffects();
		EffectManager->AddEffect(new TonemapEffect(RTT::Color, Width, Height));
		fprintf(stderr, "[FX] frame=%d rebuilt, effects=%u\n", frame, (unsigned)EffectManager->GetNumberEffects());
		fflush(stderr);
	}

	// The mirror image of the above: the chain never changes at all, but the
	// per-frame render branch flips. Isolates the capture-path/direct-path
	// switch itself, with zero object churn.
	bool renderWithEffects = haveEffect;
	// Exercises BOTH render paths in the first two frames, so every pipeline
	// either path needs already exists before the flip. If the flip then works,
	// the problem is creating a pipeline mid-run rather than the flip itself.
	if (mode == "warmup")
	{
		if (frame == 0) renderWithEffects = false;
		else if (frame < at) renderWithEffects = true;
		else renderWithEffects = false;
		if (frame == at) { fprintf(stderr, "[FX] frame=%d warmup flip\n", frame); fflush(stderr); }
	}
	if (mode == "branchflip")
	{
		if (frame == at) { fprintf(stderr, "[FX] frame=%d branch flip (chain untouched, effects=%u)\n", frame, (unsigned)EffectManager->GetNumberEffects()); fflush(stderr); }
		if (frame >= at) renderWithEffects = false;
	}

	CubeObject->SetRotation(Vec3(0.4f, (f32)GetTime(), 0.f));
	Scene->Update(GetTime());

	// P3D_LEGACYBRANCH reproduces how callers had to be written before
	// PostEffectsManager grew its implicit passthrough: branch on whether the
	// chain is empty, which silently moves RenderScene()'s target between the
	// swapchain and ExternalFBO and triggers the Vulkan second-target bug.
	// Without it, the capture path is used unconditionally - one target, always.
	if (getenv("P3D_LEGACYBRANCH") && !renderWithEffects)
	{
		Renderer->PreRender(Camera, Scene);
		Renderer->RenderScene(projection, Camera, Scene);
	}
	else
	{
		EffectManager->CaptureFrame();
		Renderer->PreRender(Camera, Scene);
		Renderer->RenderScene(projection, Camera, Scene);
		EffectManager->EndCapture();
		EffectManager->ProcessPostEffects(&projection);
	}

	frame++;
	// Quit after N frames. Exists for GPU capture: MoltenVK's auto-capture
	// records from launch, so a long run produces a multi-gigabyte trace -
	// exiting at frame 4 keeps it to a single frame and ~500MB.
	if (getenv("P3D_EXITAT") && frame >= atoi(getenv("P3D_EXITAT"))) Close();
	CaptureIfRequested();
}

// Reads back the presented frame and prints how much of it is non-black, so a
// run is a single pass/fail line instead of something a human has to look at.
void EffectToggleTest::CaptureIfRequested()
{
	const char* shot = getenv("P3D_SHOT");
	if (!shot) return;
	const int shotFrame = EnvInt("P3D_SHOTF", 200);

#if defined(_SDL2VULKAN)
	VulkanRenderDevice &vk = static_cast<VulkanRenderDevice&>(GetActiveRenderDevice());
	if (frame == shotFrame) { vk.RequestFrameCapture(); return; }
	if (frame != shotFrame + 2) return;
	std::vector<uint8_t> px; uint32 w = 0, h = 0, ro = 0;
	if (!vk.GetCapturedFrame(px, w, h, ro) || !w || !h) { fprintf(stderr, "[R] capture failed\n"); exit(2); }
	std::vector<unsigned char> rgb(w * h * 3);
	for (uint32 i = 0; i < w * h; i++)
	{
		rgb[i * 3 + 0] = px[i * 4 + ro];
		rgb[i * 3 + 1] = px[i * 4 + 1];
		rgb[i * 3 + 2] = px[i * 4 + (2 - ro)];
	}
#else
	if (frame != shotFrame) return;
	const uint32 w = Width, h = Height;
	std::vector<unsigned char> rgb(w * h * 3);
	glReadBuffer(GL_BACK);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, &rgb[0]);
#endif

	unsigned long lit = 0;
	for (uint32 i = 0; i < w * h; i++)
		if (rgb[i * 3] + rgb[i * 3 + 1] + rgb[i * 3 + 2] > 24) lit++;
	fprintf(stderr, "[R] mode=%s effects=%u frame=%d lit=%lu/%u %s\n",
		ModeName(), (unsigned)EffectManager->GetNumberEffects(), frame, lit, w * h,
		lit == 0 ? "BLACK" : "ok");
	fflush(stderr);

	if (shot[0])
	{
		FILE* f = fopen(shot, "wb");
		if (f) { fprintf(f, "P6\n%u %u\n255\n", w, h); fwrite(&rgb[0], 1, rgb.size(), f); fclose(f); }
	}
	exit(lit == 0 ? 1 : 0);
}

void EffectToggleTest::Shutdown()
{
	if (EffectManager) EffectManager->RemoveAllEffects();
	if (Scene && Camera) Scene->Remove(Camera);
	if (Scene && CubeObject) Scene->Remove(CubeObject);
	if (Scene && LightObject) Scene->Remove(LightObject);
	delete rCube; delete CubeObject;
	delete dLight; delete LightObject;
	delete cubeHandle; delete Diffuse;
	delete Renderer; delete EffectManager; delete deferredFBO;
	delete albedoTexture; delete specularTexture; delete depthTexture;
	delete normalTexture; delete metallicRoughnessTexture;
	delete Camera; delete Scene;
	ClassName::Shutdown();
}
