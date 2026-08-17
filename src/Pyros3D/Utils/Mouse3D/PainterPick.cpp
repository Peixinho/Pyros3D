//============================================================================
// Name        : PainterPick.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Painter Pick Class
//============================================================================

#include <Pyros3D/Utils/Mouse3D/PainterPick.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <cstdio>

namespace p3d {

	PainterPick::PainterPick(const uint32 Width, const uint32 Height) : IRenderer(Width, Height), colors(0), pickRadius(3) {

		// Create Material
		material = new GenericShaderMaterial(ShaderUsage::Color);
		// Frame Buffer
		fbo = new FrameBuffer();
		// Texture Creation
		texture = new Texture();
		texture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);

		// Frame Buffer Creation
		fbo->Init(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, Width, Height);
		fbo->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, texture);

		// Activate Culling
		ActivateCulling(CullingMode::FrustumCulling);

		EnableClearDepthBuffer();
	}
	void PainterPick::Resize(const uint32 Width, const uint32 Height)
	{
		IRenderer::Resize(Width, Height);
		fbo->Resize(Width, Height);
	}
	PainterPick::~PainterPick()
	{
		// Delete Material
		delete material;
		// Delete FBO
		delete fbo;
		// Delete Texture
		delete texture;
	}

	uint32 PainterPick::IdAt(const std::vector<uchar> &data, const uint32 x, const uint32 y) const
	{
		// Row 0 of the readback is the TOP of the image on Vulkan/Metal and
		// the BOTTOM on GL (see IRenderDevice::RenderTargetOriginIsTopLeft) -
		// mouse Y always counts down from the top, so only GL needs flipping.
		// Note the -1: the last valid row is Height-1, and the old
		// (Height - y) form indexed one row past the intended one (and ran
		// off the end of the buffer entirely for y == 0, which is why the
		// topmost row of the viewport could never be picked).
		const uint32 row = device->RenderTargetOriginIsTopLeft() ? y : (Height - 1 - y);
		const size_t idx = (((size_t)row * Width) + x) * 4;
		if (idx + 3 >= data.size())
			return 0;
		const Vec4 pixel(
			(f32)data[idx + 0] / 255.f, (f32)data[idx + 1] / 255.f,
			(f32)data[idx + 2] / 255.f, (f32)data[idx + 3] / 255.f);
		return Vec4ToRgba8(pixel);
	}

	RenderingMesh* PainterPick::PickObject(const f32 mouseX, const f32 mouseY, Projection projection, GameObject* Camera, SceneGraph* Scene)
	{
		this->mouseX = mouseX;
		this->mouseY = mouseY;
		this->Scene = Scene;

		if (Width == 0 || Height == 0 || mouseX < 0.f || mouseY < 0.f)
			return NULL;
		const uint32 px = (uint32)mouseX;
		const uint32 py = (uint32)mouseY;
		if (px >= Width || py >= Height)
			return NULL;

		RenderScene(projection, Camera, Scene);

		// One readback for the whole pick. GetTextureData() copies the entire
		// render target off the GPU, so calling it per-sample (as the radius
		// search below does) would cost megabytes a pixel.
		const std::vector<uchar> data = texture->GetTextureData();
		if (data.empty())
			return NULL;

		std::map<uint32, RenderingMesh*>::const_iterator hit = MeshPickingList.find(IdAt(data, px, py));
		if (hit != MeshPickingList.end())
			return hit->second;

		// Nothing exactly under the cursor - widen to a small square ring by
		// ring, nearest first. Thin geometry (a wire, a narrow edge on) covers
		// so few pixels that demanding an exact hit reads as "picking is
		// broken" even when the colour buffer is perfect. Rings are walked
		// outwards so the closest object still wins; radius 0 disables it.
		for (uint32 r = 1; r <= pickRadius; r++)
		{
			const int32 minX = (int32)px - (int32)r, maxX = (int32)px + (int32)r;
			const int32 minY = (int32)py - (int32)r, maxY = (int32)py + (int32)r;
			for (int32 y = minY; y <= maxY; y++)
			{
				if (y < 0 || y >= (int32)Height) continue;
				for (int32 x = minX; x <= maxX; x++)
				{
					// Ring only - the interior was covered by a smaller r.
					if (y != minY && y != maxY && x != minX && x != maxX) continue;
					if (x < 0 || x >= (int32)Width) continue;
					hit = MeshPickingList.find(IdAt(data, (uint32)x, (uint32)y));
					if (hit != MeshPickingList.end())
						return hit->second;
				}
			}
		}
		return NULL;
	}

	void PainterPick::RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene)
	{
		// Colors
		colors = 0;

		// Clear MeshPainter List
		MeshPickingList.clear();

		// Saves Camera
		this->Camera = Camera;
		this->CameraPosition = this->Camera->GetWorldPosition();

		// Saves Projection
		this->projection = projection;

		// Universal Cache
		ProjectionMatrix = projection.m;
		NearFarPlane = Vec2(projection.Near, projection.Far);

		// View Matrix and Position
		ViewMatrix = Camera->GetWorldTransformation().Inverse();
		CameraPosition = Camera->GetWorldPosition();

		// Flags
		ViewMatrixInverseIsDirty = true;
		ProjectionMatrixInverseIsDirty = true;
		ViewProjectionMatrixIsDirty = true;

		// Update Culling Information
		UpdateCulling(ProjectionMatrix*ViewMatrix);

		// Initialize Rendering
		InitRender();

		// Get Rendering Components List
		std::vector<RenderingMesh*> rmesh = RenderingComponent::GetRenderingMeshesSorted(Scene);

		// Id 0 means "nothing here", so the buffer has to start at a colour
		// that decodes to 0 - transparent black. Set BEFORE Bind(): a Vulkan
		// render pass bakes its clear value in at vkCmdBeginRenderPass, so a
		// SetClearColor() issued after the bind applies to the *next* pass,
		// not this one, and this buffer would inherit whatever colour the
		// last renderer to draw left behind (in the editor, the scene's grey
		// background - which decodes to a large bogus id).
		const Vec4 previousClear = device->GetClearColor();
		device->SetClearColor(Vec4(0.f, 0.f, 0.f, 0.f));

		// Bind FBO
		fbo->Bind();

		// Set ViewPort
		_SetViewPort(viewPortStartX, viewPortStartY, viewPortEndX, viewPortEndY);

		// Clear Screen
		ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
		DepthTest();
		DepthWrite();
		ClearDepthBuffer();
		ClearScreen();

		// Render Scene with Objects Material
		for (std::vector<RenderingMesh*>::iterator i = rmesh.begin(); i != rmesh.end(); i++)
		{

			if ((*i)->renderingComponent->GetOwner() != NULL)
			{
				// Culling Test
				bool cullingTest = false;
				switch ((*i)->CullingGeometry)
				{
				case CullingGeometry::Box:
					cullingTest = CullingBoxTest((*i), (*i)->renderingComponent->GetOwner());
					break;
				case CullingGeometry::Sphere:
				default:
					cullingTest = CullingSphereTest((*i), (*i)->renderingComponent->GetOwner());
					break;
				}
				if (!(*i)->renderingComponent->IsCullTesting()) cullingTest = true;
				// AND, not OR. RenderingMesh::Active defaults to true, so the
				// old `Clickable || Active` made Clickable=false a no-op -
				// there was no way to exclude a mesh (an editor grid, a
				// helper volume, a skybox) from picking at all.
				if (cullingTest && (*i)->renderingComponent->IsActive() && (*i)->Active && (*i)->Clickable)
				{
					colors++;
					Vec4 color = Rgba8ToVec4(colors);
					material->SetColor(color);
					RenderObject((*i), (*i)->renderingComponent->GetOwner(), material);
					MeshPickingList[colors] = (*i);
				}
			}
		}

		// End Rendering
		EndRender();

		// Unbind FBO
		fbo->UnBind();

		// Put back whatever the caller was clearing to - this is shared
		// device state, and the next renderer to begin a pass would
		// otherwise clear to this pass's transparent black.
		device->SetClearColor(previousClear);
	}
}
