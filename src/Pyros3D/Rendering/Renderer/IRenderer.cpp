//============================================================================
// Name        : IRenderer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Renderer Interface
//============================================================================

#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>
#include <cstring>

// Must match MAX_LIGHTS in resources/shaders/PyrosShader.glsl - sizes and
// fills the LightsUBO backing that shader's uLights[MAX_LIGHTS] block.
#define PYROS_MAX_LIGHTS 4

// Must match the array sizes declared in PyrosShader.glsl's
// DirectionalShadowBlock/PointShadowBlock/SpotShadowBlock.
#define PYROS_MAX_DIRECTIONAL_SHADOW_CASCADES 4
#define PYROS_MAX_POINT_SHADOW_MATRICES 8
#define PYROS_MAX_SPOT_SHADOW_MATRICES 4

// Must match MAX_BONES in resources/shaders/PyrosShader.glsl - sizes the
// BoneMatricesUBO backing that shader's uBoneMatrix[MAX_BONES] block.
#define PYROS_MAX_BONES 60

namespace p3d {

// ViewPort Dimension
uint32 IRenderer::_viewPortStartX = 0;
uint32 IRenderer::_viewPortStartY = 0;
uint32 IRenderer::_viewPortEndX = 0;
uint32 IRenderer::_viewPortEndY = 0;

// Shared UBOs (see IRenderer.h for why these are static/refcounted rather
// than per-instance) and their dirty-tracking cache.
uint32 IRenderer::SharedUBORefCount = 0;
uint32 IRenderer::GlobalMatricesUBO = 0;
uint32 IRenderer::LightsUBO = 0;
uint32 IRenderer::DirectionalShadowUBO = 0;
uint32 IRenderer::PointShadowUBO = 0;
uint32 IRenderer::SpotShadowUBO = 0;
bool IRenderer::GlobalMatricesUBOValid = false;
Matrix IRenderer::CachedProjectionMatrix;
Matrix IRenderer::CachedViewMatrix;
bool IRenderer::CachedRenderingPointShadowFace = false;
bool IRenderer::LightsUBOValid = false;
std::vector<Matrix> IRenderer::CachedLights;
bool IRenderer::DirectionalShadowUBOValid = false;
std::vector<Matrix> IRenderer::CachedDirectionalShadowMatrix;
Vec4 IRenderer::CachedDirectionalShadowFar;
bool IRenderer::PointShadowUBOValid = false;
std::vector<Matrix> IRenderer::CachedPointShadowMatrix;
bool IRenderer::SpotShadowUBOValid = false;
std::vector<Matrix> IRenderer::CachedSpotShadowMatrix;

uint32 IRenderer::VertexFrameUniformsUBO = 0;
uint32 IRenderer::VelocityFrameUniformsUBO = 0;
uint32 IRenderer::ObjectMatrixUniformsUBO = 0;
uint32 IRenderer::BoneMatricesUBO = 0;
uint32 IRenderer::VelocityObjectUniformsUBO = 0;
uint32 IRenderer::AmbientLightUniformsUBO = 0;
uint32 IRenderer::MaterialUniformsUBO = 0;
uint32 IRenderer::ObjectLightCountsUBO = 0;
bool IRenderer::VertexFrameUniformsUBOValid = false;
Vec3 IRenderer::CachedCameraPosition;
bool IRenderer::AmbientLightUniformsUBOValid = false;
Vec4 IRenderer::CachedGlobalLight;
bool IRenderer::VelocityFrameUniformsUBOValid = false;
Matrix IRenderer::CachedPrvProjectionMatrix;
Matrix IRenderer::CachedPrvViewMatrix;

namespace Sort {

	GameObject* _Camera;
	bool sortRenderingMeshes(const void* a, const void* b)
	{
		f32 a2 = _Camera->GetPosition().distanceSQR(((RenderingMesh*)a)->renderingComponent->GetOwner()->GetWorldPosition());
		f32 b2 = _Camera->GetPosition().distanceSQR(((RenderingMesh*)b)->renderingComponent->GetOwner()->GetWorldPosition());
		return (a2 < b2);
	}
}

std::vector<RenderingMesh*> IRenderer::GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera, const uint32 Tag)
{

	// Sort and Group Objects From Scene
	std::vector<RenderingMesh*> _OpaqueMeshes;
	std::vector<RenderingMesh*> _TranslucidMeshes;

	// LOD
	if (lod)
	{
		std::vector<RenderingComponent*> comps(RenderingComponent::GetRenderingComponents(Scene));
		for (std::vector<RenderingComponent*>::iterator i = comps.begin(); i != comps.end(); i++)
		{
			f32 distance = (Camera->GetWorldPosition().distanceSQR((*i)->GetOwner()->GetWorldPosition() + ((*i)->GetBoundingSphereCenter() - (*i)->GetBoundingSphereRadius())*(*i)->GetOwner()->GetScale()));
			(*i)->UpdateLOD((*i)->GetLODByDistance(fabs(distance)));
		}
	}
	// Get Meshes
	std::vector<RenderingMesh*> rmeshes(RenderingComponent::GetRenderingMeshes(Scene));

	if (Tag != 0)
	{
		for (std::vector<RenderingMesh*>::iterator k = rmeshes.begin(); k != rmeshes.end();)
		{
			if (!(*k)->renderingComponent->GetOwner()->HaveTag(Tag))
			{
				k = rmeshes.erase(k);
			}
			else ++k;
		}
	}

	for (std::vector<RenderingMesh*>::iterator k = rmeshes.begin(); k != rmeshes.end(); k++)
	{
		if ((*k)->Material->IsTransparent() && sorting)
		{
			_TranslucidMeshes.push_back((*k));
		}
		else _OpaqueMeshes.push_back((*k));
	}

	// sorting translucid
	Sort::_Camera = Camera;
	sort(_TranslucidMeshes.begin(), _TranslucidMeshes.end(), Sort::sortRenderingMeshes);

	// final list
	for (std::vector<RenderingMesh*>::reverse_iterator i = _TranslucidMeshes.rbegin(); i != _TranslucidMeshes.rend(); i++)
	{
		_OpaqueMeshes.push_back((*i));
	}

	Scene->SetRenderingMeshesSorted(_OpaqueMeshes);

	return _OpaqueMeshes;
}

// DebugRenderer uses this constructor and never calls RenderObject()/
// SendGlobalUniforms(), so it doesn't touch the shared UBOs at all - in
// particular it must NOT bump SharedUBORefCount, since it will never
// decrement it on destruction either (see ~IRenderer()).
IRenderer::IRenderer() : UsesSharedUBOs(false), device(new GLRenderDevice()) { RenderingPointShadowFace = false; }

// Resolves what IRenderer(Width, Height, externalDevice)'s device member
// should use, and whether it should *own* (delete on destruction) or just
// *borrow* it: an explicitly-passed device wins outright (owned, as
// before - nothing today relies on it being borrowed); otherwise, a
// device someone registered via RegisterRenderDeviceForOwnership() (e.g.
// SDL2VulkanContext, which needs a real VulkanRenderDevice + swapchain to
// exist before any IRenderer does) is adopted (owned) if present. If
// neither applies but a device is *already active* (SetActiveRenderDevice()'d
// by whichever IRenderer got constructed first - e.g. the example's own
// ForwardRenderer, built before a VelocityRenderer/PainterPick/etc.),
// borrow that instead of creating a second, broken GLRenderDevice - a
// Vulkan-only process has no real GL context, so every glad function
// pointer in that second device would be NULL, crashing on first real use
// (confirmed live in MotionBlurExample/PickingPainterMethod). Only when
// none of the above apply does this fall back to constructing a fresh,
// owned GLRenderDevice, exactly as before this existed - every GL-only
// example's very first `new ForwardRenderer(Width, Height)` call still
// hits exactly this path, unchanged.
struct ResolvedDevice { IRenderDevice *ptr; bool owns; };
static ResolvedDevice ResolveInitialDevice(IRenderDevice* externalDevice)
{
	if (externalDevice != NULL)
		return { externalDevice, true };
	if (IRenderDevice* registered = TakeRenderDeviceOwnership())
		return { registered, true };
	if (IsActiveRenderDeviceSet())
		return { &GetActiveRenderDevice(), false };
	return { new GLRenderDevice(), true };
}

IRenderer::IRenderer(const uint32 Width, const uint32 Height, IRenderDevice* externalDevice)
{
	ResolvedDevice resolved = ResolveInitialDevice(externalDevice);
	device = MaybeOwningDevicePtr(resolved.ptr, MaybeOwningDeviceDeleter{resolved.owns});

	// Every Shader/GeometryBuffer/RenderingComponent constructed anywhere
	// in the engine (no IRenderer reference available at most of those
	// call sites) shares whichever backend THIS instance ends up using -
	// see GetActiveRenderDevice()/SetActiveRenderDevice() in IRenderDevice.h.
	SetActiveRenderDevice(device.get());

	// Background Unset by Default
	BackgroundColorSet = false;

	// Set Global Light Default Color
	GlobalLight = Vec4(0.2f, 0.2f, 0.2f, 0.2f);

	// Save Dimensions
	this->Width = Width;
	this->Height = Height;

	// Depth Bias
	IsUsingDepthBias = false;

	// Custom ViewPort
	customViewPort = false;

	// Point-shadow cubemap-face Y-flip flag (see IRenderer.h's comment)
	RenderingPointShadowFace = false;

	// Blending Flag
	blending = false;

	// Defaults
	ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
	depthWritting = depthTesting = false;
	clearDepthBuffer = true;
	sorting = true;
	scissorTest = false;
	scissorTestX = 0;
	scissorTestY = 0;
	scissorTestWidth = (f32)Width;
	scissorTestHeight = (f32)Height;
	lod = false;
	ClipPlane = false;
	IsCulling = false;

	// GlobalMatricesUBOValid etc. are NOT reset here - they're static/shared
	// (see IRenderer.h), already correctly initialized to false exactly
	// once at program start, and must stay whatever they currently are if
	// another IRenderer instance is already alive and has valid data
	// uploaded to the shared UBOs.
	UsesSharedUBOs = true;

	// Shadows materials
	shadowMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows);
	shadowMaterial->SetCullFace(CullFace::DoubleSided);
	shadowSkinnedMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows | ShaderUsage::Skinning);
	shadowSkinnedMaterial->SetCullFace(CullFace::DoubleSided);

	// Created once, by whichever IRenderer instance happens to be first -
	// see the "shared/static" comment on these members in IRenderer.h.
	// Later instances just add themselves to the refcount below.
	if (SharedUBORefCount == 0)
	{
		// Global matrices UBO: sized for uProjectionMatrix + uViewMatrix,
		// bound to binding point 0. Contents are uploaded in SendGlobalUniforms().
		GlobalMatricesUBO = device->CreateUniformBuffer(sizeof(Matrix) * 2, 0);

		// Lights UBO: sized for uLights[PYROS_MAX_LIGHTS], bound to binding
		// point 1. Contents are uploaded in SendGlobalUniforms().
		LightsUBO = device->CreateUniformBuffer(sizeof(Matrix) * PYROS_MAX_LIGHTS, 1);

		// Directional shadow UBO (binding point 2): PYROS_MAX_DIRECTIONAL_SHADOW_CASCADES
		// cascade matrices followed by uDirectionalShadowFar[4] (std140 needs
		// no padding between them - the matrix array's size is already a
		// multiple of vec4's 16-byte alignment).
		DirectionalShadowUBO = device->CreateUniformBuffer(sizeof(Matrix) * PYROS_MAX_DIRECTIONAL_SHADOW_CASCADES + sizeof(Vec4) * 4, 2);

		// Point shadow UBO (binding point 3): PYROS_MAX_POINT_SHADOW_MATRICES matrices.
		PointShadowUBO = device->CreateUniformBuffer(sizeof(Matrix) * PYROS_MAX_POINT_SHADOW_MATRICES, 3);

		// Spot shadow UBO (binding point 4): PYROS_MAX_SPOT_SHADOW_MATRICES matrices.
		SpotShadowUBO = device->CreateUniformBuffer(sizeof(Matrix) * PYROS_MAX_SPOT_SHADOW_MATRICES, 4);

		// UBOs for PyrosShader.glsl's formerly-loose uniforms (binding
		// points 16-22 - see the BIND_* macros in that file). Only ever
		// written to for materials where Material->SupportsUniformBlocks()
		// is true; created unconditionally here regardless, same as every
		// UBO above, since creating a small buffer nobody currently binds
		// to is harmless and keeps this block simple.
		VertexFrameUniformsUBO = device->CreateUniformBuffer(sizeof(Vec4), 16);
		VelocityFrameUniformsUBO = device->CreateUniformBuffer(sizeof(Matrix) * 2, 17);
		ObjectMatrixUniformsUBO = device->CreateUniformBuffer(sizeof(Matrix), 18);
		BoneMatricesUBO = device->CreateUniformBuffer(sizeof(Matrix) * PYROS_MAX_BONES, 19);
		VelocityObjectUniformsUBO = device->CreateUniformBuffer(sizeof(Matrix), 20);
		AmbientLightUniformsUBO = device->CreateUniformBuffer(sizeof(Vec4), 21);
		// vec4 uColor + vec4 uSpecular + 5 floats = 52 bytes, padded to 64
		// (std140 vec4-alignment) - see MaterialUniformsData in
		// SendUserUniforms().
		MaterialUniformsUBO = device->CreateUniformBuffer(64, 22);
		// 3 ints padded to 16 bytes (std140) - see ObjectLightCountsData in
		// SendModelUniforms().
		ObjectLightCountsUBO = device->CreateUniformBuffer(16, 23);
	}
	SharedUBORefCount++;
}

