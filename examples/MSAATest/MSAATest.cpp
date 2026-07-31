//============================================================================
// Name        : MSAATest.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See MSAATest.h.
//============================================================================

#include "MSAATest.h"

using namespace p3d;

MSAATest::MSAATest() : BaseExample(1280, 720, "Pyros3D - MSAA Test", WindowType::Close | WindowType::Resize)
{
}

MSAATest::~MSAATest() {}

void MSAATest::OnResize(const uint32 width, const uint32 height)
{
	BaseExample::OnResize(width, height);

	Renderer->Resize(width, height);
	projection.Perspective(60.f, (f32)width / (f32)height, 0.1f, 100.f);

	msaaColor->Resize(width, height);
	msaaDepth->Resize(width, height);
	resolvedColor->Resize(width, height);
	plainColor->Resize(width, height);
	plainDepth->Resize(width, height);

	ResolvedDisplay->Resize(width, height);
	PlainDisplay->Resize(width, height);
}

void MSAATest::Init()
{
	BaseExample::Init();

	Renderer = new ForwardRenderer(Width, Height);
	Renderer->SetGlobalLight(Vec4(0.15f, 0.15f, 0.18f, 1.f));

	projection.Perspective(60.f, (f32)Width / (f32)Height, 0.1f, 100.f);

	FPSCamera->SetPosition(Vec3(0.f, 0.f, 22.f));

	useMSAA = true;

	// Thin, sharply-angled boxes at odd rotations - lots of near-diagonal
	// edges at typical viewing distance, exactly the case MSAA visibly
	// helps and no-MSAA visibly doesn't.
	Light = new GameObject();
	dLight = new DirectionalLight(Vec4(1.f, 1.f, 1.f, 1.f), Vec3(-0.4f, -0.6f, -0.7f));
	Light->Add(dLight);
	Scene->Add(Light);

	CubeMaterial = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse);
	CubeMaterial->SetColor(Vec4(0.85f, 0.2f, 0.2f, 1.f));

	cubeMesh = new Cube(1.5f, 12.f, 1.5f);
	for (uint32 i = 0; i < 6; i++)
	{
		f32 angle = (f32)i * (360.f / 6.f);
		CubeObjects[i] = new GameObject();
		CubeObjects[i]->SetPosition(Vec3(0.f, 0.f, 0.f));
		CubeObjects[i]->SetRotation(Vec3(23.f, angle, 41.f));
		rCubes[i] = new RenderingComponent(cubeMesh, CubeMaterial);
		CubeObjects[i]->Add(rCubes[i]);
		Scene->Add(CubeObjects[i]);
	}

	// Real MSAA target - the whole point of this example. mipmapping
	// (false) matches every other render target in this codebase; msaa=4
	// is TextureRecord::samples' input, clamped to whatever this device
	// actually supports (see VulkanRenderDevice::ClampSampleCount()).
	msaaColor = new Texture();
	msaaColor->CreateEmptyTexture(TextureType::Texture_Multisample, TextureDataType::RGBA, Width, Height, false, 0, MSAA_SAMPLES);
	msaaDepth = new Texture();
	msaaDepth->CreateEmptyTexture(TextureType::Texture_Multisample, TextureDataType::DepthComponent, Width, Height, false, 0, MSAA_SAMPLES);
	msaaFBO = new FrameBuffer();
	msaaFBO->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture_Multisample, msaaColor);
	msaaFBO->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture_Multisample, msaaDepth);

	// Resolve destination - BlitFramebuffer()'s dst, and what actually
	// gets displayed on an MSAA-on frame.
	resolvedColor = new Texture();
	resolvedColor->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	resolvedFBO = new FrameBuffer();
	resolvedFBO->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, resolvedColor);

	// Same scene, same camera, msaa=0 - direct A/B comparison target.
	plainColor = new Texture();
	plainColor->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
	plainDepth = new Texture();
	plainDepth->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
	plainFBO = new FrameBuffer();
	plainFBO->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, plainColor);
	plainFBO->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, plainDepth);

	// Display, via PostEffectsManager + the new DisplayTextureEffect -
	// see MSAATest.h's comment on why (IMaterial::extraUniforms, what a
	// CustomShaderMaterial-based full-screen quad would need, is
	// friend-only to specific renderer classes, not reachable from
	// example code). Neither manager ever captures a frame itself
	// (CaptureFrame()/EndCapture() are never called) - both textures are
	// already fully rendered by the time ProcessPostEffects() runs.
	ResolvedDisplay = new PostEffectsManager(Width, Height);
	ResolvedDisplay->AddEffect(new DisplayTextureEffect(resolvedColor, Width, Height));
	PlainDisplay = new PostEffectsManager(Width, Height);
	PlainDisplay->AddEffect(new DisplayTextureEffect(plainColor, Width, Height));

	InitImGui();
}

void MSAATest::Update()
{
	Scene->Update(GetTime());

	BaseExample::Update();

	for (uint32 i = 0; i < 6; i++)
		CubeObjects[i]->SetRotation(Vec3(23.f + (f32)GetTime() * 10.f, (f32)i * (360.f / 6.f) + (f32)GetTime() * 15.f, 41.f));

	Renderer->PreRender(FPSCamera, Scene);

	if (useMSAA)
	{
		// Real MSAA render, then real resolve.
		msaaFBO->Bind();
		Renderer->RenderScene(projection, FPSCamera, Scene);
		msaaFBO->UnBind();

		msaaFBO->Bind(FBOAccess::Read);
		resolvedFBO->Bind(FBOAccess::Write);
		resolvedFBO->BlitFrameBuffer(0, 0, Width, Height, 0, 0, Width, Height, FBOBufferBit::Color, FBOFilter::Nearest);
		msaaFBO->UnBind();
		resolvedFBO->UnBind();

		ResolvedDisplay->ProcessPostEffects(&projection);
	}
	else
	{
		// Identical scene, no MSAA at all - direct comparison.
		plainFBO->Bind();
		Renderer->RenderScene(projection, FPSCamera, Scene);
		plainFBO->UnBind();

		PlainDisplay->ProcessPostEffects(&projection);
	}

	RenderImGui();
}

void MSAATest::DrawUI()
{
	DrawBaseUI();

	if (ImGui::Begin("MSAA Test", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Real Vulkan multisample render target verification");
		ImGui::Separator();
		ImGui::Checkbox("MSAA (4x, real resolve)", &useMSAA);
		ImGui::Text(useMSAA ? "Showing: msaaFBO -> BlitFramebuffer() resolve -> resolvedColor" : "Showing: plainFBO (msaa=0), no resolve");
		ImGui::Text("Look at the cube edges - MSAA off shows visibly harder jaggies.");
	}
	ImGui::End();
}

void MSAATest::Shutdown()
{
	for (uint32 i = 0; i < 6; i++)
	{
		CubeObjects[i]->Remove(rCubes[i]);
		Scene->Remove(CubeObjects[i]);
		delete rCubes[i];
		delete CubeObjects[i];
	}
	delete cubeMesh;
	delete CubeMaterial;

	Light->Remove(dLight);
	delete dLight;

	// ResolvedDisplay/PlainDisplay's destructors delete the
	// DisplayTextureEffect each one owns.
	delete ResolvedDisplay;
	delete PlainDisplay;

	delete msaaFBO;
	delete msaaColor;
	delete msaaDepth;
	delete resolvedFBO;
	delete resolvedColor;
	delete plainFBO;
	delete plainColor;
	delete plainDepth;

	// Renderer/Scene/FPSCamera are deleted by BaseExample::Shutdown()
	BaseExample::Shutdown();
}
