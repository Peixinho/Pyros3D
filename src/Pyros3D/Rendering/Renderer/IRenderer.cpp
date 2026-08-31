//============================================================================
// Name        : IRenderer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Renderer Interface
//============================================================================

#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <cstring>

// Must match MAX_LIGHTS in resources/shaders/PyrosShader.glsl - sizes and
// fills the LightsUBO backing that shader's uLights[MAX_LIGHTS] block.
#define PYROS_MAX_LIGHTS 4
// Kept small on purpose: the 2D shadow test is a loop per fragment per light,
// so this is the budget that decides whether it is affordable at all.
#define PYROS_MAX_OCCLUDERS_2D 32

// Must match the array sizes declared in PyrosShader.glsl's
// DirectionalShadowBlock/PointShadowBlock/SpotShadowBlock.
#define PYROS_MAX_DIRECTIONAL_SHADOW_CASCADES 4
#define PYROS_MAX_POINT_SHADOW_MATRICES 8
#define PYROS_MAX_SPOT_SHADOW_MATRICES 4

// Must match MAX_BONES in resources/shaders/PyrosShader.glsl - sizes the
// BoneMatricesUBO backing that shader's uBoneMatrix[MAX_BONES] block.
#define PYROS_MAX_BONES 60

namespace p3d {

// What a scene with no SetBackground() of its own clears to - matches the
// value every backend's device already initialises its own clear colour to
// (see e.g. MetalRenderDevice's pendingClearColor), so re-asserting it is a
// no-op on a fresh device and only matters once some *other* renderer has
// overwritten that shared state. See DrawBackground()/UnsetBackground().
static const Vec4 kDefaultBackgroundColor(0.f, 0.f, 0.f, 1.f);

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
uint32 IRenderer::Occluders2DUBO = 0;
uint32 IRenderer::DirectionalShadowUBO = 0;
uint32 IRenderer::PointShadowUBO = 0;
uint32 IRenderer::SpotShadowUBO = 0;
bool IRenderer::GlobalMatricesUBOValid = false;
bool IRenderer::MaterialUniformsNeedsReupload = false;
Matrix IRenderer::CachedProjectionMatrix;
Matrix IRenderer::CachedViewMatrix;
bool IRenderer::CachedRenderingPointShadowFace = false;
bool IRenderer::LightsUBOValid = false;
std::vector<Matrix> IRenderer::CachedLights;
std::vector<Vec4> IRenderer::Occluders2D;
bool IRenderer::Occluders2DUBOValid = false;
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
bool IRenderer::CachedClipPlaneEnabled = false;
Vec4 IRenderer::CachedClipPlane0;
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

	// Layer first, and unconditionally - unlike the Tag filter below this
	// is not opt-in. A mesh belongs to exactly one pass, and the default
	// (World on both sides) keeps every existing renderer seeing exactly
	// what it saw before. Without this the main pass would draw a canvas's
	// quads a second time, out in the 3D world, because the Tag filter is
	// include-only and cannot express "everything except".
	for (std::vector<RenderingMesh*>::iterator k = rmeshes.begin(); k != rmeshes.end();)
	{
		if ((*k)->renderingComponent->GetRenderLayer() != renderLayer)
			k = rmeshes.erase(k);
		else ++k;
	}

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
IRenderer::IRenderer() : ShadowMapsAreArrayIndexed(false), UsesSharedUBOs(false), device(new GLRenderDevice()) { RenderingPointShadowFace = false; renderLayer = RenderLayer::World; }

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
// (confirmed live in MotionBlurExample). Only when
// none of the above apply does this fall back to constructing a fresh,
// owned GLRenderDevice, exactly as before this existed - every GL-only
// example's very first `new ForwardRenderer(Width, Height)` call still
// hits exactly this path, unchanged.
struct ResolvedDevice { IRenderDevice *ptr; bool owns; };
static ResolvedDevice ResolveInitialDevice(IRenderDevice* externalDevice)
{
	if (externalDevice != NULL)
		return { externalDevice, true };
	// Borrowed, NOT owned. The registrar (SDL2VulkanContext) created this
	// device, outlives every renderer, and now destroys it itself - see its
	// Shutdown(). Adopting it here meant `delete Renderer` destroyed the
	// device while a PostEffectsManager, FBOs and Textures were still alive
	// and still holding pointers to it: every example's Shutdown() deletes
	// its renderer before those. That was a use-after-free on clean exit
	// (segfault inside ~PostEffectsManager), and because it aborted
	// Shutdown() partway it also left the remaining textures' image views
	// undestroyed - the "leaked objects" vkDestroyDevice reported.
	// TakeRenderDeviceOwnership() is still consumed so only the first
	// renderer treats it as pre-existing; later ones fall through to the
	// IsActiveRenderDeviceSet() borrow below, exactly as before.
	if (IRenderDevice* registered = TakeRenderDeviceOwnership())
		return { registered, false };
	if (IsActiveRenderDeviceSet())
		return { &GetActiveRenderDevice(), false };
	return { new GLRenderDevice(), true };
}

IRenderer::IRenderer(const uint32 Width, const uint32 Height, IRenderDevice* externalDevice)
{
	// See the member's comment - only ForwardRenderer turns this on.
	ShadowMapsAreArrayIndexed = false;

	// See SetRenderLayer() - UIRenderer is the only thing that moves off
	// World, so every other renderer sees the same meshes it always did.
	renderLayer = RenderLayer::World;
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
	skipShadowMaps = false;

	// GlobalMatricesUBOValid etc. are NOT reset here - they're static/shared
	// once at program start, and must stay whatever they currently are if
	// another IRenderer instance is already alive and has valid data
	// uploaded to the shared UBOs.
	UsesSharedUBOs = true;

	// Shadows materials
	shadowMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows);
	shadowMaterial->SetCullFace(CullFace::DoubleSided);
	shadowSkinnedMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows | ShaderUsage::Skinning);
	shadowSkinnedMaterial->SetCullFace(CullFace::DoubleSided);
	// See PickShadowMaterial()'s comment in IRenderer.h - without this
	// variant an instanced caster's shadow collapsed onto one instance.
	shadowInstancedMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows | ShaderUsage::InstancedRendering);
	shadowInstancedMaterial->SetCullFace(CullFace::DoubleSided);
	// Cutout casters - see the members' comment in IRenderer.h. The
	// colormap and cutoff are filled in per draw by PickShadowMaterial().
	// VertexWind unconditionally: uWind arrives per object from whatever
	// material is being drawn, and PickShadowMaterial() lends the caster's,
	// so a caster with no wind sends zero strength and the shader's
	// `if (uWind.x > 0.0)` skips it. Cheaper than a variant per combination,
	// and without it a swaying blade would cast a rigid shadow.
	shadowAlphaTestMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows | ShaderUsage::Texture | ShaderUsage::AlphaTest | ShaderUsage::VertexWind);
	shadowAlphaTestMaterial->SetCullFace(CullFace::DoubleSided);
	shadowInstancedAlphaTestMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows | ShaderUsage::Texture | ShaderUsage::AlphaTest | ShaderUsage::InstancedRendering | ShaderUsage::VertexWind);
	shadowInstancedAlphaTestMaterial->SetCullFace(CullFace::DoubleSided);

	RetainSharedUniformBuffers(device.get());
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
	// IRenderer() - they never retained the shared UBOs.
	if (UsesSharedUBOs)
		ReleaseSharedUniformBuffers(device.get());
	delete shadowAlphaTestMaterial;
	delete shadowInstancedAlphaTestMaterial;
	delete shadowMaterial;
	delete shadowSkinnedMaterial;
	delete shadowInstancedMaterial;
}