void IRenderer::Reset()
{
	// Defaults
	depthWritting = depthTesting = false;
	clearDepthBuffer = true;
	depthTestMode = -1;
}

void IRenderer::Resize(const uint32 &Width, const uint32 &Height)
{
	// Save Dimensions
	this->Width = Width;
	this->Height = Height;

	if (!customViewPort)
	{
		viewPortEndX = Width;
		viewPortEndY = Height;
	}

	// Reset States
	Reset();
}

void IRenderer::SetViewPort(const uint32 initX, const uint32 initY, const uint32 endX, const uint32 endY)
{
	viewPortStartX = initX;
	viewPortStartY = initY;
	viewPortEndX = endX;
	viewPortEndY = endY;
	customViewPort = true;
}

void IRenderer::_SetViewPort(const uint32 initX, const uint32 initY, const uint32 endX, const uint32 endY)
{
	if (initX != _viewPortStartX || initY != _viewPortStartY || endX != _viewPortEndX || endY != _viewPortEndY)
	{
		_viewPortStartX = initX;
		_viewPortStartY = initY;
		_viewPortEndX = endX;
		_viewPortEndY = endY;
		device->SetViewport(initX, initY, endX, endY);
	}
}

IRenderer::~IRenderer()
{
	// UsesSharedUBOs is false for instances built via the no-arg
	// IRenderer() (DebugRenderer) - they never incremented SharedUBORefCount
	// in the constructor, so they must not decrement it here either.
	if (UsesSharedUBOs)
	{
		SharedUBORefCount--;
		if (SharedUBORefCount == 0)
		{
			device->DestroyUniformBuffer(GlobalMatricesUBO);
			device->DestroyUniformBuffer(LightsUBO);
			device->DestroyUniformBuffer(DirectionalShadowUBO);
			device->DestroyUniformBuffer(PointShadowUBO);
			device->DestroyUniformBuffer(SpotShadowUBO);
			device->DestroyUniformBuffer(VertexFrameUniformsUBO);
			device->DestroyUniformBuffer(VelocityFrameUniformsUBO);
			device->DestroyUniformBuffer(ObjectMatrixUniformsUBO);
			device->DestroyUniformBuffer(BoneMatricesUBO);
			device->DestroyUniformBuffer(VelocityObjectUniformsUBO);
			device->DestroyUniformBuffer(AmbientLightUniformsUBO);
			device->DestroyUniformBuffer(MaterialUniformsUBO);
			device->DestroyUniformBuffer(ObjectLightCountsUBO);
			// The next IRenderer instance (if any) will create brand new
			// (empty) buffers - the dirty-tracking cache must not survive
			// to wrongly skip that instance's first upload.
			GlobalMatricesUBOValid = false;
			LightsUBOValid = false;
			DirectionalShadowUBOValid = false;
			PointShadowUBOValid = false;
			SpotShadowUBOValid = false;
			VertexFrameUniformsUBOValid = false;
			AmbientLightUniformsUBOValid = false;
			VelocityFrameUniformsUBOValid = false;
		}
	}
	delete shadowMaterial;
	delete shadowSkinnedMaterial;
}

void IRenderer::RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene) {

}

void IRenderer::PreRender(GameObject* Camera, SceneGraph* Scene)
{
	PreRender(Camera, Scene, 0);
}

void IRenderer::PreRender(GameObject* Camera, SceneGraph* Scene, const std::string &Tag = "")
{
	PreRender(Camera, Scene, MakeStringID(Tag));
}

void IRenderer::PreRender(GameObject* Camera, SceneGraph* Scene, const uint32 Tag)
{

	// Group and Sort Meshes
	rmesh = GroupAndSortAssets(Scene, Camera, Tag);

	// Get Lights List
	lcomps = ILightComponent::GetLightsOnScene(Scene);

	if (rmesh.size() > 0 && lcomps.size() > 0)
	{
		// Initialize Renderer
		InitRender();

		// Prepare and Pack Lights to Send to Shaders
		std::vector<Matrix> _Lights;

		Lights.clear();

		// Keep user settings
		uint32 _bufferOptions = bufferOptions;
		uint32 _glBufferOptions = glBufferOptions;
		bool _clearDepthBuffer = clearDepthBuffer;

		// ShadowMaps
		DirectionalShadowMapsTextures.clear();
		DirectionalShadowMatrix.clear();
		NumberOfDirectionalShadows = 0;

		PointShadowMapsTextures.clear();
		PointShadowMatrix.clear();
		NumberOfPointShadows = 0;

		SpotShadowMapsTextures.clear();
		SpotShadowMatrix.clear();
		NumberOfSpotShadows = 0;

		ViewMatrix = Camera->GetWorldTransformation().Inverse();
		uint32 pointCounter = 0;
		uint32 spotCounter = 0;
		for (std::vector<IComponent*>::iterator i = lcomps.begin(); i != lcomps.end(); i++)
		{
			switch (((ILightComponent*)(*i))->GetLightType())
			{
			case LIGHT_TYPE::DIRECTIONAL:
			{
				DirectionalLight* d = ((DirectionalLight*)(*i));

				Vec3 direction = (d->GetOwner()->GetWorldTransformation() * Vec4(d->GetLightDirection(), 0.f)).xyz().normalize();

				// Shadows
				if (d->IsCastingShadows())
				{
					// Increase Number of Shadows
					NumberOfDirectionalShadows++;

					// Bind FBO
					d->GetShadowFBO()->Bind();

					ClearBufferBit(Buffer_Bit::Depth);
					EnableClearDepthBuffer();
					ClearDepthBuffer();
					ClearScreen();

					StartClippingPlanes();

					// Enable Depth Bias
					shadowMaterial->EnableDethBias(d->GetShadowBiasFactor(), d->GetShadowBiasUnits()); // enable polygon offset fill to combat "z-fighting"

					ViewMatrix.identity();
					ViewMatrix.LookAt(Vec3::ZERO, direction, Vec3(0.f, 0.f, -1.f));

					// Get Lights Shadow Map Texture
					for (uint32 i = 0; i < d->GetNumberCascades(); i++)
					{
						d->UpdateCascadeFrustumPoints(i, Camera->GetWorldPosition(), Camera->GetDirection());
						ProjectionMatrix = d->GetLightProjection(ViewMatrix, i, rmesh);

						// Set Viewport
						_SetViewPort((uint32)((float)(i % 2) * d->GetShadowWidth()), (uint32)((i <= (uint32)1 ? 0.0f : 1.f) * d->GetShadowHeight()), d->GetShadowWidth(), d->GetShadowHeight());

						// Update Culling
						UpdateCulling(d->GetCascade(i).ortho.GetProjectionMatrix()*ViewMatrix);

						// Render Scene with Objects Material
						for (std::vector<RenderingMesh*>::iterator k = rmesh.begin(); k != rmesh.end(); k++)
						{

							if ((*k)->renderingComponent->GetOwner() != NULL && !(*k)->Material->IsTransparent())
							{
								if ((*k)->renderingComponent->IsCastingShadows() && (*k)->renderingComponent->IsActive())
									RenderObject((*k), (*k)->renderingComponent->GetOwner(), ((*k)->SkinningBones.size() > 0 ? shadowSkinnedMaterial : shadowMaterial));
							}
						}

						// device->TranslateProjectionMatrix() (identity on
						// GL) - this matrix maps a world-space fragment
						// into the shadow map's own UV+depth space for
						// the main pass's comparison lookup, so it must
						// use the *same* clip-space convention the
						// shadow map was actually rendered with
						// (RenderObject()'s own SendGlobalUniforms()
						// call, a few lines up, already applies this same
						// translation to what gets uploaded as
						// uProjectionMatrix while rendering the shadow
						// map itself - using the raw, un-translated
						// matrix here instead would silently look up the
						// wrong texel/depth on Vulkan, exactly the kind
						// of "renders without error but is wrong" bug
						// pixel-readback verification exists to catch).
						// device->TranslateShadowBiasMatrix() (not the raw
						// Matrix::BIAS constant) for the same reason - see
						// its comment in IRenderDevice.h for why using
						// BIAS directly here double-transforms Z on
						// Vulkan.
						DirectionalShadowMatrix.push_back((device->TranslateShadowBiasMatrix() * (device->TranslateProjectionMatrix(ProjectionMatrix) * ViewMatrix * Camera->GetWorldTransformation())));

					}

					EndClippingPlanes();

					// Get Texture (only 1)
					DirectionalShadowMapsTextures.push_back(d->GetShadowMapTexture());

					// Set Shadow Far
					Vec4 _ShadowFar;
					if (d->GetNumberCascades() > 0) _ShadowFar.x = d->GetCascade(0).Far;
					if (d->GetNumberCascades() > 1) _ShadowFar.y = d->GetCascade(1).Far;
					if (d->GetNumberCascades() > 2) _ShadowFar.z = d->GetCascade(2).Far;
					if (d->GetNumberCascades() > 3) _ShadowFar.w = d->GetCascade(3).Far;

					// This maps a linear cascade-far distance into the
					// same normalized depth space gl_FragCoord.z is
					// compared against in the shader's cascade-selection
					// branches (PyrosShader.glsl's DirectionalShadow
					// block) - derived from the *main camera's*
					// projection matrix's own Z-mapping terms, so it
					// must use the same clip-space convention that
					// projection matrix was actually uploaded with
					// (SendGlobalUniforms() translates it for Vulkan -
					// see TranslateProjectionMatrix()'s comment); using
					// the raw GL-convention terms here would compute the
					// wrong threshold and silently pick the wrong
					// cascade (or none at all) on Vulkan.
					Matrix translatedProjection = device->TranslateProjectionMatrix(projection.m);
					Vec4 ShadowFar;
					ShadowFar.x = 0.5f*(-_ShadowFar.x*translatedProjection.m[10] + translatedProjection.m[14]) / _ShadowFar.x + 0.5f;
					ShadowFar.y = 0.5f*(-_ShadowFar.y*translatedProjection.m[10] + translatedProjection.m[14]) / _ShadowFar.y + 0.5f;
					ShadowFar.z = 0.5f*(-_ShadowFar.z*translatedProjection.m[10] + translatedProjection.m[14]) / _ShadowFar.z + 0.5f;
					ShadowFar.w = 0.5f*(-_ShadowFar.w*translatedProjection.m[10] + translatedProjection.m[14]) / _ShadowFar.w + 0.5f;
					DirectionalShadowFar = ShadowFar;

					// Disable Depth Bias
					DisableDepthBias();

					// Unbind FBO
					d->GetShadowFBO()->UnBind();

				}
			}
			break;
			case LIGHT_TYPE::POINT:
			{
				PointLight* p = ((PointLight*)(*i));

				// Shadows
				if (p->IsCastingShadows())
				{
					// Increase Number of Shadows
					NumberOfPointShadows++;

					// Bind FBO
					p->GetShadowFBO()->Bind();

					// Create Projection Matrix
					// Get Light Projection
					Projection ShadowProjection;
					ShadowProjection.Perspective(90.f, 1.f, p->GetShadowNear(), p->GetShadowFar());
					ProjectionMatrix = ShadowProjection.m;

					// See IRenderer.h's comment on RenderingPointShadowFace -
					// SendGlobalUniforms() (called from each face's
					// RenderObject() below) reads this to skip Vulkan's
					// clip-space Y-flip specifically for these 6 draws.
					RenderingPointShadowFace = true;

					// Get Lights Shadow Map Texture
					for (int32 i = 0; i < 6; i++)
					{
						// Clean View Matrix
						ViewMatrix.identity();

						// Create Light View Matrix For Rendering Each Face of the Cubemap
						if (i == 0)
							ViewMatrix.LookAt(p->GetOwner()->GetWorldPosition(), p->GetOwner()->GetWorldPosition() + Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)); // +X
						if (i == 1)
							ViewMatrix.LookAt(p->GetOwner()->GetWorldPosition(), p->GetOwner()->GetWorldPosition() + Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)); // -X
						if (i == 2)
							ViewMatrix.LookAt(p->GetOwner()->GetWorldPosition(), p->GetOwner()->GetWorldPosition() + Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)); // +Y
						if (i == 3)
							ViewMatrix.LookAt(p->GetOwner()->GetWorldPosition(), p->GetOwner()->GetWorldPosition() + Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)); // -Y
						if (i == 4)
							ViewMatrix.LookAt(p->GetOwner()->GetWorldPosition(), p->GetOwner()->GetWorldPosition() + Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)); // +Z
						if (i == 5)
							ViewMatrix.LookAt(p->GetOwner()->GetWorldPosition(), p->GetOwner()->GetWorldPosition() + Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f)); // -Z

						// Update Culling
						UpdateCulling(ShadowProjection.m*ViewMatrix);

						// GPU Shadows
						p->GetShadowFBO()->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::CubemapPositive_X + i, p->GetShadowMapTexture());

						ClearBufferBit(Buffer_Bit::Depth);
						EnableClearDepthBuffer();
						ClearDepthBuffer();
						ClearScreen();

						StartClippingPlanes();

						// Enable Depth Bias
						shadowMaterial->EnableDethBias(p->GetShadowBiasFactor(), p->GetShadowBiasUnits()); // enable polygon offset fill to combat "z-fighting"

						// Set Viewport
						_SetViewPort(0, 0, p->GetShadowWidth(), p->GetShadowHeight());

						// Render Scene with Objects Material
						for (std::vector<RenderingMesh*>::iterator k = rmesh.begin(); k != rmesh.end(); k++)
						{

							if ((*k)->renderingComponent->GetOwner() != NULL)
							{
								// Culling Test
								/*bool cullingTest = false;
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
								if (!(*k)->renderingComponent->IsCullTesting()) cullingTest = true;*/
								if (/*cullingTest && */ (*k)->renderingComponent->GetOwner() != NULL && !(*k)->Material->IsTransparent() && !(*k)->Material->IsTransparent())
								{
									if ((*k)->renderingComponent->IsCastingShadows() && (*k)->renderingComponent->IsActive())
										RenderObject((*k), (*k)->renderingComponent->GetOwner(), ((*k)->SkinningBones.size() > 0 ? shadowSkinnedMaterial : shadowMaterial));
								}
							}
						}

						EndClippingPlanes();

					}

					// Done rendering the 6 faces - every other pass from
					// here on (this light's own record-keeping, the next
					// light, the eventual main camera pass) needs the
					// normal Y-flip again.
					RenderingPointShadowFace = false;

					// Set Light Projection
					// PCFPOINT() (PyrosShader.glsl) reconstructs a reference
					// depth from this matrix via clip.z/clip.w, then a
					// hardcoded *0.5+0.5 remap assuming GL's raw [-1,1]
					// clip-space Z - correct only if this matrix is left
					// untranslated, which is exactly what it was: the same
					// class of bug device->TranslateShadowBiasMatrix()/
					// TranslateProjectionMatrix() already fixed for
					// directional and spot shadows (see their own comments)
					// was never applied here for point shadows. Baking both
					// in here means GL gets the identical remap it always
					// had (TranslateProjectionMatrix() is a no-op there, and
					// TranslateShadowBiasMatrix() returns Matrix::BIAS, whose
					// Z row is exactly the same 0.5/0.5 remap) while Vulkan's
					// clip.z/clip.w comes out already in [0,1] - the shader
					// was updated to stop re-applying its own remap on top.
					PointShadowMatrix.push_back(device->TranslateShadowBiasMatrix() * device->TranslateProjectionMatrix(ShadowProjection.m));
					// Set Light View Matrix
					Matrix m;
					m.Translate(p->GetOwner()->GetWorldPosition().negate());
					PointShadowMatrix.push_back(m * Camera->GetWorldTransformation());

					// Get Texture (only 1)
					PointShadowMapsTextures.push_back(p->GetShadowMapTexture());

					// Disable Depth Bias
					DisableDepthBias();

					// Unbind FBO
					p->GetShadowFBO()->UnBind();

				}
			}
			break;
			case LIGHT_TYPE::SPOT:
			{
				SpotLight* s = ((SpotLight*)(*i));

				// Shadows
				if (s->IsCastingShadows())
				{

					Vec3 direction = (s->GetOwner()->GetWorldTransformation() * Vec4(s->GetLightDirection(), 0.f)).xyz().normalize();

					// Increase Number of Shadows
					NumberOfSpotShadows++;

					// Bind FBO
					s->GetShadowFBO()->Bind();

					// Get Light Projection
					Projection ShadowProjection;
					ShadowProjection.Perspective(2 * s->GetLightOutterCone(), 1.0, s->GetShadowNear(), s->GetShadowFar());
					ProjectionMatrix = ShadowProjection.m;

					// Clean View Matrix
					ViewMatrix.identity();

					// Create Light View Matrix For Rendering the ShadowMap
					ViewMatrix.LookAt(s->GetOwner()->GetWorldPosition(), (s->GetOwner()->GetWorldPosition() + direction));

					// Update Culling
					UpdateCulling(ShadowProjection.m*ViewMatrix);

					ClearBufferBit(Buffer_Bit::Depth);
					EnableClearDepthBuffer();
					ClearDepthBuffer();
					ClearScreen();

					StartClippingPlanes();

					// Enable Depth Bias
					shadowMaterial->EnableDethBias(s->GetShadowBiasFactor(), s->GetShadowBiasUnits()); // enable polygon offset fill to combat "z-fighting"

					// Set Viewport
					_SetViewPort(0, 0, s->GetShadowWidth(), s->GetShadowHeight());

					// Render Scene with Objects Material
					for (std::vector<RenderingMesh*>::iterator k = rmesh.begin(); k != rmesh.end(); k++)
					{

						if ((*k)->renderingComponent->GetOwner() != NULL)
						{
							// Culling Test
							/*bool cullingTest = false;
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
							if (!(*k)->renderingComponent->IsCullTesting()) cullingTest = true;*/
							if (/*cullingTest && */ (*k)->renderingComponent->GetOwner() != NULL && !(*k)->Material->IsTransparent() && !(*k)->Material->IsTransparent())
							{
								if ((*k)->renderingComponent->IsCastingShadows() && (*k)->renderingComponent->IsActive())
									RenderObject((*k), (*k)->renderingComponent->GetOwner(), ((*k)->SkinningBones.size() > 0 ? shadowSkinnedMaterial : shadowMaterial));
							}
						}
					}

					EndClippingPlanes();

					// Disable Depth Bias
					DisableDepthBias();

					// Unbind FBO
					s->GetShadowFBO()->UnBind();

					// Set Light Matrix
					// See the comment on the equivalent DirectionalShadowMatrix
				// line above - same fix, same reason.
				SpotShadowMatrix.push_back((device->TranslateShadowBiasMatrix() * (device->TranslateProjectionMatrix(ProjectionMatrix) * ViewMatrix * Camera->GetWorldTransformation())));

					// Get Texture (only 1)
					SpotShadowMapsTextures.push_back(s->GetShadowMapTexture());
				}
			};
			}
		}

		// Reset User Defined for Depth Buffer
		bufferOptions = _bufferOptions;
		glBufferOptions = _glBufferOptions;
		clearDepthBuffer = _clearDepthBuffer;
		EndRender();
	}

}

