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
IRenderer::IRenderer() : UsesSharedUBOs(false), device(new GLRenderDevice()) {}

// Resolves what IRenderer(Width, Height, externalDevice)'s device member
// should own: an explicitly-passed device wins outright; otherwise, a
// device someone registered via RegisterRenderDeviceForOwnership() (e.g.
// SDL2VulkanContext, which needs a real VulkanRenderDevice + swapchain to
// exist before any IRenderer does - see IRenderDevice.h's comment on that
// function for why this can't just be GetActiveRenderDevice()) is adopted
// if present; only when neither applies does this fall back to
// constructing a fresh GLRenderDevice, exactly as before this existed -
// every GL example's `new ForwardRenderer(Width, Height)` call site never
// registers anything, so this is a no-op change for every one of them.
static IRenderDevice* ResolveInitialDevice(IRenderDevice* externalDevice)
{
	if (externalDevice != NULL)
		return externalDevice;
	if (IRenderDevice* registered = TakeRenderDeviceOwnership())
		return registered;
	return new GLRenderDevice();
}

IRenderer::IRenderer(const uint32 Width, const uint32 Height, IRenderDevice* externalDevice) : device(ResolveInitialDevice(externalDevice))
{
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

						DirectionalShadowMatrix.push_back((Matrix::BIAS * (ProjectionMatrix * ViewMatrix * Camera->GetWorldTransformation())));

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

					Vec4 ShadowFar;
					ShadowFar.x = 0.5f*(-_ShadowFar.x*projection.m.m[10] + projection.m.m[14]) / _ShadowFar.x + 0.5f;
					ShadowFar.y = 0.5f*(-_ShadowFar.y*projection.m.m[10] + projection.m.m[14]) / _ShadowFar.y + 0.5f;
					ShadowFar.z = 0.5f*(-_ShadowFar.z*projection.m.m[10] + projection.m.m[14]) / _ShadowFar.z + 0.5f;
					ShadowFar.w = 0.5f*(-_ShadowFar.w*projection.m.m[10] + projection.m.m[14]) / _ShadowFar.w + 0.5f;
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

					// Set Light Projection
					PointShadowMatrix.push_back(ShadowProjection.m);
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
					SpotShadowMatrix.push_back((Matrix::BIAS * (ProjectionMatrix * ViewMatrix * Camera->GetWorldTransformation())));

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

		// Material Stuff Pre Render
		Material->PreRender();

		// Bind Shadow Maps
		BindShadowMaps(Material);

		// Send Global Uniforms
		SendGlobalUniforms(rmesh, Material);

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
		device->BindPipeline(cmd, rmesh->PipelineCache[Material->GetShader()]);

		if (Material->depthBias)
			EnableDepthBias(Vec2(Material->depthFactor, Material->depthUnits));
	}

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
		memcmp(&CachedViewMatrix, &ViewMatrix, sizeof(Matrix)) != 0)
	{
		Matrix globalMatricesData[2] = { ProjectionMatrix, ViewMatrix };
		device->ReplaceUniformBuffer(GlobalMatricesUBO, sizeof(Matrix) * 2, globalMatricesData);
		CachedProjectionMatrix = ProjectionMatrix;
		CachedViewMatrix = ViewMatrix;
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
// bytes: 2 vec4 + 5 float = 32 + 20 = 52, padded to 64 by std140's
// vec4-multiple block size rule).
struct MaterialUniformsData
{
	Vec4 Color;
	Vec4 Specular;
	f32 Opacity;
	f32 Shininess;
	f32 UseLights;
	f32 DisplacementHeight;
	f32 Reflectivity;
	f32 _pad[3];
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

		// Vulkan pipeline for this (mesh, shader) pair - see the comment on
		// RenderingMesh::PipelineCache. Built from Material's state right
		// now, not re-evaluated per object the way RenderObject()'s own
		// depth/blend/cull dirty-tracking is below - a known simplification
		// (documented on PipelineCache itself), correct for this backend's
		// only validated target (RotatingCube: one opaque, non-blended,
		// non-double-sided, non-wireframe mesh/material pairing). No cost
		// for GL: CreatePipeline() just records a struct nobody reads
		// unless BindPipeline() is also called, which RenderObject() only
		// does at this exact same (mesh, shader) switch cadence.
		IRenderDevice::PipelineDesc pdesc;
		pdesc.shaderProgram = material->GetShader();
		pdesc.depthTest = material->IsDepthTesting();
		pdesc.depthTestMode = material->depthTestMode;
		pdesc.depthWrite = material->IsDepthWritting();
		pdesc.cullFace = material->GetCullFace();
		pdesc.wireframe = material->IsWireFrame();
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
		rmesh->PipelineCache[material->GetShader()] = device->CreatePipeline(pdesc);
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