void IRenderer::RetainSharedUniformBuffers(IRenderDevice* device)
{
	if (device == NULL)
		return;
	// Created once by the first retainer - see the shared/static comment
	// on these members in IRenderer.h.
	if (SharedUBORefCount == 0)
	{
		GlobalMatricesUBO = device->CreateUniformBuffer(sizeof(Matrix) * 2, 0);
		LightsUBO = device->CreateUniformBuffer(sizeof(Matrix) * PYROS_MAX_LIGHTS, 1);
		Occluders2DUBO = device->CreateUniformBuffer(sizeof(Vec4) * (PYROS_MAX_OCCLUDERS_2D + 1), 24);
		DirectionalShadowUBO = device->CreateUniformBuffer(sizeof(Matrix) * PYROS_MAX_DIRECTIONAL_SHADOW_CASCADES + sizeof(Vec4) * 4, 2);
		PointShadowUBO = device->CreateUniformBuffer(sizeof(Matrix) * PYROS_MAX_POINT_SHADOW_MATRICES, 3);
		SpotShadowUBO = device->CreateUniformBuffer(sizeof(Matrix) * PYROS_MAX_SPOT_SHADOW_MATRICES, 4);
		VertexFrameUniformsUBO = device->CreateUniformBuffer(sizeof(Vec4) * 3, 16);
		VelocityFrameUniformsUBO = device->CreateUniformBuffer(sizeof(Matrix) * 2, 17);
		ObjectMatrixUniformsUBO = device->CreateUniformBuffer(sizeof(Matrix) + sizeof(Vec4), 18);
		BoneMatricesUBO = device->CreateUniformBuffer(sizeof(Matrix) * PYROS_MAX_BONES, 19);
		VelocityObjectUniformsUBO = device->CreateUniformBuffer(sizeof(Matrix), 20);
		AmbientLightUniformsUBO = device->CreateUniformBuffer(sizeof(Vec4), 21);
		MaterialUniformsUBO = device->CreateUniformBuffer(80, 22);
		ObjectLightCountsUBO = device->CreateUniformBuffer(16, 23);
	}
	SharedUBORefCount++;
}

void IRenderer::ReleaseSharedUniformBuffers(IRenderDevice* device)
{
	if (SharedUBORefCount == 0)
		return;
	SharedUBORefCount--;
	if (SharedUBORefCount != 0 || device == NULL)
		return;

	device->DestroyUniformBuffer(GlobalMatricesUBO);
	device->DestroyUniformBuffer(LightsUBO);
	device->DestroyUniformBuffer(Occluders2DUBO);
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
	GlobalMatricesUBO = LightsUBO = DirectionalShadowUBO = PointShadowUBO = SpotShadowUBO = 0;
	VertexFrameUniformsUBO = VelocityFrameUniformsUBO = ObjectMatrixUniformsUBO = BoneMatricesUBO = 0;
	VelocityObjectUniformsUBO = AmbientLightUniformsUBO = MaterialUniformsUBO = ObjectLightCountsUBO = 0;
	GlobalMatricesUBOValid = false;
	LightsUBOValid = false;
	DirectionalShadowUBOValid = false;
	PointShadowUBOValid = false;
	SpotShadowUBOValid = false;
	VertexFrameUniformsUBOValid = false;
	AmbientLightUniformsUBOValid = false;
	VelocityFrameUniformsUBOValid = false;
	MaterialUniformsNeedsReupload = true;
}

void IRenderer::MarkSharedUniformsDirty()
{
	GlobalMatricesUBOValid = false;
	MaterialUniformsNeedsReupload = true;
}