void IRenderer::InitRender()
{
	LastProgramUsed = -1;
	LastMaterialUsed = -1;
	LastMeshRendered = -1;
	InternalDrawType = -1;
	LastMaterialPTR = NULL;
	LastMeshRenderedPTR = NULL;
	cullFace = -1;
	depthWritting = true;
	DepthWrite();

	// No VAO to create here anymore - BindMesh() creates and caches one per
	// (mesh, shader) pair on demand. This used to glGenVertexArrays a new
	// VAO on every InitRender() call (up to 3x per frame in
	// DeferredRenderer) and never delete it, leaking one every time.
}

void IRenderer::EndRender()
{
	if (LastMeshRenderedPTR != NULL && LastMaterialPTR != NULL)
	{
		// The next mesh's BindMesh()/glBindVertexArray() fully replaces this
		// VAO's attribute/index-buffer state, so there's nothing to unbind.
		CommandBufferHandle endRenderCmd = device->BeginCommandBuffer();
		device->BindVertexArray(endRenderCmd, 0);
		device->EndCommandBuffer(endRenderCmd);
		// Unbind Shadow Maps
		UnbindShadowMaps(LastMaterialPTR);
		// Material After Render
		LastMaterialPTR->AfterRender();
	}

	// Set Default Polygon Mode
	device->SetWireFrame(false);
	// Disable Cull Face
	device->DisableCullFace();

	// Unbind Shader Program
	device->UseProgram(0);
	// Unset Pointers
	LastMaterialPTR = NULL;
	LastMeshRenderedPTR = NULL;
	LastProgramUsed = -1;
	LastMaterialUsed = -1;
	LastMeshRendered = -1;

	DisableBlending();
}

// See the comment on RenderingMesh::PipelineCache (RenderingComponent.h) -
// packs (shader, targetFBO) into one key. Shader program handles and FBO
// handles are both DeviceHandle (uint32), so a plain 32-bit shift keeps
// each half exact with no risk of collision.
static uint64 PipelineCacheKey(const uint32 shader, const uint32 targetFBO)
{
	return ((uint64)shader << 32) | (uint64)targetFBO;
}

