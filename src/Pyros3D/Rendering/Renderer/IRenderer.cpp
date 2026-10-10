#include <chrono>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
//============================================================================
// Name        : IRenderer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Renderer Interface
//============================================================================

#include <unordered_set>
#include <cstdlib>
#include <Pyros3D/Rendering/RenderState.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>
#include <string>
#include <cctype>
#include <Pyros3D/Rendering/PostEffects/VolumetricSmoke.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten/html5.h>
#endif
#include <sstream>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <cstring>
#include <algorithm>
#include <typeinfo>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>

// Must match MAX_LIGHTS in resources/shaders/PyrosShader.glsl - sizes and
// fills the LightsUBO backing that shader's uLights[MAX_LIGHTS] block.
#define PYROS_MAX_LIGHTS 4
static_assert(PYROS_MAX_LIGHTS == p3d::IRenderer::MaxShaderLights, "IRenderer::MaxShaderLights must match PYROS_MAX_LIGHTS");

// PyrosShader.glsl declares uPointShadowMaps[4] and uSpotShadowMaps[4].
#define PYROS_SHADOW_SAMPLER_SLOTS 4

namespace {
	// Fills EVERY element of a sampler array with a texture unit, not just
	// the ones this scene happens to use.
	//
	// glUniform1iv with a count smaller than the declared array leaves the
	// tail at its default of 0. That parks a samplerCube (uPointShadowMaps)
	// and a sampler2DShadow (uSpotShadowMaps) on unit 0 at the same time,
	// which is illegal: an active sampler array's elements each occupy a
	// unit, and two different sampler TYPES may not name one unit. Desktop
	// GL drivers sample it anyway. WebGL2/ANGLE validates the program on
	// every draw, fails it with "Two textures of different types use the
	// same sampler location", and skips the draw with no GL error at all -
	// so in the browser every lit, shadow-capable mesh silently stopped
	// rendering (it did not even write depth, so it occluded nothing) while
	// the grid, the unlit icons and the selection overlay - none of which
	// declare a shadow sampler - carried on drawing normally. That is what
	// made it read as a broken material rather than a dropped draw.
	//
	// The tail is padded with the array's own first unit, so no unit is
	// introduced that was not already bound and the type stays consistent.
	// Only when the scene has no caster of that kind at all does the
	// caller's spare unit get used. The shader samples element [0] only,
	// so the padding is never read.
	// `real` is uint32 because that is how the renderer stores bound units;
	// the uniform itself is an int array, hence the separate out type.
	static void PadSamplerUnits(std::vector<p3d::int32> &out, const std::vector<p3d::uint32> &real,
								const p3d::int32 spareUnit, const p3d::uint32 slots)
	{
		out.assign(slots, real.empty() ? spareUnit : (p3d::int32)real[0]);
		for (p3d::uint32 i = 0; i < slots && i < real.size(); ++i)
			out[i] = (p3d::int32)real[i];
	}
}
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
uint32 IRenderer::DDGIUniformsUBO = 0;
uint32 IRenderer::MaterialUniformsUBO = 0;
uint32 IRenderer::ObjectLightCountsUBO = 0;
bool IRenderer::VertexFrameUniformsUBOValid = false;
Vec3 IRenderer::CachedCameraPosition;
bool IRenderer::CachedClipPlaneEnabled = false;
Vec4 IRenderer::CachedClipPlane0;
bool IRenderer::AmbientLightUniformsUBOValid = false;
Vec4 IRenderer::CachedGlobalLight;
f32 IRenderer::AmbientScale = 1.f;
namespace { f32 g_heldAmbientScale = 1.f; bool g_ambientScaleHeld = false; }
// (set while a frame may be in flight: kept, and made the scale at the next PreRender)
void IRenderer::SetAmbientScale(const f32 Scale)
{
	const f32 v = Scale < 0.f ? 0.f : Scale;
	if (GameObject::DrawCopies()) { g_heldAmbientScale = v; g_ambientScaleHeld = true; return; }
	AmbientScale = v;
}
Vec4 IRenderer::BackgroundOverride(0.f, 0.f, 0.f, 1.f);
Vec4 IRenderer::ShaderGlobals[IRenderer::kShaderGlobals];
bool IRenderer::BackgroundOverrideSet = false;
Vec4 IRenderer::CachedAmbientEnv[14];
bool IRenderer::VelocityFrameUniformsUBOValid = false;
namespace { bool g_LightCountsUBOValid = false; int32 g_CachedLightCounts[4] = { 0, 0, 0, 0 }; }
Matrix IRenderer::CachedPrvProjectionMatrix;
Matrix IRenderer::CachedPrvViewMatrix;

// What each of the shared uniform buffers was last given - which is what the
// stream of calls being recorded will find in it. One set for the frame's own
// stream; a thread recording a pass beside it (IRenderDevice::BeginParallelStream)
// has a set of its own, starting from nothing known.
namespace {
	struct StreamCaches
	{
		uint32 _viewPortStartX = 0, _viewPortStartY = 0, _viewPortEndX = 0, _viewPortEndY = 0;
		bool GlobalMatricesUBOValid = false;
		bool MaterialUniformsNeedsReupload = false;
		Matrix CachedProjectionMatrix;
		Matrix CachedViewMatrix;
		bool CachedRenderingPointShadowFace = false;
		bool LightsUBOValid = false;
		std::vector<Matrix> CachedLights;
		bool Occluders2DUBOValid = false;
		bool DirectionalShadowUBOValid = false;
		std::vector<Matrix> CachedDirectionalShadowMatrix;
		Vec4 CachedDirectionalShadowFar;
		bool PointShadowUBOValid = false;
		std::vector<Matrix> CachedPointShadowMatrix;
		bool SpotShadowUBOValid = false;
		std::vector<Matrix> CachedSpotShadowMatrix;
		bool VertexFrameUniformsUBOValid = false;
		Vec3 CachedCameraPosition;
		bool CachedClipPlaneEnabled = false;
		Vec4 CachedClipPlane0;
		bool AmbientLightUniformsUBOValid = false;
		Vec4 CachedGlobalLight;
		Vec4 CachedAmbientEnv[14];
		bool VelocityFrameUniformsUBOValid = false;
		bool LightCountsUBOValid = false;
		int32 CachedLightCounts[4] = { 0, 0, 0, 0 };
		Matrix CachedPrvProjectionMatrix;
		Matrix CachedPrvViewMatrix;
	};
	thread_local StreamCaches* t_caches = NULL;
}
// A pass recorded beside the frame, by a second renderer on another thread
// (IRenderer::RecordSunBeside). Set while one is: a mesh's own caches - its
// vertex arrays, pipelines and uniform places by program - are then reached
// one thread at a time (MeshCaches). Only the caches: never held over a call
// that waits for the device.
namespace {
	std::atomic<int> g_besideActive(0);          // how many are (each renderer may have one)
	thread_local bool t_recordingBeside = false;
	struct SpinLock
	{
		std::atomic_flag f = ATOMIC_FLAG_INIT;
		void lock() { while (f.test_and_set(std::memory_order_acquire)) std::this_thread::yield(); }
		void unlock() { f.clear(std::memory_order_release); }
	};
	// (by address: a few hundred locks for all the meshes there are)
	SpinLock g_meshLocks[256];
	struct MeshCaches
	{
		SpinLock* l;
		explicit MeshCaches(const void* mesh) : l(g_besideActive.load(std::memory_order_relaxed) > 0 ? &g_meshLocks[(reinterpret_cast<uintptr_t>(mesh) >> 5) & 255] : NULL) { if (l) l->lock(); }
		~MeshCaches() { if (l) l->unlock(); }
	};
}
#define _viewPortStartX (*(t_caches ? &t_caches->_viewPortStartX : &IRenderer::_viewPortStartX))
#define _viewPortStartY (*(t_caches ? &t_caches->_viewPortStartY : &IRenderer::_viewPortStartY))
#define _viewPortEndX (*(t_caches ? &t_caches->_viewPortEndX : &IRenderer::_viewPortEndX))
#define _viewPortEndY (*(t_caches ? &t_caches->_viewPortEndY : &IRenderer::_viewPortEndY))
#define GlobalMatricesUBOValid (*(t_caches ? &t_caches->GlobalMatricesUBOValid : &IRenderer::GlobalMatricesUBOValid))
#define MaterialUniformsNeedsReupload (*(t_caches ? &t_caches->MaterialUniformsNeedsReupload : &IRenderer::MaterialUniformsNeedsReupload))
#define CachedProjectionMatrix (*(t_caches ? &t_caches->CachedProjectionMatrix : &IRenderer::CachedProjectionMatrix))
#define CachedViewMatrix (*(t_caches ? &t_caches->CachedViewMatrix : &IRenderer::CachedViewMatrix))
#define CachedRenderingPointShadowFace (*(t_caches ? &t_caches->CachedRenderingPointShadowFace : &IRenderer::CachedRenderingPointShadowFace))
#define LightsUBOValid (*(t_caches ? &t_caches->LightsUBOValid : &IRenderer::LightsUBOValid))
#define CachedLights (*(t_caches ? &t_caches->CachedLights : &IRenderer::CachedLights))
#define Occluders2DUBOValid (*(t_caches ? &t_caches->Occluders2DUBOValid : &IRenderer::Occluders2DUBOValid))
#define DirectionalShadowUBOValid (*(t_caches ? &t_caches->DirectionalShadowUBOValid : &IRenderer::DirectionalShadowUBOValid))
#define CachedDirectionalShadowMatrix (*(t_caches ? &t_caches->CachedDirectionalShadowMatrix : &IRenderer::CachedDirectionalShadowMatrix))
#define CachedDirectionalShadowFar (*(t_caches ? &t_caches->CachedDirectionalShadowFar : &IRenderer::CachedDirectionalShadowFar))
#define PointShadowUBOValid (*(t_caches ? &t_caches->PointShadowUBOValid : &IRenderer::PointShadowUBOValid))
#define CachedPointShadowMatrix (*(t_caches ? &t_caches->CachedPointShadowMatrix : &IRenderer::CachedPointShadowMatrix))
#define SpotShadowUBOValid (*(t_caches ? &t_caches->SpotShadowUBOValid : &IRenderer::SpotShadowUBOValid))
#define CachedSpotShadowMatrix (*(t_caches ? &t_caches->CachedSpotShadowMatrix : &IRenderer::CachedSpotShadowMatrix))
#define VertexFrameUniformsUBOValid (*(t_caches ? &t_caches->VertexFrameUniformsUBOValid : &IRenderer::VertexFrameUniformsUBOValid))
#define CachedCameraPosition (*(t_caches ? &t_caches->CachedCameraPosition : &IRenderer::CachedCameraPosition))
#define CachedClipPlaneEnabled (*(t_caches ? &t_caches->CachedClipPlaneEnabled : &IRenderer::CachedClipPlaneEnabled))
#define CachedClipPlane0 (*(t_caches ? &t_caches->CachedClipPlane0 : &IRenderer::CachedClipPlane0))
#define AmbientLightUniformsUBOValid (*(t_caches ? &t_caches->AmbientLightUniformsUBOValid : &IRenderer::AmbientLightUniformsUBOValid))
#define CachedGlobalLight (*(t_caches ? &t_caches->CachedGlobalLight : &IRenderer::CachedGlobalLight))
#define CachedAmbientEnv (*(t_caches ? &t_caches->CachedAmbientEnv : &IRenderer::CachedAmbientEnv))
#define VelocityFrameUniformsUBOValid (*(t_caches ? &t_caches->VelocityFrameUniformsUBOValid : &IRenderer::VelocityFrameUniformsUBOValid))
#define LightCountsUBOValid (*(t_caches ? &t_caches->LightCountsUBOValid : &g_LightCountsUBOValid))
#define CachedLightCounts (*(t_caches ? &t_caches->CachedLightCounts : &g_CachedLightCounts))
#define CachedPrvProjectionMatrix (*(t_caches ? &t_caches->CachedPrvProjectionMatrix : &IRenderer::CachedPrvProjectionMatrix))
#define CachedPrvViewMatrix (*(t_caches ? &t_caches->CachedPrvViewMatrix : &IRenderer::CachedPrvViewMatrix))


namespace Sort {

	GameObject* _Camera;
	// WORLD position on both sides. A camera parented to a pivot - which is
	// how both the editor viewport and the 2D view frame a scene, moving the
	// pivot and leaving the camera at a fixed local offset - has a local
	// position of (0,0,kCameraZ) wherever it is actually looking from. Sorting
	// against that measures every object's distance from the ORIGIN, so in a
	// level wider than its layer spacing the ordering is decided by x, and a
	// parallax backdrop draws on top of the world it is supposed to sit
	// behind. The LOD block a few lines below already used GetWorldPosition().
	bool sortRenderingMeshes(const void* a, const void* b)
	{
		const Vec3 eye = _Camera->GetWorldPosition();
		f32 a2 = eye.distanceSQR(((RenderingMesh*)a)->renderingComponent->GetOwner()->GetWorldPosition());
		f32 b2 = eye.distanceSQR(((RenderingMesh*)b)->renderingComponent->GetOwner()->GetWorldPosition());
		return (a2 < b2);
	}
}

// How often the sun's shadow map is redrawn: every frame (1), every other (2)...
// One number for every renderer, like the ambient light.
namespace { uint32 g_shadowEvery = 1; }
void IRenderer::SetShadowUpdateInterval(const uint32 frames) { g_shadowEvery = frames < 1 ? 1 : (frames > 8 ? 8 : frames); }
uint32 IRenderer::GetShadowUpdateInterval() { return g_shadowEvery; }

