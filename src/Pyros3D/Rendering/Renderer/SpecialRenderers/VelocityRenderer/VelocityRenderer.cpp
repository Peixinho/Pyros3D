//============================================================================
// Name        : VelocityRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Dynamic Cube Map aka Environment Map
//============================================================================

#include <Pyros3D/Rendering/Renderer/SpecialRenderers/VelocityRenderer/VelocityRenderer.h>
#include <Pyros3D/Other/PyrosGL.h>

namespace p3d {

	VelocityRenderer::VelocityRenderer(const uint32 Width, const uint32 Height) : IRenderer(Width, Height)
	{

		echo("SUCCESS: Velocity Renderer Created");

		// Don't frustum-cull the velocity pass: a mismatched VP (or a
		// shared-UBO hangover from Forward PreRender shadows) can drop
		// every mesh, leaving a cleared velocity map and an identity blur
		// that looks like "motion blur does nothing" on GL.
		//ActivateCulling(CullingMode::FrustumCulling);

		// Create Texture, Frame Buffer and Set the Texture as Attachment
		velocityMap = new Texture();
		// RGBA16F (not RG16F): macOS GL has been unreliable with RG16F
		// colour attachments + out vec2; PostEffectsManager already uses
		// RGBA16F successfully on both backends. Velocity still lives in .rg.
		velocityMap->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, Width, Height, false);
		velocityMap->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		// Depth as a texture (not a renderbuffer) - same multi-attach path
		// as PostEffectsManager / Deferred G-buffer. Keeps Vulkan's
		// pending-attachment finalize path consistent across backends.
		depthMap = new Texture();
		depthMap->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
		depthMap->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		// Initialize Frame Buffer
		fbo = new FrameBuffer();
		fbo->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, depthMap);
		fbo->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, velocityMap);
		velocityMaterial = new GenericShaderMaterial(ShaderUsage::VelocityRendering);

		// Default View Port Init Values
		viewPortStartX = viewPortStartY = 0;
		viewPortEndX = viewPortEndY = 0;

	}

	void VelocityRenderer::Resize(const uint32 &Width, const uint32 &Height)
	{
		fbo->Resize(Width, Height);

		IRenderer::Resize(Width, Height);
	}

	VelocityRenderer::~VelocityRenderer()
	{
		delete velocityMap;
		delete depthMap;
		delete fbo;
		delete velocityMaterial;
	}

	void VelocityRenderer::RenderVelocityMap(const p3d::Projection &Projection, GameObject* Camera, SceneGraph* Scene)
	{

		InitRender();

		this->Scene = Scene;
		this->Camera = Camera;
		this->projection = Projection;

		// Universal Cache
		PrvProjectionMatrix = ProjectionMatrix;
		ProjectionMatrix = projection.m;
		NearFarPlane = Vec2(projection.Near, projection.Far);

		// View Matrix and Position
		PrvViewMatrix = ViewMatrix;
		ViewMatrix = Camera->GetWorldTransformation().Inverse();
		CameraPosition = Camera->GetWorldPosition();

		// Flags
		ViewMatrixInverseIsDirty = true;
		ProjectionMatrixInverseIsDirty = true;
		ViewProjectionMatrixIsDirty = true;

		// Sort ourselves - don't rely on a prior Forward PreRender having
		// filled Scene's sorted list (shadow-less / empty-light paths skip
		// that, and CubemapRenderer's old "reuse last sort" assumption is
		// fragile for DemoLauncher).
		rmesh = GroupAndSortAssets(Scene, Camera);

		if (rmesh.size() > 0)
		{

			// Save Time
			Timer = Scene->GetTime();

			// Bind FBO (color + depth were attached once in the ctor).
			// Re-AddAttach every frame breaks Vulkan: Bind already opens a
			// multi-attachment render pass, and AttachFramebufferTexture2D
			// with wasAlreadyBound=true builds a 1-attachment framebuffer
			// against it (MoltenVK EXC_BAD_ACCESS in image-view setup).
			fbo->Bind();

			// Set ViewPort
			if (viewPortEndX == 0 || viewPortEndY == 0)
			{
				viewPortEndX = Width;
				viewPortEndY = Height;
			}

			_SetViewPort(viewPortStartX, viewPortStartY, viewPortEndX, viewPortEndY);

			// Clear Screen
			ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
			EnableClearDepthBuffer();
			ClearDepthBuffer();
			ClearScreen();

			// Render Scene with Objects Material
			for (std::vector<RenderingMesh*>::iterator k = rmesh.begin(); k != rmesh.end(); k++)
			{
				if ((*k)->renderingComponent->GetOwner() != NULL)
				{
					if ((*k)->renderingComponent->IsActive() && (*k)->Active == true)
					{
						RenderObject((*k), (*k)->renderingComponent->GetOwner(), velocityMaterial);
					}
				}
			}

			fbo->UnBind();

			EndRender();
		}
	}

	Texture* VelocityRenderer::GetTexture()
	{
		return velocityMap;
	}

};