void IRenderer::RenderObject(RenderingMesh* rmesh, GameObject* owner, IMaterial* Material)
{
	// See the comment on CommandBufferHandle in IRenderDevice.h - GL ignores
	// this value entirely (ignored/no-op on this backend), so per-object
	// granularity here costs nothing; a real per-frame command buffer is a
	// Phase 5 Step D concern once a real VulkanRenderDevice needs Begin/End
	// to mean something.
	CommandBufferHandle cmd = device->BeginCommandBuffer();

	// model cache
	PrvModelMatrix = owner->GetPrvWorldTransformation() * rmesh->Pivot;
	ModelMatrix = owner->GetWorldTransformation() * rmesh->Pivot;

	NormalMatrixIsDirty = true;
	ModelViewMatrixIsDirty = true;
	ModelViewProjectionMatrixIsDirty = true;
	ModelMatrixInverseIsDirty = true;
	ModelViewMatrixInverseIsDirty = true;
	ModelMatrixInverseTransposeIsDirty = true;
	ModelViewProjectionMatrixInverseIsDirty = true;
	ViewProjectionMatrixInverseIsDirty = true;

	if ((LastMeshRenderedPTR != rmesh || LastMaterialPTR != Material) && LastProgramUsed != -1)
	{
		// Material Stuff After Render
		UnbindShadowMaps(LastMaterialPTR);
		// After Render
		LastMaterialPTR->AfterRender();
	}
	if (LastProgramUsed != Material->GetShader()) device->UseProgram(Material->GetShader());

	if (LastMeshRenderedPTR != rmesh || LastMaterialPTR != Material)
	{
		// Bind Mesh (resolves attribute/uniform locations; on desktop GL /
		// GLES3 also builds and caches a VAO the first time this mesh is
		// seen with this shader)
		BindMesh(rmesh, Material);

		// The VAO built by BindMesh() already has every attribute pointer
		// and the index buffer baked in.
		device->BindVertexArray(cmd, rmesh->VAOCache[Material->GetShader()]);

		// The pipeline BindMesh() cached alongside the VAO - see the
		// comment on RenderingMesh::PipelineCache. Called at this same
		// mesh/material-switch cadence as the individual SetCullFaceMode/
		// SetBlendingEnabled/SetDepthTest/etc calls below (which stay as-is
		// for GL - this is additive, not a replacement, so GL's existing
		// per-field dirty-tracking is untouched); GLRenderDevice::BindPipeline()
		// re-issues those same calls unconditionally, so calling it here
		// too is redundant work for GL, but only at this same rare
		// (mesh, shader)-switch frequency, not per object - negligible.
		// Deliberately called *before* Material->PreRender()/BindShadowMaps()/
		// SendGlobalUniforms() below (moved here from after them) - on
		// Vulkan, binding a texture-uniform (uColormap, uDirectionalShadowMaps,
		// etc) needs to know which pipeline's descriptor set to update
		// (VulkanRenderDevice::currentPipeline, set by BindPipeline()),
		// and those calls are exactly what triggers that write (see
		// VulkanRenderDevice::SendUniformInt()'s comment) - with the old
		// order, the very first object using a new (mesh,shader) pair
		// would send its shadow-map uniform against whatever pipeline
		// was current *before* this switch (or none at all), silently
		// leaving the real descriptor unwritten
		// (VUID-vkCmdDrawIndexed-None-08114 caught this the hard way).
		device->BindPipeline(cmd, rmesh->PipelineCache[PipelineCacheKey(Material->GetShader(), device->GetCurrentRenderTarget())]);

		// Material Stuff Pre Render
		Material->PreRender();

		// Bind Shadow Maps
		BindShadowMaps(Material);

		if (Material->depthBias)
			EnableDepthBias(Vec2(Material->depthFactor, Material->depthUnits));
	}

	// Send Global Uniforms - deliberately called on *every* RenderObject(),
	// not gated by the mesh/material-switch check above. GlobalMatricesUBO
	// carries the current view/projection, which a shadow-casting pass
	// changes per cascade/cubemap-face (IRenderer.cpp's directional/point
	// loops reassign ProjectionMatrix/ViewMatrix and call RenderObject()
	// again for the *same* single shadowMaterial+mesh) - gating this call
	// on mesh/material identity meant a scene with only one shadow-casting
	// object never re-uploaded past the first face/cascade, silently
	// rendering every subsequent face from the first face's stale
	// view/projection (confirmed via DebugReadDepthTexture: all 6
	// point-shadow cubemap faces showed byte-identical depth data for a
	// single-occluder scene). SendGlobalUniforms() already has its own
	// internal memcmp-based dirty check (skips the actual GPU upload when
	// the matrices haven't changed), so calling it unconditionally here is
	// still cheap for the common multi-object case - it was never the
	// right thing to piggyback on the mesh/material cache in the first
	// place.
	SendGlobalUniforms(rmesh, Material);

	// Check double sided
	if (rmesh->Material != Material)
	{
		if (rmesh->Material->GetCullFace() != Material->GetCullFace())
		{
			Material->SetCullFace(rmesh->Material->GetCullFace());
			cullFaceChanged = true;
		}
	}
	if (LastMaterialPTR != Material || cullFaceChanged)
	{
		// Check if Material is DoubleSided
		if (Material->GetCullFace() != cullFace)
		{
			device->SetCullFaceMode(Material->GetCullFace());
			cullFace = Material->GetCullFace();
			cullFaceChanged = false;
		}

		// Check if Material is WireFrame
		(Material->IsWireFrame() ? EnableWireFrame() : DisableWireFrame());

		// Material Render Method
		Material->Render();
	}

	if (LastMeshRenderedPTR != rmesh && (InternalDrawType == -1 || InternalDrawType != rmesh->GetDrawingType()))
	{
		// getting material drawing type
		DrawType = device->TranslateDrawType(rmesh->GetDrawingType());
		InternalDrawType = rmesh->GetDrawingType();
	}

	// Send User Uniforms
	SendUserUniforms(rmesh, Material);

	// Send Model Specific Uniforms
	SendModelUniforms(rmesh, Material);

	// Send Extra (UBO-wrapped) Uniforms - see IMaterial.h's comment on
	// extraUniformsBinding. No-op for every material except the ones that
	// opt in (DeferredRenderer's second-pass lighting materials).
	SendExtraUniforms(rmesh, Material);

	// Depth Write
	if (Material->IsDepthWritting() != depthWritting)
	{
		depthWritting = Material->IsDepthWritting();
		DepthWrite();
	}

	// Depth Test
	if (Material->IsDepthTesting() != depthTesting || Material->depthTestMode != depthTestMode)
	{
		depthTesting = Material->IsDepthTesting();
		DepthTest(Material->depthTestMode);
	}

	// Enable / Disable Blending
	if (Material->blending || Material->IsTransparent())
	{
		// Default for Transparency
		uint32 s = BlendFunc::Src_Alpha;
		uint32 d = BlendFunc::One_Minus_Src_Alpha;
		uint32 m = BlendEq::Add;

		// Override for transparency
		if (Material->blending)
		{
			s = Material->sfactor;
			d = Material->dfactor;
			m = Material->mode;
		}

		if (!blending || s != sfactor || d != dfactor || m != mode)
		{
			EnableBlending();
			BlendingEquation(m);
			BlendingFunction(s, d);
		}
	}
	else if (blending && (!Material->IsTransparent() || !Material->blending)) DisableBlending();

	// Draw
	#if !defined(GLES3)
		if (rmesh->renderingComponent->IsInstanced())
		{
			device->DrawElementsInstanced(cmd, DrawType, rmesh->Geometry->GetIndexData().size(), ((IRenderingInstancedComponent*)rmesh->renderingComponent)->NumberOfInstances());
		}
		else {
			device->DrawElements(cmd, DrawType, rmesh->Geometry->GetIndexData().size());
		}
	#else

		device->DrawElements(cmd, DrawType, rmesh->Geometry->GetIndexData().size());

	#endif

	device->EndCommandBuffer(cmd);

	// Save Last Material and Mesh
	LastProgramUsed = Material->GetShader();
	LastMaterialPTR = Material;
	LastMaterialUsed = Material->GetInternalID();
	LastMeshRendered = rmesh->Geometry->GetInternalID();
	LastMeshRenderedPTR = rmesh;

	if (Material->depthBias)
		DisableDepthBias();
}

void IRenderer::EnableSorting()
{
	sorting = true;
}

void IRenderer::DisableSorting()
{
	sorting = false;
}

void IRenderer::ClearBufferBit(const uint32 Option)
{
	glBufferOptions = device->TranslateBufferBit(Option);
	bufferOptions = Option;
}

void IRenderer::DrawBackground()
{
	if (BackgroundColorSet)
		device->SetClearColor(BackgroundColor);
}

void IRenderer::DepthTest(const uint32 test)
{
	depthTestMode = test;
	device->SetDepthTest(depthTesting, test);
}

void IRenderer::DepthWrite()
{
	device->SetDepthMask(depthWritting);
}

void IRenderer::EnableClearDepthBuffer()
{
	clearDepthBuffer = true;
}

void IRenderer::DisableClearDepthBuffer()
{
	clearDepthBuffer = false;
}

void IRenderer::ClearDepthBuffer()
{
	if (clearDepthBuffer) {
		device->PrepareDepthClear();
	}
}

void IRenderer::EnableStencil()
{
	device->SetStencilTestEnabled(true);
}

void IRenderer::DisableStencil()
{
	device->SetStencilTestEnabled(false);
}

void IRenderer::ClearStencilBuffer()
{
	device->SetClearStencilValue();
}

void IRenderer::StencilFunction(const uint32 func, const uint32 ref, const uint32 mask)
{
	device->SetStencilFunction(func, ref, mask);
}

void IRenderer::StencilOperation(const uint32 sfail, const uint32 dpfail, const uint32 dppass)
{
	device->SetStencilOperation(sfail, dpfail, dppass);
}

void IRenderer::ColorMask(const bool r, const bool g, const bool b, const bool a)
{
	device->SetColorMask(r, g, b, a);
}

void IRenderer::ClearScreen()
{
	device->Clear(glBufferOptions);
}

void IRenderer::SetGlobalLight(const Vec4& Light)
{
	GlobalLight = Light;
}

void IRenderer::EnableDepthBias(const Vec2& Bias)
{
	if (!IsUsingDepthBias)
	{
		IsUsingDepthBias = true;
		device->SetPolygonOffsetEnabled(true);    // enable polygon offset fill to combat "z-fighting"
	}
	device->SetPolygonOffset(Bias.x, Bias.y);
}

void IRenderer::DisableDepthBias()
{
	if (IsUsingDepthBias)
	{
		IsUsingDepthBias = false;
		device->SetPolygonOffsetEnabled(false);
	}
}

void IRenderer::EnableBlending()
{
	if (!blending)
	{
		// Enable Blending
		device->SetBlendingEnabled(true);
		blending = true;
	}
}

void IRenderer::DisableBlending()
{
	if (blending)
	{
		// Disables Blending
		device->SetBlendingEnabled(false);
		blending = false;
		sfactor = dfactor = mode = -1;
	}
}

void IRenderer::BlendingFunction(const uint32 sfactor, const uint32 dfactor)
{
	this->sfactor = sfactor;
	this->dfactor = dfactor;
	device->SetBlendFunction(sfactor, dfactor);
}

void IRenderer::EnableScissorTest()
{
	scissorTest = true;
}

void IRenderer::DisableScissorTest()
{
	scissorTest = false;
}

void IRenderer::ScissorTestRect(const f32 x, const f32 y, const f32 width, const f32 height)
{
	scissorTestX = x;
	scissorTestY = y;
	scissorTestWidth = width;
	scissorTestHeight = height;
}

void IRenderer::BlendingEquation(const uint32 mode)
{
	this->mode = mode;
	device->SetBlendEquation(mode);
}

void IRenderer::EnableWireFrame()
{
	device->SetWireFrame(true);
}

void IRenderer::DisableWireFrame()
{
	device->SetWireFrame(false);
}

void IRenderer::EnableClipPlane(const uint32 &numberOfClipPlanes)
{
	ClipPlane = true;
	ClipPlaneNumber = numberOfClipPlanes;
}

void IRenderer::DisableClipPlane()
{
	ClipPlane = false;
}

void IRenderer::StartClippingPlanes()
{
	if (ClipPlane)
	{
		for (uint32 k = 0; k < ClipPlaneNumber; k++)
			device->EnableClipDistance(k);
	}
}

void IRenderer::EndClippingPlanes()
{
	if (ClipPlane)
	{
		for (uint32 k = 0; k < ClipPlaneNumber; k++)
			device->DisableClipDistance(k);
	}
}

void IRenderer::StartScissorTest()
{
	if (scissorTest)
	{
		device->SetScissorRect(scissorTestX, scissorTestY, scissorTestWidth, scissorTestHeight);
		device->SetScissorTestEnabled(true);
	}
}

void IRenderer::EndScissorTest()
{
	if (scissorTest)
	{
		device->SetScissorTestEnabled(false);
	}
}

void IRenderer::SetClipPlane0(const Vec4 &clipPlane)
{
	ClipPlanes[0] = clipPlane;
}

void IRenderer::SetClipPlane1(const Vec4 &clipPlane)
{
	ClipPlanes[1] = clipPlane;
}

void IRenderer::SetClipPlane2(const Vec4 &clipPlane)
{
	ClipPlanes[2] = clipPlane;
}

void IRenderer::SetClipPlane3(const Vec4 &clipPlane)
{
	ClipPlanes[3] = clipPlane;
}

void IRenderer::SetClipPlane4(const Vec4 &clipPlane)
{
	ClipPlanes[4] = clipPlane;
}

void IRenderer::SetClipPlane5(const Vec4 &clipPlane)
{
	ClipPlanes[5] = clipPlane;
}

void IRenderer::SetClipPlane6(const Vec4 &clipPlane)
{
	ClipPlanes[6] = clipPlane;
}

void IRenderer::SetClipPlane7(const Vec4 &clipPlane)
{
	ClipPlanes[7] = clipPlane;
}

void IRenderer::SetBackground(const Vec4& Color)
{
	BackgroundColor = Color;
	BackgroundColorSet = true;
}

void IRenderer::UnsetBackground()
{
	BackgroundColorSet = false;
}

void IRenderer::ActivateCulling(const uint32 cullingType)
{
	// Releases any previously active FrustumCulling first, so calling this
	// twice without a DeactivateCulling() in between can't leak.
	culling.reset(new FrustumCulling());
	IsCulling = true;
}

void IRenderer::DeactivateCulling()
{
	IsCulling = false;
	culling.reset();
}

bool IRenderer::CullingSphereTest(RenderingMesh* rmesh, GameObject* owner)
{
	return culling->SphereInFrustum(owner->GetWorldPosition(), owner->GetBoundingSphereRadiusWorldSpace());
}

bool IRenderer::CullingBoxTest(RenderingMesh* rmesh, GameObject* owner)
{
	AABox aabb = AABox(owner->GetBoundingMinValueWorldSpace(), owner->GetBoundingMaxValueWorldSpace());

	// Return test
	return culling->ABoxInFrustum(aabb);
}

bool IRenderer::CullingPointTest(RenderingMesh* rmesh, GameObject* owner)
{
	return culling->PointInFrustum(owner->GetWorldPosition());
}