// What a scene draws for this renderer, as the scene has it: the opaque
// meshes, and those that are seen through (in the order they were gathered -
// SortTranslucent wants that).
void IRenderer::GatherFrameMeshes(SceneGraph* Scene, GameObject* Camera, const uint32 Tag, std::vector<RenderingMesh*> &_OpaqueMeshes, std::vector<RenderingMesh*> &_TranslucidMeshes)
{
	_OpaqueMeshes.clear();
	_TranslucidMeshes.clear();

	// Sort and Group Objects From Scene

	// LOD
	if (lod)
	{
		std::vector<RenderingComponent*> comps(RenderingComponent::GetRenderingComponents(Scene));
		for (std::vector<RenderingComponent*>::iterator i = comps.begin(); i != comps.end(); i++)
		{
			// (a model nobody gave levels to gets its own, the first time it is listed)
			if (!(*i)->HasLOD()) (*i)->TryAutomaticLODs();
			if (!(*i)->HasLOD()) continue;
			// Squared distance to the bounding sphere, 0 from inside it. It
			// was to (centre - radius) on every axis, a point outside the
			// object: a camera standing on a 256 m terrain tile measured
			// ~150 m and got a coarse level under its feet.
			const Vec3 &scale = (*i)->GetOwner()->GetScale();
			const f32 maxScale = std::max(fabs(scale.x), std::max(fabs(scale.y), fabs(scale.z)));
			const Vec3 centre = (*i)->GetOwner()->GetWorldPosition() + (*i)->GetBoundingSphereCenter() * scale;
			const f32 d = std::max(0.f, Camera->GetWorldPosition().distance(centre) - (*i)->GetBoundingSphereRadius() * maxScale);
			(*i)->UpdateLOD((*i)->GetLODByDistance(d * d));
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
	//
	// An instanced component with no instances draws nothing, so it is
	// dropped here rather than paying for its binds - a foliage block faded
	// out by distance, a particle system between bursts.
	//
	// One pass over the list, keeping what stays. Erasing one at a time
	// moved everything after it down for every mesh dropped: with a few
	// thousand meshes and a few hundred of them faded-out foliage blocks,
	// that copying was a twentieth of the frame.
	{
		const uint32 layer = renderLayer;
		size_t kept = 0;
		for (size_t k = 0; k < rmeshes.size(); k++)
		{
			RenderingComponent* rc = rmeshes[k]->renderingComponent;
			// (an instanced component with no instances at the moment stays in
			// the list and is passed over when it is drawn - CullEntry,
			// RenderObject: left out here, every block of grass that came
			// into reach or went out of it had the whole list made again)
			if (rc->GetRenderLayer() != layer) continue;
			if (Tag != 0 && !rc->GetOwner()->HaveTag(Tag)) continue;
			rmeshes[kept++] = rmeshes[k];
		}
		rmeshes.resize(kept);
	}

	for (std::vector<RenderingMesh*>::iterator k = rmeshes.begin(); k != rmeshes.end(); k++)
	{
		if ((*k)->Material->IsTransparent() && sorting)
		{
			_TranslucidMeshes.push_back((*k));
		}
		else _OpaqueMeshes.push_back((*k));
	}
}

// Nearest last, for drawing back to front (appended to the opaque list reversed).
void IRenderer::SortTranslucent(GameObject* Camera, std::vector<RenderingMesh*> &_TranslucidMeshes)
{
	// sorting translucid
	//
	// STABLE, so meshes at the same distance keep the order they were
	// gathered in. That is the only thing that decides draw order for the
	// parts of one cutout character: they share an owner, so they share a
	// distance, and an unstable sort permuted them arbitrarily - an arm would
	// be behind the torso in one build and in front of it in the next.
	//
	// Each one's distance worked out once, and the sort on those. (The
	// comparison used to work out both distances - and the camera's place -
	// every time it was asked: with a thousand panes of glass in a scene,
	// tens of thousands of times a frame.)
	{
		const Vec3 eye = Camera->GetWorldPosition();
		std::vector<std::pair<f32, RenderingMesh*> > keyed;
		keyed.reserve(_TranslucidMeshes.size());
		for (size_t k = 0; k < _TranslucidMeshes.size(); k++)
			keyed.push_back(std::make_pair(eye.distanceSQR(_TranslucidMeshes[k]->renderingComponent->GetOwner()->GetWorldPosition()), _TranslucidMeshes[k]));
		std::stable_sort(keyed.begin(), keyed.end(),
			[](const std::pair<f32, RenderingMesh*> &x, const std::pair<f32, RenderingMesh*> &y) { return x.first < y.first; });
		for (size_t k = 0; k < keyed.size(); k++) _TranslucidMeshes[k] = keyed[k].second;
	}
}

std::vector<RenderingMesh*> IRenderer::GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera, const uint32 Tag)
{
	std::vector<RenderingMesh*> _OpaqueMeshes;
	std::vector<RenderingMesh*> _TranslucidMeshes;
	GatherFrameMeshes(Scene, Camera, Tag, _OpaqueMeshes, _TranslucidMeshes);
	SortTranslucent(Camera, _TranslucidMeshes);

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
IRenderer::IRenderer() : ShadowMapsAreArrayIndexed(false), UsesSharedUBOs(false), device(std::make_shared<GLRenderDevice>()) { RenderingPointShadowFace = false; renderLayer = RenderLayer::World; }

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
static MaybeOwningDevicePtr ResolveInitialDevice(IRenderDevice* externalDevice)
{
	if (externalDevice != NULL)
		return AdoptRenderDevice(externalDevice);
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
	// The registrar published it as active already, and destroys it itself,
	// so this is a borrow either way - consuming the slot only keeps the
	// "first taker" bookkeeping the header describes.
	if (TakeRenderDeviceOwnership() != NULL)
		return BorrowActiveRenderDevice();
	if (MaybeOwningDevicePtr active = BorrowActiveRenderDevice())
		return active;
	return AdoptRenderDevice(new GLRenderDevice());
}

IRenderer::IRenderer(const uint32 Width, const uint32 Height, IRenderDevice* externalDevice)
{
	// See the member's comment - only ForwardRenderer turns this on.
	ShadowMapsAreArrayIndexed = false;

	// See SetRenderLayer() - UIRenderer is the only thing that moves off
	// World, so every other renderer sees the same meshes it always did.
	renderLayer = RenderLayer::World;
	device = ResolveInitialDevice(externalDevice);

	// Every Shader/GeometryBuffer/RenderingComponent constructed anywhere
	// in the engine (no IRenderer reference available at most of those
	// call sites) shares whichever backend THIS instance ends up using -
	// see GetActiveRenderDevice()/SetActiveRenderDevice() in IRenderDevice.h.
	SetActiveRenderDevice(device);

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
	// On: only a component that was given levels (RenderingComponent::AddLOD,
	// a terrain tile's steps) has anything to switch, and one that has them
	// wants them used - a streamed terrain drawn at full detail to the
	// horizon is not a working terrain.
	lod = true;
	ClipPlane = false;
	IsCulling = false;
	skipShadowMaps = false;
	unshadowed = false;

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

// Which renderer keeps a list of each scene's world: IRenderer::CurrentWorldList.
namespace { std::map<SceneGraph*, std::pair<IRenderer*, void*> > &WorldLists() { static std::map<SceneGraph*, std::pair<IRenderer*, void*> > m; return m; } }
IRenderer::~IRenderer()
{
	FinishBeside();
	beside.reset();
	for (std::map<SceneGraph*, std::pair<IRenderer*, void*> >::iterator w = WorldLists().begin(); w != WorldLists().end();)
		if (w->second.first == this) w = WorldLists().erase(w); else ++w;
	for (std::map<AutoInstanceKey, std::vector<AutoInstanceBatch*> >::iterator k = autoInstanceBatches.begin(); k != autoInstanceBatches.end(); k++)
		for (size_t b = 0; b < k->second.size(); b++)
			delete k->second[b];
	autoInstanceBatches.clear();
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

// ---------------------------------------------------------------------------
// Automatic instancing
//
// Every visible mesh used to be its own draw: material, uniforms, descriptor
// sets and a draw call each, so a scene of thousands of identical objects
// (Physics Stress: ~7.7k spheres, a shadow pass and a main pass) spent most of
// its frame recording draws. Meshes that share geometry and a plain opaque
// GenericShaderMaterial now draw as one instanced call through the path
// RenderingInstancedComponent already uses - the material's
// INSTANCED_RENDERING variant, ModelMatrix = uModelMatrix * aInstancedTransform
// - with an identity owner and pivot, so the instance matrix is each member's
// own world * pivot and position, normals and shadows come out the same.
//
// Only where draw order cannot matter: opaque, depth-tested, depth-written.
// Skinned, LOD'd, already-instanced and custom-material meshes draw as before.
// ---------------------------------------------------------------------------

static const uint32 kAutoInstanceMinimum = 4;

static bool& AutoInstancingFlag()
{
	static bool enabled = []() {
		const char* env = getenv("PYROS_AUTO_INSTANCING");
		return !(env != NULL && env[0] == '0');
	}();
	return enabled;
}

void IRenderer::SetAutoInstancing(const bool enabled) { AutoInstancingFlag() = enabled; }
bool IRenderer::IsAutoInstancing() { return AutoInstancingFlag(); }

// PYROS_TRIS_DUMP=<file>: what the triangles of a frame are. Every draw is
// tallied by the object it belongs to (the name of its topmost parent and
// its own, numbers taken off) and every ten seconds or so the table is
// written to the file: triangles a frame, draws a frame, how much of it was
// for shadow maps, the mesh's own size. Works in a built game, which has no
// editor to ask - it is how a forest and the rifles lying about were found
// to be most of what a frame drew.
namespace {
	struct TrisDump
	{
		bool on = false;
		std::string path;
		struct Row { uint64 tris = 0, shadowTris = 0, draws = 0; uint32 meshTris = 0; };
		std::map<std::string, Row> rows;
		uint32 frames = 0;
		TrisDump() { if (const char* p = std::getenv("PYROS_TRIS_DUMP")) { path = p; on = !path.empty(); } }
		static std::string Plain(const std::string &n)
		{
			size_t e = n.size();
			while (e > 0 && (std::isdigit((unsigned char)n[e - 1]) || n[e - 1] == '_' || n[e - 1] == ' ' || n[e - 1] == '(' || n[e - 1] == ')')) e--;
			return n.substr(0, e);
		}
		void Add(RenderingMesh* mesh, GameObject* owner, const bool shadow)
		{
			GameObject* root = owner;
			while (root && root->GetParent()) root = root->GetParent();
			const uint32 meshTris = (uint32)(mesh->Geometry->GetIndexData().size() / 3);
			const uint64 inst = mesh->renderingComponent->IsInstanced() ? (uint64)((IRenderingInstancedComponent*)mesh->renderingComponent)->NumberOfInstances() : 1;
			std::string key = (root ? Plain(root->GetName()) : std::string("?")) + " / " + (owner ? Plain(owner->GetName()) : std::string("?"));
			if (key == " / ") key = "(drawn together) " + std::to_string(meshTris) + "-triangle mesh";
			Row &r = rows[key];
			r.tris += meshTris * inst; r.draws++; r.meshTris = meshTris;
			if (shadow) r.shadowTris += meshTris * inst;
		}
		void Frame()
		{
			if (++frames < 600) return;
			std::vector<std::pair<uint64, std::string> > order;
			uint64 all = 0, allDraws = 0;
			for (std::map<std::string, Row>::iterator i = rows.begin(); i != rows.end(); ++i) { order.push_back(std::make_pair(i->second.tris, i->first)); all += i->second.tris; allDraws += i->second.draws; }
			std::sort(order.rbegin(), order.rend());
			if (FILE* f = std::fopen(path.c_str(), "w"))
			{
				std::fprintf(f, "%.0f thousand triangles and %.0f draws a frame (over %u frames)\n", (f64)all / frames / 1000.0, (f64)allDraws / frames, frames);
				std::fprintf(f, "%8s %7s %7s %9s  %s\n", "ktris/f", "draws/f", "shadow", "mesh tris", "object");
				for (size_t i = 0; i < order.size() && i < 40; i++)
				{
					const Row &r = rows[order[i].second];
					std::fprintf(f, "%8.1f %7.1f %6.0f%% %9u  %s\n", (f64)r.tris / frames / 1000.0, (f64)r.draws / frames, r.tris ? 100.0 * (f64)r.shadowTris / (f64)r.tris : 0.0, r.meshTris, order[i].second.c_str());
				}
				std::fclose(f);
			}
			rows.clear(); frames = 0;
		}
	} g_trisDump;
}

bool IRenderer::AutoInstanceMesh(RenderingMesh* mesh)
{
	RenderingComponent* rc = mesh->renderingComponent;
	// (a component with levels of detail too: whichever level it is showing
	// is batched with the others showing the same - a forest's far cards are
	// one draw, where leaving these out made every tree a draw of its own)
	if (rc == NULL || rc->IsInstanced() || !rc->GetRenderableShared())
		return false;
	return mesh->BonesToDraw().empty() && mesh->Geometry != NULL;
}

bool IRenderer::AutoInstanceMaterial(IMaterial* mat)
{
	if (mat == NULL) return false;
	// A shader of somebody's own, when its source can be drawn instanced (every
	// generated one can) and its shadow is the renderer's: a hundred of the same
	// thing wearing one such material are one draw too. It was only the engine's
	// own material that was ever batched.
	if (typeid(*mat) == typeid(CustomShaderMaterial))
	{
		CustomShaderMaterial* csm = static_cast<CustomShaderMaterial*>(mat);
		if (!csm->SupportsInstancing() || csm->HasCustomShadow()) return false;
		return !mat->IsTransparent() && mat->IsDepthTesting() && mat->IsDepthWritting();
	}
	if (typeid(*mat) != typeid(GenericShaderMaterial))
		return false;
	if ((static_cast<GenericShaderMaterial*>(mat)->GetOptions() & ShaderUsage::InstancedRendering) != 0)
		return false;
	return !mat->IsTransparent() && mat->IsDepthTesting() && mat->IsDepthWritting();
}

bool IRenderer::AutoInstanceEligible(RenderingMesh* mesh)
{
	return AutoInstanceMesh(mesh) && AutoInstanceMaterial(mesh->Material.get());
}

void IRenderer::BeginAutoInstancingFrame()
{
	autoInstanceFrame++;
	if (g_trisDump.on) g_trisDump.Frame();
	autoInstanceOrdinal.clear();
	autoInstanceBatchesThisFrame = autoInstanceObjectsThisFrame = autoInstanceSinglesThisFrame = 0;
	// Batches unused for a while go: they hold their material and renderable
	// alive, and the scene they were built for may be long gone.
	for (std::map<AutoInstanceKey, std::vector<AutoInstanceBatch*> >::iterator k = autoInstanceBatches.begin(); k != autoInstanceBatches.end(); )
	{
		std::vector<AutoInstanceBatch*> &v = k->second;
		for (size_t b = v.size(); b-- > 0; )
			if (v[b]->lastUsedFrame + 300 < autoInstanceFrame)
			{
				delete v[b];
				v.erase(v.begin() + b);
			}
		if (v.empty()) autoInstanceBatches.erase(k++);
		else ++k;
	}
}

IRenderer::AutoInstanceBatch* IRenderer::AcquireAutoInstanceBatch(RenderingMesh* source, const uint64 fingerprint, const uint32 count)
{
	const AutoInstanceKey key(source->Geometry, fingerprint);
	uint32 &ordinal = autoInstanceOrdinal[key];
	std::vector<AutoInstanceBatch*> &list = autoInstanceBatches[key];
	if (ordinal >= list.size())
		list.push_back(new AutoInstanceBatch());
	AutoInstanceBatch* b = list[ordinal++];
	// (a batch that could not be made is not tried again every frame: making
	// one is an object, a component and its buffers)
	if (b->failed) return NULL;
	if (b->capacity < count)
	{
		// (objects are made: one thread at a time - a pass may be recorded beside the frame)
		static std::mutex batchMaking;
		std::lock_guard<std::mutex> making(batchMaking);
		uint32 capacity = 64;
		while (capacity < count) capacity *= 2;
		RenderingComponent* rc = source->renderingComponent;
		b->mesh = NULL;
		b->comp.reset();
		b->owner.reset();
		b->owner = std::make_shared<GameObject>();
		// The renderable this mesh is of: the component's own, or the one of
		// the level of detail it belongs to.
		std::shared_ptr<Renderable> of = rc->GetRenderableShared();
		{
			const std::vector<std::shared_ptr<Renderable> > levels = rc->GetLODRenderables();
			for (size_t l = 0; l < levels.size(); l++)
			{
				if (!levels[l]) continue;
				bool has = false;
				for (size_t g = 0; g < levels[l]->Geometries.size() && !has; g++) has = levels[l]->Geometries[g] == source->Geometry;
				if (has) { of = levels[l]; break; }
			}
		}
		// (or it is the mesh the component casts its shadow from - a
		// simplified copy of the model, which is not one of its levels)
		if (const std::shared_ptr<Renderable> &shadow = rc->GetShadowRenderable())
			for (size_t g = 0; g < shadow->Geometries.size(); g++)
				if (shadow->Geometries[g] == source->Geometry) { of = shadow; break; }
		b->comp = std::make_shared<RenderingInstancedComponent>(of, source->Material, capacity, rc->GetBoundingSphereRadius());
		b->owner->Add(b->comp);
		std::vector<RenderingMesh*> &meshes = b->comp->GetMeshes(0);
		for (size_t i = 0; i < meshes.size(); i++)
			if (meshes[i]->Geometry == source->Geometry) { b->mesh = meshes[i]; break; }
		if (b->mesh == NULL)
		{
			b->failed = true;
			return NULL;
		}
		// The members' own pivots go into their instance matrices.
		b->mesh->Pivot.identity();
		b->capacity = capacity;
	}
	// Members only share the material's content, not the object: draw with
	// this group's first member's.
	b->mesh->Material = source->Material;
	b->lastUsedFrame = autoInstanceFrame;
	return b;
}

// A material's fingerprint reads every field and every uniform it has, and
// it was read again by every pass that drew: each shadow cascade, each cube
// face, the G-buffer - a scene that gives each object its own material paid
// for a thousand of them several times a frame. Nothing changes a material
// between one view's PreRender() and the passes that follow it, so within
// that span each is worked out once.
namespace {
	thread_local std::unordered_map<IMaterial*, uint64> g_fingerprintsThisView;
	uint64 FingerprintThisView(IMaterial* mat)
	{
		// (a custom shader's material is told from another by being another: two
		// objects are batched under one when they wear the very same material)
		if (typeid(*mat) != typeid(GenericShaderMaterial)) return (uint64)(size_t)mat * 0x9E3779B97F4A7C15ULL | 1ULL;
		std::unordered_map<IMaterial*, uint64>::iterator it = g_fingerprintsThisView.find(mat);
		if (it != g_fingerprintsThisView.end()) return it->second;
		const uint64 f = static_cast<GenericShaderMaterial*>(mat)->RenderFingerprint();
		g_fingerprintsThisView[mat] = f;
		return f;
	}
}

namespace p3d_instancing {
struct Scratch
{
	struct Mat { uint64 fingerprint; bool eligible; };
	std::unordered_map<IMaterial*, Mat> materials;
	struct Key { IGeometry* g; uint64 m; uint64 s; bool operator==(const Key &o) const { return g == o.g && m == o.m && s == o.s; } };
	struct Hash { size_t operator()(const Key &k) const { return (size_t)(((uint64)(size_t)k.g * 0x9E3779B97F4A7C15ULL) ^ (k.m * 0xC2B2AE3D27D4EB4FULL) ^ (k.s + 0x165667B19E3779F9ULL)); } };
	std::unordered_map<Key, uint32, Hash> index;
	std::vector<int32> groupOf;
	std::vector<uint32> count, first, cursor, members;
	std::vector<uint64> fingerprint;
};
}
using p3d_instancing::Scratch;

// Which of a pass's things go together: the same geometry, the same material (by
// what it holds), the same lights. Worked out afresh for every pass of every
// frame - and it was most of what a pass cost: a tree of keys built and thrown
// away, a vector for every group, each material's eligibility read again for
// every mesh that wears it, a counter looked up by name for every single draw.
// Now: what a material says is asked once a pass, the groups are found through a
// table that is kept from one pass to the next, and nothing is allocated once
// the tables have grown to the scene.
void IRenderer::GroupForInstancing(const std::vector<RenderingMesh*> &items, const std::vector<uint64> *signatures, void* scratch)
{
	Scratch &S = *static_cast<Scratch*>(scratch);
	const uint32 n = (uint32)items.size();
S.materials.clear(); S.index.clear();
S.groupOf.assign(n, -1);
S.count.clear(); S.fingerprint.clear();
// The fingerprints this view does not have yet, worked out on every core before
// the grouping asks for them one at a time: each reads every field and uniform of
// a material and writes nothing, and a scene that gives each thing a material of
// its own has a thousand of them - a quarter of this thread's share of the pass.
{
	static thread_local std::vector<IMaterial*> want;
	static thread_local std::vector<uint64> got;
	static thread_local std::unordered_set<IMaterial*> asked;
	want.clear(); asked.clear();
	for (uint32 i = 0; i < n; i++)
	{
		IMaterial* mat = items[i]->Material.get();
		if (mat == NULL || typeid(*mat) != typeid(GenericShaderMaterial)) continue;
		if (!asked.insert(mat).second) continue;
		if (g_fingerprintsThisView.find(mat) == g_fingerprintsThisView.end()) want.push_back(mat);
	}
	const uint32 w = (uint32)want.size();
	if (w >= 64)
	{
		got.resize(w);
		IMaterial** const wantOf = want.data();
		uint64* const gotOf = got.data();
		auto print = [wantOf, gotOf](uint32 begin, uint32 end) {
			for (uint32 k = begin; k < end; k++) gotOf[k] = static_cast<GenericShaderMaterial*>(wantOf[k])->RenderFingerprint();
		};
		static thread_local JobSystem::SharedLoop loop;
		if (loop.Begin(w, 64)) JobSystem::Instance().ParallelFor(w, 32, print);
		else print(0, w);
		loop.End();
		for (uint32 k = 0; k < w; k++) g_fingerprintsThisView[want[k]] = got[k];
	}
}
for (uint32 i = 0; i < n; i++)
{
	RenderingMesh* mesh = items[i];
	IMaterial* mat = mesh->Material.get();
	if (mat == NULL) continue;
	std::unordered_map<IMaterial*, Scratch::Mat>::iterator known = S.materials.find(mat);
	if (known == S.materials.end())
	{
		Scratch::Mat m;
		m.eligible = AutoInstanceMaterial(mat);
		m.fingerprint = m.eligible ? FingerprintThisView(mat) : 0;
		known = S.materials.insert(std::make_pair(mat, m)).first;
	}
	if (!known->second.eligible || !AutoInstanceMesh(mesh)) continue;
	const Scratch::Key key = { mesh->Geometry, known->second.fingerprint, signatures ? (*signatures)[i] : 0 };
	std::unordered_map<Scratch::Key, uint32, Scratch::Hash>::iterator it = S.index.find(key);
	if (it == S.index.end())
	{
		it = S.index.insert(std::make_pair(key, (uint32)S.count.size())).first;
		S.count.push_back(0);
		S.fingerprint.push_back(known->second.fingerprint);
	}
	S.count[it->second]++;
	S.groupOf[i] = (int32)it->second;
}
// (each group's members, one after another in one list)
const uint32 groupCount = (uint32)S.count.size();
S.first.resize(groupCount); S.cursor.resize(groupCount);
{
	uint32 at = 0;
	for (uint32 g = 0; g < groupCount; g++) { S.first[g] = at; S.cursor[g] = at; at += S.count[g]; }
	S.members.resize(at);
	for (uint32 i = 0; i < n; i++) if (S.groupOf[i] >= 0) S.members[S.cursor[S.groupOf[i]]++] = i;
}
}

void IRenderer::DrawWithAutoInstancing(const std::vector<RenderingMesh*> &items, const std::vector<uint64> *signatures,
	const std::function<void(RenderingMesh*, uint32)> &drawOne,
	const std::function<void(RenderingMesh*, uint32)> &drawBatch)
{
	const uint32 n = (uint32)items.size();
	if (!IsAutoInstancing())
	{
		for (uint32 i = 0; i < n; i++) drawOne(items[i], i);
		return;
	}

	static thread_local Scratch S;
	GroupForInstancing(items, signatures, &S);
	const std::vector<int32> &groupOf = S.groupOf;
	const std::vector<uint64> &groupFingerprint = S.fingerprint;
	struct Groups
	{
		const Scratch &s;
		struct Members { const uint32* b; uint32 n; uint32 size() const { return n; } uint32 operator[](const size_t k) const { return b[k]; } };
		Members operator[](const int32 g) const { Members m = { s.members.data() + s.first[g], s.count[g] }; return m; }
		size_t size() const { return s.count.size(); }
	} groups = { S };

	std::vector<bool> fallback(groups.size(), false);
	for (uint32 i = 0; i < n; i++)
	{
		const int32 g = groupOf[i];
		if (g < 0 || fallback[g] || groups[g].size() < kAutoInstanceMinimum)
		{
			drawOne(items[i], i);
			++autoInstanceSinglesThisFrame;
			continue;
		}
		if (groups[g][0] != i)
			continue; // drawn with its batch

		const Groups::Members members = groups[g];
		AutoInstanceBatch* b = AcquireAutoInstanceBatch(items[i], groupFingerprint[g], (uint32)members.size());
		if (b == NULL)
		{
			fallback[g] = true;
			drawOne(items[i], i);
			continue;
		}
		for (size_t k = 0; k < members.size(); k++)
		{
			RenderingMesh* m = items[members[k]];
			b->comp->transform[k] = m->renderingComponent->GetOwner()->GetDrawWorld() * m->Pivot;
		}
		b->comp->SetNumberInstances((uint32)members.size());
		b->comp->UpdateTransforms();
		drawBatch(b->mesh, i);
		autoInstanceBatchesThisFrame++;
		autoInstanceObjectsThisFrame += (uint32)members.size();
	}
	// (what is left drawn one at a time, all passes of the frame: the number that
	// says whether a frame's cost is its draw calls - said once a pass, not once a draw)
	FrameProfiler::Instance().Counter("AutoInstance.Singles", (f64)autoInstanceSinglesThisFrame);
	FrameProfiler::Instance().Counter("AutoInstance.Batches", (f64)autoInstanceBatchesThisFrame);
	FrameProfiler::Instance().Counter("AutoInstance.Objects", (f64)autoInstanceObjectsThisFrame);
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
		ObjectMatrixUniformsUBO = device->CreateUniformBuffer(sizeof(Matrix) + sizeof(Vec4) * 2, 18);
		BoneMatricesUBO = device->CreateUniformBuffer(sizeof(Matrix) * PYROS_MAX_BONES, 19);
		VelocityObjectUniformsUBO = device->CreateUniformBuffer(sizeof(Matrix), 20);
		// 14 vec4s, not 5: the block grew by the nine SH coefficients.
		// The size here must match PyrosShader.glsl's AmbientLightUniforms
		// exactly - WebGL2 validates a bound range against the shader's
		// full declared block size and drops the draw outright when it is
		// short, with the mesh simply never appearing.
		AmbientLightUniformsUBO = device->CreateUniformBuffer(sizeof(Vec4) * 14, 21);
		// Four vec4s: origin+probesPerRow, spacing, counts, and
		// (irradianceRes, visibilityRes, unused, probesPerRow) - see
		// PyrosShader.glsl's DDGIUniforms block.
		// FIVE vec4s, matching DDGIUniforms in PyrosShader.glsl exactly.
		// An undersized uniform block is not a warning anywhere: GL and
		// Metal read whatever follows, and WebGL2 validates the size
		// and silently drops the draw. When the block grows, this
		// grows.
		DDGIUniformsUBO = device->CreateUniformBuffer(sizeof(Vec4) * 5, 25);
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
	LightCountsUBOValid = false;
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
	LightCountsUBOValid = false;
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
	if (mesh->BonesToDraw().size() > 0) return shadowSkinnedMaterial;
	return shadowMaterial;
}

// Culling is a question asked of every mesh in the scene, several times a
// frame (the view, and each cascade of each shadow), and each answer depends
// on nothing but the mesh and the view: the meshes are shared out among the
// job system's workers and the answers put back in order. (Under a few
// hundred meshes the sharing out costs more than it saves.)
namespace {
	// OFF unless asked for. Measured on a scene of four thousand meshes, switched
	// every ten seconds in one run: the view pass took 0.95 ms with it off and
	// 1.15 ms with it on - all the culling of a frame is a tenth of a millisecond,
	// and waking the workers costs more. For a scene with far more to cull.
	// (Neither is assumed any more: each of these loops keeps a JobSystem::SharedLoop,
	// which times it both ways on the machine it is running on and uses the
	// quicker. These two say "always", for a test.)
	bool g_parallelCulling = false;
	bool g_parallelLists = false;
	template <class Keep>
	void AnswerInParallel(const size_t count, std::vector<uint8> &answers, const Keep &keep)
	{
		answers.resize(count);
		static thread_local JobSystem::SharedLoop loop;
		const bool together = loop.Begin((uint32)count, 512) || (g_parallelCulling && count >= 512 && JobSystem::Instance().WorkerCount() > 0);
		if (!together)
		{
			for (size_t i = 0; i < count; i++) answers[i] = keep((uint32)i);
			loop.End();
			return;
		}
		uint8* out = &answers[0];
		JobSystem::Instance().ParallelFor((uint32)count, 256, [out, &keep](uint32 begin, uint32 end) {
			for (uint32 i = begin; i < end; i++) out[i] = keep(i);
		});
		loop.End();
	}
}
void IRenderer::SetParallelCulling(const bool on) { g_parallelCulling = on; }
void IRenderer::SetParallelLists(const bool on) { g_parallelLists = on; }

// What every culling pass of the frame asks about, for each mesh of the
// frame's list, side by side in one array: its sphere in the world and what
// kind of thing it is. The view and each cascade of each shadow then read
// sixteen bytes a mesh in order, where each used to follow the mesh to its
// component to its object to its matrix - three or four places in memory a
// mesh, a pass.
// Whether a view asks the list's grid of cells before the things in them (see cullCell).
static bool g_cullGrid = true;
void IRenderer::SetCullGrid(const bool on) { g_cullGrid = on; }
bool IRenderer::GetCullGrid() { return g_cullGrid; }

void IRenderer::CullEntry(const size_t i, RenderingMesh* m)
{
	RenderingComponent* rc = m->renderingComponent;
	GameObject* owner = rc->GetOwner();
	uint8 f = 0;
	if (owner != NULL)
	{
		f |= CullOwner;
		const Matrix &world = owner->GetWorldTransformation();
		const Vec3 &local = owner->GetBoundingSphereCenter();
		cullSphere[i] = Vec4(world.m[0] * local.x + world.m[4] * local.y + world.m[8] * local.z + world.m[12],
			world.m[1] * local.x + world.m[5] * local.y + world.m[9] * local.z + world.m[13],
			world.m[2] * local.x + world.m[6] * local.y + world.m[10] * local.z + world.m[14],
			owner->GetBoundingSphereRadiusWorldSpace());
	}
	if (m->Material && m->Material->IsTransparent()) f |= CullTransparent;
	// (an instanced one with nothing to draw counts as switched off)
	if (rc->IsActive() && !(rc->IsInstanced() && static_cast<IRenderingInstancedComponent*>(rc)->NumberOfInstances() == 0)) { f |= CullComponentActive; if (m->Active == true && (m->standsFor == NULL || m->standsFor->Active)) f |= CullMeshActive; }
	if (rc->IsCastingShadows()) f |= CullCasts;
	if (rc->IsCullTesting()) f |= CullTested;
	if (m->CullingGeometry == CullingGeometry::Box) f |= CullBox;
	cullFlags[i] = f;
}

void IRenderer::BuildCullList()
{
	const size_t n = rmesh.size();
	cullCell.clear(); cullCellSphere.clear(); cullCellOut.clear();      // (whoever keeps a list puts its grid back: UseFrameList)
	cullSphere.resize(n);
	cullFlags.resize(n);
	// (SetParallelLists(true) builds it on every core: slower where frames are
	// capped and the workers asleep when it starts.)
	const std::function<void(uint32, uint32)> build = [this](uint32 begin, uint32 end) {
		for (size_t i = begin; i < end; i++) CullEntry(i, rmesh[i]);
	};
	static thread_local JobSystem::SharedLoop loop;
	if (loop.Begin((uint32)n, 512) || (g_parallelLists && n >= 512 && JobSystem::Instance().WorkerCount() > 0)) JobSystem::Instance().ParallelFor((uint32)n, 128, build);
	else build(0, (uint32)n);
	loop.End();
}

const IRenderer::FrameList* IRenderer::CurrentWorldList(SceneGraph* Scene)
{
	std::map<SceneGraph*, std::pair<IRenderer*, void*> >::iterator at = WorldLists().find(Scene);
	if (at == WorldLists().end()) return NULL;
	const FrameList* L = static_cast<const FrameList*>(at->second.second);
	// (nothing has come or gone, or been said to have changed, since it was read)
	if (!L->valid || L->version != RenderState::Version.load(std::memory_order_relaxed) || L->listedSeq != RenderState::ListedCount()) return NULL;
	return L;
}

static bool g_patchLists = true;
void IRenderer::SetListPatching(const bool on) { g_patchLists = on; }
bool IRenderer::GetListPatching() { return g_patchLists; }

// See the header. Nothing that has gone is followed here: a mesh taken off the
// list, or a component out of the scene, may no longer exist - they are only
// names until the end, when what is left is all still there.
bool IRenderer::ApplyListed(FrameList &L, SceneGraph* Scene, const uint32 Tag)
{
	std::vector<RenderState::Listed> log;
	if (!RenderState::ListedSince(L.listedSeq, log)) return false;
	if (log.empty()) return true;
	L.listedSeq += log.size();
	if (!g_patchLists)
	{
		// (to compare: as it was - anything said of this scene, and the list is made again)
		for (size_t k = 0; k < log.size(); k++) if (log[k].scene == Scene) return false;
		return true;
	}

	const size_t nO = L.opaque.size(), nT = L.translucent.size(), n = nO + nT;
	static const uint32 kGone = 0xFFFFFFFFu;

	// Components: the last thing said of each is what holds.
	{
		std::unordered_map<RenderingComponent*, bool> said;
		for (size_t k = 0; k < log.size(); k++)
			if (log[k].scene == Scene && (log[k].what == RenderState::Listed::ComponentOn || log[k].what == RenderState::Listed::ComponentOff))
				said[log[k].component] = log[k].what == RenderState::Listed::ComponentOn;
		if (!said.empty())
		{
			size_t kept = 0;
			for (size_t i = 0; i < L.lodComponents.size(); i++) if (said.find(L.lodComponents[i]) == said.end()) L.lodComponents[kept++] = L.lodComponents[i];
			L.lodComponents.resize(kept);
			kept = 0;
			for (size_t i = 0; i < L.instanced.size(); i++) if (said.find(L.instanced[i].first) == said.end()) L.instanced[kept++] = L.instanced[i];
			L.instanced.resize(kept);
			for (std::unordered_map<RenderingComponent*, bool>::iterator i = said.begin(); i != said.end(); ++i)
			{
				if (!i->second) continue;
				RenderingComponent* c = i->first;
				if (lod && !c->HasLOD()) c->TryAutomaticLODs();
				if (c->HasLOD() && c->GetOwner() != NULL) L.lodComponents.push_back(c);
				if (c->IsInstanced()) L.instanced.push_back(std::make_pair(c, static_cast<IRenderingInstancedComponent*>(c)->NumberOfInstances() == 0));
				if (c->IsInstanced() && GameObject::DrawCopies()) static_cast<IRenderingInstancedComponent*>(c)->TakeDrawCount();
			}
		}
	}

	// Meshes, in the order it happened.
	std::vector<uint8> removed;
	size_t nRemoved = 0;
	std::vector<uint32> changed;                                   // places whose mesh is another now
	std::vector<RenderingMesh*> added;                             // put at the end of the scene's list (NULL: and taken off again)
	std::unordered_map<RenderingMesh*, size_t> addedAt;
	auto meshAt = [&L, nO](const size_t i) -> RenderingMesh*& { return i < nO ? L.opaque[i] : L.translucent[i - nO]; };
	auto find = [&](GameObject* owner, RenderingMesh* m) -> uint32 {
		std::unordered_map<GameObject*, std::vector<uint32> >::iterator at = L.where.find(owner);
		if (at == L.where.end()) return kGone;
		for (size_t e = 0; e < at->second.size(); e++)
		{
			const uint32 idx = at->second[e];
			if (meshAt(idx) == m && (removed.empty() || !removed[idx])) return idx;
		}
		return kGone;
	};
	for (size_t k = 0; k < log.size(); k++)
	{
		const RenderState::Listed &e = log[k];
		if (e.scene != Scene) continue;
		if (e.what == RenderState::Listed::MeshOn)
		{
			addedAt[e.mesh] = added.size();
			added.push_back(e.mesh);
		}
		else if (e.what == RenderState::Listed::MeshOff || e.what == RenderState::Listed::MeshSwap)
		{
			const bool swap = e.what == RenderState::Listed::MeshSwap;
			std::unordered_map<RenderingMesh*, size_t>::iterator a = addedAt.find(e.mesh);
			if (a != addedAt.end())
			{
				const size_t at = a->second;
				addedAt.erase(a);
				added[at] = swap ? e.other : NULL;
				if (swap) addedAt[e.other] = at;
				continue;
			}
			const uint32 idx = find(e.owner, e.mesh);
			if (idx == kGone) continue;                           // (not one this list holds: another layer's, or tagged otherwise)
			if (swap) { meshAt(idx) = e.other; changed.push_back(idx); }
			else
			{
				if (removed.empty()) removed.assign(n, 0);
				removed[idx] = 1; nRemoved++;
			}
		}
	}

	// What was added and is still there: this list's, or not (as GatherFrameMeshes has it).
	std::vector<RenderingMesh*> moreOpaque, moreSeen;
	for (size_t k = 0; k < added.size(); k++)
	{
		RenderingMesh* m = added[k];
		if (m == NULL) continue;
		RenderingComponent* rc = m->renderingComponent;
		if (rc->GetRenderLayer() != renderLayer) continue;
		if (Tag != 0 && !rc->GetOwner()->HaveTag(Tag)) continue;
		if (m->Material->IsTransparent() && sorting) moreSeen.push_back(m); else moreOpaque.push_back(m);
	}

	// The kept arrays closed up over what went, and opened for what came.
	if (nRemoved != 0 || !moreOpaque.empty() || !moreSeen.empty())
	{
		const size_t nO2 = nO - [&]() { size_t c = 0; if (nRemoved) for (size_t i = 0; i < nO; i++) c += removed[i]; return c; }() + moreOpaque.size();
		const size_t nT2 = n - nRemoved - (nO2 - moreOpaque.size()) + moreSeen.size();
		const size_t n2 = nO2 + nT2;
		std::vector<uint32> remap(n, kGone);
		std::vector<RenderingMesh*> opaque2(nO2), seen2(nT2);
		std::vector<IMaterial*> materialOf2(n2, (IMaterial*)NULL);
		std::vector<uint8> activeOf2(n2, 0), flags2(n2, 0);
		std::vector<Vec4> sphere2(n2);
		std::vector<uint32> cellOf2(n2, kNoCell);
		std::vector<Vec3> place2(nT2);
		const bool cells = L.cellOf.size() == n;
		size_t to = 0;
		for (size_t i = 0; i < n; i++)
		{
			if (i == nO)
			{
				for (size_t k = 0; k < moreOpaque.size(); k++) { opaque2[to] = moreOpaque[k]; changed.push_back(kGone); to++; }      // (their places: below)
			}
			if (nRemoved && removed[i]) continue;
			if (i < nO) opaque2[to] = L.opaque[i]; else { seen2[to - nO2] = L.translucent[i - nO]; place2[to - nO2] = L.translucentPlace[i - nO]; }
			materialOf2[to] = L.materialOf[i]; activeOf2[to] = L.activeOf[i]; flags2[to] = L.flags[i]; sphere2[to] = L.sphere[i];
			if (cells) cellOf2[to] = L.cellOf[i];
			remap[i] = (uint32)to;
			to++;
		}
		if (nT == 0) for (size_t k = 0; k < moreOpaque.size(); k++) { opaque2[to] = moreOpaque[k]; changed.push_back(kGone); to++; }
		for (size_t k = 0; k < moreSeen.size(); k++) { seen2[to - nO2] = moreSeen[k]; to++; }
		// (the places already changed, where they are now; and the new ones)
		{
			std::vector<uint32> now;
			for (size_t k = 0; k < changed.size(); k++) if (changed[k] != kGone && remap[changed[k]] != kGone) now.push_back(remap[changed[k]]);
			for (size_t k = 0; k < moreOpaque.size(); k++) now.push_back((uint32)(nO2 - moreOpaque.size() + k));
			for (size_t k = 0; k < moreSeen.size(); k++) now.push_back((uint32)(n2 - moreSeen.size() + k));
			changed.swap(now);
		}
		for (std::unordered_map<GameObject*, std::vector<uint32> >::iterator w = L.where.begin(); w != L.where.end();)
		{
			std::vector<uint32> &v = w->second;
			size_t kept = 0;
			for (size_t e = 0; e < v.size(); e++) if (remap[v[e]] != kGone) v[kept++] = remap[v[e]];
			v.resize(kept);
			if (kept == 0) w = L.where.erase(w); else ++w;
		}
		L.opaque.swap(opaque2); L.translucent.swap(seen2); L.materialOf.swap(materialOf2); L.activeOf.swap(activeOf2);
		L.flags.swap(flags2); L.sphere.swap(sphere2); L.cellOf.swap(cellOf2); L.translucentPlace.swap(place2);
		for (size_t k = 0; k < moreOpaque.size(); k++)
			if (GameObject* o = moreOpaque[k]->renderingComponent->GetOwner()) L.where[o].push_back((uint32)(nO2 - moreOpaque.size() + k));
		for (size_t k = 0; k < moreSeen.size(); k++)
			if (GameObject* o = moreSeen[k]->renderingComponent->GetOwner()) L.where[o].push_back((uint32)(n2 - moreSeen.size() + k));
	}

	// What each changed place holds now.
	const size_t nOpaque = L.opaque.size();
	bool fits = true;
	cullSphere.swap(L.sphere); cullFlags.swap(L.flags);
	for (size_t k = 0; k < changed.size() && fits; k++)
	{
		const uint32 idx = changed[k];
		RenderingMesh* m = idx < nOpaque ? L.opaque[idx] : L.translucent[idx - nOpaque];
		// (a level that is seen through where the last was not, or the other way: not in its place)
		if ((m->Material->IsTransparent() && sorting) != (idx >= nOpaque)) { fits = false; break; }
		CullEntry(idx, m);
		L.materialOf[idx] = m->Material.get();
		L.activeOf[idx] = m->Active == true ? 1 : 0;
		if (idx < L.cellOf.size()) L.cellOf[idx] = kNoCell;
		if (idx >= nOpaque)
		{
			GameObject* o = m->renderingComponent->GetOwner();
			L.translucentPlace[idx - nOpaque] = o != NULL ? o->GetWorldPosition() : Vec3();
		}
		if (m->Material && nRemoved == 0 && L.materialSeen.insert(m->Material.get()).second)
		{
			L.materials.push_back(std::make_pair(m->Material.get(), m->Material->IsTransparent()));
			MaterialListed(m, m->Material.get());
		}
	}
	cullSphere.swap(L.sphere); cullFlags.swap(L.flags);
	if (!fits) return false;

	// The materials still some mesh's (one whose last mesh went may be gone
	// with it), each as it was last seen.
	if (nRemoved != 0)
	{
		std::unordered_map<IMaterial*, bool> was;
		for (size_t i = 0; i < L.materials.size(); i++) was[L.materials[i].first] = L.materials[i].second;
		L.materials.clear(); L.materialSeen.clear();
		for (size_t i = 0; i < L.materialOf.size(); i++)
		{
			IMaterial* mat = L.materialOf[i];
			if (mat == NULL || !L.materialSeen.insert(mat).second) continue;
			std::unordered_map<IMaterial*, bool>::iterator w = was.find(mat);
			L.materials.push_back(std::make_pair(mat, w != was.end() ? w->second : mat->IsTransparent()));
			if (w == was.end()) MaterialListed(i < L.opaque.size() ? L.opaque[i] : L.translucent[i - L.opaque.size()], mat);
		}
	}
	return true;
}

// The frame's list, kept. See FrameList in the header and RenderState.h.
void IRenderer::UseFrameList(SceneGraph* Scene, GameObject* Camera, const uint32 Tag)
{
	static const bool keep = []() { const char* v = std::getenv("PYROS_FRAME_LISTS"); return !(v && v[0] == '0'); }();
	static const bool verify = std::getenv("PYROS_VERIFY_LISTS") != NULL;
	if (!keep)
	{
		rmesh = GroupAndSortAssets(Scene, Camera, Tag);
		BuildCullList();
		return;
	}
	FrameList &L = frameLists[std::make_pair(Scene, Tag)];
	bool fresh = !L.valid || L.version != RenderState::Version.load(std::memory_order_relaxed)
		|| L.layer != renderLayer || L.lod != lod || L.sorting != sorting;

	// (what has come and gone since: before anything of the kept list is followed)
	if (!fresh && !ApplyListed(L, Scene, Tag)) fresh = true;
	if (!fresh)
	{
		// Levels of detail follow the camera every frame (and say so, through
		// RenderState's log, when one of them changes what the scene draws).
		if (lod)
		{
			const Vec3 eye = Camera->GetWorldPosition();
			// Which level each thing wants is worked out on every core - it reads
			// the thing and writes nothing shared - and only the ones that want a
			// different level than they have are then changed, here, in order.
			// (Thousands of things, every frame, were each asked on this thread.)
			static thread_local std::vector<uint32> want;
			const size_t nLod = L.lodComponents.size();
			want.resize(nLod);
			RenderingComponent** const lodOf = L.lodComponents.data();
			uint32* const wantOf = want.data();
			auto pick = [lodOf, wantOf, eye](uint32 begin, uint32 end) {
				for (uint32 i = begin; i < end; i++)
				{
					RenderingComponent* c = lodOf[i];
					const Vec3 &scale = c->GetOwner()->GetScale();
					const f32 maxScale = std::max(fabs(scale.x), std::max(fabs(scale.y), fabs(scale.z)));
					const Vec3 centre = c->GetOwner()->GetWorldPosition() + c->GetBoundingSphereCenter() * scale;
					const f32 d = std::max(0.f, eye.distance(centre) - c->GetBoundingSphereRadius() * maxScale);
					const uint32 level = c->GetLODByDistance(d * d);
					wantOf[i] = level != c->GetLODInUse() ? level : 0xFFFFFFFFu;
				}
			};
			static thread_local JobSystem::SharedLoop lodLoop;
			if (lodLoop.Begin((uint32)nLod, 512)) JobSystem::Instance().ParallelFor((uint32)nLod, 256, pick);
			else pick(0, (uint32)nLod);
			lodLoop.End();
			FrameProfiler::Instance().Counter("Lists.LodShared", lodLoop.shared ? 1.0 : 0.0);
			for (size_t i = 0; i < nLod; i++) if (wantOf[i] != 0xFFFFFFFFu) lodOf[i]->UpdateLOD(wantOf[i]);
		}
		if (!fresh && !ApplyListed(L, Scene, Tag)) fresh = true;
		// (one that has got instances, or has none left: its entries are read
		// again, as a moved object's are)
		if (GameObject::DrawCopies()) for (size_t i = 0; i < L.instanced.size(); i++) static_cast<IRenderingInstancedComponent*>(L.instanced[i].first)->TakeDrawCount();
		for (size_t i = 0; i < L.instanced.size() && !fresh; i++)
		{
			const bool none = static_cast<IRenderingInstancedComponent*>(L.instanced[i].first)->NumberOfInstances() == 0;
			if (none == L.instanced[i].second) continue;
			L.instanced[i].second = none;
			if (!g_patchLists) fresh = true;
			else if (GameObject* o = L.instanced[i].first->GetOwner()) RenderState::NoteMoved(o); else fresh = true;
		}
		// (a mesh's material and its own switch are fields anybody writes:
		// looked at, one place in memory a mesh)
		const size_t nOpaque = L.opaque.size();
		if (!fresh)
		{
			// (every listed mesh looked at, to see that it still wears what it wore
			// and is still switched as it was: read only, so on every core)
			const size_t nAll = nOpaque + L.translucent.size();
			std::atomic<bool> changed(false);
			FrameList* const Lp = &L;
			auto same = [Lp, nOpaque, &changed](uint32 begin, uint32 end) {
				for (uint32 i = begin; i < end; i++)
				{
					RenderingMesh* m = i < nOpaque ? Lp->opaque[i] : Lp->translucent[i - nOpaque];
					if (m->Material.get() != Lp->materialOf[i] || (m->Active == true ? 1 : 0) != Lp->activeOf[i]) { changed.store(true, std::memory_order_relaxed); return; }
				}
			};
			static thread_local JobSystem::SharedLoop sameLoop;
			if (sameLoop.Begin((uint32)nAll, 2048)) JobSystem::Instance().ParallelFor((uint32)nAll, 1024, same);
			else same(0, (uint32)nAll);
			sameLoop.End();
			FrameProfiler::Instance().Counter("Lists.SameShared", sameLoop.shared ? 1.0 : 0.0);
			if (changed.load(std::memory_order_relaxed)) fresh = true;
		}
		// (...and whether each material is seen through, which is the
		// material's to change. Asked of each material once, not of each mesh:
		// a scene has a few hundred. Only after the meshes: every one of these
		// is then still some mesh's, and so still there.)
		for (size_t i = 0; i < L.materials.size() && !fresh; i++)
			if (L.materials[i].first->IsTransparent() != L.materials[i].second) fresh = true;
		if (L.version != RenderState::Version.load(std::memory_order_relaxed)) fresh = true;
	}

	// (the kept arrays hold the opaque meshes and then those that are seen
	// through, in the order they were gathered)
	const size_t nOpaque0 = L.opaque.size();
	auto meshAt = [&L](const size_t i) -> RenderingMesh* { return i < L.opaque.size() ? L.opaque[i] : L.translucent[i - L.opaque.size()]; };
	auto placeOf = [](RenderingMesh* m) -> Vec3 {
		GameObject* o = m->renderingComponent->GetOwner();
		return o != NULL ? o->GetWorldPosition() : Vec3();
	};
	(void)nOpaque0;
	FrameProfiler::Instance().Counter("Lists.Fresh", fresh ? 1.0 : 0.0);
	if (fresh)
	{
		GatherFrameMeshes(Scene, Camera, Tag, L.opaque, L.translucent);
		L.version = RenderState::Version.load(std::memory_order_relaxed);
		L.listedSeq = RenderState::ListedCount();
		L.layer = renderLayer; L.lod = lod; L.sorting = sorting;
		L.movedSeq = RenderState::MovedCount();
		// (everything has just been read: what moves from here on has to be
		// noted again - an object is noted once between one reading and the next)
		RenderState::ReadEpoch.fetch_add(1, std::memory_order_relaxed);
		const size_t nOpaque = L.opaque.size(), n = nOpaque + L.translucent.size();
		L.materialOf.resize(n); L.activeOf.resize(n);
		L.lodComponents.clear(); L.instanced.clear(); L.where.clear();
		rmesh = L.opaque;
		rmesh.insert(rmesh.end(), L.translucent.begin(), L.translucent.end());
		BuildCullList();
		L.sphere = cullSphere; L.flags = cullFlags;
		// The grid: what is tested for culling by its sphere, and small enough
		// to belong to one cell of ground. (What moves afterwards leaves its
		// cell: the moved log, below.)
		{
			static const f32 kCell = 48.f;
			L.cellOf.assign(n, kNoCell);
			L.cellSphere.clear();
			std::unordered_map<uint64, uint32> cells;
			std::vector<Vec3> lo, hi;
			for (size_t i = 0; i < n; i++)
			{
				const uint8 f = cullFlags[i];
				const Vec4 &sp = cullSphere[i];
				if (!(f & CullOwner) || !(f & CullTested) || (f & CullBox) || !(sp.w > 0.f) || sp.w > kCell * 0.75f) continue;
				const int32 cx = (int32)floorf(sp.x / kCell), cz = (int32)floorf(sp.z / kCell);
				const uint64 key = ((uint64)(uint32)cx << 32) | (uint64)(uint32)cz;
				std::unordered_map<uint64, uint32>::iterator at = cells.find(key);
				uint32 c;
				if (at == cells.end())
				{
					c = (uint32)lo.size();
					cells[key] = c;
					lo.push_back(Vec3(sp.x - sp.w, sp.y - sp.w, sp.z - sp.w));
					hi.push_back(Vec3(sp.x + sp.w, sp.y + sp.w, sp.z + sp.w));
				}
				else
				{
					c = at->second;
					lo[c].x = std::min(lo[c].x, sp.x - sp.w); lo[c].y = std::min(lo[c].y, sp.y - sp.w); lo[c].z = std::min(lo[c].z, sp.z - sp.w);
					hi[c].x = std::max(hi[c].x, sp.x + sp.w); hi[c].y = std::max(hi[c].y, sp.y + sp.w); hi[c].z = std::max(hi[c].z, sp.z + sp.w);
				}
				L.cellOf[i] = c;
			}
			L.cellSphere.resize(lo.size());
			for (size_t c = 0; c < lo.size(); c++)
			{
				const Vec3 mid((lo[c].x + hi[c].x) * 0.5f, (lo[c].y + hi[c].y) * 0.5f, (lo[c].z + hi[c].z) * 0.5f);
				const Vec3 half(hi[c].x - mid.x, hi[c].y - mid.y, hi[c].z - mid.z);
				L.cellSphere[c] = Vec4(mid.x, mid.y, mid.z, sqrtf(half.x * half.x + half.y * half.y + half.z * half.z));
			}
		}
		L.translucentPlace.resize(L.translucent.size());
		L.materials.clear();
		std::unordered_set<IMaterial*> &seenMaterials = L.materialSeen;
		seenMaterials.clear();
		for (size_t i = 0; i < n; i++)
		{
			RenderingMesh* m = meshAt(i);
			L.materialOf[i] = m->Material.get();
			L.activeOf[i] = m->Active == true ? 1 : 0;
			if (m->Material && seenMaterials.insert(m->Material.get()).second)
			{
				L.materials.push_back(std::make_pair(m->Material.get(), m->Material->IsTransparent()));
				MaterialListed(m, m->Material.get());
			}
			RenderingComponent* rc = m->renderingComponent;
			if (rc->GetOwner() != NULL) L.where[rc->GetOwner()].push_back((uint32)i);
			if (i >= nOpaque) L.translucentPlace[i - nOpaque] = placeOf(m);
		}
		// (every component with levels, drawn at the moment or not: one that
		// is too far off to be in the list now is the one that comes back)
		{
			const std::vector<RenderingComponent*> &comps = RenderingComponent::GetRenderingComponents(Scene);
			for (size_t i = 0; i < comps.size(); i++)
			{
				if (comps[i]->HasLOD() && comps[i]->GetOwner() != NULL) L.lodComponents.push_back(comps[i]);
				// (one with no instances is left out of the list - and is the
				// one to watch for getting some)
				if (comps[i]->IsInstanced()) L.instanced.push_back(std::make_pair(comps[i], static_cast<IRenderingInstancedComponent*>(comps[i])->NumberOfInstances() == 0));
				if (comps[i]->IsInstanced() && GameObject::DrawCopies()) static_cast<IRenderingInstancedComponent*>(comps[i])->TakeDrawCount();
			}
		}
		L.valid = true;
	}
	else
	{
		// Where things are: only what has been moved since this was last here.
		const size_t nOpaque = L.opaque.size();
		std::vector<GameObject*> moved;
		cullSphere.swap(L.sphere); cullFlags.swap(L.flags);
		if (RenderState::MovedSince(L.movedSeq, moved))
		{
			L.movedSeq += moved.size();
			for (size_t k = 0; k < moved.size(); k++)
			{
				std::unordered_map<GameObject*, std::vector<uint32> >::iterator at = L.where.find(moved[k]);
				if (at == L.where.end()) continue;
				for (size_t e = 0; e < at->second.size(); e++)
				{
					const uint32 idx = at->second[e];
					CullEntry(idx, meshAt(idx));
					if (idx < L.cellOf.size()) L.cellOf[idx] = kNoCell;       // (it has moved: out of its cell, asked for itself from now on)
					if (idx >= nOpaque) L.translucentPlace[idx - nOpaque] = placeOf(meshAt(idx));
				}
			}
		}
		else
		{
			// more has moved than is remembered: all of them, then
			L.movedSeq = RenderState::MovedCount();
			L.cellOf.assign(L.cellOf.size(), kNoCell);
			for (size_t idx = 0; idx < nOpaque + L.translucent.size(); idx++)
			{
				CullEntry(idx, meshAt(idx));
				if (idx >= nOpaque) L.translucentPlace[idx - nOpaque] = placeOf(meshAt(idx));
			}
		}
		cullSphere.swap(L.sphere); cullFlags.swap(L.flags);
	}

	// This frame's: the opaque as kept, then what is seen through, furthest
	// first (nearest last - sorted by how far each is, those equally far in
	// the order they were gathered, as SortTranslucent does it; then turned
	// round).
	{
		const size_t nOpaque = L.opaque.size(), nSeen = L.translucent.size();
		const Vec3 eye = Camera->GetWorldPosition();
		std::vector<std::pair<f32, uint32> > keyed(nSeen);
		for (size_t k = 0; k < nSeen; k++) keyed[k] = std::make_pair(eye.distanceSQR(L.translucentPlace[k]), (uint32)k);
		std::stable_sort(keyed.begin(), keyed.end(), [](const std::pair<f32, uint32> &x, const std::pair<f32, uint32> &y) { return x.first < y.first; });
		rmesh.resize(nOpaque + nSeen);
		cullSphere.resize(nOpaque + nSeen); cullFlags.resize(nOpaque + nSeen);
		if (nOpaque)
		{
			std::memcpy(&rmesh[0], &L.opaque[0], nOpaque * sizeof(RenderingMesh*));
			std::memcpy(&cullSphere[0], &L.sphere[0], nOpaque * sizeof(Vec4));
			std::memcpy(&cullFlags[0], &L.flags[0], nOpaque);
		}
		const bool grid = g_cullGrid && L.cellOf.size() == nOpaque + nSeen;
		cullCell.assign(nOpaque + nSeen, kNoCell);
		if (grid && nOpaque) std::memcpy(&cullCell[0], &L.cellOf[0], nOpaque * sizeof(uint32));
		for (size_t k = 0; k < nSeen; k++)
		{
			const uint32 from = keyed[nSeen - 1 - k].second;
			rmesh[nOpaque + k] = L.translucent[from];
			cullSphere[nOpaque + k] = L.sphere[nOpaque + from];
			cullFlags[nOpaque + k] = L.flags[nOpaque + from];
			if (grid) cullCell[nOpaque + k] = L.cellOf[nOpaque + from];
		}
		if (grid) cullCellSphere = L.cellSphere; else cullCellSphere.clear();
		cullCellOut.assign(cullCellSphere.size(), 0);
	}
	if (Tag == 0 && renderLayer == RenderLayer::World) WorldLists()[Scene] = std::make_pair(this, (void*)&L);
	Scene->SetRenderingMeshesSorted(rmesh);

	// PYROS_VERIFY_LISTS=1: the kept list held against one made from scratch.
	if (verify)
	{
		static uint64 frames = 0, wrong = 0;
		const std::vector<RenderingMesh*> keptMeshes = rmesh;
		const std::vector<Vec4> keptSphere = cullSphere;
		const std::vector<uint8> keptFlags = cullFlags;
		rmesh = GroupAndSortAssets(Scene, Camera, Tag);
		BuildCullList();
		bool same = rmesh.size() == keptMeshes.size();
		size_t at = 0;
		for (size_t i = 0; same && i < rmesh.size(); i++)
			if (rmesh[i] != keptMeshes[i] || cullFlags[i] != keptFlags[i]
				|| fabsf(cullSphere[i].x - keptSphere[i].x) + fabsf(cullSphere[i].y - keptSphere[i].y) + fabsf(cullSphere[i].z - keptSphere[i].z) + fabsf(cullSphere[i].w - keptSphere[i].w) > 1e-3f)
			{ same = false; at = i; }
		frames++;
		if (!same && ++wrong <= 30)
		{
			GameObject* o = (at < rmesh.size() && rmesh[at]->renderingComponent) ? rmesh[at]->renderingComponent->GetOwner() : NULL;
			fprintf(stderr, "[lists] WRONG at frame %llu: %zu kept, %zu fresh; first difference at %zu (%s)%s\n", (unsigned long long)frames, keptMeshes.size(), rmesh.size(), at,
				o ? o->GetName().c_str() : "?", (at < keptMeshes.size() && at < rmesh.size() && rmesh[at] == keptMeshes[at]) ? (cullFlags[at] != keptFlags[at] ? " - its flags" : " - its sphere") : " - another mesh");
			if (at < keptSphere.size() && at < cullSphere.size())
				fprintf(stderr, "[lists]   %s: kept (%.3f %.3f %.3f r %.3f) fresh (%.3f %.3f %.3f r %.3f)\n", fresh ? "made afresh this frame" : "kept from before",
					keptSphere[at].x, keptSphere[at].y, keptSphere[at].z, keptSphere[at].w, cullSphere[at].x, cullSphere[at].y, cullSphere[at].z, cullSphere[at].w);
		}
		if (frames % 600 == 0) fprintf(stderr, "[lists] %llu frames, %llu wrong, %s\n", (unsigned long long)frames, (unsigned long long)wrong, fresh ? "made afresh" : "kept");
	}
}

// Mesh i of the frame's list against the frustum that is set now (and "too
// small to see" for the pass that is being drawn). The same answer as
// CullingSphereTest / CullingBoxTest.
bool IRenderer::CullListTest(const size_t i)
{
	if (!IsCulling || !culling) return true;
	if (cullFlags[i] & CullBox) return CullingBoxTest(rmesh[i], rmesh[i]->renderingComponent->GetOwner());
	const Vec4 &s = cullSphere[i];
	const Vec3 c(s.x, s.y, s.z);
	const f32 pixels = GetSmallObjectCull();
	bool in = true;
	if (pixels > 0.f && smallCullScale > 0.f && s.w > 0.f)
	{
		const f32 limit = pixels * smallCullFactor / smallCullScale;
		if (s.w * s.w < limit * limit * smallCullEye.distanceSQR(c)) in = false;
	}
	// (its cell wholly outside the view: so is it)
	if (in && i < cullCell.size() && cullCell[i] != kNoCell && cullCell[i] < cullCellOut.size() && cullCellOut[cullCell[i]]) in = false;
	if (in) in = culling->SphereInFrustum(c, s.w);
	// PYROS_VERIFY_CULL=1: every answer held against the one the mesh's own
	// object gives.
	static const bool verify = std::getenv("PYROS_VERIFY_CULL") != NULL;
	if (verify)
	{
		static uint64 asked = 0, wrong = 0;
		const bool was = CullingSphereTest(rmesh[i], rmesh[i]->renderingComponent->GetOwner());
		asked++;
		if (was != in && ++wrong <= 20) fprintf(stderr, "[cull] WRONG: %s list says %d, object says %d\n", rmesh[i]->renderingComponent->GetOwner()->GetName().c_str(), (int)in, (int)was);
		if (asked % 2000000 == 0) fprintf(stderr, "[cull] %llu asked, %llu wrong\n", (unsigned long long)asked, (unsigned long long)wrong);
	}
	return in;
}
void IRenderer::CullInParallel(std::vector<RenderingMesh*> &meshes, const std::function<bool(RenderingMesh*)> &keep)
{
	std::vector<uint8> answers;
	AnswerInParallel(meshes.size(), answers, [&](const uint32 i) -> uint8 { return keep(meshes[i]) ? 1 : 0; });
	size_t kept = 0;
	for (size_t i = 0; i < meshes.size(); i++) if (answers[i]) meshes[kept++] = meshes[i];
	meshes.resize(kept);
}

// A cascade's map is a square round the sphere that holds a slice of the
// view - several times the ground the slice itself stands on - and every
// caster in the square was drawn into it. Most throw their shadow where the
// camera is not looking. So, for the sun: the slice of the view (opened up a
// little, for a map that is kept a frame while the camera turns) and the way
// the light travels; a caster is drawn only if its bounding sphere, carried
// along the light, can come inside that slice. It errs on drawing: a caster
// is left out only when one plane of the slice has all of it behind and the
// light is not carrying its shadow toward that plane.
namespace {
	struct ShadowViewCull
	{
		bool on = false;
		Vec3 n[6], p[6];     // inward normals and a point on each plane, world space
		Vec3 light;          // the way the light travels
		f32 margin = 0.f;
		Vec3 probe[512];     // PYROS_VERIFY_SHADOW_CULL: points all through the slice itself
		bool verify = false;
	};
	thread_local ShadowViewCull g_shadowView;
	bool g_shadowViewCullWanted = true;
	bool g_terrainShadowBaked = false;
	thread_local uint32 g_shadowCastersDrawn = 0, g_shadowCastersLeftOut = 0;

	bool ShadowReachesView(RenderingMesh* m)
	{
		RenderingComponent* rc = m->renderingComponent;
		if (rc->IsInstanced() || !rc->IsCullTesting()) return true;
		GameObject* owner = rc->GetOwner();
		const Vec3 scale = owner->GetDrawScale();
		const f32 r = rc->GetBoundingSphereRadius() * Max(Max(fabs(scale.x), fabs(scale.y)), fabs(scale.z)) + g_shadowView.margin;
		const Vec3 c = owner->GetDrawWorld() * rc->GetBoundingSphereCenter();
		for (int k = 0; k < 6; k++)
		{
			const Vec3 &pn = g_shadowView.n[k];
			if (pn.dotProduct(c - g_shadowView.p[k]) < -r && pn.dotProduct(g_shadowView.light) <= 0.f)
			{
				// PYROS_VERIFY_SHADOW_CULL=1: the answer checked the slow way -
				// no point of the slice may lie in this caster's shadow.
				if (g_shadowView.verify)
				{
					const f32 real = r - g_shadowView.margin;
					for (int q = 0; q < 512; q++)
					{
						const Vec3 to = g_shadowView.probe[q] - c;
						const f32 along = to.dotProduct(g_shadowView.light);
						if (along < 0.f) continue;
						const Vec3 off = to - g_shadowView.light * along;
						if (off.dotProduct(off) < real * real)
						{
							fprintf(stderr, "[shadow-cull] WRONG: %s was left out and shadows a point of the view\n", owner->GetName().c_str());
							return true;
						}
					}
				}
				return false;
			}
		}
		return true;
	}
}
void IRenderer::SetShadowCasterViewCull(const bool on) { g_shadowViewCullWanted = on; }

// The sun's shadow pass: see the note where it is put together, in PreRender.
struct IRenderer::SunPass
{
	struct Cascade { Matrix projection; uint32 x = 0, y = 0, w = 0, h = 0; ShadowViewCull cull; };
	DirectionalLight* light = NULL;
	Matrix view;
	f32 biasFactor = 0.f, biasUnits = 0.f;
	bool cullCasters = true;
	std::vector<Cascade> cascades;
};
// A second renderer, and what it is doing: the sun's pass recorded on another
// thread into a stream of the device's own (IRenderDevice::BeginParallelStream)
// while this renderer records the scene.
struct IRenderer::Beside
{
	std::unique_ptr<IRenderer> twin;
	SunPass pass;
	void* stream = NULL;
	JobCounter counter;
	bool running = false;
	StreamCaches caches;
	uint32 drawn = 0, leftOut = 0;
};
namespace {
	bool g_parallelShadows = true;
	// A material that draws its own shadow is changed for the draw and put
	// back (RenderShadowCaster): nobody else draws with it meanwhile.
	bool DrawsOwnShadow(IMaterial* m)
	{
		return m != NULL && typeid(*m) == typeid(CustomShaderMaterial) && static_cast<CustomShaderMaterial*>(m)->HasCustomShadow();
	}
}
void IRenderer::SetParallelShadows(const bool on) { g_parallelShadows = on; }
bool IRenderer::GetParallelShadows() { return g_parallelShadows; }

void IRenderer::RecordSunPass(const SunPass &pass)
{
	DirectionalLight* d = pass.light;
	// Bind FBO
	d->GetShadowFBO()->Bind();

	ClearBufferBit(Buffer_Bit::Depth);
	EnableClearDepthBuffer();
	ClearDepthBuffer();
	ClearScreen();

	StartClippingPlanes();

	// Enable Depth Bias
	SetShadowDepthBias(pass.biasFactor, pass.biasUnits); // enable polygon offset fill to combat "z-fighting"

	ViewMatrix = pass.view;
	for (size_t i = 0; i < pass.cascades.size(); i++)
	{
		const SunPass::Cascade &c = pass.cascades[i];
		ProjectionMatrix = c.projection;
		// Set Viewport
		_SetViewPort(c.x, c.y, c.w, c.h);
		// Update Culling
		UpdateCulling(ProjectionMatrix*ViewMatrix);
		g_shadowView = c.cull;
		if (i == 0) g_shadowCastersDrawn = g_shadowCastersLeftOut = 0;
		RenderShadowCasters(pass.cullCasters);
		g_shadowView.on = false;
		if (!t_recordingBeside)
		{
			FrameProfiler::Instance().Counter("Shadow.SunCasters", (f64)g_shadowCastersDrawn);
			FrameProfiler::Instance().Counter("Shadow.SunLeftOut", (f64)g_shadowCastersLeftOut);
		}
	}

	EndClippingPlanes();

	// Disable Depth Bias
	DisableDepthBias();

	// Unbind FBO
	d->GetShadowFBO()->UnBind();
}

// The same pass, recorded by the second renderer on another thread. False where
// that cannot be (the device carries out calls as they are made, or it is
// switched off): the caller records it itself.
bool IRenderer::RecordSunBeside(const SunPass &pass, GameObject* Camera, SceneGraph* Scene)
{
	if (!g_parallelShadows || !recordsBeside || t_recordingBeside || JobSystem::Instance().WorkerCount() == 0) return false;
	FinishBeside();
	// A material that draws its own shadow is changed for that draw and put
	// back (RenderShadowCaster) - under the scene's own draws of it, were
	// they recorded at the same time. With one about, the pass is recorded
	// in place.
	{
		const std::map<std::pair<SceneGraph*, uint32>, FrameList>::const_iterator kept = frameLists.find(std::make_pair(Scene, (uint32)0));
		if (kept == frameLists.end() || !kept->second.valid) return false;
		for (size_t i = 0; i < kept->second.materials.size(); i++)
			if (DrawsOwnShadow(kept->second.materials[i].first)) return false;
	}
	if (!beside) beside.reset(new Beside());
	Beside &B = *beside;
	if (!B.twin)
	{
		B.twin.reset(new IRenderer(Width, Height));
		B.twin->recordsBeside = false;
	}
	void* stream = device->BeginParallelStream();
	if (stream == NULL) return false;

	// What the second renderer draws from: this one's list, as it stands.
	IRenderer &T = *B.twin;
	T.rmesh = rmesh; T.cullSphere = cullSphere; T.cullFlags = cullFlags;
	T.cullCell = cullCell; T.cullCellSphere = cullCellSphere;
	if (IsCulling && !T.culling) T.ActivateCulling(0);
	T.IsCulling = IsCulling;
	T.smallCullEye = smallCullEye; T.smallCullScale = smallCullScale; T.smallCullFactor = smallCullFactor;
	T.Camera = Camera; T.Scene = Scene; T.Timer = Timer;
	T.CameraPosition = CameraPosition; T.NearFarPlane = NearFarPlane;
	T.projection = projection; T.projectionValid = projectionValid;
	T.ClipPlane = ClipPlane; T.ClipPlaneNumber = ClipPlaneNumber;
	for (uint32 k = 0; k < 8; k++) T.ClipPlanes[k] = ClipPlanes[k];
	T.Width = Width; T.Height = Height;
	T.ResetViewPort();
	T.BeginAutoInstancingFrame();

	B.pass = pass;
	B.stream = stream;
	B.caches = StreamCaches();
	B.running = true;
	g_besideActive.fetch_add(1);
	IRenderDevice* dev = device.get();
	Beside* b = &B;
	JobSystem::Instance().Run([b, dev]() {
		dev->EnterParallelStream(b->stream);
		t_caches = &b->caches;
		t_recordingBeside = true;
		Texture::UseOwnUnitCounter(true);
		FrameBuffer::UseOwnBoundStack(true);
		g_fingerprintsThisView.clear();
		b->twin->InitRender();
		b->twin->RecordSunPass(b->pass);
		b->twin->EndRender();
		b->drawn = g_shadowCastersDrawn; b->leftOut = g_shadowCastersLeftOut;
		Texture::UseOwnUnitCounter(false);
		FrameBuffer::UseOwnBoundStack(false);
		t_recordingBeside = false;
		t_caches = NULL;
		dev->LeaveParallelStream(b->stream);
	}, B.counter);

	// What this renderer goes on recording is carried out AFTER that pass:
	// the device is then as that pass left it, not as this renderer last
	// knew it. So: nothing taken as known, and what it holds to be set, set.
	InvalidateSharedUniformCaches();
	LastProgramUsed = -1; LastMaterialUsed = -1; LastMeshRendered = -1;
	LastMaterialPTR = NULL; LastMeshRenderedPTR = NULL;
	InternalDrawType = -1;
	cullFace = -1;
	ResetViewPort();
	FrameBuffer::RebindBound();
	DepthWrite();
	return true;
}

namespace {
	bool g_parallelPasses = !(std::getenv("PYROS_PARALLEL_PASSES") && std::getenv("PYROS_PARALLEL_PASSES")[0] == '0');
	// (fewer things than this are not worth handing round)
	const uint32 kUnitsARun = 40;
}
void IRenderer::SetParallelPasses(const bool on) { g_parallelPasses = on; }
bool IRenderer::GetParallelPasses() { return g_parallelPasses; }

// See the header. What a run draws is the same DrawWithAutoInstancing would have:
// the groups are found once, here, and each run draws its own share of them.
void IRenderer::DrawPassOnEveryCore(const std::vector<RenderingMesh*> &items, const std::vector<uint64> *signatures,
	const PassDraw &drawOne, const PassDraw &drawBatch, GameObject* Camera, SceneGraph* Scene, const uint32 pass)
{
	const uint32 n = (uint32)items.size();
	const uint32 workers = JobSystem::Instance().WorkerCount();
	// (not with a volume of probes lighting the scene: its textures are this renderer's to bind)
	const bool can = g_parallelPasses && (recordsBeside || passesBeside) && !t_recordingBeside && workers > 0 && IsAutoInstancing() && n >= kUnitsARun * 2
		&& EffectiveAmbientMode() != 3;
	if (!can)
	{
		DrawWithAutoInstancing(items, signatures,
			[&](RenderingMesh* m, uint32 i) { drawOne(*this, m, i); },
			[&](RenderingMesh* m, uint32 i) { drawBatch(*this, m, i); });
		return;
	}

	static thread_local Scratch S;
	GroupForInstancing(items, signatures, &S);
	// What there is to draw, in order: a thing by itself, or the first of a batch.
	static thread_local std::vector<uint32> units;
	units.clear();
	for (uint32 i = 0; i < n; i++)
	{
		const int32 g = S.groupOf[i];
		if (g < 0 || S.count[g] < kAutoInstanceMinimum || S.members[S.first[g]] == i) units.push_back(i);
	}
	const uint32 total = (uint32)units.size();
	// How many runs: as many as this machine draws fastest with - which is found
	// out, not assumed. Cores are not all alike (a laptop's slow ones take three
	// times as long over the same run, and the frame waits for the slowest), other
	// things want them, and a run has a cost of its own to set up. So the pass is
	// timed; every few seconds one run fewer and one run more are tried for a few
	// frames each, and what was quickest for each thing drawn is kept - one run,
	// this renderer by itself, among them.
	struct Tuner
	{
		uint32 runs = 1;                 // what is in use
		uint32 frame = 0, phase = 0, left = 0, trying = 0;
		f64 sum[3] = { 0, 0, 0 }; uint32 count[3] = { 0, 0, 0 };
		std::chrono::steady_clock::time_point began;
	};
	// (one for each pass of each renderer: a pass of two hundred panes of glass and one
	// of two thousand things do not want the same number of runs)
	static thread_local std::map<std::pair<IRenderer*, uint32>, Tuner> tuners;
	Tuner &U = tuners[std::make_pair(this, pass)];
	const uint32 most = std::max((uint32)1, std::min(workers + 1, total / kUnitsARun));
	if (U.runs > most) U.runs = most;
	uint32 runs = U.runs;
	{
		// (a look every 300 frames - every 60 for the first thousand, while it is
		// finding its place: 24 frames as it is, 24 with one fewer, 24 with one more)
		U.frame++;
		const uint32 every = U.frame < 1000 ? 60 : 300;
		if (U.phase == 0 && U.frame % every == 0) { U.phase = 1; U.left = 24; U.sum[0] = U.sum[1] = U.sum[2] = 0; U.count[0] = U.count[1] = U.count[2] = 0; }
		if (U.phase == 1) runs = U.runs;
		else if (U.phase == 2) runs = U.runs > 1 ? U.runs - 1 : U.runs;
		else if (U.phase == 3) runs = std::min(most, U.runs + 1);
		// (PYROS_PASS_RUNS=n: that many, for a test - where there are things enough)
		static const uint32 forced = std::getenv("PYROS_PASS_RUNS") ? (uint32)std::atoi(std::getenv("PYROS_PASS_RUNS")) : 0;
		if (forced > 0) { runs = std::max((uint32)1, std::min(most, forced)); U.phase = 0; }
		U.trying = runs;
		U.began = std::chrono::steady_clock::now();
	}
	struct Timed
	{
		Tuner &U; uint32 total; uint32 most;
		~Timed()
		{
			if (U.phase == 0 || total == 0) return;
			const f64 ns = std::chrono::duration<f64, std::nano>(std::chrono::steady_clock::now() - U.began).count() / (f64)total;
			const uint32 slot = U.phase - 1;
			U.sum[slot] += ns; U.count[slot]++;
			if (--U.left > 0) return;
			if (U.phase < 3) { U.phase++; U.left = 24; return; }
			// the three, for each thing drawn: the one in use keeps its place unless
			// another is clearly quicker (3%)
			const f64 now = U.sum[0] / std::max<uint32>(1, U.count[0]);
			const f64 fewer = U.runs > 1 ? U.sum[1] / std::max<uint32>(1, U.count[1]) : 1e30;
			const f64 more = U.runs < most ? U.sum[2] / std::max<uint32>(1, U.count[2]) : 1e30;
			if (fewer < now * 0.97 && fewer <= more) U.runs--;
			else if (more < now * 0.97) U.runs++;
			U.phase = 0;
		}
	} timed = { U, total, most };
	FrameProfiler::Instance().Counter(pass == 0 ? "Pass.Runs" : "Pass.RunsSeeThrough", (f64)runs);
	if (runs < 2)
	{
		DrawWithAutoInstancing(items, signatures,
			[&](RenderingMesh* m, uint32 i) { drawOne(*this, m, i); },
			[&](RenderingMesh* m, uint32 i) { drawBatch(*this, m, i); });
		return;
	}

	struct Run
	{
		static void Draw(IRenderer &R, const std::vector<RenderingMesh*> &items, const Scratch &S, const uint32* unit, const uint32 count,
			const PassDraw &drawOne, const PassDraw &drawBatch, uint32 &singles, uint32 &batches, uint32 &batched)
		{
			for (uint32 u = 0; u < count; u++)
			{
				const uint32 i = unit[u];
				const int32 g = S.groupOf[i];
				if (g < 0 || S.count[g] < kAutoInstanceMinimum) { drawOne(R, items[i], i); singles++; continue; }
				const uint32* members = S.members.data() + S.first[g];
				const uint32 size = S.count[g];
				AutoInstanceBatch* b = R.AcquireAutoInstanceBatch(items[i], S.fingerprint[g], size);
				if (b == NULL)
				{
					for (uint32 k = 0; k < size; k++) { drawOne(R, items[members[k]], members[k]); singles++; }
					continue;
				}
				for (uint32 k = 0; k < size; k++)
				{
					RenderingMesh* m = items[members[k]];
					b->comp->transform[k] = m->renderingComponent->GetOwner()->GetDrawWorld() * m->Pivot;
				}
				b->comp->SetNumberInstances(size);
				b->comp->UpdateTransforms();
				drawBatch(R, b->mesh, i);
				batches++; batched += size;
			}
		}
	};

	FrameProfiler::Instance().Begin("Pass.HandOut");
	// The hands: a renderer each, kept from frame to frame.
	const uint32 hands = runs - 1;
	while (crew.size() < hands) crew.push_back(std::unique_ptr<Beside>(new Beside()));
	uint32 begun = 0;
	IRenderDevice* dev = device.get();
	const std::vector<RenderingMesh*>* itemsPtr = &items;
	const Scratch* groups = &S;
	const uint32* unitList = units.data();
	const PassDraw* one = &drawOne; const PassDraw* many = &drawBatch;
	for (uint32 h = 0; h < hands; h++)
	{
		Beside &B = *crew[h];
		if (!B.twin)
		{
			B.twin.reset(new IRenderer(Width, Height));
			B.twin->recordsBeside = false;
		}
		void* stream = dev->BeginParallelStream();
		if (stream == NULL) break;
		IRenderer &T = *B.twin;
		T.Camera = Camera; T.Scene = Scene; T.Timer = Timer;
		T.CameraPosition = CameraPosition; T.NearFarPlane = NearFarPlane;
		T.ProjectionMatrix = ProjectionMatrix; T.ViewMatrix = ViewMatrix; T.ViewProjectionMatrix = ViewProjectionMatrix;
		T.ProjectionMatrixInverse = ProjectionMatrixInverse; T.ViewMatrixInverse = ViewMatrixInverse;
		T.PrvProjectionMatrix = PrvProjectionMatrix; T.PrvViewMatrix = PrvViewMatrix;
		T.ProjectionMatrixInverseIsDirty = ProjectionMatrixInverseIsDirty; T.ViewMatrixInverseIsDirty = ViewMatrixInverseIsDirty;
		T.ViewProjectionMatrixIsDirty = ViewProjectionMatrixIsDirty;
		T.unjitteredProjectionMatrix = unjitteredProjectionMatrix;
		T.RenderingPointShadowFace = false;
		// (the light everything stands in: the renderer's own, not the process's)
		T.GlobalLight = GlobalLight; T.BackgroundColor = BackgroundColor;
		T.AmbientSky = AmbientSky; T.AmbientEquator = AmbientEquator; T.AmbientGround = AmbientGround;
		for (int k = 0; k < 9; k++) T.AmbientSH[k] = AmbientSH[k];
		T.AmbientProbeGrid = AmbientProbeGrid;
		T.AmbientMode = AmbientMode;
		T.projection = projection; T.projectionValid = projectionValid;
		T.ClipPlane = ClipPlane; T.ClipPlaneNumber = ClipPlaneNumber;
		for (uint32 k = 0; k < 8; k++) T.ClipPlanes[k] = ClipPlanes[k];
		T.Lights = Lights; T.NumberOfLights = NumberOfLights;
		// (and the shadow maps, for a pass that draws things lit and shadowed as they
		// are drawn - what is see-through, a forward view)
		T.DirectionalShadowMapsTextures = DirectionalShadowMapsTextures; T.PointShadowMapsTextures = PointShadowMapsTextures; T.SpotShadowMapsTextures = SpotShadowMapsTextures;
		T.DirectionalShadowMapsUnits = DirectionalShadowMapsUnits; T.PointShadowMapsUnits = PointShadowMapsUnits; T.SpotShadowMapsUnits = SpotShadowMapsUnits;
		T.DirectionalShadowMatrix = DirectionalShadowMatrix; T.PointShadowMatrix = PointShadowMatrix; T.SpotShadowMatrix = SpotShadowMatrix;
		T.DirectionalShadowFar = DirectionalShadowFar;
		T.NumberOfDirectionalShadows = NumberOfDirectionalShadows; T.NumberOfPointShadows = NumberOfPointShadows; T.NumberOfSpotShadows = NumberOfSpotShadows;
		T.ShadowMapsAreArrayIndexed = ShadowMapsAreArrayIndexed;
		T.unshadowed = unshadowed; T.skipShadowMaps = skipShadowMaps;
		T.Width = Width; T.Height = Height;
		T.viewPortStartX = viewPortStartX; T.viewPortStartY = viewPortStartY; T.viewPortEndX = viewPortEndX; T.viewPortEndY = viewPortEndY;
		T.BeginAutoInstancingFrame();
		B.stream = stream;
		B.caches = StreamCaches();
		B.running = true;
		B.drawn = 0; B.leftOut = 0;
		g_besideActive.fetch_add(1);
		const uint32 from = (uint32)((uint64)total * h / runs), to = (uint32)((uint64)total * (h + 1) / runs);
		Beside* b = &B;
		JobSystem::Instance().Run([b, dev, itemsPtr, groups, unitList, from, to, one, many]() {
			dev->EnterParallelStream(b->stream);
			t_caches = &b->caches;
			t_recordingBeside = true;
			Texture::UseOwnUnitCounter(true);
			FrameBuffer::UseOwnBoundStack(true);
			IRenderer &T = *b->twin;
			T.InitRender();
			T._SetViewPort(T.viewPortStartX, T.viewPortStartY, T.viewPortEndX, T.viewPortEndY);
			uint32 singles = 0, batches = 0, batched = 0;
			Run::Draw(T, *itemsPtr, *groups, unitList + from, to - from, *one, *many, singles, batches, batched);
			T.EndRender();
			b->drawn = singles; b->leftOut = batches;
			Texture::UseOwnUnitCounter(false);
			FrameBuffer::UseOwnBoundStack(false);
			t_recordingBeside = false;
			t_caches = NULL;
			dev->LeaveParallelStream(b->stream);
		}, B.counter);
		begun++;
	}

	FrameProfiler::Instance().End();
	// What this renderer records from here is carried out AFTER those runs: the
	// device is then as the last of them left it, not as this renderer knew it.
	if (begun > 0)
	{
		InvalidateSharedUniformCaches();
		LastProgramUsed = -1; LastMaterialUsed = -1; LastMeshRendered = -1;
		LastMaterialPTR = NULL; LastMeshRenderedPTR = NULL;
		InternalDrawType = -1;
		cullFace = -1;
		_SetViewPort(viewPortStartX, viewPortStartY, viewPortEndX, viewPortEndY);
		depthWritting = true; DepthWrite();
	}
	// The last run - or all that was not handed out - is this renderer's own.
	{
		PYROS_PROFILE_SCOPE("Pass.Own");
		const uint32 from = (uint32)((uint64)total * begun / runs);
		uint32 singles = 0, batches = 0, batched = 0;
		Run::Draw(*this, items, S, unitList + from, total - from, drawOne, drawBatch, singles, batches, batched);
		autoInstanceSinglesThisFrame += singles; autoInstanceBatchesThisFrame += batches; autoInstanceObjectsThisFrame += batched;
	}
	FrameProfiler::Instance().Begin("Pass.Wait");
	for (uint32 h = 0; h < begun; h++)
	{
		Beside &B = *crew[h];
		JobSystem::Instance().Wait(B.counter);
		B.running = false;
		g_besideActive.fetch_sub(1);
		autoInstanceSinglesThisFrame += B.drawn; autoInstanceBatchesThisFrame += B.leftOut;
	}
	FrameProfiler::Instance().End();
	FrameProfiler::Instance().Counter("AutoInstance.Singles", (f64)autoInstanceSinglesThisFrame);
	FrameProfiler::Instance().Counter("AutoInstance.Batches", (f64)autoInstanceBatchesThisFrame);
	FrameProfiler::Instance().Counter("AutoInstance.Objects", (f64)autoInstanceObjectsThisFrame);
}

// Until the pass being recorded beside this renderer has all been recorded.
// (What it reads - the scene's objects - may be changed again after this.)
void IRenderer::FinishBeside()
{
	if (!beside || !beside->running) return;
	JobSystem::Instance().Wait(beside->counter);
	beside->running = false;
	g_besideActive.fetch_sub(1);
	FrameProfiler::Instance().Counter("Shadow.SunCasters", (f64)beside->drawn);
	FrameProfiler::Instance().Counter("Shadow.SunLeftOut", (f64)beside->leftOut);
}

void IRenderer::RenderShadowCasters(const bool cullTest)
{
	// What each mesh has to say for itself, asked in parallel: 0 not a caster
	// here, 1 drawn, 2 left out as unable to throw a shadow into the view,
	// 3 the first mesh of a component that casts from a mesh of its own.
	std::vector<uint8> answers;
	AnswerInParallel(rmesh.size(), answers, [&](const uint32 i) -> uint8 {
		const uint8 f = cullFlags[i];
		if ((f & (CullOwner | CullCasts | CullComponentActive)) != (CullOwner | CullCasts | CullComponentActive) || (f & CullTransparent)) return 0;
		RenderingMesh* m = rmesh[i];
		RenderingComponent* rc = m->renderingComponent;
		// Something that casts with a mesh of its own for the purpose
		// (RenderingComponent::SetShadowRenderable): that, once for the
		// component, in place of every mesh of its nearest level.
		if (!rc->shadowMeshes.empty() && rc->LodInUse == 0) return (m == rc->Meshes[0][0]) ? 3 : 0;
		if (cullTest && (f & CullTested) && !CullListTest(i)) return 0;
		if (g_terrainShadowBaked && dynamic_cast<Heightfield*>(rc->GetRenderable()) != NULL) return 0;
		if (cullTest && g_shadowView.on && !ShadowReachesView(m)) return 2;
		return 1;
	});
	std::vector<RenderingMesh*> casters;
	casters.reserve(rmesh.size());
	for (size_t i = 0; i < rmesh.size(); i++)
	{
		const uint8 a = answers[i];
		if (a == 1) { g_shadowCastersDrawn++; casters.push_back(rmesh[i]); }
		else if (a == 2) g_shadowCastersLeftOut++;
		else if (a == 3)
		{
			RenderingComponent* rc = rmesh[i]->renderingComponent;
			for (size_t sm = 0; sm < rc->shadowMeshes.size(); sm++)
			{
				RenderingMesh* proxy = rc->shadowMeshes[sm];
				if (!proxy->Material || proxy->Material->IsTransparent()) continue;
				if (proxy->standsFor != NULL && !proxy->standsFor->Active) continue;       // (a part hidden on the model)
				if (cullTest && !ShadowCasterVisible(proxy)) continue;
				if (cullTest && g_shadowView.on && !ShadowReachesView(proxy)) { g_shadowCastersLeftOut++; continue; }
				g_shadowCastersDrawn++;
				casters.push_back(proxy);
			}
		}
	}
	DrawWithAutoInstancing(casters, NULL,
		[this](RenderingMesh* m, uint32) { RenderShadowCaster(m); },
		[this](RenderingMesh* m, uint32) { RenderShadowCaster(m); });
}

void IRenderer::RenderShadowCaster(RenderingMesh* mesh)
{
	GameObject* owner = mesh->renderingComponent->GetOwner();
	IMaterial* mat = mesh->Material.get();
	// Exactly the base class, like the renderers' variant swaps: subclasses
	// hand-assign extraUniforms[]. Instanced geometry has no generated
	// variant (the template reads no instance transform).
	CustomShaderMaterial* csm = (mat != NULL && typeid(*mat) == typeid(CustomShaderMaterial)) ? static_cast<CustomShaderMaterial*>(mat) : NULL;
	if (csm && !mesh->renderingComponent->IsInstanced() && csm->HasCustomShadow()
		&& csm->UseShadowVariantForNextDraw(mesh->BonesToDraw().size() > 0))
	{
		// The receive-side flag gates BindShadowMaps(): off for this draw,
		// or the maps being rendered into would also be bound for sampling.
		const bool receives = csm->IsCastingShadows();
		const bool hadBias = csm->IsDepthBiasEnabled();
		const f32 biasFactor = csm->GetDepthBiasFactor(), biasUnits = csm->GetDepthBiasUnits();
		csm->DisableCastingShadows();
		if (shadowMaterial->IsDepthBiasEnabled())
			csm->EnableDethBias(shadowMaterial->GetDepthBiasFactor(), shadowMaterial->GetDepthBiasUnits());
		else
			csm->DisableDethBias();

		RenderObject(mesh, owner, csm);

		if (hadBias) csm->EnableDethBias(biasFactor, biasUnits); else csm->DisableDethBias();
		if (receives) csm->EnableCastingShadows();
		csm->RestoreOwnProgram();
		return;
	}
	RenderObject(mesh, owner, PickShadowMaterial(mesh));
}

bool IRenderer::IsShadowMaterial(IMaterial* material) const
{
	// A CustomShaderMaterial drawing its own shadow variant - see
	// RenderShadowCaster().
	if (material != NULL && typeid(*material) == typeid(CustomShaderMaterial)
		&& static_cast<CustomShaderMaterial*>(material)->IsDrawingShadowVariant())
		return true;
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

Matrix IRenderer::JitterProjection(const Matrix &m, const Vec2 &jitter)
{
	Matrix out = m;
	for (uint32 c = 0; c < 4; c++)
	{
		out.m[c * 4 + 0] += jitter.x * m.m[c * 4 + 3];
		out.m[c * 4 + 1] += jitter.y * m.m[c * 4 + 3];
	}
	return out;
}

Matrix IRenderer::ScenePassProjection(const p3d::Projection &projection) const
{
	if (projectionJitter.x == 0.f && projectionJitter.y == 0.f)
		return projection.m;
	return JitterProjection(projection.m, projectionJitter);
}

void IRenderer::RenderOverlayObject(RenderingMesh* rmesh, GameObject* owner, IMaterial* Material)
{
	if (projectionJitter.x == 0.f && projectionJitter.y == 0.f)
	{
		RenderObject(rmesh, owner, Material);
		return;
	}
	const Matrix jittered = ProjectionMatrix;
	ProjectionMatrix = unjitteredProjectionMatrix;
	ProjectionMatrixInverseIsDirty = true;
	ViewProjectionMatrixIsDirty = true;
	RenderObject(rmesh, owner, Material);
	ProjectionMatrix = jittered;
	ProjectionMatrixInverseIsDirty = true;
	ViewProjectionMatrixIsDirty = true;
}

void IRenderer::PreRender(GameObject* Camera, SceneGraph* Scene)
{
	PreRender(Camera, Scene, 0);
}

// A shadow map only needs the casters inside that light's own frustum. This
// test existed - written out inline in all three shadow passes - and was
// commented out in every one of them, so a spot light lighting one corner of
// a level still drew EVERY mesh in the scene into its map. Measured on a
// 656-object station with two shadow-casting spots: Renderer.PreRender 29 ms
// -> 8 ms, with no visible change to the shadows, because a caster outside
// the light frustum cannot project into that map in the first place.
//
// Deliberately NOT used for the directional cascades: an object BEHIND a
// cascade's ortho box still casts into it, and the box is fitted to the view,
// not extended to the light - culling there would delete real shadows.
bool IRenderer::ShadowCasterVisible(RenderingMesh* rmesh)
{
	if (!rmesh || !rmesh->renderingComponent) return false;
	if (!rmesh->renderingComponent->IsCullTesting()) return true;
	GameObject* owner = rmesh->renderingComponent->GetOwner();
	if (!owner) return false;
	switch (rmesh->CullingGeometry)
	{
	case CullingGeometry::Box:
		return CullingBoxTest(rmesh, owner);
	case CullingGeometry::Sphere:
	default:
		return CullingSphereTest(rmesh, owner);
	}
}

void IRenderer::PreRender(GameObject* Camera, SceneGraph* Scene, const std::string &Tag = "")
{
	PreRender(Camera, Scene, MakeStringID(Tag));
}

// Smoke is drawn by a post effect, which sees a depth buffer and nothing of
// the scene. This is where it learns what is lighting that scene.
void IRenderer::PublishLightsToSmoke(const std::vector<IComponent*> &lights)
{
	// The gradient's sky and horizon are what reach the top and sides of a
	// cloud; the ground band lights the underside nobody sees.
	Vec3 ambient = (AmbientMode == 1)
		? Vec3(AmbientSky.x * 0.6f + AmbientEquator.x * 0.4f, AmbientSky.y * 0.6f + AmbientEquator.y * 0.4f, AmbientSky.z * 0.6f + AmbientEquator.z * 0.4f)
		: Vec3(GlobalLight.x, GlobalLight.y, GlobalLight.z);
	VolumetricSmoke::CaptureLights(lights, ambient * AmbientScale);
}

void IRenderer::PreRender(GameObject* Camera, SceneGraph* Scene, const uint32 Tag)
{
	// (the scene's time, read here - at the hand-over - for the passes that follow)
	if (Scene != NULL) Timer = Scene->GetTime();
	// (the ambient light asked for since the last frame: see heldAmbient)
	if (g_ambientScaleHeld) { AmbientScale = g_heldAmbientScale; g_ambientScaleHeld = false; }
	if (heldAmbient.light) { GlobalLight = heldAmbient.Light; heldAmbient.light = false; }
	if (heldAmbient.gradient) { AmbientSky = heldAmbient.Sky; AmbientEquator = heldAmbient.Equator; AmbientGround = heldAmbient.Ground; heldAmbient.gradient = false; }
	if (heldAmbient.mode) { AmbientMode = heldAmbient.Mode; heldAmbient.mode = false; }
	if (heldAmbient.sh) { for (int i = 0; i < 9; i++) AmbientSH[i] = heldAmbient.SH[i]; heldAmbient.sh = false; }
	g_fingerprintsThisView.clear();
	PYROS_PROFILE_SCOPE("Renderer.PreRender");
	// What "too small to see" is measured with, for this view: the camera's
	// place, and how many pixels a thing as wide as it is far covers (from
	// the last projection this renderer drew with - there is none yet on its
	// first frame, and nothing is left out then).
	smallCullEye = Camera != NULL ? Camera->GetWorldPosition() : Vec3();
	if (Scene != NULL && Camera != NULL) Scene->_NoteViewedFrom(smallCullEye);
	// (a scene whose terrain's shadow is baked: the terrain is left out of the shadow maps)
	g_terrainShadowBaked = Scene != NULL && Scene->GetTerrainHorizon() != NULL;
	smallCullScale = (Camera != NULL && projectionValid && projection.m.m[11] != 0.f)
		? fabsf(projection.m.m[5]) * 0.5f * (f32)(viewPortEndY > 0 ? viewPortEndY : Height) : 0.f;
	smallCullFactor = 2.f;       // the shadow passes below; RenderScene() puts it back to 1
	shadowPassCounter++;

	FinishBeside();
	BeginAutoInstancingFrame();

	// Group and Sort Meshes
	{
		PYROS_PROFILE_SCOPE("Renderer.GroupAndSort");
		UseFrameList(Scene, Camera, Tag);
	}

	// Get Lights List
	lcomps = ILightComponent::GetLightsOnScene(Scene);
	PublishLightsToSmoke(lcomps);

	// Every frame, not only when there is something to render. These hold
	// raw pointers to the lights' shadow maps and were cleared inside the
	// block below - so deleting the last light in a scene (or its last
	// mesh) left last frame's pointers in place, and RenderScene()'s
	// BindShadowMaps() bound a texture that no longer existed. GL's error
	// check aborted the editor on it; Vulkan and Metal read freed memory.
	DirectionalShadowMapsTextures.clear();
	DirectionalShadowMatrix.clear();
	NumberOfDirectionalShadows = 0;

	PointShadowMapsTextures.clear();
	PointShadowMatrix.clear();
	NumberOfPointShadows = 0;

	SpotShadowMapsTextures.clear();
	SpotShadowMatrix.clear();
	NumberOfSpotShadows = 0;

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

		ViewMatrix = Camera->GetWorldTransformation().Inverse();
		uint32 pointCounter = 0;
		uint32 spotCounter = 0;
		PYROS_PROFILE_SCOPE("Renderer.ShadowMaps");
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

					// The map made on an earlier frame, where the sun's map is
					// only redrawn every so many (SetShadowUpdateInterval). It
					// is still read with the right matrix: light-space is what
					// was kept, and this frame's camera goes on the end. What
					// is a frame old is where the casters were - nothing to
					// see at a frame's distance - and half of all the
					// triangles a frame draws can be this pass.
					{
						std::map<ILightComponent*, std::vector<Matrix> >::iterator kept = sunLightSpace.find(d);
						if (g_shadowEvery > 1 && (shadowPassCounter % g_shadowEvery) != 0
							&& kept != sunLightSpace.end() && kept->second.size() == d->GetNumberCascades() && d->GetShadowMapTexture() != NULL)
						{
							const Matrix cameraNow = Camera->GetWorldTransformation();
							for (size_t c = 0; c < kept->second.size(); c++)
								DirectionalShadowMatrix.push_back(kept->second[c] * cameraNow);
							DirectionalShadowMapsTextures.push_back(d->GetShadowMapTexture());
							DirectionalShadowFar = d->GetCascadeSplits();
							break;
						}
						sunLightSpace[d].clear();
					}

					// What each cascade is drawn with, worked out here; then the drawing -
					// by this renderer, in place, or by a second one on another thread
					// while this one goes on to the scene (RecordSunPass, RecordSunBeside).
					SunPass pass;
					pass.light = d;
					pass.biasFactor = d->GetShadowBiasFactor(); pass.biasUnits = d->GetShadowBiasUnits();
					static const bool cullCasters = std::getenv("PYROS_NO_SHADOW_CULL") == NULL;
					pass.cullCasters = cullCasters;

					ViewMatrix = DirectionalLight::ShadowViewMatrix(direction);
					pass.view = ViewMatrix;

					// The camera actually rendering - see FitCascade(). Before
					// the first RenderScene() there is none yet, so fall back
					// to a plain 60-degree view rather than read garbage.
					Projection cameraProjection = projection;
					if (!projectionValid)
						cameraProjection.Perspective(60.f, 16.f / 9.f, 0.1f, 1000.f);
					const Matrix cameraWorld = Camera->GetWorldTransformation();


					for (uint32 i = 0; i < d->GetNumberCascades(); i++)
					{
						ProjectionMatrix = (cullSphere.size() == rmesh.size() && cullFlags.size() == rmesh.size() && !rmesh.empty())
							? d->FitCascade(i, cameraWorld, cameraProjection, ViewMatrix, rmesh, &cullSphere[0], &cullFlags[0], (uint8)(CullOwner | CullCasts))
							: d->FitCascade(i, cameraWorld, cameraProjection, ViewMatrix, rmesh);

						pass.cascades.push_back(SunPass::Cascade());
						SunPass::Cascade &c = pass.cascades.back();
						c.projection = ProjectionMatrix;
						c.x = (uint32)((float)(i % 2) * d->GetShadowWidth()); c.y = (uint32)((i <= (uint32)1 ? 0.0f : 1.f) * d->GetShadowHeight());
						c.w = d->GetShadowWidth(); c.h = d->GetShadowHeight();
						{
							c.cull.on = false;
							if (g_shadowViewCullWanted && cameraProjection.m.m[11] != 0.f)
							{
								const Cascade slice = d->GetCascade(i);
								const f32 sliceFar = slice.Far;
								const f32 sliceNear = i == 0 ? d->GetCascade(0).Near : d->GetCascade(i - 1).Far * (1.f - DirectionalLight::CascadeBlendFraction);
								// (opened up: a quarter wider than the view, and more
								// when the map is kept for frames the camera may turn in)
								const f32 open = Min(2.f, 1.25f + 0.35f * (f32)(g_shadowEvery > 1 ? g_shadowEvery - 1 : 0));
								const f32 tanX = open / cameraProjection.m.m[0];
								const f32 tanY = open / cameraProjection.m.m[5];
								c.cull.margin = 2.f + sliceFar * 0.03f;
								const Vec3 cn[6] = { Vec3(0, 0, -1), Vec3(0, 0, 1), Vec3(1, 0, -tanX), Vec3(-1, 0, -tanX), Vec3(0, 1, -tanY), Vec3(0, -1, -tanY) };
								const Vec3 cp[6] = { Vec3(0, 0, -sliceNear), Vec3(0, 0, -sliceFar), Vec3(), Vec3(), Vec3(), Vec3() };
								for (int k = 0; k < 6; k++)
								{
									c.cull.p[k] = cameraWorld * cp[k];
									c.cull.n[k] = ((cameraWorld * (cp[k] + cn[k])) - c.cull.p[k]).normalize();
								}
								// the light looks down its own -Z
								c.cull.light = Vec3(-ViewMatrix.m[2], -ViewMatrix.m[6], -ViewMatrix.m[10]).normalize();
								static const bool verify = std::getenv("PYROS_VERIFY_SHADOW_CULL") != NULL;
								c.cull.verify = verify;
								if (verify)
								{
									const f32 tx = 1.f / cameraProjection.m.m[0], ty = 1.f / cameraProjection.m.m[5];
									for (int q = 0; q < 512; q++)
									{
										const f32 depth = sliceNear + (sliceFar - sliceNear) * ((f32)(q / 64) / 7.f);
										const f32 u = (f32)(q % 8) / 3.5f - 1.f, v = (f32)((q / 8) % 8) / 3.5f - 1.f;
										c.cull.probe[q] = cameraWorld * Vec3(u * tx * depth, v * ty * depth, -depth);
									}
								}
								c.cull.on = true;
							}
						}

						// device->TranslateProjectionMatrix() (identity on
						// GL) - this matrix maps a view-space fragment
						// into the shadow map's own UV+depth space for
						// the main pass's comparison lookup, so it must
						// use the *same* clip-space convention the
						// shadow map was actually rendered with.
						// device->TranslateShadowBiasMatrix() (not the raw
						// Matrix::BIAS constant) for the same reason - see
						// its comment in IRenderDevice.h for why using
						// BIAS directly here double-transforms Z on
						// Vulkan.
						sunLightSpace[d].push_back(device->TranslateShadowBiasMatrix() * (device->TranslateProjectionMatrix(ProjectionMatrix) * ViewMatrix));
						DirectionalShadowMatrix.push_back((device->TranslateShadowBiasMatrix() * (device->TranslateProjectionMatrix(ProjectionMatrix) * ViewMatrix * cameraWorld)));
					}

					if (!RecordSunBeside(pass, Camera, Scene)) RecordSunPass(pass);

					// Get Texture (only 1)
					DirectionalShadowMapsTextures.push_back(d->GetShadowMapTexture());

					// Linear view-space far distance per cascade. The shaders
					// select (and cross-fade) cascades on the fragment's own
					// view depth. This used to be each distance pushed
					// through the camera's projection into window depth and
					// compared with gl_FragCoord.z - correct only while the
					// matrix used here matched the backend's clip
					// convention, which the deferred path's copy never did.
					DirectionalShadowFar = d->GetCascadeSplits();

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
					// (this renderer's own shadow draws use the programs the sun's pass is
					// being recorded with on another thread: that one first)
					FinishBeside();

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
						RenderShadowCasters(true);

						EndClippingPlanes();

					}

					// Put the scene's own clear colour back - see the
					// device->SetClearColor() call in the face loop above.
					device->SetClearColor(BackgroundOverrideSet ? BackgroundOverride : (BackgroundColorSet ? BackgroundColor : Vec4(0.f, 0.f, 0.f, 1.f)));

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
					// (this renderer's own shadow draws use the programs the sun's pass is
					// being recorded with on another thread: that one first)
					FinishBeside();

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
					RenderShadowCasters(true);

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
		UnbindGITextures();
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
	// Keep an intentionally DoubleSided override (shadow materials,
	// deferred fullscreen quads). Falling through to the mesh's BackFace
	// undoes why those overrides are DoubleSided: a caster with flipped
	// winding or an open shell writes nothing into the shadow map, so the
	// object receives shadows from others but never self-shadows or casts.
	if (cf == CullFace::DoubleSided)
		return cf;
	if (rmesh->Material && rmesh->Material.get() != Material)
	{
		const uint32 meshCf = rmesh->Material->GetCullFace();
		if (meshCf != cf)
			return meshCf;
	}
	return cf;
}

void IRenderer::RenderObject(RenderingMesh* rmesh, GameObject* owner, IMaterial* Material)
{
	// (an instanced component with no instances at the moment is in the list all the same: nothing to draw)
	if (rmesh->renderingComponent && rmesh->renderingComponent->IsInstanced()
		&& static_cast<IRenderingInstancedComponent*>(rmesh->renderingComponent)->InstancesToDraw() == 0) return;
	// (whoever animates only what is drawn - RenderingComponent::SetAnimateWhenUnseen - is told)
	if (rmesh->renderingComponent) rmesh->renderingComponent->MarkSeen();

	// See the comment on CommandBufferHandle in IRenderDevice.h - GL ignores
	// this value entirely (ignored/no-op on this backend), so per-object
	// granularity here costs nothing; a real per-frame command buffer is a
	// Phase 5 Step D concern once a real VulkanRenderDevice needs Begin/End
	// to mean something.
	CommandBufferHandle cmd = device->BeginCommandBuffer();

	// model cache
	// (where it is drawn: see GameObject::GetDrawWorld)
	PrvModelMatrix = owner->GetDrawPrvWorld() * rmesh->Pivot;
	ModelMatrix = owner->GetDrawWorld() * rmesh->Pivot;

	NormalMatrixIsDirty = true;
	ModelViewMatrixIsDirty = true;
	ModelViewProjectionMatrixIsDirty = true;
	ModelMatrixInverseIsDirty = true;
	ModelViewMatrixInverseIsDirty = true;
	ModelMatrixInverseTransposeIsDirty = true;
	ModelViewProjectionMatrixInverseIsDirty = true;
	ViewProjectionMatrixInverseIsDirty = true;

	// A program switch is a state change even for the same mesh+material:
	// CustomShaderMaterial draws one material with several programs (its
	// G-buffer/skinned/shadow variants), and each program has its own VAO
	// and pipeline. Comparing only the pointers skipped BindMesh() when the
	// shadow pass's last draw and the main pass's first were the same
	// object, and drew it with the other program's pipeline.
	const bool programChanged = LastProgramUsed != (int32)Material->GetShader();
	if ((LastMeshRenderedPTR != rmesh || LastMaterialPTR != Material || programChanged) && LastProgramUsed != -1)
	{
		// Material Stuff After Render
		UnbindShadowMaps(LastMaterialPTR);
		UnbindGITextures();
		// After Render
		LastMaterialPTR->AfterRender();
	}
	if (programChanged) device->UseProgram(Material->GetShader());

	if (LastMeshRenderedPTR != rmesh || LastMaterialPTR != Material || programChanged)
	{
		// Bind Mesh (resolves attribute/uniform locations; on desktop GL /
		// GLES3 also builds and caches a VAO the first time this mesh is
		// seen with this shader)
		BindMesh(rmesh, Material);

		// The VAO built by BindMesh() already has every attribute pointer
		// and the index buffer baked in.
		DeviceHandle meshVao = 0, meshPipeline = 0;
		{
			MeshCaches held(rmesh);
			meshVao = rmesh->VAOCache[Material->GetShader()];
			meshPipeline = rmesh->PipelineCache[PipelineCacheKey(Material->GetShader(), device->GetCurrentRenderTarget(), EffectiveCullFace(rmesh, Material))];
		}
		device->BindVertexArray(cmd, meshVao);

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
		device->BindPipeline(cmd, meshPipeline);

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

		// Override for transparency. Zero/Zero is "no factors chosen", not a
		// request for black: it is what every scene saved while IMaterial
		// left them uninitialised carries - see IMaterial's constructor.
		if (Material->blending && !(Material->sfactor == BlendFunc::Zero && Material->dfactor == BlendFunc::Zero))
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
	if (g_trisDump.on) g_trisDump.Add(rmesh, owner, IsShadowMaterial(Material));
	if (rmesh->renderingComponent->IsInstanced())
	{
		device->DrawElementsInstanced(cmd, DrawType, rmesh->Geometry->GetIndexData().size(), ((IRenderingInstancedComponent*)rmesh->renderingComponent)->InstancesToDraw());
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
	// The run-time override (SetBackgroundOverride) stands in for a background, never for the lack of one:
	// a renderer with no background is a thumbnail or a preview, not a view of the world.
	device->SetClearColor((BackgroundOverrideSet && BackgroundColorSet) ? BackgroundOverride : (BackgroundColorSet ? BackgroundColor : kDefaultBackgroundColor));
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
	{
		// Say so. Truncating silently loses the shadows of whichever
		// occluders happened to be collected last, which looks like those
		// particular objects being broken rather than like a budget.
		// Once per overflowing count, not per frame - this runs every frame
		// and the message is about the scene, not about this frame.
		static size_t lastReported = 0;
		if (segments.size() != lastReported)
		{
			lastReported = segments.size();
			echo("WARNING: " + std::to_string(segments.size()) + " 2D occluder segments, but only "
				+ std::to_string((int)PYROS_MAX_OCCLUDERS_2D) + " fit - the rest cast no shadow."
				" Each box is 4 segments and each circle 8.");
		}
		Occluders2D.resize(PYROS_MAX_OCCLUDERS_2D);
	}
	// Upload happens on the next draw that needs it - see the dirty flag's
	// use alongside LightsUBOValid.
	Occluders2DUBOValid = false;
}

void IRenderer::SetGlobalLight(const Vec4& Light)
{
	if (GameObject::DrawCopies()) { heldAmbient.Light = Light; heldAmbient.light = true; return; }
	GlobalLight = Light;
}

void IRenderer::SetAmbientGradient(const Vec4& Sky, const Vec4& Equator, const Vec4& Ground)
{
	if (GameObject::DrawCopies()) { heldAmbient.Sky = Sky; heldAmbient.Equator = Equator; heldAmbient.Ground = Ground; heldAmbient.gradient = true; return; }
	AmbientSky = Sky;
	AmbientEquator = Equator;
	AmbientGround = Ground;
}

void IRenderer::SetAmbientMode(const uint32 Mode)
{
	if (GameObject::DrawCopies()) { heldAmbient.Mode = Mode; heldAmbient.mode = true; return; }
	AmbientMode = Mode;
}

// Mode 3 with no volume behind it falls back to flat ambient.
//
// The mode is set per renderer, and a caller copying a scene's settings
// onto a NEW renderer - the editor switching Forward<->Deferred, a
// preview, a thumbnail - hands it mode 3 before anything has solved a
// volume for it. The shader then reads the DDGI uniform block and
// atlases that were never uploaded, and the result is not black: it is
// a grid of saturated discs over every surface, which reads as broken
// GI rather than as missing GI.
uint32 IRenderer::EffectiveAmbientMode() const
{
	if (AmbientMode == 3 && DDGIVol == NULL)
		return 0;
	return AmbientMode;
}

bool IRenderer::BakeGlobalIllumination(SceneGraph *scene, const SceneGISettings &settings)
{
	if (OwnedDDGI == NULL)
		OwnedDDGI = new DDGIVolume();
	if (!BakeSceneGI(scene, settings, *OwnedDDGI))
	{
		ClearGlobalIllumination(AmbientMode == 3 ? 0 : AmbientMode);
		return false;
	}
	// Keep the geometry and its tree for per-frame refresh - see
	// UpdateGlobalIllumination. Rebuilt here rather than handed back by
	// BakeSceneGI so that function stays a single self-contained call
	// for anyone who only wants one.
	if (OwnedRayScene == NULL)
		OwnedRayScene = new RayScene();
	if (OwnedRayScene->BuildFromScene(scene))
		OwnedRayScene->Build(4);
	DDGIRaysPerProbe = settings.raysPerProbe;
	DDGIShaderRoot = settings.shaderRoot;
	DDGIFrame = settings.passes;

	// Hand the work to the GPU if this backend can take it. The CPU
	// solve above still ran and is what the first frame shows - the
	// compute path then takes over for every refresh, which is where
	// the cost actually lives.
	if (GPUCompute == NULL)
		GPUCompute = new DDGICompute();
	if (!GPUCompute->Initialize(*OwnedRayScene, *OwnedDDGI, settings.shaderRoot))
	{
		// Not an error. No compute, no shader files, or a kernel that
		// would not build - the CPU path is still correct and still
		// there, it just has to be rationed.
		delete GPUCompute;
		GPUCompute = NULL;
		echo("BakeSceneGI: GPU probe tracing unavailable - refreshing on the CPU, which needs a small probe budget.");
	}
	else
	{
		echo("BakeSceneGI: probe tracing running in compute.");
	}
	// Bump the revision so the atlases actually re-upload - the upload
	// is keyed on it, and a second bake with the same number would
	// silently keep showing the first one.
	SetDDGIVolume(OwnedDDGI, DDGIRevision + 1);
	SetAmbientMode(3);
	return true;
}

bool IRenderer::UpdateGlobalIllumination(SceneGraph *scene, const uint32 probeBudget, const f32 hysteresis)
{
	if (OwnedDDGI == NULL || OwnedRayScene == NULL || scene == NULL)
		return false;
	if (OwnedRayScene->TriangleCount() == 0)
		return false;

	// Geometry, before the lights.
	//
	// A moving LIGHT was always followed - the lights are re-read every
	// refresh. Moving GEOMETRY was not: the triangles were baked into
	// world space once and the BVH built over them once, so a door that
	// opened went on blocking light where it used to be. This is the
	// other half.
	//
	// Refit rather than rebuild. Rebuilding the tree every frame would
	// cost more than the tracing it accelerates, and a refit is exact
	// for the bounds - what it loses is split-plane quality, which
	// matters only once things have moved far from where the tree was
	// built for.
	{
		const RaySceneChange::Enum change = OwnedRayScene->RefreshTransforms();
		if (change == RaySceneChange::NeedsRebuild)
		{
			// An object the volume was built from is gone, so the
			// triangle list describes a scene that no longer exists.
			// Re-extract, which also re-sizes the GPU buffers.
			if (!OwnedRayScene->BuildFromScene(scene))
				return false;
			OwnedRayScene->Build(4);
			if (GPUCompute != NULL && !GPUCompute->UpdateGeometry(*OwnedRayScene))
			{
				// The geometry no longer fits the buffers allocated at
				// Initialize time. Rebuild the compute side outright
				// rather than trace against a stale scene.
				delete GPUCompute;
				GPUCompute = new DDGICompute();
				if (!GPUCompute->Initialize(*OwnedRayScene, *OwnedDDGI, DDGIShaderRoot))
				{
					delete GPUCompute;
					GPUCompute = NULL;
				}
			}
		}
		else if (change == RaySceneChange::Moved)
		{
			OwnedRayScene->RefitBVH();
			if (GPUCompute != NULL)
				GPUCompute->UpdateGeometry(*OwnedRayScene);
		}
	}

	if (GPUCompute != NULL)
	{
		// Lights are re-read every refresh on both paths, which is what
		// makes a moving light change the bounce.
		// No ceiling here: the whole volume fits in a frame on the GPU,
		// and the volume object survives a backend switch - leaving a
		// CPU-path budget on it would throttle the GPU for no reason.
		OwnedDDGI->SetUpdateTimeBudget(0.f);
		std::vector<RayLight> lights;
		CollectRayLights(scene, lights);
		GPUCompute->Update(*OwnedDDGI, lights, DDGIRaysPerProbe, DDGIFrame++, hysteresis, probeBudget);
	}
	else
	{
		// No compute on this backend - WebGL2 has no compute stage at
		// all, and the GLES3 profile the web, Android and Raspberry Pi
		// builds share is ES 3.0, where it does not exist either. So
		// these are exactly the machines doing this on the CPU, and
		// exactly the ones that cannot afford a fixed probe count: a
		// probe costs about 0.7 ms at 128 rays on a fast desktop and
		// several times that on a phone.
		//
		// A ceiling in milliseconds instead. Whatever the machine
		// manages in that time is what it traces; the rest are next in
		// line on the update cursor. A slow device converges slowly
		// rather than dropping frames.
		OwnedDDGI->SetUpdateTimeBudget(DDGICPUTimeBudgetMs);
		UpdateSceneGI(scene, *OwnedRayScene, *OwnedDDGI, DDGIRaysPerProbe,
			DDGIFrame++, hysteresis, probeBudget);
	}
	// The atlases changed, so the upload has to happen again - it is
	// keyed on this and would otherwise keep showing the first frame's
	// data forever.
	DDGIRevision++;
	return true;
}

void IRenderer::ClearGlobalIllumination(const uint32 fallbackMode)
{
	SetDDGIVolume(NULL, DDGIRevision + 1);
	if (OwnedDDGI != NULL) { delete OwnedDDGI; OwnedDDGI = NULL; }
	if (GPUCompute != NULL) { delete GPUCompute; GPUCompute = NULL; }
	if (OwnedRayScene != NULL) { delete OwnedRayScene; OwnedRayScene = NULL; }
	SetAmbientMode(fallbackMode);
}

void IRenderer::SetDDGIVolume(const DDGIVolume *Volume, const uint32 revision)
{
	DDGIVol = (Volume != NULL && Volume->IsValid()) ? Volume : NULL;
	DDGIRevision = revision;
}

// The filter a 32-bit float texture may be sampled with here.
//
// WebGL2 can only filter one when OES_texture_float_linear is enabled,
// and a texture asking for LINEAR without it is INCOMPLETE - it samples
// as black, with no error. That is the whole indirect light gone on the
// platform that is already tracing these probes on the CPU, so the
// answer decides the filter rather than the other way round.
//
// Asked once: the context does not change underneath us, and the
// probe atlases and the BRDF table have to agree - one of them
// sampling black while the other does not would look like a bug in the
// lighting rather than in the filter.
static uint32 FloatTextureFilter()
{
#if defined(GLES3)
#if defined(__EMSCRIPTEN__)
	static const bool filterable =
		emscripten_webgl_enable_extension(emscripten_webgl_get_current_context(),
			"OES_texture_float_linear") != 0;
	return filterable ? TextureFilter::Linear : TextureFilter::Nearest;
#else
	// Android and Raspberry Pi reach here too, and there is no way to
	// ask from this side of the GL API. Nearest always works.
	return TextureFilter::Nearest;
#endif
#else
	return TextureFilter::Linear;
#endif
}

// Makes `tex` exist at exactly this size, replacing it if the volume
// has been re-solved at a different probe count.
//
// A texture created once and then fed a differently sized atlas is the
// failure this exists to stop: UpdateData uploads the new buffer into
// the old dimensions, and the shader - which derives the atlas size
// from the probe counts in its uniform block, not from the texture -
// then reads texels that are not where it thinks. The lighting does
// not error; it goes dim and patchy, and stays that way until the
// scene is reloaded.
//
// Which is exactly what pressing Solve after editing the probe grid
// does, so it is the first thing anyone changing those numbers in the
// editor would hit.
static void EnsureGITexture(Texture *&tex, const uint32 dataType,
	const uint32 width, const uint32 height, const uint32 filter)
{
	if (width == 0 || height == 0)
		return;
	if (tex != NULL && (tex->GetWidth() == width && tex->GetHeight() == height))
		return;
	if (tex != NULL)
	{
		delete tex;
		tex = NULL;
	}
	tex = new Texture();
	tex->CreateEmptyTexture(TextureType::Texture, dataType, (int32)width, (int32)height, false);
	// Filtering across a tile is what the octahedral border exists for;
	// clamped, because repeat would wrap into the neighbouring probe.
	tex->SetMinMagFilter(filter, filter);
	tex->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
}

void IRenderer::UploadDDGIIfDirty()
{
	if (DDGIVol == NULL || DDGIRevision == DDGIUploadedRevision)
		return;

	const ProbeAtlas &irr = DDGIVol->GetIrradianceAtlas();
	const ProbeAtlas &vis = DDGIVol->GetVisibilityAtlas();

	// Float targets, not 8-bit. Irradiance is not bounded to [0,1] -
	// a probe near a bright light legitimately exceeds it, and
	// clamping produces a hard plateau exactly where the indirect
	// light is strongest. Visibility stores squared distances, which
	// are worse: at any real scene scale they leave [0,1] immediately.
	// Float atlases and how they may be filtered.
	//
	// WebGL2 can only filter a 32-bit float texture when
	// OES_texture_float_linear is present, and a texture asking for
	// LINEAR without it is INCOMPLETE - it samples as black. Not an
	// error, not a warning: the GI simply disappears on the one
	// platform that already has no compute and is tracing these probes
	// on the CPU to begin with. The extension is common on desktop
	// browsers and far from universal on mobile.
	//
	// So the GLES3 profile - which the web, Android and Raspberry Pi
	// builds share - asks for NEAREST, which float textures always
	// support. What that costs is smoothness between probe texels, not
	// correctness: the octahedral border exists to make FILTERING
	// across a tile edge read the right directions, and with nearest
	// sampling nothing filters across it. Blockier, and present.
	const uint32 kAtlasFilter = FloatTextureFilter();

	EnsureGITexture(DDGIIrradianceTex, TextureDataType::RGBA32F,
		irr.GetWidth(), irr.GetHeight(), kAtlasFilter);
	EnsureGITexture(DDGIVisibilityTex, TextureDataType::RG32F,
		vis.GetWidth(), vis.GetHeight(), kAtlasFilter);

	const ProbeAtlas &rad = DDGIVol->GetRadianceAtlas();
	if (rad.GetResolution() > 0)
		EnsureGITexture(DDGIRadianceTex, TextureDataType::RGBA32F,
			rad.GetWidth(), rad.GetHeight(), kAtlasFilter);

	DDGIIrradianceTex->UpdateData((void*)irr.GetData().data());
	DDGIVisibilityTex->UpdateData((void*)vis.GetData().data());
	if (DDGIRadianceTex != NULL)
		DDGIRadianceTex->UpdateData((void*)rad.GetData().data());

	Vec4 ddgi[5];
	ddgi[0] = Vec4(DDGIVol->origin, (f32)irr.GetProbesPerRow());
	ddgi[1] = Vec4(DDGIVol->spacing, 0.f);
	ddgi[2] = Vec4((f32)DDGIVol->counts[0], (f32)DDGIVol->counts[1], (f32)DDGIVol->counts[2], 0.f);
	ddgi[3] = Vec4((f32)irr.GetResolution(), (f32)vis.GetResolution(), 0.f, (f32)irr.GetProbesPerRow());
	// Specular description. levels == 0 is how the shader is told this
	// volume has no radiance atlas, which is also the state a device
	// that could not allocate one ends up in - so the shader must treat
	// it as "no specular", never as "sample anyway".
	ddgi[4] = Vec4((f32)rad.GetResolution(), (f32)rad.GetProbesPerRow(),
		(f32)DDGIVol->GetRadianceLevels(), DDGIVolume::MinRoughness());
	// Relocation offsets, and the flag (counts.w) that tells the shader
	// whether to believe them.
	//
	// One texel per probe, laid out with the same probes-per-row the
	// irradiance atlas uses so the index arithmetic is the same
	// everywhere. A texture rather than a uniform block because a block
	// is capped at 16KB on the targets that matter - 1024 probes - and
	// a volume larger than that had to render with relocation switched
	// off.
	{
		const std::vector<Vec4> &pd = DDGIVol->GetProbeData();
		const uint32 perRow = irr.GetProbesPerRow();
		ddgi[2].w = (!pd.empty() && perRow > 0) ? 1.f : 0.f;
		if (ddgi[2].w > 0.f)
		{
			const uint32 rows = ((uint32)pd.size() + perRow - 1) / perRow;
			// Read with texelFetch, so filtering never applies - but a
			// driver still wants a consistent sampler state, and
			// Nearest says what is meant.
			EnsureGITexture(DDGIProbeDataTex, TextureDataType::RGBA32F,
				perRow, rows, TextureFilter::Nearest);
			// The tail of the last row matters: a probe index never
			// reaches it, but leaving it uninitialised means a driver
			// reading past the upload sees whatever was there. Active
			// and unoffset is the harmless value.
			std::vector<Vec4> texels((size_t)perRow * rows, Vec4(0.f, 0.f, 0.f, 1.f));
			for (size_t i = 0; i < pd.size(); i++) texels[i] = pd[i];
			DDGIProbeDataTex->UpdateData((void*)texels.data());
		}
	}
	device->ReplaceUniformBuffer(DDGIUniformsUBO, sizeof(ddgi), ddgi);
	DDGIUploadedRevision = DDGIRevision;
}

void IRenderer::BuildBRDFLutIfNeeded()
{
	if (BRDFLutTex != NULL)
		return;
	BRDFLut lut;
	// 64x64 at 512 samples: the table is smooth everywhere except the
	// mirror corner, and tools/tests/brdf_lut.cpp measures 64 samples
	// as already within 0.026 of 1024. Generating it costs a few
	// milliseconds once, against shipping a binary blob nobody can
	// review or regenerate.
	if (!lut.Generate(64, 512))
		return;
	// RG32F rather than a packed 8-bit texture: the scale channel runs
	// to 1.0 and the bias to ~0.5, so 8 bits would quantise the
	// grazing-angle rim - the one place the table matters most - into
	// visible steps.
	BRDFLutTex = new Texture();
	BRDFLutTex->CreateEmptyTexture(TextureType::Texture, TextureDataType::RG32F,
		(int32)lut.GetSize(), (int32)lut.GetSize(), false);
	// Clamped, because the table IS the domain: N.V and roughness are
	// both already in [0,1] and a wrapped lookup would return the
	// opposite end of the roughness range.
	// Same rule as the atlases above - see UploadDDGIIfDirty. The table
	// is smooth and 64x64, so nearest costs it very little.
	{
		// The same decision the atlases take - see FloatTextureFilter.
		const uint32 f = FloatTextureFilter();
		BRDFLutTex->SetMinMagFilter(f, f);
	}
	BRDFLutTex->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	BRDFLutTex->UpdateData((void*)lut.GetData().data());
}

void IRenderer::SetAmbientProbeGrid(const IrradianceProbeGrid *Grid)
{
	// Only accept a grid that can actually be sampled. A half-allocated
	// one would be rejected per object by Sample() anyway, silently, on
	// every draw.
	AmbientProbeGrid = (Grid != NULL && Grid->IsValid()) ? Grid : NULL;
}

void IRenderer::SetAmbientSH(const SphericalHarmonicsL2 &SH)
{
	if (GameObject::DrawCopies())
	{
		for (uint32 i = 0; i < SphericalHarmonicsL2::kCoefficientCount && i < 9; i++) { const Vec3 &c = SH.coefficients[i]; heldAmbient.SH[i] = Vec4(c.x, c.y, c.z, 0.f); }
		heldAmbient.sh = true;
		return;
	}
	for (uint32 i = 0; i < SphericalHarmonicsL2::kCoefficientCount; i++)
	{
		const Vec3 &c = SH.coefficients[i];
		AmbientSH[i] = Vec4(c.x, c.y, c.z, 0.f);
	}
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
	if (device) device->SetClearColor(BackgroundOverrideSet ? BackgroundOverride : BackgroundColor);
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

namespace { f32 g_smallObjectPixels = 1.f; }
void IRenderer::SetSmallObjectCull(const f32 pixels) { g_smallObjectPixels = pixels < 0.f ? 0.f : pixels; }
f32 IRenderer::GetSmallObjectCull() { return g_smallObjectPixels; }

bool IRenderer::TooSmallToSee(GameObject* owner) const
{
	if (g_smallObjectPixels <= 0.f || smallCullScale <= 0.f || owner == NULL) return false;
	const f32 r = owner->GetBoundingSphereRadiusWorldSpace();
	if (r <= 0.f) return false;
	const Vec3 c = owner->GetWorldTransformation() * owner->GetBoundingSphereCenter();
	// radius over distance, against the size of the limit at one unit away
	const f32 limit = g_smallObjectPixels * smallCullFactor / smallCullScale;
	return r * r < limit * limit * smallCullEye.distanceSQR(c);
}

bool IRenderer::CullingSphereTest(RenderingMesh* rmesh, GameObject* owner)
{
	if (!IsCulling || !culling) return true;
	// The sphere's own centre, not the object's origin: the two only agree
	// for geometry built around its origin. A terrain tile's origin is its
	// corner, and a sphere there leaves the far half of the tile outside it -
	// the tile vanished whenever that corner left the screen.
	// (Worked out once for both questions - too small to see, and in the
	// view: it was a matrix times a point for each, on every mesh, for every
	// view and cascade.)
	const Matrix &world = owner->GetDrawWorld();
	const Vec3 &local = owner->GetBoundingSphereCenter();
	const Vec3 c(world.m[0] * local.x + world.m[4] * local.y + world.m[8] * local.z + world.m[12],
		world.m[1] * local.x + world.m[5] * local.y + world.m[9] * local.z + world.m[13],
		world.m[2] * local.x + world.m[6] * local.y + world.m[10] * local.z + world.m[14]);
	const f32 r = owner->GetBoundingSphereRadiusWorldSpace();
	if (g_smallObjectPixels > 0.f && smallCullScale > 0.f && r > 0.f)
	{
		const f32 limit = g_smallObjectPixels * smallCullFactor / smallCullScale;
		if (r * r < limit * limit * smallCullEye.distanceSQR(c)) return false;
	}
	return culling->SphereInFrustum(c, r);
}

bool IRenderer::CullingBoxTest(RenderingMesh* rmesh, GameObject* owner)
{
	if (!IsCulling || !culling) return true;
	if (TooSmallToSee(owner)) return false;
	AABox aabb = AABox(owner->GetBoundingMinValueWorldSpace(), owner->GetBoundingMaxValueWorldSpace());

	// Return test
	return culling->ABoxInFrustum(aabb);
}

// Deliberately the light's own radius, not a bounding sphere: `radius` is
// where Attenuation() reaches zero (secondpassPoint.glsl / secondpassSpot.glsl
// both clamp `1 - d/Radius`), so nothing outside it is lit by definition. A
// spot is treated as a sphere rather than a cone - conservative, and the cone
// test is not worth the complexity for a handful of spots.
bool IRenderer::LightAffectsView(const Vec3 &worldPosition, const f32 radius)
{
	if (!IsCulling || !culling) return true;
	if (radius <= 0.f) return true;
	// (and one that lights a patch of under a pixel lights nothing that shows:
	// a lamp turned down to nothing for the day, a lantern a mile off)
	if (smallCullScale > 0.f)
	{
		const f32 limit = 0.75f / smallCullScale;
		if (radius * radius < limit * limit * smallCullEye.distanceSQR(worldPosition)) return false;
	}
	return culling->SphereInFrustum(worldPosition, radius);
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
	// (the cells of the list's grid, for this view: see cullCell)
	cullCellOut.resize(cullCellSphere.size());
	for (size_t c = 0; c < cullCellSphere.size(); c++)
	{
		const Vec4 &s = cullCellSphere[c];
		cullCellOut[c] = culling->SphereInFrustum(Vec3(s.x, s.y, s.z), s.w) ? 0 : 1;
	}
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
			// above), so glBufferSubData's pipeline-stall risk applies here
			// too.
			//
			// Always the FULL block, never just the lights in use, exactly as
			// the BoneMatrices upload below already does and for a harder
			// reason. ReplaceUniformBuffer is glBufferData, which reallocates:
			// one light shrank this buffer from mat4[4] (256 bytes) to 64. The
			// trailing slots really are never read - the shader loop is gated
			// by uNumberOfLights - but WebGL2 does not care what the shader
			// reads. It validates every bound uniform buffer against the
			// block's full std140 size at EVERY draw and drops the draw with
			// GL_INVALID_OPERATION when it is short. Measured 2026-09-05 in
			// the browser editor: every lit mesh silently vanished (Forward
			// and Deferred alike, the G-buffer came back all black), while
			// grid lines and ImGui - which declare no LightsBlock - kept
			// drawing. Desktop GL validates none of this, which is why it only
			// ever showed up on the web.
			Matrix lightsUpload[PYROS_MAX_LIGHTS]; // default ctor = identity pad
			memcpy(lightsUpload, &Lights[0], sizeof(Matrix) * lightsToUpload);
			device->ReplaceUniformBuffer(LightsUBO, sizeof(Matrix) * PYROS_MAX_LIGHTS, lightsUpload);
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
		{
			// std140: five consecutive vec4s, matching PyrosShader.glsl's
			// AmbientLightUniforms block exactly - flat colour, the three
			// gradient bands, then params.x = mode.
			Vec4 env[14];
			env[0] = ScaleAmbient(GlobalLight);
			env[1] = ScaleAmbient(AmbientSky);
			env[2] = ScaleAmbient(AmbientEquator);
			env[3] = ScaleAmbient(AmbientGround);
			env[4] = Vec4((f32)EffectiveAmbientMode(), 0.f, 0.f, 0.f);
			// Always uploaded, not just in mode 2. The block is one
			// contiguous std140 allocation and ReplaceUniformBuffer
			// re-specifies the whole thing (see its comment on
			// orphaning), so writing only the first five would leave the
			// SH coefficients undefined rather than merely unused - and
			// they would then be read as garbage the moment anything
			// switched to mode 2.
			for (uint32 i = 0; i < 9; i++)
				env[5 + i] = AmbientSH[i];
			if (!AmbientLightUniformsUBOValid || memcmp(CachedAmbientEnv, env, sizeof(env)) != 0)
			{
				device->ReplaceUniformBuffer(AmbientLightUniformsUBO, sizeof(env), env);
				memcpy(CachedAmbientEnv, env, sizeof(env));
				CachedGlobalLight = GlobalLight;
				AmbientLightUniformsUBOValid = true;
			}
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

	std::vector<int32>* _ShadersGlobalCache = NULL;
	{ MeshCaches held(rmesh); _ShadersGlobalCache = &rmesh->ShadersGlobalCache[Material->GetShader()]; }

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
				{ Vec4 v = ScaleAmbient(GlobalLight); Shader::SendUniform((*k), &v, (*_ShadersGlobalCache)[counter]); }
				break;
			case Uniforms::DataUsage::AmbientSky:
				{ Vec4 v = ScaleAmbient(AmbientSky); Shader::SendUniform((*k), &v, (*_ShadersGlobalCache)[counter]); }
				break;
			case Uniforms::DataUsage::AmbientEquator:
				{ Vec4 v = ScaleAmbient(AmbientEquator); Shader::SendUniform((*k), &v, (*_ShadersGlobalCache)[counter]); }
				break;
			case Uniforms::DataUsage::AmbientGround:
				{ Vec4 v = ScaleAmbient(AmbientGround); Shader::SendUniform((*k), &v, (*_ShadersGlobalCache)[counter]); }
				break;
			case Uniforms::DataUsage::AmbientParams:
			{
				Vec4 params((f32)EffectiveAmbientMode(), 0.f, 0.f, 0.f);
				Shader::SendUniform((*k), &params, (*_ShadersGlobalCache)[counter]);
			}
			break;
			case Uniforms::DataUsage::AmbientSH:
				Shader::SendUniform((*k), &AmbientSH[0], (*_ShadersGlobalCache)[counter], 9);
				break;
			case Uniforms::DataUsage::ShaderGlobals:
				Shader::SendUniform((*k), &ShaderGlobals[0], (*_ShadersGlobalCache)[counter], kShaderGlobals);
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
			case Uniforms::DataUsage::BRDFLutMap:
			{
				// Scene-independent: the same table for every volume,
				// every material and every frame. Built once.
				BuildBRDFLutIfNeeded();
				int32 unit = 0;
				if (BRDFLutTex != NULL)
				{
					BRDFLutTex->Bind();
					unit = (int32)Texture::GetLastBindedUnit();
					BoundGITextures.push_back(BRDFLutTex);
				}
				Shader::SendUniform((*k), &unit, (*_ShadersGlobalCache)[counter], 1);
				break;
			}
			case Uniforms::DataUsage::DDGIIrradianceMap:
			case Uniforms::DataUsage::DDGIVisibilityMap:
			case Uniforms::DataUsage::DDGIRadianceMap:
			case Uniforms::DataUsage::DDGIProbeDataMap:
			{
				// Uploaded lazily and only when the volume says it
				// changed - see UploadDDGIIfDirty. Binding happens here
				// rather than in BindShadowMaps because the unit has to
				// be whatever Texture::Bind just handed out, and that is
				// only known after the bind.
				UploadDDGIIfDirty();
				Texture *tex = NULL;
				switch ((*k).Usage)
				{
					case Uniforms::DataUsage::DDGIIrradianceMap: tex = DDGIIrradianceTex; break;
					case Uniforms::DataUsage::DDGIRadianceMap:   tex = DDGIRadianceTex;   break;
					case Uniforms::DataUsage::DDGIProbeDataMap:  tex = DDGIProbeDataTex;  break;
					default:                                     tex = DDGIVisibilityTex; break;
				}
				int32 unit = 0;
				if (tex != NULL)
				{
					tex->Bind();
					unit = (int32)Texture::GetLastBindedUnit();
					BoundGITextures.push_back(tex);
				}
				Shader::SendUniform((*k), &unit, (*_ShadersGlobalCache)[counter], 1);
				break;
			}
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
			{
				// All four slots, always - see PadSamplerUnits. 14 is the
				// spare, distinct from the directional one above.
				std::vector<int32> units;
				PadSamplerUnits(units, PointShadowMapsUnits, 14, PYROS_SHADOW_SAMPLER_SLOTS);
				Shader::SendUniform((*k), &units[0], (*_ShadersGlobalCache)[counter], (uint32)units.size());
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
			{
				// Was &SpotShadowMapsUnits[0] unguarded, unlike its
				// Directional/Point neighbours - indexing an empty vector.
				// All four slots, always - see PadSamplerUnits. 15 is the
				// spare, distinct from the two above.
				std::vector<int32> units;
				PadSamplerUnits(units, SpotShadowMapsUnits, 15, PYROS_SHADOW_SAMPLER_SLOTS);
				Shader::SendUniform((*k), &units[0], (*_ShadersGlobalCache)[counter], (uint32)units.size());
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
		// One walk down the material's values, each told by its second letter and
		// then by its name: this was eleven walks, each making a string of the name
		// looked for and comparing it with every value's - for every draw that
		// changed material.
		{
			static const std::string kColor("uColor"), kSpecular("uSpecular"), kOpacity("uOpacity"), kShininess("uShininess"),
				kUseLights("uUseLights"), kDisplacement("uDisplacementHeight"), kReflectivity("uReflectivity"), kMetallic("uMetallic"),
				kRoughness("uRoughness"), kSSR("uSSRReflective"), kAlphaCutoff("uAlphaCutoff");
			for (std::list<Uniform>::const_iterator it = Material->UserUniforms.begin(); it != Material->UserUniforms.end(); ++it)
			{
				const std::string &n = it->Name;
				if (n.size() < 6 || it->Value.empty()) continue;
				const uchar* v = &it->Value[0];
				switch (n[1])
				{
				case 'C': if (n == kColor && it->Value.size() >= sizeof(Vec4)) memcpy(&data.Color, v, sizeof(Vec4)); break;
				case 'S':
					if (n == kSpecular) { if (it->Value.size() >= sizeof(Vec4)) memcpy(&data.Specular, v, sizeof(Vec4)); }
					else if (n == kShininess) memcpy(&data.Shininess, v, sizeof(f32));
					else if (n == kSSR) memcpy(&data.SSRReflective, v, sizeof(f32));
					break;
				case 'O': if (n == kOpacity) memcpy(&data.Opacity, v, sizeof(f32)); break;
				case 'U': if (n == kUseLights) memcpy(&data.UseLights, v, sizeof(f32)); break;
				case 'D': if (n == kDisplacement) memcpy(&data.DisplacementHeight, v, sizeof(f32)); break;
				case 'R':
					if (n == kReflectivity) memcpy(&data.Reflectivity, v, sizeof(f32));
					else if (n == kRoughness) memcpy(&data.Roughness, v, sizeof(f32));
					break;
				case 'M': if (n == kMetallic) memcpy(&data.Metallic, v, sizeof(f32)); break;
				case 'A': if (n == kAlphaCutoff) memcpy(&data.AlphaCutoff, v, sizeof(f32)); break;
				default: break;
				}
			}
		}
		device->ReplaceUniformBuffer(MaterialUniformsUBO, sizeof(MaterialUniformsData), &data);
	}

	std::vector<int32>* _ShadersUserCache = NULL;
	{ MeshCaches held(rmesh); _ShadersUserCache = &rmesh->ShadersUserCache[Material->GetShader()]; }

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
		struct { Matrix model; Vec4 wind; Vec4 growth; } objectMatrixData;
		objectMatrixData.model = ModelMatrix;
		objectMatrixData.wind = Vec4(0.f, 0.f, 0.f, 0.f);
		// (an instanced component's growth of its instances: see
		// IRenderingInstancedComponent::SetInstanceGrowth)
		objectMatrixData.growth = Vec4(1.f, 1.f, 0.f, 0.f);
		if (rmesh != NULL && rmesh->renderingComponent != NULL && rmesh->renderingComponent->IsInstanced())
		{
			const Vec2 &growth = static_cast<IRenderingInstancedComponent*>(rmesh->renderingComponent)->GrowthToDraw();
			objectMatrixData.growth = Vec4(growth.x, growth.y, 0.f, 0.f);
		}
		{
			GenericShaderMaterial* genericMat = dynamic_cast<GenericShaderMaterial*>(Material);
			if (genericMat != NULL && (genericMat->GetOptions() & ShaderUsage::VertexWind))
			{
				objectMatrixData.wind = genericMat->GetWind();
				WindInUseThisFrame = true;
			}
		}
		device->ReplaceUniformBuffer(ObjectMatrixUniformsUBO, sizeof(objectMatrixData), &objectMatrixData);

		// Per-object probe lookup. Only when a grid is published and the
		// scene is actually in SH mode - otherwise this is an extra UBO
		// write per object for a block nothing reads differently, and
		// ObjectMatrixUniforms above is already the measured-expensive
		// one (see ReplaceUniformBuffer's comment on orphaning).
		if (AmbientProbeGrid != NULL && AmbientMode == 2)
		{
			// The model matrix's translation IS the object's world
			// position, and it is already in hand here - SendModelUniforms
			// has no GameObject, and reaching for one would mean
			// threading it through purely to read back a value this
			// matrix was built from.
			const SphericalHarmonicsL2 sampled = AmbientProbeGrid->Sample(ModelMatrix.GetTranslation());
			// Same 14-vec4 layout as SendGlobalUniforms writes - the
			// whole block, because ReplaceUniformBuffer re-specifies the
			// entire allocation and writing only the tail would leave the
			// flat colour and gradient undefined.
			Vec4 env[14];
			env[0] = ScaleAmbient(GlobalLight);
			env[1] = ScaleAmbient(AmbientSky);
			env[2] = ScaleAmbient(AmbientEquator);
			env[3] = ScaleAmbient(AmbientGround);
			env[4] = Vec4((f32)EffectiveAmbientMode(), 0.f, 0.f, 0.f);
			for (uint32 i = 0; i < 9; i++)
			{
				const Vec3 &c = sampled.coefficients[i];
				env[5 + i] = Vec4(c.x, c.y, c.z, 0.f);
			}
			device->ReplaceUniformBuffer(AmbientLightUniformsUBO, sizeof(env), env);
			// The per-frame cache in SendGlobalUniforms no longer
			// describes what is in the buffer, so make it re-upload next
			// frame rather than skip on a stale comparison.
			AmbientLightUniformsUBOValid = false;
		}
		if (rmesh->BonesToDraw().size() > 0)
		{
			// Always upload the full UBO size. ReplaceUniformBuffer →
			// glBufferData with a shorter size orphans storage smaller than
			// the shader's mat4 uBoneMatrix[MAX_BONES] block; on macOS GL
			// that left the binding unloadable / zeros, so skinned meshes
			// stayed in bind pose ("no animation").
			// (into a shadow map: the pose kept for shadows, where there is one)
			const std::vector<Matrix> &palette = (!rmesh->ShadowBonesToDraw().empty() && IsShadowMaterial(Material)) ? rmesh->ShadowBonesToDraw() : rmesh->BonesToDraw();
			uint32 bonesToUpload = palette.size() < PYROS_MAX_BONES ? (uint32)palette.size() : PYROS_MAX_BONES;
			Matrix boneUpload[PYROS_MAX_BONES]; // default-ctor = identity pad past bonesToUpload
			memcpy(boneUpload, &palette[0], sizeof(Matrix) * bonesToUpload);
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
		ObjectLightCountsData lightCounts = ObjectLightCountsData();
		lightCounts.NumberOfLights = (int32)NumberOfLights;
		lightCounts.NumberOfPointShadows = (int32)NumberOfPointShadows;
		lightCounts.NumberOfSpotShadows = (int32)NumberOfSpotShadows;
		// (sent when it changes: in most passes it is the same three numbers for
		// every thing drawn, and it was sent again with each)
		if (!LightCountsUBOValid || memcmp(&CachedLightCounts, &lightCounts, sizeof(lightCounts)) != 0)
		{
			device->ReplaceUniformBuffer(ObjectLightCountsUBO, sizeof(ObjectLightCountsData), &lightCounts);
			memcpy(&CachedLightCounts, &lightCounts, sizeof(lightCounts));
			LightCountsUBOValid = true;
		}
	}

	uint32 counter = 0;

	std::vector<int32>* _ShadersModelCache = NULL;
	{ MeshCaches held(rmesh); _ShadersModelCache = &rmesh->ShadersModelCache[Material->GetShader()]; }

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
				if (rmesh->BonesToDraw().size() > 0)
				{
					const std::vector<Matrix> &palette = (!rmesh->ShadowBonesToDraw().empty() && IsShadowMaterial(Material)) ? rmesh->ShadowBonesToDraw() : rmesh->BonesToDraw();
					Shader::SendUniform((*k), (void*)&palette[0], (*_ShadersModelCache)[counter], (uint32)palette.size());
				}
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

// Where a material's block is filled before it is sent. The block's own memory -
// except on a thread recording a pass beside the frame: two of those may be
// drawing two things that wear the same material at the same moment, and each
// fills a copy of its own (every value the block has is written for every draw).
namespace {
	template <class Block> std::vector<uchar> &ExtraScratch(Block &block, const int index)
	{
		if (!t_recordingBeside) return block.scratch;
		static thread_local std::vector<uchar> mine[2];
		if (mine[index].size() != block.scratch.size()) mine[index].assign(block.scratch.size(), 0);
		return mine[index];
	}
}

void IRenderer::CaptureExtraUniform(IMaterial* Material, const Uniform &u, RenderingMesh* rmesh)
{
	Vec4 ambientParams, scaledAmbient;
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
		scaledAmbient = ScaleAmbient(GlobalLight); valuePtr = &scaledAmbient; valueSize = sizeof(Vec4);
		break;
	case Uniforms::DataUsage::AmbientSky:
		scaledAmbient = ScaleAmbient(AmbientSky); valuePtr = &scaledAmbient; valueSize = sizeof(Vec4);
		break;
	case Uniforms::DataUsage::AmbientEquator:
		scaledAmbient = ScaleAmbient(AmbientEquator); valuePtr = &scaledAmbient; valueSize = sizeof(Vec4);
		break;
	case Uniforms::DataUsage::AmbientGround:
		scaledAmbient = ScaleAmbient(AmbientGround); valuePtr = &scaledAmbient; valueSize = sizeof(Vec4);
		break;
	case Uniforms::DataUsage::AmbientParams:
		ambientParams = Vec4((f32)EffectiveAmbientMode(), 0.f, 0.f, 0.f);
		valuePtr = &ambientParams; valueSize = sizeof(Vec4);
		break;
	case Uniforms::DataUsage::AmbientSH:
		valuePtr = &AmbientSH[0]; valueSize = sizeof(Vec4) * 9;
		break;
	case Uniforms::DataUsage::ShaderGlobals:
		valuePtr = &ShaderGlobals[0]; valueSize = sizeof(Vec4) * kShaderGlobals;
		break;
	// Bone palette for a loose `uniform mat4 uBoneMatrix[]` - the Material
	// Editor's generated shaders. GenericShaderMaterial's bones go through
	// the BoneMatrices UBO instead, so nothing needed this until now, and
	// without it a skinned custom material stayed in bind pose on every
	// SPIR-V backend while GL (SendModelUniforms' own case) animated.
	// Clamped to the shader's array: the scratch-bounds check below drops
	// a write that would overrun it entirely rather than truncating it.
	case Uniforms::DataUsage::Skinning:
		if (rmesh != NULL && rmesh->BonesToDraw().size() > 0)
		{
			const std::vector<Matrix> &drawn = rmesh->BonesToDraw();
			valuePtr = &drawn[0];
			valueSize = (uint32)sizeof(Matrix) * (drawn.size() < PYROS_MAX_BONES ? (uint32)drawn.size() : PYROS_MAX_BONES);
		}
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
	// The shadow matrices, as SendGlobalUniforms() sends them. Missing
	// until now, so on every SPIR-V backend a CustomShaderMaterial's
	// uDirectionalDepthsMVP/uPointDepthsMVP/uSpotDepthsMVP stayed zero: every
	// fragment projected to the shadow map's origin, compared as lit, and a
	// Material Editor material received no shadows at all - while the
	// counts, splits and samplers above all arrived, which made it look
	// like a sampling bug. GL was unaffected (its loose uniforms go through
	// SendGlobalUniforms). Capped at what the generated shader declares:
	// 4 cascades, 2 matrices per point light, one per spot light.
	case Uniforms::DataUsage::DirectionalShadowMatrix:
		if (!DirectionalShadowMatrix.empty())
		{
			valuePtr = &DirectionalShadowMatrix[0];
			valueSize = (uint32)sizeof(Matrix) * (uint32)std::min<size_t>(DirectionalShadowMatrix.size(), 4);
		}
		break;
	case Uniforms::DataUsage::PointShadowMatrix:
		if (!PointShadowMatrix.empty())
		{
			valuePtr = &PointShadowMatrix[0];
			valueSize = (uint32)sizeof(Matrix) * (uint32)std::min<size_t>(PointShadowMatrix.size(), 2 * PYROS_SHADOW_SAMPLER_SLOTS);
		}
		break;
	case Uniforms::DataUsage::SpotShadowMatrix:
		if (!SpotShadowMatrix.empty())
		{
			valuePtr = &SpotShadowMatrix[0];
			valueSize = (uint32)sizeof(Matrix) * (uint32)std::min<size_t>(SpotShadowMatrix.size(), PYROS_SHADOW_SAMPLER_SLOTS);
		}
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
		IMaterial::ExtraUniformsBlock &block = Material->ExtraBlock(i);
		if (block.binding == 0)
			continue;
		std::map<std::string, uint32>::const_iterator offIt = block.offsets.find(u.Name);
		std::vector<uchar> &scratch = ExtraScratch(block, i);
		if (offIt != block.offsets.end() && offIt->second + valueSize <= scratch.size())
			memcpy(&scratch[offIt->second], valuePtr, valueSize);
	}
}

void IRenderer::SendExtraUniforms(RenderingMesh* rmesh, IMaterial* Material)
{
	if (Material->ExtraBlock(0).binding == 0 && Material->ExtraBlock(1).binding == 0)
		return;

	for (std::list<Uniform>::const_iterator k = Material->GlobalUniforms.begin(); k != Material->GlobalUniforms.end(); k++)
		CaptureExtraUniform(Material, *k, rmesh);
	for (std::list<Uniform>::const_iterator k = Material->UserUniforms.begin(); k != Material->UserUniforms.end(); k++)
		CaptureExtraUniform(Material, *k, rmesh);
	for (std::list<Uniform>::const_iterator k = Material->ModelUniforms.begin(); k != Material->ModelUniforms.end(); k++)
		CaptureExtraUniform(Material, *k, rmesh);

	for (int i = 0; i < 2; i++)
	{
		IMaterial::ExtraUniformsBlock &block = Material->ExtraBlock(i);
		if (block.binding == 0)
			continue;
		// (made once, by whichever thread gets here first)
		static std::mutex making;
		std::unique_lock<std::mutex> made(making, std::defer_lock);
		if (block.bufferHandle == 0) made.lock();
		if (block.bufferHandle == 0)
		{
			// A custom material's ring is its program's (see
			// CustomShaderMaterial::SharedExtraBuffer); its subclasses fill
			// their blocks by hand and keep their own.
			if (typeid(*Material) == typeid(CustomShaderMaterial))
				block.bufferHandle = CustomShaderMaterial::SharedExtraBuffer(Material->GetShader(), block);
			else
				block.bufferHandle = device->CreateUniformBuffer(block.size, block.binding);
		}
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
		device->ReplaceUniformBuffer(block.bufferHandle, block.size, &ExtraScratch(block, i)[0]);
	}
}

void IRenderer::BindMesh(RenderingMesh* rmesh, IMaterial* material)
{
	MeshCaches held(rmesh);         // (nothing in here waits for the device)
	// Drop every cached VAO if the geometry has been given new GPU buffers
	// since they were built. A VAO bakes in the buffer handles it was
	// recorded against, so one built before the rebuild would keep sourcing
	// vertices from the freed buffers while the draw count - read from
	// CPU-side index data - tracks the new geometry. Only the VAOs are
	// invalidated: the shader attribute/uniform location caches alongside
	// them depend on the shader, not on the buffers, and stay valid.
	//
	// The component's own attribute buffers count too (particle and
	// instance streams - see RenderingComponent::ownBuffersRevision).
	// Both counters only ever increase, so their sum changes whenever
	// either does.
	const uint32 buffersRevision = (rmesh->Geometry != NULL ? rmesh->Geometry->buffersRevision : 0)
		+ (rmesh->renderingComponent != NULL ? rmesh->renderingComponent->ownBuffersRevision : 0);
	if (rmesh->VAOCacheRevision != buffersRevision)
	{
		for (std::map<uint32, uint32>::iterator i = rmesh->VAOCache.begin(); i != rmesh->VAOCache.end(); i++)
			device->DeleteVertexArray(i->second);
		rmesh->VAOCache.clear();
		rmesh->VAOCacheRevision = buffersRevision;
	}

	// Everything this draw sources vertex data from: the geometry's own
	// attribute buffers, then any the component owns (the per-instance
	// transform stream, particle streams - see
	// RenderingComponent::ownAttributeBuffers for why those can't live on
	// the shared geometry). Built once here because all three passes below
	// - the attribute-location cache, the VAO, and the pipeline's vertex
	// layout - have to walk the exact same list in the exact same order:
	// _ShadersAttributesCache is indexed positionally.
	// (this thread's list, kept: a list was made and thrown away for every draw)
	static thread_local std::vector<AttributeArray*> meshAttributes;
	meshAttributes.assign(rmesh->Geometry->Attributes.begin(), rmesh->Geometry->Attributes.end());
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
		// IfPresent, so this is a no-op for every material not compiled
		// with ShaderUsage::GlobalIllumination - which is most of them.
		// Without it the block reads as zeros, counts comes back 0, the
		// probe loop never runs, and SampleDDGI returns black. That
		// looks exactly like "the volume did not bake", which is where
		// the first hour of looking went.
		device->BindUniformBlockIfPresent(material->GetShader(), "DDGIUniforms", 25);
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
			// Same Zero/Zero rule as RenderObject()'s blend state.
			if (material->blending && !(material->sfactor == BlendFunc::Zero && material->dfactor == BlendFunc::Zero))
			{
				pdesc.blendSrcFactor = material->sfactor;
				pdesc.blendDstFactor = material->dfactor;
				pdesc.blendEquation = material->mode;
			}
		}
		rmesh->PipelineCache[pipelineKey] = device->CreatePipeline(pdesc);
	}
}

// SetMinMagFilter binds the texture on whichever unit is active and then binds nothing
// there. Called on every shadow-map bind, that unit was the one the material's last
// texture had just been given - so on GL every lit, textured, shadow-receiving mesh drew
// with its colour map (or its normal map) unbound, i.e. black. So: only when the filter
// really is wrong, and then on the unit the shadow map is about to take, which is free.
static void EnsureNearest(Texture* map)
{
	if (map->HasMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest)) return;
	map->Bind();
	map->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
	map->Unbind();
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
			// fix in EnableCastShadows still need it.
			EnsureNearest(*i);
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
			EnsureNearest(*i);
			(*i)->Bind();
			SpotShadowMapsUnits.push_back(Texture::GetLastBindedUnit());
		}
	}
}

void IRenderer::UnbindGITextures()
{
	// In reverse, because Unbind() rewinds Texture::UnitBinded rather
	// than releasing a particular unit - unbinding out of order would
	// leave the counter pointing at a unit that is still bound.
	//
	// Without this the counter climbs by one per GI sampler per draw
	// and never comes back down: the first few objects in a frame get
	// valid units and everything after them is handed a unit past the
	// hardware limit, which samples as black. It looks exactly like the
	// volume not being uploaded, and it is why a metal sphere added
	// late in the scene reflected nothing while the walls lit fine.
	for (std::vector<Texture*>::reverse_iterator i = BoundGITextures.rbegin();
		i != BoundGITextures.rend(); ++i)
		(*i)->Unbind();
	BoundGITextures.clear();
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
