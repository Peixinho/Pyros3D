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

		ActivateCulling(CullingMode::FrustumCulling);

		// Create Texture (CubeMap), Frame Buffer and Set the Texture as Attachment
		velocityMap = new Texture();
		velocityMap->CreateEmptyTexture(TextureType::Texture, TextureDataType::RG16F, Width, Height, false);
		velocityMap->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		// Initialize Frame Buffer
		fbo = new FrameBuffer();
		fbo->Init(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, Width, Height);
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
		if (IsCulling)
		{
			delete culling;
		}

		delete velocityMap;
		delete fbo;
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

		// Group and Sort Meshes
		rmesh = GroupAndSortAssets(Scene, Camera); // version updating everytime
		rmesh = RenderingComponent::GetRenderingMeshesSorted(Scene); // using last ordered meshes

		if (rmesh.size() > 0)
		{

			// Save Time
			Timer = Scene->GetTime();

			// Bind FBO
			fbo->Bind();

			// Frame Buffer Attachment
			fbo->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, velocityMap);

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

			// Update Culling
			UpdateCulling(ProjectionMatrix*ViewMatrix);

			// Render Scene with Objects Material
			for (std::vector<RenderingMesh*>::iterator k = rmesh.begin(); k != rmesh.end(); k++)
			{
				if ((*k)->renderingComponent->GetOwner() != NULL)
				{
					// Culling Test
					bool cullingTest = false;
					switch ((*k)->CullingGeometry)
					{
					case CullingGeometry::Box:
						cullingTest = CullingBoxTest((*k), (*k)->renderingComponent->GetOwner());
						break;
					case CullingGeometry::Sphere:
					default:
						cullingTest = CullingSphereTest((*k), (*k)->renderingComponent->GetOwner());
						break;
					}
					if (!(*k)->renderingComponent->IsCullTesting()) cullingTest = true;
					if (cullingTest && (*k)->renderingComponent->IsActive() && (*k)->Active == true)
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