void IRenderer::UpdateCulling(const Matrix& ViewProjectionMatrix)
{
	culling->Update(ViewProjectionMatrix);
}

void IRenderer::SendGlobalUniforms(RenderingMesh* rmesh, IMaterial* Material)
{
	// Upload uProjectionMatrix + uViewMatrix into the shared UBO, but only
	// when they've actually changed since the last upload (compared
	// byte-for-byte against CachedProjectionMatrix/CachedViewMatrix) -
	// skips the GPU upload on the (common) case of several consecutive
	// mesh/material switches within the same pass. Shadow passes, which
	// reassign ProjectionMatrix/ViewMatrix to the shadow-casting light's
	// view before calling this again for the shadow material, still upload
	// correctly - they just don't match the cache, so the comparison itself
	// is what decides freshness, not any assumption about when these change.
	if (!GlobalMatricesUBOValid ||
		memcmp(&CachedProjectionMatrix, &ProjectionMatrix, sizeof(Matrix)) != 0 ||
		memcmp(&CachedViewMatrix, &ViewMatrix, sizeof(Matrix)) != 0 ||
		CachedRenderingPointShadowFace != RenderingPointShadowFace)
	{
		// TranslateProjectionMatrix() is a no-op on GL (Matrix::PerspectiveMatrix()/
		// OrthoMatrix() already build GL's own NDC convention) and applies
		// Vulkan's Z-range/Y-flip correction on that backend - see the
		// comment on IRenderDevice::TranslateProjectionMatrix(). The dirty-
		// check above deliberately compares the untranslated ProjectionMatrix,
		// not this - the translation is a pure backend-specific function of
		// it, so "did the source change" is still the right question (plus
		// RenderingPointShadowFace, since it also affects the translation).
		Matrix globalMatricesData[2] = { device->TranslateProjectionMatrix(ProjectionMatrix, RenderingPointShadowFace), ViewMatrix };
		device->ReplaceUniformBuffer(GlobalMatricesUBO, sizeof(Matrix) * 2, globalMatricesData);
		CachedProjectionMatrix = ProjectionMatrix;
		CachedViewMatrix = ViewMatrix;
		CachedRenderingPointShadowFace = RenderingPointShadowFace;
		GlobalMatricesUBOValid = true;
	}

	// Same idea for uLights[]. Lights/NumberOfLights are rebuilt per object
	// (each object only gets its nearby lights - see ForwardRenderer/
	// DeferredRenderer's RenderScene()), so in practice this will usually
	// find a change and upload anyway, but it's free insurance for the
	// case where consecutive objects happen to see the same light set.
	if (Lights.size() > 0)
	{
		uint32 lightsToUpload = NumberOfLights < PYROS_MAX_LIGHTS ? NumberOfLights : PYROS_MAX_LIGHTS;
		if (!LightsUBOValid || CachedLights.size() != lightsToUpload ||
			memcmp(&CachedLights[0], &Lights[0], sizeof(Matrix) * lightsToUpload) != 0)
		{
			// ReplaceUniformBuffer, not UpdateUniformBuffer: this fires
			// effectively every object in a lit scene (see the comment
			// above), so glBufferSubData's pipeline-stall risk applies
			// here too - the trailing unused slots (lightsToUpload <
			// PYROS_MAX_LIGHTS) are never read since the shader loop is
			// gated by uNumberOfLights, so orphaning them is harmless.
			device->ReplaceUniformBuffer(LightsUBO, sizeof(Matrix) * lightsToUpload, &Lights[0]);
			CachedLights.assign(Lights.begin(), Lights.begin() + lightsToUpload);
			LightsUBOValid = true;
		}
	}

	// Shadow matrices: computed once at the start of RenderScene() (not
	// per-object like Lights), so these are constant across a whole main
	// pass - this is the case where skipping redundant uploads actually
	// matters, since the old code resent all three arrays on every single
	// mesh/material switch for the entire pass regardless.
	if (DirectionalShadowMatrix.size() > 0)
	{
		uint32 count = DirectionalShadowMatrix.size() < PYROS_MAX_DIRECTIONAL_SHADOW_CASCADES ? DirectionalShadowMatrix.size() : PYROS_MAX_DIRECTIONAL_SHADOW_CASCADES;
		if (!DirectionalShadowUBOValid || CachedDirectionalShadowMatrix.size() != count ||
			memcmp(&CachedDirectionalShadowMatrix[0], &DirectionalShadowMatrix[0], sizeof(Matrix) * count) != 0 ||
			memcmp(&CachedDirectionalShadowFar, &DirectionalShadowFar, sizeof(Vec4)) != 0)
		{
			device->UpdateUniformBuffer(DirectionalShadowUBO, 0, sizeof(Matrix) * count, &DirectionalShadowMatrix[0]);
			// uDirectionalShadowFar[4] starts right after the matrix array;
			// only element [0] is ever read in the shader (its 4 components
			// are the per-cascade far distances), so only it is uploaded
			// here - matching what the old individual-uniform send did.
			device->UpdateUniformBuffer(DirectionalShadowUBO, sizeof(Matrix) * PYROS_MAX_DIRECTIONAL_SHADOW_CASCADES, sizeof(Vec4), &DirectionalShadowFar);
			CachedDirectionalShadowMatrix.assign(DirectionalShadowMatrix.begin(), DirectionalShadowMatrix.begin() + count);
			CachedDirectionalShadowFar = DirectionalShadowFar;
			DirectionalShadowUBOValid = true;
		}
	}
	if (PointShadowMatrix.size() > 0)
	{
		uint32 count = PointShadowMatrix.size() < PYROS_MAX_POINT_SHADOW_MATRICES ? PointShadowMatrix.size() : PYROS_MAX_POINT_SHADOW_MATRICES;
		if (!PointShadowUBOValid || CachedPointShadowMatrix.size() != count ||
			memcmp(&CachedPointShadowMatrix[0], &PointShadowMatrix[0], sizeof(Matrix) * count) != 0)
		{
			device->ReplaceUniformBuffer(PointShadowUBO, sizeof(Matrix) * count, &PointShadowMatrix[0]);
			CachedPointShadowMatrix.assign(PointShadowMatrix.begin(), PointShadowMatrix.begin() + count);
			PointShadowUBOValid = true;
		}
	}
	if (SpotShadowMatrix.size() > 0)
	{
		uint32 count = SpotShadowMatrix.size() < PYROS_MAX_SPOT_SHADOW_MATRICES ? SpotShadowMatrix.size() : PYROS_MAX_SPOT_SHADOW_MATRICES;
		if (!SpotShadowUBOValid || CachedSpotShadowMatrix.size() != count ||
			memcmp(&CachedSpotShadowMatrix[0], &SpotShadowMatrix[0], sizeof(Matrix) * count) != 0)
		{
			device->ReplaceUniformBuffer(SpotShadowUBO, sizeof(Matrix) * count, &SpotShadowMatrix[0]);
			CachedSpotShadowMatrix.assign(SpotShadowMatrix.begin(), SpotShadowMatrix.begin() + count);
			SpotShadowUBOValid = true;
		}
	}

	// UBOs for PyrosShader.glsl's formerly-loose per-frame uniforms - see
	// IMaterial::SupportsUniformBlocks(). glGetUniformLocation() correctly
	// returns -1 for uniforms that are now block members (they're no
	// longer "active uniform variables" in the GL sense), so the
	// individual Shader::SendUniform() calls in the loop below already
	// naturally no-op for these on a SupportsUniformBlocks() material -
	// nothing needs to be removed there, this just adds the actual upload.
	if (Material->SupportsUniformBlocks())
	{
		if (!VertexFrameUniformsUBOValid || memcmp(&CachedCameraPosition, &CameraPosition, sizeof(Vec3)) != 0)
		{
			Vec4 cameraPosPadded(CameraPosition, 0.0f);
			device->ReplaceUniformBuffer(VertexFrameUniformsUBO, sizeof(Vec4), &cameraPosPadded);
			CachedCameraPosition = CameraPosition;
			VertexFrameUniformsUBOValid = true;
		}
		if (!AmbientLightUniformsUBOValid || memcmp(&CachedGlobalLight, &GlobalLight, sizeof(Vec4)) != 0)
		{
			device->ReplaceUniformBuffer(AmbientLightUniformsUBO, sizeof(Vec4), &GlobalLight);
			CachedGlobalLight = GlobalLight;
			AmbientLightUniformsUBOValid = true;
		}
		if (!VelocityFrameUniformsUBOValid ||
			memcmp(&CachedPrvProjectionMatrix, &PrvProjectionMatrix, sizeof(Matrix)) != 0 ||
			memcmp(&CachedPrvViewMatrix, &PrvViewMatrix, sizeof(Matrix)) != 0)
		{
			Matrix velocityFrameData[2] = { PrvProjectionMatrix, PrvViewMatrix };
			device->ReplaceUniformBuffer(VelocityFrameUniformsUBO, sizeof(Matrix) * 2, velocityFrameData);
			CachedPrvProjectionMatrix = PrvProjectionMatrix;
			CachedPrvViewMatrix = PrvViewMatrix;
			VelocityFrameUniformsUBOValid = true;
		}
	}

	std::vector<int32> *_ShadersGlobalCache = &rmesh->ShadersGlobalCache[Material->GetShader()];

	// Send Global Uniforms
	uint32 counter = 0;
	for (std::list<Uniform>::iterator k = Material->GlobalUniforms.begin(); k != Material->GlobalUniforms.end(); k++)
	{
		if ((*_ShadersGlobalCache)[counter] == -2)
			(*_ShadersGlobalCache)[counter] = Shader::GetUniformLocation(Material->GetShader(), (*k).Name);

		if ((*_ShadersGlobalCache)[counter] >= 0)
		{
			switch ((*k).Usage)
			{
			case Uniforms::DataUsage::ViewMatrix:
				Shader::SendUniform((*k), &ViewMatrix, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::ProjectionMatrix:
				Shader::SendUniform((*k), &ProjectionMatrix, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::ViewProjectionMatrix:
				if (ViewProjectionMatrixIsDirty == true)
				{
					ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
					ViewProjectionMatrixIsDirty = false;
				}
				Shader::SendUniform((*k), &ViewProjectionMatrix, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::ViewMatrixInverse:
				if (ViewMatrixInverseIsDirty == true)
				{
					ViewMatrixInverse = ViewMatrix.Inverse();
					ViewMatrixInverseIsDirty = false;
				}
				Shader::SendUniform((*k), &ViewMatrixInverse, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::ProjectionMatrixInverse:
				if (ProjectionMatrixInverseIsDirty == true)
				{
					ProjectionMatrixInverse = ProjectionMatrix.Inverse();
					ProjectionMatrixInverseIsDirty = false;
				}
				Shader::SendUniform((*k), &ProjectionMatrixInverse, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::ViewProjectionMatrixInverse:
				if (ViewProjectionMatrixInverseIsDirty == true)
				{
					ViewProjectionMatrixInverse = (ProjectionMatrix * ViewMatrix).Inverse();
					ViewProjectionMatrixInverseIsDirty = false;
				}
				Shader::SendUniform((*k), &ProjectionMatrixInverse, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::CameraPosition:
				Shader::SendUniform((*k), &CameraPosition, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::Timer:
			{
				f32 t = (f32)Timer;
				Shader::SendUniform((*k), &t, (*_ShadersGlobalCache)[counter]);
			}
			break;
			case Uniforms::DataUsage::GlobalAmbientLight:
				Shader::SendUniform((*k), &GlobalLight, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::Lights:
				if (Lights.size() > 0)
					Shader::SendUniform((*k), &Lights[0], (*_ShadersGlobalCache)[counter], NumberOfLights);
				break;
			case Uniforms::DataUsage::NumberOfLights:
				Shader::SendUniform((*k), &NumberOfLights, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::NearFarPlane:
				Shader::SendUniform((*k), &NearFarPlane, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::ScreenDimensions:
			{
				Vec2 dim = Vec2((f32)Width, (f32)Height);
				Shader::SendUniform((*k), &dim, (*_ShadersGlobalCache)[counter]);
			}
			break;
			case Uniforms::DataUsage::DirectionalShadowMap:
				if (DirectionalShadowMapsUnits.size() > 0)
					Shader::SendUniform((*k), &DirectionalShadowMapsUnits[0], (*_ShadersGlobalCache)[counter], DirectionalShadowMapsUnits.size());
				break;
			case Uniforms::DataUsage::DirectionalShadowMatrix:
				if (DirectionalShadowMatrix.size() > 0)
					Shader::SendUniform((*k), &DirectionalShadowMatrix[0], (*_ShadersGlobalCache)[counter], DirectionalShadowMatrix.size());
				break;
			case Uniforms::DataUsage::DirectionalShadowFar:
				Shader::SendUniform((*k), &DirectionalShadowFar, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::NumberOfDirectionalShadows:
				Shader::SendUniform((*k), &NumberOfDirectionalShadows, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::PointShadowMap:
				if (PointShadowMapsUnits.size() > 0)
					Shader::SendUniform((*k), &PointShadowMapsUnits[0], (*_ShadersGlobalCache)[counter], PointShadowMapsUnits.size());
				break;
			case Uniforms::DataUsage::PointShadowMatrix:
				Shader::SendUniform((*k), &PointShadowMatrix[0], (*_ShadersGlobalCache)[counter], PointShadowMatrix.size());
				break;
			case Uniforms::DataUsage::NumberOfPointShadows:
				Shader::SendUniform((*k), &NumberOfPointShadows, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::SpotShadowMap:
				Shader::SendUniform((*k), &SpotShadowMapsUnits[0], (*_ShadersGlobalCache)[counter], SpotShadowMapsUnits.size());
				break;
			case Uniforms::DataUsage::SpotShadowMatrix:
				Shader::SendUniform((*k), &SpotShadowMatrix[0], (*_ShadersGlobalCache)[counter], SpotShadowMatrix.size());
				break;
			case Uniforms::DataUsage::NumberOfSpotShadows:
				Shader::SendUniform((*k), &NumberOfSpotShadows, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::ClipPlanes:
				Shader::SendUniform((*k), &ClipPlanes, (*_ShadersGlobalCache)[counter], ClipPlaneNumber);
				break;
			case Uniforms::DataUsage::PrvViewMatrix:
				Shader::SendUniform((*k), &PrvViewMatrix, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::PrvProjectionMatrix:
				Shader::SendUniform((*k), &PrvProjectionMatrix, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::PrvModelViewProjectionMatrix:
				{
					Matrix PrvModelViewProjectionMatrix = PrvProjectionMatrix*PrvViewMatrix*PrvModelMatrix;
					Shader::SendUniform((*k), &PrvModelViewProjectionMatrix, (*_ShadersGlobalCache)[counter]);
				}
				break;
			default:
				Shader::SendUniform((*k), (*_ShadersGlobalCache)[counter]);
				break;
			}
		}
		counter++;
	}
}

// Finds a UserUniforms entry by name and returns a pointer to its raw
// Uniform::Value bytes, or NULL if the material never registered one (e.g.
// GenericShaderMaterial only adds uColor/uSpecular lazily, on the first
// SetColor()/SetSpecular() call - see MaterialUniformsData below).
static const uchar* FindUserUniformValue(const std::list<Uniform> &uniforms, const std::string &name)
{
	for (std::list<Uniform>::const_iterator it = uniforms.begin(); it != uniforms.end(); it++)
		if (it->Name == name && it->Value.size() > 0)
			return &it->Value[0];
	return NULL;
}

// std140 layout matching MaterialUniforms in PyrosShader.glsl exactly (64
// bytes: 2 vec4 + 8 float = 32 + 32 = 64, no implicit std140 tail padding
// left). Metallic/Roughness (PBR) and SSRReflective occupy what used to be
// 3 spare padding floats - block size/binding unchanged.
struct MaterialUniformsData
{
	Vec4 Color;
	Vec4 Specular;
	f32 Opacity;
	f32 Shininess;
	f32 UseLights;
	f32 DisplacementHeight;
	f32 Reflectivity;
	f32 Metallic;
	f32 Roughness;
	// See GenericShaderMaterial::SetSSREnabled()'s comment - real
	// per-material SSR opt-in, not uReflectivity above (unrelated,
	// older env-map/skybox reflection blend amount).
	f32 SSRReflective;
};
static_assert(sizeof(MaterialUniformsData) == 64, "MaterialUniformsData must byte-match PyrosShader.glsl's MaterialUniforms std140 layout exactly");

// std140 layout matching ObjectLightCounts in PyrosShader.glsl exactly (16
// bytes: 3 int = 12, padded to 16 by std140's vec4-multiple block size rule).
struct ObjectLightCountsData
{
	int32 NumberOfLights;
	int32 NumberOfPointShadows;
	int32 NumberOfSpotShadows;
	int32 _pad;
};
static_assert(sizeof(ObjectLightCountsData) == 16, "ObjectLightCountsData must byte-match PyrosShader.glsl's ObjectLightCounts std140 layout exactly");

void IRenderer::SendUserUniforms(RenderingMesh* rmesh, IMaterial* Material)
{
	// UBO for PyrosShader.glsl's formerly-loose material-scalar uniforms -
	// see IMaterial::SupportsUniformBlocks() and the equivalent comment in
	// SendGlobalUniforms(). Values come from the same source the individual
	// send below would otherwise read (Uniform::Value, set by SetColor()/
	// SetSpecular()/SetShininess()/etc - see GenericShaderMaterial.cpp) via
	// name lookup. These fields only change when the material itself
	// changes (SetColor()/etc mutate Material->UserUniforms, not anything
	// per-object), so - unlike ObjectLightCountsUBO in SendModelUniforms(),
	// which genuinely is per-object - this is gated the same way
	// SendGlobalUniforms() is: only re-uploaded on mesh/material switch,
	// not on every RenderObject() call. Safe because SendUserUniforms()
	// runs before RenderObject() updates LastMeshRenderedPTR/LastMaterialPTR
	// (see the end of RenderObject()), so they still hold the *previous*
	// object's pointers here.
	if (Material->SupportsUniformBlocks() && (LastMeshRenderedPTR != rmesh || LastMaterialPTR != Material))
	{
		MaterialUniformsData data = MaterialUniformsData();
		if (const uchar* v = FindUserUniformValue(Material->UserUniforms, "uColor")) memcpy(&data.Color, v, sizeof(Vec4));
		if (const uchar* v = FindUserUniformValue(Material->UserUniforms, "uSpecular")) memcpy(&data.Specular, v, sizeof(Vec4));
		if (const uchar* v = FindUserUniformValue(Material->UserUniforms, "uOpacity")) memcpy(&data.Opacity, v, sizeof(f32));
		if (const uchar* v = FindUserUniformValue(Material->UserUniforms, "uShininess")) memcpy(&data.Shininess, v, sizeof(f32));
		if (const uchar* v = FindUserUniformValue(Material->UserUniforms, "uUseLights")) memcpy(&data.UseLights, v, sizeof(f32));
		if (const uchar* v = FindUserUniformValue(Material->UserUniforms, "uDisplacementHeight")) memcpy(&data.DisplacementHeight, v, sizeof(f32));
		if (const uchar* v = FindUserUniformValue(Material->UserUniforms, "uReflectivity")) memcpy(&data.Reflectivity, v, sizeof(f32));
		if (const uchar* v = FindUserUniformValue(Material->UserUniforms, "uMetallic")) memcpy(&data.Metallic, v, sizeof(f32));
		if (const uchar* v = FindUserUniformValue(Material->UserUniforms, "uRoughness")) memcpy(&data.Roughness, v, sizeof(f32));
		if (const uchar* v = FindUserUniformValue(Material->UserUniforms, "uSSRReflective")) memcpy(&data.SSRReflective, v, sizeof(f32));
		// ReplaceUniformBuffer (not UpdateUniformBuffer) - see
		// IRenderDevice.h's comment on ReplaceUniformBuffer(); still the
		// right call here even though this now only fires on
		// mesh/material switch, since a stale in-flight read is possible
		// any time this buffer is shared across IRenderer instances.
		device->ReplaceUniformBuffer(MaterialUniformsUBO, sizeof(MaterialUniformsData), &data);
	}

	std::vector<int32>* _ShadersUserCache = &rmesh->ShadersUserCache[Material->GetShader()];

	// User Specific Uniforms
	uint32 counter = 0;
	for (std::list<Uniform>::iterator k = Material->UserUniforms.begin(); k != Material->UserUniforms.end(); k++)
	{
		if ((*_ShadersUserCache)[counter] == -2)
			(*_ShadersUserCache)[counter] = Shader::GetUniformLocation(Material->GetShader(), (*k).Name);

		if ((*_ShadersUserCache)[counter] >= 0)
			Shader::SendUniform((*k), (*_ShadersUserCache)[counter]);

		counter++;
	}
}

void IRenderer::SendModelUniforms(RenderingMesh* rmesh, IMaterial* Material)
{
	// UBOs for PyrosShader.glsl's formerly-loose per-object uniforms - see
	// IMaterial::SupportsUniformBlocks() and the equivalent comment in
	// SendGlobalUniforms(). Uploaded unconditionally every call (no dirty
	// check) since ModelMatrix/bones/PrvModelMatrix change on essentially
	// every RenderObject() call anyway - matches how the individual-send
	// loop below already resends its own uniforms unconditionally too.
	if (Material->SupportsUniformBlocks())
	{
		// ReplaceUniformBuffer, not UpdateUniformBuffer - see the comment
		// on SendUserUniforms()'s MaterialUniformsUBO call, same reasoning
		// (these fire every RenderObject() call too). BoneMatrices' write
		// is only ever a prefix starting at offset 0 (bonesToUpload may be
		// less than PYROS_MAX_BONES), and the shader never reads past the
		// bone indices a mesh's vertices actually reference, so orphaning
		// the unwritten tail is harmless - same reasoning already applies
		// to LightsBlock's existing partial writes.
		device->ReplaceUniformBuffer(ObjectMatrixUniformsUBO, sizeof(Matrix), &ModelMatrix);
		if (rmesh->SkinningBones.size() > 0)
		{
			uint32 bonesToUpload = rmesh->SkinningBones.size() < PYROS_MAX_BONES ? rmesh->SkinningBones.size() : PYROS_MAX_BONES;
			device->ReplaceUniformBuffer(BoneMatricesUBO, sizeof(Matrix) * bonesToUpload, &rmesh->SkinningBones[0]);
		}
		device->ReplaceUniformBuffer(VelocityObjectUniformsUBO, sizeof(Matrix), &PrvModelMatrix);

		// uNumberOfLights/uNumberOfPointShadows/uNumberOfSpotShadows -
		// split out of MaterialUniformsUBO (see the struct/comment in
		// SendUserUniforms()) because these are genuinely per-object: each
		// object gets its own nearby-lights count from the renderer's
		// light-culling loop (see RenderObject()), so - unlike the rest of
		// MaterialUniforms - they can't be gated on mesh/material change
		// without going stale.
		ObjectLightCountsData lightCounts;
		lightCounts.NumberOfLights = (int32)NumberOfLights;
		lightCounts.NumberOfPointShadows = (int32)NumberOfPointShadows;
		lightCounts.NumberOfSpotShadows = (int32)NumberOfSpotShadows;
		device->ReplaceUniformBuffer(ObjectLightCountsUBO, sizeof(ObjectLightCountsData), &lightCounts);
	}

	uint32 counter = 0;

	std::vector<int32>* _ShadersModelCache = &rmesh->ShadersModelCache[Material->GetShader()];

	for (std::list<Uniform>::iterator k = Material->ModelUniforms.begin(); k != Material->ModelUniforms.end(); k++)
	{
		if ((*_ShadersModelCache)[counter] == -2)
			(*_ShadersModelCache)[counter] = Shader::GetUniformLocation(Material->GetShader(), (*k).Name);

		if ((*_ShadersModelCache)[counter] >= 0)
		{
			switch ((*k).Usage)
			{
			case Uniforms::DataUsage::ModelMatrix:
				Shader::SendUniform((*k), &ModelMatrix, (*_ShadersModelCache)[counter]);
				break;
			case Uniforms::DataUsage::NormalMatrix:
				if (NormalMatrixIsDirty == true)
				{
					NormalMatrix = (ViewMatrix*ModelMatrix);
					NormalMatrixIsDirty = false;
				}
				Shader::SendUniform((*k), &NormalMatrix, (*_ShadersModelCache)[counter]);
				break;
			case Uniforms::DataUsage::ModelViewMatrix:
				if (ModelViewMatrixIsDirty == true)
				{
					ModelViewMatrix = ViewMatrix*ModelMatrix;
					ModelViewMatrixIsDirty = false;
				}
				Shader::SendUniform((*k), &ModelViewMatrix, (*_ShadersModelCache)[counter]);
				break;
			case Uniforms::DataUsage::ModelViewProjectionMatrix:
				if (ModelViewProjectionMatrixIsDirty == true)
				{
					ModelViewProjectionMatrix = ProjectionMatrix*ViewMatrix*ModelMatrix;
					ModelViewProjectionMatrixIsDirty = false;
				}
				Shader::SendUniform((*k), &ModelViewProjectionMatrix, (*_ShadersModelCache)[counter]);
				break;
			case Uniforms::DataUsage::ModelMatrixInverse:
				if (ModelMatrixInverseIsDirty == true)
				{
					ModelMatrixInverse = ModelMatrix.Inverse();
					ModelMatrixInverseIsDirty = false;
				}
				Shader::SendUniform((*k), &ModelMatrixInverse, (*_ShadersModelCache)[counter]);
				break;
			case Uniforms::DataUsage::ModelViewMatrixInverse:
				if (ModelViewMatrixInverseIsDirty == true)
				{
					ModelViewMatrixInverse = (ViewMatrix*ModelMatrix).Inverse();
					ModelViewMatrixInverseIsDirty = false;
				}
				Shader::SendUniform((*k), &ModelViewMatrixInverse, (*_ShadersModelCache)[counter]);
				break;
			case Uniforms::DataUsage::ModelMatrixInverseTranspose:
				if (ModelMatrixInverseTransposeIsDirty == true)
				{
					ModelMatrixInverseTranspose = ModelMatrixInverse.Transpose();
					ModelMatrixInverseTransposeIsDirty = false;
				}
				Shader::SendUniform((*k), &ModelMatrixInverseTranspose, (*_ShadersModelCache)[counter]);
				break;
			case Uniforms::DataUsage::Skinning:
			{
				if (rmesh->SkinningBones.size() > 0)
					Shader::SendUniform((*k), &rmesh->SkinningBones[0], (*_ShadersModelCache)[counter], rmesh->SkinningBones.size());
			}
			break;
			case Uniforms::DataUsage::ModelViewProjectionMatrixInverse:
				if (ModelViewProjectionMatrixInverseIsDirty == true)
				{
					ModelViewProjectionMatrixInverse = (ProjectionMatrix * ViewMatrix * ModelMatrix).Inverse();
					ModelViewProjectionMatrixInverseIsDirty = false;
				}
				Shader::SendUniform((*k), &ModelViewProjectionMatrixInverse, (*_ShadersModelCache)[counter]);
			break;
			case Uniforms::DataUsage::PrvModelMatrix:
				Shader::SendUniform((*k), &PrvModelMatrix, (*_ShadersModelCache)[counter]);
				break;
			}
		}
		counter++;
	}
}

void IRenderer::CaptureExtraUniform(IMaterial* Material, const Uniform &u)
{
	Vec2 screenDimensions((f32)Width, (f32)Height);
	f32 timerF = (f32)Timer;
	// SendGlobalUniforms()'s GlobalMatricesUBO write (this file, ~line 1371)
	// always runs every ProjectionMatrix/PrvProjectionMatrix use through
	// device->TranslateProjectionMatrix() (Vulkan's Y-flip + [-1,1]->[0,1]
	// Z remap; a no-op on GL) before it reaches a shader - every DataUsage
	// below that's built from either raw matrix has to do the same, or a
	// CustomShaderMaterial reading it via extraUniforms (the only way a
	// shader ever sees these on Vulkan - see this function's own class
	// comment) gets GL-only clip space and renders in the wrong place.
	// Found via p3d::ParticleSystem's billboard rendering near the floor
	// instead of at its emitter's height on Vulkan only - GL doesn't need
	// the translation so it never showed the bug; a same-position "marker"
	// test object using the regular GlobalMatrices-UBO path rendered
	// correctly the whole time, isolating the bug to exactly this
	// function.
	Matrix translatedProjectionMatrix = device->TranslateProjectionMatrix(ProjectionMatrix);
	Matrix translatedPrvProjectionMatrix = device->TranslateProjectionMatrix(PrvProjectionMatrix);
	Matrix prvModelViewProjectionMatrix = translatedPrvProjectionMatrix * PrvViewMatrix * PrvModelMatrix;

	// Mirrors SendGlobalUniforms()/SendModelUniforms()'s switches - those
	// two only ever reach a *regular* (non-extra) uniform (Shader::
	// SendUniform, a no-op on Vulkan once absorbed into a UBO - see
	// GetUniformLocation()'s comment), so any DataUsage they compute
	// live has to be duplicated here too, or a CustomShaderMaterial that
	// puts one of these in extraUniforms[] silently reads stale/zero
	// data on Vulkan forever (found via IslandDemo's water going static
	// - uTime/uCameraPos were falling through to the `default` case
	// below, which only ever reads u.Value - never populated for a
	// DataUsage that's meant to be computed by the renderer itself, not
	// hand-set via SetValue()). Dirty-tracked matrices use the exact
	// same lazy-recompute-if-dirty pattern as the two switches above,
	// since SendExtraUniforms() runs after both but can't assume either
	// one already visited the same usage this frame (a material might
	// reference a usage *only* via extraUniforms, with no matching
	// regular AddUniform() of the same DataUsage).
	const void* valuePtr = NULL;
	uint32 valueSize = 0;
	switch (u.Usage)
	{
	case Uniforms::DataUsage::ViewMatrix:
		valuePtr = &ViewMatrix; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::ProjectionMatrix:
		valuePtr = &translatedProjectionMatrix; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::ViewProjectionMatrix:
		if (ViewProjectionMatrixIsDirty) { ViewProjectionMatrix = translatedProjectionMatrix * ViewMatrix; ViewProjectionMatrixIsDirty = false; }
		valuePtr = &ViewProjectionMatrix; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::ViewMatrixInverse:
		if (ViewMatrixInverseIsDirty) { ViewMatrixInverse = ViewMatrix.Inverse(); ViewMatrixInverseIsDirty = false; }
		valuePtr = &ViewMatrixInverse; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::ProjectionMatrixInverse:
		if (ProjectionMatrixInverseIsDirty) { ProjectionMatrixInverse = translatedProjectionMatrix.Inverse(); ProjectionMatrixInverseIsDirty = false; }
		valuePtr = &ProjectionMatrixInverse; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::ViewProjectionMatrixInverse:
		if (ViewProjectionMatrixInverseIsDirty) { ViewProjectionMatrixInverse = (translatedProjectionMatrix * ViewMatrix).Inverse(); ViewProjectionMatrixInverseIsDirty = false; }
		valuePtr = &ViewProjectionMatrixInverse; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::CameraPosition:
		valuePtr = &CameraPosition; valueSize = sizeof(CameraPosition);
		break;
	case Uniforms::DataUsage::Timer:
		valuePtr = &timerF; valueSize = sizeof(timerF);
		break;
	case Uniforms::DataUsage::GlobalAmbientLight:
		valuePtr = &GlobalLight; valueSize = sizeof(GlobalLight);
		break;
	case Uniforms::DataUsage::NumberOfLights:
		valuePtr = &NumberOfLights; valueSize = sizeof(NumberOfLights);
		break;
	case Uniforms::DataUsage::NearFarPlane:
		valuePtr = &NearFarPlane; valueSize = sizeof(NearFarPlane);
		break;
	case Uniforms::DataUsage::ScreenDimensions:
		valuePtr = &screenDimensions; valueSize = sizeof(screenDimensions);
		break;
	case Uniforms::DataUsage::DirectionalShadowFar:
		valuePtr = &DirectionalShadowFar; valueSize = sizeof(DirectionalShadowFar);
		break;
	case Uniforms::DataUsage::NumberOfDirectionalShadows:
		valuePtr = &NumberOfDirectionalShadows; valueSize = sizeof(NumberOfDirectionalShadows);
		break;
	case Uniforms::DataUsage::NumberOfPointShadows:
		valuePtr = &NumberOfPointShadows; valueSize = sizeof(NumberOfPointShadows);
		break;
	case Uniforms::DataUsage::NumberOfSpotShadows:
		valuePtr = &NumberOfSpotShadows; valueSize = sizeof(NumberOfSpotShadows);
		break;
	case Uniforms::DataUsage::PrvViewMatrix:
		valuePtr = &PrvViewMatrix; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::PrvProjectionMatrix:
		valuePtr = &translatedPrvProjectionMatrix; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::PrvModelViewProjectionMatrix:
		valuePtr = &prvModelViewProjectionMatrix; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::ModelMatrix:
		valuePtr = &ModelMatrix; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::NormalMatrix:
		if (NormalMatrixIsDirty) { NormalMatrix = ViewMatrix * ModelMatrix; NormalMatrixIsDirty = false; }
		valuePtr = &NormalMatrix; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::ModelViewMatrix:
		if (ModelViewMatrixIsDirty) { ModelViewMatrix = ViewMatrix * ModelMatrix; ModelViewMatrixIsDirty = false; }
		valuePtr = &ModelViewMatrix; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::ModelViewProjectionMatrix:
		if (ModelViewProjectionMatrixIsDirty) { ModelViewProjectionMatrix = translatedProjectionMatrix * ViewMatrix * ModelMatrix; ModelViewProjectionMatrixIsDirty = false; }
		valuePtr = &ModelViewProjectionMatrix; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::ModelMatrixInverse:
		if (ModelMatrixInverseIsDirty) { ModelMatrixInverse = ModelMatrix.Inverse(); ModelMatrixInverseIsDirty = false; }
		valuePtr = &ModelMatrixInverse; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::ModelViewMatrixInverse:
		if (ModelViewMatrixInverseIsDirty) { ModelViewMatrixInverse = (ViewMatrix * ModelMatrix).Inverse(); ModelViewMatrixInverseIsDirty = false; }
		valuePtr = &ModelViewMatrixInverse; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::ModelMatrixInverseTranspose:
		if (ModelMatrixInverseTransposeIsDirty) { ModelMatrixInverseTranspose = ModelMatrixInverse.Transpose(); ModelMatrixInverseTransposeIsDirty = false; }
		valuePtr = &ModelMatrixInverseTranspose; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::ModelViewProjectionMatrixInverse:
		if (ModelViewProjectionMatrixInverseIsDirty) { ModelViewProjectionMatrixInverse = (translatedProjectionMatrix * ViewMatrix * ModelMatrix).Inverse(); ModelViewProjectionMatrixInverseIsDirty = false; }
		valuePtr = &ModelViewProjectionMatrixInverse; valueSize = sizeof(Matrix);
		break;
	case Uniforms::DataUsage::PrvModelMatrix:
		valuePtr = &PrvModelMatrix; valueSize = sizeof(Matrix);
		break;
	default:
		valuePtr = u.Value.empty() ? NULL : &u.Value[0];
		valueSize = (uint32)u.Value.size();
		break;
	}
	if (valuePtr == NULL)
		return;

	for (int i = 0; i < 2; i++)
	{
		IMaterial::ExtraUniformsBlock &block = Material->extraUniforms[i];
		if (block.binding == 0)
			continue;
		std::map<std::string, uint32>::const_iterator offIt = block.offsets.find(u.Name);
		if (offIt != block.offsets.end() && offIt->second + valueSize <= block.scratch.size())
			memcpy(&block.scratch[offIt->second], valuePtr, valueSize);
	}
}

void IRenderer::SendExtraUniforms(RenderingMesh* rmesh, IMaterial* Material)
{
	if (Material->extraUniforms[0].binding == 0 && Material->extraUniforms[1].binding == 0)
		return;

	for (std::list<Uniform>::const_iterator k = Material->GlobalUniforms.begin(); k != Material->GlobalUniforms.end(); k++)
		CaptureExtraUniform(Material, *k);
	for (std::list<Uniform>::const_iterator k = Material->UserUniforms.begin(); k != Material->UserUniforms.end(); k++)
		CaptureExtraUniform(Material, *k);
	for (std::list<Uniform>::const_iterator k = Material->ModelUniforms.begin(); k != Material->ModelUniforms.end(); k++)
		CaptureExtraUniform(Material, *k);

	for (int i = 0; i < 2; i++)
	{
		IMaterial::ExtraUniformsBlock &block = Material->extraUniforms[i];
		if (block.binding == 0)
			continue;
		if (block.bufferHandle == 0)
			block.bufferHandle = device->CreateUniformBuffer(block.size, block.binding);
		device->BindUniformBlockIfPresent(Material->GetShader(), block.blockName, block.binding);
		device->ReplaceUniformBuffer(block.bufferHandle, block.size, &block.scratch[0]);
	}
}

void IRenderer::BindMesh(RenderingMesh* rmesh, IMaterial* material)
{
	std::vector< std::vector<int32> >* _ShadersAttributesCache = &rmesh->ShadersAttributesCache[material->GetShader()];
	if ((*_ShadersAttributesCache).size()==0)
	{
		// Reset Attribute IDs
		for (std::vector<AttributeArray*>::iterator i = rmesh->Geometry->Attributes.begin(); i != rmesh->Geometry->Attributes.end(); i++)
		{
			std::vector<int32> attribs;
			for (std::vector<VertexAttribute*>::iterator k = (*i)->Attributes.begin(); k != (*i)->Attributes.end(); k++)
			{
				attribs.push_back(Shader::GetAttributeLocation(material->GetShader(), (*k)->Name));
			}
			(*_ShadersAttributesCache).push_back(attribs);
		}

		std::vector<int32>* _ShadersGlobalCache = &rmesh->ShadersGlobalCache[material->GetShader()];
		for (std::list<Uniform>::iterator k = material->GlobalUniforms.begin(); k != material->GlobalUniforms.end(); k++)
		{
			(*_ShadersGlobalCache).push_back(Shader::GetUniformLocation(material->GetShader(), (*k).Name));
		}

		std::vector<int32>* _ShadersModelCache = &rmesh->ShadersModelCache[material->GetShader()];
		for (std::list<Uniform>::iterator k = material->ModelUniforms.begin(); k != material->ModelUniforms.end(); k++)
		{
			(*_ShadersModelCache).push_back(Shader::GetUniformLocation(material->GetShader(), (*k).Name));
		}

		std::vector<int32>* _ShadersUserCache = &rmesh->ShadersUserCache[material->GetShader()];
		for (std::list<Uniform>::iterator k = material->UserUniforms.begin(); k != material->UserUniforms.end(); k++)
		{
			(*_ShadersUserCache).push_back(Shader::GetUniformLocation(material->GetShader(), (*k).Name));
		}
	}

	// Build and cache a VAO for this (mesh, shader) pair the first time
	// it's seen, baking in every attribute's enable/pointer/divisor state
	// plus the bound index buffer. RenderObject() just glBindVertexArray()s
	// this afterward instead of re-issuing all of that per mesh switch.
	if (rmesh->VAOCache.find(material->GetShader()) == rmesh->VAOCache.end())
	{
		DeviceHandle vao = device->CreateVertexArray();
		CommandBufferHandle bindMeshCmd = device->BeginCommandBuffer();
		device->BindVertexArray(bindMeshCmd, vao);

		if (rmesh->Geometry->Attributes.size() > 0)
		{
			uint32 counterBuffers = 0;
			for (std::vector<AttributeArray*>::iterator k = rmesh->Geometry->Attributes.begin(); k != rmesh->Geometry->Attributes.end(); k++)
			{
				AttributeBuffer* bf = (AttributeBuffer*)(*k);

				device->BindArrayBuffer(bf->Buffer->ID);

				if (bf->attributeSize == 0)
				{
					for (std::vector<VertexAttribute*>::iterator l = (*k)->Attributes.begin(); l != (*k)->Attributes.end(); l++)
					{
						bf->attributeSize += (*l)->byteSize;
					}
				}

				uint32 counter = 0;
				for (std::vector<VertexAttribute*>::iterator l = (*k)->Attributes.begin(); l != (*k)->Attributes.end(); l++)
				{
					if ((*_ShadersAttributesCache)[counterBuffers][counter] == -2)
					{
						(*_ShadersAttributesCache)[counterBuffers][counter] = Shader::GetAttributeLocation(material->GetShader(), (*l)->Name);
					}
					int32 location = (*_ShadersAttributesCache)[counterBuffers][counter];
					if (location >= 0)
					{
						uint32 typeCount = Buffer::Attribute::GetTypeCount((*l)->Type);
						uint32 nativeType = Buffer::Attribute::GetType((*l)->Type);

						device->SetVertexAttribute(location, typeCount, nativeType, bf->attributeSize, (*l)->Offset);
						if ((*l)->Type==Buffer::Attribute::Type::Matrix)
						{
							device->SetVertexAttribute(location+1, typeCount, nativeType, bf->attributeSize, 16);
							device->SetVertexAttribute(location+2, typeCount, nativeType, bf->attributeSize, 32);
							device->SetVertexAttribute(location+3, typeCount, nativeType, bf->attributeSize, 48);
						}

						if (rmesh->renderingComponent->IsInstanced())
						{
							device->SetVertexAttributeDivisor(location, (*l)->VertexDivisor);
							if ((*l)->Type==Buffer::Attribute::Type::Matrix)
							{
								device->SetVertexAttributeDivisor(location+1, (*l)->VertexDivisor);
								device->SetVertexAttributeDivisor(location+2, (*l)->VertexDivisor);
								device->SetVertexAttributeDivisor(location+3, (*l)->VertexDivisor);
							}
						}
					}
					counter++;
				}
				counterBuffers++;
			}
		}

		// Bind the index buffer into the VAO's own state too, so it doesn't
		// need rebinding on every mesh switch either.
		device->BindElementBuffer(rmesh->Geometry->IndexBuffer->ID);

		device->BindVertexArray(bindMeshCmd, 0);
		device->EndCommandBuffer(bindMeshCmd);

		rmesh->VAOCache[material->GetShader()] = vao;

		// Wire this shader's GlobalMatrices/LightsBlock uniform blocks (if
		// it declares them - only PyrosShader.glsl does; custom materials'
		// shaders keep sending these as plain uniforms and won't have these
		// blocks) to their UBOs' binding points.
		device->BindUniformBlockIfPresent(material->GetShader(), "GlobalMatrices", 0);
		device->BindUniformBlockIfPresent(material->GetShader(), "LightsBlock", 1);
		device->BindUniformBlockIfPresent(material->GetShader(), "DirectionalShadowBlock", 2);
		device->BindUniformBlockIfPresent(material->GetShader(), "PointShadowBlock", 3);
		device->BindUniformBlockIfPresent(material->GetShader(), "SpotShadowBlock", 4);
		// Same idea for the formerly-loose uniforms' new blocks (see
		// SupportsUniformBlocks() in IMaterial.h) - safe no-ops for any
		// shader that doesn't declare them, e.g. CustomShaderMaterial's.
		device->BindUniformBlockIfPresent(material->GetShader(), "VertexFrameUniforms", 16);
		device->BindUniformBlockIfPresent(material->GetShader(), "VelocityFrameUniforms", 17);
		device->BindUniformBlockIfPresent(material->GetShader(), "ObjectMatrixUniforms", 18);
		device->BindUniformBlockIfPresent(material->GetShader(), "BoneMatrices", 19);
		device->BindUniformBlockIfPresent(material->GetShader(), "VelocityObjectUniforms", 20);
		device->BindUniformBlockIfPresent(material->GetShader(), "AmbientLightUniforms", 21);
		device->BindUniformBlockIfPresent(material->GetShader(), "MaterialUniforms", 22);
		device->BindUniformBlockIfPresent(material->GetShader(), "ObjectLightCounts", 23);
	}

	// Vulkan pipeline for this (mesh, shader, render target) triple - see
	// the comment on RenderingMesh::PipelineCache. Deliberately gated on
	// its *own* cache key, not folded into the VAOCache-gated block above -
	// a VAO is render-target-agnostic (pure vertex-attribute layout), but
	// a Vulkan pipeline bakes in a specific render pass's attachment shape,
	// so the first time this (mesh, shader) pair is drawn into a *new*
	// render target, a VAO already exists (skipping the block above
	// entirely) while a pipeline for this target still doesn't - checking
	// only VAOCache here would silently leave PipelineCache[key] unset,
	// and the next BindPipeline() call would fail with "pipeline handle 0
	// not found" (found via a live regression once color-attachment FBOs
	// started working at all and a mesh got drawn into two different
	// targets for the first time). Built from Material's state right now,
	// not re-evaluated per object the way RenderObject()'s own
	// depth/blend/cull dirty-tracking is below - a known simplification,
	// correct for any Material whose blend/depth/cull state doesn't
	// change after the fact for a given mesh/shader/target combination.
	// No cost for GL: CreatePipeline() just records a struct nobody reads
	// unless BindPipeline() is also called, which RenderObject() only
	// does at this exact same (mesh, shader) switch cadence, and
	// GetCurrentRenderTarget() always returns 0 there.
	uint64 pipelineKey = PipelineCacheKey(material->GetShader(), device->GetCurrentRenderTarget());
	if (rmesh->PipelineCache.find(pipelineKey) == rmesh->PipelineCache.end())
	{
		IRenderDevice::PipelineDesc pdesc;
		pdesc.shaderProgram = material->GetShader();
		pdesc.depthTest = material->IsDepthTesting();
		pdesc.depthTestMode = material->depthTestMode;
		pdesc.depthWrite = material->IsDepthWritting();
		pdesc.cullFace = material->GetCullFace();
		pdesc.wireframe = material->IsWireFrame();
		// See the comment on PipelineDesc::isShadowPass - shadowMaterial/
		// shadowSkinnedMaterial are the two specific shared IRenderer
		// members RenderObject() passes in as `Material` while rendering
		// a shadow-casting pass (see PreRender()'s DIRECTIONAL/POINT/
		// SPOT blocks), never for a real scene material.
		pdesc.isShadowPass = (material == shadowMaterial || material == shadowSkinnedMaterial);
		// Mesh's actual per-buffer vertex attribute layout (name/type/
		// offset/divisor per attribute, stride per buffer) - see the
		// comment on IRenderDevice::PipelineDesc::vertexLayout.
		for (std::vector<AttributeArray*>::iterator k = rmesh->Geometry->Attributes.begin(); k != rmesh->Geometry->Attributes.end(); k++)
		{
			AttributeBuffer* bf = (AttributeBuffer*)(*k);
			IRenderDevice::VertexBufferLayoutDesc bufferLayout;
			bufferLayout.stride = bf->attributeSize;
			for (std::vector<VertexAttribute*>::iterator l = (*k)->Attributes.begin(); l != (*k)->Attributes.end(); l++)
			{
				IRenderDevice::VertexAttributeDesc attr;
				attr.name = (*l)->Name;
				attr.type = (*l)->Type;
				attr.offset = (*l)->Offset;
				attr.divisor = (*l)->VertexDivisor;
				bufferLayout.attributes.push_back(attr);
			}
			pdesc.vertexLayout.push_back(bufferLayout);
		}
		if (material->blending || material->IsTransparent())
		{
			pdesc.blendingEnabled = true;
			pdesc.blendSrcFactor = BlendFunc::Src_Alpha;
			pdesc.blendDstFactor = BlendFunc::One_Minus_Src_Alpha;
			pdesc.blendEquation = BlendEq::Add;
			if (material->blending)
			{
				pdesc.blendSrcFactor = material->sfactor;
				pdesc.blendDstFactor = material->dfactor;
				pdesc.blendEquation = material->mode;
			}
		}
		rmesh->PipelineCache[pipelineKey] = device->CreatePipeline(pdesc);
	}
}

void IRenderer::BindShadowMaps(IMaterial* material)
{
	// Bind Shadows Textures
	if (material->IsCastingShadows())
	{
		DirectionalShadowMapsUnits.clear();
		for (std::vector<Texture*>::iterator i = DirectionalShadowMapsTextures.begin(); i != DirectionalShadowMapsTextures.end(); i++)
		{
			(*i)->Bind();
			DirectionalShadowMapsUnits.push_back(Texture::GetLastBindedUnit());
		}

		PointShadowMapsUnits.clear();
		for (std::vector<Texture*>::iterator i = PointShadowMapsTextures.begin(); i != PointShadowMapsTextures.end(); i++)
		{
			(*i)->Bind();
			PointShadowMapsUnits.push_back(Texture::GetLastBindedUnit());
		}

		SpotShadowMapsUnits.clear();
		for (std::vector<Texture*>::iterator i = SpotShadowMapsTextures.begin(); i != SpotShadowMapsTextures.end(); i++)
		{
			(*i)->Bind();
			SpotShadowMapsUnits.push_back(Texture::GetLastBindedUnit());
		}
	}
}

void IRenderer::UnbindShadowMaps(IMaterial* material)
{
	// Unbind Shadows Textures
	if (material->IsCastingShadows())
	{
		// Spot Lights
		for (std::vector<Texture*>::reverse_iterator i = SpotShadowMapsTextures.rbegin(); i != SpotShadowMapsTextures.rend(); i++)
		{
			(*i)->Unbind();
		}
		// Point Lights
		for (std::vector<Texture*>::reverse_iterator i = PointShadowMapsTextures.rbegin(); i != PointShadowMapsTextures.rend(); i++)
		{
			(*i)->Unbind();
		}
		// Directional Lights
		for (std::vector<Texture*>::reverse_iterator i = DirectionalShadowMapsTextures.rbegin(); i != DirectionalShadowMapsTextures.rend(); i++)
		{
			(*i)->Unbind();
		}
	}
}

};