void IRenderer::InvalidateSharedUniformCaches()
{
	GlobalMatricesUBOValid = false;
	LightsUBOValid = false;
	DirectionalShadowUBOValid = false;
	PointShadowUBOValid = false;
	SpotShadowUBOValid = false;
	VertexFrameUniformsUBOValid = false;
	AmbientLightUniformsUBOValid = false;
	VelocityFrameUniformsUBOValid = false;
	MaterialUniformsNeedsReupload = true;
}

void IRenderer::MarkSharedGlobalMatricesDirty()
{
	GlobalMatricesUBOValid = false;
}

GenericShaderMaterial* IRenderer::PickShadowMaterial(RenderingMesh* mesh)
{
	// A cutout caster needs a shadow material that samples its colormap,
	// or its shadow is the silhouette of the whole quad rather than of
	// what survives the alpha test. Only when the geometry can actually
	// feed that shader's texcoord attribute - see
	// RenderingMesh::hasTexcoordAttribute.
	GenericShaderMaterial* caster = dynamic_cast<GenericShaderMaterial*>(mesh->Material.get());
	if (caster != NULL && (caster->GetOptions() & ShaderUsage::AlphaTest) && caster->GetColorMap() != NULL)
	{
		if (mesh->hasTexcoordAttribute < 0)
		{
			mesh->hasTexcoordAttribute = 0;
			for (std::vector<AttributeArray*>::iterator i = mesh->Geometry->Attributes.begin(); i != mesh->Geometry->Attributes.end() && mesh->hasTexcoordAttribute == 0; i++)
				for (std::vector<VertexAttribute*>::iterator k = (*i)->Attributes.begin(); k != (*i)->Attributes.end(); k++)
					if ((*k)->Name.compare(std::string("aTexcoord")) == 0)
					{
						mesh->hasTexcoordAttribute = 1;
						break;
					}
		}

		if (mesh->hasTexcoordAttribute == 1)
		{
			// Lend the caster its own map and threshold. One shared
			// override material can stand in for casters with different
			// textures because RenderObject() sends this material's
			// uniforms and binds its textures per draw, immediately after
			// this call.
			GenericShaderMaterial* cutoutShadow = mesh->renderingComponent->IsInstanced()
				? shadowInstancedAlphaTestMaterial
				: shadowAlphaTestMaterial;
			cutoutShadow->SetColorMap(caster->GetColorMapShared());
			cutoutShadow->SetAlphaCutoff(caster->GetAlphaCutoff());
			const Vec4 &casterWind = caster->GetWind();
			cutoutShadow->SetWind(casterWind.x, casterWind.y, casterWind.z);
			return cutoutShadow;
		}
	}

	// Instanced first - see the members' comment in IRenderer.h for why
	// skinned+instanced resolves this way rather than getting its own
	// variant.
	if (mesh->renderingComponent->IsInstanced()) return shadowInstancedMaterial;
	if (mesh->SkinningBones.size() > 0) return shadowSkinnedMaterial;
	return shadowMaterial;
}

bool IRenderer::IsShadowMaterial(IMaterial* material) const
{
	return material == shadowMaterial
		|| material == shadowSkinnedMaterial
		|| material == shadowInstancedMaterial
		|| material == shadowAlphaTestMaterial
		|| material == shadowInstancedAlphaTestMaterial;
}

void IRenderer::SetShadowDepthBias(const f32 factor, const f32 units)
{
	shadowMaterial->EnableDethBias(factor, units);
	shadowSkinnedMaterial->EnableDethBias(factor, units);
	shadowInstancedMaterial->EnableDethBias(factor, units);
	shadowAlphaTestMaterial->EnableDethBias(factor, units);
	shadowInstancedAlphaTestMaterial->EnableDethBias(factor, units);
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
	PYROS_PROFILE_SCOPE("Renderer.PreRender");

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
				//
				// Capped at MaxDirectionalShadowLights, which is 1: PyrosShader.glsl
				// has a single uDirectionalShadowMaps sampler and its
				// uDirectionalDepthsMVP[4] is four *cascades* of one light.
				// Rendering a second light's map anyway meant two maps were
				// bound to that one sampler and each backend picked a
				// different one - on two suns in forward, GL sampled the
				// second light's map through the first light's matrices and
				// reported the first light fully occluded (its whole
				// contribution gone, floor lit only by the other), while
				// Vulkan happened to pick the other way round. Skipping the
				// pass entirely for the extras leaves exactly one map to
				// bind, so both backends agree and the extra lights simply
				// don't cast. ForwardRenderer clamps the flag it packs to
				// match - see its comment on m[15].
				if (!skipShadowMaps && d->IsCastingShadows()
					&& (!ShadowMapsAreArrayIndexed || NumberOfDirectionalShadows < MaxDirectionalShadowLights))
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
					SetShadowDepthBias(d->GetShadowBiasFactor(), d->GetShadowBiasUnits()); // enable polygon offset fill to combat "z-fighting"

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
									RenderObject((*k), (*k)->renderingComponent->GetOwner(), PickShadowMaterial(*k));
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

				// Shadows. See the directional block's comment on the cap -
				// same reason, same forward-only condition; uPointShadowMaps
				// is a samplerCube[4].
				if (!skipShadowMaps && p->IsCastingShadows()
					&& (!ShadowMapsAreArrayIndexed || NumberOfPointShadows < MaxPointShadowLights))
				{
					// Increase Number of Shadows
					NumberOfPointShadows++;

					// See IRenderer.h's comment on RenderingPointShadowFace -
					// SendGlobalUniforms() (called from each face's
					// RenderObject() below) reads this to skip Vulkan's
					// clip-space Y-flip specifically for these 6 draws.
					// Set before the Bind(), not after: on Vulkan a Bind()
					// of an already-built FBO re-begins its render pass
					// there and then, and the device flag has to be in
					// place for everything that pass covers.
					RenderingPointShadowFace = true;
					// The other half of that same Y-flip skip: it reverses
					// the winding the rasterizer sees, so a backend that
					// negates Y in every *other* pass has to invert its
					// front-face rule here or face culling keeps the far
					// side of each occluder. No-op on GL.
					device->SetPointShadowCubeFacePass(true);

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
						// Clear colour BEFORE the attach, not after. Metal bakes
						// it into the render pass descriptor when the encoder
						// begins, and the attach is what begins it - so setting
						// it afterwards left the *first* face clearing to the
						// scene's colour (black -> stored depth 0, i.e. an
						// occluder at zero distance) while faces 1-5 happened to
						// inherit white from the previous iteration. Straight to
						// the device rather than SetBackground(), which is
						// persistent scene state and would leak a white clear
						// into the main pass; restored after the six faces below.
						device->SetClearColor(Vec4(1.f, 1.f, 1.f, 1.f));

						// Colour slot - see PointLight::EnableCastShadows for why
						// the cube map is R32F colour rather than a depth format.
						p->GetShadowFBO()->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::CubemapPositive_X + i, p->GetShadowMapTexture());

						// Colour too, not just depth: the cube face is an R32F
						// colour attachment now (see
						// PointLight::EnableCastShadows), and anything the
						// shadow pass doesn't draw over has to read as "far"
						// or PCFPOINT treats it as an occluder at distance 0.
						// Clearing only depth left those texels at 0 and
						// shadowed everything they covered.
						ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
						EnableClearDepthBuffer();
						ClearDepthBuffer();
						ClearScreen();

						StartClippingPlanes();

						// Enable Depth Bias
						SetShadowDepthBias(p->GetShadowBiasFactor(), p->GetShadowBiasUnits()); // enable polygon offset fill to combat "z-fighting"

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
										RenderObject((*k), (*k)->renderingComponent->GetOwner(), PickShadowMaterial(*k));
								}
							}
						}

						EndClippingPlanes();

					}

					// Put the scene's own clear colour back - see the
					// device->SetClearColor() call in the face loop above.
					device->SetClearColor(BackgroundColorSet ? BackgroundColor : Vec4(0.f, 0.f, 0.f, 1.f));

					// Done rendering the 6 faces - every other pass from
					// here on (this light's own record-keeping, the next
					// light, the eventual main camera pass) needs the
					// normal Y-flip again.
					RenderingPointShadowFace = false;
					device->SetPointShadowCubeFacePass(false);

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

				// Shadows. See the directional block's comment on the cap -
				// uSpotShadowMaps is a sampler2DShadow[4].
				if (!skipShadowMaps && s->IsCastingShadows()
					&& (!ShadowMapsAreArrayIndexed || NumberOfSpotShadows < MaxSpotShadowLights))
				{

					Vec3 direction = (s->GetOwner()->GetWorldTransformation() * Vec4(s->GetLightDirection(), 0.f)).xyz().normalize();

					// Increase Number of Shadows
					NumberOfSpotShadows++;

					// Bind FBO
					s->GetShadowFBO()->Bind();

					// Get Light Projection
					Projection ShadowProjection;
					// 2.5x the cone half-angle, not 2x. Perspective()'s first
					// argument is the full vertical FOV, so 2 * outterCone
					// makes the shadow frustum's half-angle exactly equal to
					// the lit cone's: the circle of light reaches the edge of
					// its own shadow map, and every lookup past that rim
					// leaves [0,1] and gets ClampToEdge's border texel back,
					// which reads as occluded. Measured with a probe in
					// secondpassSpot.glsl - the outer third of the lit cone
					// classified as "uv outside [0,1]" at 2x and stopped
					// doing so here. Costs a little shadow resolution.
					ShadowProjection.Perspective(2.5f * s->GetLightOutterCone(), 1.0, s->GetShadowNear(), s->GetShadowFar());
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
					SetShadowDepthBias(s->GetShadowBiasFactor(), s->GetShadowBiasUnits()); // enable polygon offset fill to combat "z-fighting"

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
									RenderObject((*k), (*k)->renderingComponent->GetOwner(), PickShadowMaterial(*k));
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

	// Samplers in materials are hard-coded to units 0..N via AddSampler;
	// PreRender() binds starting at Texture::UnitBinded. If a previous
	// pass leaked the counter, water/custom materials sample the wrong
	// units (white/garbage). Reset once per RenderScene.
	Texture::ResetUnitCounter();

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

// Packs (shader, targetFBO, cullFace). Cull is baked into Vulkan pipelines
// (SetCullFaceMode is a no-op there) - without it in the key, toggling
// FrontFace/BackFace for water reflection kept reusing the first-baked
// cull and flashed dark/lit.
static uint64 PipelineCacheKey(const uint32 shader, const uint32 targetFBO, const uint32 cullFace)
{
	return ((uint64)shader << 32) | ((uint64)(cullFace & 0xFFu) << 24) | (uint64)(targetFBO & 0xFFFFFFu);
}

// The cull face a draw actually ends up using. When a pass substitutes its
// own material for the mesh's (the shadow passes, PainterPick's flat-colour
// id pass) the *mesh's* material still owns culling - RenderObject() has
// always applied it that way via SetCullFaceMode. That call is a no-op on
// Vulkan/Metal, where cull mode is baked into the pipeline instead, so the
// pipeline has to be built and looked up under this same value or the
// backends silently disagree: PainterPick's material is single-sided, so
// every DoubleSided billboard (the light/sound/particle/empty-GameObject
// helper icons) had both its triangles culled in the id pass and became
// completely unpickable, while rendering normally in the main pass.
static uint32 EffectiveCullFace(RenderingMesh* rmesh, IMaterial* Material)
{
	const uint32 cf = Material->GetCullFace();
	if (rmesh->Material.get() != Material && rmesh->Material->GetCullFace() != cf)
		return rmesh->Material->GetCullFace();
	return cf;
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
		device->BindPipeline(cmd, rmesh->PipelineCache[PipelineCacheKey(Material->GetShader(), device->GetCurrentRenderTarget(), EffectiveCullFace(rmesh, Material))]);

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

	// Check double sided. Resolved into a local - this used to write the
	// mesh's own material's cull face *into* `Material` via SetCullFace().
	// When `Material` is an override (every DeferredRenderer second-pass
	// material, drawn over a shared Plane/Sphere whose own material is
	// BackFace) that permanently rewrote a shared, long-lived material:
	// deferredLastPass/Ambient/Directional are all constructed
	// CullFace::DoubleSided and were silently flipped to BackFace by their
	// first draw, for the rest of the process.
	//
	// On GL that was invisible - cull face is dynamic state re-sent per
	// draw, and these full-screen quads happen to be front-facing there, so
	// culling BackFace removes nothing. On Vulkan cull mode is baked into
	// the pipeline at BindMesh() time (it is not in the dynamic-state list),
	// and the projection Y-flip makes the same quad *back*-facing - so any
	// pipeline built after the mutation discarded both triangles and drew
	// nothing at all. BindMesh() runs before this block, so the first
	// pipeline for a given (mesh, shader) captured the correct DoubleSided
	// and worked, while the pipeline for the *second* render target that
	// pair was ever drawn into baked in BackFace and rendered black.
	// That is the whole "second render target never rasterizes" bug -
	// found by reading setCullMode:Back on a 6-index quad in a Metal
	// frame capture, after every CPU-side probe had come back clean.
	const uint32 effectiveCullFace = EffectiveCullFace(rmesh, Material);
	if (effectiveCullFace != Material->GetCullFace())
		cullFaceChanged = true;
	if (LastMaterialPTR != Material || cullFaceChanged)
	{
		// Check if Material is DoubleSided
		if (effectiveCullFace != cullFace)
		{
			device->SetCullFaceMode(effectiveCullFace);
			cullFace = effectiveCullFace;
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

	// Draw — WebGL2/GLES3 support instanced draws (needed by ParticleSystem).
	if (rmesh->renderingComponent->IsInstanced())
	{
		device->DrawElementsInstanced(cmd, DrawType, rmesh->Geometry->GetIndexData().size(), ((IRenderingInstancedComponent*)rmesh->renderingComponent)->NumberOfInstances());
	}
	else {
		device->DrawElements(cmd, DrawType, rmesh->Geometry->GetIndexData().size());
	}

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
	// Push a colour every frame, including the "no background" one. The
	// clear colour is device-global state that outlives any single
	// IRenderer, so only writing it when a background *is* set left
	// whatever the last renderer to set one had chosen in place forever:
	// switching from IslandDemo (which sets its sky) to any scene without
	// a background of its own kept clearing to Island's blue. Not
	// backend-specific - reproduced identically on GL, Vulkan and Metal,
	// since all three just hold the last SetClearColor() value.
	device->SetClearColor(BackgroundColorSet ? BackgroundColor : kDefaultBackgroundColor);
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

void IRenderer::SetOccluders2D(const std::vector<Vec4>& segments)
{
	Occluders2D = segments;
	if (Occluders2D.size() > PYROS_MAX_OCCLUDERS_2D)
		Occluders2D.resize(PYROS_MAX_OCCLUDERS_2D);
	// Upload happens on the next draw that needs it - see the dirty flag's
	// use alongside LightsUBOValid.
	Occluders2DUBOValid = false;
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
	// Force VertexFrameUniforms re-upload — stale uClipEnabled after a
	// disable/enable sequence was leaving reflection passes unclipped or
	// fully discarded on alternate frames.
	VertexFrameUniformsUBOValid = false;
}

void IRenderer::DisableClipPlane()
{
	ClipPlane = false;
	// Prevent stale uClipPlanes uploads on materials that still declare
	// the uniform (SendGlobalUniforms always sends ClipPlaneNumber entries).
	ClipPlaneNumber = 0;
	VertexFrameUniformsUBOValid = false;
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
	VertexFrameUniformsUBOValid = false;
}

void IRenderer::SetClipPlane1(const Vec4 &clipPlane)
{
	ClipPlanes[1] = clipPlane;
	VertexFrameUniformsUBOValid = false;
}

void IRenderer::SetClipPlane2(const Vec4 &clipPlane)
{
	ClipPlanes[2] = clipPlane;
	VertexFrameUniformsUBOValid = false;
}

void IRenderer::SetClipPlane3(const Vec4 &clipPlane)
{
	ClipPlanes[3] = clipPlane;
	VertexFrameUniformsUBOValid = false;
}

void IRenderer::SetClipPlane4(const Vec4 &clipPlane)
{
	ClipPlanes[4] = clipPlane;
	VertexFrameUniformsUBOValid = false;
}

void IRenderer::SetClipPlane5(const Vec4 &clipPlane)
{
	ClipPlanes[5] = clipPlane;
	VertexFrameUniformsUBOValid = false;
}

void IRenderer::SetClipPlane6(const Vec4 &clipPlane)
{
	ClipPlanes[6] = clipPlane;
	VertexFrameUniformsUBOValid = false;
}

void IRenderer::SetClipPlane7(const Vec4 &clipPlane)
{
	ClipPlanes[7] = clipPlane;
	VertexFrameUniformsUBOValid = false;
}

void IRenderer::SetBackground(const Vec4& Color)
{
	BackgroundColor = Color;
	BackgroundColorSet = true;
	// Apply immediately so the next offscreen FBO Bind() (Vulkan clears at
	// begin-render-pass; GL at glClear) sees this colour - Island water
	// reflection/refraction must match GL's sky clear on Vulkan too.
	if (device) device->SetClearColor(BackgroundColor);
}

void IRenderer::ApplyBackgroundClearColor()
{
	DrawBackground();
}

void IRenderer::UnsetBackground()
{
	BackgroundColorSet = false;
	// Mirror SetBackground()'s immediate apply, so a caller that unsets
	// between frames doesn't have to wait for the next DrawBackground().
	if (device) device->SetClearColor(kDefaultBackgroundColor);
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
	if (!IsCulling || !culling) return true;
	return culling->SphereInFrustum(owner->GetWorldPosition(), owner->GetBoundingSphereRadiusWorldSpace());
}

bool IRenderer::CullingBoxTest(RenderingMesh* rmesh, GameObject* owner)
{
	if (!IsCulling || !culling) return true;
	AABox aabb = AABox(owner->GetBoundingMinValueWorldSpace(), owner->GetBoundingMaxValueWorldSpace());

	// Return test
	return culling->ABoxInFrustum(aabb);
}

bool IRenderer::CullingPointTest(RenderingMesh* rmesh, GameObject* owner)
{
	if (!IsCulling || !culling) return true;
	return culling->PointInFrustum(owner->GetWorldPosition());
}

void IRenderer::UpdateCulling(const Matrix& ViewProjectionMatrix)
{
	if (!IsCulling || !culling) return;
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

	// Occluder segments for 2D shadows. Its own block, not nested in the
	// lights one: it is scene state rather than per-object, and a scene can
	// have occluders published before any object with lights has been drawn.
	if (!Occluders2DUBOValid)
	{
		Occluders2DUBOValid = true;
		std::vector<Vec4> payload = Occluders2D;
		payload.resize(PYROS_MAX_OCCLUDERS_2D, Vec4(0.f, 0.f, 0.f, 0.f));
		// The count rides in the slot after the array - std140 would pad a
		// lone int to 16 bytes anyway, so it costs nothing to make it a vec4.
		payload.push_back(Vec4((f32)Occluders2D.size(), 0.f, 0.f, 0.f));
		device->ReplaceUniformBuffer(Occluders2DUBO, sizeof(Vec4) * payload.size(), &payload[0]);
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
		// Deliberately not part of the dirty check below: uTimeParams
		// changes every frame by definition, so this block re-uploads every
		// frame whenever anything animates from it (VERTEXWIND). 48 bytes.
		if (WindInUseThisFrame || !VertexFrameUniformsUBOValid ||
			memcmp(&CachedCameraPosition, &CameraPosition, sizeof(Vec3)) != 0 ||
			CachedClipPlaneEnabled != ClipPlane ||
			memcmp(&CachedClipPlane0, &ClipPlanes[0], sizeof(Vec4)) != 0)
		{
			// std140: vec4 uCameraPos (xyz + clipEnabled in w), vec4
			// uClipPlane0, vec4 uTimeParams (x = seconds).
			f32 vertexFrameData[12] = {
				CameraPosition.x, CameraPosition.y, CameraPosition.z,
				ClipPlane ? 1.0f : 0.0f,
				ClipPlanes[0].x, ClipPlanes[0].y, ClipPlanes[0].z, ClipPlanes[0].w,
				(f32)Timer, 0.0f, 0.0f, 0.0f
			};
			device->ReplaceUniformBuffer(VertexFrameUniformsUBO, sizeof(vertexFrameData), vertexFrameData);
			CachedCameraPosition = CameraPosition;
			CachedClipPlaneEnabled = ClipPlane;
			CachedClipPlane0 = ClipPlanes[0];
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
			// Must match GlobalMatricesUBO: uProjectionMatrix is always
			// TranslateProjectionMatrix()'d (Vulkan Y-flip + Z remap; no-op
			// on GL). Uploading raw PrvProjection here made velocity
			// (a_current - b_previous) explode on Vulkan every frame -
			// MotionBlur then smeared the whole screen. Same rule as
			// CaptureExtraUniform()'s translatedPrvProjectionMatrix.
			Matrix velocityFrameData[2] = {
				device->TranslateProjectionMatrix(PrvProjectionMatrix),
				PrvViewMatrix
			};
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
			// A sampler that is never assigned a texture unit keeps the
			// default of 0. SAMPLER_BINDING() expands to nothing on GL (see
			// PyrosShader.glsl), so these three are the C++ side's job every
			// bind - but they were only assigned when the scene actually had
			// shadow-casting lights. With none, uDirectionalShadowMaps
			// (sampler2DShadow), uPointShadowMaps (samplerCube) and
			// uSpotShadowMaps (sampler2DShadow) all sat on unit 0, and GL
			// makes it a draw-time GL_INVALID_OPERATION for two active
			// samplers of different types to name the same texture image
			// unit. Every draw with a shadow-capable material therefore
			// failed the moment the scene had no shadow casters - which is
			// every scene before the first light is added.
			//
			// Nothing needs to be *bound* to the fallback units: the shader
			// only samples these inside its per-light loops, which run zero
			// times when uNumberOfDirectionalShadows/etc are 0. They just
			// have to be distinct from each other so the type conflict goes
			// away. Units 13-15 are used because material samplers are
			// allocated upward from 0 by Texture::Bind() and there are only
			// eight of them (colormap/fontmap/normalmap/displacement/env/
			// refract/skybox/specular), while GL 4.1 guarantees at least 16
			// per-stage texture image units.
			case Uniforms::DataUsage::DirectionalShadowMap:
				if (DirectionalShadowMapsUnits.size() > 0)
					Shader::SendUniform((*k), &DirectionalShadowMapsUnits[0], (*_ShadersGlobalCache)[counter], DirectionalShadowMapsUnits.size());
				else
				{
					int32 unusedUnit = 13;
					Shader::SendUniform((*k), &unusedUnit, (*_ShadersGlobalCache)[counter], 1);
				}
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
				else
				{
					int32 unusedUnit = 14;   // distinct from the directional one above
					Shader::SendUniform((*k), &unusedUnit, (*_ShadersGlobalCache)[counter], 1);
				}
				break;
			case Uniforms::DataUsage::PointShadowMatrix:
				if (PointShadowMatrix.size() > 0)
					Shader::SendUniform((*k), &PointShadowMatrix[0], (*_ShadersGlobalCache)[counter], PointShadowMatrix.size());
				break;
			case Uniforms::DataUsage::NumberOfPointShadows:
				Shader::SendUniform((*k), &NumberOfPointShadows, (*_ShadersGlobalCache)[counter]);
				break;
			case Uniforms::DataUsage::SpotShadowMap:
				// Was &SpotShadowMapsUnits[0] unguarded, unlike its
				// Directional/Point neighbours - indexing an empty vector.
				if (SpotShadowMapsUnits.size() > 0)
					Shader::SendUniform((*k), &SpotShadowMapsUnits[0], (*_ShadersGlobalCache)[counter], SpotShadowMapsUnits.size());
				else
				{
					int32 unusedUnit = 15;   // distinct from the two above
					Shader::SendUniform((*k), &unusedUnit, (*_ShadersGlobalCache)[counter], 1);
				}
				break;
			case Uniforms::DataUsage::SpotShadowMatrix:
				if (SpotShadowMatrix.size() > 0)
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
				{
					Matrix translatedPrvProjection = device->TranslateProjectionMatrix(PrvProjectionMatrix);
					Shader::SendUniform((*k), &translatedPrvProjection, (*_ShadersGlobalCache)[counter]);
				}
				break;
			case Uniforms::DataUsage::PrvModelViewProjectionMatrix:
				{
					Matrix PrvModelViewProjectionMatrix =
						device->TranslateProjectionMatrix(PrvProjectionMatrix) * PrvViewMatrix * PrvModelMatrix;
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
	// See GenericShaderMaterial::SetAlphaCutoff() - fragments below this
	// are discarded when ShaderUsage::AlphaTest is on.
	f32 AlphaCutoff;
	// std140 rounds the block to a multiple of 16: two vec4s (32) plus 9
	// floats (36) is 68, which becomes 80. The padding is explicit so the
	// static_assert below measures the real layout rather than relying on
	// the compiler happening to agree.
	f32 _pad[3];
};
static_assert(sizeof(MaterialUniformsData) == 80, "MaterialUniformsData must byte-match PyrosShader.glsl's MaterialUniforms std140 layout exactly");

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
	if (Material->SupportsUniformBlocks() && (MaterialUniformsNeedsReupload || LastMeshRenderedPTR != rmesh || LastMaterialPTR != Material))
	{
		MaterialUniformsNeedsReupload = false;
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
		if (const uchar* v = FindUserUniformValue(Material->UserUniforms, "uAlphaCutoff")) memcpy(&data.AlphaCutoff, v, sizeof(f32));
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
		// std140: mat4 uModelMatrix then vec4 uWind - one upload, so the
		// two can't drift apart the way two separate writes could.
		struct { Matrix model; Vec4 wind; } objectMatrixData;
		objectMatrixData.model = ModelMatrix;
		objectMatrixData.wind = Vec4(0.f, 0.f, 0.f, 0.f);
		{
			GenericShaderMaterial* genericMat = dynamic_cast<GenericShaderMaterial*>(Material);
			if (genericMat != NULL && (genericMat->GetOptions() & ShaderUsage::VertexWind))
			{
				objectMatrixData.wind = genericMat->GetWind();
				WindInUseThisFrame = true;
			}
		}
		device->ReplaceUniformBuffer(ObjectMatrixUniformsUBO, sizeof(objectMatrixData), &objectMatrixData);
		if (rmesh->SkinningBones.size() > 0)
		{
			// Always upload the full UBO size. ReplaceUniformBuffer →
			// glBufferData with a shorter size orphans storage smaller than
			// the shader's mat4 uBoneMatrix[MAX_BONES] block; on macOS GL
			// that left the binding unloadable / zeros, so skinned meshes
			// stayed in bind pose ("no animation").
			uint32 bonesToUpload = rmesh->SkinningBones.size() < PYROS_MAX_BONES ? (uint32)rmesh->SkinningBones.size() : PYROS_MAX_BONES;
			Matrix boneUpload[PYROS_MAX_BONES]; // default-ctor = identity pad past bonesToUpload
			memcpy(boneUpload, &rmesh->SkinningBones[0], sizeof(Matrix) * bonesToUpload);
			device->ReplaceUniformBuffer(BoneMatricesUBO, sizeof(Matrix) * PYROS_MAX_BONES, boneUpload);
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
	case Uniforms::DataUsage::Lights:
		// Mirrors the LightsUBO upload in SendGlobalUniforms() above: the
		// trailing unused slots (lightsToUpload < PYROS_MAX_LIGHTS) are
		// never read since the shader loop is gated by uNumberOfLights.
		if (Lights.size() > 0)
		{
			uint32 lightsToUpload = NumberOfLights < PYROS_MAX_LIGHTS ? NumberOfLights : PYROS_MAX_LIGHTS;
			valuePtr = &Lights[0]; valueSize = sizeof(Matrix) * lightsToUpload;
		}
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
		// This material's own buffer, explicitly - NOT whatever the device's
		// global binding-point registry happens to hold (see
		// IRenderDevice::BindUniformBlockIfPresent()). Two live instances of
		// the same material type - two DeferredRenderers, say the editor's
		// Scene View plus the Material Editor's live preview - each allocate
		// a buffer at this same binding, and only one of them can be the
		// registry's entry; the other one's shader would otherwise read its
		// rival's block (which is how a 220x220 preview's uScreenDimensions
		// ended up driving the full-size viewport's deferred composite).
		device->BindUniformBlockIfPresent(Material->GetShader(), block.blockName, block.binding, block.bufferHandle);
		device->ReplaceUniformBuffer(block.bufferHandle, block.size, &block.scratch[0]);
	}
}

void IRenderer::BindMesh(RenderingMesh* rmesh, IMaterial* material)
{
	// Drop every cached VAO if the geometry has been given new GPU buffers
	// since they were built. A VAO bakes in the buffer handles it was
	// recorded against, so one built before the rebuild would keep sourcing
	// vertices from the freed buffers while the draw count - read from
	// CPU-side index data - tracks the new geometry. Only the VAOs are
	// invalidated: the shader attribute/uniform location caches alongside
	// them depend on the shader, not on the buffers, and stay valid.
	if (rmesh->Geometry != NULL && rmesh->VAOCacheRevision != rmesh->Geometry->buffersRevision)
	{
		for (std::map<uint32, uint32>::iterator i = rmesh->VAOCache.begin(); i != rmesh->VAOCache.end(); i++)
			device->DeleteVertexArray(i->second);
		rmesh->VAOCache.clear();
		rmesh->VAOCacheRevision = rmesh->Geometry->buffersRevision;
	}

	// Everything this draw sources vertex data from: the geometry's own
	// attribute buffers, then any the component owns (the per-instance
	// transform stream, particle streams - see
	// RenderingComponent::ownAttributeBuffers for why those can't live on
	// the shared geometry). Built once here because all three passes below
	// - the attribute-location cache, the VAO, and the pipeline's vertex
	// layout - have to walk the exact same list in the exact same order:
	// _ShadersAttributesCache is indexed positionally.
	std::vector<AttributeArray*> meshAttributes = rmesh->Geometry->Attributes;
	if (rmesh->renderingComponent != NULL)
	{
		for (std::vector<AttributeBuffer*>::iterator i = rmesh->renderingComponent->ownAttributeBuffers.begin(); i != rmesh->renderingComponent->ownAttributeBuffers.end(); i++)
			meshAttributes.push_back(*i);
	}

	std::vector< std::vector<int32> >* _ShadersAttributesCache = &rmesh->ShadersAttributesCache[material->GetShader()];
	if ((*_ShadersAttributesCache).size()==0)
	{
		// Reset Attribute IDs
		for (std::vector<AttributeArray*>::iterator i = meshAttributes.begin(); i != meshAttributes.end(); i++)
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

		if (meshAttributes.size() > 0)
		{
			uint32 counterBuffers = 0;
			for (std::vector<AttributeArray*>::iterator k = meshAttributes.begin(); k != meshAttributes.end(); k++)
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
		// 2D shadow occluders. A block a shader declares is not bound just by
		// existing at a binding point - it has to be named here, which is why
		// adding the UBO and the shader block was not enough on its own.
		device->BindUniformBlockIfPresent(material->GetShader(), "Occluders2DBlock", 24);
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
	const uint32 pipelineCullFace = EffectiveCullFace(rmesh, material);
	uint64 pipelineKey = PipelineCacheKey(material->GetShader(), device->GetCurrentRenderTarget(), pipelineCullFace);
	if (rmesh->PipelineCache.find(pipelineKey) == rmesh->PipelineCache.end())
	{
		IRenderDevice::PipelineDesc pdesc;
		pdesc.shaderProgram = material->GetShader();
		pdesc.depthTest = material->IsDepthTesting();
		pdesc.depthTestMode = material->depthTestMode;
		pdesc.depthWrite = material->IsDepthWritting();
		pdesc.cullFace = pipelineCullFace;
		pdesc.wireframe = material->IsWireFrame();
		// Vulkan bakes primitive topology into the pipeline. The cache this
		// feeds is per-RenderingMesh and drawingType is a per-mesh property,
		// so no pipeline key change is needed - two meshes with different
		// topologies already get separate pipelines.
		pdesc.drawingType = rmesh->drawingType;
		// See the comment on PipelineDesc::isShadowPass - the shadow
		// materials are the specific shared IRenderer members
		// RenderObject() passes in as `Material` while rendering a
		// shadow-casting pass (see PreRender()'s DIRECTIONAL/POINT/SPOT
		// blocks), never for a real scene material. Asking
		// IsShadowMaterial() rather than comparing against the two that
		// existed when this was written is what keeps a newly added
		// variant from silently building its pipeline against the wrong
		// render pass on Vulkan/Metal.
		pdesc.isShadowPass = IsShadowMaterial(material);
		// Mesh's actual per-buffer vertex attribute layout (name/type/
		// offset/divisor per attribute, stride per buffer) - see the
		// comment on IRenderDevice::PipelineDesc::vertexLayout.
		for (std::vector<AttributeArray*>::iterator k = meshAttributes.begin(); k != meshAttributes.end(); k++)
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
			// Depth+compare + Linear is unloadable on Apple GL (sampler2DShadow
			// then hits unit 0's colour map). Maps created before the Nearest
			// fix in EnableCastShadows still need this every bind.
			(*i)->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
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
			(*i)->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
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
