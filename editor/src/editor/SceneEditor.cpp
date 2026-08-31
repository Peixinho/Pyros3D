//============================================================================
// Name        : Scene.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ( ͡° ͜ʖ ͡°)
// Description : Pyros Scene
//============================================================================

#include <cmath>
#include <set>
#include <filesystem>
#include <cstring>
#include <algorithm>

#include "SceneEditor.h"
#include "UIDispatch.h"
#include "UI/EasingPreview.h"
#include "SceneCommands.h"
#include "AssetCommands.h"
#include "UndoValueEdit.h"
#include "ProjectManager.h"
#include "AgentServer.h"
#include "UI/MaterialEditor.h"
#include "libgizmo/GizmoTransformRender.h"
#include <Pyros3D/Core/Logs/Log.h>
#include <chrono>
#include <functional>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/Assets/Renderable/Decals/Decals.h>
#include <cmath>
#ifdef isnan
#undef isnan
#endif
#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/Core/InputManager/InputManager.h>
#include <Pyros3D/Physics/Components/IPhysicsComponent.h>
#include <Pyros3D/Rendering/Components/Layer2D/Layer2D.h>
#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <Pyros3D/Rendering/Components/Occluder2D/Occluder2D.h>
#include <fstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <Pyros3D/Ext/stb/stb_image_write.h>

#define MAX_F32 3.40282e+38

using namespace p3d;
using json = nlohmann::json;

namespace {
struct DragCompPayload {
	uint32 compId;
	uint32 ownerGoId;
};
static const char* kEditorCameraTag = "PyrosEditor.Camera";

// Empty-GO helpers stay visible when the only components are scripts/audio
// (no mesh or light) — otherwise attaching a script hides the icon and it
// looks like selection/attach failed.
bool GameObjectHasVisualComponent(GameObject* go)
{
	if (!go) return false;
	const std::vector<std::shared_ptr<IComponent>>& comps = go->GetComponents();
	for (size_t i = 0; i < comps.size(); ++i)
	{
		IComponent* c = comps[i].get();
		if (!c) continue;
		const uint32 t = c->GetComponentType();
		if (t == ComponentType::RenderingComponent
			|| t == ComponentType::RenderingInstancedComponent
			|| t == ComponentType::ParticleSystem
			|| t == ComponentType::DirectionalLight
			|| t == ComponentType::PointLight
			|| t == ComponentType::SpotLight)
			return true;
	}
	return false;
}

bool GameObjectShowsEmptyHelper(GameObject* go)
{
	return go && !GameObjectHasVisualComponent(go);
}

static float HalfToFloat(uint16_t h)
{
	const uint32_t sign = (uint32_t)(h >> 15) << 31;
	uint32_t exp = (h >> 10) & 0x1Fu;
	uint32_t mant = h & 0x3FFu;
	uint32_t f;
	if (exp == 0)
	{
		if (mant == 0)
			f = sign;
		else
		{
			exp = 127 - 15 + 1;
			while ((mant & 0x400u) == 0) { mant <<= 1; --exp; }
			mant &= 0x3FFu;
			f = sign | (exp << 23) | (mant << 13);
		}
	}
	else if (exp == 31)
		f = sign | 0x7F800000u | (mant << 13);
	else
		f = sign | ((exp + (127 - 15)) << 23) | (mant << 13);
	float out;
	std::memcpy(&out, &f, sizeof(out));
	return out;
}

static uint8_t FloatToByte(float v)
{
	if (v < 0.f) v = 0.f;
	if (v > 1.f) v = 1.f;
	return (uint8_t)(v * 255.f + 0.5f);
}

static bool ConvertPreviewPixelsToRGBA8(const std::vector<uchar>& pixels, uint32 dataType,
	uint32 w, uint32 h, std::vector<unsigned char>& outRGBA)
{
	outRGBA.clear();
	if (w == 0 || h == 0 || pixels.empty()) return false;
	const size_t n = (size_t)w * (size_t)h;
	outRGBA.resize(n * 4);

	if (dataType == TextureDataType::RGBA)
	{
		if (pixels.size() < n * 4) return false;
		std::memcpy(outRGBA.data(), pixels.data(), n * 4);
		return true;
	}
	if (dataType == TextureDataType::RGBA16F)
	{
		if (pixels.size() < n * 8) return false;
		const uint16_t* src = reinterpret_cast<const uint16_t*>(pixels.data());
		for (size_t i = 0; i < n; ++i)
		{
			outRGBA[i * 4 + 0] = FloatToByte(HalfToFloat(src[i * 4 + 0]));
			outRGBA[i * 4 + 1] = FloatToByte(HalfToFloat(src[i * 4 + 1]));
			outRGBA[i * 4 + 2] = FloatToByte(HalfToFloat(src[i * 4 + 2]));
			outRGBA[i * 4 + 3] = FloatToByte(HalfToFloat(src[i * 4 + 3]));
		}
		return true;
	}
	if (dataType == TextureDataType::RGBA32F)
	{
		if (pixels.size() < n * 16) return false;
		const float* src = reinterpret_cast<const float*>(pixels.data());
		for (size_t i = 0; i < n; ++i)
		{
			outRGBA[i * 4 + 0] = FloatToByte(src[i * 4 + 0]);
			outRGBA[i * 4 + 1] = FloatToByte(src[i * 4 + 1]);
			outRGBA[i * 4 + 2] = FloatToByte(src[i * 4 + 2]);
			outRGBA[i * 4 + 3] = FloatToByte(src[i * 4 + 3]);
		}
		return true;
	}
	return false;
}

static void FlipRGBA8Vertically(std::vector<unsigned char>& rgba, uint32 w, uint32 h)
{
	if (w == 0 || h == 0 || rgba.size() < (size_t)w * h * 4) return;
	const size_t rowBytes = (size_t)w * 4;
	std::vector<unsigned char> tmp(rowBytes);
	for (uint32 y = 0; y < h / 2; ++y)
	{
		unsigned char* a = rgba.data() + (size_t)y * rowBytes;
		unsigned char* b = rgba.data() + (size_t)(h - 1 - y) * rowBytes;
		std::memcpy(tmp.data(), a, rowBytes);
		std::memcpy(a, b, rowBytes);
		std::memcpy(b, tmp.data(), rowBytes);
	}
}
}

	SceneEditor::SceneEditor(uint32 documentId)
	{
		this->documentId = documentId;
		shutDownDone = false;
		_scale = Vec3(1, 1, 1);
		showDir = false;
		showingSceneDialog = false;
		sceneDialogIsSave = false;
		sceneDialogBrowse = false;
		sceneDirty = false;
		pendingUnsavedAction = UnsavedNone;
		showUnsavedModal = false;
		awaitingSaveDialog = false;
		hostCloseProject = NULL;
		hostQuitApp = NULL;
		hostQuitDiscardingUnsaved = NULL;
		hostNewProject = NULL;
		hostOpenProject = NULL;
		hostActivateDocument = NULL;
		hostRequestCloseDocument = NULL;
		hostNewSceneDocument = NULL;
		hostNewSceneKind = NULL;
		hostOpenSceneDocument = NULL;
		hostOpenLuaScript = NULL;
		hostEditMaterialInline = NULL;
		hostAssignMaterialAsset = NULL;
		previewRenderer = NULL;
		previewEffects = NULL;
		thumbRenderer = NULL;
		thumbEffects = NULL;
		Renderer = NULL;
		uiRenderer = NULL;
		usingDeferredRenderer = false;
		pendingUseDeferredRenderer = false;
		gbufferDepth = gbufferAlbedo = gbufferSpecular = gbufferNormal = gbufferMatRough = NULL;
		gbufferFBO = NULL;
		assetSoundPreview = NULL;
		sharedAudioManager = NULL;
		lastListenerTime = -1.0;

		// The Add form's fields are only ever written by its ImGui widgets,
		// and DragFloat/DragInt clamp to their min/max only once the user
		// actually edits them - open the form and press Add without touching
		// anything and whatever garbage these held went straight into the
		// primitive constructors. An uninitialized segment count is enough to
		// throw std::length_error out of the geometry's vertex vector and
		// terminate the editor.
		AddForm_w = AddForm_h = AddForm_d = 1.f;
		AddForm_p = 2.f;
		AddForm_q = 3.f;
		AddForm_oc = 45.f;
		AddForm_ic = 30.f;
		AddForm_sw = AddForm_sh = 16;
		AddForm_r = 8;
		AddForm_hscale = 1;
		AddForm_sn = true;
		AddForm_fn = false;
		AddForm_cgo = true;
		AddForm_hs = false;
		AddForm_oe = false;
		AddForm_cs = false;
		AddForm_dir = Vec3(0.f, -1.f, 0.f);
		AddForm_color = Vec4(1.f, 1.f, 1.f, 1.f);
		// What the Cast Shadows checkbox used to hardcode; now just the
		// starting value of an editable property.
		PropertiesShadowBiasFactor = 5.f;
		PropertiesShadowBiasUnits = 3.f;
		PropertiesShadowMapSize = 2048;
		PropertiesShadowNear = 0.01f;
		PropertiesShadowFar = 50.f;
		PropertiesShadowCascades = 1;
		activeSceneCameraId = 0;
		playModeSavedCameraId = 0;
		viewportOverlayValid = false;
		playMode = false;
		audio = NULL;
		project = NULL;
#ifdef LUA_BINDINGS
		sharedLua = NULL;
		sceneMainScriptPath.clear();
		sceneMainScript.reset();
		openNewGoScriptModal = false;
		hierarchyForceOpenId = 0;
#endif
		sceneRootSelected = false;
		viewportMouseValid = false;
		viewportHovered = false;
		viewportInputAllowed = false;
	}

	void SceneEditor::OnResize(const uint32 width, const uint32 height)
	{
		Width = width;
		Height = height;

		// Resize
		if (usingDeferredRenderer && gbufferFBO)
		{
			gbufferDepth->Resize(width, height);
			gbufferAlbedo->Resize(width, height);
			gbufferSpecular->Resize(width, height);
			gbufferNormal->Resize(width, height);
			gbufferMatRough->Resize(width, height);
			gbufferFBO->Resize(width, height);
		}
		Renderer->Resize(width, height);
		Renderer->SetViewPort(0, 0, width, height);
	}

	// G-buffer for DeferredRenderer, matching the shared demo render stack's
	// own setup (examples/assets/demos/scripts/render_host.lua) exactly:
	// depth + 4 color attachments (albedo/specular/normal/metallic-roughness).
	void SceneEditor::BuildGBuffer(uint32 width, uint32 height)
	{
		gbufferDepth = new Texture();
		gbufferDepth->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, width, height, false);
		gbufferDepth->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		gbufferAlbedo = new Texture();
		// RGBA16F, not RGBA8. These two carry the additive ambient+emissive
		// term in their alpha channels (see secondpassAmbient.glsl), and an
		// 8-bit UNORM alpha clamps that at 1.0 - which silently flattened
		// the brightest parts of any emissive material. The normal target
		// was already float for the same kind of reason.
		gbufferAlbedo->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, width, height, false);
		gbufferAlbedo->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		gbufferSpecular = new Texture();
		// RGBA16F, not RGBA8. These two carry the additive ambient+emissive
		// term in their alpha channels (see secondpassAmbient.glsl), and an
		// 8-bit UNORM alpha clamps that at 1.0 - which silently flattened
		// the brightest parts of any emissive material. The normal target
		// was already float for the same kind of reason.
		gbufferSpecular->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, width, height, false);
		gbufferSpecular->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		gbufferNormal = new Texture();
		gbufferNormal->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, width, height, false);
		gbufferNormal->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		gbufferMatRough = new Texture();
		gbufferMatRough->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, width, height, false);
		gbufferMatRough->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		gbufferFBO = new FrameBuffer();
		gbufferFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, gbufferDepth);
		gbufferFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, gbufferAlbedo);
		gbufferFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment1, TextureType::Texture, gbufferSpecular);
		gbufferFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment2, TextureType::Texture, gbufferNormal);
		gbufferFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment3, TextureType::Texture, gbufferMatRough);
	}

	void SceneEditor::DestroyGBuffer()
	{
		delete gbufferFBO; gbufferFBO = NULL;
		delete gbufferDepth; gbufferDepth = NULL;
		delete gbufferAlbedo; gbufferAlbedo = NULL;
		delete gbufferSpecular; gbufferSpecular = NULL;
		delete gbufferNormal; gbufferNormal = NULL;
		delete gbufferMatRough; gbufferMatRough = NULL;
	}

	void SceneEditor::SwitchRenderer(bool useDeferred)
	{
		// Deferred to the start of ShowViewport() (see
		// ApplyPendingRendererSwitchIfAny()), not executed here - this is
		// called from a UI callback (Project Settings' Apply button, or
		// the "set_renderer" AgentServer command), which can fire mid-frame
		// while THIS frame's own render commands are still being recorded
		// with an open, unended render pass (worse the heavier the active
		// material's shader - a lit Diffuse/PBR object's G-buffer+shadow+
		// lighting passes stay "in flight" longer into the frame than a
		// trivial unlit Color one does). WaitIdle()'ing straight into that
        // is a real Vulkan deadlock, not just a stall - reproduced as a
		// hang that only happened with a lit object in the scene. Applying
		// at the next ShowViewport() start guarantees no command buffer
		// from a previous frame is still open.
		// A 2D scene is always forward, whatever the project is set to.
		// Deferred exists to make many lights cheap by shading from a
		// G-buffer, and a G-buffer holds one opaque fragment per pixel - so
		// it cannot blend, which is the one thing sprites need. A cut-out
		// sprite rendered through it writes its fully transparent texels at
		// full opacity, giving every sprite a hard ring of whatever colour
		// its invisible border happens to be. There is nothing to gain in
		// exchange: 2D lighting is a handful of lights on flat quads, which
		// is exactly what forward is good at.
		if (sceneIsTwoD)
			useDeferred = false;
		if (useDeferred == usingDeferredRenderer)
			return;
		queuedRendererSwitch = true;
		queuedUseDeferredRenderer = useDeferred;
	}

	void SceneEditor::ApplyPendingRendererSwitchIfAny()
	{
		if (!queuedRendererSwitch)
			return;
		queuedRendererSwitch = false;
		const bool useDeferred = queuedUseDeferredRenderer;
		if (useDeferred == usingDeferredRenderer)
			return;

		// A frame can still be in flight referencing the old Renderer's
		// pipelines/descriptors/FBOs the moment this runs (this is a live,
		// mid-session toggle, not app startup/shutdown) - WaitIdle first,
		// same reasoning as DeferredRenderer::Resize()'s leading WaitIdle.
		GetActiveRenderDevice().WaitIdle();

		delete EffectsManager;
		EffectsManager = NULL;
		delete Renderer;
		Renderer = NULL;
		if (usingDeferredRenderer)
			DestroyGBuffer();

		usingDeferredRenderer = useDeferred;
		if (usingDeferredRenderer)
		{
			BuildGBuffer(Width, Height);
			DeferredRenderer* deferred = new DeferredRenderer(Width, Height, gbufferFBO);
			// The viewport reads this renderer's output through
			// GetColorTexture() and composites it into an ImGui::Image, so
			// RenderScene()'s extra "Render to Screen" pass has no consumer -
			// it just blits the bare scene over framebuffer 0 every frame,
			// racing whatever else targets the drawable. See
			// SetSkipRenderToScreen()'s comment; MaterialPreview already
			// does this and SceneEditor was the other caller it names.
			deferred->SetSkipRenderToScreen(true);
			Renderer = deferred;
		}
		else
		{
			Renderer = new ForwardRenderer(Width, Height);
		}
		Renderer->SetViewPort(0, 0, Width, Height);
		Renderer->SetBackground(Vec4(0.2f, 0.2f, 0.2f, 1.0f));
		// A freshly constructed Renderer resets to IRenderer's own
		// hardcoded ambient default - reapply the scene's actual value so
		// switching Forward<->Deferred doesn't silently change the look.
		Renderer->SetGlobalLight(ambientLightColor);

		// Freshly constructed, not yet Bound - SetFramebufferPreserveDepth()
		// only takes effect before ExternalFBO's render pass is first
		// built (see Init()'s identical call), which is exactly why this
		// recreates EffectsManager wholesale instead of reusing the old
		// one: a live-recreated PostEffectsManager is the only way to set
		// this again once the previous instance's render pass was already
		// built and cached.
		EffectsManager = new PostEffectsManager(Width, Height);
		if (usingDeferredRenderer)
			GetActiveRenderDevice().SetFramebufferPreserveDepth(EffectsManager->GetExternalFrameBuffer()->GetBindID(), true);
	}

	void SceneEditor::Init(const uint32 width, const uint32 height)
	{
		Width = width;
		Height = height;

		// Initialize Scene
		scene = new SceneGraph();

		// Initialize Renderer - Forward or Deferred, chosen by
		// SetUseDeferredRenderer() (called before Init() by whoever creates
		// this document) from the project's Renderer setting. Used for both
		// the edit viewport and Play Mode so materials render consistently
		// in both - see the Renderer/usingDeferredRenderer comment in the header.
		usingDeferredRenderer = pendingUseDeferredRenderer;
		if (usingDeferredRenderer)
		{
			BuildGBuffer(Width, Height);
			DeferredRenderer* deferred = new DeferredRenderer(Width, Height, gbufferFBO);
			// The viewport reads this renderer's output through
			// GetColorTexture() and composites it into an ImGui::Image, so
			// RenderScene()'s extra "Render to Screen" pass has no consumer -
			// it just blits the bare scene over framebuffer 0 every frame,
			// racing whatever else targets the drawable. See
			// SetSkipRenderToScreen()'s comment; MaterialPreview already
			// does this and SceneEditor was the other caller it names.
			deferred->SetSkipRenderToScreen(true);
			Renderer = deferred;
		}
		else
		{
			Renderer = new ForwardRenderer(Width, Height);
		}
		Renderer->SetViewPort(0, 0, Width, Height);
		Renderer->SetBackground(Vec4(0.2, 0.2, 0.2, 1.0));
		// Mirrors the renderer-switch path, which has always done this: a
		// freshly constructed renderer starts on IRenderer's own hardcoded
		// ambient default, and the value that actually reaches the shader
		// lives in a process-wide UBO shared by every IRenderer in the
		// process (see IRenderer's static AmbientLightUniformsUBO /
		// CachedGlobalLight). Init never asserted its own value, so this
		// document rendered with whatever the last renderer to touch that
		// UBO had left in it - which on Vulkan+Deferred came out red.
		Renderer->SetGlobalLight(ambientLightColor);

		// Projection
		isPerspective = true;
		viewIsOrtho = false;
		viewOrthoL = viewOrthoB = -5.f; viewOrthoR = viewOrthoT = 5.f;
		zoomOrtho = 5;

		// Physics
		physics = new Physics();
		physics->InitPhysics();
		// Edit mode: GameObject transforms are authoritative; physics bodies
		// follow the gizmo/properties panel until Play is pressed.
		static_cast<Box3DPhysics*>(physics)->SetSimulationEnabled(false);
		physics->EnableDebugDraw();

		// Audio - one process-wide manager owned by Editor; all scene tabs share it.
		audio = sharedAudioManager;
		if (!audio || !audio->IsInitialized())
			echo("WARNING: SceneEditor - no initialized shared AudioManager");
		else
			SetAsActiveAudioDevice();

		// Create Camera
		Camera = std::make_shared<GameObject>();
		Camera->SetPosition(Vec3(0, 10, 20));
		Camera->SetRotation(Vec3(-0.464, 0, 0));
		CameraPivot = std::make_shared<GameObject>();
		CameraPivot->Add(Camera);
		scene->Add(CameraPivot);
		scene->Add(Camera);

		// Create Grid
		// grid/gridhandle/rGrid/GridMaterial stay constructed (several other
		// systems - IsInternalGameObject, SetEditorChromeVisible,
		// RenderCameraPreview, viewport picking exclusion - reference them)
		// but `grid` is deliberately never scene->Add()'d any more: the grid
		// is an editor-only helper with nothing to do with the actual scene,
		// so it's now drawn immediate-mode via DebugRenderer instead (see
		// ShowViewport()) - same as gizmos/axis-helper/physics-debug - and
		// never touches Renderer->RenderScene() (Forward or Deferred) at all.
		grid = std::make_shared<GameObject>();
		gridhandle = std::make_shared<Grid>(30.f, 30,
			Vec4(0.35f, 0.35f, 0.35f, 1.f), Vec4(0.55f, 0.25f, 0.25f, 1.f));
		GridMaterial = std::make_shared<GenericShaderMaterial>(ShaderUsage::Color);
		GridMaterial->SetColor(Vec4(1.f, 1.f, 1.f, 1.f));
		rGrid = std::make_shared<RenderingComponent>(gridhandle, GridMaterial);
		rGrid->DisableCastShadows();
		grid->Add(rGrid);
		RenderingMesh* rGridMesh = rGrid->GetMeshes()[0];
		rGridMesh->drawingType = DrawingType::Lines;
		// Editor chrome, not scene content - keep it out of the pick buffer
		// entirely instead of picking it and then filtering the result.
		rGridMesh->Clickable = false;

		_leftMouse = _middleMouse = _rightMouse = _mousePanned = false;

		gizmo = NULL;
		localTransform = false;
		gizmoDragging = false;

		playMode = false;
		playModeSavedCameraId = 0;
		showPhysicsDebug = true;
		physics2D = new Physics2DWorld();
		uiEditMode = false;
		sceneIsTwoD = false;
		canvasDragHandle = -1;
		canvasDragGoId = 0;

		// Null GameObject
		SelectedSceneObject = NULL;
		sceneObjects = new SceneObjects(scene);

		// Picking
		Picking = new PainterPick(Width, Height);
		Picking->SetViewPort(0, 0, Width, Height);

		// Translation Gizmo By Default
		UseTranslationManipulator();

		EffectsManager = new PostEffectsManager(Width, Height);
		// Deferred's gizmo/grid overlay (see ShowViewport()) explicitly
		// copies DeferredRenderer's real scene depth into this FBO's Depth
		// attachment right before rebinding it each frame, so that bind
		// must not clear depth back to 1.0 the way a normal capture would -
		// must be set before ExternalFBO's render pass is first built (i.e.
		// before any Bind()), same as DeferredRenderer.cpp does for its own
		// lastPassFBO. Forward mode never touches this (its capture wraps
		// the real RenderScene() call, which needs depth cleared fresh each
		// frame like any normal draw), so left at the default there.
		if (usingDeferredRenderer)
			GetActiveRenderDevice().SetFramebufferPreserveDepth(EffectsManager->GetExternalFrameBuffer()->GetBindID(), true);
		// Independent of the Forward/Deferred switch: it composites into
		// whatever target the viewport is already assembling, so it is built
		// here once and SwitchRenderer() leaves it alone.
		uiRenderer = new UIRenderer(Width, Height);

		previewRenderer = new ForwardRenderer(previewWidth, previewHeight);
		previewRenderer->SetSkipShadowMaps(true);
		previewEffects = new PostEffectsManager(previewWidth, previewHeight);
		thumbRenderer = new ForwardRenderer(thumbWidth, thumbHeight);
		thumbRenderer->SetSkipShadowMaps(true);
		thumbEffects = new PostEffectsManager(thumbWidth, thumbHeight);

		InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &SceneEditor::MouseMove);
		InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Wheel, this, &SceneEditor::MouseWheel);
		InputManager::AddEvent(Event::Type::OnPress, Event::Input::Mouse::Left, this, &SceneEditor::MouseLeftPress);
		InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Mouse::Left, this, &SceneEditor::MouseLeftRelease);
		InputManager::AddEvent(Event::Type::OnPress, Event::Input::Mouse::Middle, this, &SceneEditor::MouseMiddlePress);
		InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Mouse::Middle, this, &SceneEditor::MouseMiddleRelease);
		InputManager::AddEvent(Event::Type::OnPress, Event::Input::Mouse::Right, this, &SceneEditor::MouseRightPress);
		InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Mouse::Right, this, &SceneEditor::MouseRightRelease);

		axisHelper = new AxisHelper();

		node_clicked = -1;
		pendingDeleteId = 0;

		draggin_id = -1;
		droppin_id = -1;
		sub_selection = -1;

		showingAddFrom = false;
		showingAddFormType = 0;
		AddForm_mass = 1.0f;
		AddForm_ghost = false;
		AddForm_stream = false;
		AddForm_loop = false;
		AddForm_spatialized = true;
		AddForm_volume = 1.0f;
		AddForm_soundPath.clear();
		AddForm_particleTexturePath.clear();
		AddForm_particleMax = 200;
		AddForm_particlePreset = 0;
		propertiesParticleTexturePath.clear();
		propertiesParticleMax = 200;
		propertiesParticleSeededId = 0;
		particlePreviewSelectionId = 0;
		particlePreviewSynced = false;
		particlePreviewSystem = NULL;

		SelectedMeshMaterial = std::make_shared<SelectedMaterial>();
		SelectedMeshMaterial->EnableDepthTest(DepthTest::LEqual);
		SelectedMeshMaterial->EnableBlending();
		SelectedMeshMaterial->BlendingEquation(BlendEq::Add);
		SelectedMeshMaterial->BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
		SelectedMesh = NULL;
		SelectedRenderingComponent.reset();

		debugRenderer = new DebugRenderer();
		editorDebugDraw = new EditorDebugDraw();
		// The gizmo draws through it - see CGizmoTransformRender.
		CGizmoTransformRender::SetDebugRenderer(debugRenderer);
		// Share the editor's debugRenderer with physics debug draw so it
		// uses the correct backend device (GL/Vulkan/Metal) rather than
		// creating its own with a stale GL fallback.
		Box3DPhysics* box3d = static_cast<Box3DPhysics*>(physics);
		box3d->SetDebugRenderer(debugRenderer);

		// Load Icons
		icons = new Texture();
		icons->LoadTexture("assets/icons/icons.png");
	}

	void SceneEditor::BindSharedEditorHooks()
	{
		if (debugRenderer)
			CGizmoTransformRender::SetDebugRenderer(debugRenderer);
		if (physics)
		{
			Box3DPhysics* box3d = static_cast<Box3DPhysics*>(physics);
			box3d->SetDebugRenderer(debugRenderer);
		}
	}

	void SceneEditor::SetHostDocumentCallbacks(void (*onActivate)(SceneEditor*), void (*onRequestClose)(SceneEditor*),
		void (*onNewSceneDocument)(), void (*onOpenSceneDocument)(const std::string&),
		void (*onOpenLuaScript)(const std::string&),
		void (*onEditMaterialInline)(std::shared_ptr<p3d::IMaterial>, const std::string&),
		std::string (*onAssignMaterialAsset)(const std::string&, int, const std::string&))
	{
		hostActivateDocument = onActivate;
		hostRequestCloseDocument = onRequestClose;
		hostNewSceneDocument = onNewSceneDocument;
		hostOpenSceneDocument = onOpenSceneDocument;
		hostOpenLuaScript = onOpenLuaScript;
		hostEditMaterialInline = onEditMaterialInline;
		hostAssignMaterialAsset = onAssignMaterialAsset;
	}

	std::string SceneEditor::GetSceneDisplayName() const
	{
		if (scenePath.empty())
			return "Untitled";
		namespace fs = std::filesystem;
		return fs::path(scenePath).stem().string();
	}

	void SceneEditor::Show()
	{
		// Panels are hosted by Editor (project dock layout).
		ShowViewport();
		ShowHierarchy();
	}

	void SceneEditor::ShowViewport()
	{
		// Must run before anything below touches Renderer/EffectsManager -
		// see its own comment on why a queued SwitchRenderer() is applied
		// here instead of inline from whatever UI callback requested it.
		ApplyPendingRendererSwitchIfAny();
#ifdef LUA_BINDINGS
		// Same safe point, same reason - between frames, before anything
		// this frame touches the scene graph.
		ApplyPendingSceneLoadIfAny();
#endif

		const bool scene_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
		// Escape always exits play (even with mouse capture / other panels focused).
		if (playMode && ImGui::IsKeyPressed(ImGuiKey_Escape))
			StopPlayMode();
		if (scene_focused && !playMode)
		{
			// Unmodified only: Ctrl+S is Save Project in the menu bar, and
			// letting it also swap the gizmo made one keystroke do two things.
			const bool plainKey = !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeySuper
				&& !ImGui::GetIO().KeyAlt;
			if (plainKey && ImGui::IsKeyPressed(ImGuiKey_T)) UseTranslationManipulator();
			if (plainKey && ImGui::IsKeyPressed(ImGuiKey_R)) UseRotationManipulator();
			if (plainKey && ImGui::IsKeyPressed(ImGuiKey_S)) UseScaleManipulator();
			if ((ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) && !ImGui::GetIO().WantTextInput)
				DeleteSelected();
			if (ImGui::IsKeyPressed(ImGuiKey_D) && ImGui::GetIO().KeyCtrl)
				DuplicateSelected();
		}
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));
		if (playMode) ImGui::BeginDisabled();
		if (ImGui::SmallButton("T")) UseTranslationManipulator();
		ImGui::SameLine();
		if (ImGui::SmallButton("R")) UseRotationManipulator();
		ImGui::SameLine();
		if (ImGui::SmallButton("S")) UseScaleManipulator();
		ImGui::SameLine();
		if (ImGui::Checkbox("Local", &localTransform))
		{
			if (localTransform) UseLocalManipulator();
			else UseGlobalManipulator();
		}
		if (playMode) ImGui::EndDisabled();
		ImGui::SameLine();
		if (playMode)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.2f, 0.15f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.2f, 1.f));
			if (ImGui::SmallButton("Stop"))
				StopPlayMode();
			ImGui::PopStyleColor(2);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.f, 0.75f, 0.2f, 1.f), "PLAYING");
		}
		else if (ImGui::SmallButton("Play"))
		{
			EnterPlayMode();
		}
		if (activeSceneCameraId != 0)
		{
			SceneObject* activeCamObj = sceneObjects->GetSceneObject(activeSceneCameraId);
			if (activeCamObj != NULL)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("| View: %s", activeCamObj->GetName().c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton("Editor Cam"))
					ClearActiveSceneCamera();
			}
			else
				activeSceneCameraId = 0;
		}
		ImGui::PopStyleVar();
		ImGui::Separator();
		dim = Vec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);

		if (dim.x < 1.f || dim.y < 1.f)
		{
			NotifyViewportNotDrawn();
			if (showingAddFrom) ShowAddForm();
			else if (!playMode) editorDisabled = false;
			return;
		}

		GameObject* viewCam = GetViewCameraGO();
		const f32 viewFov = GetViewFovDeg();
		const bool haveSceneCam = (activeSceneCameraId != 0 && sceneCameras.count(activeSceneCameraId) > 0);
		const f32 viewNear = haveSceneCam ? sceneCameras[activeSceneCameraId].nearPlane : 0.1f;
		const f32 viewFar = haveSceneCam ? sceneCameras[activeSceneCameraId].farPlane : 100000.f;

		// Looking through a scene camera means looking through ITS
		// projection, orthographic included - otherwise the viewport shows a
		// perspective view of a game that will not render that way, which is
		// worse than showing nothing.
		if (haveSceneCam && sceneCameras[activeSceneCameraId].orthographic)
		{
			const f32 halfH = sceneCameras[activeSceneCameraId].orthoSize;
			const f32 halfW = halfH * ((f32)dim.x / (f32)dim.y);
			projection.Ortho(-halfW, halfW, -halfH, halfH, viewNear, viewFar);
			// Recorded for the gizmo: looking through an orthographic scene
			// camera is an orthographic view, whatever the editor camera's
			// own perspective toggle says.
			viewIsOrtho = true;
			viewOrthoR = halfW; viewOrthoL = -halfW;
			viewOrthoT = halfH; viewOrthoB = -halfH;
		}
		else
		{
			projection.Perspective(viewFov, (f32)dim.x / (f32)dim.y, viewNear, viewFar);
			viewIsOrtho = false;
		}

		{
			if (dim.x > dim.y)
			{
				r = zoomOrtho;
				l = -r;
				t = dim.y*zoomOrtho / dim.x;
				b = -t;
			}
			else if (dim.x < dim.y)
			{
				r = dim.x*zoomOrtho / dim.y;
				l = -r;
				t = zoomOrtho;
				b = -t;
			}
			else
			{
				r = zoomOrtho;
				l = -r;
				t = zoomOrtho;
				b = -t;
			}
			projectionOrtho.Ortho(l, r, b, t, 0.1f, 100000.f);
			// The editor camera's own ortho toggle. Takes precedence, because
			// when it is on that projection is what actually gets drawn.
			if (!isPerspective)
			{
				viewIsOrtho = true;
				viewOrthoL = l; viewOrthoR = r; viewOrthoB = b; viewOrthoT = t;
			}
		}

		viewportImgMin = ImGui::GetCursorScreenPos();
		viewportImgSize = ImVec2(dim.x, dim.y);
		UpdateViewportMouse();
		// Canvas mode has its own input model - see HandleCanvasInput. The
		// 3D gizmo moves a transform, and a UI element's transform is
		// output: the layout writes it every frame, so anything the gizmo
		// put there is gone by the next solve.
		UICanvas* editingCanvas = uiEditMode ? GetEditingCanvas() : NULL;
		if (editingCanvas)
			HandleCanvasInput(editingCanvas);
		else
			HandleViewportGizmoInput(viewCam);

		const uint32 viewW = (uint32)dim.x;
		const uint32 viewH = (uint32)dim.y;

		// Camera preview shares IRenderer statics (shadow counts, MaterialUniforms,
		// etc.) with the main ForwardRenderer. Run it *before* the viewport pass
		// so PreRender below restores that state. Properties only displays the
		// cached texture — never re-renders after the main pass.
		if (!playMode && SelectedSceneObject != NULL
			&& SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT
			&& IsSceneCamera(SelectedSceneObject->GetID()))
		{
			RenderCameraPreview((GameObject*)SelectedSceneObject->GetPTR());
		}

		IRenderer::InvalidateSharedUniformCaches();
		EffectsManager->ProcessPostEffects((isPerspective?&projection:&projectionOrtho));
		EffectsManager->Resize(viewW, viewH);
		// DeferredRenderer::Resize() only resizes its own internal FBOs, not
		// this SceneEditor-owned gbufferFBO handed in at construction - has
		// to be resized independently, same as render_host.lua's setup.
		if (usingDeferredRenderer && gbufferFBO && (viewW != gbufferAlbedo->GetWidth() || viewH != gbufferAlbedo->GetHeight()))
		{
			gbufferDepth->Resize(viewW, viewH);
			gbufferAlbedo->Resize(viewW, viewH);
			gbufferSpecular->Resize(viewW, viewH);
			gbufferNormal->Resize(viewW, viewH);
			gbufferMatRough->Resize(viewW, viewH);
			gbufferFBO->Resize(viewW, viewH);
		}
		// Canvas mode: the 3D scene is not what is being edited, so it is
		// not drawn. Done by pointing the renderer at the empty layer rather
		// than by skipping RenderScene(), which is also what clears the
		// target and leaves the depth and device state everything below
		// expects. It has to be set before PreRender(), which is where the
		// draw list is actually built.
		//
		// RenderLayer::None, not ::UI: aiming it at the UI layer made the
		// world pass draw the canvas a second time, through the scene
		// camera's perspective projection - which put a copy of every
		// element somewhere else entirely on screen.
		const uint32 restoreLayer = Renderer->GetRenderLayer();
		if (editingCanvas) Renderer->SetRenderLayer(RenderLayer::None);

		Renderer->Resize(viewW, viewH);
		Renderer->ResetViewPort();
		Renderer->SetViewPort(0, 0, viewW, viewH);
		// 2D shadow occluders, before anything is drawn with them. Scene
		// content, not a debug overlay, so this sits here rather than in the
		// gizmo/debug block below - that block is skipped entirely in canvas
		// mode, which would have silently turned shadows off there.
		Occluder2D::PublishSceneOccluders(scene);

		Renderer->PreRender(viewCam, scene);
		Renderer->ApplyBackgroundClearColor();
		// Same reason ApplyBackgroundClearColor() is re-asserted every frame
		// rather than set once: both the clear colour and the ambient term
		// are process-wide state that any other IRenderer can overwrite, and
		// this editor runs several (preview, thumbnail, axis helper, the
		// other scene tabs). AxisHelper::Render() re-asserts its own ambient
		// every frame for exactly this reason; whoever renders last wins, so
		// the viewport has to state its value rather than assume it survived.
		Renderer->SetGlobalLight(ambientLightColor);
		EffectsManager->CaptureFrame();
		if (isPerspective)
			Renderer->RenderScene(projection, viewCam, scene);
		else
			Renderer->RenderScene(projectionOrtho, viewCam, scene);
		if (editingCanvas) Renderer->SetRenderLayer(restoreLayer);

		// Debug/gizmo/grid/axis-helper below draw into whatever framebuffer
		// is currently bound - for Deferred, DeferredRenderer::RenderScene()
		// leaves that as framebuffer 0 (see GetColorTexture()'s comment),
		// which gets fully overdrawn by the rest of ImGui before the frame
		// presents, so none of this ever showed up. Re-bind EffectsManager's
		// capture target fresh here and treat it as a transparent overlay
		// canvas, composited as a second layer over colorTexture below (see
		// the ImGui::Image() pair further down). Safe to rebind color
		// (unlike lastPassFBO - see DeferredRenderer.cpp's LOAD_OP_CLEAR
		// comment): ExternalFBO was never actually drawn into this frame for
		// Deferred (RenderScene() bypassed it entirely), so there is nothing
		// in it to lose. Depth is a different story - copy DeferredRenderer's
		// real scene depth in first (SetFramebufferPreserveDepth() in Init()
		// tells this FBO's Vulkan render pass to LOAD rather than CLEAR its
		// depth attachment, so this copy survives the Bind() right below it),
		// so the grid/physics-debug draws that follow depth-test against the
		// actual opaque scene instead of drawing blind.
		if (usingDeferredRenderer)
		{
			DeferredRenderer* dr = static_cast<DeferredRenderer*>(Renderer);
			GetActiveRenderDevice().CopyDepthTexture(dr->GetDepthTexture()->GetBindID(),
				EffectsManager->GetDepth()->GetBindID(), viewW, viewH);
			GetActiveRenderDevice().SetClearColor(Vec4(0.f, 0.f, 0.f, 0.f));
			EffectsManager->CaptureFrame();
		}

		// Scene UI, over the finished 3D frame and under the editor's own
		// gizmo/grid/axis overlay - which is the right order while
		// authoring: a canvas must not swallow the handles used to edit it.
		// Targets whatever is bound here, which is EffectsManager's capture
		// for Forward (the viewport image itself) and its transparent
		// overlay layer for Deferred - both end up composited over the
		// scene by the ImGui::Image pair below.
		if (uiRenderer)
		{
			uiRenderer->Resize(viewW, viewH);
			uiRenderer->RenderUI(scene);
			// Buttons only respond in play mode. In edit mode a click in the
			// viewport is a selection or a drag, and a UI that reacted to
			// being authored would be unusable.
			if (playMode) DispatchPlayModeUIInput();
		}

		// The gizmo/grid/debug overlay below never sets a viewport of its own
		// (DebugRenderer::SetViewPort() is not called from here at all) - it
		// relies on the render pass it lands in having the right one already.
		// Say so explicitly rather than inheriting it, so this does not
		// silently depend on which pass happened to be opened last.
		GetActiveRenderDevice().SetViewport(0, 0, viewW, viewH);

		// Gizmo must submit into DebugRenderer before the flush below.
		//
		// Canvas mode takes the debug pass over completely: everything in
		// this block is a 3D tool (a translate gizmo on a screen-space rect
		// is meaningless, and so are physics shapes and camera frustums),
		// and - more concretely - DebugRenderer::Render() opens its own
		// command buffer and rewrites the shared matrices UBO, so it must
		// happen exactly once per frame. Submitting canvas lines and calling
		// Render() a second time is what blacked the whole viewport.
		// 2D physics, in BOTH modes - deliberately outside the !playMode
		// branch below. While authoring, the bodies follow the authored
		// transforms and are never stepped: that is a view of what the shapes
		// are, not a simulation of them. In play mode it has to be the other
		// way round. It used to sit inside the editor-only branch, so play
		// mode ran no 2D solver at all - no gravity, no collisions, no
		// contact callbacks - and a script setting a velocity looked exactly
		// like dead input when it was the solver that was never running.
		if (physics2D)
		{
			physics2D->Sync(scene);
			if (playMode)
				physics2D->Step(ImGui::GetIO().DeltaTime, scene);
			else
				physics2D->PullTransforms();
			if (showPhysicsDebug && debugRenderer && !playMode)
				physics2D->DebugDraw(debugRenderer);
		}

		if (!playMode && editingCanvas)
		{
			DrawCanvasOverlay(editingCanvas, dim);
		}
		else if (!playMode)
		{
			PrepareGizmoForDraw(viewCam);
			if (SelectedSceneObject != NULL && SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT && gizmo != NULL)
				gizmo->Draw();

			if (showPhysicsDebug && physics)
				physics->RenderDebugDraw((isPerspective ? projection : projectionOrtho), viewCam);


			// The grid is an editor-only helper, not scene content - it's
			// deliberately NOT a SceneGraph object, so it never goes through
			// Renderer->RenderScene() (nor gets serialized/picked/etc. as if
			// it were real scene content). Drawn as a real RenderObject()
			// call instead of an immediate-mode DebugRenderer line batch
			// specifically so it depth-tests against the opaque scene
			// (DebugMaterial disables depth test/write for everything drawn
			// through DebugRenderer, by design, so gizmo/physics-debug below
			// stay always-on-top - the grid is the one thing here that
			// should actually be occluded by/occlude real geometry). Targets
			// whatever's currently bound, which by this point already has
			// the right depth for either renderer - EffectsManager's own
			// capture-wrapped RenderScene() for Forward, or the copy from
			// DeferredRenderer's forwardDepthTexture above for Deferred.
			// IsActive() respected so RenderCameraPreview()'s temporary
			// rGrid->Disable() (hiding it from camera-preview renders)
			// applies here too.
			if (rGrid && rGrid->IsActive())
				Renderer->RenderOverlayObject(rGrid->GetMeshes()[0], grid.get(), GridMaterial.get());

			std::vector<SceneCameraDebugEntry> sceneCameraDebugScratch;
			BuildSceneCameraDebugList(sceneCameraDebugScratch);
			if (editorDebugDraw)
				editorDebugDraw->Draw(debugRenderer, scene, viewCam, viewFov,
					(f32)dim.x / (f32)dim.y, viewH,
					grid.get(), Camera.get(), CameraPivot.get(), &sceneCameraDebugScratch);

			if (debugRenderer)
				debugRenderer->Render(viewCam->GetWorldTransformation().Inverse(),
					(isPerspective ? projection : projectionOrtho).GetProjectionMatrix());

#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
			axisHelper->Render((uint32)(dim.x - 90), 10, 80, 80, isPerspective);
#else
			axisHelper->Render((uint32)(dim.x - 90), (uint32)(dim.y - 90), 80, 80, isPerspective);
#endif
		}

		EffectsManager->EndCapture();
		void* viewportTex = NULL;
		// DeferredRenderer::RenderScene()'s final composite always targets
		// framebuffer 0, not whatever EffectsManager captured (see
		// DeferredRenderer::GetColorTexture()'s comment) - so its actual
		// output has to be read directly instead of through EffectsManager's
		// capture, unlike Forward. For Deferred, EffectsManager's capture
		// target instead holds this frame's gizmo/grid/debug/axis overlay
		// (see the CaptureFrame() re-bind above) - drawn as a second layer
		// over colorTexture below, not used as the primary viewport image.
		Texture* color = usingDeferredRenderer
			? static_cast<DeferredRenderer*>(Renderer)->GetColorTexture()
			: EffectsManager->GetViewportColor();
		Texture* overlay = usingDeferredRenderer ? EffectsManager->GetViewportColor() : NULL;
		if (color)
			viewportTex = GetActiveRenderDevice().GetImGuiTextureID(color->GetBindID(), color->GetTextureType());
		void* overlayTex = overlay ? GetActiveRenderDevice().GetImGuiTextureID(overlay->GetBindID(), overlay->GetTextureType()) : NULL;

#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
		const ImVec2 uv0(0, 0), uv1(1, 1);
#else
		const ImVec2 uv0(0, 1), uv1(1, 0);
#endif
		if (viewportTex != NULL)
		{
			const ImVec2 drawPos = ImGui::GetCursorScreenPos();
			ImGui::Image((ImTextureID)viewportTex, ImVec2(dim.x, dim.y), uv0, uv1);
			// Layered on top via ImGui's normal alpha blending - overlay is
			// transparent everywhere except the gizmo/grid/debug/axis pixels
			// actually drawn into it this frame.
			if (overlayTex != NULL)
			{
				ImGui::SetCursorScreenPos(drawPos);
				ImGui::Image((ImTextureID)overlayTex, ImVec2(dim.x, dim.y), uv0, uv1);
			}
		}
		else
			ImGui::Dummy(ImVec2(dim.x, dim.y));
		const ImVec2 imgMin = ImGui::GetItemRectMin();
		const ImVec2 imgSize = ImGui::GetItemRectSize();
		viewportImgMin = imgMin;
		viewportImgSize = imgSize;
		viewportOverlayValid = true;


		// Which renderer is actually active - top left, clear of the axis
		// gizmo (top right) and the T/R/S + Play toolbar row above the image.
		{
			const char* label = usingDeferredRenderer ? "Deferred" : "Forward";
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 textSize = ImGui::CalcTextSize(label);
			const float pad = 6.f;
			ImVec2 textPos(imgMin.x + pad + 4.f, imgMin.y + pad + 4.f);
			ImVec2 bgMin(textPos.x - pad, textPos.y - 3.f);
			ImVec2 bgMax(textPos.x + textSize.x + pad, textPos.y + textSize.y + 3.f);
			dl->AddRectFilled(bgMin, bgMax, ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 0.5f)), 3.f);
			dl->AddText(textPos, ImGui::GetColorU32(usingDeferredRenderer
				? ImVec4(0.55f, 0.8f, 1.f, 1.f) : ImVec4(1.f, 0.8f, 0.4f, 1.f)), label);
		}
		ImGui::SetCursorScreenPos(imgMin);
		ImGui::InvisibleButton("##scene_viewport_capture", imgSize,
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
		viewportHovered = ImGui::IsItemHovered();
		// See viewportInputAllowed's declaration. IsItemActive() keeps a
		// drag alive once it has started here, so orbit/pan don't stop the
		// moment the cursor crosses out of the image.
		viewportInputAllowed = viewportHovered || ImGui::IsItemActive();
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_REL"))
			{
				const char* rel = (const char*)payload->Data;
				if (rel && project && project->IsOpen())
					PlaceAssetInScene(project->AbsolutePath(rel));
			}
			ImGui::EndDragDropTarget();
		}
		UpdateViewportMouse();
		if (!playMode)
			DrawSceneViewportIcons(imgMin, imgSize, viewCam);

		if (showingAddFrom) ShowAddForm();
		else if (!playMode) editorDisabled = false;
	}

	void SceneEditor::ShowHierarchy()
	{
		ImGui::SetNextItemOpen(true);
		if (ImGui::TreeNode("Scene"))
		{
			if (ImGui::IsItemClicked())
			{
				sceneRootSelected = true;
				DeselectSceneObject();
				selection.clear();
				node_clicked = -1;
			}
			if (ImGui::BeginPopupContextItem("SceneRootContext"))
			{
				if (playMode)
					ImGui::TextDisabled("Stop play mode to edit");
				else
				{
#ifdef LUA_BINDINGS
					if (!scenePath.empty() && ImGui::MenuItem("Open Scene Script"))
					{
						EnsureAndBindSceneCompanionScript();
						if (hostOpenLuaScript && !sceneMainScriptPath.empty())
							hostOpenLuaScript(sceneMainScriptPath);
					}
					ImGui::Separator();
#endif
					ShowRightMenu();
				}
				ImGui::EndPopup();
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GO_ID"))
				{
					uint32 draggedId = *(const uint32*)payload->Data;
					std::string err;
					OpReparentGameObject(draggedId, 0, err);
				}
				ImGui::EndDragDropTarget();
			}

			DrawNodes();

			// Deferred delete (see pendingDeleteId's declaration) - now
			// safe to mutate sceneObjects since DrawNodes() has fully
			// returned and nothing is still holding a pointer/iterator
			// into the map it walks.
			if (pendingDeleteId != 0)
			{
				const uint32 deleteId = pendingDeleteId;
				pendingDeleteId = 0;
				SceneObject* toDelete = sceneObjects->GetSceneObject(deleteId);
				if (toDelete)
				{
					if (toDelete->GetType() == SceneObjectTypes::GAMEOBJECT)
						DeleteGameObjectById(deleteId);
					else
						DeleteComponentById(deleteId);
				}
			}

			if (node_clicked != -1)
			{
				if (ImGui::GetIO().KeyCtrl)
				{
					if (selection.size() == 1 && sceneObjects->GetList().at(selection[0])->GetType() != SceneObjectTypes::GAMEOBJECT) selection.clear();
					if (sceneObjects->GetList().at(node_clicked)->GetType() != SceneObjectTypes::GAMEOBJECT) node_clicked = -1;
					sub_selection = -1;
					selection.push_back(node_clicked);
				}
				else {
					selection.clear();
					selection.push_back(node_clicked);
				}
			}
			node_clicked = -1;
			ImGui::TreePop();
		}

		// Right-click empty Scene Tree space → Add
		if (ImGui::BeginPopupContextWindow("SceneTreeBlankContext", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
		{
			if (playMode)
				ImGui::TextDisabled("Stop play mode to edit");
			else
				ShowRightMenu();
			ImGui::EndPopup();
		}

		if (showingAddFrom) ShowAddForm();
		else if (!playMode) editorDisabled = false;

		// Drawn here rather than inside the menus that raise them: a
		// BeginPopupModal nested in a BeginPopupContextItem (or in a
		// BeginMenu) closes with its parent popup the moment the item is
		// clicked.
		DrawPrefabModals();
		DrawBuildModal();
	}

	// The prefab entries of a GameObject's context menu. Two different menus
	// depending on what the object is: an ordinary object can become a
	// prefab, an instance can be pushed back to / rebuilt from / detached
	// from its own.
	void SceneEditor::ShowPrefabMenu(uint32 goId)
	{
		SceneObject* obj = sceneObjects->GetSceneObject(goId);
		if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return;
		GameObject* go = (GameObject*)obj->GetPTR();
		if (!go) return;

		ImGui::Separator();

		if (obj->prefabSource.empty())
		{
			if (ImGui::MenuItem("Create Prefab…"))
			{
				prefabModalTargetId = goId;
				prefabModalName = go->GetName();
				prefabModalError.clear();
				openCreatePrefabModal = true;
			}
			return;
		}

		const std::string src = obj->prefabSource;
		const bool clean = !PrefabInstanceIsModified(goId);

		if (ImGui::BeginMenu("Prefab"))
		{
			ImGui::TextDisabled("%s", src.c_str());
			ImGui::TextDisabled("%s", clean ? "In sync" : "Modified");
			ImGui::Separator();

			// Only offered when there is something to push: applying an
			// unmodified instance would rewrite the file with what it
			// already contains, and invite the "why did my undo history
			// vanish?" question for no gain.
			if (ImGui::MenuItem("Apply to Prefab…", NULL, false, !clean))
			{
				prefabModalTargetId = goId;
				prefabModalError.clear();
				openApplyPrefabModal = true;
			}
			if (ImGui::MenuItem("Revert to Prefab", NULL, false, !clean))
			{
				std::string err;
				if (!OpRevertPrefab(goId, err)) echo("ERROR: Revert to Prefab - " + err);
			}
			if (ImGui::MenuItem("Unpack Prefab"))
			{
				std::string err;
				if (!OpUnpackPrefab(goId, err)) echo("ERROR: Unpack Prefab - " + err);
			}
			ImGui::EndMenu();
		}
	}

	void SceneEditor::DrawPrefabModals()
	{
		if (openCreatePrefabModal)
		{
			ImGui::SetNextWindowFocus();
			ImGui::OpenPopup("Create Prefab");
			openCreatePrefabModal = false;
		}
		if (ImGui::BeginPopupModal("Create Prefab", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Saves this object and its children to assets/prefabs/<name>.prefab.");
			ImGui::TextDisabled("The object becomes the first instance of it.");
			ImGui::SetNextItemWidth(280.f);
			ImGui::InputText("Name", &prefabModalName);
			if (!prefabModalError.empty())
				ImGui::TextColored(ImVec4(1.f, 0.45f, 0.4f, 1.f), "%s", prefabModalError.c_str());
			ImGui::Spacing();
			if (ImGui::Button("Create", ImVec2(120, 0)))
			{
				std::string rel;
				if (OpCreatePrefab(prefabModalTargetId, prefabModalName, rel, prefabModalError))
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		if (openApplyPrefabModal)
		{
			ImGui::SetNextWindowFocus();
			ImGui::OpenPopup("Apply to Prefab");
			openApplyPrefabModal = false;
		}
		if (ImGui::BeginPopupModal("Apply to Prefab", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			const std::string src = PrefabPathOf(prefabModalTargetId);

			ImGui::Text("Overwrite %s with this object's current state?", src.c_str());
			ImGui::Spacing();
			ImGui::TextUnformatted("Every instance of this prefab that has no changes of its own");
			ImGui::TextUnformatted("will be rebuilt - in this scene, and in any other scene that");
			ImGui::TextUnformatted("uses it, the next time that scene is opened.");
			ImGui::Spacing();
			// The one destructive part worth naming outright, because it is
			// the part a user cannot reason their way back from.
			ImGui::TextColored(ImVec4(1.f, 0.75f, 0.35f, 1.f),
				"This writes a project asset and clears the undo history.");
			if (!prefabModalError.empty())
				ImGui::TextColored(ImVec4(1.f, 0.45f, 0.4f, 1.f), "%s", prefabModalError.c_str());
			ImGui::Spacing();
			if (ImGui::Button("Apply", ImVec2(120, 0)))
			{
				if (OpApplyPrefab(prefabModalTargetId, prefabModalError))
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
	}

	void SceneEditor::DrawNodes(uint32 parentID, uint32 depth)
	{
		// Prevent infinite recursion by limiting depth
		if (depth > 100) {
			return;
		}
		
		bool parent_selected = false;
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); i++)
		{
			if ((*i).second == NULL) {
				continue;
			}
			if ((*i).second->GetParentID() == parentID)
			{
				ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow;
				for (int k = 0; k<selection.size(); k++)
					if (selection[k] == (*i).second->GetID())
					{
						base_flags |= ImGuiTreeNodeFlags_Selected;
						parent_selected = true;
						break;
					}

				bool node_open = false;
				const std::string label = TreeLabel((*i).second);

				switch ((*i).second->GetType())
				{
				case SceneObjectTypes::GAMEOBJECT:
				{
                    if ((*i).second->GetID() != draggin_id) {
                        ImGuiTreeNodeFlags gameobject_flags = base_flags;
						// Not Lua-only: ViewportPickAtMouse() uses this too, to
						// reveal a component it just selected in the viewport
						// (clicking a light's billboard selects the light
						// component, which is invisible while its GameObject is
						// collapsed - the selection looked like it had failed).
						if (hierarchyForceOpenId != 0 && (*i).second->GetID() == hierarchyForceOpenId)
						{
							ImGui::SetNextItemOpen(true, ImGuiCond_Always);
							hierarchyForceOpenId = 0;
						}
                        node_open = ImGui::TreeNodeEx((void*)(intptr_t)(*i).second->GetID(), gameobject_flags, "%s", label.c_str());
                    } else {
                        node_open = false;
                    }
					break;
				}
				case SceneObjectTypes::RENDERING_COMPONENT:
				{
                    if ((*i).second->GetID() != draggin_id) {
                        ImGuiTreeNodeFlags rendering_flags = base_flags;
                        node_open = ImGui::TreeNodeEx((void*)(intptr_t)(*i).second->GetID(), rendering_flags, "%s", label.c_str());
                    } else {
                        node_open = false;
                    }
					break;
				}
				case SceneObjectTypes::PHYSICS_COMPONENT:
				{
					ImGuiTreeNodeFlags physics_flags = base_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if ((*i).second->GetID() != draggin_id) {
                        node_open = ImGui::TreeNodeEx((void*)(intptr_t)(*i).second->GetID(), physics_flags, "%s", label.c_str());
                    } else {
                        node_open = false;
                    }
					break;
				}
				case SceneObjectTypes::AUDIO_SOURCE_COMPONENT:
				{
					ImGuiTreeNodeFlags audio_flags = base_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
					if ((*i).second->GetID() != draggin_id)
						node_open = ImGui::TreeNodeEx((void*)(intptr_t)(*i).second->GetID(), audio_flags, "%s", label.c_str());
					else
						node_open = false;
					break;
				}
				case SceneObjectTypes::PARTICLE_SYSTEM_COMPONENT:
				{
					ImGuiTreeNodeFlags particles_flags = base_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
					if ((*i).second->GetID() != draggin_id)
						node_open = ImGui::TreeNodeEx((void*)(intptr_t)(*i).second->GetID(), particles_flags, "%s", label.c_str());
					else
						node_open = false;
					break;
				}
				case SceneObjectTypes::LUA_COMPONENT:
				{
					ImGuiTreeNodeFlags lua_flags = base_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
					if ((*i).second->GetID() != draggin_id)
						node_open = ImGui::TreeNodeEx((void*)(intptr_t)(*i).second->GetID(), lua_flags, "%s", label.c_str());
					else
						node_open = false;
					break;
				}
				case SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT:
				case SceneObjectTypes::POINTLIGHT_COMPONENT:
				case SceneObjectTypes::SPOTLIGHT_COMPONENT:
				{
					ImGuiTreeNodeFlags light_flags = base_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if ((*i).second->GetID() != draggin_id) {
                        node_open = ImGui::TreeNodeEx((void*)(intptr_t)(*i).second->GetID(), light_flags, "%s", label.c_str());
                    } else {
                        node_open = false;
                    }
					break;
				}
				default:
					break;
				}

				// Select on label click regardless of expanded/collapsed state.
				// TreeNodeEx returns false for collapsed nodes and for Leaf|NoTreePushOnOpen
				// entries — gating on node_open made most tree clicks do nothing.
				if ((*i).second->GetID() != draggin_id && ImGui::IsItemClicked())
				{
					node_clicked = (*i).second->GetID();
					if (sub_selection >= 0)
					{
						DeselectMesh();
						sub_selection = -1;
					}
					SelectSceneObject(sceneObjects->GetSceneObject(node_clicked));
				}

#ifdef LUA_BINDINGS
				// Double-click a script component to open it in the code editor.
				if ((*i).second->GetType() == SceneObjectTypes::LUA_COMPONENT
					&& (*i).second->GetID() != draggin_id
					&& ImGui::IsItemHovered()
					&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					LuaComponent* lc = (LuaComponent*)(*i).second->GetPTR();
					if (lc && !lc->scriptFile.empty() && hostOpenLuaScript)
						hostOpenLuaScript(ResolveScriptPath(lc->scriptFile));
				}
#endif

				if ((*i).second->GetID() != draggin_id)
					DrawTreeNodeWidgets((*i).second, node_open);

//							}
//
//						}
//						else // No parent selected
//						{
//							if ((*i).second->GetType() == SceneObjectTypes::GAMEOBJECT)
//							{
//								GameObject* child = (GameObject*)sceneObjects->GetSceneObject(draggin_id)->GetPTR();
//								if (child->GetParent() != NULL)
//									child->GetParent()->Remove(child);
//
//								sceneObjects->GetSceneObject(draggin_id)->SetParentID(0);
//							}
//						}
//						draggin_id = -1;
//					}
//				}
//
//				if ((*i).second->GetType() == SceneObjectTypes::GAMEOBJECT)
//				{
//					// Mark for Dropping
//					if (ImGui::IsMouseDragging() && ImGui::IsItemHovered() && draggin_id != (*i).second->GetID())
//						droppin_id = (*i).second->GetID();
//
//					if (ImGui::IsMouseDragging() && !ImGui::IsItemHovered() && droppin_id == (*i).second->GetID())
//						droppin_id = -1;
//				}
//
				if (node_open)
				{
					const uint32 nodeType = (*i).second->GetType();
					// Leaf | NoTreePushOnOpen nodes never TreePush — must not TreePop.
					const bool leafNoPush =
						nodeType == SceneObjectTypes::PHYSICS_COMPONENT
						|| nodeType == SceneObjectTypes::AUDIO_SOURCE_COMPONENT
						|| nodeType == SceneObjectTypes::PARTICLE_SYSTEM_COMPONENT
						|| nodeType == SceneObjectTypes::LUA_COMPONENT
						|| nodeType == SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT
						|| nodeType == SceneObjectTypes::POINTLIGHT_COMPONENT
						|| nodeType == SceneObjectTypes::SPOTLIGHT_COMPONENT;

					if (nodeType == SceneObjectTypes::RENDERING_COMPONENT)
					{
						uint32 meshesCounter = 0;
						RenderingComponent* r = (RenderingComponent*)(*i).second->GetPTR();
						for (std::vector<RenderingMesh*>::iterator k = r->GetMeshes().begin(); k != r->GetMeshes().end(); k++)
						{
							ImGuiTreeNodeFlags mesh_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick;

							if (sub_selection == meshesCounter && parent_selected)
								mesh_flags |= ImGuiTreeNodeFlags_Selected;

							ImGui::TreeNodeEx((void*)(intptr_t)(*k), mesh_flags, "Mesh[%d]", meshesCounter);
							if (ImGui::IsItemClicked())
							{
								DeselectMesh();
								SelectMesh((*k));
								sub_selection = meshesCounter;
								node_clicked = (*i).second->GetID();
								SelectSceneObject(sceneObjects->GetSceneObject((*i).second->GetID()));
							}

							meshesCounter++;
						}
					}
					if (!leafNoPush)
					{
						bool hasChildren = false;
						for (std::map<uint32, SceneObject*>::const_iterator j = sceneObjects->GetList().begin(); j != sceneObjects->GetList().end(); j++)
						{
							if ((*j).second != NULL && (*j).second->GetParentID() == (*i).second->GetID())
							{
								hasChildren = true;
								break;
							}
						}
						if (hasChildren && (*i).second->GetID() != parentID)
							DrawNodes((*i).second->GetID(), depth + 1);
						ImGui::TreePop();
					}
				}
			}
		}
	}

	bool SceneEditor::IsInternalGameObject(GameObject* go) const
	{
		if (!go) return true;
		if (grid && go == grid.get()) return true;
		if (Camera && go == Camera.get()) return true;
		if (CameraPivot && go == CameraPivot.get()) return true;
		return false;
	}

	bool SceneEditor::IsSceneCamera(uint32 id) const
	{
		return sceneCameras.find(id) != sceneCameras.end();
	}

	void SceneEditor::UnregisterSceneCamera(uint32 id)
	{
		sceneCameras.erase(id);
	}

	void SceneEditor::RegisterSceneCamera(uint32 id, const EditorCameraSettings& settings)
	{
		sceneCameras[id] = settings;
		SceneObject* obj = sceneObjects->GetSceneObject(id);
		if (obj != NULL && obj->GetType() == SceneObjectTypes::GAMEOBJECT)
			((GameObject*)obj->GetPTR())->AddTag(kEditorCameraTag);
	}

	void SceneEditor::ApplyCameraTagsFromScene()
	{
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); i++)
		{
			if (i->second == NULL || i->second->GetType() != SceneObjectTypes::GAMEOBJECT) continue;
			if (IsSceneCamera(i->second->GetID())) continue;
			GameObject* go = (GameObject*)i->second->GetPTR();
			if (go == NULL) continue;
			const std::map<uint32, std::string>& tags = go->GetTags();
			for (std::map<uint32, std::string>::const_iterator t = tags.begin(); t != tags.end(); ++t)
			{
				if (t->second == kEditorCameraTag)
				{
					RegisterSceneCamera(i->second->GetID());
					break;
				}
			}
		}
	}

	GameObject* SceneEditor::GetViewCameraGO() const
	{
		if (playMode && scriptRenderCamera != nullptr)
			return scriptRenderCamera;

		if (activeSceneCameraId != 0)
		{
			SceneObject* so = sceneObjects->GetSceneObject(activeSceneCameraId);
			if (so != NULL && so->GetType() == SceneObjectTypes::GAMEOBJECT)
				return (GameObject*)so->GetPTR();
		}
		return Camera.get();
	}

	void SceneEditor::SetScriptRenderCamera(GameObject* go)
	{
		scriptRenderCamera = go;
		if (go && playMode)
			echo(std::string("SUCCESS: Render camera set to \"") + go->GetName() + "\"");
		else if (!go)
			echo("SUCCESS: Render camera cleared (using scene camera)");
	}

	f32 SceneEditor::GetViewFovDeg() const
	{
		if (activeSceneCameraId != 0)
		{
			std::map<uint32, EditorCameraSettings>::const_iterator it = sceneCameras.find(activeSceneCameraId);
			if (it != sceneCameras.end()) return it->second.fov;
		}
		return 70.f;
	}

	void SceneEditor::SetActiveSceneCamera(uint32 sceneObjectId)
	{
		if (sceneObjectId == 0 || !IsSceneCamera(sceneObjectId)) return;
		activeSceneCameraId = sceneObjectId;
	}

	void SceneEditor::ClearActiveSceneCamera()
	{
		activeSceneCameraId = 0;
	}

	bool SceneEditor::SaveEditorSidecar(const std::string& path) const
	{
		if (path.size() == 0) return false;
		json j;
		json cams = json::object();
		for (std::map<uint32, EditorCameraSettings>::const_iterator i = sceneCameras.begin(); i != sceneCameras.end(); ++i)
		{
			SceneObject* so = sceneObjects->GetSceneObject(i->first);
			if (so == NULL) continue;
			cams[so->GetName()] = {
				{"orthographic", i->second.orthographic},
				{"fov", i->second.fov},
				{"orthoSize", i->second.orthoSize},
				{"near", i->second.nearPlane},
				{"far", i->second.farPlane}
			};
		}
		j["cameras"] = cams;
		if (activeSceneCameraId != 0)
		{
			SceneObject* active = sceneObjects->GetSceneObject(activeSceneCameraId);
			if (active != NULL)
				j["activeCamera"] = active->GetName();
		}
		std::ofstream out(path + ".editor.json");
		if (!out) return false;
		out << j.dump(2);
		return true;
	}

	bool SceneEditor::LoadEditorSidecar(const std::string& path)
	{
		if (path.size() == 0) return false;
		std::ifstream in(path + ".editor.json");
		if (!in) return false;
		json j;
		try { in >> j; }
		catch (...) { return false; }

		std::string activeName;
		if (j.contains("activeCamera") && j["activeCamera"].is_string())
			activeName = j["activeCamera"].get<std::string>();

		activeSceneCameraId = 0;
		if (j.contains("cameras") && j["cameras"].is_object())
		{
			for (json::iterator it = j["cameras"].begin(); it != j["cameras"].end(); ++it)
			{
				const std::string name = it.key();
				EditorCameraSettings s;
				// Defaults chosen so a sidecar written before orthographic
				// cameras existed reads back as the perspective camera it
				// has always been.
				s.orthographic = it.value().value("orthographic", false);
				s.fov = it.value().value("fov", 70.f);
				s.orthoSize = it.value().value("orthoSize", 10.f);
				s.nearPlane = it.value().value("near", 0.1f);
				s.farPlane = it.value().value("far", 2000.f);
				for (std::map<uint32, SceneObject*>::const_iterator o = sceneObjects->GetList().begin(); o != sceneObjects->GetList().end(); ++o)
				{
					if (o->second == NULL || o->second->GetType() != SceneObjectTypes::GAMEOBJECT) continue;
					if (o->second->GetName() != name) continue;
					RegisterSceneCamera(o->second->GetID(), s);
					if (name == activeName)
						activeSceneCameraId = o->second->GetID();
					break;
				}
			}
		}
		return true;
	}

	void SceneEditor::DrawSceneViewportIcons(const ImVec2& imgMin, const ImVec2& imgSize, GameObject* viewCam)
	{
		viewportIcons.clear();
		if (!viewCam || imgSize.x < 1.f || imgSize.y < 1.f) return;

		ImDrawList* dl = ImGui::GetWindowDrawList();
		// Clip to the rendered image, not to the Scene View window. These
		// glyphs are drawn in screen space from a 3D projection, so an object
		// near the edge of the frustum lands a few pixels outside the image -
		// and without a clip rect ImGui happily painted it over the toolbar
		// above the viewport or the panel beside it, which is what made
		// helper icons appear to escape the scene view. The hit rects
		// registered below are clipped to the same box, so a glyph that is
		// only half drawn can still only be clicked where it is visible.
		const ImVec2 imgMax(imgMin.x + imgSize.x, imgMin.y + imgSize.y);
		dl->PushClipRect(imgMin, imgMax, true);
		const Matrix viewM = viewCam->GetWorldTransformation().Inverse();
		const Matrix projM = (isPerspective ? projection : projectionOrtho).GetProjectionMatrix();

		// outDepth is view-space distance in front of the eye, kept so
		// overlapping icons can be resolved nearest-first when picking.
		auto projectToImage = [&](const Vec3& wp, ImVec2& out, f32& outDepth) -> bool {
			const Vec4 viewP = viewM * Vec4(wp.x, wp.y, wp.z, 1.f);
			Vec4 clip = projM * viewP;
			if (clip.w <= 0.0001f) return false;
			const f32 ndcX = clip.x / clip.w;
			const f32 ndcY = clip.y / clip.w;
			const f32 ndcZ = clip.z / clip.w;
			if (ndcZ < -1.f || ndcZ > 1.f) return false;
			const f32 u = ndcX * 0.5f + 0.5f;
			const f32 v = 1.f - (ndcY * 0.5f + 0.5f);
			out.x = imgMin.x + u * imgSize.x;
			out.y = imgMin.y + v * imgSize.y;
			outDepth = -viewP.z; // view space looks down -Z
			return true;
		};

		// Draws the glyph AND registers its on-screen rect as clickable, in
		// one place - see ViewportIcon's comment on why this is not split.
		auto drawIconAt = [&](const char* icon, const Vec3& wp, ImU32 color, f32 pxSize, uint32 sceneObjectId) {
			ImVec2 p;
			f32 depth = 0.f;
			if (!projectToImage(wp, p, depth)) return;
			// Centre outside the image: the whole icon belongs to a part of
			// the scene that isn't on screen, so drop it rather than leave a
			// clipped sliver (and a hit rect) pinned to the border.
			if (p.x < imgMin.x || p.x > imgMax.x || p.y < imgMin.y || p.y > imgMax.y) return;
			const ImVec2 sz = ImGui::CalcTextSize(icon);
			const f32 scale = pxSize / (sz.y > 0.f ? sz.y : 1.f);
			const ImVec2 topLeft(p.x - (sz.x * scale) * 0.5f, p.y - (sz.y * scale) * 0.5f);
			dl->AddText(NULL, pxSize, topLeft, color, icon);

			if (sceneObjectId == 0) return;
			// Glyphs are narrow and the rendered ink is narrower still, so a
			// tight advance-box target is genuinely hard to hit. Pad out to
			// at least a square of the icon's height, which is what the eye
			// reads as "the icon" anyway.
			const f32 drawnW = sz.x * scale, drawnH = sz.y * scale;
			const f32 halfW = (drawnW < drawnH ? drawnH : drawnW) * 0.5f + 2.f;
			const f32 halfH = drawnH * 0.5f + 2.f;
			ViewportIcon hit;
			hit.min = ImVec2(std::max(p.x - halfW, imgMin.x), std::max(p.y - halfH, imgMin.y));
			hit.max = ImVec2(std::min(p.x + halfW, imgMax.x), std::min(p.y + halfH, imgMax.y));
			if (hit.min.x >= hit.max.x || hit.min.y >= hit.max.y) return;
			hit.sceneObjectId = sceneObjectId;
			hit.viewDepth = depth;
			viewportIcons.push_back(hit);
		};

		std::vector<std::shared_ptr<GameObject>>& all = scene->GetAllGameObjectList();
		for (std::vector<std::shared_ptr<GameObject>>::iterator it = all.begin(); it != all.end(); ++it)
		{
			GameObject* go = (*it).get();
			if (!go || IsInternalGameObject(go)) continue;

			uint32 goId = sceneObjects->GetSceneObjectID(go);
			if (IsSceneCamera(goId) && go != viewCam && editorDebugDraw->IsCameraOn(go))
				drawIconAt(u8"\uf030", go->GetWorldPosition(), IM_COL32(0, 255, 255, 255), 22.f, goId);

			const std::vector<std::shared_ptr<IComponent>>& comps = go->GetComponents();
			for (std::vector<std::shared_ptr<IComponent>>::const_iterator ci = comps.begin(); ci != comps.end(); ++ci)
			{
				IComponent* c = (*ci).get();
				if (!c || !editorDebugDraw->IsOn(c)) continue;
				// Select the light component itself when it is registered as
				// its own scene object, so the Properties panel opens on the
				// light rather than on its host GameObject; fall back to the
				// GameObject when it is not.
				uint32 pickId = sceneObjects->GetSceneObjectID(c);
				if (pickId == 0) pickId = goId;
				if (dynamic_cast<DirectionalLight*>(c))
					drawIconAt(u8"\uf185", go->GetWorldPosition(), IM_COL32(255, 220, 0, 255), 18.f, pickId);
				else if (dynamic_cast<PointLight*>(c) || dynamic_cast<SpotLight*>(c))
					drawIconAt(u8"\uf0eb", go->GetWorldPosition(), IM_COL32(255, 220, 0, 255), 18.f, pickId);
			}
		}

		dl->PopClipRect();
	}

	void SceneEditor::CreateSceneCamera()
	{
		SceneObject* obj = sceneObjects->CreateGameObject("Camera");
		if (obj != NULL)
		{
			RegisterSceneCamera(obj->GetID());
			MarkSceneDirty();
			PushAddCommand(obj);
		}
	}

	std::string SceneEditor::TreeLabel(SceneObject* obj) const
	{
		if (!obj) return "";
		std::string label = EditorIcons::ForSceneObject(obj, IsSceneCamera(obj->GetID())) + obj->GetName();
#ifdef LUA_BINDINGS
		// Show a script glyph on the GO while collapsed so attach is obvious.
		if (obj->GetType() == SceneObjectTypes::GAMEOBJECT && sceneObjects)
		{
			const uint32 goId = obj->GetID();
			for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin();
				i != sceneObjects->GetList().end(); ++i)
			{
				if (i->second && i->second->GetParentID() == goId
					&& i->second->GetType() == SceneObjectTypes::LUA_COMPONENT)
				{
					label += u8"  \uf121";
					break;
				}
			}
		}
#endif
		// Prefab instances are marked in the tree, because "editing this
		// object edits every copy of it" is not something to discover by
		// accident. Deliberately only the glyph, with no in-sync/modified
		// indicator: answering that means re-serializing the subtree and
		// re-reading the prefab file (PrefabInstanceMatchesSource), and this
		// runs for every object in the tree every frame. The context menu
		// computes it once, on demand, where one click pays for it.
		if (obj->GetType() == SceneObjectTypes::GAMEOBJECT && !obj->prefabSource.empty())
			label += u8"  \uf1b3";
		return label;
	}

	void SceneEditor::BuildSceneCameraDebugList(std::vector<SceneCameraDebugEntry>& out) const
	{
		out.clear();
		for (std::map<uint32, EditorCameraSettings>::const_iterator i = sceneCameras.begin(); i != sceneCameras.end(); ++i)
		{
			SceneObject* so = sceneObjects->GetSceneObject(i->first);
			if (!so || so->GetType() != SceneObjectTypes::GAMEOBJECT) continue;
			SceneCameraDebugEntry entry;
			entry.go = (GameObject*)so->GetPTR();
			entry.settings = i->second;
			entry.isViewCamera = (i->first == activeSceneCameraId);
			out.push_back(entry);
		}
	}

	std::string SceneEditor::ModelThumbnailPath(const std::string& p3dmPath)
	{
		namespace fs = std::filesystem;
		// v2: previous framing sat too low on wide/flat meshes (island roofs only).
		return (fs::path(p3dmPath).parent_path() / ".thumbnails" / "thumb.png").string();
	}

	bool SceneEditor::RenderModelPreviewToRGBA8(const std::string& p3dmPath,
		std::vector<unsigned char>& outRGBA, uint32& outW, uint32& outH)
	{
		outRGBA.clear();
		outW = outH = 0;
		if (p3dmPath.empty() || !thumbRenderer || !thumbEffects) return false;

		// Finish any in-flight Metal/Vulkan work before we allocate textures
		// and encode a one-shot offscreen pass on a separate FBO.
		GetActiveRenderDevice().WaitIdle();

		SceneGraph* previewScene = NULL;
		bool ok = false;
		try
		{
			previewScene = new SceneGraph();

			std::shared_ptr<GameObject> modelGo = std::make_shared<GameObject>();
			std::shared_ptr<Renderable> mesh = std::make_shared<Model>(p3dmPath, true);
			if (!mesh || mesh->Geometries.empty())
			{
				delete previewScene;
				return false;
			}

			std::shared_ptr<RenderingComponent> rModel = std::make_shared<RenderingComponent>(
				mesh, ShaderUsage::Diffuse | ShaderUsage::SpecularColor);
			rModel->DisableCastShadows();
			modelGo->Add(rModel);
			previewScene->Add(modelGo);

			std::shared_ptr<GameObject> lightGo = std::make_shared<GameObject>();
			std::shared_ptr<DirectionalLight> light = std::make_shared<DirectionalLight>(
				Vec4(1.f, 1.f, 1.f, 1.f), Vec3(-0.45f, -1.f, -0.35f));
			lightGo->Add(light);
			previewScene->Add(lightGo);

			std::shared_ptr<GameObject> fillGo = std::make_shared<GameObject>();
			std::shared_ptr<DirectionalLight> fill = std::make_shared<DirectionalLight>(
				Vec4(0.4f, 0.42f, 0.5f, 1.f), Vec3(0.55f, 0.2f, 0.6f));
			fillGo->Add(fill);
			previewScene->Add(fillGo);

			std::shared_ptr<GameObject> cam = std::make_shared<GameObject>();
			previewScene->Add(cam);
			previewScene->Update(0);

			Vec3 bmin = mesh->GetBoundingMinValue();
			Vec3 bmax = mesh->GetBoundingMaxValue();
			if ((bmax - bmin).magnitudeSQR() < 1e-12f)
			{
				bmin = modelGo->GetBoundingMinValue();
				bmax = modelGo->GetBoundingMaxValue();
			}

			const Vec3 center = (bmin + bmax) * 0.5f;
			const Vec3 halfExt = (bmax - bmin) * 0.5f;
			f32 radius = halfExt.magnitude();
			if (radius < 1e-4f)
				radius = Max(mesh->GetBoundingSphereRadius(), 1.f);

			modelGo->SetPosition(Vec3(-center.x, -center.y, -center.z));
			previewScene->Update(0);

			const f32 fovDeg = 40.f;
			const f32 aspect = (f32)thumbWidth / (f32)thumbHeight;
			const f32 halfFovY = (fovDeg * 0.5f) * (f32)M_PI / 180.f;
			const f32 halfFovX = atanf(tanf(halfFovY) * aspect);
			const f32 halfFov = Min(halfFovX, halfFovY);

			const Vec3 viewDir = Vec3(1.f, 0.9f, 1.f).normalize();
			f32 dist = radius / Max(sinf(halfFov), 1e-4f);
			dist = Max(dist, Max(halfExt.x, halfExt.z) / Max(tanf(halfFov), 1e-4f));
			dist = Max(dist, halfExt.y / Max(tanf(halfFov), 1e-4f));
			dist *= 2.4f;
			if (!(dist > 1e-3f) || dist != dist)
				dist = Max(radius * 3.f, 1.f);

			const Vec3 eye = viewDir * dist;
			Matrix view;
			view.LookAt(eye, Vec3(0.f, 0.f, 0.f), Vec3::UP);
			cam->SetTransformationMatrix(view.Inverse());
			previewScene->Update(0);

			const f32 zNear = Max(0.01f, dist * 0.005f);
			const f32 zFar = Max(dist * 20.f, dist + radius * 8.f);

			Projection p;
			p.Perspective(fovDeg, aspect, zNear, zFar);

			// Fixed-size dedicated FBO — never Resize (shared preview path
			// crashed Metal when resized for thumbnails).
			thumbRenderer->SetBackground(Vec4(0.18f, 0.18f, 0.2f, 1.f));
			thumbEffects->ProcessPostEffects(&p);
			thumbRenderer->ResetViewPort();
			thumbRenderer->SetViewPort(0, 0, thumbWidth, thumbHeight);
			thumbRenderer->PreRender(cam.get(), previewScene);
			thumbRenderer->ApplyBackgroundClearColor();
			thumbEffects->CaptureFrame();
			thumbRenderer->RenderScene(p, cam.get(), previewScene);
			thumbEffects->EndCapture();

			GetActiveRenderDevice().WaitIdle();

			Texture* src = thumbEffects->GetViewportColor();
			if (src == NULL)
			{
				echo("ERROR: model thumbnail has no color target");
			}
			else
			{
				const uint32 w = src->GetWidth();
				const uint32 h = src->GetHeight();
				const uint32 srcType = src->GetDataType();
				std::vector<uchar> pixels = src->GetTextureData();
				if (!ConvertPreviewPixelsToRGBA8(pixels, srcType, w, h, outRGBA))
				{
					echo("ERROR: model thumbnail readback/convert failed");
				}
				else
				{
#if !defined(_SDL2VULKAN) && !defined(_SDL2METAL)
					FlipRGBA8Vertically(outRGBA, w, h);
#endif
					outW = w;
					outH = h;
					ok = true;
				}
			}
		}
		catch (const std::exception& e)
		{
			echo(std::string("ERROR: model thumbnail failed: ") + e.what());
		}
		catch (...)
		{
			echo("ERROR: model thumbnail failed");
		}

		GetActiveRenderDevice().WaitIdle();
		delete previewScene;
		return ok;
	}

	std::string SceneEditor::EnsureModelThumbnail(const std::string& p3dmPath, bool force)
	{
		namespace fs = std::filesystem;
		if (p3dmPath.empty()) return std::string();

		const std::string thumbPath = ModelThumbnailPath(p3dmPath);
		std::error_code ec;
		if (!force && fs::exists(thumbPath, ec) && fs::is_regular_file(thumbPath, ec))
			return thumbPath;

		std::vector<unsigned char> rgba;
		uint32 w = 0, h = 0;
		if (!RenderModelPreviewToRGBA8(p3dmPath, rgba, w, h) || rgba.empty())
			return std::string();

		fs::create_directories(fs::path(thumbPath).parent_path(), ec);
		if (ec)
		{
			echo("ERROR: could not create .thumbnails folder: " + ec.message());
			return std::string();
		}

		if (!stbi_write_png(thumbPath.c_str(), (int)w, (int)h, 4, rgba.data(), (int)w * 4))
		{
			echo("ERROR: failed writing model thumbnail: " + DisplayPath(thumbPath));
			return std::string();
		}
		return thumbPath;
	}

	void SceneEditor::QueueModelThumbnail(const std::string& p3dmPath, bool force)
	{
		namespace fs = std::filesystem;
		if (p3dmPath.empty()) return;
		std::error_code ec;
		if (!force)
		{
			const std::string thumbPath = ModelThumbnailPath(p3dmPath);
			if (fs::exists(thumbPath, ec) && fs::is_regular_file(thumbPath, ec))
				return;
			if (pendingModelThumbnailSet.count(p3dmPath))
				return;
		}
		pendingModelThumbnails.push_back(std::make_pair(p3dmPath, force));
		pendingModelThumbnailSet.insert(p3dmPath);
	}

	void SceneEditor::QueueMissingProjectModelThumbnails()
	{
		namespace fs = std::filesystem;
		if (!project || !project->IsOpen()) return;

		const fs::path modelsRoot = project->ModelsPath();
		std::error_code ec;
		if (!fs::exists(modelsRoot, ec)) return;

		for (fs::recursive_directory_iterator it(modelsRoot, ec), end; it != end && !ec; it.increment(ec))
		{
			if (!it->is_regular_file(ec)) continue;
			const fs::path p = it->path();
			if (p.extension() != ".p3dm") continue;

			bool underThumb = false;
			for (fs::path cur = p.parent_path(); !cur.empty() && cur != modelsRoot; cur = cur.parent_path())
			{
				if (cur.filename() == ".thumbnails") { underThumb = true; break; }
			}
			if (underThumb) continue;
			QueueModelThumbnail(p.string(), false);
		}
	}

	void SceneEditor::ProcessPendingModelThumbnails(int maxPerFrame)
	{
		if (maxPerFrame < 1) maxPerFrame = 1;
		int done = 0;
		while (done < maxPerFrame && !pendingModelThumbnails.empty())
		{
			const std::pair<std::string, bool> job = pendingModelThumbnails.front();
			pendingModelThumbnails.erase(pendingModelThumbnails.begin());
			pendingModelThumbnailSet.erase(job.first);
			const std::string path = EnsureModelThumbnail(job.first, job.second);
			if (!path.empty())
				echo("Thumbnail: " + DisplayPath(path));
			++done;
		}
	}

	Texture* SceneEditor::RenderCameraPreview(GameObject* camGO)
	{
		if (!camGO || !previewRenderer || !previewEffects) return NULL;
		uint32 id = sceneObjects->GetSceneObjectID(camGO);
		if (!IsSceneCamera(id)) return NULL;

		IRenderer::InvalidateSharedUniformCaches();

		EditorCameraSettings& s = sceneCameras[id];
		Projection p;
		if (s.orthographic)
		{
			const f32 halfH = s.orthoSize;
			const f32 halfW = halfH * ((f32)previewWidth / (f32)previewHeight);
			p.Ortho(-halfW, halfW, -halfH, halfH, s.nearPlane, s.farPlane);
		}
		else
			p.Perspective(s.fov, (f32)previewWidth / (f32)previewHeight, s.nearPlane, s.farPlane);

		// Hide editor chrome without SceneGraph::Remove — that UnregisterComponents
		// every ImGui frame (helpers/grid/editor cameras) and left shared renderer
		// state / registration flags in a bad place after leaving camera selection.
		std::vector<RenderingComponent*> disabled;
		if (rGrid && rGrid->IsActive())
		{
			rGrid->Disable();
			disabled.push_back(rGrid.get());
		}
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin();
			i != sceneObjects->GetList().end(); ++i)
		{
			if ((*i).second == NULL || !(*i).second->Helper) continue;
			IHelper* helper = (IHelper*)(*i).second->Helper.get();
			if (helper && helper->rcomp && helper->rcomp->IsActive())
			{
				helper->rcomp->Disable();
				disabled.push_back(helper->rcomp.get());
			}
		}

		previewEffects->ProcessPostEffects(&p);
		previewEffects->Resize(previewWidth, previewHeight);
		previewRenderer->Resize(previewWidth, previewHeight);
		previewRenderer->ResetViewPort();
		previewRenderer->SetViewPort(0, 0, previewWidth, previewHeight);
		previewRenderer->PreRender(camGO, scene);
		previewRenderer->ApplyBackgroundClearColor();
		previewEffects->CaptureFrame();
		previewRenderer->RenderScene(p, camGO, scene);
		previewEffects->EndCapture();

		for (size_t i = 0; i < disabled.size(); ++i)
			disabled[i]->Enable();

#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
		// Preview and the main viewport share one GlobalMatrices UBO. Without
		// waiting here the main pass can overwrite view/proj on the GPU while
		// preview draws are still in flight — alternating cameras in the preview.
		GetActiveRenderDevice().WaitIdle();
#endif
		IRenderer::InvalidateSharedUniformCaches();
		return previewEffects->GetViewportColor();
	}

	GameObject* SceneEditor::GetSelectedOwnerGameObject() const
	{
		if (SelectedSceneObject == NULL) return NULL;
		switch (SelectedSceneObject->GetType())
		{
		case SceneObjectTypes::GAMEOBJECT:
			return (GameObject*)SelectedSceneObject->GetPTR();
		case SceneObjectTypes::RENDERING_COMPONENT:
		case SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT:
		case SceneObjectTypes::POINTLIGHT_COMPONENT:
		case SceneObjectTypes::SPOTLIGHT_COMPONENT:
		case SceneObjectTypes::PHYSICS_COMPONENT:
		case SceneObjectTypes::AUDIO_SOURCE_COMPONENT:
		case SceneObjectTypes::PARTICLE_SYSTEM_COMPONENT:
		case SceneObjectTypes::LUA_COMPONENT:
		{
			IComponent* c = (IComponent*)SelectedSceneObject->GetPTR();
			return c ? c->GetOwner() : NULL;
		}
		default:
			return NULL;
		}
	}

#ifdef LUA_BINDINGS
	void SceneEditor::PushLuaHostGlobals()
	{
		if (!sharedLua) return;
		(*sharedLua)["physics"] = static_cast<IPhysics*>(physics);
		(*sharedLua)["scene"] = scene;

		// Expose the active scene camera so game scripts can use it for audio/shake.
		GameObject* luaCamera = nullptr;
		if (activeSceneCameraId != 0)
		{
			SceneObject* so = sceneObjects->GetSceneObject(activeSceneCameraId);
			if (so && so->GetType() == SceneObjectTypes::GAMEOBJECT)
				luaCamera = static_cast<GameObject*>(so->GetPTR());
		}
		if (!luaCamera && !sceneCameras.empty())
		{
			for (auto& kv : sceneCameras)
			{
				SceneObject* so = sceneObjects->GetSceneObject(kv.first);
				if (so && so->GetType() == SceneObjectTypes::GAMEOBJECT)
				{
					luaCamera = static_cast<GameObject*>(so->GetPTR());
					break;
				}
			}
		}
		if (!luaCamera) luaCamera = Camera.get();
		(*sharedLua)["camera"] = luaCamera;

		// Expose setRenderCamera() so scripts can override which camera renders the viewport.
		SceneEditor* self = this;
		(*sharedLua)["setRenderCamera"] = [self](GameObject* go) { self->SetScriptRenderCamera(go); };

		// Scene transitions. Queued, not immediate: the caller is running
		// inside a LuaComponent owned by the scene graph this tears down,
		// so switching here would free the running script. Applied between
		// frames by ApplyPendingSceneLoadIfAny(). Takes a bare scene name
		// ("Level2") or an explicit project-relative .json path.
		(*sharedLua)["loadScene"] = [self](const std::string& name) { self->pendingLoadSceneName = name; };
		// Which scene is running, for a boot script that branches on it.
		(*sharedLua)["currentScene"] = [self]() { return self->GetSceneDisplayName(); };
		// Expose echo() so Lua scripts can write to the editor log window.
		(*sharedLua)["echo"] = [](const std::string& msg) { p3d::LOG::_LOG::_echo(msg); };

		// Expose which renderer the editor viewport is actually using, so
		// scripts that build their own materials/usage flags based on
		// "am I under a deferred renderer" (e.g. NeonPulse's Arena.build())
		// can match reality instead of guessing/hardcoding - a mismatch here
		// means a material never gets flagged transparent/gbuffer-compiled
		// the way the *actual* active renderer (this one, not whatever the
		// script assumes) needs it to be.
		(*sharedLua)["editorRendererType"] = usingDeferredRenderer ? std::string("deferred") : std::string("forward");

		if (project && project->IsOpen())
		{
			std::string assets = project->AssetsPath();
			if (!assets.empty() && assets.back() != '/' && assets.back() != '\\')
				assets += "/";
			(*sharedLua)["ASSETS_PATH"] = assets;
		}
	}

	void SceneEditor::ResetLuaComponentsLifecycle()
	{
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin();
			i != sceneObjects->GetList().end(); ++i)
		{
			if (!i->second || i->second->GetType() != SceneObjectTypes::LUA_COMPONENT) continue;
			LuaComponent* lc = (LuaComponent*)i->second->GetPTR();
			if (lc) lc->ResetLifecycle();
		}
	}

	bool SceneEditor::AttachLuaScriptToGameObject(uint32 goId, const std::string& absoluteScriptPath)
	{
		if (playMode || editorDisabled) return false;
		if (!sharedLua)
		{
			echo("ERROR: Lua host not available (build with HAVE_LUA_BINDINGS)");
			return false;
		}
		SceneObject* goObj = sceneObjects->GetSceneObject(goId);
		if (!goObj || goObj->GetType() != SceneObjectTypes::GAMEOBJECT)
		{
			echo("ERROR: Select a GameObject to attach a script");
			return false;
		}
		GameObject* go = (GameObject*)goObj->GetPTR();
		if (!go || IsInternalGameObject(go)) return false;
		if (absoluteScriptPath.empty())
		{
			echo("ERROR: Script path is empty");
			return false;
		}

		// Captured before attaching so a successful attach can be pushed as
		// one ReplaceGameObjectCommand (same mechanism as AddFormSubmit's
		// attach-to-existing-GameObject path / DeleteComponentById).
		std::string beforeSnapshot = SnapshotSubtree(goId);

		PushLuaHostGlobals();
		std::shared_ptr<LuaComponent> comp;
		try {
			comp = LuaComponent_FromFile(*sharedLua, absoluteScriptPath);
		}
		catch (const std::exception& e) {
			echo(std::string("ERROR: Failed to load script: ") + e.what());
			return false;
		}
		catch (...) {
			echo("ERROR: Failed to load script");
			return false;
		}
		if (!comp)
		{
			// Surface the real Lua error instead of a generic message.
			sol::load_result chunk = sharedLua->load_file(absoluteScriptPath);
			if (!chunk.valid())
			{
				sol::error err = chunk;
				echo(std::string("ERROR: Script load failed: ") + err.what());
			}
			else
			{
				sol::protected_function_result loaded = chunk();
				if (!loaded.valid())
				{
					sol::error err = loaded;
					echo(std::string("ERROR: Script runtime error: ") + err.what());
				}
				else
					echo("ERROR: Script must return a middleclass class with :new() — " + DisplayPath(absoluteScriptPath));
			}
			return false;
		}

		SceneObject* so = sceneObjects->CreateLuaComponent(go, comp);
		if (!so)
		{
			echo("ERROR: Could not attach script");
			return false;
		}
		MarkSceneDirty();
		// Keep the GameObject selected and expand it so the new script child
		// is visible — otherwise attach feels like selection vanished.
		SelectSceneObject(goObj);
		selection.clear();
		selection.push_back(goId);
		node_clicked = -1;
		hierarchyForceOpenId = goId;
		echo("SUCCESS: Attached script " + DisplayPath(absoluteScriptPath) + " to " + goObj->GetName());
		if (!beforeSnapshot.empty())
			PushReplaceCommand(goId, beforeSnapshot, "Attach Script");
		return true;
	}

	bool SceneEditor::DebugAutoAttachScript(const std::string& absoluteScriptPath)
	{
		uint32 goId = 0;
		for (std::map<uint32, EditorCameraSettings>::const_iterator i = sceneCameras.begin();
			i != sceneCameras.end(); ++i)
		{
			SceneObject* so = sceneObjects->GetSceneObject(i->first);
			if (so && so->GetType() == SceneObjectTypes::GAMEOBJECT)
			{
				goId = i->first;
				break;
			}
		}
		if (goId == 0)
		{
			for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin();
				i != sceneObjects->GetList().end(); ++i)
			{
				if (!i->second || i->second->GetType() != SceneObjectTypes::GAMEOBJECT) continue;
				GameObject* go = (GameObject*)i->second->GetPTR();
				if (!go || IsInternalGameObject(go)) continue;
				goId = i->first;
				break;
			}
		}
		if (goId == 0)
		{
			echo("ERROR: DebugAutoAttachScript - no GameObject");
			return false;
		}
		return AttachLuaScriptToGameObject(goId, absoluteScriptPath);
	}

	void SceneEditor::DrawScriptAssetPicker(const char* id, std::string& pathBuf)
	{
		const char* preview = pathBuf.empty() ? "(select script…)" : pathBuf.c_str();
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::BeginCombo(id, preview))
		{
			if (ImGui::Selectable("(none)", pathBuf.empty()))
				pathBuf.clear();
			if (project && project->IsOpen())
			{
				std::vector<ProjectAssetEntry> scripts;
				project->ListAssets("assets/lua", scripts, true);
				bool any = false;
				for (size_t si = 0; si < scripts.size(); ++si)
				{
					const ProjectAssetEntry& e = scripts[si];
					if (e.isDirectory || !ProjectManager::IsLuaExtension(e.relativePath))
						continue;
					if (ProjectManager::IsSceneLuaScript(e.relativePath)
						|| ProjectManager::IsInternalAssetPath(e.relativePath))
						continue;
					any = true;
					const bool selected = (pathBuf == e.relativePath);
					if (ImGui::Selectable(e.relativePath.c_str(), selected))
						pathBuf = e.relativePath;
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				if (!any)
					ImGui::TextDisabled("No .lua files in assets/lua");
			}
			else
				ImGui::TextDisabled("Open a project first");
			ImGui::EndCombo();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_REL"))
			{
				const char* rel = (const char*)payload->Data;
				if (rel && ProjectManager::IsLuaExtension(rel)
					&& !ProjectManager::IsSceneLuaScript(rel)
					&& !ProjectManager::IsInternalAssetPath(rel))
					pathBuf = rel;
			}
			ImGui::EndDragDropTarget();
		}
	}

	// Every UI field edits live and undoes as one snapshot/replace pair -
	// the same mechanism the add-component ops use. Per-field Apply* ops
	// would be a dozen near-identical functions for properties that are all
	// just "set this value on that component"; snapshotting the subtree when
	// a widget is grabbed and pushing the replace when it is released covers
	// all of them, including the ones that rebuild geometry.
	void SceneEditor::BeginComponentUndo(uint32 goId)
	{
		if (!ImGui::IsItemActivated()) return;
		componentUndoBefore = SnapshotSubtree(goId);
	}

	void SceneEditor::EndComponentUndo(uint32 goId, const char* what)
	{
		if (!ImGui::IsItemDeactivatedAfterEdit() || componentUndoBefore.empty()) return;
		PushReplaceCommand(goId, componentUndoBefore, what);
		componentUndoBefore.clear();
	}

	void SceneEditor::BeginUIUndo(uint32 goId)
	{
		if (!ImGui::IsItemActivated()) return;
		SceneObject* obj = sceneObjects->GetSceneObject(goId);
		if (obj && obj->GetType() == SceneObjectTypes::GAMEOBJECT)
			uiUndoBefore = CaptureUIProperties((GameObject*)obj->GetPTR());
	}

	void SceneEditor::EndUIUndo(uint32 goId, const char* what)
	{
		if (!ImGui::IsItemDeactivatedAfterEdit() || uiUndoBefore.empty()) return;
		SceneObject* obj = sceneObjects->GetSceneObject(goId);
		if (obj && obj->GetType() == SceneObjectTypes::GAMEOBJECT)
			PushUIPropertyUndo(goId, uiUndoBefore, CaptureUIProperties((GameObject*)obj->GetPTR()), what);
		uiUndoBefore = json();
	}

	void SceneEditor::DrawUIComponentProperties(GameObject* go, uint32 goId)
	{
		if (!go) return;
		const std::vector<std::shared_ptr<IComponent> >& comps = go->GetComponents();

		for (size_t i = 0; i < comps.size(); i++)
		{
			if (!comps[i]) continue;
			const uint32 type = comps[i]->GetComponentType();
			// The 2D component branches below (Occluder2D, Physics2D,
			// Layer2D) were written but unreachable: this filter let only UI
			// types through and `continue`d before ever reaching them, so
			// selecting a layer or a body showed an empty Properties panel.
			// RenderingComponent is here for the Sprite Animation section.
			if (type != ComponentType::UICanvas && type != ComponentType::UIRect
				&& type != ComponentType::UIImage && type != ComponentType::UIText
				&& type != ComponentType::UIButton
				&& type != ComponentType::Occluder2D && type != ComponentType::Physics2D
				&& type != ComponentType::Layer2D && type != ComponentType::RenderingComponent)
				continue;

			ImGui::PushID((int)i);
			ImGui::Separator();

			if (type == ComponentType::RenderingComponent)
			{
				RenderingComponent* rc = static_cast<RenderingComponent*>(comps[i].get());
				Vec2 pv;
				if (GetSpritePivot(rc, pv) && ImGui::CollapsingHeader("Pivot"))
				{
					// Normalized over the geometry's own bounds, so the
					// numbers mean the same thing whatever the sprite's size
					// or aspect: (0.5,0.5) middle, (0.5,0) bottom edge.
					f32 v[2] = { pv.x, pv.y };
					const std::string b = SnapshotSubtree(goId);
					bool changed = ImGui::DragFloat2("Pivot", v, 0.01f, -1.f, 2.f);
					if (changed)
					{
						std::string err;
						OpSetSpritePivot(goId, Vec2(v[0], v[1]), err);
					}
					if (ImGui::IsItemDeactivatedAfterEdit())
						PushReplaceCommand(goId, b, "Set Pivot");

					// Bottom-centre first: it is what a standing character
					// wants, and what makes a limb rotate about its joint.
					struct P { const char* name; f32 x, y; };
					static const P presets[] = {
						{ "Bottom", 0.5f, 0.f }, { "Centre", 0.5f, 0.5f },
						{ "Top", 0.5f, 1.f }, { "Left", 0.f, 0.5f }, { "Right", 1.f, 0.5f },
					};
					for (int pi = 0; pi < 5; pi++)
					{
						if (pi) ImGui::SameLine();
						if (ImGui::SmallButton(presets[pi].name))
						{
							const std::string pb = SnapshotSubtree(goId);
							std::string err;
							if (OpSetSpritePivot(goId, Vec2(presets[pi].x, presets[pi].y), err))
								PushReplaceCommand(goId, pb, "Set Pivot");
						}
					}
				}
				if (ImGui::CollapsingHeader("Sprite Animation"))
				{
					TextureAnimationInstance* inst =
						static_cast<TextureAnimationInstance*>(rc->GetActiveTextureAnimation());
					if (inst && inst->GetOwner())
					{
						ImGui::Text("%u frames, frame %u", inst->GetOwner()->GetNumberFrames(), inst->GetFrame());
						f32 fps = inst->GetFrameSpeed();
						if (ImGui::DragFloat("FPS", &fps, 0.5f, 0.1f, 120.f))
						{ inst->SetFrameSpeed(fps); MarkSceneDirty(); }
						bool yo = inst->IsYoyo();
						if (ImGui::Checkbox("Ping-pong", &yo)) { inst->YoYo(yo); MarkSceneDirty(); }
						ImGui::SameLine();
						if (inst->IsPaused()) { if (ImGui::Button("Resume")) inst->Play(inst->IsLooping() ? -1 : inst->GetRepeat()); }
						else { if (ImGui::Button("Pause")) inst->Pause(); }
					}
					else ImGui::TextDisabled("No animation on this sprite yet.");

					ImGui::Separator();
					ImGui::InputText("Sheet", &propertiesSheetPath);
					ImGui::DragInt("Columns", &propertiesSheetCols, 0.2f, 1, 64);
					ImGui::DragInt("Rows", &propertiesSheetRows, 0.2f, 1, 64);
					ImGui::DragFloat("Slice FPS", &propertiesSheetFps, 0.5f, 0.1f, 120.f);
					if (ImGui::Button("Slice Spritesheet"))
					{
						std::string err;
						if (!OpSliceSpritesheet(goId, propertiesSheetPath, propertiesSheetCols,
							propertiesSheetRows, propertiesSheetFps, true, err))
							echo("ERROR: " + err);
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Cuts the sheet into cols x rows PNGs beside it and plays\nthem. Written as real files so the scene stores paths\nrather than embedding every frame.");
				}
			}
			else if (type == ComponentType::Occluder2D)
			{
				Occluder2D* oc = static_cast<Occluder2D*>(comps[i].get());
				ImGui::Text("Occluder 2D");

				int sh = (int)oc->GetShapeType();
				const char* shapes[] = { "Box", "Circle" };
				{
					// Combos and checkboxes report once, so the snapshot is
					// taken up front and pushed on the same frame; only drags
					// need the Begin/End pairing below.
					const std::string b = SnapshotSubtree(goId);
					if (ImGui::Combo("Shape", &sh, shapes, 2))
					{
						oc->SetShapeType((uint32)sh);
						MarkSceneDirty();
						PushReplaceCommand(goId, b, "Set Occluder Shape");
					}
				}

				Vec2 sz = oc->GetSize();
				BeginComponentUndo(goId);
				if (ImGui::DragFloat2("Half Extents", (float*)&sz, 0.01f, 0.001f, 1000.f))
				{
					oc->SetSize(sz);
					MarkSceneDirty();
				}
				EndComponentUndo(goId, "Set Occluder Size");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Half-extents, matching Physics2D - a 1x1 box is 0.5,\n0.5. For a circle only X is used, as the radius.");

				bool en = oc->IsEnabled();
				{
					const std::string b = SnapshotSubtree(goId);
					if (ImGui::Checkbox("Enabled", &en))
					{
						oc->SetEnabled(en);
						MarkSceneDirty();
						PushReplaceCommand(goId, b, "Toggle Occluder");
					}
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Turns the shadow off without removing the component.");

				ImGui::TextDisabled("Blocks 2D light. No physics involved.");
			}
			else if (type == ComponentType::Physics2D)
			{
				Physics2D* ph = static_cast<Physics2D*>(comps[i].get());
				ImGui::Text("Physics 2D");

				int bt = (int)ph->GetBodyType();
				const char* bodyTypes[] = { "Static", "Kinematic", "Dynamic" };
				{
					const std::string b = SnapshotSubtree(goId);
					if (ImGui::Combo("Body", &bt, bodyTypes, 3))
					{ ph->SetBodyType((uint32)bt); MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Body Type"); }
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Static never moves (the ground). Kinematic is moved by\nscript and pushes others but ignores forces. Dynamic is\nmoved by the solver.");

				int sh = (int)ph->GetShapeType();
				const char* shapes[] = { "Box", "Circle" };
				{
					const std::string b = SnapshotSubtree(goId);
					if (ImGui::Combo("Shape", &sh, shapes, 2))
					{ ph->SetShapeType((uint32)sh); MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Body Shape"); }
				}

				Vec2 sz = ph->GetSize();
				BeginComponentUndo(goId);
				if (ImGui::DragFloat2("Half Extents", (float*)&sz, 0.01f, 0.001f, 1000.f))
				{
					ph->SetSize(sz);
					MarkSceneDirty();
				}
				EndComponentUndo(goId, "Set Body Size");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Half-extents, because that is what Box2D takes - a 1x1\nbox is 0.5, 0.5. For a circle only X is used, as the\nradius.");

				f32 d = ph->GetDensity();
				BeginComponentUndo(goId);
				if (ImGui::DragFloat("Density", &d, 0.05f, 0.f, 100.f)) { ph->SetDensity(d); MarkSceneDirty(); }
				EndComponentUndo(goId, "Set Density");
				f32 fr = ph->GetFriction();
				BeginComponentUndo(goId);
				if (ImGui::DragFloat("Friction", &fr, 0.01f, 0.f, 1.f)) { ph->SetFriction(fr); MarkSceneDirty(); }
				EndComponentUndo(goId, "Set Friction");
				f32 re = ph->GetRestitution();
				BeginComponentUndo(goId);
				if (ImGui::DragFloat("Bounciness", &re, 0.01f, 0.f, 1.f)) { ph->SetRestitution(re); MarkSceneDirty(); }
				EndComponentUndo(goId, "Set Bounciness");

				bool fx = ph->IsFixedRotation();
				{
					const std::string b = SnapshotSubtree(goId);
					if (ImGui::Checkbox("Fixed Rotation", &fx))
					{ ph->SetFixedRotation(fx); MarkSceneDirty(); PushReplaceCommand(goId, b, "Toggle Fixed Rotation"); }
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Stops the body tipping over - what almost every\nplatformer character wants.");

				ImGui::TextDisabled("Edits apply on the next play; the body is built then.");
			}
			else if (type == ComponentType::Layer2D)
			{
				Layer2D* l = static_cast<Layer2D*>(comps[i].get());
				ImGui::Text("Layer 2D");

				Vec2 par = l->GetParallax();
				BeginComponentUndo(goId);
				if (ImGui::DragFloat2("Parallax", (float*)&par, 0.01f, 0.f, 4.f))
				{
					l->SetParallax(par);
					MarkSceneDirty();
				}
				EndComponentUndo(goId, "Set Parallax");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("1 = moves with the camera, 0 = pinned on screen,\n0.5 = half speed (further away). The two axes are\nindependent, so a sky can scroll sideways and stay\nput vertically.");

				bool vis = l->IsVisible();
				{
					const std::string b = SnapshotSubtree(goId);
					if (ImGui::Checkbox("Visible", &vis))
					{
						l->SetVisible(vis);
						MarkSceneDirty();
						PushReplaceCommand(goId, b, "Toggle Layer Visible");
					}
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Hides every mesh under this layer without unparenting\nor deleting anything.");

				ImGui::TextDisabled("Draw order is this object's Z.");
			}
			else if (type == ComponentType::UICanvas)
			{
				UICanvas* c = static_cast<UICanvas*>(comps[i].get());
				ImGui::Text("UI Canvas");
				Vec2 ref = c->GetReferenceResolution();
				if (ImGui::DragFloat2("Reference", (float*)&ref, 1.f, 1.f, 16384.f))
				{
					c->SetReferenceResolution(ref.x, ref.y);
					MarkSceneDirty();
				}
				BeginUIUndo(goId); EndUIUndo(goId, "Set Canvas Reference");

				int mode = (int)c->GetScaleMode();
				const char* modes[] = { "Constant Pixel", "Match Width", "Match Height", "Stretch" };
				if (ImGui::Combo("Scale Mode", &mode, modes, 4))
				{
					const json uiBefore = CaptureUIProperties(go);
					c->SetScaleMode((uint32)mode);
					MarkSceneDirty();
					PushUIPropertyUndo(goId, uiBefore, CaptureUIProperties(go), "Set Canvas Scale Mode");
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("How canvas units map to screen pixels. Match Width keeps the\nauthored width exactly and lets height follow the real aspect.");

				int order = c->GetSortOrder();
				if (ImGui::DragInt("Sort Order", &order, 1.f, -1000, 1000))
				{
					c->SetSortOrder(order);
					MarkSceneDirty();
				}
				BeginUIUndo(goId); EndUIUndo(goId, "Set Canvas Sort Order");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Canvases are drawn in ascending order, so a pause overlay above\na HUD is one number rather than a hierarchy edit.");

				const UIRectValue& solved = c->GetCanvasRect();
				ImGui::TextDisabled("Solved: %.0f x %.0f units", solved.width, solved.height);
			}
			else if (type == ComponentType::UIRect)
			{
				UIRect* r = static_cast<UIRect*>(comps[i].get());
				ImGui::Text("UI Rect");

				Vec2 aMin = r->GetAnchorMin(), aMax = r->GetAnchorMax();
				Vec2 oMin = r->GetOffsetMin(), oMax = r->GetOffsetMax();
				Vec2 pivot = r->GetPivot();

				// The four common cases spelled out, because deriving them
				// from raw anchors is the part of this model people get
				// wrong. Each one only rewrites the anchors, leaving the
				// offsets to be read in their new meaning.
				if (ImGui::Button("Top Left")) { const json b = CaptureUIProperties(go); r->SetAnchors(Vec2(0,0), Vec2(0,0)); MarkSceneDirty(); PushUIPropertyUndo(goId, b, CaptureUIProperties(go), "Set Anchors"); }
				ImGui::SameLine();
				if (ImGui::Button("Center")) { const json b = CaptureUIProperties(go); r->SetAnchors(Vec2(0.5f,0.5f), Vec2(0.5f,0.5f)); MarkSceneDirty(); PushUIPropertyUndo(goId, b, CaptureUIProperties(go), "Set Anchors"); }
				ImGui::SameLine();
				if (ImGui::Button("Stretch X")) { const json b = CaptureUIProperties(go); r->SetAnchors(Vec2(0,aMin.y), Vec2(1,aMax.y)); MarkSceneDirty(); PushUIPropertyUndo(goId, b, CaptureUIProperties(go), "Set Anchors"); }
				ImGui::SameLine();
				if (ImGui::Button("Fill")) { const json b = CaptureUIProperties(go); r->SetAnchors(Vec2(0,0), Vec2(1,1)); MarkSceneDirty(); PushUIPropertyUndo(goId, b, CaptureUIProperties(go), "Set Anchors"); }

				bool changed = false;
				changed |= ImGui::DragFloat2("Anchor Min", (float*)&aMin, 0.01f, 0.f, 1.f);
				BeginUIUndo(goId); EndUIUndo(goId, "Set Anchor Min");
				changed |= ImGui::DragFloat2("Anchor Max", (float*)&aMax, 0.01f, 0.f, 1.f);
				BeginUIUndo(goId); EndUIUndo(goId, "Set Anchor Max");
				const bool pinnedX = (aMin.x == aMax.x), pinnedY = (aMin.y == aMax.y);
				changed |= ImGui::DragFloat2(pinnedX && pinnedY ? "Position" : "Offset Min", (float*)&oMin, 1.f);
				BeginUIUndo(goId); EndUIUndo(goId, "Set Offset Min");
				changed |= ImGui::DragFloat2(pinnedX && pinnedY ? "Size" : "Offset Max", (float*)&oMax, 1.f);
				BeginUIUndo(goId); EndUIUndo(goId, "Set Offset Max");
				changed |= ImGui::DragFloat2("Pivot", (float*)&pivot, 0.01f, 0.f, 1.f);
				BeginUIUndo(goId); EndUIUndo(goId, "Set Pivot");

				if (changed)
				{
					r->SetAnchors(aMin, aMax);
					r->SetOffsets(oMin, oMax);
					r->SetPivot(pivot);
					MarkSceneDirty();
				}
				ImGui::TextDisabled(pinnedX && pinnedY
					? "Pinned: offsets read as position and size."
					: "Stretched: offsets read as insets from the anchor edges.");
				const UIRectValue& solved = r->GetRect();
				ImGui::TextDisabled("Solved: %.0f, %.0f  %.0f x %.0f", solved.x, solved.y, solved.width, solved.height);

				// ---- style ----
				// On the rect because that is where the reference lives, and
				// because every element has one - an image and a text on the
				// same object share a style rather than arguing over two.
				ImGui::Spacing();
				const std::string currentRef = r->GetStyleRef();
				std::string currentLabel = currentRef.empty() ? std::string("(none)")
					: std::filesystem::path(currentRef).stem().string();
				if (ImGui::BeginCombo("Style", currentLabel.c_str()))
				{
					const std::vector<std::string> styles = ListUIStyles();
					if (styles.empty())
						ImGui::TextDisabled("No .uistyle in assets/ui yet");
					for (size_t sIdx = 0; sIdx < styles.size(); sIdx++)
					{
						const std::string stem = std::filesystem::path(styles[sIdx]).stem().string();
						if (ImGui::Selectable(stem.c_str(), styles[sIdx] == currentRef))
						{
							std::string err;
							if (!OpApplyUIStyle(goId, styles[sIdx], err))
								echo("ERROR: " + err);
						}
					}
					ImGui::EndCombo();
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Applies the style's look and remembers the link, so later\nedits to the file - or a palette swap - reach this element.\nStyles never carry text or layout.");

				if (!currentRef.empty())
				{
					ImGui::SameLine();
					if (ImGui::SmallButton("Unlink"))
					{
						std::string err;
						if (!OpClearUIStyle(goId, err)) echo("ERROR: " + err);
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Keeps the look, stops following the file.");

					// Anything hand-edited since the style was applied is
					// held back from every re-apply, so it has to be visible
					// - an element quietly ignoring half its style would be
					// worse than one that never followed it.
					const size_t nOverrides = r->GetStyleOverrides().size();
					if (nOverrides > 0)
					{
						ImGui::TextDisabled("%zu propert%s overridden", nOverrides, nOverrides == 1 ? "y" : "ies");
						if (ImGui::IsItemHovered())
						{
							std::string list;
							for (size_t k = 0; k < r->GetStyleOverrides().size(); k++)
								list += (k ? ", " : "") + r->GetStyleOverrides()[k];
							ImGui::SetTooltip("Changed by hand, so the style no longer sets them:\n%s", list.c_str());
						}
						ImGui::SameLine();
						if (ImGui::SmallButton("Revert"))
						{
							std::string err;
							if (!OpRevertUIStyle(goId, err)) echo("ERROR: " + err);
						}
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("Drops those edits and puts the element back under the style.");
					}
				}

				ImGui::SetNextItemWidth(140.f);
				ImGui::InputTextWithHint("##stylename", "new style name", &uiStyleNameBuf);
				ImGui::SameLine();
				if (ImGui::Button("Extract"))
				{
					std::string outPath, err;
					if (OpExtractUIStyle(goId, uiStyleNameBuf, outPath, err))
					{
						echo("Wrote " + outPath + " and linked '" + go->GetName() + "' to it");
						uiStyleNameBuf.clear();
					}
					else echo("ERROR: " + err);
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Promotes this element's current look into assets/ui and links\nit. Colours matching a palette entry are written back as @names.");
			}
			else if (type == ComponentType::UIImage)
			{
				UIImage* img = static_cast<UIImage*>(comps[i].get());
				ImGui::Text("UI Image");

				Vec4 tint = img->GetTint();
				if (ImGui::ColorEdit4("Tint", (float*)&tint)) { img->SetTint(tint); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Image Tint");

				Vec4 border = img->GetBorder();
				if (ImGui::DragFloat4("9-Slice", (float*)&border, 1.f, 0.f, 4096.f)) { img->SetBorder(border); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Image Border");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Left, top, right, bottom, in source pixels. Corners keep their\nsize at any element size; only the edges stretch. All zero is a\nplain quad.");

				bool showDir = false;
				std::string texPath = uiTexturePickerPath;
				ImGui::FilePath("Texture", "", "png,jpg,jpeg,tga,bmp", &uiTexturePickerPath, 1024, &showDir);
				if (uiTexturePickerPath != texPath && !uiTexturePickerPath.empty())
				{
					const json b = CaptureUIProperties(go);
					const std::string rel = ImportParticleTexture(uiTexturePickerPath);
					std::shared_ptr<Texture> t = std::make_shared<Texture>();
					if (t->LoadTexture(ResolveAssetPath(rel), TextureType::Texture))
					{
						t->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
						t->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
						img->SetTexture(t);
						MarkSceneDirty();
						PushUIPropertyUndo(goId, b, CaptureUIProperties(go), "Set Image Texture");
					}
					else echo("WARNING: could not load " + rel);
					uiTexturePickerPath.clear();
				}
				if (img->GetTexture() && !img->GetTexture()->GetFilename().empty())
				{
					ImGui::TextDisabled("%s", DisplayPath(img->GetTexture()->GetFilename()).c_str());
					if (ImGui::Button("Clear Texture"))
					{
						const json b = CaptureUIProperties(go);
						img->SetTexture(std::shared_ptr<Texture>());
						MarkSceneDirty();
						PushUIPropertyUndo(goId, b, CaptureUIProperties(go), "Clear Image Texture");
					}
				}
				else ImGui::TextDisabled("No texture - a flat tinted rectangle.");
			}
			else if (type == ComponentType::UIButton || type == ComponentType::UIToggle
				|| type == ComponentType::UIMenuItem)
			{
				// A toggle is a button (see UIToggle), so it gets the same
				// panel - states, transition, click handler - with its own
				// value and grouping on top.
				UIButton* b = static_cast<UIButton*>(comps[i].get());
				UIToggle* tg = (type == ComponentType::UIToggle) ? static_cast<UIToggle*>(comps[i].get()) : NULL;
				UIMenuItem* mi = (type == ComponentType::UIMenuItem) ? static_cast<UIMenuItem*>(comps[i].get()) : NULL;
				ImGui::Text(tg ? "UI Toggle" : mi ? "UI Menu Item" : "UI Button");

				if (mi)
				{
					std::string sub = mi->GetSubmenu();
					if (ImGui::InputText("Submenu", &sub)) { mi->SetSubmenu(sub); MarkSceneDirty(); }
					BeginUIUndo(goId); EndUIUndo(goId, "Set Submenu");
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Child element shown while this entry is open, by name.\nEmpty makes it a leaf: clicking it runs its handler and\ncloses the whole menu.");
					ImGui::TextDisabled(mi->IsOpen() ? "Open" : (mi->HasSubmenu() ? "Closed" : "Leaf"));
					ImGui::Separator();
				}

				if (tg)
				{
					bool value = tg->GetValue();
					if (ImGui::Checkbox("Value", &value))
					{
						const json ub = CaptureUIProperties(go);
						tg->SetValue(value);
						MarkSceneDirty();
						PushUIPropertyUndo(goId, ub, CaptureUIProperties(go), "Set Toggle Value");
					}

					std::string check = tg->GetCheckElement();
					if (ImGui::InputText("Check Element", &check)) { tg->SetCheckElement(check); MarkSceneDirty(); }
					BeginUIUndo(goId); EndUIUndo(goId, "Set Toggle Check");
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Child element shown while this is on, by name. Hiding it\nhides its whole subtree, so a tick made of several pieces\nworks as one.");

					std::string group = tg->GetGroup();
					if (ImGui::InputText("Group", &group)) { tg->SetGroup(group); MarkSceneDirty(); }
					BeginUIUndo(goId); EndUIUndo(goId, "Set Toggle Group");
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Toggles that share a group behave as radio buttons. Matched\namong siblings only, so two unrelated sets that happen to\nshare a name cannot fight.");

					std::string onChange = tg->GetOnChange();
					if (ImGui::InputText("On Change", &onChange)) { tg->SetOnChange(onChange); MarkSceneDirty(); }
					BeginUIUndo(goId); EndUIUndo(goId, "Set Toggle Handler");
					ImGui::Separator();
				}

				bool inter = b->IsInteractable();
				if (ImGui::Checkbox("Interactable", &inter))
				{
					const json ub = CaptureUIProperties(go);
					b->SetInteractable(inter);
					MarkSceneDirty();
					PushUIPropertyUndo(goId, ub, CaptureUIProperties(go), "Set Button Interactable");
				}

				std::string handler = b->GetOnClick();
				if (ImGui::InputText("On Click", &handler)) { b->SetOnClick(handler); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Button Handler");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Name of a global Lua function, called with this object's\nname when the button is clicked. Scripts can also poll\nui.wasClicked(element) instead.");

				f32 tr = b->GetTransition();
				if (ImGui::DragFloat("Transition", &tr, 0.01f, 0.f, 2.f, "%.2f s")) { b->SetTransition(tr); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Button Transition");

				// One row per state, and only the states that are not just
				// "same as Normal" - which is what an unset tint means.
				const char* stateNames[3] = { "Hover", "Pressed", "Disabled" };
				const uint32 stateIds[3] = { UIState::Hover, UIState::Pressed, UIState::Disabled };
				for (int st = 0; st < 3; st++)
				{
					ImGui::PushID(st);
					UIStateStyle &ss = b->State(stateIds[st]);
					bool on = ss.hasTint;
					if (ImGui::Checkbox("##tinton", &on))
					{
						const json ub = CaptureUIProperties(go);
						ss.hasTint = on;
						MarkSceneDirty();
						PushUIPropertyUndo(goId, ub, CaptureUIProperties(go), "Set Button State");
					}
					ImGui::SameLine();
					if (!ss.hasTint) ImGui::BeginDisabled();
					if (ImGui::ColorEdit4(stateNames[st], (float*)&ss.tint)) MarkSceneDirty();
					BeginUIUndo(goId); EndUIUndo(goId, "Set Button State Tint");
					if (!ss.hasTint) ImGui::EndDisabled();
					ImGui::PopID();
				}
				if (ImGui::DragFloat2("Press Nudge", (float*)&b->State(UIState::Pressed).offset, 0.5f, -64.f, 64.f))
					MarkSceneDirty();
				BeginUIUndo(goId); EndUIUndo(goId, "Set Button Press Nudge");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Canvas units the element shifts while held. Applied on top\nof the layout, never saved into it.");

				const char* cur = "Normal";
				switch (b->GetCurrentState())
				{
				case UIState::Hover: cur = "Hover"; break;
				case UIState::Pressed: cur = "Pressed"; break;
				case UIState::Disabled: cur = "Disabled"; break;
				case UIState::Focused: cur = "Focused"; break;
				default: break;
				}
				ImGui::TextDisabled("Current state: %s", cur);
			}
			else if (type == ComponentType::UIPopup)
			{
				UIPopup* pp = static_cast<UIPopup*>(comps[i].get());
				ImGui::Text("UI Popup");

				bool isOpen = pp->IsOpen();
				if (ImGui::Checkbox("Open", &isOpen))
				{
					const json ub = CaptureUIProperties(go);
					pp->SetOpen(isOpen);
					MarkSceneDirty();
					PushUIPropertyUndo(goId, ub, CaptureUIProperties(go), "Set Popup Open");
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Authored open so it can be laid out. Uncheck it before\nshipping, and let a script or a menu entry open it.");

				bool isModal = pp->IsModalPopup();
				if (ImGui::Checkbox("Modal", &isModal))
				{
					const json ub = CaptureUIProperties(go);
					pp->SetModal(isModal);
					MarkSceneDirty();
					PushUIPropertyUndo(goId, ub, CaptureUIProperties(go), "Set Popup Modal");
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("While open, nothing outside this element can be clicked,\nhovered or focused. Off makes it a floating panel.");

				bool esc = pp->ClosesOnEscape();
				if (ImGui::Checkbox("Close on Escape", &esc))
				{
					const json ub = CaptureUIProperties(go);
					pp->SetCloseOnEscape(esc);
					MarkSceneDirty();
					PushUIPropertyUndo(goId, ub, CaptureUIProperties(go), "Set Popup Escape");
				}
				bool outside = pp->ClosesOnOutside();
				if (ImGui::Checkbox("Close on Click Outside", &outside))
				{
					const json ub = CaptureUIProperties(go);
					pp->SetCloseOnOutside(outside);
					MarkSceneDirty();
					PushUIPropertyUndo(goId, ub, CaptureUIProperties(go), "Set Popup Outside");
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Both off for something the user has to answer rather than\ndismiss.");

				std::string dlg = pp->GetDialogElement();
				if (ImGui::InputText("Dialog Element", &dlg)) { pp->SetDialogElement(dlg); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Popup Dialog");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("The child that counts as the dialog for \"clicked outside\" -\nthis element covers the whole canvas, so it cannot be the\nthing being clicked outside of.");

				std::string onClose = pp->GetOnClose();
				if (ImGui::InputText("On Close", &onClose)) { pp->SetOnClose(onClose); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Popup Handler");
			}
			else if (type == ComponentType::UISlider)
			{
				UISlider* sl = static_cast<UISlider*>(comps[i].get());
				ImGui::Text("UI Slider");

				bool inter = sl->IsInteractable();
				if (ImGui::Checkbox("Interactable", &inter))
				{
					const json ub = CaptureUIProperties(go);
					sl->SetInteractable(inter);
					MarkSceneDirty();
					PushUIPropertyUndo(goId, ub, CaptureUIProperties(go), "Set Slider Interactable");
				}

				f32 value = sl->GetValue();
				if (ImGui::SliderFloat("Value", &value, sl->GetMin(), sl->GetMax())) { sl->SetValue(value); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Slider Value");

				f32 range[2] = { sl->GetMin(), sl->GetMax() };
				if (ImGui::DragFloat2("Range", range, 0.1f)) { sl->SetRange(range[0], range[1]); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Slider Range");

				f32 step = sl->GetStep();
				if (ImGui::DragFloat("Step", &step, 0.01f, 0.f, 1000.f)) { sl->SetStep(step); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Slider Step");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("0 is continuous. Otherwise the value snaps to multiples of\nthis from the minimum, and the arrow keys move by one.");

				bool vertical = sl->IsVertical();
				if (ImGui::Checkbox("Vertical", &vertical))
				{
					const json ub = CaptureUIProperties(go);
					sl->SetVertical(vertical);
					MarkSceneDirty();
					PushUIPropertyUndo(goId, ub, CaptureUIProperties(go), "Set Slider Vertical");
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Vertical sliders fill upwards: the minimum is at the bottom,\nwhich is what every volume fader does even though canvas y\ngrows downwards.");

				std::string fill = sl->GetFillElement();
				if (ImGui::InputText("Fill Element", &fill)) { sl->SetFillElement(fill); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Slider Fill");
				std::string handle = sl->GetHandleElement();
				if (ImGui::InputText("Handle Element", &handle)) { sl->SetHandleElement(handle); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Slider Handle");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Child elements this slider drives, by name. It only ever\nwrites their anchors, so whatever padding or size they were\nauthored with survives every value.");

				std::string handler = sl->GetOnChange();
				if (ImGui::InputText("On Change", &handler)) { sl->SetOnChange(handler); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Slider Handler");
			}
			else if (type == ComponentType::UIInput)
			{
				UIInput* in = static_cast<UIInput*>(comps[i].get());
				ImGui::Text("UI Input");

				bool inter = in->IsInteractable();
				if (ImGui::Checkbox("Interactable", &inter))
				{
					const json ub = CaptureUIProperties(go);
					in->SetInteractable(inter);
					MarkSceneDirty();
					PushUIPropertyUndo(goId, ub, CaptureUIProperties(go), "Set Input Interactable");
				}

				std::string value = in->GetText();
				if (ImGui::InputText("Text", &value)) { in->SetText(value); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Input Text");

				std::string ph = in->GetPlaceholder();
				if (ImGui::InputText("Placeholder", &ph)) { in->SetPlaceholder(ph); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Input Placeholder");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Shown in the Placeholder child while the field is empty - a\nseparate element so it can be styled differently, which is\nthe only reason to have one.");

				int maxLength = (int)in->GetMaxLength();
				if (ImGui::DragInt("Max Length", &maxLength, 1.f, 0, 4096)) { in->SetMaxLength((uint32)(maxLength < 0 ? 0 : maxLength)); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Input Max Length");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 is unlimited.");

				std::string filter = in->GetFilter();
				if (ImGui::InputText("Filter", &filter)) { in->SetFilter(filter); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Input Filter");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Characters this field accepts. Empty allows everything\nprintable; a number field is 0123456789.-");

				bool password = in->IsPassword();
				if (ImGui::Checkbox("Password", &password))
				{
					const json ub = CaptureUIProperties(go);
					in->SetPassword(password);
					MarkSceneDirty();
					PushUIPropertyUndo(goId, ub, CaptureUIProperties(go), "Set Input Password");
				}
				ImGui::SameLine();
				bool readOnly = in->IsReadOnly();
				if (ImGui::Checkbox("Read Only", &readOnly))
				{
					const json ub = CaptureUIProperties(go);
					in->SetReadOnly(readOnly);
					MarkSceneDirty();
					PushUIPropertyUndo(goId, ub, CaptureUIProperties(go), "Set Input Read Only");
				}

				std::string onChange = in->GetOnChange();
				if (ImGui::InputText("On Change", &onChange)) { in->SetOnChange(onChange); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Input Handler");
				std::string onSubmit = in->GetOnSubmit();
				if (ImGui::InputText("On Submit", &onSubmit)) { in->SetOnSubmit(onSubmit); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Input Submit Handler");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Called when Enter is pressed in the field.");
			}
			else if (type == ComponentType::UIList || type == ComponentType::UIDropdown)
			{
				const bool isList = (type == ComponentType::UIList);
				UIList* l = isList ? static_cast<UIList*>(comps[i].get()) : NULL;
				UIDropdown* d = isList ? NULL : static_cast<UIDropdown*>(comps[i].get());
				UIWidget* w = isList ? (UIWidget*)l : (UIWidget*)d;
				ImGui::Text(isList ? "UI List" : "UI Dropdown");

				bool inter = w->IsInteractable();
				if (ImGui::Checkbox("Interactable", &inter))
				{
					const json ub = CaptureUIProperties(go);
					w->SetInteractable(inter);
					MarkSceneDirty();
					PushUIPropertyUndo(goId, ub, CaptureUIProperties(go), "Set Interactable");
				}

				// One string per line: an array editor with add and remove
				// buttons is a lot of UI for something every author would
				// rather paste into.
				const std::vector<std::string> &values = isList ? l->GetItems() : d->GetOptions();
				std::string joined;
				for (size_t k = 0; k < values.size(); k++) { joined += values[k]; if (k + 1 < values.size()) joined += "\n"; }
				if (ImGui::InputTextMultiline(isList ? "Items" : "Options", &joined, ImVec2(0.f, 90.f)))
				{
					std::vector<std::string> parsed;
					std::string line;
					for (size_t k = 0; k <= joined.size(); k++)
					{
						if (k == joined.size() || joined[k] == '\n')
						{
							if (!line.empty()) parsed.push_back(line);
							line.clear();
						}
						else line += joined[k];
					}
					if (isList) l->SetItems(parsed); else d->SetOptions(parsed);
					MarkSceneDirty();
				}
				BeginUIUndo(goId); EndUIUndo(goId, "Set Items");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("One per line.");

				int selected = isList ? l->GetSelected() : d->GetSelected();
				if (ImGui::DragInt("Selected", &selected, 0.2f, -1, (int)values.size() - 1))
				{
					if (isList) l->SetSelected(selected); else d->SetSelected(selected);
					MarkSceneDirty();
				}
				BeginUIUndo(goId); EndUIUndo(goId, "Set Selection");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("-1 is nothing selected, which is a real state.");

				if (isList)
				{
					f32 itemHeight = l->GetItemHeight();
					if (ImGui::DragFloat("Item Height", &itemHeight, 0.5f, 1.f, 512.f)) { l->SetItemHeight(itemHeight); MarkSceneDirty(); }
					BeginUIUndo(goId); EndUIUndo(goId, "Set Item Height");
					ImGui::TextDisabled("%u of %u rows in use", l->GetVisibleRows(), (uint32)values.size());
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Rows are child elements named Row0, Row1... and are recycled\nas the list scrolls, so enough to cover the viewport is\nenough for a list of any length.");
				}
				else
				{
					std::string ph = d->GetPlaceholder();
					if (ImGui::InputText("Placeholder", &ph)) { d->SetPlaceholder(ph); MarkSceneDirty(); }
					BeginUIUndo(goId); EndUIUndo(goId, "Set Dropdown Placeholder");
					ImGui::TextDisabled(d->IsExpanded() ? "Open" : "Closed");
				}

				std::string handler = w->GetOnChange();
				if (ImGui::InputText("On Change", &handler)) { w->SetOnChange(handler); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Handler");
			}
			else if (type == ComponentType::UIText)
			{
				UIText* t = static_cast<UIText*>(comps[i].get());
				ImGui::Text("UI Text");

				std::string text = t->GetText();
				if (ImGui::InputText("Text", &text)) { t->SetText(text); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Text");

				f32 size = t->GetSize();
				if (ImGui::DragFloat("Size", &size, 0.5f, 1.f, 1024.f)) { t->SetSize(size); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Text Size");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Glyph height in canvas units. The font's atlas is baked at a\nfixed pixel size, so text far from it goes soft.");

				Vec4 color = t->GetColor();
				if (ImGui::ColorEdit4("Color", (float*)&color)) { t->SetColor(color); MarkSceneDirty(); }
				BeginUIUndo(goId); EndUIUndo(goId, "Set Text Color");

				bool sdfFont = t->IsFontSDF();
				if (ImGui::Checkbox("Crisp (SDF)", &sdfFont))
				{
					const json ub = CaptureUIProperties(go);
					t->SetFontSDF(sdfFont);
					MarkSceneDirty();
					PushUIPropertyUndo(goId, ub, CaptureUIProperties(go), "Set Crisp Text");
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Re-bakes the font as a signed distance field. One bake then\nstays sharp at any size, where a normal atlas is only sharp at\nthe size it was baked. Costs a wider atlas cell per glyph.");

				bool wrap = t->IsWordWrap();
				if (ImGui::Checkbox("Word Wrap", &wrap))
				{
					const json ub = CaptureUIProperties(go);
					t->SetWordWrap(wrap);
					MarkSceneDirty();
					PushUIPropertyUndo(goId, ub, CaptureUIProperties(go), "Set Word Wrap");
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Breaks lines to fit the element's rect. Off by default: a\nreadout that silently became two lines because a value grew\nis worse than one that overflows visibly.");

				int h = (int)t->GetHorizontalAlignment();
				const char* hNames[] = { "Left", "Center", "Right" };
				if (ImGui::Combo("Align", &h, hNames, 3))
				{
					const json b = CaptureUIProperties(go);
					t->SetAlignment((uint32)h, t->GetVerticalAlignment());
					MarkSceneDirty();
					PushUIPropertyUndo(goId, b, CaptureUIProperties(go), "Set Text Align");
				}
				int v = (int)t->GetVerticalAlignment();
				const char* vNames[] = { "Top", "Middle", "Bottom" };
				if (ImGui::Combo("Vertical", &v, vNames, 3))
				{
					const json b = CaptureUIProperties(go);
					t->SetAlignment(t->GetHorizontalAlignment(), (uint32)v);
					MarkSceneDirty();
					PushUIPropertyUndo(goId, b, CaptureUIProperties(go), "Set Text Vertical Align");
				}
				if (t->GetFont())
					ImGui::TextDisabled("%s @ %.0fpx atlas", DisplayPath(t->GetFont()->GetPath()).c_str(), t->GetFont()->GetFontSize());
			}

			ImGui::PopID();
		}
	}

	void SceneEditor::DrawGameObjectScriptProperties(uint32 goId)
	{
		ImGui::Separator();
		ImGui::TextUnformatted("Script");
		if (playMode)
		{
			ImGui::TextDisabled("Stop play mode to attach scripts");
			return;
		}
		if (!sharedLua)
		{
			ImGui::TextDisabled("Lua host not available");
			return;
		}

		ImGui::TextDisabled("Attached");
		// List scripts already on this GameObject with Detach.
		bool anyAttached = false;
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin();
			i != sceneObjects->GetList().end(); ++i)
		{
			if (!i->second || i->second->GetParentID() != goId) continue;
			if (i->second->GetType() != SceneObjectTypes::LUA_COMPONENT) continue;
			LuaComponent* lc = (LuaComponent*)i->second->GetPTR();
			if (!lc) continue;
			anyAttached = true;
			ImGui::PushID((int)i->first);
			std::string label = i->second->GetName();
			if (!lc->scriptFile.empty())
			{
				std::string rel = lc->scriptFile;
				if (project && project->IsOpen())
				{
					const std::string r = project->RelativePath(lc->scriptFile);
					if (!r.empty()) rel = r;
				}
				label += "  (" + rel + ")";
			}
			ImGui::BulletText("%s", label.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Open") && hostOpenLuaScript && !lc->scriptFile.empty())
				hostOpenLuaScript(ResolveScriptPath(lc->scriptFile));
			ImGui::SameLine();
			if (ImGui::SmallButton("Detach"))
			{
				DeleteComponentById(i->first);
				ImGui::PopID();
				return;
			}
			ImGui::PopID();
		}
		if (!anyAttached)
			ImGui::TextDisabled("(none attached)");

		ImGui::Spacing();
		ImGui::TextDisabled("Attach an existing assets/lua script, or create a new one.");
		DrawScriptAssetPicker("##goscript", propertiesScriptAttachPath);
		if (ImGui::Button("Attach"))
		{
			const std::string abs = ResolveScriptPath(propertiesScriptAttachPath);
			if (AttachLuaScriptToGameObject(goId, abs))
				propertiesScriptAttachPath.clear();
		}
		ImGui::SameLine();
		if (ImGui::Button("Create New…"))
		{
			propertiesNewGoScriptName.clear();
			propertiesNewGoScriptError.clear();
			openNewGoScriptModal = true;
		}

		if (openNewGoScriptModal)
		{
			ImGui::SetNextWindowFocus();
			ImGui::OpenPopup("New GameObject Script");
		openNewGoScriptModal = false;
		openAddFormTrigger = false;
		}
		if (ImGui::BeginPopupModal("New GameObject Script", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Creates assets/lua/<name>.lua with a GameObject snippet, then attaches it.");
			ImGui::SetNextItemWidth(280.f);
			ImGui::InputText("Name", &propertiesNewGoScriptName);
			if (!propertiesNewGoScriptError.empty())
				ImGui::TextColored(ImVec4(1.f, 0.45f, 0.4f, 1.f), "%s", propertiesNewGoScriptError.c_str());
			ImGui::Spacing();
			if (ImGui::Button("Create & Attach", ImVec2(140, 0)))
			{
				if (!project || !project->IsOpen())
					propertiesNewGoScriptError = "Open a project first";
				else
				{
					std::string abs;
					std::string err;
					if (project->CreateLuaScript(propertiesNewGoScriptName, abs, &err, LuaScriptKind::GameObject))
					{
						const std::string createdRel = project->RelativePath(abs);
						if (!createdRel.empty())
							sceneUndo.Push(std::make_unique<CreateAssetCommand>(project, createdRel, "Create Script '" + createdRel + "'"));
						project->Save();
						if (hostOpenLuaScript) hostOpenLuaScript(abs);
						if (AttachLuaScriptToGameObject(goId, abs))
						{
							propertiesScriptAttachPath.clear();
							ImGui::CloseCurrentPopup();
						}
						else
							propertiesNewGoScriptError = "Created but attach failed";
					}
					else
						propertiesNewGoScriptError = err;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(100, 0)))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
	}

	bool SceneEditor::EnsureAndBindSceneCompanionScript()
	{
		if (scenePath.empty()) return false;
		std::string abs;
		std::string err;
		if (project && project->IsOpen())
		{
			if (!project->EnsureSceneCompanionScript(scenePath, abs, &err))
			{
				echo("ERROR: " + err);
				return false;
			}
		}
		else
		{
			abs = ProjectManager::SceneScriptPathForSceneJson(scenePath);
			std::error_code ec;
			if (!std::filesystem::exists(abs, ec))
			{
				echo("ERROR: Scene script missing and no project open to create it");
				return false;
			}
		}
		return SetSceneMainScript(abs);
	}

	bool SceneEditor::SetSceneMainScript(const std::string& absoluteOrRelativePath)
	{
		if (playMode) return false;
		std::string abs = absoluteOrRelativePath;
		if (project && project->IsOpen() && !abs.empty())
		{
			if (abs.find("assets/") == 0 || abs.find("scenes/") == 0)
				abs = project->AbsolutePath(abs);
		}
		sceneMainScriptPath = abs;
		if (!RebuildSceneMainScriptInstance() && !abs.empty())
		{
			echo("ERROR: Could not load scene main script: " + abs);
			sceneMainScriptPath.clear();
			sceneMainScript.reset();
			return false;
		}
		MarkSceneDirty();
		if (!abs.empty())
			echo("SUCCESS: Scene script = " + abs);
		return true;
	}

	void SceneEditor::ClearSceneMainScript()
	{
		if (playMode) return;
		sceneMainScriptPath.clear();
		sceneMainScript.reset();
		MarkSceneDirty();
	}

	bool SceneEditor::RebuildSceneMainScriptInstance()
	{
		sceneMainScript.reset();
		if (sceneMainScriptPath.empty() || !sharedLua) return true;
		PushLuaHostGlobals();
		try {
			sceneMainScript = LuaComponent_FromFile(*sharedLua, sceneMainScriptPath);
		}
		catch (const std::exception& e) {
			echo(std::string("ERROR: Scene main script load: ") + e.what());
			return false;
		}
		catch (...) {
			echo("ERROR: Scene main script load failed");
			return false;
		}
		return sceneMainScript != NULL;
	}

	void SceneEditor::InitSceneMainScript()
	{
		if (!sceneMainScript) return;
		try { sceneMainScript->Init(); }
		catch (const std::exception& e) {
			echo(std::string("ERROR: Scene main script init: ") + e.what());
			echo("ERROR: Main Script has no GameObject owner — use it for scene-level logic only. Attach FlyCamera (etc.) to a Camera GO via Properties → Script.");
		}
		catch (...) { echo("ERROR: Scene main script init failed"); }
	}

	void SceneEditor::UpdateSceneMainScript(f64 time)
	{
		if (!sceneMainScript) return;
		try { sceneMainScript->Update(time); }
		catch (const std::exception& e) { echo(std::string("ERROR: Scene main script update: ") + e.what()); }
		catch (...) { echo("ERROR: Scene main script update failed"); }
	}

	void SceneEditor::ResetSceneMainScriptLifecycle()
	{
		if (sceneMainScript)
			sceneMainScript->ResetLifecycle();
	}
#endif

	void SceneEditor::DrawSceneSettingsInProperties()
	{
		ImGui::Spacing();
		ImGui::Indent(5.f);
		ImGui::TextUnformatted("Scene");
		ImGui::Separator();
		if (!scenePath.empty())
			ImGui::TextWrapped("File: %s", DisplayPath(scenePath).c_str());
		else
			ImGui::TextDisabled("File: (unsaved)");

		ImGui::Spacing();
		ImGui::TextUnformatted("Ambient Light");
		// Flat colour added to every lit surface regardless of real
		// lights - see SceneMeta::ambientLight's comment. Only .xyz are
		// meaningful (no shader reads .w), so a 3-component picker is
		// exact, not a simplification.
		if (ImGui::ColorEdit3("##ambient_light", (float*)&ambientLightColor))
		{
			Renderer->SetGlobalLight(ambientLightColor);
			sceneDirty = true;
		}
#ifdef LUA_BINDINGS
		ImGui::Spacing();
		ImGui::TextUnformatted("Scene Script");
		ImGui::TextDisabled("Companion file scenes/<SceneName>.lua (also under Assets → Lua / Scenes).");
		if (scenePath.empty())
		{
			ImGui::TextDisabled("Save the scene to create its companion script.");
		}
		else
		{
			const std::string rel = DisplayPath(ProjectManager::SceneScriptPathForSceneJson(scenePath));
			if (playMode)
			{
				ImGui::TextWrapped("%s", sceneMainScriptPath.empty() ? rel.c_str() : sceneMainScriptPath.c_str());
			}
			else
			{
				ImGui::TextWrapped("%s", rel.c_str());
				if (ImGui::Button("Open Script", ImVec2(140, 0)))
				{
					EnsureAndBindSceneCompanionScript();
					if (hostOpenLuaScript && !sceneMainScriptPath.empty())
						hostOpenLuaScript(sceneMainScriptPath);
				}
				ImGui::SameLine();
				if (ImGui::Button("Ensure / Reload"))
					EnsureAndBindSceneCompanionScript();
			}
		}
#else
		ImGui::TextDisabled("Lua bindings disabled in this build");
#endif
		ImGui::Unindent(5.f);
	}

	void SceneEditor::OpenAddFormOnGameObject(uint32 goId, uint32 formType)
	{
		SceneObject* obj = sceneObjects->GetSceneObject(goId);
		if (obj == NULL || obj->GetType() != SceneObjectTypes::GAMEOBJECT || IsInternalGameObject((GameObject*)obj->GetPTR()))
			return;
		SelectSceneObject(obj);
		AddForm_cgo = false;
		showingAddFormType = formType;
		showingAddFrom = true;
	}

	void SceneEditor::AddQuickLightOnGameObject(uint32 goId, uint32 formType)
	{
		SceneObject* obj = sceneObjects->GetSceneObject(goId);
		if (obj == NULL || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return;
		SelectSceneObject(obj);
		AddForm_cgo = false;
		showingAddFormType = formType;
		if (formType == 10) { AddForm_dir = Vec3(0.f, -1.f, 0.f); AddForm_color = Vec4(1.f, 1.f, 1.f, 1.f); }
		else if (formType == 11) { AddForm_w = 10.f; AddForm_color = Vec4(1.f, 0.95f, 0.8f, 1.f); }
		else if (formType == 12) { AddForm_w = 12.f; AddForm_dir = Vec3(0.f, -1.f, 0.f); AddForm_color = Vec4(1.f, 1.f, 1.f, 1.f); AddForm_oc = 45.f; AddForm_ic = 25.f; }
		AddFormSubmit();
	}

	void SceneEditor::ShowAddComponentMenu(uint32 goId)
	{
		if (ImGui::BeginMenu("Add Component"))
		{
			if (ImGui::BeginMenu("Mesh"))
			{
				if (ImGui::MenuItem("Cube")) { OpenAddFormOnGameObject(goId, 1); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Sphere")) { OpenAddFormOnGameObject(goId, 2); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Capsule")) { OpenAddFormOnGameObject(goId, 3); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Cylinder")) { OpenAddFormOnGameObject(goId, 6); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Cone")) { OpenAddFormOnGameObject(goId, 5); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Plane")) { OpenAddFormOnGameObject(goId, 4); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Torus")) { OpenAddFormOnGameObject(goId, 7); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Torus Knot")) { OpenAddFormOnGameObject(goId, 8); ImGui::CloseCurrentPopup(); }
				ImGui::Separator();
				if (ImGui::MenuItem("Import Model...")) { OpenAddFormOnGameObject(goId, 9); ImGui::CloseCurrentPopup(); }
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Physics"))
			{
				if (ImGui::MenuItem("Box")) { OpenAddFormOnGameObject(goId, 13); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Sphere")) { OpenAddFormOnGameObject(goId, 17); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Capsule")) { OpenAddFormOnGameObject(goId, 14); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Cylinder")) { OpenAddFormOnGameObject(goId, 16); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Cone")) { OpenAddFormOnGameObject(goId, 15); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Static Plane")) { OpenAddFormOnGameObject(goId, 18); ImGui::CloseCurrentPopup(); }
				ImGui::EndMenu();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Audio Source"))
			{
				AddForm_soundPath.clear();
				AddForm_stream = false;
				AddForm_loop = false;
				AddForm_spatialized = true;
				AddForm_volume = 1.0f;
				OpenAddFormOnGameObject(goId, 19);
				ImGui::CloseCurrentPopup();
			}
			// 2D authoring shortcuts. Neither is a new component type - a
			// sprite is a textured quad and a layer is a transform with a
			// parallax factor, both built out of what already exists.
			if (ImGui::MenuItem("Sprite"))
			{
				std::string serr;
				if (!OpAddSprite(goId, std::string(), serr))
					echo("WARNING: could not add Sprite: " + serr);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("A textured, alpha-blended quad. Assign its texture in\nProperties - it starts white and square.");

			if (ImGui::MenuItem("Occluder 2D"))
			{
				std::string oerr;
				if (!OpAddOccluder2D(goId, oerr))
					echo("WARNING: could not add Occluder2D: " + oerr);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Blocks 2D light. No physics needed - a painted wall\ncan cast without being solid to the simulation.");

			if (ImGui::MenuItem("Physics 2D"))
			{
				std::string perr;
				if (!OpAddPhysics2D(goId, perr))
					echo("WARNING: could not add Physics2D: " + perr);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("A Box2D rigid body. Its (x, y) and rotation about z\nare driven by the solver; z is left alone because that\nis draw order.");

			// A 2D scene layer. One click, no form: the useful defaults are
			// parallax 1 and visible, and both are edited in Properties.
			if (ImGui::MenuItem("Layer 2D"))
			{
				std::string lerr;
				if (!OpAddLayer2D(goId, lerr))
					echo("WARNING: could not add Layer2D: " + lerr);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Everything under this object becomes one 2D layer:\nits z is the draw order, and its parallax factor is how\nfast it scrolls relative to the camera.");

			if (ImGui::MenuItem("Particle System"))
			{
				AddForm_particleTexturePath.clear();
				AddForm_particleMax = 200;
				AddForm_particlePreset = 0;
				OpenAddFormOnGameObject(goId, 20);
				ImGui::CloseCurrentPopup();
			}
			ImGui::Separator();
			// Lights skip the add form entirely (AddQuickLight...), but they are
			// grouped and named exactly as in ShowAddObjectMenuItems so the two
			// menus stay readable side by side.
			if (ImGui::BeginMenu("Lights"))
			{
				if (ImGui::MenuItem("Directional")) { AddQuickLightOnGameObject(goId, 10); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Point")) { AddQuickLightOnGameObject(goId, 11); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Spot")) { AddQuickLightOnGameObject(goId, 12); ImGui::CloseCurrentPopup(); }
				ImGui::EndMenu();
			}
			// Screen-space UI. No add form: every one of these has a usable
			// default, and the Properties panel is where they get tuned.
			if (ImGui::BeginMenu("UI"))
			{
				const char* kinds[] = { "Canvas", "Rect", "Image", "Text", "Button",
					"Toggle", "Slider", "Input", "List", "Dropdown", "Menu", "Popup" };
				const char* tips[] = {
					"Root of a screen-space UI tree. Add elements as children.",
					"Anchored rectangle - the layout half of an element.",
					"Tinted, optionally 9-sliced quad. Adds a Rect if missing.",
					"A line of text aligned in its rect. Adds a Rect if missing.",
					"Clickable, with hover/pressed/disabled states. Adds a Rect\nand an Image if missing; put a Text child on it for a label.",
					"A checkbox: a button that remembers. Comes with the Check\nelement it shows and hides. Name a Group to make several\nof them behave as radio buttons.",
					"A value dragged along a track. Comes with the Fill and\nHandle children it drives.",
					"A single-line text field, with its label, placeholder and\ncaret elements.",
					"A scrolling list. Comes with four rows, which are recycled\nas it scrolls - enough to cover the viewport is enough for\na list of any length.",
					"A closed list that opens to be picked from. Comes with its\nlabel and a popup containing a list.",
					"A menu bar with two menus and a submenu inside one of them.\nOnce open it follows the pointer: hovering the next title\nopens it without a second click.",
					"A dialog: a scrim, a panel and two buttons. While it is open\nnothing underneath it can be clicked, hovered or focused."
				};
				for (int i = 0; i < 12; i++)
				{
					if (ImGui::MenuItem(kinds[i]))
					{
						std::string err;
						std::string kind = kinds[i];
						for (size_t c = 0; c < kind.size(); c++) kind[c] = (char)tolower((unsigned char)kind[c]);
						if (!OpAddUIComponent(goId, kind, std::string(), err))
							echo(std::string("ERROR: ") + err);
						ImGui::CloseCurrentPopup();
					}
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tips[i]);
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
	}

	bool SceneEditor::TryPickViewportIcon(const Vec2& viewportMouse, uint32& outSceneObjectId) const
	{
		outSceneObjectId = 0;
		if (!viewportOverlayValid || viewportImgSize.x < 1.f || viewportImgSize.y < 1.f
			|| dim.x < 1.f || dim.y < 1.f || viewportIcons.empty())
			return false;

		// viewportMouse is in render-target pixels; the icon rects recorded
		// by DrawSceneViewportIcons are in ImGui screen space. Same mapping
		// UpdateViewportMouse() inverts to produce viewportMouse in the first
		// place, so the two agree even when the image is displayed at a
		// different scale than it was rendered at.
		const ImVec2 mouseScreen(
			viewportImgMin.x + (viewportMouse.x / dim.x) * viewportImgSize.x,
			viewportImgMin.y + (viewportMouse.y / dim.y) * viewportImgSize.y);

		// Nearest icon wins where several overlap, rather than whichever
		// happened to be visited first by the scene walk.
		bool found = false;
		f32 bestDepth = 0.f;
		for (std::vector<ViewportIcon>::const_iterator i = viewportIcons.begin(); i != viewportIcons.end(); ++i)
		{
			if (mouseScreen.x < i->min.x || mouseScreen.x > i->max.x
				|| mouseScreen.y < i->min.y || mouseScreen.y > i->max.y)
				continue;
			if (found && i->viewDepth >= bestDepth)
				continue;
			bestDepth = i->viewDepth;
			outSceneObjectId = i->sceneObjectId;
			found = true;
		}
		return found;
	}

	void SceneEditor::ShowViewOptions()
	{
		bool frustum = editorDebugDraw->IsCameraFrustumOn();
		if (ImGui::MenuItem("Show Camera Frustums", NULL, frustum))
			editorDebugDraw->ToggleCameraFrustum(!frustum);
		if (ImGui::MenuItem("Show Physics Debug", NULL, showPhysicsDebug))
			showPhysicsDebug = !showPhysicsDebug;
		ImGui::Separator();
		// Always enabled, and it creates what it needs. It used to be greyed
		// out until the scene already had a UICanvas, which made the one menu
		// item named after 2D editing the one place that could not get you
		// there - the canvas itself is only offered on a GameObject's own Add
		// menu, so you had to know to make an object and right-click it first.
		const bool haveCanvas = (GetEditingCanvas() != NULL);
		if (ImGui::MenuItem("Canvas (2D) Mode", "", uiEditMode))
		{
			if (!uiEditMode && !haveCanvas)
				CreateCanvasForEditing();
			uiEditMode = !uiEditMode;
		}
		if (!haveCanvas && ImGui::IsItemHovered())
			ImGui::SetTooltip("Adds a UICanvas to this scene and edits it in 2D.");
	}

	// A 2D *scene*: sprites, layers, 2D lights and bodies laid out in the XY
	// plane, viewed orthographically. Deliberately does NOT create a Canvas
	// or turn on uiEditMode - this used to do both, which meant "2D scene"
	// and "UI screen" were the same thing and there was no way to ask for a
	// 2D game without also getting a UI canvas you did not want. See
	// MakeUIScene() for that one.
	void SceneEditor::MakeTwoDScene()
	{
		sceneIsTwoD = true;
		SwitchRenderer(false);
		// A 2D scene opens looking at the XY plane through an orthographic
		// view. It used to open in the default perspective three-quarter
		// view, which is why every 2D scene needed the camera dragged into
		// place by hand before it looked like a 2D scene at all - and why the
		// gizmo's orthographic paths never ran even in a scene marked twoD.
		LookAtPlaneXY(0.f, 0.f);
		uiEditMode = false;
		MarkSceneDirty();
	}

	// A UI screen: one Canvas, selected and ready to have elements added to
	// it, in canvas edit mode. Also 2D and also orthographic - a UI screen is
	// a 2D scene whose content happens to be widgets.
	void SceneEditor::MakeUIScene()
	{
		sceneIsTwoD = true;
		SwitchRenderer(false);
		LookAtPlaneXY(0.f, 0.f);
		if (GetEditingCanvas() == NULL)
			CreateCanvasForEditing();
		uiEditMode = true;
		MarkSceneDirty();
	}

	void SceneEditor::CreateCanvasForEditing()
	{
		SceneObject* obj = sceneObjects->CreateGameObject("Canvas");
		if (!obj) return;
		std::string err;
		if (!OpAddUIComponent(obj->GetID(), "Canvas", std::string(), err))
		{
			echo("WARNING: could not add UICanvas: " + err);
			return;
		}
		MarkSceneDirty();
		PushAddCommand(obj);
		SelectedSceneObject = obj;
	}

	UICanvas* SceneEditor::GetEditingCanvas() const
	{
		// The canvas that owns the selection, so editing an element puts you
		// in its canvas rather than in whichever one happens to be first.
		if (SelectedSceneObject != NULL && SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT)
		{
			for (GameObject* go = (GameObject*)SelectedSceneObject->GetPTR(); go != NULL; go = go->GetParent())
			{
				const std::vector<std::shared_ptr<IComponent> >& cs = go->GetComponents();
				for (size_t i = 0; i < cs.size(); i++)
					if (cs[i] && cs[i]->GetComponentType() == ComponentType::UICanvas)
						return static_cast<UICanvas*>(cs[i].get());
			}
		}
		std::vector<UICanvas*> all = UICanvas::GetCanvasesOnScene(scene);
		return all.empty() ? NULL : all[0];
	}

	void SceneEditor::DrawCanvasOverlay(UICanvas* canvas, const Vec2& viewSize)
	{
		if (!canvas || !debugRenderer) return;
		const UIRectValue& c = canvas->GetCanvasRect();
		if (c.width <= 0.f || c.height <= 0.f) return;

		// Canvas space is y-down with its origin top-left, and the canvas
		// GameObject holds point (x, y) at (x, -y) - see UIRect.h. Drawing
		// through the same mapping puts these lines exactly on top of the
		// elements they describe.
		struct L {
			DebugRenderer* d;
			void line(const f32 x0, const f32 y0, const f32 x1, const f32 y1, const Vec4& col) const
			{
				d->drawLine(Vec3(x0, -y0, 0.f), Vec3(x1, -y1, 0.f), col);
			}
			void rect(const UIRectValue& r, const Vec4& col) const
			{
				line(r.x, r.y, r.Right(), r.y, col);
				line(r.Right(), r.y, r.Right(), r.Bottom(), col);
				line(r.Right(), r.Bottom(), r.x, r.Bottom(), col);
				line(r.x, r.Bottom(), r.x, r.y, col);
			}
		} L{ debugRenderer };

		// A grid in canvas units, so distances on screen mean something.
		// Spaced to stay readable rather than fixed: at a 1920-wide canvas
		// this is 80 units minor, 480 major.
		const f32 minor = c.width / 24.f;
		const Vec4 minorCol(1.f, 1.f, 1.f, 0.06f), majorCol(1.f, 1.f, 1.f, 0.14f);
		for (int i = 1; i * minor < c.width; i++)
			L.line(i * minor, 0.f, i * minor, c.height, (i % 6) ? minorCol : majorCol);
		for (int i = 1; i * minor < c.height; i++)
			L.line(0.f, i * minor, c.width, i * minor, (i % 6) ? minorCol : majorCol);

		// The canvas bounds themselves - the one edge that actually exists.
		L.rect(c, Vec4(0.30f, 0.80f, 1.f, 0.9f));

		// And the selected element's solved rect, with corner ticks. This is
		// the thing being edited, so it gets the strongest colour.
		if (SelectedSceneObject != NULL && SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT)
		{
			GameObject* go = (GameObject*)SelectedSceneObject->GetPTR();
			const std::vector<std::shared_ptr<IComponent> >& cs = go->GetComponents();
			for (size_t i = 0; i < cs.size(); i++)
			{
				if (!cs[i] || cs[i]->GetComponentType() != ComponentType::UIRect) continue;
				const UIRectValue& r = static_cast<UIRect*>(cs[i].get())->GetRect();
				const Vec4 sel(1.f, 0.78f, 0.20f, 1.f);
				L.rect(r, sel);
				const f32 t = c.width / 90.f;
				const f32 xs[3] = { r.x, r.x + r.width * 0.5f, r.Right() };
				const f32 ys[3] = { r.y, r.y + r.height * 0.5f, r.Bottom() };
				for (int hx = 0; hx < 3; hx++)
					for (int hy = 0; hy < 3; hy++)
					{
						if (hx == 1 && hy == 1) continue;
						L.line(xs[hx] - t, ys[hy] - t, xs[hx] + t, ys[hy] - t, sel);
						L.line(xs[hx] + t, ys[hy] - t, xs[hx] + t, ys[hy] + t, sel);
						L.line(xs[hx] + t, ys[hy] + t, xs[hx] - t, ys[hy] + t, sel);
						L.line(xs[hx] - t, ys[hy] + t, xs[hx] - t, ys[hy] - t, sel);
					}
				break;
			}
		}

		// Drawn with the canvas's own projection, not the scene camera's:
		// the same ortho box UIRenderer uses, so a line at canvas x lands on
		// the pixel the element at canvas x was drawn to.
		Projection canvasProj;
		canvasProj.Ortho(0.f, c.width, -c.height, 0.f, -1000.f, 1000.f);
		Matrix identity;
		identity.identity();
		debugRenderer->Render(identity, canvasProj.GetProjectionMatrix());
		(void)viewSize;
	}

	void SceneEditor::DrawTreeNodeWidgets(SceneObject* obj, bool node_open)
	{
		if (!obj) return;
		(void)node_open;

		if (ImGui::BeginPopupContextItem())
		{
			if (playMode)
			{
				ImGui::TextDisabled("Stop play mode to edit");
				ImGui::EndPopup();
				return;
			}
			ShowRightMenu();
			ImGui::Separator();
			if (obj->GetType() == SceneObjectTypes::GAMEOBJECT)
			{
				GameObject* go = (GameObject*)obj->GetPTR();
				if (go && go->HaveParent() && !IsInternalGameObject(go))
				{
					if (ImGui::MenuItem("Unparent to Root"))
					{
						std::string err;
						OpReparentGameObject(obj->GetID(), 0, err);
					}
					ImGui::Separator();
				}
				if (go && IsSceneCamera(obj->GetID()))
				{
					if (ImGui::MenuItem("Set as Active Camera"))
						SetActiveSceneCamera(obj->GetID());
					if (activeSceneCameraId == obj->GetID() && ImGui::MenuItem("Use Editor Camera"))
						ClearActiveSceneCamera();
					ImGui::Separator();
				}
				if (go && !IsInternalGameObject(go))
					ShowAddComponentMenu(obj->GetID());
				if (go && !IsInternalGameObject(go) && ImGui::MenuItem("Duplicate"))
				{
					SelectSceneObject(obj);
					DuplicateSelected();
				}
				if (go && !IsInternalGameObject(go))
					ShowPrefabMenu(obj->GetID());
			}
			else
			{
				if (obj->GetType() == SceneObjectTypes::LUA_COMPONENT)
				{
#ifdef LUA_BINDINGS
					LuaComponent* lc = (LuaComponent*)obj->GetPTR();
					if (lc && !lc->scriptFile.empty() && hostOpenLuaScript
						&& ImGui::MenuItem("Open Script"))
					{
						hostOpenLuaScript(ResolveScriptPath(lc->scriptFile));
						ImGui::EndPopup();
						return;
					}
#endif
				}
				const char* deleteLabel = (obj->GetType() == SceneObjectTypes::LUA_COMPONENT)
					? "Detach Script" : "Delete Component";
				if (ImGui::MenuItem(deleteLabel))
				{
					// Deferred (see pendingDeleteId's declaration) - the
					// caller (DrawNodes) is still mid-walk over
					// sceneObjects->GetList() and keeps dereferencing `obj`
					// after this function returns, so deleting immediately
					// here would free it out from under that walk.
					pendingDeleteId = obj->GetID();
					ImGui::EndPopup();
					return;
				}
			}
			if (obj->GetType() == SceneObjectTypes::GAMEOBJECT && ImGui::MenuItem("Delete GameObject"))
			{
				pendingDeleteId = obj->GetID();
			}
			ImGui::EndPopup();
		}

		const uint32 type = obj->GetType();
		if (type == SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT ||
			type == SceneObjectTypes::POINTLIGHT_COMPONENT ||
			type == SceneObjectTypes::SPOTLIGHT_COMPONENT ||
			type == SceneObjectTypes::RENDERING_COMPONENT)
		{
			ImGui::SameLine(0.f, 6.f);
			IComponent* comp = (IComponent*)obj->GetPTR();
			bool on = (type == SceneObjectTypes::RENDERING_COMPONENT)
				? editorDebugDraw->IsNormalsOn(comp)
				: editorDebugDraw->IsOn(comp);
			ImGui::PushID((int)obj->GetID() + 100000);
			if (ImGui::SmallButton(on ? EditorIcons::VisibilityOn() : EditorIcons::VisibilityOff()))
			{
				if (type == SceneObjectTypes::RENDERING_COMPONENT)
					editorDebugDraw->ToggleNormalsForRenderingComponent(comp);
				else
					editorDebugDraw->ToggleForComponent(comp);
			}
			ImGui::PopID();
		}

		if (type == SceneObjectTypes::GAMEOBJECT)
		{
			GameObject* go = (GameObject*)obj->GetPTR();
			if (go && IsSceneCamera(obj->GetID()))
			{
				ImGui::SameLine(0.f, 6.f);
				bool on = editorDebugDraw->IsCameraOn(go);
				ImGui::PushID((int)obj->GetID() + 200000);
				if (ImGui::SmallButton(on ? EditorIcons::VisibilityOn() : EditorIcons::VisibilityOff()))
					editorDebugDraw->ToggleForCamera(go);
				ImGui::PopID();
			}
			if (go && !IsInternalGameObject(go))
			{
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers))
				{
					uint32 id = obj->GetID();
					ImGui::SetDragDropPayload("GO_ID", &id, sizeof(id));
					ImGui::Text("%s", obj->GetName().c_str());
					ImGui::EndDragDropSource();
				}
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GO_ID"))
					{
						uint32 draggedId = *(const uint32*)payload->Data;
						if (draggedId != obj->GetID())
						{
							std::string err;
							OpReparentGameObject(draggedId, obj->GetID(), err);
						}
					}
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("COMP_ID"))
					{
						const DragCompPayload* data = (const DragCompPayload*)payload->Data;
						SceneObject* compObj = sceneObjects->GetSceneObject(data->compId);
						if (compObj && data->ownerGoId != obj->GetID())
						{
							editorDebugDraw->ForgetComponent((IComponent*)compObj->GetPTR());
							sceneObjects->MoveComponent(data->compId, obj->GetID());
							SelectSceneObject(compObj);
						}
					}
					ImGui::EndDragDropTarget();
				}
			}
		}
		else if (type != SceneObjectTypes::PHYSICS_COMPONENT)
		{
			IComponent* comp = (IComponent*)obj->GetPTR();
			GameObject* owner = comp ? comp->GetOwner() : NULL;
			if (comp && owner && !IsInternalGameObject(owner))
			{
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover))
				{
					DragCompPayload payload;
					payload.compId = obj->GetID();
					payload.ownerGoId = obj->GetParentID();
					ImGui::SetDragDropPayload("COMP_ID", &payload, sizeof(payload));
					ImGui::Text("%s", obj->GetName().c_str());
					ImGui::EndDragDropSource();
				}
			}
		}
	}

	void SceneEditor::DeselectMesh()
	{
		if (SelectedRenderingComponent)
		{
			// Detaching drops the GameObject's reference; ours goes with
			// the reset() below.
			SelectedRenderingComponent->GetOwner()->Remove(SelectedRenderingComponent);
		}
		SelectedMesh = NULL;
		SelectedRenderingComponent.reset();
	}

	void SceneEditor::SelectMesh(RenderingMesh* rmesh)
	{
		if (SelectedMesh != rmesh)
		{
			DeselectMesh();

			SelectedMesh = rmesh;
			SelectedRenderingComponent = std::make_shared<RenderingComponent>(SelectedMesh->renderingComponent->GetRenderableShared(), SelectedMeshMaterial);
			rmesh->renderingComponent->GetOwner()->Add(SelectedRenderingComponent);
			for (int i = 0; i < SelectedRenderingComponent->GetMeshes().size(); i++)
			{
				SelectedRenderingComponent->GetMeshes()[i]->Active = false;
				if (SelectedRenderingComponent->GetMeshes()[i]->Geometry->GetInternalID() == rmesh->Geometry->GetInternalID())
					SelectedRenderingComponent->GetMeshes()[i]->Active = true;
			}
		}
	}

	void SceneEditor::SyncTransformFromGameObject(SceneObject* obj)
	{
		if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return;
		GameObject* go = (GameObject*)obj->GetPTR();
		if (!go) return;

		_translation = go->GetPosition();
		_rotation = go->GetRotation();
		_scale = go->GetScale();

		obj->globalRotation.identity();
		obj->LocalTransform.identity();
		obj->LocalTransform.Translate(_translation);
		obj->LocalTransform.SetRotationFromEuler(_rotation);
		obj->ScaleTransform.ForceScale(_scale);
	}

	void SceneEditor::NotifyViewportNotDrawn()
	{
		viewportOverlayValid = false;
		viewportInputAllowed = false;
		viewportMouseValid = false;
		viewportHovered = false;
	}

	void SceneEditor::UpdateViewportMouse()
	{
		viewportMouseValid = false;
		viewportHovered = false;
		// viewportInputAllowed is what stops this from claiming events that
		// belong to whatever ImGui window is currently over the viewport -
		// see its declaration in SceneEditor.h. It is one frame old when
		// this runs from an InputManager callback (those fire between
		// frames, before the ImGui frame that would refresh it), which is
		// the same staleness viewportImgMin/Size have always had here.
		if (!viewportOverlayValid || !viewportInputAllowed || dim.x < 1.f || dim.y < 1.f
			|| viewportImgSize.x < 1.f || viewportImgSize.y < 1.f)
			return;

		const ImVec2 mp = ImGui::GetIO().MousePos;
		const f32 sx = dim.x / viewportImgSize.x;
		const f32 sy = dim.y / viewportImgSize.y;
		viewportMouse.x = (mp.x - viewportImgMin.x) * sx;
		viewportMouse.y = (mp.y - viewportImgMin.y) * sy;
		viewportMouseValid = (viewportMouse.x >= 0.f && viewportMouse.x < dim.x
			&& viewportMouse.y >= 0.f && viewportMouse.y < dim.y);
		mousePosition = viewportMouse;
		mPos = viewportMouse;
	}

	void SceneEditor::Update(const f64 time)
	{
		UpdateViewportMouse();
		PollUIStyleFiles(time);

		if (playMode)
			physics->Update(time, 10);

		// Outside Play, only the selected emitter simulates - see
		// UpdateParticlePreview(). Cheap: it early-outs unless the selection
		// actually changed.
		if (!playMode)
			UpdateParticlePreview();

		// Update Light / Sound / empty-GO Helpers (editor chrome only).
		GameObject* viewCam = GetViewCameraGO();
		if (!playMode)
		{
			for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); i++)
			{
				if ((*i).second->Helper)
				{
					IHelper* helper = (IHelper*)(*i).second->Helper.get();
					if (helper->type == HELPER_TYPE::LIGHT || helper->type == HELPER_TYPE::SOUND
						|| helper->type == HELPER_TYPE::PARTICLES
						|| GameObjectShowsEmptyHelper(helper->owner))
					{
						helper->rcomp->Enable();
						helper->Update(viewCam, projection.GetProjectionMatrix(), isPerspective, projectionOrtho.Right, projectionOrtho.Top);
					}
					else
						helper->rcomp->Disable();
				}
			}
		}

		// Push properties-panel values before scene->Update. While the gizmo
		// is dragging, Show() owns the transform — do not overwrite here.
		if (!playMode && SelectedSceneObject != NULL
			&& SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT
			&& !gizmoDragging)
		{
			SelectedSceneObject->LocalTransform.identity();
			SelectedSceneObject->LocalTransform.Translate(_translation);
			SelectedSceneObject->LocalTransform.SetRotationFromEuler(_rotation);
			SelectedSceneObject->ScaleTransform.ForceScale(_scale);
			SelectedSceneObject->globalRotation.identity();

			GameObject* go = (GameObject*)SelectedSceneObject->GetPTR();
			Vec3 scale = _scale;
			if (IsSceneCamera(SelectedSceneObject->GetID()))
			{
				// A zero scale makes the world matrix singular — view = inverse(world) then fails.
				if (fabsf(scale.x) < 1e-4f) scale.x = 1.f;
				if (fabsf(scale.y) < 1e-4f) scale.y = 1.f;
				if (fabsf(scale.z) < 1e-4f) scale.z = 1.f;
			}
			go->SetPosition(_translation);
			go->SetRotation(_rotation);
			go->SetScale(scale);
			SyncPhysicsForGameObject(go);
		}

		// Update Scene
		scene->Update(time);
#ifdef LUA_BINDINGS
		if (playMode)
		{
			// Project script first: a boot script that calls loadScene()
			// should get its request in before the scene it is leaving runs
			// another frame of its own logic.
			if (projectMainScript)
			{
				try { projectMainScript->Update(time); }
				catch (const std::exception& e) { echo(std::string("ERROR: Project main script update: ") + e.what()); }
				catch (...) { echo("ERROR: Project main script update failed"); }
			}
			UpdateSceneMainScript(time);
		}
#endif

		// Listener follows the active view camera after scene transforms update.
		if (audio && viewCam)
		{
			f32 listenerDt = 0.f;
			if (lastListenerTime >= 0.0)
				listenerDt = (f32)(time - lastListenerTime);
			lastListenerTime = time;
			audio->SetListenerFromGameObject(viewCam, listenerDt);
		}

		// Send Mouse Coordinates in viewport space
		if (!playMode && axisHelper)
			axisHelper->Update(time, viewCam, Vec2(mPos.x-dim.x+90, mPos.y-10));

		if (!playMode && activeSceneCameraId == 0 && _middleMouse)
		{
			qX.AxisToQuaternion(Vec3(-1.f, 0.f, 0.f), DEGTORAD(mousePosition.y - mouse.y));
			qY.AxisToQuaternion(Vec3(0.f, -1.f, 0.f), DEGTORAD(mousePosition.x - mouse.x));
			rotation = (rotY * qY) * (rotX * qX);
			Matrix m = rotation.ConvertToMatrix();
			m.Translate(CameraPivot->GetPosition());
			CameraPivot->SetTransformationMatrix(m);

		}
		else if (!playMode && activeSceneCameraId == 0 && _rightMouse)
		{
			// Pan
			rotation = rotY * rotX;
			Matrix m = rotation.ConvertToMatrix();
			Matrix m2; m2.Translate((rotY * rotX).ConvertToMatrix()*(pos - Vec3((mousePosition.x - mouse.x) / 75.f, -(mousePosition.y - mouse.y) / 75.f, 0)));
			CameraPivot->SetTransformationMatrix(m2*m);
			if (mousePosition.x - mouse.x != 0 || mousePosition.y - mouse.y != 0)
				_mousePanned = true;
		}

		if (SelectedSceneObject != NULL)
		{
			// Light colour/direction are applied from the Properties widgets
			// when the user edits them — do not push cached UI values every
			// frame (that also avoided re-running shadow rebuild side effects).
		}
		// Debug Draw - accumulates into DebugRenderer, so it is gated the same
		// way as the flush in Show().
#if defined(PYROS_EDITOR_HAS_DEBUG_DRAW)
		if (!playMode)
			DrawBoundings(SelectedSceneObject);
#endif
	}

	void SceneEditor::DrawBoundings(SceneObject* obj)
	{
		if (obj != NULL)
		{
			switch (obj->GetType())
			{
			case SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT:
			case SceneObjectTypes::POINTLIGHT_COMPONENT:
			case SceneObjectTypes::SPOTLIGHT_COMPONENT:
				// Light volume overlays come from EditorDebugDraw; the old
				// DrawBoundingCylinder/Sphere/Cone path flushed through
				// DebugRenderer after the lit pass and was redundant.
				break;
			case SceneObjectTypes::PHYSICS_COMPONENT:
			case SceneObjectTypes::AUDIO_SOURCE_COMPONENT:
			// A ParticleSystem is a RenderingComponent, but its particles live
			// in world space, decoupled from the owner - the owner's bounding
			// box says nothing about where they are.
			case SceneObjectTypes::PARTICLE_SYSTEM_COMPONENT:
#ifdef LUA_BINDINGS
			case SceneObjectTypes::LUA_COMPONENT:
#endif
				// Not GameObjects — do not cast GetPTR() to GameObject*.
				break;
			case SceneObjectTypes::RENDERING_COMPONENT:
			{
				Vec3 minBounds = ((RenderingComponent*)obj->GetPTR())->GetOwner()->GetBoundingMinValue();
				Vec3 maxBounds = ((RenderingComponent*)obj->GetPTR())->GetOwner()->GetBoundingMaxValue();
				DrawBoundingBox(minBounds, maxBounds, ((RenderingComponent*)obj->GetPTR())->GetOwner()->GetWorldTransformation());
			}
			break;
			case SceneObjectTypes::GAMEOBJECT:
			{
				GameObject *go = ((GameObject*)obj->GetPTR());
				if (!go) break;
				for (std::vector<std::shared_ptr<IComponent>>::const_iterator i = go->GetComponents().begin(); i != go->GetComponents().end(); i++)
				{
					SceneObject* child = sceneObjects->GetSceneObject(sceneObjects->GetSceneObjectID((*i).get()));
					if (child) DrawBoundings(child);
				}
			}
			break;
			default:
				break;
			};
		}
	}

	void SceneEditor::DrawBoundingBox(const Vec3 &vec1, const Vec3 &vec2, const Matrix &transform)
	{
		debugRenderer->pushMatrix(transform);
		Vec3 min = vec1;
		Vec3 max = vec2;
		debugRenderer->drawLine(Vec3(min.x, min.y, min.z), Vec3(max.x, min.y, min.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(max.x, min.y, min.z), Vec3(max.x, min.y, max.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(max.x, min.y, max.z), Vec3(min.x, min.y, max.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(min.x, min.y, max.z), Vec3(min.x, min.y, min.z), Vec4(1, 0, 0, 0.5));

		debugRenderer->drawLine(Vec3(min.x, max.y, min.z), Vec3(max.x, max.y, min.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(max.x, max.y, min.z), Vec3(max.x, max.y, max.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(max.x, max.y, max.z), Vec3(min.x, max.y, max.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(min.x, max.y, max.z), Vec3(min.x, max.y, min.z), Vec4(1, 0, 0, 0.5));

		debugRenderer->drawLine(Vec3(min.x, max.y, min.z), Vec3(min.x, min.y, min.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(max.x, max.y, max.z), Vec3(max.x, min.y, max.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(max.x, max.y, min.z), Vec3(max.x, min.y, min.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(min.x, max.y, max.z), Vec3(min.x, min.y, max.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->popMatrix();
	}

	void SceneEditor::DrawBoundingSphere(const f32 radius, const Matrix &transform)
	{
		debugRenderer->pushMatrix(transform);
		debugRenderer->drawSphere(Vec3::ZERO, radius, Vec4(1, 1, 0, 0.5));
		debugRenderer->popMatrix();
	}

	void SceneEditor::DrawBoundingCone(const f32 radius, const f32 height, const Matrix &transform)
	{
		debugRenderer->pushMatrix(transform);
		debugRenderer->drawCone(radius, height, Vec4(1, 1, 0, 0.5));
		debugRenderer->popMatrix();
	}

	void SceneEditor::DrawBoundingCylinder(const f32 radius, const f32 height, const Matrix &transform)
	{
		debugRenderer->pushMatrix(transform);
		debugRenderer->drawCylinder(radius, height, Vec4(1, 1, 0, 0.5));
		debugRenderer->popMatrix();
	}

	void SceneEditor::SyncPhysicsFromScene()
	{
		std::set<GameObject*> synced;
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin();
			i != sceneObjects->GetList().end(); ++i)
		{
			if (i->second == NULL || i->second->GetType() != SceneObjectTypes::PHYSICS_COMPONENT) continue;
			IPhysicsComponent* pcomp = (IPhysicsComponent*)i->second->GetPTR();
			GameObject* owner = pcomp ? pcomp->GetOwner() : NULL;
			if (!pcomp || !owner) continue;
			if (synced.insert(owner).second)
				SyncPhysicsForGameObject(owner);
		}
	}

	void SceneEditor::SyncPhysicsForGameObject(GameObject* go)
	{
		if (!go || !physics || playMode) return;

		const std::vector<std::shared_ptr<IComponent>>& comps = go->GetComponents();
		for (size_t i = 0; i < comps.size(); ++i)
		{
			IPhysicsComponent* pcomp = dynamic_cast<IPhysicsComponent*>(comps[i].get());
			if (!pcomp || !pcomp->RigidBodyRegistered()) continue;
			physics->UpdatePosition(pcomp, go->GetPosition());
			physics->UpdateRotation(pcomp, go->GetRotation());
		}
	}

	namespace {
		void ForEachAudioSourceOnGameObject(GameObject* go,
			void (*fn)(AudioSource*, void*), void* ctx)
		{
			if (!go) return;
			const std::vector<std::shared_ptr<IComponent>> &comps = go->GetComponents();
			for (std::vector<std::shared_ptr<IComponent>>::const_iterator c = comps.begin();
				c != comps.end(); ++c)
			{
				AudioSource* asrc = dynamic_cast<AudioSource*>((*c).get());
				if (asrc) fn(asrc, ctx);
			}
			const std::vector<std::shared_ptr<GameObject>> &children = go->GetChildren();
			for (std::vector<std::shared_ptr<GameObject>>::const_iterator ch = children.begin();
				ch != children.end(); ++ch)
				ForEachAudioSourceOnGameObject((*ch).get(), fn, ctx);
		}

		void ForEachAudioSourceInScene(SceneGraph* sceneGraph,
			void (*fn)(AudioSource*, void*), void* ctx)
		{
			if (!sceneGraph) return;
			const std::vector<std::shared_ptr<GameObject>> roots = sceneGraph->GetAllGameObjectList();
			for (std::vector<std::shared_ptr<GameObject>>::const_iterator i = roots.begin();
				i != roots.end(); ++i)
				ForEachAudioSourceOnGameObject((*i).get(), fn, ctx);
		}

		struct PlayModeAudioCtx { int started; int missing; };

		void StartAudioSourceForPlayMode(AudioSource* asrc, void* ctx)
		{
			PlayModeAudioCtx* c = (PlayModeAudioCtx*)ctx;
			if (!asrc) return;
			if (!asrc->EnsureLoaded())
			{
				c->missing++;
				echo("WARNING: Play mode - sound not loaded: " + asrc->GetFile());
				return;
			}
			asrc->ResetVelocityTracking();
			asrc->Play();
			c->started++;
		}

		void StopAudioSourceForPlayMode(AudioSource* asrc, void*)
		{
			if (asrc && asrc->IsLoaded())
				asrc->Stop();
		}

		void ForEachParticleSystemOnGameObject(GameObject* go,
			void (*fn)(ParticleSystem*, void*), void* ctx)
		{
			if (!go) return;
			const std::vector<std::shared_ptr<IComponent>> &comps = go->GetComponents();
			for (std::vector<std::shared_ptr<IComponent>>::const_iterator c = comps.begin();
				c != comps.end(); ++c)
			{
				ParticleSystem* ps = dynamic_cast<ParticleSystem*>((*c).get());
				if (ps) fn(ps, ctx);
			}
			const std::vector<std::shared_ptr<GameObject>> &children = go->GetChildren();
			for (std::vector<std::shared_ptr<GameObject>>::const_iterator ch = children.begin();
				ch != children.end(); ++ch)
				ForEachParticleSystemOnGameObject((*ch).get(), fn, ctx);
		}

		void ForEachParticleSystemInScene(SceneGraph* sceneGraph,
			void (*fn)(ParticleSystem*, void*), void* ctx)
		{
			if (!sceneGraph) return;
			const std::vector<std::shared_ptr<GameObject>> roots = sceneGraph->GetAllGameObjectList();
			for (std::vector<std::shared_ptr<GameObject>>::const_iterator i = roots.begin();
				i != roots.end(); ++i)
				ForEachParticleSystemOnGameObject((*i).get(), fn, ctx);
		}

		// Emitters run in the editor viewport too (that is the whole point of
		// authoring one), so entering play mode has to discard whatever the
		// preview left lying around and start the effect from nothing - and,
		// for a one-shot burst, is what actually fires it.
		void StartParticleSystemForPlayMode(ParticleSystem* ps, void*)
		{
			if (!ps) return;
			ps->Clear();
			ps->Play();
		}

		// Leaving Play returns every emitter to the edit-mode default:
		// idle. UpdateParticlePreview() immediately restarts whichever one
		// the current selection asks for, so a selected emitter keeps
		// previewing and everything else goes quiet.
		void StopParticleSystemForPlayMode(ParticleSystem* ps, void*)
		{
			if (!ps) return;
			ps->Stop();
			ps->Clear();
		}
	}

	void SceneEditor::SetAsActiveAudioDevice()
	{
		if (audio && audio->IsInitialized())
			AudioManager::MakeActive(audio);
	}

	std::string SceneEditor::ResolveSoundPath(const std::string& path) const
	{
		if (path.empty()) return path;
		namespace fs = std::filesystem;
		std::error_code ec;
		const fs::path p(path);
		if (p.is_absolute() && fs::exists(p, ec))
			return path;
		if (project && project->IsOpen())
		{
			const std::string abs = project->AbsolutePath(path);
			if (fs::exists(abs, ec))
				return abs;
		}
		if (fs::exists(p, ec))
			return p.string();
		return path;
	}

	std::string SceneEditor::ResolveScriptPath(const std::string& path) const
	{
		return ResolveSoundPath(path);
	}

	std::string SceneEditor::ResolveAssetPath(const std::string& path) const
	{
		return ResolveSoundPath(path);
	}

	std::shared_ptr<Texture> SceneEditor::LoadParticleTexture(const std::string& path)
	{
		std::shared_ptr<Texture> tex = std::make_shared<Texture>();
		if (!path.empty() && tex->LoadTexture(ResolveAssetPath(path), TextureType::Texture))
			return tex;

		if (!path.empty())
			echo("WARNING: could not load particle sprite '" + path + "', using the default one");
		// Ships with the editor, so it always exists - but a scene saved with
		// it references an editor path no exported game would have, hence the
		// import into the project below whenever there is one.
		tex = std::make_shared<Texture>();
		if (tex->LoadTexture("assets/particle_default.png", TextureType::Texture))
			return tex;
		echo("ERROR: could not load the default particle sprite (assets/particle_default.png)");
		return std::shared_ptr<Texture>();
	}

	std::string SceneEditor::ImportParticleTexture(const std::string& path)
	{
		const std::string source = path.empty() ? std::string("assets/particle_default.png") : path;
		if (!project || !project->IsOpen())
			return source;

		const std::string absolute = ResolveAssetPath(source);
		// RelativePath() is empty for anything outside the project, so a
		// non-empty result means the sprite already lives there (picked from
		// the Assets panel, or re-used from another emitter) - copying it
		// again would just fork it.
		const std::string alreadyInProject = project->RelativePath(absolute);
		if (!alreadyInProject.empty())
			return alreadyInProject;

		std::string imported, err;
		if (project->ImportAssetFile(absolute, imported, &err))
			return project->RelativePath(imported);
		echo("WARNING: could not import particle sprite into the project (" + err + "), referencing it in place");
		return source;
	}

	void SceneEditor::ApplyParticlePreset(ParticleSystemDesc& desc, int32 preset)
	{
		switch (preset)
		{
		case 1: // Fire - fast, short-lived, shrinking, additive embers
			desc.looping = true;
			desc.emissionRate = 60.f;
			desc.burstCount = 1;
			desc.minLifetime = 0.6f; desc.maxLifetime = 1.2f;
			desc.direction = Vec3(0.f, 1.f, 0.f);
			desc.spreadAngle = (f32)DEGTORAD(20.0);
			desc.minSpeed = 1.2f; desc.maxSpeed = 2.4f;
			desc.gravity = Vec3(0.f, 0.6f, 0.f);
			desc.damping = 0.8f;
			desc.startSize = 0.7f; desc.endSize = 0.15f;
			desc.startColor = Vec4(1.f, 0.75f, 0.25f, 1.f);
			desc.endColor = Vec4(0.9f, 0.15f, 0.05f, 0.f);
			desc.fadeInFraction = 0.08f; desc.fadeOutFraction = 0.45f;
			desc.blendMode = ParticleBlendMode::Additive;
			break;
		case 2: // Smoke - slow, long-lived, growing, drifting
			desc.looping = true;
			desc.emissionRate = 12.f;
			desc.burstCount = 1;
			desc.minLifetime = 3.f; desc.maxLifetime = 6.f;
			desc.direction = Vec3(0.f, 1.f, 0.f);
			desc.spreadAngle = (f32)DEGTORAD(25.0);
			desc.minSpeed = 0.3f; desc.maxSpeed = 0.8f;
			desc.gravity = Vec3(0.f, 0.15f, 0.f);
			desc.damping = 0.3f;
			desc.startSize = 0.6f; desc.endSize = 2.5f;
			desc.startColor = Vec4(0.5f, 0.5f, 0.55f, 0.5f);
			desc.endColor = Vec4(0.25f, 0.25f, 0.3f, 0.f);
			desc.fadeInFraction = 0.2f; desc.fadeOutFraction = 0.4f;
			desc.minRotationSpeed = -0.4f; desc.maxRotationSpeed = 0.4f;
			desc.blendMode = ParticleBlendMode::AlphaBlend;
			break;
		case 3: // Explosion - a one-shot burst in every direction
			desc.looping = false;
			desc.burstCount = 80;
			desc.minLifetime = 0.5f; desc.maxLifetime = 1.4f;
			desc.direction = Vec3(0.f, 1.f, 0.f);
			desc.spreadAngle = (f32)DEGTORAD(180.0);
			desc.minSpeed = 3.f; desc.maxSpeed = 8.f;
			desc.gravity = Vec3(0.f, -4.f, 0.f);
			desc.damping = 1.6f;
			desc.startSize = 0.8f; desc.endSize = 0.2f;
			desc.startColor = Vec4(1.f, 0.9f, 0.6f, 1.f);
			desc.endColor = Vec4(1.f, 0.3f, 0.05f, 0.f);
			desc.fadeInFraction = 0.02f; desc.fadeOutFraction = 0.35f;
			desc.blendMode = ParticleBlendMode::Additive;
			break;
		default:
			// ParticleSystemDesc()'s own defaults, deliberately left alone.
			break;
		}
	}

	SceneObject* SceneEditor::AttachParticleSystem(GameObject* go, const ParticleSystemDesc& desc)
	{
		if (go == NULL) return NULL;
		SceneObject* obj = sceneObjects->CreateParticleSystem(go, desc);
		if (obj == NULL) return NULL;
		std::shared_ptr<ParticleHelper> h = std::make_shared<ParticleHelper>(go);
		obj->Helper = h;
		scene->Add(h);
		// A ParticleSystem constructs itself playing. Outside Play that is
		// the wrong default (see UpdateParticlePreview) - hand it over idle
		// and let the preview rule decide, which for a just-added emitter
		// means it starts previewing as soon as its owner is selected.
		if (!playMode)
		{
			ParticleSystem* ps = (ParticleSystem*)obj->GetPTR();
			ps->Stop();
			ps->Clear();
			particlePreviewSynced = false;
		}
		return obj;
	}

	bool SceneEditor::ParticleSystemPreviewsForSelection(ParticleSystem* ps) const
	{
		if (ps == NULL || SelectedSceneObject == NULL) return false;
		// The emitter component itself, and nothing else. Selecting the host
		// GameObject used to count too, which meant every emitter in the
		// scene lit up as soon as its object was touched for an unrelated
		// reason (moving it, renaming it, clicking it on the way to
		// something else) and made the viewport look like particles were
		// firing at random. Outside Play, a preview is something you ask for
		// by selecting the emitter - the icon in the viewport selects it
		// directly now (see ViewportPickAtMouse), so it is one click either
		// way.
		if (SelectedSceneObject->GetType() == SceneObjectTypes::PARTICLE_SYSTEM_COMPONENT)
			return SelectedSceneObject->GetPTR() == (void*)ps;
		return false;
	}

	void SceneEditor::UpdateParticlePreview()
	{
		const uint32 selectionId = (SelectedSceneObject != NULL) ? SelectedSceneObject->GetID() : 0;
		if (particlePreviewSynced && selectionId == particlePreviewSelectionId)
		{
			// A one-shot emitter fires once and is then empty for as long as
			// it stays selected, so its "preview" was a single burst you had
			// to keep reselecting to see again. Re-fire it whenever it runs
			// dry - a preview is meant to show what the emitter does, and a
			// looping one already does exactly this on its own.
			if (particlePreviewSystem != NULL
				&& !particlePreviewSystem->GetDesc().looping
				&& particlePreviewSystem->GetLiveParticleCount() == 0)
			{
				particlePreviewSystem->Play();
			}
			return;
		}
		particlePreviewSelectionId = selectionId;
		particlePreviewSynced = true;
		particlePreviewSystem = NULL;

		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin();
			i != sceneObjects->GetList().end(); ++i)
		{
			if (!i->second || i->second->GetType() != SceneObjectTypes::PARTICLE_SYSTEM_COMPONENT) continue;
			ParticleSystem* ps = (ParticleSystem*)i->second->GetPTR();
			if (!ps) continue;
			if (ParticleSystemPreviewsForSelection(ps))
			{
				// Clear first so a one-shot burst actually fires on select
				// rather than adding to whatever the last preview left.
				ps->Clear();
				ps->Play();
				particlePreviewSystem = ps;
			}
			else
			{
				ps->Stop();
				ps->Clear();
			}
		}
	}

	void SceneEditor::ResetParticlePreview()
	{
		ForEachParticleSystemInScene(scene, StopParticleSystemForPlayMode, NULL);
		particlePreviewSynced = false;
		particlePreviewSystem = NULL;
	}

	void SceneEditor::SetEditorChromeVisible(bool visible)
	{
		if (rGrid)
		{
			if (visible) rGrid->Enable();
			else rGrid->Disable();
		}
		if (!sceneObjects) return;
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin();
			i != sceneObjects->GetList().end(); ++i)
		{
			if (!i->second || !i->second->Helper) continue;
			IHelper* helper = (IHelper*)i->second->Helper.get();
			if (!helper || !helper->rcomp) continue;
			if (visible)
			{
				if (helper->type == HELPER_TYPE::LIGHT || helper->type == HELPER_TYPE::SOUND
					|| helper->type == HELPER_TYPE::PARTICLES
					|| GameObjectShowsEmptyHelper(helper->owner))
					helper->rcomp->Enable();
				else
					helper->rcomp->Disable();
			}
			else
				helper->rcomp->Disable();
		}
	}

	void SceneEditor::ResolvePlayModeCamera()
	{
		playModeSavedCameraId = activeSceneCameraId;

		// Prefer an already-valid scene camera; otherwise the first registered one.
		if (activeSceneCameraId != 0 && IsSceneCamera(activeSceneCameraId))
		{
			SceneObject* so = sceneObjects->GetSceneObject(activeSceneCameraId);
			if (so != NULL)
			{
				echo(std::string("SUCCESS: Play mode using scene camera \"") + so->GetName() + "\"");
				return;
			}
		}

		for (std::map<uint32, EditorCameraSettings>::const_iterator i = sceneCameras.begin();
			i != sceneCameras.end(); ++i)
		{
			SceneObject* so = sceneObjects->GetSceneObject(i->first);
			if (so == NULL || so->GetType() != SceneObjectTypes::GAMEOBJECT) continue;
			GameObject* go = (GameObject*)so->GetPTR();
			if (go == NULL || IsInternalGameObject(go)) continue;
			activeSceneCameraId = i->first;
			echo(std::string("SUCCESS: Play mode using scene camera \"") + so->GetName() + "\"");
			return;
		}

		activeSceneCameraId = 0;
		echo("WARNING: Play mode - no camera in the scene; using the editor camera");
	}

	void SceneEditor::InitSceneLuaComponents()
	{
		lastSceneLuaInitCount = 0;
#ifdef LUA_BINDINGS
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin();
			i != sceneObjects->GetList().end(); ++i)
		{
			if (!i->second || i->second->GetType() != SceneObjectTypes::LUA_COMPONENT) continue;
			LuaComponent* lc = (LuaComponent*)i->second->GetPTR();
			if (!lc) continue;
			try {
				lc->Init();
				++lastSceneLuaInitCount;
				if (!lc->scriptFile.empty())
					echo(std::string("SUCCESS: Lua init — ") + lc->scriptFile);
			}
			catch (const std::exception& e) { echo(std::string("ERROR: LuaComponent::Init - ") + e.what()); }
			catch (...) { echo("ERROR: LuaComponent::Init - unknown exception"); }
		}
#endif
	}

#ifdef LUA_BINDINGS
	void SceneEditor::StartProjectMainScript()
	{
		projectMainScript.reset();
		projectMainScriptPath.clear();
		if (!sharedLua || !project || !project->IsOpen()) return;
		const std::string rel = project->GetSettings().defaultMainScript;
		if (rel.empty()) return;

		projectMainScriptPath = (rel.find('/') == 0 || (rel.size() > 1 && rel[1] == ':'))
			? rel : project->AbsolutePath(rel);
		std::error_code ec;
		if (!std::filesystem::exists(projectMainScriptPath, ec))
		{
			echo("ERROR: Project main script not found: " + projectMainScriptPath);
			projectMainScriptPath.clear();
			return;
		}
		try {
			projectMainScript = LuaComponent_FromFile(*sharedLua, projectMainScriptPath);
		}
		catch (const std::exception& e) {
			echo(std::string("ERROR: Project main script load: ") + e.what());
			projectMainScript.reset();
			projectMainScriptPath.clear();
			return;
		}
		catch (...) {
			echo("ERROR: Project main script load failed");
			projectMainScript.reset();
			projectMainScriptPath.clear();
			return;
		}
		if (!projectMainScript) return;
		try { projectMainScript->ResetLifecycle(); projectMainScript->Init(); }
		catch (const std::exception& e) { echo(std::string("ERROR: Project main script Init: ") + e.what()); }
		catch (...) { echo("ERROR: Project main script Init failed"); }
		echo("SUCCESS: Project main script — " + projectMainScriptPath);
	}

	void SceneEditor::ApplyPendingSceneLoadIfAny()
	{
		if (pendingLoadSceneName.empty()) return;
		const std::string name = pendingLoadSceneName;
		pendingLoadSceneName.clear();
		if (!playMode) return; // loadScene() is a play-mode API; ignore stragglers

		// Resolve the way a game author would name it: "Level2", or an
		// explicit project-relative path if they prefer.
		std::string rel = name;
		if (rel.size() < 5 || rel.compare(rel.size() - 5, 5, ".json") != 0)
			rel = "scenes/" + rel + ".json";
		const std::string abs = (project && project->IsOpen()) ? project->AbsolutePath(rel) : rel;
		std::error_code ec;
		if (!std::filesystem::exists(abs, ec))
		{
			echo("ERROR: loadScene('" + name + "') - no such scene: " + abs);
			return;
		}

		// The scene graph is about to be replaced wholesale, so everything
		// hanging off the old one goes first. projectMainScript deliberately
		// survives - that persistence is the point of it.
		static_cast<Box3DPhysics*>(physics)->SetSimulationEnabled(false);
		sceneMainScript.reset();
		sceneMainScriptPath.clear();
		DeselectSceneObject();

		// Snapshots key on the OLD scene's object ids; a later real stop must
		// not try to restore them onto a different graph.
		playModeSnapshots.clear();
		loadingSceneForPlay = true;
		const bool ok = LoadSceneFromFile(abs);
		loadingSceneForPlay = false;
		if (!ok)
		{
			echo("ERROR: loadScene('" + name + "') failed to load " + abs);
			static_cast<Box3DPhysics*>(physics)->SetSimulationEnabled(true);
			return;
		}

		// Put the fresh graph into the same state EnterPlayMode() leaves one
		// in - LoadSceneFromFile() builds it for editing, not for playing.
		SetEditorChromeVisible(false);
		ResolvePlayModeCamera();
		SyncPhysicsFromScene();
		static_cast<Box3DPhysics*>(physics)->SetSimulationEnabled(true);
		PushLuaHostGlobals();
		LuaComponent::SetUpdatesEnabled(true);
		ResetLuaComponentsLifecycle();
		InitSceneLuaComponents();
		ResetSceneMainScriptLifecycle();
		InitSceneMainScript();
		editorDisabled = true;
		echo("SUCCESS: loadScene('" + name + "')");
	}
#endif

	void SceneEditor::EnterPlayMode()
	{
		if (playMode) return;
		scriptRenderCamera = nullptr;
		echo("SUCCESS: Entering play mode");
		// Every mutator already refuses to run while playMode is true, so
		// nothing new can be pushed during play - this just protects
		// against replaying stale edit-mode undo history against the
		// post-play state StopPlayMode() restores.
		sceneUndo.Clear();
		playModeSnapshots.clear();
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin();
			i != sceneObjects->GetList().end(); ++i)
		{
			if (i->second == NULL || i->second->GetType() != SceneObjectTypes::GAMEOBJECT) continue;
			GameObject* go = (GameObject*)i->second->GetPTR();
			if (IsInternalGameObject(go)) continue;
			PlayModeObjectSnapshot snap;
			snap.position = go->GetPosition();
			snap.rotation = go->GetRotation();
			snap.scale = go->GetScale();
			snap.localTransform = i->second->LocalTransform;
			snap.scaleTransform = i->second->ScaleTransform;
			snap.globalRotation = i->second->globalRotation;
			playModeSnapshots[i->second->GetID()] = snap;
		}
		ResolvePlayModeCamera();
		SetEditorChromeVisible(false);
		SyncPhysicsFromScene();
		static_cast<Box3DPhysics*>(physics)->SetSimulationEnabled(true);
		StopAssetSoundPreview();
		SetAsActiveAudioDevice();
		PlayModeAudioCtx audioCtx = { 0, 0 };
		ForEachAudioSourceInScene(scene, StartAudioSourceForPlayMode, &audioCtx);
		if (audioCtx.started == 0 && audioCtx.missing > 0)
			echo("ERROR: Play mode - no sounds could play (files missing or audio device unavailable)");
		else if (audioCtx.started > 0)
			echo("SUCCESS: Play mode started " + std::to_string(audioCtx.started) + " sound(s)");
		ForEachParticleSystemInScene(scene, StartParticleSystemForPlayMode, NULL);
		lastListenerTime = -1.0;
		gizmoDragging = false;
		_leftMouse = false;
		playMode = true;
		editorDisabled = true;
#ifdef LUA_BINDINGS
		PushLuaHostGlobals();
		if (sharedLua)
			(*sharedLua)["editorAutoCapture"] = true;
		// Tab is both ImGui nav and FlyCamera's capture toggle — disable
		// keyboard nav while playing so game scripts get Tab/WASD cleanly.
		if (ImGui::GetCurrentContext() != NULL)
			ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
		LuaComponent::SetUpdatesEnabled(true);
		ResetLuaComponentsLifecycle();
		// Explicit Init so camera scripts register input before the first frame.
		const int luaInited = (InitSceneLuaComponents(), lastSceneLuaInitCount);
		if (luaInited == 0)
			echo("WARNING: Play mode — no LuaComponent on any GameObject (attach a script and Save scene)");
		else
			echo("SUCCESS: Play mode — Tab toggles mouse capture; Esc stops play");
		ResetSceneMainScriptLifecycle();
		InitSceneMainScript();
		StartProjectMainScript();
#endif
	}

	void SceneEditor::StopPlayMode()
	{
		if (!playMode) return;
		scriptRenderCamera = nullptr;
		echo("SUCCESS: Stopping play mode");
#ifdef LUA_BINDINGS
		LuaComponent::SetUpdatesEnabled(false);
		ResetSceneMainScriptLifecycle();
		ResetLuaComponentsLifecycle();
		// Before releasing the mouse below, not after: a script's Input
		// handlers outlive its destroy() (the bridge is a Lua-owned
		// userdata, freed whenever the GC gets to it - see
		// LuaInputBridge::ClearAllCallbacks), and a fly-camera's still-live
		// mouse-move handler re-warping the cursor to the window centre is
		// exactly what made Stop look like it never released the mouse.
		LuaInputBridge::ClearAllCallbacks();
		if (sharedLua)
		{
			(*sharedLua)["editorAutoCapture"] = false;
			sol::object fn = (*sharedLua)["setMouseCaptured"];
			if (fn.get_type() == sol::type::function)
				fn.as<sol::protected_function>()(false);
		}
		if (ImGui::GetCurrentContext() != NULL)
			ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#endif
		ForEachAudioSourceInScene(scene, StopAudioSourceForPlayMode, NULL);
		// Every emitter goes idle; the next UpdateParticlePreview() restarts
		// whichever one the restored selection asks for.
		ForEachParticleSystemInScene(scene, StopParticleSystemForPlayMode, NULL);
		particlePreviewSynced = false;
		for (std::map<uint32, PlayModeObjectSnapshot>::iterator i = playModeSnapshots.begin();
			i != playModeSnapshots.end(); ++i)
		{
			SceneObject* obj = sceneObjects->GetSceneObject(i->first);
			if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) continue;
			GameObject* go = (GameObject*)obj->GetPTR();
			if (!go) continue;
			go->SetPosition(i->second.position);
			go->SetRotation(i->second.rotation);
			go->SetScale(i->second.scale);
			obj->LocalTransform = i->second.localTransform;
			obj->ScaleTransform = i->second.scaleTransform;
			obj->globalRotation = i->second.globalRotation;
		}
		// SyncPhysicsForGameObject skips while playMode is true — clear it
		// before pushing restored transforms back into the physics world.
		playMode = false;
#ifdef LUA_BINDINGS
		// The project script's whole point is outliving scene loads, so it is
		// dropped here rather than anywhere in the scene-load path - a play
		// session is its lifetime.
		projectMainScript.reset();
		projectMainScriptPath.clear();
		pendingLoadSceneName.clear();
#endif
		static_cast<Box3DPhysics*>(physics)->SetSimulationEnabled(false);
		SyncPhysicsFromScene();
		playModeSnapshots.clear();
		// Restore edit-mode camera preference (0 = editor camera).
		if (playModeSavedCameraId != 0 && IsSceneCamera(playModeSavedCameraId))
			activeSceneCameraId = playModeSavedCameraId;
		else
			activeSceneCameraId = 0;
		playModeSavedCameraId = 0;
		SetEditorChromeVisible(true);
		editorDisabled = false;
		if (SelectedSceneObject != NULL && SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT)
			SelectSceneObject(SelectedSceneObject);
	}

	void SceneEditor::PrepareGizmoForDraw(GameObject* viewCam)
	{
		if (!viewCam || playMode || !gizmo || !SelectedSceneObject
			|| SelectedSceneObject->GetType() != SceneObjectTypes::GAMEOBJECT)
			return;

		GameObject* selGo = (GameObject*)SelectedSceneObject->GetPTR();
		const bool hasParent = selGo->HaveParent();
		Matrix liveParentWorld = hasParent ? selGo->GetParent()->GetWorldTransformation() : Matrix();
		// Child anchor: compose the parent's (stable) world matrix with the
		// editor's live LocalTransform. The gizmo edits LocalTransform in
		// place while dragging (move updates it every frame; rotate/scale
		// keep its translation), so this tracks the mouse exactly. The GO's
		// world matrix is only rebuilt in scene->Update() and would lag a
		// frame behind, and a drag-start baseline would freeze the gizmo at
		// its initial spot.
		Matrix anchorWorld = hasParent
			? (liveParentWorld * SelectedSceneObject->LocalTransform)
			: SelectedSceneObject->LocalTransform;
		Matrix parentWorld = liveParentWorld;

		// Everything below keys off the projection actually in use, not off
		// isPerspective. Under an orthographic projection the gizmo's
		// perspective branches are both wrong: ComputeScreenFactor() falls
		// back to the clip w, which ortho pins at 1 - so the gizmo drew at a
		// fixed 0.15 world units no matter how far out the view was zoomed -
		// and BuildRay() unprojects through m_Proj[0][0], which for an ortho
		// matrix is 1/halfWidth and yields a ray pointing nowhere near the
		// cursor, so no axis ever highlighted.
		Projection &viewProj = isPerspective ? projection : projectionOrtho;
		gizmo->SetDisplayScale(viewIsOrtho ? .22f : .15f);

		Mouse3D mray;
		mray.GenerateRay(dim.x, dim.y, viewportMouse.x, viewportMouse.y, Matrix(),
			viewCam->GetWorldTransformation().Inverse(), viewProj.GetProjectionMatrix());
		gizmo->SetOrthoMouse(mray.GetOrigin().x, mray.GetOrigin().y, mray.GetOrigin().z,
			mray.GetDirection().x, mray.GetDirection().y, mray.GetDirection().z);
		gizmo->SetScreenDimension(dim.x, dim.y, !viewIsOrtho,
			viewOrthoL, viewOrthoR, viewOrthoB, viewOrthoT);

		// In a 2D scene, Z is draw order, not a thing you drag - and under the
		// orthographic camera such a scene is viewed with, the Z arrow points
		// straight at the viewer, so it drew as a dot sitting on the origin
		// that still took clicks aimed at X and Y. Mask it off; rotation keeps
		// Z, because rotating *in* the plane is the one rotation 2D wants and
		// it is the Z axis that does it.
		if (sceneIsTwoD)
			gizmo->SetAxisMask(GizmoInUse == GizmoFunction::ROTATION
				? (IGizmo::AXIS_Z | IGizmo::AXIS_SCREEN)
				: (IGizmo::AXIS_X | IGizmo::AXIS_Y));
		else
			gizmo->SetAxisMask(IGizmo::AXIS_ALL);

		if (GizmoInUse == GizmoFunction::SCALE) {
			if (hasParent)
				gizmo->SetLocalTransform((float*)&anchorWorld.m);
			else
				gizmo->SetLocalTransform((float*)&SelectedSceneObject->LocalTransform.m);
			gizmo->SetEditMatrix((float*)&SelectedSceneObject->ScaleTransform.m);
		}
		else if (GizmoInUse == GizmoFunction::ROTATION && !localTransform)
		{
			if (hasParent)
				gizmo->SetLocalTransform((float*)&anchorWorld.m);
			else
				gizmo->SetLocalTransform((float*)&SelectedSceneObject->LocalTransform.m);
			gizmo->SetEditMatrix((float*)&SelectedSceneObject->globalRotation.m);
		}
		else {
			if (hasParent)
			{
				gizmo->SetLocalTransform((float*)&anchorWorld.m);
				gizmo->SetGlobalTransform((float*)&parentWorld.m);
			}
			else {
				gizmo->SetLocalTransform((float*)&SelectedSceneObject->LocalTransform.m);
				gizmo->SetGlobalTransform((float*)Matrix().m);
			}
			gizmo->SetEditMatrix((float*)&SelectedSceneObject->LocalTransform.m);
		}

		gizmo->SetCameraMatrix(viewCam->GetWorldTransformation().Inverse().m,
			viewProj.GetProjectionMatrix().m);
	}

	Matrix SceneEditor::LocalizeWorldRotation(const Matrix &worldDelta)
	{
		// Conjugates a world-space rotation delta into the selected object's
		// local (parent) frame: P^-1 * delta * P. Only the parent's rotation
		// part takes part - its translation/scale are irrelevant to the
		// conjugation and would corrupt it for a scaled parent.
		if (!SelectedSceneObject || SelectedSceneObject->GetType() != SceneObjectTypes::GAMEOBJECT)
			return worldDelta;
		GameObject* go = (GameObject*)SelectedSceneObject->GetPTR();
		if (!go || !go->HaveParent())
			return worldDelta;
		GameObject* parent = go->GetParent();
		Vec3 parentScale = parent->GetScale();
		if (fabs(parentScale.x) < 0.0001f) parentScale.x = 1.0f;
		if (fabs(parentScale.y) < 0.0001f) parentScale.y = 1.0f;
		if (fabs(parentScale.z) < 0.0001f) parentScale.z = 1.0f;
		Matrix parentRot = parent->GetWorldTransformation().GetRotation(parentScale);
		return parentRot.Inverse() * worldDelta * parentRot;
	}

	void SceneEditor::ApplyGizmoTransformToObject()
	{
		if (!SelectedSceneObject || SelectedSceneObject->GetType() != SceneObjectTypes::GAMEOBJECT)
			return;

		// World rotate: libgizmo writes a delta into globalRotation around a
		// world axis. Bake it onto the *orientation* of LocalTransform and
		// keep translation — Pyros Matrix multiply would otherwise rotate the
		// position around the world origin (R * T). When the object has a
		// parent the local frame is the parent's, so the world delta must be
		// conjugated by the parent's rotation first (P^-1 * delta * P).
		Matrix baked = SelectedSceneObject->LocalTransform;
		if (GizmoInUse == GizmoFunction::ROTATION && !localTransform)
		{
			const Vec3 pos = SelectedSceneObject->LocalTransform.GetTranslation();
			Matrix orient = SelectedSceneObject->LocalTransform;
			orient.Translate(Vec3(0, 0, 0));
			baked = LocalizeWorldRotation(SelectedSceneObject->globalRotation) * orient;
			baked.Translate(pos);
		}

		Vec3 pos = baked.GetTranslation();
		Vec3 scl = SelectedSceneObject->ScaleTransform.GetScale();
		// LocalTransform carries no scale (it is T*R; the scale lives in
		// ScaleTransform), so the 3x3 of baked is a pure rotation and the
		// Euler angles are read off directly. Dividing by baked.GetScale()
		// treated the rotation's diagonal as scale and produced garbage
		// angles for any non-trivial rotation.
		Vec3 rot = baked.GetEulerFromRotationMatrix();

		SetObjectProperties(pos, rot, scl);
		GameObject* go = (GameObject*)SelectedSceneObject->GetPTR();
		go->SetPosition(_translation);
		go->SetRotation(_rotation);
		go->SetScale(_scale);
		SyncPhysicsForGameObject(go);
		MarkSceneDirty();
	}

	void SceneEditor::ViewportPickAtMouse()
	{
		// Icons first: a light or a camera has no geometry of its own, so its
		// billboard is the only thing there is to click. They are drawn on top
		// of the scene, so picking them on top of it matches what is on screen.
		uint32 iconPickId = 0;
		if (TryPickViewportIcon(viewportMouse, iconPickId))
		{
			DeselectMesh();
			SceneObject* iconSO = sceneObjects->GetSceneObject(iconPickId);
			SelectSceneObject(iconSO);
			node_clicked = iconPickId;
			// A light icon selects the light *component*, which lives under
			// its GameObject in the tree - open that node so the selection is
			// actually visible instead of hidden inside a collapsed parent.
			if (iconSO != NULL && iconSO->GetParentID() != 0)
				hierarchyForceOpenId = iconSO->GetParentID();
			return;
		}

		Picking->Resize((uint32)dim.x, (uint32)dim.y);
		Picking->ResetViewPort();
		Picking->SetViewPort(0, 0, (uint32)dim.x, (uint32)dim.y);
		RenderingMesh* rm = Picking->PickObject(viewportMouse.x, viewportMouse.y,
			(isPerspective ? projection : projectionOrtho), GetViewCameraGO(), scene);

		if (rm == NULL || rm->renderingComponent == NULL)
		{
			// Empty space - clear the selection, the way every other editor
			// does. Leaving the previous object selected made a missed click
			// indistinguishable from a click that did nothing.
			DeselectMesh();
			DeselectSceneObject();
			selection.clear();
			node_clicked = -1;
			return;
		}

		// A helper (the clickable stand-in body drawn for a light, sound
		// emitter, particle emitter or empty GameObject) selects the scene
		// object it was created for - which is the map key here, and is the
		// *component* for a light/sound/particle icon rather than the
		// GameObject hosting it. Resolving via the helper's `owner` instead
		// (which is only ever the host GameObject, because that is what the
		// icon has to follow in space) collapsed every component icon onto
		// its GameObject: clicking a particle emitter could not select the
		// emitter, only the object it hangs off, which is also what the
		// preview rule in ParticleSystemPreviewsForSelection() keys on.
		bool helper = false;
		uint32 helperSceneObjectId = 0;
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin();
			i != sceneObjects->GetList().end(); i++)
		{
			if ((*i).second->Helper)
			{
				if ((*i).second->Helper.get() == rm->renderingComponent->GetOwner())
				{
					node_clicked = (*i).first;
					helperSceneObjectId = (*i).first;
					helper = true;
					break;
				}
			}
		}

		if (!helper)
			node_clicked = sceneObjects->GetSceneObjectID(rm->renderingComponent->GetOwner());

		SceneObject* pickedSO = sceneObjects->GetSceneObject(node_clicked);
		if (pickedSO == NULL)
		{
			// A mesh under the cursor was picked, but its owner is not a
			// registered scene object - report it rather than silently doing
			// nothing, which reads as "picking is broken".
			char pickWarn[256];
			snprintf(pickWarn, sizeof(pickWarn),
				"WARNING: picked mesh owner %p is not a registered scene object (node id %u) - nothing selected",
				(void*)rm->renderingComponent->GetOwner(), (unsigned)node_clicked);
			echo(pickWarn);
		}

		DeselectMesh();
		SelectSceneObject(pickedSO);
		// A component icon lives under its GameObject in the tree - open that
		// node so the new selection is actually visible rather than hidden
		// inside a collapsed parent, same as the light/camera glyph path.
		if (helperSceneObjectId != 0 && pickedSO != NULL && pickedSO->GetParentID() != 0)
			hierarchyForceOpenId = pickedSO->GetParentID();
	}

	// The whole of what dragging a rect means. rect.x is anchorX0 +
	// offsetMin.x and rect.right is anchorX1 + offsetMax.x, so each edge maps
	// to exactly one offset component - and identically whether that axis is
	// pinned or stretched, which is why there is no special case for either.
	// handle is hy*3+hx over the corners and edges; 4 is the body, which
	// moves both sides of both axes and so preserves the size.
	void SceneEditor::ApplyCanvasDrag(UIRect* rect, int handle, const Vec2& delta)
	{
		if (!rect || handle < 0 || handle > 8) return;
		Vec2 oMin = rect->GetOffsetMin(), oMax = rect->GetOffsetMax();
		const int hx = handle % 3, hy = handle / 3;
		const bool body = (handle == 4);
		if (body || hx == 0) oMin.x += delta.x;
		if (body || hx == 2) oMax.x += delta.x;
		if (body || hy == 0) oMin.y += delta.y;
		if (body || hy == 2) oMax.y += delta.y;
		rect->SetOffsets(oMin, oMax);
	}

	bool SceneEditor::ViewportMouseInCanvas(UICanvas* canvas, Vec2& out) const
	{
		if (!canvas || !viewportMouseValid || dim.x < 1.f || dim.y < 1.f) return false;
		const UIRectValue& c = canvas->GetCanvasRect();
		if (c.width <= 0.f || c.height <= 0.f) return false;
		// The canvas fills the viewport exactly - that is what UIRenderer's
		// ortho box says - so this is a straight proportional map, and it
		// stays correct at any window size or scale mode.
		out = Vec2(viewportMouse.x / dim.x * c.width, viewportMouse.y / dim.y * c.height);
		return true;
	}

	// One place a button's handler is called from, because a click and a key
	// press must not be able to behave differently.
	void SceneEditor::DispatchUIClick(GameObject* clicked)
	{
#ifdef LUA_BINDINGS
		if (!clicked || !sharedLua) return;
		const std::vector<std::shared_ptr<IComponent> > &cs = clicked->GetComponents();
		for (size_t j = 0; j < cs.size(); j++)
		{
			if (!cs[j] || cs[j]->GetComponentType() != ComponentType::UIButton) continue;
			const std::string &handler = static_cast<UIButton*>(cs[j].get())->GetOnClick();
			if (handler.empty()) return;
			sol::protected_function fn = (*sharedLua)[handler];
			if (!fn.valid())
			{
				echo("WARNING: UIButton on '" + clicked->GetName() + "' wants '" + handler + "', which is not a global function");
				return;
			}
			sol::protected_function_result res = fn(clicked->GetName());
			if (!res.valid()) { sol::error e = res; echo(std::string("ERROR: UIButton handler '") + handler + "' - " + e.what()); }
			return;
		}
#else
		(void)clicked;
#endif
	}

	// The same dispatch the player uses - see shared/UIDispatch.h. A UI
	// tested in play mode has to behave the way it will once it ships, and
	// two copies of "which handler does this event call" would not.
	void SceneEditor::DispatchUIEvents(UICanvas* canvas)
	{
#ifdef LUA_BINDINGS
		if (!sharedLua) return;
		uidispatch::Dispatch(canvas, *sharedLua, [](const std::string &m, void*) { echo(m); }, NULL);
#else
		(void)canvas;
#endif
	}

	// Mirrors PyrosPlayer::DispatchUIInput - the editor's play mode has to
	// behave the same way as the game or it is not a preview.
	void SceneEditor::DispatchPlayModeUIInput()
	{
		std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(scene);
		if (canvases.empty()) return;
		const bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left);

		// Same navigation the player gives a built game - ImGui's own
		// The focused widget sees a key first and can claim it - the same
		// rule the player uses, so testing a UI here behaves the way it
		// will when it ships. Typing is ImGui's queued characters, which is
		// the editor's equivalent of the window layer's text hook.
		struct Key { ImGuiKey key; uint32 uiKey; f32 dx, dy; };
		static const Key keyMap[] = {
			{ ImGuiKey_LeftArrow,  UIKey::Left,      -1.f,  0.f },
			{ ImGuiKey_RightArrow, UIKey::Right,      1.f,  0.f },
			{ ImGuiKey_UpArrow,    UIKey::Up,         0.f, -1.f },
			{ ImGuiKey_DownArrow,  UIKey::Down,       0.f,  1.f },
			{ ImGuiKey_Backspace,  UIKey::Backspace,  0.f,  0.f },
			{ ImGuiKey_Delete,     UIKey::Delete,     0.f,  0.f },
			{ ImGuiKey_Home,       UIKey::Home,       0.f,  0.f },
			{ ImGuiKey_End,        UIKey::End,        0.f,  0.f },
			{ ImGuiKey_Escape,     UIKey::Escape,     0.f,  0.f },
		};
		for (size_t n = 0; n < sizeof(keyMap) / sizeof(keyMap[0]); n++)
		{
			if (!ImGui::IsKeyPressed(keyMap[n].key, false)) continue;
			bool claimed = false;
			for (size_t i = canvases.size(); i > 0 && !claimed; i--)
			{
				claimed = canvases[i - 1]->UpdateKey(keyMap[n].uiKey);
				DispatchUIEvents(canvases[i - 1]);
			}
			if (!claimed && (keyMap[n].dx != 0.f || keyMap[n].dy != 0.f))
				for (size_t i = canvases.size(); i > 0; i--)
					if (canvases[i - 1]->MoveFocus(Vec2(keyMap[n].dx, keyMap[n].dy))) break;
		}

		// Characters, from whatever ImGui decoded this frame.
		const ImGuiIO &io = ImGui::GetIO();
		for (int c = 0; c < io.InputQueueCharacters.Size; c++)
		{
			const ImWchar ch = io.InputQueueCharacters[c];
			if (ch < 0x20 || ch > 0x7e) continue;
			const std::string typed(1, (char)ch);
			for (size_t i = canvases.size(); i > 0; i--)
			{
				if (!canvases[i - 1]->GetFocused()) continue;
				canvases[i - 1]->UpdateText(typed);
				DispatchUIEvents(canvases[i - 1]);
				break;
			}
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false))
		{
			bool claimed = false;
			for (size_t i = canvases.size(); i > 0 && !claimed; i--)
			{
				claimed = canvases[i - 1]->UpdateKey(UIKey::Enter);
				DispatchUIEvents(canvases[i - 1]);
			}
			if (!claimed)
				for (size_t i = canvases.size(); i > 0; i--)
					if (GameObject* go = canvases[i - 1]->ActivateFocused())
					{
						DispatchUIEvents(canvases[i - 1]);
						DispatchUIClick(go);
						break;
					}
		}

		// The wheel, to whatever is under the pointer.
		if (viewportMouseValid && io.MouseWheel != 0.f && dim.x >= 1.f && dim.y >= 1.f)
			for (size_t i = canvases.size(); i > 0; i--)
			{
				const UIRectValue &r = canvases[i - 1]->GetCanvasRect();
				if (r.width <= 0.f || r.height <= 0.f) continue;
				canvases[i - 1]->UpdateScroll(
					Vec2(viewportMouse.x / dim.x * r.width, viewportMouse.y / dim.y * r.height), io.MouseWheel);
				DispatchUIEvents(canvases[i - 1]);
				break;
			}

		for (size_t i = canvases.size(); i > 0; i--)
		{
			UICanvas* c = canvases[i - 1];
			const UIRectValue &r = c->GetCanvasRect();
			if (r.width <= 0.f || r.height <= 0.f || dim.x < 1.f || dim.y < 1.f) continue;
			const Vec2 p(viewportMouse.x / dim.x * r.width, viewportMouse.y / dim.y * r.height);
			GameObject* clicked = c->UpdateInput(p, down, viewportMouseValid);
			// Every event, not only the click - a slider dragged and a row
			// picked have handlers too.
			DispatchUIEvents(c);
			if (clicked) { DispatchUIClick(clicked); return; }
		}
	}

	void SceneEditor::HandleCanvasInput(UICanvas* canvas)
	{
		if (playMode || editorDisabled || !canvas) return;

		Vec2 m;
		const bool haveMouse = ViewportMouseInCanvas(canvas, m);
		const UIRectValue& c = canvas->GetCanvasRect();

		// Finish a drag wherever the pointer ended up, including outside the
		// viewport - a drag that silently cancels when you leave the window
		// loses the edit.
		if (canvasDragHandle >= 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			SceneObject* dragged = sceneObjects->GetSceneObject(canvasDragGoId);
			if (!canvasDragBefore.empty() && dragged && dragged->GetType() == SceneObjectTypes::GAMEOBJECT)
				PushUIPropertyUndo(canvasDragGoId, canvasDragBefore,
					CaptureUIProperties((GameObject*)dragged->GetPTR()), "Move UI Element");
			canvasDragBefore = json();
			canvasDragHandle = -1;
			return;
		}

		UIRect* rect = NULL;
		uint32 selectedId = 0;
		if (SelectedSceneObject != NULL && SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT)
		{
			selectedId = SelectedSceneObject->GetID();
			const std::vector<std::shared_ptr<IComponent> >& cs = ((GameObject*)SelectedSceneObject->GetPTR())->GetComponents();
			for (size_t i = 0; i < cs.size(); i++)
				if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
				{ rect = static_cast<UIRect*>(cs[i].get()); break; }
		}

		if (canvasDragHandle >= 0 && rect && haveMouse)
		{
			Vec2 d(m.x - canvasDragLast.x, m.y - canvasDragLast.y);
			// Whole canvas units by default, and the visible grid with Ctrl
			// held - a layout built on fractional units is a layout nobody
			// can reproduce by typing numbers into the inspector.
			const f32 step = ImGui::GetIO().KeyCtrl ? (c.width / 24.f) : 1.f;
			d.x = floorf(d.x / step + 0.5f) * step;
			d.y = floorf(d.y / step + 0.5f) * step;
			if (d.x != 0.f || d.y != 0.f)
			{
				ApplyCanvasDrag(rect, canvasDragHandle, d);
				MarkSceneDirty();
				canvasDragLast = Vec2(canvasDragLast.x + d.x, canvasDragLast.y + d.y);
			}
			return;
		}

		if (!haveMouse || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;

		// A handle of the current selection wins over picking something else,
		// so grabbing a corner that overlaps a sibling does what it looks
		// like it does.
		if (rect)
		{
			const UIRectValue& r = rect->GetRect();
			const f32 grab = c.width / 60.f;
			const f32 xs[3] = { r.x, r.x + r.width * 0.5f, r.Right() };
			const f32 ys[3] = { r.y, r.y + r.height * 0.5f, r.Bottom() };
			for (int hy = 0; hy < 3; hy++)
				for (int hx = 0; hx < 3; hx++)
				{
					if (hx == 1 && hy == 1) continue;
					if (fabsf(m.x - xs[hx]) > grab || fabsf(m.y - ys[hy]) > grab) continue;
					canvasDragHandle = hy * 3 + hx;
					canvasDragLast = m;
					canvasDragGoId = selectedId;
					canvasDragBefore = CaptureUIProperties((GameObject*)SelectedSceneObject->GetPTR());
					return;
				}
			if (r.Contains(m))
			{
				canvasDragHandle = 4;
				canvasDragLast = m;
				canvasDragGoId = selectedId;
				canvasDragBefore = CaptureUIProperties((GameObject*)SelectedSceneObject->GetPTR());
				return;
			}
		}

		// Otherwise this is a pick. Canvas-space rect containment, not the
		// 3D painter pick - screen-space quads have no depth to sort by, and
		// the canvas already knows what is on top.
		if (GameObject* hit = canvas->HitTest(m))
		{
			const uint32 id = sceneObjects->GetSceneObjectID(hit);
			if (id != 0)
			{
				SceneObject* obj = sceneObjects->GetSceneObject(id);
				if (obj) SelectAndFocusSceneObject(obj);
			}
		}
		else
			DeselectSceneObject();
	}

	void SceneEditor::HandleViewportGizmoInput(GameObject* viewCam)
	{
		if (playMode || editorDisabled || !viewportMouseValid || !viewCam || !viewportOverlayValid)
			return;

		const ImVec2 mp = ImGui::GetIO().MousePos;
		const bool hovered = (mp.x >= viewportImgMin.x && mp.x < viewportImgMin.x + viewportImgSize.x
			&& mp.y >= viewportImgMin.y && mp.y < viewportImgMin.y + viewportImgSize.y);
		if (!hovered)
			return;

		if (!SelectedSceneObject || SelectedSceneObject->GetType() != SceneObjectTypes::GAMEOBJECT || !gizmo)
		{
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				ViewportPickAtMouse();
			return;
		}

		PrepareGizmoForDraw(viewCam);

		const unsigned gizmoX = (unsigned)std::min(std::max(0.f, viewportMouse.x), dim.x - 1.f);
		const unsigned gizmoY = (unsigned)std::min(std::max(0.f, viewportMouse.y), dim.y - 1.f);

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			gizmo->OnMouseMove(gizmoX, gizmoY);
			const bool gizmoHit = gizmo->OnMouseDown(gizmoX, gizmoY);
			if (gizmoHit)
			{
				gizmoDragging = true;
				gizmoBaselinePos = _translation;
				gizmoBaselineRot = _rotation;
				gizmoBaselineScale = _scale;
			}
			else
			{
				gizmoDragging = false;
				ViewportPickAtMouse();
			}
		}
		else if (gizmoDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			gizmo->OnMouseMove(gizmoX, gizmoY);
			ApplyGizmoTransformToObject();
		}
		else
		{
			// Axis hover highlight while not dragging.
			gizmo->OnMouseMove(gizmoX, gizmoY);
		}

		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			if (gizmoDragging)
			{
				gizmo->OnMouseUp(gizmoX, gizmoY);
				if (GizmoInUse == GizmoFunction::ROTATION && !localTransform)
				{
					// Fold world delta into LocalTransform (orientation only),
					// conjugating it into the parent's frame when the object
					// has a parent (see LocalizeWorldRotation).
					const Vec3 trans = SelectedSceneObject->LocalTransform.GetTranslation();
					Matrix orient = SelectedSceneObject->LocalTransform;
					orient.Translate(Vec3(0, 0, 0));
					Matrix baked = LocalizeWorldRotation(SelectedSceneObject->globalRotation) * orient;
					const Vec3 rot = baked.GetEulerFromRotationMatrix();
					SelectedSceneObject->LocalTransform.identity();
					SelectedSceneObject->LocalTransform.Translate(trans);
					SelectedSceneObject->LocalTransform.SetRotationFromEuler(rot);
					SelectedSceneObject->globalRotation.identity();
				}
				ApplyGizmoTransformToObject();
				SyncTransformFromGameObject(SelectedSceneObject);
				if (_translation != gizmoBaselinePos || _rotation != gizmoBaselineRot || _scale != gizmoBaselineScale)
					sceneUndo.Push(std::make_unique<SetTransformCommand>(this, SelectedSceneObject->GetID(),
						gizmoBaselinePos, gizmoBaselineRot, gizmoBaselineScale,
						_translation, _rotation, _scale, SelectedSceneObject->GetName()));
			}
			gizmoDragging = false;
			_leftMouse = false;
		}
	}

	void SceneEditor::DeleteComponentById(uint32 objId)
	{
		SceneObject* obj = sceneObjects->GetSceneObject(objId);
		if (!obj || obj->GetType() == SceneObjectTypes::GAMEOBJECT) return;

		// Capture the owner's subtree before detaching, so the edit can be
		// pushed as one ReplaceGameObjectCommand afterward - same mechanism
		// AddFormSubmit's attach-to-existing-GameObject path uses, just the
		// other direction.
		const uint32 ownerId = obj->GetParentID();
		SceneObject* ownerObj = sceneObjects->GetSceneObject(ownerId);
		std::string beforeSnapshot;
		if (ownerObj && ownerObj->GetType() == SceneObjectTypes::GAMEOBJECT)
		{
			beforeSnapshot = SnapshotSubtree(ownerId);
		}

		editorDebugDraw->ForgetComponent((IComponent*)obj->GetPTR());
		if (SelectedSceneObject == obj)
		{
			DeselectMesh();
			DeselectSceneObject();
			selection.clear();
		}
		sceneObjects->DestroySceneObject(objId);
		MarkSceneDirty();

		if (!beforeSnapshot.empty())
			PushReplaceCommand(ownerId, beforeSnapshot, "Detach Component");
	}

	void SceneEditor::DeleteGameObjectById(uint32 objId)
	{
		std::string err;
		OpDeleteGameObject(objId, err);
	}

	void SceneEditor::DeleteSelected()
	{
		if (!SelectedSceneObject || editorDisabled) return;
		if (SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT)
			DeleteGameObjectById(SelectedSceneObject->GetID());
		else
			DeleteComponentById(SelectedSceneObject->GetID());
	}

	uint32 SceneEditor::DuplicateSelected()
	{
		if (!SelectedSceneObject || SelectedSceneObject->GetType() != SceneObjectTypes::GAMEOBJECT) return 0;
		std::string err;
		return OpDuplicateGameObject(SelectedSceneObject->GetID(), err);
	}

    	void SceneEditor::SelectSceneObject(SceneObject* go)
	{
        if (go == NULL) {
            return;
        }
		sceneRootSelected = false;
        gizmoDragging = false;
        SelectedSceneObject = go;
		switch (go->GetType())
		{
			case SceneObjectTypes::GAMEOBJECT:
			{
				SyncTransformFromGameObject(go);
#ifdef LUA_BINDINGS
				propertiesScriptAttachPath.clear();
#endif
			}
			break;
			case SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT:
			{
				PropertiesLightDirection = ((DirectionalLight*)SelectedSceneObject->GetPTR())->GetLightDirection();
				PropertiesLightColor = ((DirectionalLight*)SelectedSceneObject->GetPTR())->GetLightColor();
				AddForm_cs = ((DirectionalLight*)SelectedSceneObject->GetPTR())->IsCastingShadows();
				SeedShadowProperties((DirectionalLight*)SelectedSceneObject->GetPTR());
				PropertiesShadowCascades = (int32)((DirectionalLight*)SelectedSceneObject->GetPTR())->GetNumberCascades();
			}
			break;
			case SceneObjectTypes::POINTLIGHT_COMPONENT:
			{
				PropertiesLightRadius = ((PointLight*)SelectedSceneObject->GetPTR())->GetLightRadius();
				PropertiesLightColor = ((PointLight*)SelectedSceneObject->GetPTR())->GetLightColor();
				AddForm_cs = ((PointLight*)SelectedSceneObject->GetPTR())->IsCastingShadows();
				SeedShadowProperties((PointLight*)SelectedSceneObject->GetPTR());
			}
			break;
			case SceneObjectTypes::SPOTLIGHT_COMPONENT:
			{
				PropertiesLightRadius = ((SpotLight*)SelectedSceneObject->GetPTR())->GetLightRadius();
				PropertiesLightDirection = ((SpotLight*)SelectedSceneObject->GetPTR())->GetLightDirection();
				PropertiesLightColor = ((SpotLight*)SelectedSceneObject->GetPTR())->GetLightColor();
				PropertiesLightInnerCone = ((SpotLight*)SelectedSceneObject->GetPTR())->GetLightInnerCone();
				PropertiesLightOutterCone = ((SpotLight*)SelectedSceneObject->GetPTR())->GetLightOutterCone();
				AddForm_cs = ((SpotLight*)SelectedSceneObject->GetPTR())->IsCastingShadows();
				SeedShadowProperties((SpotLight*)SelectedSceneObject->GetPTR());
			}
			break;
#ifdef LUA_BINDINGS
			case SceneObjectTypes::LUA_COMPONENT:
			{
				propertiesScriptAttachPath.clear();
				LuaComponent* lc = (LuaComponent*)go->GetPTR();
				if (lc && !lc->scriptFile.empty())
				{
					if (project && project->IsOpen())
						propertiesScriptAttachPath = project->RelativePath(lc->scriptFile);
					if (propertiesScriptAttachPath.empty())
						propertiesScriptAttachPath = lc->scriptFile;
				}
			}
			break;
#endif
		};

	}

	// One modal for Open Scene and Save Scene As. A typed path rather than a
	// browser: the editor has no file-listing widget (the original one was
	// never finished), and a path field is honest about that.
	void SceneEditor::DrawSceneFileDialog()
	{
		if (!showingSceneDialog) return;

		const char* title = sceneDialogIsSave ? "Save Scene" : "Open Scene";
		ImGui::SetNextWindowFocus();
		if (!ImGui::IsPopupOpen(title, ImGuiPopupFlags_AnyPopupId))
			ImGui::OpenPopup(title);
		
		if (ImGui::BeginPopupModal(title, &showingSceneDialog,
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
		{
			editorDisabled = true;
			if (project && project->IsOpen())
				ImGui::TextDisabled("Project scenes: %s", project->ScenesPath().c_str());
			ImGui::Text("Scene file (.json):");
			ImGui::SetNextItemWidth(300.f);
			ImGui::InputText("##scenepath", &sceneDialogPath);
			ImGui::SameLine();
			if (!sceneDialogIsSave)
			{
				if (ImGui::Button("Browse..."))
				{
					const std::string start = (project && project->IsOpen()) ? project->ScenesPath() : std::string("");
					ImGui::_priv::OpenLocation(start, "json", &sceneDialogBrowse);
				}
			}
			else if (project && project->IsOpen())
				ImGui::TextDisabled("(name or path under scenes/)");
			else
				ImGui::TextDisabled("(type a name)");

			if (sceneDialogBrowse)
			{
				std::string picked;
				if (ImGui::FilePath("##browse", "", "json", &picked, 1024, &sceneDialogBrowse))
					if (picked.size() > 0) sceneDialogPath = picked;
			}

			if (sceneDialogError.size() > 0)
				ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "%s", sceneDialogError.c_str());

			ImGui::Spacing();
			if (ImGui::Button(sceneDialogIsSave ? "Save" : "Open"))
			{
				std::string path = sceneDialogPath;
				if (project && project->IsOpen())
				{
					const bool hasSep = path.find('/') != std::string::npos || path.find('\\') != std::string::npos;
					if (!hasSep)
					{
						if (path.size() < 5 || path.substr(path.size() - 5) != ".json")
							path += ".json";
						path = project->AbsolutePath(std::string("scenes/") + path);
					}
				}
				bool ok = false;
				if (sceneDialogIsSave)
					ok = SaveSceneToFile(path);
				else if (hostOpenSceneDocument)
				{
					hostOpenSceneDocument(path);
					ok = true;
				}
				else
					ok = LoadSceneFromFile(path);
				if (ok)
				{
					if (project && project->IsOpen() && sceneDialogIsSave)
					{
						std::string rel = project->RelativePath(path);
						if (!rel.empty())
						{
							project->SetActiveSceneRel(rel);
							project->Save();
						}
					}
					showingSceneDialog = false;
					editorDisabled = false;
					ImGui::CloseCurrentPopup();
				}
				else
					sceneDialogError = sceneDialogIsSave ? "Could not write that file." : "Could not read that file.";
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				showingSceneDialog = false;
				editorDisabled = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void SceneEditor::DetachEditorObjects(std::vector<std::shared_ptr<GameObject>> &out)
	{
		out.clear();
		// Helper icons first - they are per-SceneObject and must come back
		// attached to the same registry entries.
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); i++)
			if ((*i).second != NULL && (*i).second->Helper)
				out.push_back((*i).second->Helper);

		out.push_back(grid);
		out.push_back(Camera);
		out.push_back(CameraPivot);

		for (std::vector<std::shared_ptr<GameObject>>::iterator i = out.begin(); i != out.end(); i++)
			scene->Remove(*i);
	}

	void SceneEditor::AttachEditorObjects(std::vector<std::shared_ptr<GameObject>> &saved)
	{
		for (std::vector<std::shared_ptr<GameObject>>::iterator i = saved.begin(); i != saved.end(); i++)
			scene->Add(*i);
		saved.clear();
	}

	// Gives every adopted GameObject and light its viewport icon, the way the
	// Add form does for objects created through the UI.
	std::string SceneEditor::DisplayPath(const std::string& path) const
	{
		if (!project || !project->IsOpen()) return path;
		return project->DisplayPath(path);
	}

	void SceneEditor::RebuildHelpers()
	{
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); i++)
		{
			SceneObject* obj = (*i).second;
			if (obj == NULL || obj->Helper) continue;

			if (obj->GetType() == SceneObjectTypes::GAMEOBJECT)
			{
				std::shared_ptr<GameObjectHelper> h = std::make_shared<GameObjectHelper>((GameObject*)obj->GetPTR());
				obj->Helper = h;
				scene->Add(h);
			}
			else if (obj->GetType() == SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT ||
					 obj->GetType() == SceneObjectTypes::POINTLIGHT_COMPONENT ||
					 obj->GetType() == SceneObjectTypes::SPOTLIGHT_COMPONENT)
			{
				IComponent* c = (IComponent*)obj->GetPTR();
				if (c == NULL || c->GetOwner() == NULL) continue;
				std::shared_ptr<LightHelper> h = std::make_shared<LightHelper>(c->GetOwner());
				obj->Helper = h;
				scene->Add(h);
			}
			else if (obj->GetType() == SceneObjectTypes::AUDIO_SOURCE_COMPONENT)
			{
				IComponent* c = (IComponent*)obj->GetPTR();
				if (c == NULL || c->GetOwner() == NULL) continue;
				std::shared_ptr<SoundHelper> h = std::make_shared<SoundHelper>(c->GetOwner());
				obj->Helper = h;
				scene->Add(h);
			}
			else if (obj->GetType() == SceneObjectTypes::PARTICLE_SYSTEM_COMPONENT)
			{
				IComponent* c = (IComponent*)obj->GetPTR();
				if (c == NULL || c->GetOwner() == NULL) continue;
				std::shared_ptr<ParticleHelper> h = std::make_shared<ParticleHelper>(c->GetOwner());
				obj->Helper = h;
				scene->Add(h);
			}
		}
		// A scene full of freshly-deserialized emitters arrives playing (the
		// ParticleSystem constructor's default) - outside Play they should
		// all be idle until selected.
		if (!playMode)
			ResetParticlePreview();
	}

	void SceneEditor::NewScene(bool applyProjectDefaults)
	{
		(void)applyProjectDefaults;
		// A script-driven loadScene() keeps playing straight through the swap;
		// only an editor-initiated load ends the session.
		if (playMode && !loadingSceneForPlay)
			StopPlayMode();
		DeselectMesh();
		DeselectSceneObject();
		selection.clear();
		node_clicked = -1;
		// Released here rather than at load time: the outgoing scene's
		// objects are torn down just below, and anything still referenced
		// stays alive on its own shared_ptr.
		sceneAssets = LoadedSceneAssets();

		// Drops every user GameObject/component (and its helper) - the
		// SceneGraph holds the only strong references, so this frees them.
		sceneObjects->DestroyAll();
		sceneCameras.clear();
		activeSceneCameraId = 0;
		scenePath.clear();
#ifdef LUA_BINDINGS
		sceneMainScriptPath.clear();
		sceneMainScript.reset();
#endif
		sceneRootSelected = true;
		sceneDirty = false;

		ambientLightColor = Vec4(0.2f, 0.2f, 0.2f, 0.2f);
		Renderer->SetGlobalLight(ambientLightColor);
	}

	bool SceneEditor::SaveSceneToFile(const std::string &path)
	{
		if (path.size() == 0) return false;

#ifdef LUA_BINDINGS
		// Companion script shares the scene stem (Foo.json → Foo.lua).
		{
			std::string scriptAbs;
			std::string err;
			if (project && project->IsOpen())
			{
				if (project->EnsureSceneCompanionScript(path, scriptAbs, &err))
					sceneMainScriptPath = scriptAbs;
				else
					echo("ERROR: " + err);
			}
			else
				sceneMainScriptPath = ProjectManager::SceneScriptPathForSceneJson(path);
		}
#endif

		std::vector<std::shared_ptr<GameObject>> furniture;
		DetachEditorObjects(furniture);
		bool ok = false;
		try {
			SceneMeta meta;
			meta.ambientLight = ambientLightColor;
			meta.twoD = sceneIsTwoD;
#ifdef LUA_BINDINGS
			PushLuaHostGlobals();
			meta.mainScript = sceneMainScriptPath;
			ok = SceneSerializer::SaveScene(scene, path, sharedLua, &meta);
#else
			ok = SceneSerializer::SaveScene(scene, path, NULL, &meta);
#endif
			// Before AttachEditorObjects() because the collapse pass matches
			// roots by position in GetAllGameObjectList(), so that list must
			// still hold exactly what SaveScene wrote - user content only.
			// Inside the try for a blunter reason: it reads and rewrites a
			// file, and anything it threw would skip the re-attach below and
			// leave the editor's own grid, cameras and helper icons detached
			// from the scene for the rest of the session.
			if (ok) CollapseSceneFileAfterSave(path);
		}
		catch (const std::exception &e) { echo(std::string("ERROR: scene save threw: ") + e.what()); ok = false; }

		AttachEditorObjects(furniture);

		if (ok) scenePath = path;
		else echo("ERROR: failed to save scene to " + path);
		if (ok) SaveEditorSidecar(path);
		if (ok) sceneDirty = false;
#ifdef LUA_BINDINGS
		if (ok) RebuildSceneMainScriptInstance();
#endif
		return ok;
	}

	namespace {
		// The RenderingComponent whose material is *project* data, if any.
		//
		// ParticleSystem is a RenderingComponent too (it draws instanced
		// billboards) and its material is a CustomShaderMaterial - so both
		// sweeps below used to pick it up and try to "recompile" it. Its
		// shader is the engine's own shaders/particleSystem.glsl, resolved
		// against the *editor's* working directory, not the project, so
		// recompiling it against projectRoot always failed to find the file
		// and left every emitter running the Material Editor's magenta error
		// shader instead. That shader draws through uModelMatrix, which
		// ParticleSystemMaterial never sets, so the quads collapsed to a
		// degenerate point and the emitters simply vanished - a renderer
		// switch or a project open was all it took, which is why particles
		// only disappeared "sometimes".
		RenderingComponent* FirstProjectRenderingComponent(GameObject* go)
		{
			if (!go) return NULL;
			for (auto& c : go->GetComponents())
			{
				if (dynamic_cast<ParticleSystem*>(c.get())) continue;
				RenderingComponent* rc = dynamic_cast<RenderingComponent*>(c.get());
				if (rc) return rc;
			}
			return NULL;
		}
	}

	void SceneEditor::RecompileOrphanedCustomMaterials(const std::string& projectRoot, bool deferredGBuffer,
	                                                    const std::set<IMaterial*>& skipMaterials)
	{
		if (!scene) return;
		std::set<IMaterial*> visited;
		for (auto& goPtr : scene->GetAllGameObjectList())
		{
			GameObject* go = goPtr.get();
			if (!go) continue;
			RenderingComponent* rc = FirstProjectRenderingComponent(go);
			if (!rc) continue;
			for (RenderingMesh* mesh : rc->GetMeshes(0))
			{
				auto* cm = dynamic_cast<CustomShaderMaterial*>(mesh->Material.get());
				if (!cm || skipMaterials.count(cm) || visited.count(cm)) continue;
				visited.insert(cm);
				std::string err;
				if (!MaterialEditor::RecompileFromDisk(cm, projectRoot, deferredGBuffer, &err))
					echo("WARNING: could not recompile material for renderer switch: " + err);
			}
		}
	}

	int SceneEditor::RefreshMaterialsFromGeneratedGlsl(const std::string& generatedGlslRel, const std::string& projectRoot,
	                                                    bool deferredGBuffer, const std::set<IMaterial*>& skipMaterials)
	{
		if (!scene || generatedGlslRel.empty()) return 0;
		int refreshed = 0;
		std::set<IMaterial*> visited;
		for (auto& goPtr : scene->GetAllGameObjectList())
		{
			GameObject* go = goPtr.get();
			if (!go) continue;
			RenderingComponent* rc = FirstProjectRenderingComponent(go);
			if (!rc) continue;
			for (RenderingMesh* mesh : rc->GetMeshes(0))
			{
				auto* cm = dynamic_cast<CustomShaderMaterial*>(mesh->Material.get());
				if (!cm || skipMaterials.count(cm) || visited.count(cm)) continue;
				visited.insert(cm);
				// GetShaderFile() may be stored absolute or project-relative
				// depending on which path created the material, so compare on
				// the tail rather than requiring one form.
				const std::string file = cm->GetShaderFile();
				if (file.empty()) continue;
				if (file.size() < generatedGlslRel.size()
					|| file.compare(file.size() - generatedGlslRel.size(), generatedGlslRel.size(), generatedGlslRel) != 0)
					continue;
				std::string err;
				if (MaterialEditor::RecompileFromDisk(cm, projectRoot, deferredGBuffer, &err))
					refreshed++;
				else
					echo("WARNING: could not refresh scene material from " + generatedGlslRel + ": " + err);
			}
		}
		return refreshed;
	}

	bool SceneEditor::LoadSceneFromFile(const std::string &path)
	{
		if (path.size() == 0) return false;

		NewScene(false);

		std::vector<std::shared_ptr<GameObject>> furniture;
		DetachEditorObjects(furniture);

		// outAssets deliberately NULL: the SceneGraph holds the only strong
		// references to what LoadScene builds, so removing the roots (which
		// DestroyAll does) frees everything. Tracking them separately and
		// calling UnloadScene as well would free the same objects twice.
		// Defence in depth alongside the serializer's own shape check: the
		// user can point this at any file on disk, and the loader walks a lot
		// of nested JSON that a structurally-valid-but-wrong document could
		// still trip over. A bad pick should be a dialog error, never a
		// terminate.
		bool ok = false;
		SceneMeta meta;
		try {
		// Prefab references are resolved here, not by the engine - it is
		// handed a scene whose roots are all written out in full. An empty
		// string means the file could not be read at all; the engine's own
		// call below reports that.
		const std::string expanded = ExpandSceneFileForLoad(path);
		std::vector<std::string> rootPrefabPaths;
		if (!expanded.empty())
		{
			try
			{
				const nlohmann::json j = nlohmann::json::parse(expanded);
				if (j.is_object() && j.find("roots") != j.end() && j["roots"].is_array())
					for (size_t i = 0; i < j["roots"].size(); ++i)
						rootPrefabPaths.push_back(prefab::LinkOf(j["roots"][i]));
			}
			catch (const std::exception&) { rootPrefabPaths.clear(); }
		}

#ifdef LUA_BINDINGS
			PushLuaHostGlobals();
			ok = expanded.empty()
				? SceneSerializer::LoadScene(scene, path, physics, sharedLua, &sceneAssets, &meta)
				: SceneSerializer::LoadSceneFromText(scene, expanded, path, physics, sharedLua, &sceneAssets, &meta);
			if (ok)
			{
				RelinkPrefabInstancesAfterLoad(rootPrefabPaths);
				scenePath = path;
				// Prefer companion scenes/<Name>.lua; create if missing.
				std::string companion;
				std::string err;
				if (project && project->IsOpen()
					&& project->EnsureSceneCompanionScript(path, companion, &err))
				{
					sceneMainScriptPath = companion;
				}
				else if (!meta.mainScript.empty())
					sceneMainScriptPath = meta.mainScript;
				else
					sceneMainScriptPath = ProjectManager::SceneScriptPathForSceneJson(path);
				sceneIsTwoD = meta.twoD;
				if (sceneIsTwoD)
				{
					LookAtPlaneXY(0.f, 0.f);
					// See SwitchRenderer(): 2D is forward, always.
					SwitchRenderer(false);
				}
				RebuildSceneMainScriptInstance();
			}
#else
			ok = expanded.empty()
				? SceneSerializer::LoadScene(scene, path, physics, NULL, &sceneAssets, &meta)
				: SceneSerializer::LoadSceneFromText(scene, expanded, path, physics, NULL, &sceneAssets, &meta);
			if (ok) RelinkPrefabInstancesAfterLoad(rootPrefabPaths);
#endif
		}
		catch (const std::exception &e) { echo(std::string("ERROR: scene load threw: ") + e.what()); ok = false; }

		if (ok)
		{
			ambientLightColor = meta.ambientLight;
			Renderer->SetGlobalLight(ambientLightColor);
		}

		if (ok)
		{
			std::vector<std::shared_ptr<GameObject>> roots = scene->GetAllGameObjectList();
			for (std::vector<std::shared_ptr<GameObject>>::iterator i = roots.begin(); i != roots.end(); i++)
				sceneObjects->Adopt((*i).get());
			ApplyCameraTagsFromScene();
			LoadEditorSidecar(path);
			scenePath = path;
			sceneDirty = false;
			lastLoadMtime = SceneEditor::FileMtime(path);
		}
		else echo("ERROR: failed to load scene from " + path);

		// SceneSerializer::BuildMaterial compiles custom materials with the
		// platform #defines only - it never chooses a DEFERRED_GBUFFER branch
		// (see its header comment), so without this every freshly loaded
		// scene's custom materials would be stuck on whichever branch they
		// happened to compile as. Recompile them for the renderer THIS scene
		// is currently using. No MaterialEditorDocuments reference these
		// just-built materials yet, so the skip-set stays empty.
		if (ok && project)
			RecompileOrphanedCustomMaterials(project->GetProjectPath(), usingDeferredRenderer, std::set<IMaterial*>());

		AttachEditorObjects(furniture);
		if (ok) RebuildHelpers();
		// After the helpers, so the walk sees the finished tree: every
		// element naming a style gets it re-applied, which is what makes
		// editing a style file - or swapping the palette - reach scenes that
		// were saved before the change.
		if (ok) ReapplyUIStyles();
		return ok;
	}

	void SceneEditor::SetHostCallbacks(void (*onCloseProject)(), void (*onQuitApp)(),
		void (*onNewProject)(), void (*onOpenProject)(), void (*onQuitDiscardingUnsaved)())
	{
		hostCloseProject = onCloseProject;
		hostQuitApp = onQuitApp;
		hostNewProject = onNewProject;
		hostOpenProject = onOpenProject;
		hostQuitDiscardingUnsaved = onQuitDiscardingUnsaved;
	}

	bool SceneEditor::HasUnsavedWork() const
	{
		if (sceneDirty) return true;
		if (project && project->IsOpen() && project->IsDirty()) return true;
		return false;
	}

	void SceneEditor::OpenSaveSceneDialog()
	{
		showingSceneDialog = true;
		sceneDialogIsSave = true;
		if (scenePath.size() > 0)
			sceneDialogPath = scenePath;
		else if (project && project->IsOpen())
			sceneDialogPath = "Untitled.json";
		else
			sceneDialogPath = "scene.json";
		sceneDialogError.clear();
	}

	bool SceneEditor::TrySaveCurrentScene()
	{
		if (scenePath.size() > 0)
			return SaveSceneToFile(scenePath);
		OpenSaveSceneDialog();
		awaitingSaveDialog = true;
		return false;
	}

	bool SceneEditor::ConfirmUnsavedThen(int action, const std::string& path)
	{
		if (!HasUnsavedWork())
			return true;
		pendingUnsavedAction = action;
		pendingLoadPath = path;
		showUnsavedModal = true;
		return false;
	}

	void SceneEditor::ExecutePendingUnsavedAction()
	{
		const int action = pendingUnsavedAction;
		const std::string path = pendingLoadPath;
		pendingUnsavedAction = UnsavedNone;
		pendingLoadPath.clear();
		awaitingSaveDialog = false;

		switch (action)
		{
		case UnsavedNewScene:
			NewScene();
			if (project && project->IsOpen())
			{
				OpenSaveSceneDialog();
				sceneDialogPath = "NewScene.json";
			}
			break;
		case UnsavedOpenDialog:
			showingSceneDialog = true;
			sceneDialogIsSave = false;
			sceneDialogPath = scenePath;
			sceneDialogError.clear();
			break;
		case UnsavedLoadPath:
			if (!path.empty())
			{
				if (hostOpenSceneDocument)
					hostOpenSceneDocument(path);
				else
				{
					LoadSceneFromFile(path);
					if (project && project->IsOpen())
					{
						const std::string rel = project->RelativePath(path);
						if (!rel.empty())
						{
							project->SetActiveSceneRel(rel);
							project->Save();
						}
					}
				}
			}
			break;
		case UnsavedCloseProject:
			if (hostCloseProject) hostCloseProject();
			break;
		case UnsavedQuitApp:
			if (hostQuitApp) hostQuitApp();
			break;
		case UnsavedOpenProject:
			if (hostOpenProject) hostOpenProject();
			break;
		case UnsavedNewProject:
			if (hostNewProject) hostNewProject();
			break;
		default:
			break;
		}
	}

	void SceneEditor::DrawUnsavedChangesModal()
	{
		// After Save As dialog opened from this prompt: continue once saved,
		// or cancel pending if the dialog was dismissed without saving.
		if (awaitingSaveDialog)
		{
			if (!showingSceneDialog)
			{
				if (!sceneDirty)
					ExecutePendingUnsavedAction();
				else
				{
					pendingUnsavedAction = UnsavedNone;
					pendingLoadPath.clear();
					awaitingSaveDialog = false;
				}
			}
		}

		if (showUnsavedModal)
		{
			ImGui::OpenPopup("Unsaved Changes");
			showUnsavedModal = false;
		}

		ImGuiViewport* vp = ImGui::GetMainViewport();
		if (vp)
			ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowFocus();

		if (ImGui::BeginPopupModal("Unsaved Changes", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Save changes before continuing?");
			if (sceneDirty)
				ImGui::TextDisabled("Scene has unsaved edits.");
			if (project && project->IsOpen() && project->IsDirty())
				ImGui::TextDisabled("Project settings have unsaved edits.");
			ImGui::Spacing();
			if (ImGui::Button("Save", ImVec2(110, 0)))
			{
				bool sceneOk = true;
				if (sceneDirty)
					sceneOk = TrySaveCurrentScene();
				if (project && project->IsOpen())
					project->Save();

				if (sceneOk)
				{
					ImGui::CloseCurrentPopup();
					ExecutePendingUnsavedAction();
				}
				else
					ImGui::CloseCurrentPopup(); // Save As is open; awaitingSaveDialog tracks it
			}
			ImGui::SameLine();
			if (ImGui::Button("Don't Save", ImVec2(110, 0)))
			{
				ImGui::CloseCurrentPopup();
				if (pendingUnsavedAction == UnsavedQuitApp && hostQuitDiscardingUnsaved)
				{
					pendingUnsavedAction = UnsavedNone;
					pendingLoadPath.clear();
					awaitingSaveDialog = false;
					hostQuitDiscardingUnsaved();
				}
				else
				{
					sceneDirty = false;
					if (project) project->ClearDirty();
					ExecutePendingUnsavedAction();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(110, 0)))
			{
				pendingUnsavedAction = UnsavedNone;
				pendingLoadPath.clear();
				awaitingSaveDialog = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	bool SceneEditor::PlaceAssetInScene(const std::string& absolutePath)
	{
		if (absolutePath.empty() || playMode || editorDisabled) return false;

		namespace fs = std::filesystem;
		const std::string name = fs::path(absolutePath).stem().string();
		const bool isModel = ProjectManager::IsP3dm(absolutePath);
		const bool isSound = ProjectManager::IsSoundExtension(absolutePath);
		const bool isTexture = ProjectManager::IsTextureExtension(absolutePath);
		const bool isScene = ProjectManager::IsSceneExtension(absolutePath)
			&& (absolutePath.find("/scenes/") != std::string::npos
				|| absolutePath.find("\\scenes\\") != std::string::npos
				|| (project && !project->RelativePath(absolutePath).empty()
					&& project->RelativePath(absolutePath).find("scenes/") == 0));

		// Prefabs are checked before everything else: instantiating one is
		// the whole point of the asset, so a double-click or a drop into
		// the viewport means "put one here" with no further qualification.
		if (ProjectManager::IsPrefabExtension(absolutePath))
		{
			if (!project || !project->IsOpen())
			{
				echo("ERROR: Open a project before instantiating a prefab");
				return false;
			}
			const std::string rel = project->RelativePath(absolutePath);
			if (rel.empty())
			{
				echo("ERROR: Prefab lives outside the project: " + absolutePath);
				return false;
			}
			std::string err;
			// At the origin rather than under the cursor: PlaceAssetInScene
			// has no viewport hit position to work from (models land the
			// same way), and the object is selected on arrival so it can be
			// dragged straight into place.
			if (!OpInstantiatePrefab(rel, Vec3(0.f, 0.f, 0.f), err))
			{
				echo("ERROR: Instantiate prefab - " + err);
				return false;
			}
			return true;
		}

		if (isScene)
		{
			if (ConfirmUnsavedThen(UnsavedLoadPath, absolutePath))
			{
				LoadSceneFromFile(absolutePath);
				if (project && project->IsOpen())
				{
					const std::string rel = project->RelativePath(absolutePath);
					if (!rel.empty())
					{
						project->SetActiveSceneRel(rel);
						project->Save();
					}
				}
			}
			return true;
		}

		if (!isModel && !isSound)
		{
			if (isTexture)
				echo("Texture: " + absolutePath + " (preview in Assets; assign via materials)");
#ifdef LUA_BINDINGS
			else if (ProjectManager::IsLuaExtension(absolutePath))
			{
				GameObject* parentGo = GetSelectedOwnerGameObject();
				if (!parentGo)
				{
					echo("ERROR: Select a GameObject before attaching a script");
					return false;
				}
				uint32 goId = sceneObjects->GetSceneObjectID(parentGo);
				return AttachLuaScriptToGameObject(goId, absolutePath);
			}
#else
			else if (ProjectManager::IsLuaExtension(absolutePath))
				echo("Lua script: " + absolutePath);
#endif
			else if (ProjectManager::IsShaderExtension(absolutePath))
				echo("Shader: " + absolutePath);
			else if (ProjectManager::IsMaterialExtension(absolutePath))
				echo("Material: " + absolutePath);
			else
				echo("Asset: " + absolutePath);
			return false;
		}

		GameObject* parentGo = NULL;
		if (SelectedSceneObject && SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT)
			parentGo = (GameObject*)SelectedSceneObject->GetPTR();

		// Two different edits share this tail. With nothing selected a new
		// top-level GameObject is created and pushed as an Add; dropping onto
		// an existing selection instead attaches a renderer/audio source to
		// it, which is a Replace of that object's subtree. Both are undoable -
		// the attach case used to push nothing at all, so Ctrl+Z after
		// dropping a model onto a selected object silently undid whatever
		// unrelated edit happened to be on top of the stack.
		const bool createdNewObject = (parentGo == NULL);
		uint32 attachOwnerId = 0;
		std::string attachBeforeSnapshot;
		if (!parentGo)
		{
			CreateGameObject(name.empty() ? (isSound ? "Sound" : "Model") : name);
			if (!SelectedSceneObject) return false;
			parentGo = (GameObject*)SelectedSceneObject->GetPTR();
		}
		else
		{
			// Captured before the component exists, so Undo() restores the
			// object exactly as it was - same pattern as Attach Component.
			attachOwnerId = sceneObjects->GetSceneObjectID(parentGo);
			attachBeforeSnapshot = SnapshotSubtree(attachOwnerId);
		}

		if (isModel)
		{
			sceneObjects->CreateRenderingModel(parentGo, absolutePath);
			MarkSceneDirty();
			if (createdNewObject)
				PushAddCommand(SelectedSceneObject);
			else
				PushReplaceCommand(attachOwnerId, attachBeforeSnapshot,
					"Add Model '" + name + "'");
			echo("Placed model: " + absolutePath);
			return true;
		}

		SceneObject* soundObj = sceneObjects->CreateAudioSource(parentGo, ResolveSoundPath(absolutePath), false, true, false, 1.f);
		if (soundObj)
		{
			std::shared_ptr<SoundHelper> h = std::make_shared<SoundHelper>(parentGo);
			soundObj->Helper = h;
			scene->Add(h);
			MarkSceneDirty();
			if (createdNewObject)
				PushAddCommand(SelectedSceneObject);
			else
				PushReplaceCommand(attachOwnerId, attachBeforeSnapshot,
					"Add Sound '" + name + "'");
			echo("Placed sound: " + absolutePath);
			return true;
		}
		return false;
	}

	void SceneEditor::StopAssetSoundPreview()
	{
		if (assetSoundPreview)
		{
			assetSoundPreview->Stop();
			delete assetSoundPreview;
			assetSoundPreview = NULL;
		}
		assetSoundPreviewPath.clear();
	}

	void SceneEditor::PreviewAssetSound(const std::string& absolutePath)
	{
		if (absolutePath.empty() || !audio) return;

		if (assetSoundPreview && assetSoundPreviewPath == absolutePath
			&& assetSoundPreview->GetPlayingCount() > 0)
		{
			StopAssetSoundPreview();
			return;
		}

		StopAssetSoundPreview();
		assetSoundPreview = new Sound(absolutePath, 1);
		assetSoundPreviewPath = absolutePath;
		if (!assetSoundPreview->IsLoaded())
		{
			echo("ERROR: could not load sound preview: " + absolutePath);
			delete assetSoundPreview;
			assetSoundPreview = NULL;
			assetSoundPreviewPath.clear();
			return;
		}
		assetSoundPreview->Play(1.f, 1.f, 0.f);
	}

	bool SceneEditor::IsAssetSoundPreviewPlaying() const
	{
		return assetSoundPreview != NULL && assetSoundPreview->GetPlayingCount() > 0;
	}

	void SceneEditor::DeselectSceneObject()
	{
		SelectedSceneObject = NULL;
	}

	void SceneEditor::Shutdown()
	{
		if (shutDownDone) return;
		shutDownDone = true;

		// All your Shutdown Code Here
		if (grid && rGrid)
			grid->Remove(rGrid);
		if (scene)
		{
			if (grid) scene->Remove(grid);
			if (Camera) scene->Remove(Camera);
			if (CameraPivot) scene->Remove(CameraPivot);
		}

		rGrid.reset();
		grid.reset();
		gridhandle.reset();
		GridMaterial.reset();
		Camera.reset();
		CameraPivot.reset();
		SelectedRenderingComponent.reset();
		SelectedMeshMaterial.reset();
		tempMaterial.reset();

		// Ordering matters here, and it used to be wrong. ForwardRenderer
		// owns the active IRenderDevice; everything below *borrows* it -
		// PostEffectsManager explicitly so (ResolvePostEffectsDevice() hands
		// it a non-owning pointer when a device is already active), and the
		// first statement of ~PostEffectsManager is device->WaitIdle().
		// Deleting Renderer first therefore turned every clean exit into a
		// call through a freed device, i.e. closing the editor always
		// segfaulted. Destroy the consumers, then the scene and its GPU
		// resources, and let the renderer that owns the device go last.
		delete sceneObjects;
		sceneObjects = NULL;
		delete thumbEffects;
		thumbEffects = NULL;
		delete thumbRenderer;
		thumbRenderer = NULL;
		delete previewEffects;
		previewEffects = NULL;
		delete previewRenderer;
		previewRenderer = NULL;
		delete EffectsManager;
		EffectsManager = NULL;
		delete uiRenderer;
		uiRenderer = NULL;
		delete Picking;
		Picking = NULL;
		delete axisHelper;
		axisHelper = NULL;
		CGizmoTransformRender::SetDebugRenderer(NULL);
		delete editorDebugDraw;
		editorDebugDraw = NULL;
		delete debugRenderer;
		debugRenderer = NULL;
		delete icons;
		icons = NULL;
		delete scene;
		scene = NULL;
		delete physics2D; physics2D = NULL;
		delete physics;
		physics = NULL;
		StopAssetSoundPreview();
		audio = NULL;
		delete Renderer;
		Renderer = NULL;
		// Renderer (if DeferredRenderer) doesn't own gbufferFBO - SceneEditor
		// does, so it's torn down after, matching render_host.lua's ordering
		// (renderer, then fbo, then gbuffer textures).
		if (usingDeferredRenderer)
			DestroyGBuffer();

		InputManager::RemoveEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &SceneEditor::MouseMove);
		InputManager::RemoveEvent(Event::Type::OnMove, Event::Input::Mouse::Wheel, this, &SceneEditor::MouseWheel);
		InputManager::RemoveEvent(Event::Type::OnPress, Event::Input::Mouse::Left, this, &SceneEditor::MouseLeftPress);
		InputManager::RemoveEvent(Event::Type::OnRelease, Event::Input::Mouse::Left, this, &SceneEditor::MouseLeftRelease);
		InputManager::RemoveEvent(Event::Type::OnPress, Event::Input::Mouse::Middle, this, &SceneEditor::MouseMiddlePress);
		InputManager::RemoveEvent(Event::Type::OnRelease, Event::Input::Mouse::Middle, this, &SceneEditor::MouseMiddleRelease);
		InputManager::RemoveEvent(Event::Type::OnPress, Event::Input::Mouse::Right, this, &SceneEditor::MouseRightPress);
		InputManager::RemoveEvent(Event::Type::OnRelease, Event::Input::Mouse::Right, this, &SceneEditor::MouseRightRelease);

		if (gizmo != NULL) delete gizmo;
		gizmo = NULL;

	}

	SceneEditor::~SceneEditor()
	{
		Shutdown();
	}

	void SceneEditor::MouseWheel(Event::Input::Info e)
	{
		UpdateViewportMouse();
		if (viewportMouseValid && !editorDisabled)
		{

			Vec2 tempMouse = mousePosition;
			if (tempMouse.x > 0 && tempMouse.x < Width)
			{
				if (isPerspective)
				{
					// zoomOrtho In and Out
					Vec3 finalPosition;
					Vec3 direction = Vec3(Camera->GetLocalTransformation().m[8], Camera->GetLocalTransformation().m[9], Camera->GetLocalTransformation().m[10]);
					finalPosition -= direction * f32(e.Value);
					Camera->SetPosition(Camera->GetPosition() + finalPosition);
				}
				else zoomOrtho -= 0.1 * f32(e.Value);
			}
		}
	}

	void SceneEditor::MouseLeftPress(Event::Input::Info e)
	{
		(void)e;
		UpdateViewportMouse();
		if (!viewportMouseValid || editorDisabled)
			return;

		switch (axisHelper->MouseClick())
		{
			case AXIS_HELPER_AXIS::CENTER:
				UseCamera0();
				break;
			case AXIS_HELPER_AXIS::NEGATIVE_X:
				UseCamera2(true);
				break;
			case AXIS_HELPER_AXIS::POSITIVE_X:
				UseCamera2();
				break;
			case AXIS_HELPER_AXIS::NEGATIVE_Y:
				UseCamera3(true);
				break;
			case AXIS_HELPER_AXIS::POSITIVE_Y:
				UseCamera3();
				break;
			case AXIS_HELPER_AXIS::NEGATIVE_Z:
				UseCamera1(true);
				break;
			case AXIS_HELPER_AXIS::POSITIVE_Z:
				UseCamera1();
				break;
			case -1:
			default:
				break;
		}
	}

	void SceneEditor::MouseLeftRelease(Event::Input::Info e)
	{
		(void)e;
		_leftMouse = false;
	}

	void SceneEditor::MouseMiddlePress(Event::Input::Info e)
	{
		UpdateViewportMouse();
		if (viewportMouseValid && !editorDisabled)
		{
			Vec2 tempMouse = mousePosition;
			if (!_rightMouse && !_leftMouse && (tempMouse.x > 0 && tempMouse.x < Width))
			{
				_middleMouse = true;
				mouse = mousePosition;
			}
		}
	}

	void SceneEditor::MouseMiddleRelease(Event::Input::Info e)
	{
		if (_middleMouse)
		{
			_middleMouse = false;
			rotX = rotX * qX;
			rotY = rotY * qY;
			rotation = Quaternion();
		}
	}

	void SceneEditor::MouseRightPress(Event::Input::Info e)
	{
		UpdateViewportMouse();
		if (viewportMouseValid && !editorDisabled)
		{
			Vec2 tempMouse = mousePosition;
			if (!_middleMouse && !_leftMouse && (tempMouse.x > 0 && tempMouse.x < Width))
			{
				_mousePanned = false;
				_rightMouse = true;
				mouse = mousePosition;
				pos = (rotY*rotX).ConvertToMatrix().Inverse()*CameraPivot->GetPosition();
			}
		}
	}

	void SceneEditor::MouseRightRelease(Event::Input::Info e)
	{
		(void)e;
		UpdateViewportMouse();
		_rightMouse = false;
	}

	void SceneEditor::MouseMove(Event::Input::Info e)
	{
		(void)e;
		UpdateViewportMouse();
	}

	void SceneEditor::SetObjectProperties(const Vec3 &Translation, const Vec3 &Rotation, const Vec3 &Scale)
	{
		_translation = Translation;
		_rotation = Rotation;
		_scale = Scale;
	}

	void SceneEditor::KeyPressed(Event::Input::Info e)
	{
		(void)e;
	}

	void SceneEditor::KeyReleased(Event::Input::Info e)
	{
		if (e.Input == Event::Input::Keyboard::Numpad0) UseCamera0();
		if (e.Input == Event::Input::Keyboard::Numpad1) UseCamera1();
		if (e.Input == Event::Input::Keyboard::Numpad2) UseCamera2();
		if (e.Input == Event::Input::Keyboard::Numpad3) UseCamera3();
	}

	void SceneEditor::UseCamera0() // Default View
	{
	/*	Camera->SetPosition(Vec3(0, 10, 20));
		Camera->SetRotation(Vec3(-0.464, 0, 0));
		CameraPivot->SetTransformationMatrix(Matrix());
		qX = qY = Quaternion();
		rotX = rotY = Quaternion();*/
		isPerspective = !isPerspective;
	}

	void SceneEditor::UseCamera1(bool invert) // Z Axis
	{
		if (!invert)
		{
			Camera->SetPosition(Vec3(0, 0, 20));
			Camera->SetRotation(Vec3(0, 0, 0));
		}
		else
		{
			Camera->SetPosition(Vec3(0, 0, -20));
			Camera->SetRotation(Vec3(0, 3.14, 0));
		}
		CameraPivot->SetTransformationMatrix(Matrix());
		qX = qY = Quaternion();
		rotX = rotY = Quaternion();
	}

	void SceneEditor::UseCamera2(bool invert) // X Axis
	{
		Camera->SetPosition(Vec3(0, 0, 20));
		Camera->SetRotation(Vec3(0, 0, 0));
		qX = rotX = rotY = Quaternion();

		if (!invert)
			qY = Quaternion(0.700909, 0, 0.71325, 0);
		else
			qY = Quaternion(0.700909, 0, -0.71325, 0);

		rotation = (rotY * qY) * (rotX * qX);
		Matrix m = rotation.ConvertToMatrix();
		CameraPivot->SetTransformationMatrix(m);
		rotX = rotX * qX;
		rotY = rotY * qY;
		rotation = Quaternion();
	}

	void SceneEditor::UseCamera3(bool invert) // Y Axis
	{
		Camera->SetRotation(Vec3(0, 0, 0));
		qY = rotX = rotY = Quaternion();
		if (!invert)
		{
			Camera->SetPosition(Vec3(0, 0, 20));
			qX = Quaternion(0.700909, -0.71325, 0, 0);
		}
		else
		{
			Camera->SetPosition(Vec3(0, 0, 20));
			qX = Quaternion(0.700909, 0.71325, 0, 0);
		}
		rotation = (rotY * qY) * (rotX * qX);
		Matrix m = rotation.ConvertToMatrix();
		CameraPivot->SetTransformationMatrix(m);
		rotX = rotX * qX;
		rotY = rotY * qY;
		rotation = Quaternion();
	}

	void SceneEditor::CreateGameObject(const std::string &name)
	{
		std::string NewName = name;
		if (NewName.size() == 0) NewName = "GameObject";
		
		SceneObject* s = sceneObjects->CreateGameObject(NewName);
		if (s == NULL) {
			echo(std::string("ERROR: Failed to create GameObject '") + NewName + "'");
			return;
		}
		
		SelectSceneObject(s);

		// Re-enabled: what used to crash here was GameObjectHelper's
		// constructor calling LoadTexture("assets/gameobject.dds",
		// ShaderUsage::Diffuse) - an unloadable format passed with the wrong
		// enum, which threw std::length_error out of the texture loader.
		// Same bug that killed the directional light; fixed in
		// GameObjectHelper.cpp. SceneEditor::Update() only draws this icon
		// while the GameObject has no components of its own.
		std::shared_ptr<GameObjectHelper> h = std::make_shared<GameObjectHelper>((GameObject*)SelectedSceneObject->GetPTR());
		s->Helper = h;
		scene->Add(h);

		node_clicked = SelectedSceneObject->GetID();
		MarkSceneDirty();
	}

	// Reads a light's actual shadow setup into the Properties fields, so the
	// panel shows what the scene contains rather than a stale UI value.
	void SceneEditor::SeedShadowProperties(ILightComponent* light)
	{
		if (light == NULL || !light->IsCastingShadows()) return;
		PropertiesShadowBiasFactor = light->GetShadowBiasFactor();
		PropertiesShadowBiasUnits = light->GetShadowBiasUnits();
		PropertiesShadowMapSize = (int32)light->GetShadowWidth();
		PropertiesShadowNear = light->GetShadowNear();
		PropertiesShadowFar = light->GetShadowFar();
	}

	// Shadow acne is a function of bias, map resolution and how much depth
	// range the map has to cover, so all of it belongs in the UI rather than
	// baked into the Cast Shadows checkbox. Bias feeds glPolygonOffset and
	// takes effect on the next shadow pass; everything else changes the map
	// itself, so those return true and the caller re-runs EnableCastShadows()
	// (whose signature differs per light type).
	bool SceneEditor::ShowShadowProperties(ILightComponent* light, bool directional)
	{
		if (light == NULL || !light->IsCastingShadows()) return false;

		bool rebuild = false;

		f32 bias[2] = { PropertiesShadowBiasFactor, PropertiesShadowBiasUnits };
		if (ImGui::DragFloat2("Shadow Bias", bias, 0.05f, -32.f, 32.f, "%.2f"))
		{
			PropertiesShadowBiasFactor = bias[0];
			PropertiesShadowBiasUnits = bias[1];
			light->SetShadowBias(PropertiesShadowBiasFactor, PropertiesShadowBiasUnits);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("glPolygonOffset factor / units for the shadow map pass.\nRaise to remove acne; too much detaches contact shadows.");

		static const int32 sizes[] = { 512, 1024, 2048, 4096 };
		int32 sizeIndex = 2;
		for (int32 i = 0; i < 4; i++)
			if (sizes[i] == PropertiesShadowMapSize) sizeIndex = i;
		if (ImGui::Combo("Map Size", &sizeIndex, "512\0" "1024\0" "2048\0" "4096\0"))
		{
			PropertiesShadowMapSize = sizes[sizeIndex];
			rebuild = true;
		}

		if (directional)
		{
			f32 range[2] = { PropertiesShadowNear, PropertiesShadowFar };
			if (ImGui::DragFloat2("Range", range, 0.05f, 0.001f, 10000.f, "%.2f"))
			{
				PropertiesShadowNear = range[0];
				PropertiesShadowFar = range[1];
				rebuild = true;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Near / far depth range the shadow map covers.\nA range far larger than the scene wastes precision and causes acne.");

			if (ImGui::SliderInt("Cascades", &PropertiesShadowCascades, 1, 4))
				rebuild = true;
		}
		else
		{
			if (ImGui::DragFloat("Near", &PropertiesShadowNear, 0.01f, 0.001f, 1000.f, "%.3f"))
				rebuild = true;
		}

		// Never rebuild just because ImGui clamped a display value — only when
		// the authored settings actually differ from the live light.
		if (rebuild)
		{
			const bool sizeChanged = PropertiesShadowMapSize != (int32)light->GetShadowWidth();
			const bool nearChanged = PropertiesShadowNear != light->GetShadowNear();
			bool farOrCascadesChanged = false;
			if (directional)
			{
				DirectionalLight* d = (DirectionalLight*)light;
				farOrCascadesChanged =
					PropertiesShadowFar != light->GetShadowFar()
					|| PropertiesShadowCascades != (int32)d->GetNumberCascades();
			}
			else
				farOrCascadesChanged = false;
			if (!sizeChanged && !nearChanged && !farOrCascadesChanged)
				rebuild = false;
		}

		return rebuild;
	}

	void SceneEditor::ShowProperties()
	{
		if (sceneRootSelected)
		{
			DrawSceneSettingsInProperties();
			return;
		}
		if (SelectedSceneObject != NULL)
		{
			ImGui::Spacing();
			ImGui::Indent(5.f);

			switch (SelectedSceneObject->GetType())
			{
				case SceneObjectTypes::GAMEOBJECT:
				{
                    // Baseline snapshot taken BEFORE the widget call so it is
                    // correct even if this turns out to be the activation
                    // frame (InputText could in principle mutate the buffer
                    // the same frame focus is gained, e.g. via paste).
                    const std::string preEditName = SelectedSceneObject->Name;
                    if (ImGui::InputText("Name", &SelectedSceneObject->Name))
					{
						sceneObjects->SetName(SelectedSceneObject->GetID(), SelectedSceneObject->Name);
						MarkSceneDirty();
					}
					else
						sceneObjects->SetName(SelectedSceneObject->GetID(), SelectedSceneObject->Name);
					if (ImGui::IsItemActivated())
						undoBaselineName = preEditName;
					if (ImGui::IsItemDeactivatedAfterEdit())
					{
						const std::string finalName = SelectedSceneObject->Name;
						if (finalName != undoBaselineName)
							sceneUndo.Push(std::make_unique<RenameGameObjectCommand>(this,
								SelectedSceneObject->GetID(), undoBaselineName, finalName));
					}
					if (ImGui::DragFloat3("Position", (float *)&_translation, 0.1f, 0.0f, 0.0f))
						MarkSceneDirty();
					UndoValueEdit<Vec3>(undoBaselinePos, _translation, [this](const Vec3& before, const Vec3& after) {
						sceneUndo.Push(std::make_unique<SetTransformCommand>(this, SelectedSceneObject->GetID(),
							before, _rotation, _scale, after, _rotation, _scale, SelectedSceneObject->GetName()));
					});
					if (ImGui::DragFloat3("Rotation", (float *)&_rotation, 0.1f, 0.0f, 0.0f))
						MarkSceneDirty();
					UndoValueEdit<Vec3>(undoBaselineRot, _rotation, [this](const Vec3& before, const Vec3& after) {
						sceneUndo.Push(std::make_unique<SetTransformCommand>(this, SelectedSceneObject->GetID(),
							_translation, before, _scale, _translation, after, _scale, SelectedSceneObject->GetName()));
					});
					if (IsSceneCamera(SelectedSceneObject->GetID()))
					{
						if (ImGui::DragFloat3("Scale", (float *)&_scale, 0.1f, 0.001f, 0.0f))
							MarkSceneDirty();
					}
					else if (ImGui::DragFloat3("Scale", (float *)&_scale, 0.1f, 0.0f, 0.0f))
						MarkSceneDirty();
					UndoValueEdit<Vec3>(undoBaselineScale, _scale, [this](const Vec3& before, const Vec3& after) {
						sceneUndo.Push(std::make_unique<SetTransformCommand>(this, SelectedSceneObject->GetID(),
							_translation, _rotation, before, _translation, _rotation, after, SelectedSceneObject->GetName()));
					});
					if (IsSceneCamera(SelectedSceneObject->GetID()))
					{
						EditorCameraSettings& cam = sceneCameras[SelectedSceneObject->GetID()];
						const uint32 camGoId = SelectedSceneObject->GetID();
						ImGui::Separator();
						ImGui::Text("Camera");
						// Projection first: it decides which of the two size
						// controls below is even meaningful, so showing both
						// at once would just invite editing the dead one.
						{
							int projIndex = cam.orthographic ? 1 : 0;
							const char* projNames[] = { "Perspective", "Orthographic" };
							if (ImGui::Combo("Projection", &projIndex, projNames, 2))
							{
								const bool wasOrtho = cam.orthographic;
								const bool nowOrtho = (projIndex == 1);
								if (wasOrtho != nowOrtho)
								{
									ApplyCameraOrthographic(camGoId, nowOrtho);
									sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
										[this, camGoId, wasOrtho]() { ApplyCameraOrthographic(camGoId, wasOrtho); },
										[this, camGoId, nowOrtho]() { ApplyCameraOrthographic(camGoId, nowOrtho); },
										"Set Camera Projection"));
								}
							}
						}
						if (cam.orthographic)
						{
							ImGui::DragFloat("Size", &cam.orthoSize, 0.1f, 0.01f, 100000.f);
							if (ImGui::IsItemHovered())
								ImGui::SetTooltip("Half the height of the view, in world units.");
							UndoValueEdit<f32>(undoBaselineOrthoSize, cam.orthoSize, [this, camGoId](const f32& before, const f32& after) {
								sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
									[this, camGoId, before]() { ApplyCameraOrthoSize(camGoId, before); },
									[this, camGoId, after]() { ApplyCameraOrthoSize(camGoId, after); }, "Set Camera Size"));
							});
						}
						else
						{
							ImGui::DragFloat("FOV", &cam.fov, 0.5f, 10.f, 170.f);
							UndoValueEdit<f32>(undoBaselineFov, cam.fov, [this, camGoId](const f32& before, const f32& after) {
								sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
									[this, camGoId, before]() { ApplyCameraFov(camGoId, before); },
									[this, camGoId, after]() { ApplyCameraFov(camGoId, after); }, "Set Camera FOV"));
							});
						}
						ImGui::DragFloat("Near", &cam.nearPlane, 0.01f, 0.001f, 100.f);
						UndoValueEdit<f32>(undoBaselineNear, cam.nearPlane, [this, camGoId](const f32& before, const f32& after) {
							sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
								[this, camGoId, before]() { ApplyCameraNear(camGoId, before); },
								[this, camGoId, after]() { ApplyCameraNear(camGoId, after); }, "Set Camera Near"));
						});
						ImGui::DragFloat("Far", &cam.farPlane, 1.f, 1.f, 100000.f);
						UndoValueEdit<f32>(undoBaselineFar, cam.farPlane, [this, camGoId](const f32& before, const f32& after) {
							sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
								[this, camGoId, before]() { ApplyCameraFar(camGoId, before); },
								[this, camGoId, after]() { ApplyCameraFar(camGoId, after); }, "Set Camera Far"));
						});
						if (ImGui::Button("Set Active Camera"))
							SetActiveSceneCamera(SelectedSceneObject->GetID());
						if (activeSceneCameraId == SelectedSceneObject->GetID() && ImGui::Button("Use Editor Camera"))
							ClearActiveSceneCamera();
						// Preview is rendered in ShowViewport (before the main pass)
						// so it does not leave shared IRenderer state dirty.
						Texture* preview = (previewEffects != NULL) ? previewEffects->GetViewportColor() : NULL;
						if (preview != NULL)
						{
							void* previewTex = GetActiveRenderDevice().GetImGuiTextureID(preview->GetBindID(), preview->GetTextureType());
							if (previewTex != NULL)
							{
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
								const ImVec2 uv0(0, 0), uv1(1, 1);
#else
								const ImVec2 uv0(0, 1), uv1(1, 0);
#endif
								ImGui::Image((ImTextureID)previewTex, ImVec2(320, 180), uv0, uv1);
							}
						}
					}
					// Screen-space UI, inspected on the GameObject that
					// carries it rather than as separate hierarchy rows: an
					// element is a rect plus what fills it, and splitting
					// those across three nodes to edit one button is busywork.
					DrawUIComponentProperties((GameObject*)SelectedSceneObject->GetPTR(), SelectedSceneObject->GetID());
#ifdef LUA_BINDINGS
					DrawGameObjectScriptProperties(SelectedSceneObject->GetID());
#endif
				}
				break;
				case SceneObjectTypes::RENDERING_COMPONENT:
				{
					RenderingComponent* r = (RenderingComponent*)SelectedSceneObject->GetPTR();
					std::vector<RenderingMesh*>& meshes = r->GetMeshes();
					// SelectedSceneObject here is the *component's* own tree
					// entry (each component gets its own SceneObject/hierarchy
					// node - see the RENDERING_COMPONENT case in the tree-draw
					// switch above), so GetName() returns a generic component
					// label ("Mesh"), not the owning GameObject's actual name.
					// AgentAssignMaterial/AgentFindGameObjectByName resolve by
					// GameObject name, so passing the component's own name
					// here always failed with "object 'Mesh' not found" -
					// resolve the real owner via ParentID once and use that
					// for every assign call below instead.
					SceneObject* ownerGO = sceneObjects->GetSceneObject(SelectedSceneObject->GetParentID());
					const std::string ownerName = ownerGO ? ownerGO->GetName() : SelectedSceneObject->Name;
					if (meshes.empty())
						ImGui::TextDisabled("(no submeshes)");
					for (size_t m = 0; m < meshes.size(); ++m)
					{
						ImGui::PushID((int)m);

						// One collapsible section per submesh. These blocks
						// are long (Edit Material, the shared Material
						// Settings tree, the assign combo, New Material) and
						// used to run together as one flat list, so with more
						// than one submesh there was nothing to tell you which
						// submesh the controls under your cursor belonged to.
						// Only the first is open by default; the header names
						// the material so the collapsed ones stay readable.
						const char* headMatName = "(no material)";
						std::string headMatShaderLabel;
						if (meshes[m]->Material)
						{
							if (auto* headCm = dynamic_cast<CustomShaderMaterial*>(meshes[m]->Material.get()))
							{
								headMatShaderLabel = headCm->GetShaderFile().empty()
									? std::string("Custom Shader") : DisplayPath(headCm->GetShaderFile());
								headMatName = headMatShaderLabel.c_str();
							}
							else
								headMatName = "Generic Shader";
						}
						char submeshHeader[320];
						snprintf(submeshHeader, sizeof(submeshHeader), "Submesh %zu - %s###submesh_%zu", m, headMatName, m);
						if (m == 0)
							ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
						if (!ImGui::TreeNodeEx(submeshHeader, ImGuiTreeNodeFlags_Framed))
						{
							ImGui::PopID();
							continue;
						}
						if (meshes[m]->Material)
						{
							if (ImGui::SmallButton("Edit Material") && hostEditMaterialInline)
								hostEditMaterialInline(meshes[m]->Material,
									ownerName + " / Submesh " + std::to_string(m));

							// Same common fields as the Material Editor's own
							// Inspector tab (DrawCommonMaterialSettings) -
							// editable right here too so a quick tweak
							// doesn't require leaving the Properties panel.
							// This IS the live material instance (matPtr),
							// not a copy - a material can be assigned to
							// multiple submeshes/objects, so a change here
							// is visible everywhere else it's used too,
							// same as editing it from the Material Editor
							// would be. matPtr is captured by value (not
							// just a raw IMaterial*) in every undo/redo
							// closure below so Undo still mutates the right
							// object even if this submesh gets a different
							// material assigned later.
							if (ImGui::TreeNodeEx("Material Settings", ImGuiTreeNodeFlags_None))
							{
								std::shared_ptr<IMaterial> matPtr = meshes[m]->Material;
								IMaterial* mat = matPtr.get();
								ImGui::TextDisabled("Shared material - affects everything using it");

								// 2D lighting. A button rather than a checkbox
								// because ShaderUsage is fixed when a
								// GenericShaderMaterial is constructed - turning
								// this on rebuilds the material rather than
								// flipping a bit on it (OpMakeSprite2DLit), and
								// a checkbox would imply otherwise.
								{
									GenericShaderMaterial* gsm = dynamic_cast<GenericShaderMaterial*>(mat);
									const bool is2D = gsm && (gsm->GetOptions() & ShaderUsage::Lighting2D);
									if (is2D)
									{
										ImGui::TextDisabled("2D lighting: on (distance falloff, no N.L)");
									}
									else if (gsm && ImGui::SmallButton("Use 2D Lighting"))
									{
										std::string lerr;
										if (!ownerGO || !OpMakeSprite2DLit(ownerGO->GetID(), lerr))
											echo("WARNING: could not switch to 2D lighting: " + lerr);
									}
									if (!is2D && gsm && ImGui::IsItemHovered())
										ImGui::SetTooltip("Lights this by distance alone, with no N.L term.\nWhat a flat sprite wants: a light lying in the sprite's\nown plane is at grazing incidence and leaves it unlit\notherwise. Rebuilds the material.");
								}

								#define MAT_TOGGLE(before, EnableCall, DisableCall) \
									sceneUndo.Push(std::make_unique<ApplyClosureCommand>( \
										[matPtr, before]() { if (before) matPtr->EnableCall; else matPtr->DisableCall; }, \
										[matPtr, before]() { if (!before) matPtr->EnableCall; else matPtr->DisableCall; }, \
										"Toggle " #EnableCall))

								float opacity = mat->GetOpacity();
								if (ImGui::SliderFloat("Opacity", &opacity, 0.f, 1.f))
									mat->SetOpacity(opacity);
								{
									static f32 undoBaselineOpacity;
									UndoValueEdit<f32>(undoBaselineOpacity, opacity, [this, matPtr](const f32& before, const f32& after) {
										sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
											[matPtr, before]() { matPtr->SetOpacity(before); },
											[matPtr, after]() { matPtr->SetOpacity(after); },
											"Set Material Opacity"));
									});
								}

								bool transparent = mat->IsTransparent();
								if (ImGui::Checkbox("Transparent", &transparent))
								{
									const bool before = !transparent;
									mat->SetTransparencyFlag(transparent);
									MAT_TOGGLE(before, SetTransparencyFlag(true), SetTransparencyFlag(false));
								}

								bool blending = mat->IsBlendingEnabled();
								if (ImGui::Checkbox("Blending", &blending))
								{
									const bool before = !blending;
									if (blending) mat->EnableBlending(); else mat->DisableBlending();
									MAT_TOGGLE(before, EnableBlending(), DisableBlending());
								}

								bool depthTest = mat->IsDepthTesting();
								if (ImGui::Checkbox("Depth Test", &depthTest))
								{
									const bool before = !depthTest;
									if (depthTest) mat->EnableDepthTest(); else mat->DisableDepthTest();
									MAT_TOGGLE(before, EnableDepthTest(), DisableDepthTest());
								}

								bool depthWrite = mat->IsDepthWritting();
								if (ImGui::Checkbox("Depth Write", &depthWrite))
								{
									const bool before = !depthWrite;
									if (depthWrite) mat->EnableDepthWrite(); else mat->DisableDepthWrite();
									MAT_TOGGLE(before, EnableDepthWrite(), DisableDepthWrite());
								}

								uint32 cullFace = mat->GetCullFace();
								static const char* cullLabels[] = { "None", "Front", "Back" };
								int cullIdx = (int)cullFace;
								if (ImGui::Combo("Cull Face", &cullIdx, cullLabels, IM_ARRAYSIZE(cullLabels)))
								{
									const uint32 before = cullFace;
									const uint32 after = (uint32)cullIdx;
									mat->SetCullFace(after);
									sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
										[matPtr, before]() { matPtr->SetCullFace(before); },
										[matPtr, after]() { matPtr->SetCullFace(after); },
										"Set Material Cull Face"));
								}

								bool wireframe = mat->IsWireFrame();
								if (ImGui::Checkbox("Wireframe", &wireframe))
								{
									const bool before = !wireframe;
									if (wireframe) mat->StartRenderWireFrame(); else mat->StopRenderWireFrame();
									MAT_TOGGLE(before, StartRenderWireFrame(), StopRenderWireFrame());
								}

								bool castShadows = mat->IsCastingShadows();
								if (ImGui::Checkbox("Cast Shadows", &castShadows))
								{
									const bool before = !castShadows;
									if (castShadows) mat->EnableCastingShadows(); else mat->DisableCastingShadows();
									MAT_TOGGLE(before, EnableCastingShadows(), DisableCastingShadows());
								}

								#undef MAT_TOGGLE
								ImGui::TreePop();
							}
						}
						else
							ImGui::TextDisabled("(no material)");

						// Attach a different material asset to this submesh
						// (drop a tile from the Assets panel, or pick from the combo).
						ImGui::SetNextItemWidth(-1.f);
						if (ImGui::BeginCombo("##assign_material", "Assign material asset..."))
						{
							if (project && project->IsOpen())
							{
								std::vector<ProjectAssetEntry> mats;
								project->ListAssets("assets/materials", mats, true);
								bool any = false;
								for (size_t mi = 0; mi < mats.size(); ++mi)
								{
									const ProjectAssetEntry& e = mats[mi];
									if (e.isDirectory || !ProjectManager::IsMaterialExtension(e.relativePath))
										continue;
									any = true;
									if (ImGui::Selectable(e.relativePath.c_str()) && hostAssignMaterialAsset)
										propertiesMaterialAssignError = hostAssignMaterialAsset(ownerName, (int)m, e.relativePath);
								}
								if (!any)
									ImGui::TextDisabled("No materials in assets/materials");
							}
							else
								ImGui::TextDisabled("Open a project first");
							ImGui::EndCombo();
						}
						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_REL"))
							{
								const char* rel = (const char*)payload->Data;
								if (rel && ProjectManager::IsMaterialExtension(rel) && hostAssignMaterialAsset)
									propertiesMaterialAssignError = hostAssignMaterialAsset(ownerName, (int)m, rel);
							}
							ImGui::EndDragDropTarget();
						}

						// No existing material asset fits -> make one from
						// scratch and attach it in one step, rather than
						// requiring a detour through the Assets panel's own
						// New Material flow followed by coming back here to
						// assign it.
						if (ImGui::SmallButton("New Material...") && project)
							ImGui::OpenPopup("NewSubmeshMaterial");
						if (ImGui::BeginPopup("NewSubmeshMaterial"))
						{
							static char newSubmeshMatName[128] = "";
							static int newSubmeshMatKindCombo = 1; // default Custom Shader
							static int newSubmeshMatModeCombo = 0; // default Node Graph - only asked when kind == Custom
							static std::string newSubmeshMatError;
							if (ImGui::IsWindowAppearing())
							{
								snprintf(newSubmeshMatName, sizeof(newSubmeshMatName), "%s_Material", ownerName.c_str());
								newSubmeshMatError.clear();
							}
							ImGui::SetNextItemWidth(220.f);
							ImGui::InputText("Name", newSubmeshMatName, sizeof(newSubmeshMatName));
							static const char* kindLabels[] = { "Generic Shader", "Custom Shader" };
							ImGui::SetNextItemWidth(220.f);
							ImGui::Combo("Type", &newSubmeshMatKindCombo, kindLabels, IM_ARRAYSIZE(kindLabels));
							if (newSubmeshMatKindCombo == 1)
							{
								// Text and Node Graph are two separate,
								// INCOMPATIBLE representations of a Custom
								// material (see MaterialCodegen.h) - asked
								// up front rather than defaulted, since
								// switching later throws away whichever
								// one you're leaving.
								static const char* modeLabels[] = { "Node Graph", "Text (GLSL)" };
								ImGui::SetNextItemWidth(220.f);
								ImGui::Combo("Editing Mode", &newSubmeshMatModeCombo, modeLabels, IM_ARRAYSIZE(modeLabels));
							}
							if (!newSubmeshMatError.empty())
								ImGui::TextColored(ImVec4(1.f, 0.45f, 0.4f, 1.f), "%s", newSubmeshMatError.c_str());
							if (ImGui::Button("Create && Assign"))
							{
								std::string abs, err;
								MaterialAssetKind kind = (newSubmeshMatKindCombo == 1) ? MaterialAssetKind::Custom : MaterialAssetKind::Generic;
								const bool useTextMode = (newSubmeshMatKindCombo == 1) && (newSubmeshMatModeCombo == 1);
								if (project->CreateMaterial(newSubmeshMatName, kind, abs, &err, useTextMode))
								{
									const std::string rel = project->RelativePath(abs);
									propertiesMaterialAssignError = hostAssignMaterialAsset ? hostAssignMaterialAsset(ownerName, (int)m, rel) : std::string();
									if (propertiesMaterialAssignError.empty())
									{
										// Assigning alone stays quiet (no tab
										// pops open - see
										// Editor::LoadMaterialQuietly), but a
										// material JUST CREATED here is one
										// the user obviously wants to start
										// editing right away, so open it same
										// as clicking "Edit Material" would.
										if (hostEditMaterialInline && meshes[m]->Material)
											hostEditMaterialInline(meshes[m]->Material, ownerName + " / Submesh " + std::to_string(m));
										ImGui::CloseCurrentPopup();
									}
								}
								else
								{
									newSubmeshMatError = err;
								}
							}
							ImGui::SameLine();
							if (ImGui::Button("Cancel"))
								ImGui::CloseCurrentPopup();
							ImGui::EndPopup();
						}

						if (!propertiesMaterialAssignError.empty())
							ImGui::TextColored(ImVec4(1.f, 0.4f, 0.35f, 1.f), "%s", propertiesMaterialAssignError.c_str());

						ImGui::TreePop();
						ImGui::PopID();
					}
				}
				break;
				case SceneObjectTypes::LUA_COMPONENT:
				{
#ifdef LUA_BINDINGS
					LuaComponent* lc = (LuaComponent*)SelectedSceneObject->GetPTR();
					ImGui::TextUnformatted("Lua Script");
					if (playMode)
					{
					if (lc && !lc->scriptFile.empty())
						ImGui::TextWrapped("%s", DisplayPath(lc->scriptFile).c_str());
					else
						ImGui::TextDisabled("(no script file)");
					if (lc && !lc->scriptFile.empty() && hostOpenLuaScript
						&& ImGui::Button("Open Script", ImVec2(140, 0)))
						hostOpenLuaScript(ResolveScriptPath(lc->scriptFile));
				}
				else
				{
					if (lc && !lc->scriptFile.empty())
						ImGui::TextWrapped("%s", DisplayPath(lc->scriptFile).c_str());
					if (lc && !lc->scriptFile.empty() && hostOpenLuaScript)
					{
						if (ImGui::Button("Open Script", ImVec2(140, 0)))
							hostOpenLuaScript(ResolveScriptPath(lc->scriptFile));
					}
					if (propertiesScriptAttachPath.empty() && lc && !lc->scriptFile.empty())
					{
						if (project && project->IsOpen())
							propertiesScriptAttachPath = project->RelativePath(lc->scriptFile);
						if (propertiesScriptAttachPath.empty())
							propertiesScriptAttachPath = lc->scriptFile;
					}
					DrawScriptAssetPicker("##luascript", propertiesScriptAttachPath);
						if (ImGui::Button("Apply"))
						{
							GameObject* owner = lc ? lc->GetOwner() : NULL;
							const uint32 ownerId = owner ? sceneObjects->GetSceneObjectID(owner) : 0;
							const uint32 selfId = SelectedSceneObject->GetID();
							const std::string abs = ResolveScriptPath(propertiesScriptAttachPath);
							if (ownerId != 0)
							{
								sceneObjects->DestroySceneObject(selfId);
								DeselectSceneObject();
								if (AttachLuaScriptToGameObject(ownerId, abs))
									propertiesScriptAttachPath.clear();
								else
								{
									SceneObject* ownerObj = sceneObjects->GetSceneObject(ownerId);
									if (ownerObj) SelectSceneObject(ownerObj);
								}
							}
						}
						ImGui::SameLine();
						if (ImGui::Button("Detach"))
						{
							GameObject* owner = lc ? lc->GetOwner() : NULL;
							const uint32 ownerId = owner ? sceneObjects->GetSceneObjectID(owner) : 0;
							const uint32 selfId = SelectedSceneObject->GetID();
							DeleteComponentById(selfId);
							propertiesScriptAttachPath.clear();
							if (ownerId != 0)
							{
								SceneObject* ownerObj = sceneObjects->GetSceneObject(ownerId);
								if (ownerObj) SelectSceneObject(ownerObj);
							}
						}
					}
#endif
				}
				break;
				case SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT:
				{
					DirectionalLight* l = (DirectionalLight*)SelectedSceneObject->GetPTR();
					const uint32 lightId = SelectedSceneObject->GetID();
					if (ImGui::ColorEdit4("Color", (float*)&PropertiesLightColor))
					{
						l->SetLightColor(PropertiesLightColor);
						MarkSceneDirty();
					}
					UndoValueEdit<Vec4>(undoBaselineLightColor, PropertiesLightColor, [this, lightId](const Vec4& before, const Vec4& after) {
						sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
							[this, lightId, before]() { ApplyLightColor(lightId, before); },
							[this, lightId, after]() { ApplyLightColor(lightId, after); }, "Set Light Color"));
					});
					if (ImGui::DragFloat3("Direction", (float *)&PropertiesLightDirection, 0.01f, -1.0f, 1.0f))
					{
						l->SetLightDirection(PropertiesLightDirection);
						MarkSceneDirty();
					}
					UndoValueEdit<Vec3>(undoBaselineLightDirection, PropertiesLightDirection, [this, lightId](const Vec3& before, const Vec3& after) {
						sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
							[this, lightId, before]() { ApplyLightDirection(lightId, before); },
							[this, lightId, after]() { ApplyLightDirection(lightId, after); }, "Set Light Direction"));
					});
					if (ImGui::Checkbox("Cast Shadows", &AddForm_cs))
					{
						if (AddForm_cs)
						{
							l->EnableCastShadows(PropertiesShadowMapSize, PropertiesShadowMapSize, projection,
								PropertiesShadowNear, PropertiesShadowFar, PropertiesShadowCascades);
							l->SetShadowBias(PropertiesShadowBiasFactor, PropertiesShadowBiasUnits);
						}
						else
							l->DisableCastShadows();
						MarkSceneDirty();
					}
					if (ShowShadowProperties(l, true))
					{
						// Only rebuild when the user actually changed map size/range/cascades.
						l->EnableCastShadows(PropertiesShadowMapSize, PropertiesShadowMapSize, projection,
							PropertiesShadowNear, PropertiesShadowFar, PropertiesShadowCascades);
						l->SetShadowBias(PropertiesShadowBiasFactor, PropertiesShadowBiasUnits);
						MarkSceneDirty();
					}
				}
				break;
				case SceneObjectTypes::POINTLIGHT_COMPONENT:
				{
					PointLight* l = (PointLight*)SelectedSceneObject->GetPTR();
					const uint32 lightId = SelectedSceneObject->GetID();
					if (ImGui::ColorEdit4("Color", (float*)&PropertiesLightColor))
					{
						l->SetLightColor(PropertiesLightColor);
						MarkSceneDirty();
					}
					UndoValueEdit<Vec4>(undoBaselineLightColor, PropertiesLightColor, [this, lightId](const Vec4& before, const Vec4& after) {
						sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
							[this, lightId, before]() { ApplyLightColor(lightId, before); },
							[this, lightId, after]() { ApplyLightColor(lightId, after); }, "Set Light Color"));
					});
					if (ImGui::DragFloat("Radius", (float *)&PropertiesLightRadius, 0.01f, 0.001f, 0.0f))
					{
						l->SetLightRadius(PropertiesLightRadius);
						MarkSceneDirty();
					}
					UndoValueEdit<f32>(undoBaselineLightRadius, PropertiesLightRadius, [this, lightId](const f32& before, const f32& after) {
						sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
							[this, lightId, before]() { ApplyLightRadius(lightId, before); },
							[this, lightId, after]() { ApplyLightRadius(lightId, after); }, "Set Light Radius"));
					});
					if (ImGui::Checkbox("Cast Shadows", &AddForm_cs))
					{
						if (AddForm_cs)
						{
							l->EnableCastShadows(PropertiesShadowMapSize, PropertiesShadowMapSize, PropertiesShadowNear);
							l->SetShadowBias(PropertiesShadowBiasFactor, PropertiesShadowBiasUnits);
						}
						else
							l->DisableCastShadows();
						MarkSceneDirty();
					}
					if (ShowShadowProperties(l, false))
					{
						l->EnableCastShadows(PropertiesShadowMapSize, PropertiesShadowMapSize, PropertiesShadowNear);
						l->SetShadowBias(PropertiesShadowBiasFactor, PropertiesShadowBiasUnits);
						MarkSceneDirty();
					}
				}
				break;
				case SceneObjectTypes::SPOTLIGHT_COMPONENT:
				{
					SpotLight* l = (SpotLight*)SelectedSceneObject->GetPTR();
					const uint32 lightId = SelectedSceneObject->GetID();
					if (ImGui::ColorEdit4("Color", (float*)&PropertiesLightColor))
					{
						l->SetLightColor(PropertiesLightColor);
						MarkSceneDirty();
					}
					UndoValueEdit<Vec4>(undoBaselineLightColor, PropertiesLightColor, [this, lightId](const Vec4& before, const Vec4& after) {
						sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
							[this, lightId, before]() { ApplyLightColor(lightId, before); },
							[this, lightId, after]() { ApplyLightColor(lightId, after); }, "Set Light Color"));
					});
					if (ImGui::DragFloat("Radius", (float *)&PropertiesLightRadius, 0.01f, 0.001f, 0.0f))
					{
						l->SetLightRadius(PropertiesLightRadius);
						MarkSceneDirty();
					}
					UndoValueEdit<f32>(undoBaselineLightRadius, PropertiesLightRadius, [this, lightId](const f32& before, const f32& after) {
						sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
							[this, lightId, before]() { ApplyLightRadius(lightId, before); },
							[this, lightId, after]() { ApplyLightRadius(lightId, after); }, "Set Light Radius"));
					});
					if (ImGui::DragFloat3("Direction", (float *)&PropertiesLightDirection, 0.01f, -1.0f, 1.0f))
					{
						l->SetLightDirection(PropertiesLightDirection);
						MarkSceneDirty();
					}
					UndoValueEdit<Vec3>(undoBaselineLightDirection, PropertiesLightDirection, [this, lightId](const Vec3& before, const Vec3& after) {
						sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
							[this, lightId, before]() { ApplyLightDirection(lightId, before); },
							[this, lightId, after]() { ApplyLightDirection(lightId, after); }, "Set Light Direction"));
					});
					if (ImGui::DragFloat("Outter Cone", (float *)&PropertiesLightOutterCone, 0.01f, 0.002f, 0.0f))
					{
						l->SetLightOutterCone(PropertiesLightOutterCone);
						MarkSceneDirty();
					}
					UndoValueEdit<f32>(undoBaselineLightOuterCone, PropertiesLightOutterCone, [this, lightId](const f32& before, const f32& after) {
						sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
							[this, lightId, before]() { ApplyLightOuterCone(lightId, before); },
							[this, lightId, after]() { ApplyLightOuterCone(lightId, after); }, "Set Light Outer Cone"));
					});
					if (ImGui::DragFloat("Inner Cone", (float *)&PropertiesLightInnerCone, 0.01f, 0.001f, 0.0f))
					{
						l->SetLightInnerCone(PropertiesLightInnerCone);
						MarkSceneDirty();
					}
					UndoValueEdit<f32>(undoBaselineLightInnerCone, PropertiesLightInnerCone, [this, lightId](const f32& before, const f32& after) {
						sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
							[this, lightId, before]() { ApplyLightInnerCone(lightId, before); },
							[this, lightId, after]() { ApplyLightInnerCone(lightId, after); }, "Set Light Inner Cone"));
					});
					if (ImGui::Checkbox("Cast Shadows", &AddForm_cs))
					{
						if (AddForm_cs)
						{
							l->EnableCastShadows(PropertiesShadowMapSize, PropertiesShadowMapSize, PropertiesShadowNear);
							l->SetShadowBias(PropertiesShadowBiasFactor, PropertiesShadowBiasUnits);
						}
						else
							l->DisableCastShadows();
						MarkSceneDirty();
					}
					if (ShowShadowProperties(l, false))
					{
						l->EnableCastShadows(PropertiesShadowMapSize, PropertiesShadowMapSize, PropertiesShadowNear);
						l->SetShadowBias(PropertiesShadowBiasFactor, PropertiesShadowBiasUnits);
						MarkSceneDirty();
					}
				}
				break;
				case SceneObjectTypes::PHYSICS_COMPONENT:
				{
					IPhysicsComponent* pcomp = (IPhysicsComponent*)SelectedSceneObject->GetPTR();
					ImGui::Text("Shape:");
					switch (pcomp->GetShape())
					{
						case CollisionShapes::Box:             ImGui::SameLine(); ImGui::Text("Box");             break;
						case CollisionShapes::Sphere:          ImGui::SameLine(); ImGui::Text("Sphere");          break;
						case CollisionShapes::Cylinder:        ImGui::SameLine(); ImGui::Text("Cylinder");        break;
						case CollisionShapes::Cone:            ImGui::SameLine(); ImGui::Text("Cone");            break;
						case CollisionShapes::Capsule:         ImGui::SameLine(); ImGui::Text("Capsule");         break;
						case CollisionShapes::MultipleSphere:  ImGui::SameLine(); ImGui::Text("Multiple Sphere"); break;
						case CollisionShapes::ConvexHull:      ImGui::SameLine(); ImGui::Text("Convex Hull");     break;
						case CollisionShapes::ConvexTriangleMesh: ImGui::SameLine(); ImGui::Text("Convex Tri Mesh"); break;
						case CollisionShapes::TriangleMesh:    ImGui::SameLine(); ImGui::Text("Triangle Mesh");   break;
						case CollisionShapes::StaticPlane:     ImGui::SameLine(); ImGui::Text("Static Plane");    break;
						case CollisionShapes::Vehicle:         ImGui::SameLine(); ImGui::Text("Vehicle");         break;
						default:                              ImGui::SameLine(); ImGui::Text("Unknown");         break;
					}
					{
						f32 massVal = pcomp->GetMass();
						if (pcomp->GetShape() == CollisionShapes::StaticPlane)
						{
							ImGui::BeginDisabled();
							ImGui::DragFloat("Mass", &massVal, 0.01f, 0.0f, MAX_F32);
							ImGui::EndDisabled();
							ImGui::TextDisabled("Static planes are always immovable");
						}
						else if (ImGui::DragFloat("Mass", &massVal, 0.01f, 0.0f, MAX_F32))
						{
							pcomp->SetMass(massVal);
							SyncPhysicsForGameObject(pcomp->GetOwner());
						}
					}
					{
						bool ghostVal = pcomp->IsGhost();
						ImGui::Checkbox("Ghost (Trigger)", &ghostVal);
						ImGui::SameLine(); ImGui::Text("(edit in add form)");
					}
					ImGui::Separator();
					ImGui::Text("Velocity:");
					{
						Vec3 linVel = pcomp->GetLinearVelocity();
						ImGui::DragFloat3("Linear", (float*)&linVel, 0.01f, 0.0f, 0.0f);
						if (ImGui::Button("Apply Linear Velocity"))
							pcomp->SetLinearVelocity(linVel);
					}
					{
						Vec3 angVel = pcomp->GetAngularVelocity();
						ImGui::DragFloat3("Angular", (float*)&angVel, 0.01f, 0.0f, 0.0f);
						if (ImGui::Button("Apply Angular Velocity"))
							pcomp->SetAngularVelocity(angVel);
					}
					ImGui::Separator();
					if (ImGui::Button("Apply Upward Impulse"))
						pcomp->ApplyCentralImpulse(Vec3(0, 5, 0));
					ImGui::SameLine();
					if (ImGui::Button("Clean Forces"))
						pcomp->CleanForces();
				}
				break;
				case SceneObjectTypes::AUDIO_SOURCE_COMPONENT:
				{
					AudioSource* asrc = (AudioSource*)SelectedSceneObject->GetPTR();
					if (asrc && !asrc->IsLoaded())
						asrc->EnsureLoaded();
					ImGui::Text("File: %s", DisplayPath(asrc->GetFile()).c_str());
					if (!asrc->IsLoaded())
						ImGui::TextColored(ImVec4(1.f, 0.4f, 0.3f, 1.f), "Not loaded (missing file or no audio device)");
					else
						ImGui::TextDisabled("%s", asrc->IsStreamed() ? "Streamed" : "Decoded in memory");

					ImGui::Separator();
					if (ImGui::Button(asrc->IsPlaying() ? "Pause" : "Play"))
					{
						if (asrc->IsPlaying()) asrc->Pause();
						else asrc->Play();
					}
					ImGui::SameLine();
					if (ImGui::Button("Stop"))
						asrc->Stop();
					ImGui::SameLine();
					if (ImGui::Button("Fade In"))
						asrc->FadeIn(500.f);
					ImGui::SameLine();
					if (ImGui::Button("Fade Out"))
						asrc->FadeOut(500.f);

					{
						bool loop = asrc->IsLooping();
						if (ImGui::Checkbox("Loop", &loop))
							asrc->SetLooping(loop);
					}
					{
						bool spat = asrc->IsSpatialized();
						if (ImGui::Checkbox("Spatialized", &spat))
							asrc->SetSpatialization(spat);
					}
					{
						f32 vol = asrc->GetVolume();
						if (ImGui::DragFloat("Volume", &vol, 0.01f, 0.0f, 4.0f))
							asrc->SetVolume(vol);
					}
					{
						f32 pitch = asrc->GetPitch();
						if (ImGui::DragFloat("Pitch", &pitch, 0.01f, 0.01f, 4.0f))
							asrc->SetPitch(pitch);
					}

					if (asrc->IsSpatialized())
					{
						ImGui::Separator();
						ImGui::Text("3D Attenuation");
						f32 minD = asrc->GetMinDistance();
						f32 maxD = asrc->GetMaxDistance();
						int modelIdx = (int)asrc->GetAttenuationModel();
						if (modelIdx < 0 || modelIdx > 3) modelIdx = 1;
						const char* models[] = { "None", "Inverse", "Linear", "Exponential" };
						bool attenChanged = false;
						if (ImGui::Combo("Attenuation", &modelIdx, models, 4))
							attenChanged = true;
						if (ImGui::DragFloat("Min Distance", &minD, 0.1f, 0.01f, MAX_F32))
							attenChanged = true;
						if (ImGui::DragFloat("Max Distance", &maxD, 0.1f, 0.01f, MAX_F32))
							attenChanged = true;
						if (attenChanged)
							asrc->SetAttenuation((uint32)modelIdx, minD, maxD);

						{
							f32 dirAtt = asrc->GetDirectionalAttenuation();
							if (ImGui::DragFloat("Listener Direction Factor", &dirAtt, 0.01f, 0.0f, 1.0f))
								asrc->SetDirectionalAttenuation(dirAtt);
						}
						{
							f32 doppler = asrc->GetDopplerFactor();
							if (ImGui::DragFloat("Doppler Factor", &doppler, 0.01f, 0.0f, 2.0f))
								asrc->SetDopplerFactor(doppler);
						}

						ImGui::Separator();
						bool useCone = asrc->HasCone();
						if (ImGui::Checkbox("Directional Cone", &useCone))
						{
							if (useCone)
								asrc->SetCone(DEGTORAD(30.f), DEGTORAD(60.f), 0.f);
							else
								asrc->ClearCone();
						}
						if (asrc->HasCone())
						{
							f32 innerDeg = RADTODEG(asrc->GetConeInnerAngle());
							f32 outerDeg = RADTODEG(asrc->GetConeOuterAngle());
							f32 outerGain = asrc->GetConeOuterGain();
							bool coneChanged = false;
							if (ImGui::DragFloat("Inner Angle (deg)", &innerDeg, 0.5f, 0.f, 180.f))
								coneChanged = true;
							if (ImGui::DragFloat("Outer Angle (deg)", &outerDeg, 0.5f, 0.f, 180.f))
								coneChanged = true;
							if (ImGui::DragFloat("Outer Gain", &outerGain, 0.01f, 0.f, 1.f))
								coneChanged = true;
							if (coneChanged)
								asrc->SetCone(DEGTORAD(innerDeg), DEGTORAD(outerDeg), outerGain);
							ImGui::TextDisabled("Aimed along the GameObject forward axis");
						}
					}
					else
					{
						f32 pan = asrc->GetPan();
						if (ImGui::DragFloat("Pan", &pan, 0.01f, -1.0f, 1.0f))
							asrc->SetPan(pan);
					}

					ImGui::Separator();
					if (ImGui::CollapsingHeader("Filter / EQ / Delay"))
					{
						int filterIdx = (int)asrc->GetFilterType();
						const char* filters[] = { "None", "LowPass", "HighPass", "BandPass" };
						if (filterIdx < 0 || filterIdx > 3) filterIdx = 0;
						f32 cutoff = asrc->GetFilterCutoff();
						int order = (int)asrc->GetFilterOrder();
						if (order < 1) order = 2;
						bool filterChanged = false;
						if (ImGui::Combo("Filter", &filterIdx, filters, 4))
							filterChanged = true;
						if (filterIdx != AudioFilterType::None)
						{
							if (ImGui::DragFloat("Cutoff Hz", &cutoff, 1.f, 20.f, 20000.f))
								filterChanged = true;
							if (ImGui::DragInt("Order", &order, 1, 1, 8))
								filterChanged = true;
						}
						if (filterChanged)
						{
							if (filterIdx == AudioFilterType::None)
								asrc->ClearFilter();
							else
								asrc->SetFilter((uint32)filterIdx, cutoff, (uint32)order);
						}

						ImGui::Separator();
						int eqIdx = (int)asrc->GetEQType();
						const char* eqs[] = { "None", "Peak", "Notch", "LowShelf", "HighShelf" };
						if (eqIdx < 0 || eqIdx > 4) eqIdx = 0;
						f32 eqFreq = asrc->GetEQFrequency();
						f32 eqGain = asrc->GetEQGain();
						f32 eqQ = asrc->GetEQQ();
						if (eqFreq < 1.f) eqFreq = 1000.f;
						if (eqQ < 0.01f) eqQ = 1.f;
						bool eqChanged = false;
						if (ImGui::Combo("EQ", &eqIdx, eqs, 5))
							eqChanged = true;
						if (eqIdx != AudioEQType::None)
						{
							if (ImGui::DragFloat("EQ Frequency Hz", &eqFreq, 1.f, 20.f, 20000.f))
								eqChanged = true;
							if (eqIdx != AudioEQType::Notch)
							{
								if (ImGui::DragFloat("EQ Gain dB", &eqGain, 0.1f, -24.f, 24.f))
									eqChanged = true;
							}
							if (ImGui::DragFloat("EQ Q", &eqQ, 0.01f, 0.1f, 20.f))
								eqChanged = true;
						}
						if (eqChanged)
						{
							if (eqIdx == AudioEQType::None)
								asrc->ClearEQ();
							else
								asrc->SetEQ((uint32)eqIdx, eqFreq, eqGain, eqQ);
						}

						ImGui::Separator();
						bool useDelay = asrc->HasDelay();
						if (ImGui::Checkbox("Delay / Echo", &useDelay))
						{
							if (useDelay)
								asrc->SetDelay(0.2f, 0.5f, 1.f, 1.f);
							else
								asrc->ClearDelay();
						}
						if (asrc->HasDelay())
						{
							f32 delaySec = asrc->GetDelaySeconds();
							f32 decay = asrc->GetDelayDecay();
							f32 wet = asrc->GetDelayWet();
							f32 dry = asrc->GetDelayDry();
							bool delayChanged = false;
							if (ImGui::DragFloat("Delay Seconds", &delaySec, 0.01f, 0.01f, 5.f))
								delayChanged = true;
							if (ImGui::DragFloat("Decay", &decay, 0.01f, 0.f, 0.99f))
								delayChanged = true;
							if (ImGui::DragFloat("Wet", &wet, 0.01f, 0.f, 1.f))
								delayChanged = true;
							if (ImGui::DragFloat("Dry", &dry, 0.01f, 0.f, 1.f))
								delayChanged = true;
							if (delayChanged)
								asrc->SetDelay(delaySec, decay, wet, dry);
						}
					}

					if (asrc->IsLoaded())
					{
						ImGui::Separator();
						const f32 len = asrc->GetLengthSeconds();
						f32 cursor = asrc->GetCursorSeconds();
						ImGui::TextDisabled("Length: %.2fs", len);
						if (len > 0.01f && ImGui::SliderFloat("Seek", &cursor, 0.f, len, "%.2fs"))
							asrc->SeekSeconds(cursor);
						if (asrc->AtEnd())
							ImGui::TextDisabled("At end");
					}
				}
				break;
				case SceneObjectTypes::PARTICLE_SYSTEM_COMPONENT:
				{
					ParticleSystem* ps = (ParticleSystem*)SelectedSceneObject->GetPTR();
					if (ps == NULL) break;
					const uint32 psId = SelectedSceneObject->GetID();
					// Read straight from the live desc every frame - the two
					// draft fields below are the only cached state, seeded
					// once per selection.
					const ParticleSystemDesc& d = ps->GetDesc();
					if (propertiesParticleSeededId != psId)
					{
						propertiesParticleSeededId = psId;
						// DisplayPath, not the Texture's own filename: the
						// texture was loaded through ResolveAssetPath, so it
						// remembers the ABSOLUTE path it came from, and
						// showing that put a machine-specific
						// /Users/... string in the inspector. What the desc
						// actually stores (and what gets serialized) is the
						// project-relative path, so that is what belongs on
						// screen. Falls back to the raw name with no project
						// open, where there is nothing to be relative to.
						propertiesParticleTexturePath = d.texture
							? (project ? project->DisplayPath(d.texture->GetFilename())
								: d.texture->GetFilename())
							: std::string();
						propertiesParticleMax = (int32)d.maxParticles;
					}
					// Undo commit for a drag/type gesture on any of the
					// widgets below: they apply live (so the viewport follows
					// the drag), and this fires once at the end of the gesture
					// with the whole before/after desc - see PushParticleDescCommand.
					auto commitParticleEdit = [this, psId, ps](const char* label) {
						const std::string name(label);
						UndoValueEdit<ParticleSystemDesc>(undoBaselineParticleDesc, ps->GetDesc(),
							[this, psId, name](const ParticleSystemDesc& before, const ParticleSystemDesc& after) {
								PushParticleDescCommand(psId, before, after, name);
							});
					};

					ImGui::Text("%u / %u particles alive", ps->GetLiveParticleCount(), d.maxParticles);
					ImGui::TextDisabled("Previewing because this is selected - emitters only run in Play, or while selected here");
					// Playback is preview state, not scene data (it is not
					// serialized) - deliberately no undo entry and no dirty flag.
					if (ImGui::Button(ps->IsPlaying() ? "Stop" : "Play"))
					{
						if (ps->IsPlaying()) ps->Stop();
						else ps->Play();
					}
					ImGui::SameLine();
					if (ImGui::Button("Restart"))
					{
						// Clear before Play: for a one-shot burst Play() emits
						// a fresh burst, and without the clear the previous
						// one's survivors would still be in the way.
						ps->Clear();
						ps->Play();
					}
					ImGui::SameLine();
					if (ImGui::Button("Clear"))
						ps->Clear();
					if (!d.looping)
						ImGui::TextDisabled("One-shot burst - Restart to fire it again");

					ImGui::Separator();
					ImGui::Text("Emission");
					{
						// Discrete widgets (checkbox/combo/button) don't go
						// through UndoValueEdit: there is no drag gesture to
						// bracket, so the command is pushed on the spot from a
						// copy of the desc taken before the change.
						bool looping = d.looping;
						if (ImGui::Checkbox("Looping", &looping))
						{
							ParticleSystemDesc after = d;
							after.looping = looping;
							PushParticleDescCommand(psId, d, after, "Toggle Particle Looping");
						}
					}
					if (d.looping)
					{
						f32 rate = d.emissionRate;
						if (ImGui::DragFloat("Rate (per second)", &rate, 0.5f, 0.f, 10000.f))
						{
							ps->SetEmissionRate(rate);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Emission Rate");
					}
					{
						int32 burst = (int32)d.burstCount;
						const char* label = d.looping ? "Particles per Tick" : "Burst Count";
						if (ImGui::DragInt(label, &burst, 1, 1, 100000))
						{
							ps->SetBurstCount((uint32)(burst < 1 ? 1 : burst));
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Burst Count");
					}
					{
						f32 life[2] = { d.minLifetime, d.maxLifetime };
						if (ImGui::DragFloat2("Lifetime (min/max)", life, 0.01f, 0.01f, 1000.f))
						{
							if (life[1] < life[0]) life[1] = life[0];
							ps->SetLifetime(life[0], life[1]);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Lifetime");
					}

					ImGui::Separator();
					ImGui::Text("Shape");
					{
						Vec3 dir = d.direction;
						if (ImGui::DragFloat3("Direction", (float*)&dir, 0.01f, -1.f, 1.f))
						{
							// A zero direction has no axis to build the
							// emission cone around - normalize() would divide
							// by zero and every particle would spawn NaN.
							if (dir.magnitude() < 1e-4f) dir = Vec3(0.f, 1.f, 0.f);
							ps->SetDirection(dir);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Direction");
						ImGui::TextDisabled("World space - not rotated by the emitter");
					}
					{
						f32 spreadDeg = (f32)RADTODEG(d.spreadAngle);
						if (ImGui::DragFloat("Spread (deg)", &spreadDeg, 0.5f, 0.f, 180.f))
						{
							ps->SetSpread((f32)DEGTORAD(spreadDeg));
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Spread");
					}
					{
						f32 speed[2] = { d.minSpeed, d.maxSpeed };
						if (ImGui::DragFloat2("Speed (min/max)", speed, 0.01f, 0.f, 1000.f))
						{
							if (speed[1] < speed[0]) speed[1] = speed[0];
							ps->SetSpeed(speed[0], speed[1]);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Speed");
					}

					ImGui::Separator();
					ImGui::Text("Motion");
					{
						Vec3 gravity = d.gravity;
						if (ImGui::DragFloat3("Gravity", (float*)&gravity, 0.05f, -100.f, 100.f))
						{
							ps->SetGravity(gravity);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Gravity");
					}
					{
						f32 damping = d.damping;
						if (ImGui::DragFloat("Damping", &damping, 0.01f, 0.f, 20.f))
						{
							ps->SetDamping(damping);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Damping");
					}
					{
						f32 rot[2] = { d.minRotationSpeed, d.maxRotationSpeed };
						if (ImGui::DragFloat2("Spin rad/s (min/max)", rot, 0.01f, -50.f, 50.f))
						{
							if (rot[1] < rot[0]) rot[1] = rot[0];
							ps->SetRotationSpeed(rot[0], rot[1]);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Spin");
					}

					ImGui::Separator();
					ImGui::Text("Appearance");
					{
						Vec4 startColor = d.startColor;
						if (ImGui::ColorEdit4("Start Color", (float*)&startColor))
						{
							ps->SetColors(startColor, d.endColor);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Start Color");
						Vec4 endColor = d.endColor;
						if (ImGui::ColorEdit4("End Color", (float*)&endColor))
						{
							ps->SetColors(d.startColor, endColor);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle End Color");
					}
					{
						f32 startSize = d.startSize;
						if (ImGui::DragFloat("Start Size", &startSize, 0.01f, 0.f, 1000.f))
						{
							ps->SetSizes(startSize, d.endSize, d.sizeRandomJitter);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Start Size");
						f32 endSize = d.endSize;
						if (ImGui::DragFloat("End Size", &endSize, 0.01f, 0.f, 1000.f))
						{
							ps->SetSizes(d.startSize, endSize, d.sizeRandomJitter);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle End Size");
						f32 jitter = d.sizeRandomJitter;
						if (ImGui::DragFloat("Size Jitter", &jitter, 0.01f, 0.f, 1.f))
						{
							ps->SetSizes(d.startSize, d.endSize, jitter);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Size Jitter");
					}
					{
						// How each ramp travels between its two ends. Bezier
						// is withheld: it reads an incoming tangent from the
						// next key, and a ramp has no next key.
						uchar sizeEase = d.sizeEase;
						if (EasingUI::Picker("Size Ramp", sizeEase, /*allowBezier=*/false))
						{
							ps->SetEasing(sizeEase, d.colorEase);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Size Ramp");
						ImGui::SameLine();
						EasingUI::Curve(d.sizeEase);

						uchar colorEase = d.colorEase;
						if (EasingUI::Picker("Colour Ramp", colorEase, /*allowBezier=*/false))
						{
							ps->SetEasing(d.sizeEase, colorEase);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Colour Ramp");
						ImGui::SameLine();
						EasingUI::Curve(d.colorEase);
					}
					{
						f32 fadeIn = d.fadeInFraction;
						if (ImGui::DragFloat("Fade In (fraction)", &fadeIn, 0.01f, 0.f, 1.f))
						{
							ps->SetFade(fadeIn, d.fadeOutFraction);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Fade In");
						f32 fadeOut = d.fadeOutFraction;
						if (ImGui::DragFloat("Fade Out (fraction)", &fadeOut, 0.01f, 0.f, 1.f))
						{
							ps->SetFade(d.fadeInFraction, fadeOut);
							MarkSceneDirty();
						}
						commitParticleEdit("Set Particle Fade Out");
					}
					{
						int32 blend = (int32)d.blendMode;
						const char* blendModes[] = { "Alpha Blend", "Additive" };
						if (ImGui::Combo("Blend Mode", &blend, blendModes, 2))
						{
							ParticleSystemDesc after = d;
							after.blendMode = (uint32)blend;
							PushParticleDescCommand(psId, d, after, "Set Particle Blend Mode");
						}
					}

					ImGui::Separator();
					ImGui::Text("Sprite");
					ImGui::FilePath("Texture", "", "png,jpg,jpeg,tga,bmp,dds", &propertiesParticleTexturePath, 1024, &showDir);
					if (ImGui::Button("Apply Sprite"))
					{
						const std::string spritePath = ImportParticleTexture(propertiesParticleTexturePath);
						if (std::shared_ptr<Texture> tex = LoadParticleTexture(spritePath))
						{
							// The desc carries the shared_ptr itself, so undo
							// puts the previous sprite back without reloading it.
							ParticleSystemDesc after = d;
							after.texture = tex;
							PushParticleDescCommand(psId, d, after, "Set Particle Sprite");
						}
					}

					ImGui::Separator();
					// Not applied live: the capacity is the one setting whose
					// every intermediate drag value would reallocate the GPU
					// buffer and kill every live particle.
					ImGui::DragInt("Max Particles", &propertiesParticleMax, 1, 1, 100000);
					ImGui::SameLine();
					if (ImGui::Button("Apply"))
					{
						ParticleSystemDesc after = d;
						after.maxParticles = (uint32)(propertiesParticleMax < 1 ? 1 : propertiesParticleMax);
						PushParticleDescCommand(psId, d, after, "Set Particle Max Particles");
					}
				}
				break;
			default:

				break;
			}

			ImGui::Unindent(5.f);
		}

	}

	#define MAX_INT32 2147483647
	void SceneEditor::ShowAddForm()
	{
		if (openAddFormTrigger)
		{
			ImGuiViewport* vp = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::OpenPopup("Add");
			openAddFormTrigger = false;
		}
		
		if (ImGui::BeginPopupModal("Add", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			editorDisabled = true;

			switch (showingAddFormType)
			{
			case 0:
				// Game Object
				break;
			case 1:
				// Cube
				ImGui::Text("Cube");
				ImGui::DragFloat("Width", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Height", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Depth", &AddForm_d, 0.01f, 0.001f, MAX_F32);
				break;
			case 2:
				// Sphere
				ImGui::Text("Sphere");
				ImGui::DragFloat("Radius", &AddForm_w, 0.001f, 0.001f, MAX_F32);
				ImGui::DragInt("Segments W", &AddForm_sw, 1, 1, 512);
				ImGui::DragInt("Segments H", &AddForm_sh, 1, 1, 512);
				ImGui::Checkbox("Half Sphere", &AddForm_hs);
				break;
			case 3:
				// Capsule
				ImGui::Text("Capsule");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Height", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragInt("Rings", &AddForm_r, 1, 1, 512);
				ImGui::DragInt("Segments W", &AddForm_sw, 1, 1, 512);
				ImGui::DragInt("Segments H", &AddForm_sh, 1, 1, 512);
				break;
			case 4:
				// Plane
				ImGui::Text("Plane");
				ImGui::DragFloat("Width", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Height", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				break;
			case 5:
				// Cone
				ImGui::Text("Cone");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Height", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragInt("Segments W", &AddForm_sw, 1, 1, 512);
				ImGui::DragInt("Segments H", &AddForm_sh, 1, 1, 512);
				ImGui::Checkbox("Open Ended", &AddForm_oe);
				break;
			case 6:
				// Cylinder
				ImGui::Text("Cylinder");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Height", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragInt("Segments W", &AddForm_sw, 1, 1, 512);
				ImGui::DragInt("Segments H", &AddForm_sh, 1, 1, 512);
				ImGui::Checkbox("Open Ended", &AddForm_oe);
				break;
			case 7:
				// Torus
				ImGui::Text("Torus");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Tube", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragInt("Segments W", &AddForm_sw, 1, 1, 512);
				ImGui::DragInt("Segments H", &AddForm_sh, 1, 1, 512);
				break;
			case 8:
				// Torus
				ImGui::Text("Torus Knot");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Tube", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragInt("Segments W", &AddForm_sw, 1, 1, 512);
				ImGui::DragInt("Segments H", &AddForm_sh, 1, 1, 512);
				ImGui::DragFloat("P", &AddForm_p, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Q", &AddForm_q, 0.01f, 0.001f, MAX_F32);
				ImGui::DragInt("Height Scale", &AddForm_hscale, 1, 1, 512);
				break;
			case 9:
				// Model - prefer project assets; convert source formats via AssimpImporter.
				ImGui::Text("Import Model");
				if (project && project->IsOpen())
					ImGui::TextDisabled("Imports into assets/models/ as .p3dm");
				else
					ImGui::TextDisabled("Open a project to convert/import into assets/models");
				ImGui::FilePath("Path", "", "p3dm,obj,fbx,dae,gltf,glb,blend,3ds", &AddForm_modelPath, 1024, &showDir);
				break;
			case 10:
				// Directional Light
				ImGui::Text("Directional Light");
				ImGui::DragFloat3("Direction", (float *)&AddForm_dir, 0.01f, -1.0f, 1.0f);
				ImGui::ColorEdit4("Color", (float*)&AddForm_color);
				break;
			case 11:
				// Point Light
				ImGui::Text("Point Light");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::ColorEdit4("Color", (float*)&AddForm_color);
				break;
			case 12:
				// Spot Light
				ImGui::Text("Spot Light");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat3("Direction", (float *)&AddForm_dir, 0.01f, -1.0f, 1.0f);
				ImGui::ColorEdit4("Color", (float*)&AddForm_color);
				ImGui::DragFloat("Outter Cone", &AddForm_oc, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Inner Cone", &AddForm_ic, 0.01f, 0.001f, MAX_F32);
				break;
			case 13:
				ImGui::Text("Physics Box");
				ImGui::DragFloat("Width", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Height", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Depth", &AddForm_d, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Mass", &AddForm_mass, 0.01f, 0.0f, MAX_F32);
				ImGui::TextDisabled("Mass 0 = static (immovable)");
				ImGui::Checkbox("Ghost (Trigger)", &AddForm_ghost);
				break;
			case 14:
				ImGui::Text("Physics Capsule");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Height", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Mass", &AddForm_mass, 0.01f, 0.0f, MAX_F32);
				ImGui::Checkbox("Ghost (Trigger)", &AddForm_ghost);
				break;
			case 15:
				ImGui::Text("Physics Cone");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Height", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Mass", &AddForm_mass, 0.01f, 0.0f, MAX_F32);
				ImGui::Checkbox("Ghost (Trigger)", &AddForm_ghost);
				break;
			case 16:
				ImGui::Text("Physics Cylinder");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Height", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Mass", &AddForm_mass, 0.01f, 0.0f, MAX_F32);
				ImGui::Checkbox("Ghost (Trigger)", &AddForm_ghost);
				break;
			case 17:
				ImGui::Text("Physics Sphere");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Mass", &AddForm_mass, 0.01f, 0.0f, MAX_F32);
				ImGui::Checkbox("Ghost (Trigger)", &AddForm_ghost);
				break;
			case 18:
				ImGui::Text("Physics Static Plane");
				ImGui::DragFloat3("Normal", (float *)&AddForm_dir, 0.01f, -1.0f, 1.0f);
				ImGui::DragFloat("Constant", &AddForm_w, 0.01f, 0.0f, MAX_F32);
				ImGui::TextDisabled("Infinite static collider (mass is always 0)");
				ImGui::Checkbox("Ghost (Trigger)", &AddForm_ghost);
				break;
			case 19:
				ImGui::Text("Sound (Audio Source)");
				ImGui::FilePath("Path", "", "wav,ogg,mp3,flac", &AddForm_soundPath, 1024, &showDir);
				ImGui::TextDisabled("wav / ogg / mp3 / flac");
				ImGui::Checkbox("Stream", &AddForm_stream);
				ImGui::SameLine();
				ImGui::TextDisabled("(music / long ambience)");
				ImGui::Checkbox("Loop", &AddForm_loop);
				ImGui::Checkbox("Spatialized", &AddForm_spatialized);
				ImGui::SameLine();
				ImGui::TextDisabled("(3D positional)");
				ImGui::DragFloat("Volume", &AddForm_volume, 0.01f, 0.0f, 4.0f);
				break;
			case 20:
			{
				// Only the three things that are awkward to change afterwards
				// are asked for here - everything else in ParticleSystemDesc
				// is live-editable in the Properties panel.
				ImGui::Text("Particle System");
				const char* presets[] = { "Default", "Fire", "Smoke", "Explosion (burst)" };
				ImGui::Combo("Preset", &AddForm_particlePreset, presets, 4);
				ImGui::FilePath("Sprite", "", "png,jpg,jpeg,tga,bmp,dds", &AddForm_particleTexturePath, 1024, &showDir);
				if (AddForm_particleTexturePath.empty())
					ImGui::TextDisabled("Leave empty for the default soft round sprite");
				else if (project && project->IsOpen())
					ImGui::TextDisabled("Copied into assets/textures/");
				ImGui::DragInt("Max Particles", &AddForm_particleMax, 1, 1, 100000);
				ImGui::TextDisabled("Fixed capacity - spawns past it are dropped");
			}
				break;
			default:
				break;
			}

			if (showingAddFormType < 10 && showingAddFormType > 0)
			{
				ImGui::Checkbox("Smooth Normals", &AddForm_sn);
				ImGui::Checkbox("Flip Normals", &AddForm_fn);
			}

			// The selection has to be a GameObject, not merely non-NULL:
			// AddFormSubmit() casts SelectedSceneObject->GetPTR() straight to
			// GameObject* and calls Add() on it, so offering "use the current
			// selection as the parent" while a light or a rendering component
			// is selected handed that cast a component pointer. Forcing a new
			// GameObject in every other case is what the NULL branch already
			// did.
			if (SelectedSceneObject != NULL && SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT && showingAddFormType > 0)
			{
				ImGui::Checkbox("Create GameObject", &AddForm_cgo);
			}
			else AddForm_cgo = true;

			if (showingAddFormType == 0)
				AddForm_cgo = true;

            if (AddForm_cgo)
            {
                ImGui::InputText("GO Name", &AddForm_go);
            }

            if (ImGui::Button("Create"))
			{
				const bool needsPath = (showingAddFormType == 9 || showingAddFormType == 19);
				const bool hasPath = (showingAddFormType == 9) ? (AddForm_modelPath.size() != 0)
					: (showingAddFormType == 19) ? (AddForm_soundPath.size() != 0) : true;
				if (!needsPath || hasPath)
				{
                    if (AddForm_go.empty() && AddForm_cgo)
                        AddForm_go = "GameObject";
										AddFormSubmit();
					showingAddFrom = false;
					ImGui::CloseCurrentPopup();
					editorDisabled = false;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) { showingAddFrom = false; ImGui::CloseCurrentPopup(); editorDisabled = false; }
			ImGui::Spacing();

			ImGui::EndPopup();
		}
	}

	void SceneEditor::AddFormSubmit()
	{
		if (AddForm_cgo)
			CreateGameObject(AddForm_go);

		GameObject* ownerGO = GetSelectedOwnerGameObject();
		// Mesh / light / physics / sound components need a GameObject parent.
		// Creating a light while a light (or other component) is selected used
		// to cast the component pointer to GameObject* and segfault.
		const bool needsOwner = (showingAddFormType >= 1 && showingAddFormType <= 20);
		if (needsOwner && ownerGO == NULL)
		{
			echo("ERROR: Select a GameObject (or one of its components) before adding.");
			return;
		}

		// Attach-to-existing-GameObject case (AddForm_cgo false): capture the
		// owner's subtree before the switch below attaches anything, so the
		// whole gesture can be pushed as one ReplaceGameObjectCommand at the
		// end (see the AddForm_cgo check after the switch).
		uint32 attachOwnerId = 0;
		std::string attachBeforeSnapshot;
		if (!AddForm_cgo && ownerGO != NULL)
		{
			attachOwnerId = sceneObjects->GetSceneObjectID(ownerGO);
			if (attachOwnerId != 0)
				attachBeforeSnapshot = SnapshotSubtree(attachOwnerId);
		}

		switch (showingAddFormType)
		{
		case 0:
			// Game Object

			break;
		case 1:
			// Cube
			sceneObjects->CreateRenderingCube(ownerGO, AddForm_w, AddForm_h, AddForm_d, AddForm_sn, AddForm_fn);
			break;
		case 2:
			// Sphere
			sceneObjects->CreateRenderingSphere(ownerGO, AddForm_w, AddForm_sw, AddForm_sh, AddForm_sn, AddForm_hs, AddForm_fn);
			break;
		case 3:
			// Capsule
			sceneObjects->CreateRenderingCapsule(ownerGO, AddForm_w, AddForm_h, AddForm_r, AddForm_sw, AddForm_sh, AddForm_sn, AddForm_fn);
			break;
		case 4:
			// Plane
			sceneObjects->CreateRenderingPlane(ownerGO, AddForm_w, AddForm_h, AddForm_sn, AddForm_fn);
			break;
		case 5:
			// Cone
			sceneObjects->CreateRenderingCone(ownerGO, AddForm_w, AddForm_h, AddForm_sw, AddForm_sh, AddForm_oe, AddForm_sn, AddForm_fn);
			break;
		case 6:
			// Cylinder
			sceneObjects->CreateRenderingCylinder(ownerGO, AddForm_w, AddForm_h, AddForm_sw, AddForm_sh, AddForm_oe, AddForm_sn, AddForm_fn);
			break;
		case 7:
			// Torus
			sceneObjects->CreateRenderingTorus(ownerGO, AddForm_w, AddForm_h, AddForm_sw, AddForm_sh, AddForm_sn, AddForm_fn);
			break;
		case 8:
			// Torus Knot
			sceneObjects->CreateRenderingTorusKnot(ownerGO, AddForm_w, AddForm_h, AddForm_sw, AddForm_sh, AddForm_p, AddForm_q, AddForm_sn, AddForm_fn);
			break;
		case 9:
		{
			// Model — convert/copy into the open project's assets/models when possible.
			std::string modelPath = AddForm_modelPath;
			if (project && project->IsOpen())
			{
				std::string imported, err, trashedPackageDir;
				if (project->ImportModel(AddForm_modelPath, imported, &err, &trashedPackageDir))
				{
					modelPath = imported;
					if (!trashedPackageDir.empty())
					{
						const std::string importedRel = project->RelativePath(imported);
						const std::string packageRel = std::filesystem::path(importedRel).parent_path().string();
						if (!packageRel.empty())
							sceneUndo.Push(std::make_unique<ImportOverwriteCommand>(project, packageRel, trashedPackageDir,
								"Import Model (overwrite) '" + packageRel + "'"));
					}
				}
				else
					echo("ERROR: model import failed: " + err);
			}
			sceneObjects->CreateRenderingModel(ownerGO, modelPath);
		}
			break;
		case 10:
		{
			// Directional Light
			SceneObject* s = sceneObjects->CreateDirectionalLight(ownerGO, AddForm_dir, AddForm_color);
			std::shared_ptr<LightHelper> h = std::make_shared<LightHelper>(ownerGO);
			s->Helper = h;
			scene->Add(h);
		}
			break;
		case 11:
		{
			// Point Light
			SceneObject* s = sceneObjects->CreatePointLight(ownerGO, AddForm_w, AddForm_color);
			std::shared_ptr<LightHelper> h = std::make_shared<LightHelper>(ownerGO);
			s->Helper = h;
			scene->Add(h);
		}
			break;
		case 12:
		{
			// Spot Light
			SceneObject* s = sceneObjects->CreateSpotLight(ownerGO, AddForm_w, AddForm_dir, AddForm_oc, AddForm_ic, AddForm_color);
			std::shared_ptr<LightHelper> h = std::make_shared<LightHelper>(ownerGO);
			s->Helper = h;
			scene->Add(h);
		}
			break;
		case 13:
		{
			GameObject* go = ownerGO;
			std::shared_ptr<IPhysicsComponent> pcomp = physics->CreateBox(AddForm_w, AddForm_h, AddForm_d, AddForm_mass, AddForm_ghost);
			go->Add(pcomp);
			uint32 id = ++sceneObjects->_ID;
			SceneObject* obj = new SceneObject("Physics Box", pcomp.get(), id, SceneObjectTypes::PHYSICS_COMPONENT);
			sceneObjects->listObjects[id] = obj;
			obj->SetParentID(sceneObjects->GetSceneObjectID(go));
		}
			break;
		case 14:
		{
			GameObject* go = ownerGO;
			std::shared_ptr<IPhysicsComponent> pcomp = physics->CreateCapsule(AddForm_w, AddForm_h, AddForm_mass, AddForm_ghost);
			go->Add(pcomp);
			uint32 id = ++sceneObjects->_ID;
			SceneObject* obj = new SceneObject("Physics Capsule", pcomp.get(), id, SceneObjectTypes::PHYSICS_COMPONENT);
			sceneObjects->listObjects[id] = obj;
			obj->SetParentID(sceneObjects->GetSceneObjectID(go));
		}
			break;
		case 15:
		{
			GameObject* go = ownerGO;
			std::shared_ptr<IPhysicsComponent> pcomp = physics->CreateCone(AddForm_w, AddForm_h, AddForm_mass, AddForm_ghost);
			go->Add(pcomp);
			uint32 id = ++sceneObjects->_ID;
			SceneObject* obj = new SceneObject("Physics Cone", pcomp.get(), id, SceneObjectTypes::PHYSICS_COMPONENT);
			sceneObjects->listObjects[id] = obj;
			obj->SetParentID(sceneObjects->GetSceneObjectID(go));
		}
			break;
		case 16:
		{
			GameObject* go = ownerGO;
			std::shared_ptr<IPhysicsComponent> pcomp = physics->CreateCylinder(AddForm_w, AddForm_h, AddForm_mass, AddForm_ghost);
			go->Add(pcomp);
			uint32 id = ++sceneObjects->_ID;
			SceneObject* obj = new SceneObject("Physics Cylinder", pcomp.get(), id, SceneObjectTypes::PHYSICS_COMPONENT);
			sceneObjects->listObjects[id] = obj;
			obj->SetParentID(sceneObjects->GetSceneObjectID(go));
		}
			break;
		case 17:
		{
			GameObject* go = ownerGO;
			std::shared_ptr<IPhysicsComponent> pcomp = physics->CreateSphere(AddForm_w, AddForm_mass, AddForm_ghost);
			go->Add(pcomp);
			uint32 id = ++sceneObjects->_ID;
			SceneObject* obj = new SceneObject("Physics Sphere", pcomp.get(), id, SceneObjectTypes::PHYSICS_COMPONENT);
			sceneObjects->listObjects[id] = obj;
			obj->SetParentID(sceneObjects->GetSceneObjectID(go));
		}
			break;
		case 18:
		{
			GameObject* go = ownerGO;
			std::shared_ptr<IPhysicsComponent> pcomp = physics->CreateStaticPlane(AddForm_dir, AddForm_w, 0.0f, AddForm_ghost);
			go->Add(pcomp);
			uint32 id = ++sceneObjects->_ID;
			SceneObject* obj = new SceneObject("Physics Static Plane", pcomp.get(), id, SceneObjectTypes::PHYSICS_COMPONENT);
			sceneObjects->listObjects[id] = obj;
			obj->SetParentID(sceneObjects->GetSceneObjectID(go));
		}
			break;
		case 19:
		{
			GameObject* go = ownerGO;
			SceneObject* soundObj = sceneObjects->CreateAudioSource(go, ResolveSoundPath(AddForm_soundPath), AddForm_stream,
				AddForm_loop, AddForm_spatialized, AddForm_volume);
			if (soundObj)
			{
				std::shared_ptr<SoundHelper> h = std::make_shared<SoundHelper>(go);
				soundObj->Helper = h;
				scene->Add(h);
			}
		}
			break;
		case 20:
		{
			// Particle System
			ParticleSystemDesc desc;
			ApplyParticlePreset(desc, AddForm_particlePreset);
			desc.maxParticles = (uint32)(AddForm_particleMax < 1 ? 1 : AddForm_particleMax);
			// Import first: the desc keeps the loaded Texture, but the scene
			// file records where it came from, so it has to be a path the
			// project (and an exported game) can still resolve.
			const std::string spritePath = ImportParticleTexture(AddForm_particleTexturePath);
			desc.texture = LoadParticleTexture(spritePath);
			AttachParticleSystem(ownerGO, desc);
		}
			break;
		default:
			break;
		}
		MarkSceneDirty();
		// AddForm_cgo == true means CreateGameObject() above created a brand
		// new GameObject and the switch just attached its component(s) to
		// it - capture the whole gesture as one Add command. When false, an
		// existing GameObject was selected and a component was attached to
		// it instead - one Replace command (before/after snapshot of the
		// owner, captured above/here) covers that case identically to
		// DetachComponent, without needing per-component-kind undo logic.
		if (AddForm_cgo && SelectedSceneObject && SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT)
			PushAddCommand(SelectedSceneObject);
		else if (!AddForm_cgo && attachOwnerId != 0 && !attachBeforeSnapshot.empty())
			PushReplaceCommand(attachOwnerId, attachBeforeSnapshot, "Attach Component");
	}

	// Scene document entries for the host's File menu. Every New/Open/Save in
	// the editor - project or scene - now lives in that one menu; these are
	// pushed in from here rather than written inline by the host because the
	// state they drive (scenePath, the scene file dialog) is private to this
	// document.
	void SceneEditor::ShowFileMenuItems()
	{
		// "New Scene..." rather than "New Scene": what comes up is a choice of
		// 3D, 2D or UI. There is no sensible default - a 2D scene wants a
		// different camera, a different gizmo and a different set of
		// components from a 3D one - and the old arrangement made 3D the
		// silent default with 2D hidden behind a second, differently named
		// menu item further down the menu.
		if (ImGui::MenuItem("New Scene...", ""))
		{
			if (hostNewSceneKind)
				hostNewSceneKind();
			else if (hostNewSceneDocument)
				hostNewSceneDocument();
			else if (ConfirmUnsavedThen(UnsavedNewScene))
			{
				NewScene();
				if (project && project->IsOpen())
				{
					showingSceneDialog = true;
					sceneDialogIsSave = true;
					sceneDialogPath = "NewScene.json";
					sceneDialogError.clear();
				}
			}
		}

		if (ImGui::MenuItem("Open Scene...", ""))
		{
			if (hostOpenSceneDocument)
			{
				showingSceneDialog = true;
				sceneDialogIsSave = false;
				sceneDialogPath = scenePath;
				sceneDialogError.clear();
			}
			else if (ConfirmUnsavedThen(UnsavedOpenDialog))
			{
				showingSceneDialog = true;
				sceneDialogIsSave = false;
				sceneDialogPath = scenePath;
				sceneDialogError.clear();
			}
		}

		ImGui::Separator();

		// Saves straight over the current file once there is one; the
		// first save has nowhere to go, so it falls through to Save As.
		if (ImGui::MenuItem("Save Scene", ""))
		{
			if (scenePath.size() > 0)
				SaveSceneToFile(scenePath);
			else
			{
				showingSceneDialog = true;
				sceneDialogIsSave = true;
				sceneDialogPath = (project && project->IsOpen()) ? std::string("Untitled.json") : std::string("scene.json");
				sceneDialogError.clear();
			}
		}

		if (ImGui::MenuItem("Save Scene As...", ""))
		{
			showingSceneDialog = true;
			sceneDialogIsSave = true;
			if (scenePath.size() > 0)
				sceneDialogPath = scenePath;
			else if (project && project->IsOpen())
				sceneDialogPath = "Untitled.json";
			else
				sceneDialogPath = "scene.json";
			sceneDialogError.clear();
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Build Game...", "", false, project && project->IsOpen() && !playMode))
		{
			buildDialogError.clear();
			buildDialogResult.clear();
			buildDialogWarnings.clear();
			if (buildDialogOutputDir.empty() && project)
			{
				// Beside the project, not inside it - BuildGame refuses to
				// build into the project folder, and offering a path it will
				// reject is a worse first impression than a sensible default.
				buildDialogOutputDir = (std::filesystem::path(project->GetProjectPath()).parent_path()
					/ (project->GetProjectName() + "_Build")).string();
			}
			if (buildDialogTitle.empty() && project) buildDialogTitle = project->GetProjectName();
			if (buildDialogSceneRel.empty())
				buildDialogSceneRel = !scenePath.empty() && project
					? project->RelativePath(scenePath) : (project ? project->GetActiveSceneRel() : std::string());
			openBuildModal = true;
		}
	}

	void SceneEditor::DrawBuildModal()
	{
		if (openBuildModal)
		{
			ImGui::SetNextWindowFocus();
			ImGui::OpenPopup("Build Game");
			openBuildModal = false;
		}
		if (!ImGui::BeginPopupModal("Build Game", NULL, ImGuiWindowFlags_AlwaysAutoResize)) return;

		ImGui::TextUnformatted("Stages a runnable game folder: the player, the engine shaders,");
		ImGui::TextUnformatted("and this project's scenes and assets, with a game.json beside them.");
		ImGui::Spacing();

		ImGui::SetNextItemWidth(420.f);
		ImGui::InputText("Output folder", &buildDialogOutputDir);
		ImGui::SetNextItemWidth(420.f);
		ImGui::InputText("Window title", &buildDialogTitle);

		// Every scene in the project, so the one the game starts on is a
		// choice rather than whatever happened to be open.
		if (project)
		{
			std::vector<std::string> scenes;
			project->ListScenes(scenes);
			if (ImGui::BeginCombo("Startup scene", buildDialogSceneRel.c_str()))
			{
				for (size_t i = 0; i < scenes.size(); ++i)
					if (ImGui::Selectable(scenes[i].c_str(), scenes[i] == buildDialogSceneRel))
						buildDialogSceneRel = scenes[i];
				ImGui::EndCombo();
			}
		}

		ImGui::SetNextItemWidth(120.f);
		ImGui::InputInt("Width", &buildDialogWidth);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.f);
		ImGui::InputInt("Height", &buildDialogHeight);
		ImGui::Checkbox("Fullscreen", &buildDialogFullscreen);

		ImGui::Spacing();
		ImGui::TextDisabled("Renderer: %s (from Project Settings)",
			project && project->GetSettings().rendererType == ProjectRendererType::Deferred ? "deferred" : "forward");

		if (!buildDialogError.empty())
			ImGui::TextColored(ImVec4(1.f, 0.45f, 0.4f, 1.f), "%s", buildDialogError.c_str());
		if (!buildDialogResult.empty())
			ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.f), "%s", buildDialogResult.c_str());
		for (size_t i = 0; i < buildDialogWarnings.size(); ++i)
			ImGui::TextColored(ImVec4(1.f, 0.75f, 0.35f, 1.f), "! %s", buildDialogWarnings[i].c_str());

		ImGui::Spacing();
		if (ImGui::Button("Build", ImVec2(120, 0)) && project)
		{
			// The scene in the editor may have unsaved edits, and the build
			// copies files off disk - so what ships would silently be the
			// last saved version. Save first rather than explain that later.
			if (sceneDirty && !scenePath.empty()) SaveSceneToFile(scenePath);

			ProjectManager::BuildOptions opts;
			opts.outputDir = buildDialogOutputDir;
			opts.startupSceneRel = buildDialogSceneRel;
			opts.title = buildDialogTitle;
			opts.width = buildDialogWidth;
			opts.height = buildDialogHeight;
			opts.fullscreen = buildDialogFullscreen;
			opts.deferred = (project->GetSettings().rendererType == ProjectRendererType::Deferred);

			ProjectManager::BuildResult r = project->BuildGame(opts);
			buildDialogError = r.error;
			buildDialogWarnings = r.warnings;
			buildDialogResult = r.ok
				? ("Built " + std::to_string(r.filesCopied) + " file(s) into " + r.outputDir)
				: std::string();
			if (r.ok) echo("SUCCESS: Build Game - " + r.outputDir);
			else echo("ERROR: Build Game - " + r.error);
		}
		ImGui::SameLine();
		if (ImGui::Button("Close", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	void SceneEditor::ShowMenubarOptions()
	{
		// Creation only, mirroring the per-object "Add Component" menu. The
		// scene's file operations sit in File and its selection operations in
		// Edit, so nothing here needs a selection to be useful.
		if (ImGui::BeginMenu("GameObject"))
		{
			if (playMode)
				ImGui::TextDisabled("Stop play mode to edit");
			else
				ShowAddObjectMenuItems();
			ImGui::EndMenu();
		}

		// What acts on the scene as a whole, as opposed to on one object.
		if (ImGui::BeginMenu("Scene"))
		{
			if (playMode)
			{
				if (ImGui::MenuItem("Stop Playing", "Esc"))
					StopPlayMode();
			}
			else if (ImGui::MenuItem("Play"))
				EnterPlayMode();

			ImGui::Separator();

			if (ImGui::MenuItem("Open Scene Script", NULL, false, !scenePath.empty() && !playMode))
			{
				EnsureAndBindSceneCompanionScript();
				if (hostOpenLuaScript && !sceneMainScriptPath.empty())
					hostOpenLuaScript(sceneMainScriptPath);
			}

			ImGui::EndMenu();
		}
	}

	// Context-menu form of the GameObject menu. Same entries, wrapped in an
	// "Add" submenu because the popups this appears in carry other items too.
	void SceneEditor::ShowRightMenu()
	{
		if (playMode) return;
		if (ImGui::BeginMenu("Add", ""))
		{
			ShowAddObjectMenuItems();
			ImGui::EndMenu();
		}
	}

	// Ordered to match ShowAddComponentMenu - same submenus, same names, same
	// order within them - so that adding a thing as a new object and adding it
	// to an existing one read as the same menu.
	void SceneEditor::ShowAddObjectMenuItems()
	{
		if (ImGui::MenuItem("Empty GameObject", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 0; AddForm_go = "GameObject"; }
		if (ImGui::MenuItem("Camera", "")) CreateSceneCamera();
		ImGui::Separator();
		if (ImGui::BeginMenu("Mesh", ""))
		{
			if (ImGui::MenuItem("Cube", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 1; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_d = 1.0; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
			if (ImGui::MenuItem("Sphere", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 2; AddForm_w = 1.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_cgo = false; AddForm_hs = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
			if (ImGui::MenuItem("Capsule", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 3; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_r = 8.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
			if (ImGui::MenuItem("Cylinder", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 6; AddForm_w = 1.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_oe = false; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
			if (ImGui::MenuItem("Cone", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 5; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_oe = false; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
			if (ImGui::MenuItem("Plane", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 4; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
			if (ImGui::MenuItem("Torus", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 7; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
			if (ImGui::MenuItem("Torus Knot", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 8; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_p = 1.0; AddForm_q = 1.0; AddForm_hscale = 1.0; AddForm_cgo = false; AddForm_go.clear(); AddForm_sn = false; AddForm_fn = false; }
			ImGui::Separator();
			if (ImGui::MenuItem("Import Model...")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 9; AddForm_modelPath.clear(); AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; AddForm_cgo = false; }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Physics", ""))
		{
			if (ImGui::MenuItem("Box", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 13; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_d = 1.0; AddForm_mass = 1.0f; AddForm_ghost = false; AddForm_cgo = false; AddForm_go = ""; }
			if (ImGui::MenuItem("Sphere", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 17; AddForm_w = 0.5f; AddForm_mass = 1.0f; AddForm_ghost = false; AddForm_cgo = false; AddForm_go = ""; }
			if (ImGui::MenuItem("Capsule", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 14; AddForm_w = 0.5f; AddForm_h = 1.0; AddForm_mass = 1.0f; AddForm_ghost = false; AddForm_cgo = false; AddForm_go = ""; }
			if (ImGui::MenuItem("Cylinder", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 16; AddForm_w = 0.5f; AddForm_h = 1.0; AddForm_mass = 1.0f; AddForm_ghost = false; AddForm_cgo = false; AddForm_go = ""; }
			if (ImGui::MenuItem("Cone", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 15; AddForm_w = 0.5f; AddForm_h = 1.0; AddForm_mass = 1.0f; AddForm_ghost = false; AddForm_cgo = false; AddForm_go = ""; }
			if (ImGui::MenuItem("Static Plane", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 18; AddForm_dir = Vec3(0, 1, 0); AddForm_w = 0.0f; AddForm_mass = 0.0f; AddForm_ghost = false; AddForm_cgo = false; AddForm_go = ""; }
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Audio Source", ""))
		{
			showingAddFrom = true;
			openAddFormTrigger = true;
			showingAddFormType = 19;
			AddForm_soundPath.clear();
			AddForm_stream = false;
			AddForm_loop = false;
			AddForm_spatialized = true;
			AddForm_volume = 1.0f;
			AddForm_cgo = false;
			AddForm_go = "";
		}
		if (ImGui::MenuItem("Particle System", ""))
		{
			showingAddFrom = true;
			openAddFormTrigger = true;
			showingAddFormType = 20;
			AddForm_particleTexturePath.clear();
			AddForm_particleMax = 200;
			AddForm_particlePreset = 0;
			AddForm_cgo = false;
			AddForm_go = "";
		}
		ImGui::Separator();
		if (ImGui::BeginMenu("Lights", ""))
		{
			if (ImGui::MenuItem("Directional", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 10; AddForm_color = Vec4(1, 1, 1, 1); AddForm_dir = Vec3(0, -1, 0); AddForm_cs = false; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
			if (ImGui::MenuItem("Point", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 11; AddForm_w = 10.0; AddForm_color = Vec4(1, 1, 1, 1); AddForm_cs = false; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
			if (ImGui::MenuItem("Spot", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 12; AddForm_w = 10.0; AddForm_color = Vec4(1, 1, 1, 1); AddForm_dir = Vec3(0, -1, 0); AddForm_cs = false; AddForm_oc = 45.f; AddForm_ic = 30.f; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
			ImGui::EndMenu();
		}
	}

	// The icon sheet as an ImTextureID. Every ImageButton below used to pass
	// icons->GetBindID() straight through - a raw GL texture name. ImGui's
	// Vulkan backend reinterprets ImTextureID as a VkDescriptorSet, so a small
	// integer became a garbage descriptor and MoltenVK crashed inside
	// vkQueueSubmit -> bindDescriptorSets. Only reachable with something
	// selected, which is why the Vulkan editor looked fine until then.
	ImTextureID SceneEditor::IconsTextureID() const
	{
		if (icons == NULL) return (ImTextureID)0;
		return (ImTextureID)GetActiveRenderDevice().GetImGuiTextureID(icons->GetBindID(), icons->GetTextureType());
	}

	void SceneEditor::ShowTools()
	{
		if (playMode)
		{
			ImGui::TextDisabled("Play mode — stop to use tools");
			return;
		}
		if (SelectedSceneObject != NULL)
		{
			// A backend that cannot hand ImGui a texture handle returns NULL;
			// drawing the buttons anyway would submit an invalid descriptor.
			const ImTextureID iconsTex = IconsTextureID();
			if (iconsTex == (ImTextureID)0)
			{
				ImGui::TextDisabled("(tool icons unavailable on this backend)");
				return;
			}
			ImGui::Spacing();
			switch (SelectedSceneObject->GetType())
			{
			case SceneObjectTypes::GAMEOBJECT:
			{
				f32 nrItems = 9;
				f32 bid = 0;
				ImGui::PushID(bid);
                if (ImGui::ImageButton(
                    "##img_btn",
                    iconsTex,
                    ImVec2(16, 16),
                    (GizmoInUse == GizmoFunction::TRANSLATION ? ImVec2((bid + 1) * 16.f / (nrItems*16.f), 0) : ImVec2(bid * 16.f / (nrItems*16.f), 0)),
                    (GizmoInUse == GizmoFunction::TRANSLATION ? ImVec2((bid + 2) * 16.f / (nrItems*16.f), 1) : ImVec2((bid + 1) * 16.f / (nrItems*16.f), 1)))
                    )
				{
					UseTranslationManipulator();
				}
				ImGui::PopID();
				bid = 4;
				ImGui::SameLine();
				ImGui::PushID(bid);
                if (ImGui::ImageButton(
                    "##img_btn",
                    iconsTex,
                    ImVec2(16, 16),
                    (GizmoInUse == GizmoFunction::ROTATION ? ImVec2((bid + 1) * 16.f / (nrItems*16.f), 0) : ImVec2(bid * 16.f / (nrItems*16.f), 0)),
                    (GizmoInUse == GizmoFunction::ROTATION ? ImVec2((bid + 2) * 16.f / (nrItems*16.f), 1) : ImVec2((bid + 1) * 16.f / (nrItems*16.f), 1)))
                    )
				{
					UseRotationManipulator();
				}
				ImGui::PopID();
				bid = 2;
				ImGui::SameLine();
				ImGui::PushID(bid);
                if (ImGui::ImageButton(
                    "##img_btn",
                    iconsTex,
                    ImVec2(16, 16),
                    (GizmoInUse == GizmoFunction::SCALE ? ImVec2((bid + 1) * 16.f / (nrItems*16.f), 0) : ImVec2(bid * 16.f / (nrItems*16.f), 0)),
                    (GizmoInUse == GizmoFunction::SCALE ? ImVec2((bid + 2) * 16.f / (nrItems*16.f), 1) : ImVec2((bid + 1) * 16.f / (nrItems*16.f), 1)))
                    )
				{
					UseScaleManipulator();
				}
				ImGui::PopID();
				bid = 6;
				ImGui::SameLine();
				ImGui::PushID(bid);
                if (ImGui::ImageButton(
                    "##img_btn",
                    iconsTex,
                    ImVec2(16, 16),
                    (localTransform || GizmoInUse == GizmoFunction::SCALE ? ImVec2((bid + 1) * 16.f / (nrItems*16.f), 0) : ImVec2(bid * 16.f / (nrItems*16.f), 0)),
                    (localTransform || GizmoInUse == GizmoFunction::SCALE ? ImVec2((bid + 2) * 16.f / (nrItems*16.f), 1) : ImVec2((bid + 1) * 16.f / (nrItems*16.f), 1)))
                    )
				{
					if (localTransform) UseGlobalManipulator();
					else UseLocalManipulator();
				}
				ImGui::PopID();
				bid = 7;
				ImGui::SameLine();
				ImGui::PushID(bid);
                if (ImGui::ImageButton(
                    "##img_btn",
                    iconsTex,
                    ImVec2(16, 16),
                    ImVec2((bid + 1) * 16.f / (nrItems*16.f), 0),ImVec2((bid + 2) * 16.f / (nrItems*16.f), 1))
                    )
				{
					DeleteSelected();
					node_clicked = -1;
				}
				ImGui::PopID();
			}
			break;
			default:

				break;
			}
		}
	}

	//=========================================================================
	// Agent API implementation (driven by AgentServer on the main thread)
	//=========================================================================

	namespace {

		SceneObject* AgentFindGameObjectByName(SceneObjects* so, const std::string& name)
		{
			const std::map<uint32, SceneObject*>& list = so->GetList();
			for (std::map<uint32, SceneObject*>::const_iterator i = list.begin(); i != list.end(); i++)
			{
				if (i->second && i->second->GetType() == SceneObjectTypes::GAMEOBJECT
					&& i->second->GetName() == name)
					return i->second;
			}
			return NULL;
		}

		void AgentApplyTransform(GameObject* go, const std::vector<f32>& p,
			const std::vector<f32>& r, const std::vector<f32>& s)
		{
			if (!go) return;
			if (p.size() == 3) go->SetPosition(Vec3(p[0], p[1], p[2]));
			if (r.size() == 3) go->SetRotation(Vec3(r[0], r[1], r[2]));
			if (s.size() == 3) go->SetScale(Vec3(s[0], s[1], s[2]));
		}

		Vec3 AgentQuatToEuler(const std::vector<f32>& q)
		{
			const f32 x = q[0], y = q[1], z = q[2], w = q[3];
			f32 pitch = atan2f(2.f * (w * x + y * z), 1.f - 2.f * (x * x + y * y));
			f32 yaw = asinf(std::max(f32(-1.f), std::min(f32(1.f), 2.f * (w * y - z * x))));
			f32 roll = atan2f(2.f * (w * z + x * y), 1.f - 2.f * (y * y + z * z));
			return Vec3(pitch, yaw, roll);
		}

		std::string AgentPrimitiveTypeName(uint32 t)
		{
			switch (t)
			{
			case PrimitiveType::Cube: return "Cube";
			case PrimitiveType::Sphere: return "Sphere";
			case PrimitiveType::Cone: return "Cone";
			case PrimitiveType::Cylinder: return "Cylinder";
			case PrimitiveType::Plane: return "Plane";
			case PrimitiveType::Capsule: return "Capsule";
			case PrimitiveType::Torus: return "Torus";
			case PrimitiveType::TorusKnot: return "TorusKnot";
			default: return "Custom";
			}
		}

		json AgentRenderableToJson(Renderable* r)
		{
			if (!r) return json();
			if (Decal* d = dynamic_cast<Decal*>(r)) { json j; j["kind"] = "decal"; return j; }
			if (Text* t = dynamic_cast<Text*>(r)) { json j; j["kind"] = "text"; return j; }
			if (Model* m = dynamic_cast<Model*>(r))
			{
				json j;
				j["kind"] = "model";
				if (!m->GetPath().empty()) j["path"] = m->GetPath();
				return j;
			}
			if (Primitive* p = dynamic_cast<Primitive*>(r))
			{
				json j;
				j["kind"] = "primitive";
				j["shape"] = AgentPrimitiveTypeName(p->GetPrimitiveType());
				return j;
			}
			return json();
		}

		// `project` only to report asset paths project-relative; NULL is
		// fine and falls back to whatever the asset itself remembers.
		json AgentComponentToJson(IComponent* c, const ProjectManager* project)
		{
			if (!c) return json();
			json j;
			// Screen-space UI. Reported before the RenderingComponent test
			// for the same reason ParticleSystem is: UIImage and UIText are
			// RenderingComponents, and would otherwise come back as
			// anonymous meshes - which is what an agent asking "what is on
			// this canvas" was being told until now.
			switch (c->GetComponentType())
			{
			case ComponentType::UICanvas:
			{
				UICanvas* cv = static_cast<UICanvas*>(c);
				j["type"] = "UICanvas";
				j["referenceWidth"] = (double)cv->GetReferenceResolution().x;
				j["referenceHeight"] = (double)cv->GetReferenceResolution().y;
				j["sortOrder"] = cv->GetSortOrder();
				return j;
			}
			case ComponentType::UIRect:
			{
				UIRect* r = static_cast<UIRect*>(c);
				j["type"] = "UIRect";
				j["anchorMin"] = { (double)r->GetAnchorMin().x, (double)r->GetAnchorMin().y };
				j["anchorMax"] = { (double)r->GetAnchorMax().x, (double)r->GetAnchorMax().y };
				j["offsetMin"] = { (double)r->GetOffsetMin().x, (double)r->GetOffsetMin().y };
				j["offsetMax"] = { (double)r->GetOffsetMax().x, (double)r->GetOffsetMax().y };
				j["pivot"] = { (double)r->GetPivot().x, (double)r->GetPivot().y };
				if (!r->IsVisible()) j["visible"] = false;
				if (r->IsClipChildren()) j["clip"] = true;
				if (!r->GetStyleRef().empty()) j["styleRef"] = r->GetStyleRef();
				return j;
			}
			case ComponentType::UIImage:
			{
				UIImage* img = static_cast<UIImage*>(c);
				j["type"] = "UIImage";
				j["tint"] = { (double)img->GetTint().x, (double)img->GetTint().y,
					(double)img->GetTint().z, (double)img->GetTint().w };
				j["border"] = { (double)img->GetBorder().x, (double)img->GetBorder().y,
					(double)img->GetBorder().z, (double)img->GetBorder().w };
				j["texture"] = (img->GetTexture() && !img->GetTexture()->GetFilename().empty())
					? (project ? project->DisplayPath(img->GetTexture()->GetFilename())
						: img->GetTexture()->GetFilename())
					: std::string();
				return j;
			}
			case ComponentType::UIText:
			{
				UIText* t = static_cast<UIText*>(c);
				j["type"] = "UIText";
				j["text"] = t->GetText();
				j["size"] = (double)t->GetSize();
				j["color"] = { (double)t->GetColor().x, (double)t->GetColor().y,
					(double)t->GetColor().z, (double)t->GetColor().w };
				j["wrap"] = t->IsWordWrap();
				j["sdf"] = t->IsFontSDF();
				return j;
			}
			case ComponentType::UIButton:
			case ComponentType::UIToggle:
			case ComponentType::UIMenuItem:
			{
				UIButton* b = static_cast<UIButton*>(c);
				const uint32 kind = c->GetComponentType();
				j["type"] = kind == ComponentType::UIToggle ? "UIToggle"
					: kind == ComponentType::UIMenuItem ? "UIMenuItem" : "UIButton";
				j["interactable"] = b->IsInteractable();
				j["onClick"] = b->GetOnClick();
				if (kind == ComponentType::UIToggle)
				{
					UIToggle* t = static_cast<UIToggle*>(c);
					j["value"] = t->GetValue();
					j["group"] = t->GetGroup();
					j["check"] = t->GetCheckElement();
				}
				else if (kind == ComponentType::UIMenuItem)
				{
					UIMenuItem* m = static_cast<UIMenuItem*>(c);
					j["submenu"] = m->GetSubmenu();
					j["open"] = m->IsOpen();
				}
				return j;
			}
			case ComponentType::UIPopup:
			{
				UIPopup* p = static_cast<UIPopup*>(c);
				j["type"] = "UIPopup";
				j["open"] = p->IsOpen();
				j["modal"] = p->IsModalPopup();
				j["closeOnEscape"] = p->ClosesOnEscape();
				j["closeOnOutside"] = p->ClosesOnOutside();
				j["dialogElement"] = p->GetDialogElement();
				return j;
			}
			case ComponentType::UIMenu:
			{
				j["type"] = "UIMenu";
				j["active"] = static_cast<UIMenu*>(c)->IsActive();
				return j;
			}
			case ComponentType::UISlider:
			{
				UISlider* s = static_cast<UISlider*>(c);
				j["type"] = "UISlider";
				j["value"] = (double)s->GetValue();
				j["min"] = (double)s->GetMin();
				j["max"] = (double)s->GetMax();
				j["step"] = (double)s->GetStep();
				j["vertical"] = s->IsVertical();
				j["interactable"] = s->IsInteractable();
				j["onChange"] = s->GetOnChange();
				return j;
			}
			case ComponentType::UIInput:
			{
				UIInput* in = static_cast<UIInput*>(c);
				j["type"] = "UIInput";
				j["text"] = in->GetText();
				j["placeholder"] = in->GetPlaceholder();
				j["maxLength"] = in->GetMaxLength();
				j["password"] = in->IsPassword();
				j["readOnly"] = in->IsReadOnly();
				j["filter"] = in->GetFilter();
				j["interactable"] = in->IsInteractable();
				j["onChange"] = in->GetOnChange();
				j["onSubmit"] = in->GetOnSubmit();
				return j;
			}
			case ComponentType::UIList:
			{
				UIList* l = static_cast<UIList*>(c);
				j["type"] = "UIList";
				json items = json::array();
				for (size_t i = 0; i < l->GetItems().size(); i++) items.push_back(l->GetItems()[i]);
				j["items"] = std::move(items);
				j["selected"] = l->GetSelected();
				j["itemHeight"] = (double)l->GetItemHeight();
				j["interactable"] = l->IsInteractable();
				j["onChange"] = l->GetOnChange();
				j["onSubmit"] = l->GetOnSubmit();
				return j;
			}
			case ComponentType::UIDropdown:
			{
				UIDropdown* d = static_cast<UIDropdown*>(c);
				j["type"] = "UIDropdown";
				json options = json::array();
				for (size_t i = 0; i < d->GetOptions().size(); i++) options.push_back(d->GetOptions()[i]);
				j["options"] = std::move(options);
				j["selected"] = d->GetSelected();
				j["placeholder"] = d->GetPlaceholder();
				j["expanded"] = d->IsExpanded();
				j["interactable"] = d->IsInteractable();
				j["onChange"] = d->GetOnChange();
				return j;
			}
			default: break;
			}

			// Before the RenderingComponent test - a ParticleSystem is one
			// (via IRenderingInstancedComponent), and would otherwise be
			// reported as a mesh with no renderable.
			if (ParticleSystem* ps = dynamic_cast<ParticleSystem*>(c))
			{
				const ParticleSystemDesc& d = ps->GetDesc();
				j["type"] = "ParticleSystem";
				j["maxParticles"] = d.maxParticles;
				j["liveParticles"] = ps->GetLiveParticleCount();
				j["playing"] = ps->IsPlaying();
				j["looping"] = d.looping;
				j["emissionRate"] = (double)d.emissionRate;
				j["burstCount"] = d.burstCount;
				j["blendMode"] = d.blendMode;
				// The sprite is the one part of an emitter that silently
				// produces "nothing visible at all" when it goes missing
				// (an unbound sampler under additive blending draws black
				// on black), so it is worth reporting explicitly.
				// Project-relative, matching every other path this API
				// reports - see the inspector's seeding above for why the
				// Texture's own filename is absolute.
				j["texture"] = d.texture
					? (project ? project->DisplayPath(d.texture->GetFilename())
						: d.texture->GetFilename())
					: std::string();
				j["textureWidth"] = d.texture ? d.texture->GetWidth() : 0;
				j["instances"] = ps->NumberOfInstances();
				j["meshes"] = (uint32)ps->GetMeshes(0).size();
				if (ps->GetOwner())
				{
					const Vec3 wp = ps->GetOwner()->GetWorldPosition();
					j["ownerWorldPos"] = { (double)wp.x, (double)wp.y, (double)wp.z };
				}
				return j;
			}
			if (RenderingComponent* rc = dynamic_cast<RenderingComponent*>(c))
			{
				j["type"] = "RenderingComponent";
				j["cullTest"] = rc->IsCullTesting();
				j["castingShadows"] = rc->IsCastingShadows();
				if (!rc->GetMeshes(0).empty() && rc->GetMeshes(0)[0]->Material)
				{
					IMaterial* mat = rc->GetMeshes(0)[0]->Material.get();
					j["opacity"] = (double)mat->GetOpacity();
					if (GenericShaderMaterial* gm = dynamic_cast<GenericShaderMaterial*>(mat))
					{
						const Vec4 col = gm->GetColor();
						j["materialColor"] = { (double)col.x, (double)col.y, (double)col.z, (double)col.w };
					}
				}
				json rj = AgentRenderableToJson(rc->GetRenderable());
				if (rj.is_object()) j["renderable"] = rj;
				return j;
			}
			if (DirectionalLight* l = dynamic_cast<DirectionalLight*>(c))
			{
				j["type"] = "DirectionalLight";
				const Vec4 col = l->GetLightColor();
				j["color"] = { (double)col.x, (double)col.y, (double)col.z, (double)col.w };
				const Vec3 dir = l->GetLightDirection();
				j["direction"] = { (double)dir.x, (double)dir.y, (double)dir.z };
				j["intensity"] = (double)l->GetLightIntensity();
				return j;
			}
			if (SpotLight* l = dynamic_cast<SpotLight*>(c))
			{
				j["type"] = "SpotLight";
				j["intensity"] = (double)l->GetLightIntensity();
				j["radius"] = (double)l->GetLightRadius();
				j["innerCone"] = (double)l->GetLightInnerCone();
				j["outerCone"] = (double)l->GetLightOutterCone();
				return j;
			}
			if (PointLight* l = dynamic_cast<PointLight*>(c))
			{
				j["type"] = "PointLight";
				j["intensity"] = (double)l->GetLightIntensity();
				j["radius"] = (double)l->GetLightRadius();
				return j;
			}
			if (AudioSource* a = dynamic_cast<AudioSource*>(c))
			{
				j["type"] = "AudioSource";
				j["file"] = a->GetFile();
				j["volume"] = (double)a->GetVolume();
				j["looping"] = a->IsLooping();
				j["spatialized"] = a->IsSpatialized();
				j["playing"] = a->IsPlaying();
				return j;
			}
			if (IPhysicsComponent* p = dynamic_cast<IPhysicsComponent*>(c))
			{
				j["type"] = "Physics";
				j["mass"] = (double)p->GetMass();
				j["ghost"] = p->IsGhost();
				return j;
			}
#ifdef LUA_BINDINGS
			if (LuaComponent* lc = dynamic_cast<LuaComponent*>(c))
			{
				j["type"] = "LuaComponent";
				if (!lc->scriptFile.empty()) j["scriptFile"] = lc->scriptFile;
				return j;
			}
#endif
			return json();
		}

	} // namespace

	bool SceneEditor::AgentAddObject(const std::string& name, const std::string& parentName,
		const std::vector<f32>& position, const std::vector<f32>& rotation,
		const std::vector<f32>& scale, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* parent = NULL;
		if (!parentName.empty())
		{
			parent = AgentFindGameObjectByName(sceneObjects, parentName);
			if (!parent) { errOut = "parent '" + parentName + "' not found"; return false; }
		}
		SceneObject* obj = sceneObjects->CreateGameObject(name.empty() ? "GameObject" : name);
		if (!obj) { errOut = "failed to create game object"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();
		AgentApplyTransform(go, position, rotation, scale);
		if (parent)
			sceneObjects->ReparentGameObject(obj->GetID(), parent->GetID());
		MarkSceneDirty();
		PushAddCommand(obj);
		return true;
	}

	bool SceneEditor::AgentAddUI(const std::string& objectName, const std::string& kind,
		const std::string& fontPath, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, objectName);
		if (!obj) { errOut = "object '" + objectName + "' not found"; return false; }
		return OpAddUIComponent(obj->GetID(), kind, fontPath, errOut);
	}

	void SceneEditor::AgentSetViewport2D(f32 x, f32 y, f32 orthoHalfWidth)
	{
		if (orthoHalfWidth > 0.f) zoomOrtho = orthoHalfWidth;
		LookAtPlaneXY(x, y);
	}

	void SceneEditor::LookAtPlaneXY(const f32 x, const f32 y)
	{
		// Look through the EDITOR camera, not whatever scene camera happens to
		// be active. Every 2D scene ships one (Cam2D, tagged
		// PyrosEditor.Camera) and the viewport prefers it, so moving the
		// editor camera while that was active reported success and changed
		// nothing on screen - set_viewport_2d looked like it worked and did
		// not, which is exactly the sort of silent no-op that invalidates a
		// visual check. Detaching rather than moving the scene camera keeps
		// this a view operation instead of an edit to authored data.
		activeSceneCameraId = 0;
		isPerspective = false;
		// Through the orbit state, not by writing the pivot's position and
		// rotation: the pivot's transform is recomposed from rotX/rotY/pos
		// every time the view is orbited or panned (see ShowViewport), so a
		// SetPosition() here was overwritten by the next mouse move and the
		// camera never actually went anywhere. That is why set_viewport_2d
		// used to do nothing at all.
		qX = qY = Quaternion();
		rotX = rotY = Quaternion();
		rotation = Quaternion();
		pos = Vec3(x, y, 0.f);
		if (CameraPivot)
		{
			Matrix m;
			m.Translate(pos);
			CameraPivot->SetTransformationMatrix(m);
		}
		if (Camera)
		{
			// Straight down -Z, no pitch: a 2D scene lies in the XY plane and
			// any tilt at all shows it edge-on.
			Camera->SetPosition(Vec3(0.f, 0.f, 20.f));
			Camera->SetRotation(Vec3(0.f, 0.f, 0.f));
			Camera->RefreshTransformation();
		}
	}

	bool SceneEditor::AgentAddBone2D(const std::string& objName, const std::string& boneName,
		const std::string& parentBone, const Vec2 &localPos, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, objName);
		if (!obj) { errOut = "object '" + objName + "' not found"; return false; }
		return OpAddBone2D(obj->GetID(), boneName, parentBone, localPos, errOut);
	}

	// Bone list with both the local rest transform and the composed global
	// one, so a caller can tell whether posing a parent actually moved its
	// children - which is the whole point of a hierarchy.
	json SceneEditor::AgentSkeletonState(const std::string& objName, std::string& errOut)
	{
		json out;
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, objName);
		if (!obj) { errOut = "object '" + objName + "' not found"; return out; }
		GameObject* go = (GameObject*)obj->GetPTR();
		RenderingComponent* rc = FindRenderingComponent(go);
		if (!rc) { errOut = "object has no RenderingComponent"; return out; }

		SkeletonAnimationInstance* inst =
			static_cast<SkeletonAnimationInstance*>(rc->GetActiveSkeletonAnimation());
		if (!inst) inst = RebuildSkeletonInstance(rc);

		out["object"] = objName;
		out["bones"] = json::array();
		if (!inst) { out["boneCount"] = 0; return out; }

		const std::vector<Bone> &bones = inst->GetSkeletonBones();
		out["boneCount"] = (int)bones.size();
		for (size_t i = 0; i < bones.size(); i++)
		{
			const int32 id = bones[i].self;
			if (id < 0 || (size_t)id >= bones.size()) continue;
			const Vec3 lp = inst->GetBoneLocalTransform(id).GetTranslation();
			const Vec3 gp = inst->GetBoneGlobalTransform(id).GetTranslation();
			json bj;
			bj["name"] = bones[i].name;
			bj["id"] = id;
			bj["parent"] = bones[i].parent;
			bj["local"] = { (double)lp.x, (double)lp.y };
			bj["global"] = { (double)gp.x, (double)gp.y };
			out["bones"].push_back(bj);
		}
		return out;
	}

	bool SceneEditor::AgentPoseBone2D(const std::string& objName, const std::string& boneName,
		const f32 degreesZ, std::string& errOut)
	{
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, objName);
		if (!obj) { errOut = "object '" + objName + "' not found"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();
		RenderingComponent* rc = FindRenderingComponent(go);
		if (!rc) { errOut = "object has no RenderingComponent"; return false; }

		SkeletonAnimationInstance* inst =
			static_cast<SkeletonAnimationInstance*>(rc->GetActiveSkeletonAnimation());
		if (!inst) inst = RebuildSkeletonInstance(rc);
		if (!inst) { errOut = "object has no skeleton"; return false; }

		const std::vector<Bone> &bones = inst->GetSkeletonBones();
		int32 id = -1;
		for (size_t i = 0; i < bones.size(); i++)
			if (bones[i].name == boneName) { id = bones[i].self; break; }
		if (id < 0) { errOut = "bone '" + boneName + "' not found"; return false; }

		// Rebuilt from the bind local so a pose is absolute rather than
		// accumulating on every call: keep the rest translation, replace the
		// rotation.
		Matrix local;
		local.RotationZ(DEGTORAD(degreesZ));
		local.Translate(inst->GetBindPoseLocal(id).GetTranslation());
		inst->SetBoneLocalTransform(id, local);
		inst->RefreshSkinning();
		MarkSceneDirty();
		return true;
	}

	bool SceneEditor::AgentSetSpritePivot(const std::string& name, const Vec2 &norm, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		const std::string before = SnapshotSubtree(obj->GetID());
		if (!OpSetSpritePivot(obj->GetID(), norm, errOut)) return false;
		PushReplaceCommand(obj->GetID(), before, "Set Pivot");
		return true;
	}

	bool SceneEditor::AgentSliceSpritesheet(const std::string& name, const std::string& sheetPath,
		int cols, int rows, f32 fps, bool loop, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		return OpSliceSpritesheet(obj->GetID(), sheetPath, cols, rows, fps, loop, errOut);
	}

	bool SceneEditor::AgentAddLayer2D(const std::string& name, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		return OpAddLayer2D(obj->GetID(), errOut);
	}

	bool SceneEditor::AgentAddPhysics2D(const std::string& name, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		return OpAddPhysics2D(obj->GetID(), errOut);
	}

	bool SceneEditor::AgentAddOccluder2D(const std::string& name, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		return OpAddOccluder2D(obj->GetID(), errOut);
	}

	bool SceneEditor::AgentMakeSprite2DLit(const std::string& name, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		return OpMakeSprite2DLit(obj->GetID(), errOut);
	}

	bool SceneEditor::AgentAddSprite(const std::string& name, const std::string& texturePath,
		const std::string& parentName, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* parent = NULL;
		if (!parentName.empty())
		{
			parent = AgentFindGameObjectByName(sceneObjects, parentName);
			if (!parent) { errOut = "parent '" + parentName + "' not found"; return false; }
		}
		SceneObject* obj = sceneObjects->CreateGameObject(name.empty() ? "Sprite" : name);
		if (!obj) { errOut = "failed to create game object"; return false; }
		if (!OpAddSprite(obj->GetID(), texturePath, errOut)) return false;
		if (parent) sceneObjects->ReparentGameObject(obj->GetID(), parent->GetID());
		PushAddCommand(obj);
		return true;
	}

	bool SceneEditor::AgentAddPrimitive(const std::string& name, const std::string& shape, const json& p,
		const std::string& parentName, const json& color, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* parent = NULL;
		if (!parentName.empty())
		{
			parent = AgentFindGameObjectByName(sceneObjects, parentName);
			if (!parent) { errOut = "parent '" + parentName + "' not found"; return false; }
		}

		SceneObject* obj = sceneObjects->CreateGameObject(name.empty() ? shape : name);
		if (!obj) { errOut = "failed to create game object"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();

		auto F = [&p](const char* key, f32 dflt) -> f32 { return p.is_object() ? (f32)p.value(key, (double)dflt) : dflt; };
		auto B = [&p](const char* key, bool dflt) -> bool { return p.is_object() ? p.value(key, dflt) : dflt; };

		bool created = false;
		if (shape == "Cube")
			created = sceneObjects->CreateRenderingCube(go, F("width", 1), F("height", 1), F("depth", 1), false, false) != NULL;
		else if (shape == "Sphere")
			created = sceneObjects->CreateRenderingSphere(go, F("radius", 1), (uint32)F("segmentsW", 8), (uint32)F("segmentsH", 6), false, B("halfSphere", false), false) != NULL;
		else if (shape == "Cone")
			created = sceneObjects->CreateRenderingCone(go, F("radius", 1), F("height", 1), (uint32)F("segmentsW", 8), (uint32)F("segmentsH", 6), B("openEnded", false), false, false) != NULL;
		else if (shape == "Cylinder")
			created = sceneObjects->CreateRenderingCylinder(go, F("radius", 1), F("height", 1), (uint32)F("segmentsW", 8), (uint32)F("segmentsH", 6), B("openEnded", false), false, false) != NULL;
		else if (shape == "Plane")
			created = sceneObjects->CreateRenderingPlane(go, F("width", 1), F("height", 1), false, false) != NULL;
		else if (shape == "Capsule")
			created = sceneObjects->CreateRenderingCapsule(go, F("radius", 1), F("height", 1), (uint32)F("numRings", 8), (uint32)F("segmentsW", 8), (uint32)F("segmentsH", 6), false, false) != NULL;
		else if (shape == "Torus")
			created = sceneObjects->CreateRenderingTorus(go, F("radius", 1), F("tube", 0.5), (uint32)F("segmentsW", 8), (uint32)F("segmentsH", 6), false, false) != NULL;
		else if (shape == "TorusKnot")
			created = sceneObjects->CreateRenderingTorusKnot(go, F("radius", 1), F("tube", 0.5), (uint32)F("segmentsW", 8), (uint32)F("segmentsH", 6), (uint32)F("p", 1), (uint32)F("q", 1), false, false) != NULL;
		else
			{ errOut = "unknown primitive shape '" + shape + "'"; sceneObjects->DestroySceneObject(obj->GetID()); return false; }

		if (!created)
		{
			errOut = "failed to create " + shape;
			sceneObjects->DestroySceneObject(obj->GetID());
			return false;
		}

		if (color.is_array() && color.size() >= 3)
		{
			RenderingComponent* rc = NULL;
			for (auto& c : go->GetComponents())
				if ((rc = dynamic_cast<RenderingComponent*>(c.get()))) break;
			if (rc && !rc->GetMeshes(0).empty() && rc->GetMeshes(0)[0]->Material)
			{
				if (GenericShaderMaterial* gm = dynamic_cast<GenericShaderMaterial*>(rc->GetMeshes(0)[0]->Material.get()))
				{
					const f32 r = (f32)color[0].get<double>();
					const f32 g = (f32)color[1].get<double>();
					const f32 b = (f32)color[2].get<double>();
					const f32 a = color.size() > 3 ? (f32)color[3].get<double>() : 1.f;
					gm->SetColor(Vec4(r, g, b, a));
				}
			}
		}

		AgentApplyTransform(go,
			p.is_object() && p.contains("position") && p["position"].is_array() ? std::vector<f32>{ (f32)p["position"][0].get<double>(), (f32)p["position"][1].get<double>(), (f32)p["position"][2].get<double>() } : std::vector<f32>(),
			p.is_object() && p.contains("rotation") && p["rotation"].is_array() ? std::vector<f32>{ (f32)p["rotation"][0].get<double>(), (f32)p["rotation"][1].get<double>(), (f32)p["rotation"][2].get<double>() } : std::vector<f32>(),
			p.is_object() && p.contains("scale") && p["scale"].is_array() ? std::vector<f32>{ (f32)p["scale"][0].get<double>(), (f32)p["scale"][1].get<double>(), (f32)p["scale"][2].get<double>() } : std::vector<f32>());

		if (parent)
			sceneObjects->ReparentGameObject(obj->GetID(), parent->GetID());
		MarkSceneDirty();
		PushAddCommand(obj);
		return true;
	}

	bool SceneEditor::AgentAddLight(const std::string& name, const std::string& type, const json& p,
		const std::string& parentName, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* parent = NULL;
		if (!parentName.empty())
		{
			parent = AgentFindGameObjectByName(sceneObjects, parentName);
			if (!parent) { errOut = "parent '" + parentName + "' not found"; return false; }
		}

		SceneObject* obj = sceneObjects->CreateGameObject(name.empty() ? type : name);
		if (!obj) { errOut = "failed to create game object"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();

		auto F = [&p](const char* key, f32 dflt) -> f32 { return p.is_object() ? (f32)p.value(key, (double)dflt) : dflt; };
		Vec4 color(1.f, 1.f, 1.f, 1.f);
		Vec3 direction(0.f, -1.f, 0.f);
		if (p.is_object())
		{
			if (p.contains("color") && p["color"].is_array() && p["color"].size() >= 3)
				color = Vec4((f32)p["color"][0].get<double>(), (f32)p["color"][1].get<double>(), (f32)p["color"][2].get<double>(), p["color"].size() > 3 ? (f32)p["color"][3].get<double>() : 1.f);
			if (p.contains("direction") && p["direction"].is_array() && p["direction"].size() == 3)
				direction = Vec3((f32)p["direction"][0].get<double>(), (f32)p["direction"][1].get<double>(), (f32)p["direction"][2].get<double>());
		}

		SceneObject* lightObj = NULL;
		if (type == "DirectionalLight")
			lightObj = sceneObjects->CreateDirectionalLight(go, direction, color);
		else if (type == "PointLight")
			lightObj = sceneObjects->CreatePointLight(go, F("radius", 10.f), color);
		else if (type == "SpotLight")
			lightObj = sceneObjects->CreateSpotLight(go, F("radius", 10.f), direction, F("outer", 45.f), F("inner", 30.f), color);
		else
			{ errOut = "unknown light type '" + type + "'"; sceneObjects->DestroySceneObject(obj->GetID()); return false; }

		if (!lightObj)
		{
			errOut = "failed to create " + type;
			sceneObjects->DestroySceneObject(obj->GetID());
			return false;
		}

		IComponent* light = (IComponent*)lightObj->GetPTR();
		if (p.is_object() && p.contains("intensity"))
		{
			ILightComponent* il = dynamic_cast<ILightComponent*>(light);
			if (il) il->SetLightIntensity((f32)p["intensity"].get<double>());
		}
		if (p.is_object() && p.contains("radius"))
		{
			if (PointLight* pl = dynamic_cast<PointLight*>(light)) pl->SetLightRadius((f32)p["radius"].get<double>());
			if (SpotLight* sl = dynamic_cast<SpotLight*>(light)) sl->SetLightRadius((f32)p["radius"].get<double>());
		}

		AgentApplyTransform(go,
			p.is_object() && p.contains("position") && p["position"].is_array() ? std::vector<f32>{ (f32)p["position"][0].get<double>(), (f32)p["position"][1].get<double>(), (f32)p["position"][2].get<double>() } : std::vector<f32>(),
			std::vector<f32>(),
			std::vector<f32>());

		if (parent)
			sceneObjects->ReparentGameObject(obj->GetID(), parent->GetID());
		MarkSceneDirty();
		PushAddCommand(obj);
		return true;
	}

	bool SceneEditor::AgentAddAudio(const std::string& name, const std::string& file, const json& p,
		const std::string& parentName, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		if (file.empty()) { errOut = "audio file path is required"; return false; }
		SceneObject* parent = NULL;
		if (!parentName.empty())
		{
			parent = AgentFindGameObjectByName(sceneObjects, parentName);
			if (!parent) { errOut = "parent '" + parentName + "' not found"; return false; }
		}

		std::string resolved = ResolveSoundPath(file);
		std::error_code ec;
		if (!std::filesystem::exists(resolved, ec))
			{ errOut = "sound file not found: " + file; return false; }

		SceneObject* obj = sceneObjects->CreateGameObject(name.empty() ? "Sound" : name);
		if (!obj) { errOut = "failed to create game object"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();

		auto F = [&p](const char* key, f32 dflt) -> f32 { return p.is_object() ? (f32)p.value(key, (double)dflt) : dflt; };
		auto B = [&p](const char* key, bool dflt) -> bool { return p.is_object() ? p.value(key, dflt) : dflt; };

		SceneObject* soundObj = sceneObjects->CreateAudioSource(go, resolved,
			B("stream", false), B("looping", false), B("spatialized", true), F("volume", 1.f));
		if (!soundObj)
		{
			errOut = "failed to create audio source";
			sceneObjects->DestroySceneObject(obj->GetID());
			return false;
		}
		std::shared_ptr<SoundHelper> h = std::make_shared<SoundHelper>(go);
		soundObj->Helper = h;
		scene->Add(h);

		if (AudioSource* a = dynamic_cast<AudioSource*>((IComponent*)soundObj->GetPTR()))
		{
			a->SetPitch(F("pitch", 1.f));
			a->SetPan(F("pan", 0.f));
		}

		if (parent)
			sceneObjects->ReparentGameObject(obj->GetID(), parent->GetID());
		MarkSceneDirty();
		PushAddCommand(obj);
		return true;
	}

	bool SceneEditor::AgentAddParticles(const std::string& name, const json& p,
		const std::string& parentName, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* parent = NULL;
		if (!parentName.empty())
		{
			parent = AgentFindGameObjectByName(sceneObjects, parentName);
			if (!parent) { errOut = "parent '" + parentName + "' not found"; return false; }
		}

		auto F = [&p](const char* key, f32 dflt) -> f32 { return p.is_object() ? (f32)p.value(key, (double)dflt) : dflt; };
		auto B = [&p](const char* key, bool dflt) -> bool { return p.is_object() ? p.value(key, dflt) : dflt; };
		auto U = [&p](const char* key, uint32 dflt) -> uint32 { return p.is_object() ? p.value(key, dflt) : dflt; };
		auto V3 = [&p](const char* key, const Vec3& dflt) -> Vec3 {
			if (!p.is_object() || !p.contains(key) || !p[key].is_array() || p[key].size() < 3) return dflt;
			return Vec3((f32)p[key][0].get<double>(), (f32)p[key][1].get<double>(), (f32)p[key][2].get<double>());
		};
		auto V4 = [&p](const char* key, const Vec4& dflt) -> Vec4 {
			if (!p.is_object() || !p.contains(key) || !p[key].is_array() || p[key].size() < 4) return dflt;
			return Vec4((f32)p[key][0].get<double>(), (f32)p[key][1].get<double>(),
				(f32)p[key][2].get<double>(), (f32)p[key][3].get<double>());
		};

		ParticleSystemDesc desc;
		// Same presets the Add form offers, by name, applied before the
		// individual overrides below so a caller can say "fire, but slower".
		const std::string preset = p.is_object() ? p.value("preset", std::string()) : std::string();
		if (preset == "fire") ApplyParticlePreset(desc, 1);
		else if (preset == "smoke") ApplyParticlePreset(desc, 2);
		else if (preset == "explosion") ApplyParticlePreset(desc, 3);
		else if (!preset.empty() && preset != "default")
			{ errOut = "unknown preset '" + preset + "' (default, fire, smoke, explosion)"; return false; }

		desc.maxParticles = U("maxParticles", desc.maxParticles);
		desc.looping = B("looping", desc.looping);
		desc.emissionRate = F("emissionRate", desc.emissionRate);
		desc.burstCount = U("burstCount", desc.burstCount);
		desc.minLifetime = F("minLifetime", desc.minLifetime);
		desc.maxLifetime = F("maxLifetime", desc.maxLifetime);
		desc.direction = V3("direction", desc.direction);
		desc.spreadAngle = (f32)DEGTORAD(F("spreadAngleDegrees", (f32)RADTODEG(desc.spreadAngle)));
		desc.minSpeed = F("minSpeed", desc.minSpeed);
		desc.maxSpeed = F("maxSpeed", desc.maxSpeed);
		desc.gravity = V3("gravity", desc.gravity);
		desc.damping = F("damping", desc.damping);
		desc.startSize = F("startSize", desc.startSize);
		desc.endSize = F("endSize", desc.endSize);
		desc.sizeRandomJitter = F("sizeRandomJitter", desc.sizeRandomJitter);
		desc.startColor = V4("startColor", desc.startColor);
		desc.endColor = V4("endColor", desc.endColor);
		// Ramp curves, by name rather than by number - "easeIn" is what
		// someone writing this by hand means, and the enum's numbering is an
		// internal detail. Bezier is not offered: a ramp has two ends and no
		// next key to take an incoming tangent from.
		{
			auto easeArg = [&](const char* key, uchar dflt, bool &bad) -> uchar {
				if (!p.is_object() || !p.contains(key)) return dflt;
				const std::string v = p.value(key, std::string());
				for (unsigned m = 0; m + 1 < p3d::kInterpolationModeCount; m++)
				{
					std::string name = InterpolationModeName((uchar)m);
					std::string flat;
					for (size_t i = 0; i < name.size(); i++)
						if (name[i] != ' ' && name[i] != '/') flat += (char)tolower(name[i]);
					std::string want;
					for (size_t i = 0; i < v.size(); i++)
						if (v[i] != ' ' && v[i] != '/') want += (char)tolower(v[i]);
					if (flat == want) return (uchar)m;
				}
				bad = true;
				return dflt;
			};
			bool bad = false;
			desc.sizeEase = easeArg("sizeEase", desc.sizeEase, bad);
			desc.colorEase = easeArg("colorEase", desc.colorEase, bad);
			if (bad)
			{
				errOut = "unknown ease (linear, step, easein, easeout, easeinout)";
				return false;
			}
		}
		desc.fadeInFraction = F("fadeInFraction", desc.fadeInFraction);
		desc.fadeOutFraction = F("fadeOutFraction", desc.fadeOutFraction);
		desc.minRotationSpeed = F("minRotationSpeed", desc.minRotationSpeed);
		desc.maxRotationSpeed = F("maxRotationSpeed", desc.maxRotationSpeed);
		if (p.is_object() && p.contains("blendMode"))
		{
			const std::string blend = p.value("blendMode", std::string("alpha"));
			if (blend == "additive") desc.blendMode = ParticleBlendMode::Additive;
			else if (blend == "alpha") desc.blendMode = ParticleBlendMode::AlphaBlend;
			else { errOut = "unknown blendMode '" + blend + "' (alpha, additive)"; return false; }
		}

		const std::string texture = p.is_object() ? p.value("texture", std::string()) : std::string();
		desc.texture = LoadParticleTexture(ImportParticleTexture(texture));

		SceneObject* obj = sceneObjects->CreateGameObject(name.empty() ? "Particles" : name);
		if (!obj) { errOut = "failed to create game object"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();

		// Placed before the component is attached: particles spawn at the
		// owner's world position from the very first frame, so an emitter
		// moved after the fact would puff out a stray burst at the origin.
		AgentApplyTransform(go,
			p.is_object() && p.contains("position") && p["position"].is_array() ? std::vector<f32>{ (f32)p["position"][0].get<double>(), (f32)p["position"][1].get<double>(), (f32)p["position"][2].get<double>() } : std::vector<f32>(),
			p.is_object() && p.contains("rotation") && p["rotation"].is_array() ? std::vector<f32>{ (f32)p["rotation"][0].get<double>(), (f32)p["rotation"][1].get<double>(), (f32)p["rotation"][2].get<double>() } : std::vector<f32>(),
			std::vector<f32>());

		if (!AttachParticleSystem(go, desc))
		{
			errOut = "failed to create particle system";
			sceneObjects->DestroySceneObject(obj->GetID());
			return false;
		}

		if (parent)
			sceneObjects->ReparentGameObject(obj->GetID(), parent->GetID());
		MarkSceneDirty();
		PushAddCommand(obj);
		return true;
	}

	bool SceneEditor::AgentAddPhysics(const std::string& name, const json& p, const std::string& parentName, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* parent = NULL;
		if (!parentName.empty())
		{
			parent = AgentFindGameObjectByName(sceneObjects, parentName);
			if (!parent) { errOut = "parent '" + parentName + "' not found"; return false; }
		}
		if (!physics) { errOut = "physics engine not available"; return false; }

		const std::string shape = p.is_object() ? p.value("shape", std::string("Box")) : std::string("Box");
		auto F = [&p](const char* key, f32 dflt) -> f32 { return p.is_object() ? (f32)p.value(key, (double)dflt) : dflt; };
		const f32 mass = F("mass", 1.f);
		const bool ghost = p.is_object() ? p.value("ghost", false) : false;

		SceneObject* obj = sceneObjects->CreateGameObject(name.empty() ? ("Physics " + shape) : name);
		if (!obj) { errOut = "failed to create game object"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();

		std::shared_ptr<IPhysicsComponent> pcomp;
		if (shape == "Box")
			pcomp = physics->CreateBox(F("width", 1), F("height", 1), F("depth", 1), mass, ghost);
		else if (shape == "Sphere")
			pcomp = physics->CreateSphere(F("radius", 0.5), mass, ghost);
		else if (shape == "Capsule")
			pcomp = physics->CreateCapsule(F("radius", 0.5), F("height", 1), mass, ghost);
		else if (shape == "Cone")
			pcomp = physics->CreateCone(F("radius", 0.5), F("height", 1), mass, ghost);
		else if (shape == "Cylinder")
			pcomp = physics->CreateCylinder(F("radius", 0.5), F("height", 1), mass, ghost);
		else if (shape == "StaticPlane")
		{
			Vec3 normal(0.f, 1.f, 0.f);
			if (p.is_object() && p.contains("normal") && p["normal"].is_array() && p["normal"].size() == 3)
				normal = Vec3((f32)p["normal"][0].get<double>(), (f32)p["normal"][1].get<double>(), (f32)p["normal"][2].get<double>());
			pcomp = physics->CreateStaticPlane(normal, F("constant", 0.f), 0.f, false);
		}
		else
			{ errOut = "unknown physics shape '" + shape + "'"; sceneObjects->DestroySceneObject(obj->GetID()); return false; }

		if (!pcomp)
		{
			errOut = "failed to create physics shape " + shape;
			sceneObjects->DestroySceneObject(obj->GetID());
			return false;
		}

		go->Add(pcomp);
		uint32 id = ++sceneObjects->_ID;
		SceneObject* compObj = new SceneObject("Physics " + shape, pcomp.get(), id, SceneObjectTypes::PHYSICS_COMPONENT);
		sceneObjects->listObjects[id] = compObj;
		compObj->SetParentID(sceneObjects->GetSceneObjectID(go));

		AgentApplyTransform(go,
			p.is_object() && p.contains("position") && p["position"].is_array() ? std::vector<f32>{ (f32)p["position"][0].get<double>(), (f32)p["position"][1].get<double>(), (f32)p["position"][2].get<double>() } : std::vector<f32>(),
			std::vector<f32>(),
			std::vector<f32>());

		if (parent)
			sceneObjects->ReparentGameObject(obj->GetID(), parent->GetID());
		MarkSceneDirty();
		PushAddCommand(obj);
		return true;
	}

	bool SceneEditor::AgentAddModel(const std::string& name, const std::string& modelFile, const std::string& parentName, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		if (modelFile.empty()) { errOut = "model file path is required"; return false; }
		SceneObject* parent = NULL;
		if (!parentName.empty())
		{
			parent = AgentFindGameObjectByName(sceneObjects, parentName);
			if (!parent) { errOut = "parent '" + parentName + "' not found"; return false; }
		}

		std::string p3dm = modelFile;
		if (project && project->IsOpen() && !ProjectManager::IsP3dm(modelFile))
		{
			std::string outAbs, err, trashedPackageDir;
			if (!project->ImportModel(modelFile, outAbs, &err, &trashedPackageDir))
			{
				errOut = "model import failed: " + err;
				return false;
			}
			if (!trashedPackageDir.empty())
			{
				const std::string importedRel = project->RelativePath(outAbs);
				const std::string packageRel = std::filesystem::path(importedRel).parent_path().string();
				if (!packageRel.empty())
					sceneUndo.Push(std::make_unique<ImportOverwriteCommand>(project, packageRel, trashedPackageDir,
						"Import Model (overwrite) '" + packageRel + "'"));
			}
			p3dm = outAbs;
		}
		std::error_code ec;
		if (!std::filesystem::exists(p3dm, ec))
			{ errOut = "model file not found: " + modelFile; return false; }

		SceneObject* obj = sceneObjects->CreateGameObject(name.empty() ? std::filesystem::path(modelFile).stem().string() : name);
		if (!obj) { errOut = "failed to create game object"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();

		if (!sceneObjects->CreateRenderingModel(go, p3dm))
		{
			errOut = "failed to create model renderable";
			sceneObjects->DestroySceneObject(obj->GetID());
			return false;
		}

		if (parent)
			sceneObjects->ReparentGameObject(obj->GetID(), parent->GetID());
		MarkSceneDirty();
		PushAddCommand(obj);
		return true;
	}

	// Changes an existing scene camera. Only the keys present are touched,
	// so this can flip a camera to orthographic without also having to
	// restate its clip planes.
	// Selection is editor state the agent could not reach at all before -
	// which meant nothing driven through the socket could exercise anything
	// that acts on the selection (the transform gizmo, the Properties panel,
	// the canvas overlay's element outline).
	bool SceneEditor::AgentCanvasDrag(const std::string& objectName, int handle,
		const std::vector<f32>& delta, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		if (handle < 0 || handle > 8) { errOut = "handle must be 0..8 (row*3+column, 4 = the body)"; return false; }
		if (delta.size() != 2) { errOut = "delta must be [x, y] in canvas units"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, objectName);
		if (!obj) { errOut = "object '" + objectName + "' not found"; return false; }
		UIRect* rect = NULL;
		const std::vector<std::shared_ptr<IComponent> >& cs = ((GameObject*)obj->GetPTR())->GetComponents();
		for (size_t i = 0; i < cs.size(); i++)
			if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
			{ rect = static_cast<UIRect*>(cs[i].get()); break; }
		if (!rect) { errOut = "'" + objectName + "' has no UIRect"; return false; }

		const json before = CaptureUIProperties((GameObject*)obj->GetPTR());
		ApplyCanvasDrag(rect, handle, Vec2(delta[0], delta[1]));
		PushUIPropertyUndo(obj->GetID(), before, CaptureUIProperties((GameObject*)obj->GetPTR()), "Move UI Element");
		MarkSceneDirty();
		return true;
	}

	bool SceneEditor::AgentApplyUIStyle(const std::string& objectName, const std::string& stylePath, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, objectName);
		if (!obj) { errOut = "object '" + objectName + "' not found"; return false; }
		return OpApplyUIStyle(obj->GetID(), stylePath, errOut);
	}

	bool SceneEditor::AgentExtractUIStyle(const std::string& objectName, const std::string& name,
		std::string& outPath, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, objectName);
		if (!obj) { errOut = "object '" + objectName + "' not found"; return false; }
		return OpExtractUIStyle(obj->GetID(), name, outPath, errOut);
	}

	bool SceneEditor::AgentRevertUIStyle(const std::string& objectName, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, objectName);
		if (!obj) { errOut = "object '" + objectName + "' not found"; return false; }
		return OpRevertUIStyle(obj->GetID(), errOut);
	}

	bool SceneEditor::AgentClearUIStyle(const std::string& objectName, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, objectName);
		if (!obj) { errOut = "object '" + objectName + "' not found"; return false; }
		return OpClearUIStyle(obj->GetID(), errOut);
	}

	bool SceneEditor::AgentSetUI(const std::string& objectName, const json& p, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, objectName);
		if (!obj) { errOut = "object '" + objectName + "' not found"; return false; }
		return OpSetUIProperties(obj->GetID(), p, errOut);
	}

	bool SceneEditor::AgentSelect(const std::string& name, std::string& errOut)
	{
		if (name.empty()) { DeselectSceneObject(); return true; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		SelectAndFocusSceneObject(obj);
		return true;
	}

	bool SceneEditor::AgentSetCanvasMode(bool on, std::string& errOut)
	{
		// Creates the canvas rather than refusing, so this matches what the
		// View menu's "Canvas (2D) Mode" item now does - the two used to
		// disagree, and an agent could not get into 2D mode at all without
		// knowing to add_object + add_ui Canvas first.
		if (on && GetEditingCanvas() == NULL)
		{
			CreateCanvasForEditing();
			if (GetEditingCanvas() == NULL) { errOut = "could not create a UICanvas"; return false; }
		}
		uiEditMode = on;
		return true;
	}

	bool SceneEditor::AgentSetCamera(const std::string& name, const json& p, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		if (!IsSceneCamera(obj->GetID())) { errOut = "'" + name + "' is not a scene camera"; return false; }
		const uint32 id = obj->GetID();

		if (p.is_object() && p.find("projection") != p.end() && p["projection"].is_string())
		{
			const std::string proj = p["projection"].get<std::string>();
			if (proj == "orthographic" || proj == "ortho") ApplyCameraOrthographic(id, true);
			else if (proj == "perspective") ApplyCameraOrthographic(id, false);
			else { errOut = "projection must be 'perspective' or 'orthographic'"; return false; }
		}
		if (p.is_object() && p.find("fov") != p.end() && p["fov"].is_number())
			ApplyCameraFov(id, p["fov"].get<f32>());
		if (p.is_object() && p.find("size") != p.end() && p["size"].is_number())
			ApplyCameraOrthoSize(id, p["size"].get<f32>());
		if (p.is_object() && p.find("near") != p.end() && p["near"].is_number())
			ApplyCameraNear(id, p["near"].get<f32>());
		if (p.is_object() && p.find("far") != p.end() && p["far"].is_number())
			ApplyCameraFar(id, p["far"].get<f32>());
		if (p.is_object() && p.find("active") != p.end() && p["active"].is_boolean() && p["active"].get<bool>())
			SetActiveSceneCamera(id);

		MarkSceneDirty();
		return true;
	}

	bool SceneEditor::AgentAddCamera(const std::string& name, const std::vector<f32>& position,
		f32 fov, f32 nearPlane, f32 farPlane, bool active, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = sceneObjects->CreateGameObject(name.empty() ? "Camera" : name);
		if (!obj) { errOut = "failed to create camera"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();
		if (position.size() == 3)
			go->SetPosition(Vec3(position[0], position[1], position[2]));

		EditorCameraSettings cs;
		cs.fov = fov;
		cs.nearPlane = nearPlane;
		cs.farPlane = farPlane;
		RegisterSceneCamera(obj->GetID(), cs);
		if (active)
			SetActiveSceneCamera(obj->GetID());
		MarkSceneDirty();
		PushAddCommand(obj);
		return true;
	}

	bool SceneEditor::AgentSetTransform(const std::string& name, const json& t, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();
		Vec3 pos = go->GetPosition(), rot = go->GetRotation(), scale = go->GetScale();
		if (t.is_object())
		{
			if (t.contains("position") && t["position"].is_array() && t["position"].size() == 3)
				pos = Vec3((f32)t["position"][0].get<double>(), (f32)t["position"][1].get<double>(), (f32)t["position"][2].get<double>());
			if (t.contains("rotation") && t["rotation"].is_array())
			{
				const auto& r = t["rotation"];
				if (r.size() == 4)
					rot = AgentQuatToEuler({ (f32)r[0].get<double>(), (f32)r[1].get<double>(), (f32)r[2].get<double>(), (f32)r[3].get<double>() });
				else if (r.size() == 3)
					rot = Vec3((f32)r[0].get<double>(), (f32)r[1].get<double>(), (f32)r[2].get<double>());
			}
			if (t.contains("scale") && t["scale"].is_array() && t["scale"].size() == 3)
				scale = Vec3((f32)t["scale"][0].get<double>(), (f32)t["scale"][1].get<double>(), (f32)t["scale"][2].get<double>());
		}
		return OpSetTransform(obj->GetID(), pos, rot, scale, errOut);
	}

	bool SceneEditor::AgentSetTags(const std::string& name, const json& addTags, const json& removeTags, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();
		if (addTags.is_array())
			for (auto& tag : addTags) if (tag.is_string()) go->AddTag(tag.get<std::string>());
		if (removeTags.is_array())
			for (auto& tag : removeTags) if (tag.is_string()) go->RemoveTag(tag.get<std::string>());
		MarkSceneDirty();
		return true;
	}

	bool SceneEditor::AgentRename(const std::string& name, const std::string& newName, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		if (newName.empty()) { errOut = "new name is empty"; return false; }
		return OpRenameGameObject(obj->GetID(), newName, errOut);
	}

	bool SceneEditor::AgentReparent(const std::string& name, const std::string& newParentName, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* child = AgentFindGameObjectByName(sceneObjects, name);
		if (!child) { errOut = "object '" + name + "' not found"; return false; }
		uint32 parentId = 0;
		if (!newParentName.empty())
		{
			SceneObject* parent = AgentFindGameObjectByName(sceneObjects, newParentName);
			if (!parent) { errOut = "parent '" + newParentName + "' not found"; return false; }
			parentId = parent->GetID();
		}
		return OpReparentGameObject(child->GetID(), parentId, errOut);
	}

	// ---------------------- prefabs over the agent API -------------------------
	//
	// Same chokepoint rule as every other Agent* method: these do no work of
	// their own, they resolve a name and call the Op* the context menu calls, so
	// the MCP bridge, the AI assistant and the menus can never drift apart.

	bool SceneEditor::AgentCreatePrefab(const std::string& name, const std::string& prefabName,
		std::string& outRelPath, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		return OpCreatePrefab(obj->GetID(), prefabName, outRelPath, errOut);
	}

	bool SceneEditor::AgentInstantiatePrefab(const std::string& relPath, const Vec3& position,
		std::string& outName, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		const uint32 id = OpInstantiatePrefab(relPath, position, errOut);
		if (!id) return false;
		SceneObject* obj = sceneObjects->GetSceneObject(id);
		outName = obj ? obj->GetName() : std::string();
		return true;
	}

	bool SceneEditor::AgentApplyPrefab(const std::string& name, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		return OpApplyPrefab(obj->GetID(), errOut);
	}

	bool SceneEditor::AgentRevertPrefab(const std::string& name, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		return OpRevertPrefab(obj->GetID(), errOut);
	}

	bool SceneEditor::AgentUnpackPrefab(const std::string& name, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		return OpUnpackPrefab(obj->GetID(), errOut);
	}

	// Every instance in the open scene, with the state Apply and Revert care
	// about. Reported per-instance rather than per-file because "which of these
	// has local changes" is the question worth asking before touching a prefab.
	nlohmann::json SceneEditor::AgentPrefabState()
	{
		nlohmann::json out = nlohmann::json::array();
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin();
			i != sceneObjects->GetList().end(); ++i)
		{
			SceneObject* o = i->second;
			if (!o || o->GetType() != SceneObjectTypes::GAMEOBJECT || o->prefabSource.empty()) continue;
			GameObject* go = (GameObject*)o->GetPTR();
			if (!go || IsInternalGameObject(go)) continue;
			nlohmann::json e;
			e["name"] = o->GetName();
			e["prefab"] = o->prefabSource;
			e["modified"] = PrefabInstanceIsModified(o->GetID());
			out.push_back(e);
		}
		return out;
	}

	bool SceneEditor::AgentDuplicate(const std::string& name, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		return OpDuplicateGameObject(obj->GetID(), errOut) != 0;
	}

	bool SceneEditor::AgentDeleteObject(const std::string& name, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		return OpDeleteGameObject(obj->GetID(), errOut);
	}

	bool SceneEditor::AgentAttachScript(const std::string& name, const std::string& scriptFile, const json& data, std::string& errOut)
	{
		(void)data; // live attach uses an empty data table; set via script serialize()
#ifdef LUA_BINDINGS
		if (playMode) { errOut = "editor is in play mode"; return false; }
		if (scriptFile.empty()) { errOut = "script path is required"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		std::string resolved = ResolveScriptPath(scriptFile);
		std::error_code ec;
		if (!std::filesystem::exists(resolved, ec))
			{ errOut = "script not found: " + scriptFile; return false; }
		bool ok = AttachLuaScriptToGameObject(obj->GetID(), resolved);
		if (ok)
			MarkSceneDirty();
		else
			errOut = "failed to attach script (Lua host unavailable?)";
		return ok;
#else
		errOut = "editor built without Lua bindings";
		return false;
#endif
	}

	bool SceneEditor::AgentDetachComponent(const std::string& objectName, const std::string& componentType, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, objectName);
		if (!obj) { errOut = "object '" + objectName + "' not found"; return false; }

		uint32 compId = 0;
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); i++)
		{
			SceneObject* c = i->second;
			if (!c || c->GetParentID() != obj->GetID() || c->GetType() == SceneObjectTypes::GAMEOBJECT) continue;
			std::string typeStr;
			switch (c->GetType())
			{
			case SceneObjectTypes::RENDERING_COMPONENT: typeStr = "RenderingComponent"; break;
			case SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT: typeStr = "DirectionalLight"; break;
			case SceneObjectTypes::POINTLIGHT_COMPONENT: typeStr = "PointLight"; break;
			case SceneObjectTypes::SPOTLIGHT_COMPONENT: typeStr = "SpotLight"; break;
			case SceneObjectTypes::PHYSICS_COMPONENT: typeStr = "Physics"; break;
			case SceneObjectTypes::AUDIO_SOURCE_COMPONENT: typeStr = "AudioSource"; break;
			case SceneObjectTypes::PARTICLE_SYSTEM_COMPONENT: typeStr = "ParticleSystem"; break;
#ifdef LUA_BINDINGS
			case SceneObjectTypes::LUA_COMPONENT: typeStr = "LuaComponent"; break;
#endif
			default: break;
			}
			if (typeStr == componentType) { compId = c->GetID(); break; }
		}
		if (compId == 0) { errOut = "no " + componentType + " found on '" + objectName + "'"; return false; }
		DeleteComponentById(compId);
		return true;
	}

	bool SceneEditor::AgentSetMaterial(const std::string& objectName, const json& fields, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, objectName);
		if (!obj) { errOut = "object '" + objectName + "' not found"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();
		RenderingComponent* rc = NULL;
		for (auto& c : go->GetComponents())
			if ((rc = dynamic_cast<RenderingComponent*>(c.get()))) break;
		if (!rc || rc->GetMeshes(0).empty() || !rc->GetMeshes(0)[0]->Material)
			{ errOut = "object has no renderable material"; return false; }
		IMaterial* mat = rc->GetMeshes(0)[0]->Material.get();
		GenericShaderMaterial* gm = dynamic_cast<GenericShaderMaterial*>(mat);

		if (gm)
		{
			if (fields.is_object() && fields.contains("color") && fields["color"].is_array() && fields["color"].size() >= 3)
			{
				const auto& c = fields["color"];
				gm->SetColor(Vec4((f32)c[0].get<double>(), (f32)c[1].get<double>(), (f32)c[2].get<double>(), c.size() > 3 ? (f32)c[3].get<double>() : 1.f));
			}
			if (fields.is_object() && fields.contains("specular") && fields["specular"].is_array() && fields["specular"].size() >= 3)
			{
				const auto& c = fields["specular"];
				gm->SetSpecular(Vec4((f32)c[0].get<double>(), (f32)c[1].get<double>(), (f32)c[2].get<double>(), c.size() > 3 ? (f32)c[3].get<double>() : 1.f));
			}
			if (fields.is_object() && fields.contains("shininess")) gm->SetShininess((f32)fields["shininess"].get<double>());
			if (fields.is_object() && fields.contains("reflectivity")) gm->SetReflectivity((f32)fields["reflectivity"].get<double>());
			if (fields.is_object() && fields.contains("metallic")) gm->SetMetallic((f32)fields["metallic"].get<double>());
			if (fields.is_object() && fields.contains("roughness")) gm->SetRoughness((f32)fields["roughness"].get<double>());
			if (fields.is_object() && fields.contains("alphaCutoff")) gm->SetAlphaCutoff((f32)fields["alphaCutoff"].get<double>());
		}
		if (fields.is_object() && fields.contains("opacity")) mat->SetOpacity((f32)fields["opacity"].get<double>());
		if (fields.is_object() && fields.contains("transparent")) mat->SetTransparencyFlag(fields["transparent"].get<bool>());
		if (fields.is_object() && fields.contains("cullFace")) mat->SetCullFace((uint32)fields["cullFace"].get<int>());
		MarkSceneDirty();
		return true;
	}

	bool SceneEditor::AgentAssignMaterial(const std::string& objectName, int submeshIndex, std::shared_ptr<IMaterial> mat, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, objectName);
		if (!obj) { errOut = "object '" + objectName + "' not found"; return false; }
		return OpAssignMaterial(obj->GetID(), submeshIndex, mat, errOut);
	}

	void SceneEditor::RelativizeAgentAssetPaths(json& j) const
	{
		if (j.is_array())
		{
			for (auto& e : j) RelativizeAgentAssetPaths(e);
			return;
		}
		if (!j.is_object()) return;
		static const char* kPathKeys[] = { "path", "texture", "file", "scriptFile", "shaderFile" };
		for (const char* key : kPathKeys)
		{
			auto it = j.find(key);
			if (it != j.end() && it->is_string())
			{
				const std::string v = it->get<std::string>();
				if (!v.empty()) *it = DisplayPath(v);
			}
		}
		for (auto it = j.begin(); it != j.end(); ++it)
			if (it->is_object() || it->is_array())
				RelativizeAgentAssetPaths(*it);
	}

	bool SceneEditor::AgentGetObject(const std::string& name, json& outObject, std::string& errOut)
	{
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();
		if (!go) { errOut = "object '" + name + "' has no game object"; return false; }

		json j;
		j["name"] = go->GetName();
		const Vec3& p = go->GetPosition();
		const Vec3& r = go->GetRotation();
		const Vec3& s = go->GetScale();
		j["position"] = { (double)p.x, (double)p.y, (double)p.z };
		j["rotation"] = { (double)r.x, (double)r.y, (double)r.z };
		j["scale"] = { (double)s.x, (double)s.y, (double)s.z };
		const Vec3 wp = go->GetWorldPosition();
		j["worldPosition"] = { (double)wp.x, (double)wp.y, (double)wp.z };
		json tags = json::array();
		for (auto& t : go->GetTags()) tags.push_back(t.second);
		j["tags"] = std::move(tags);
		json comps = json::array();
		for (auto& c : go->GetComponents())
		{
			if (!c) continue;
			json cj = AgentComponentToJson(c.get(), project);
			if (cj.is_null()) continue;
			RelativizeAgentAssetPaths(cj);
			comps.push_back(std::move(cj));
		}
		j["components"] = std::move(comps);
		json kids = json::array();
		for (auto& ch : go->GetChildren())
			if (ch) kids.push_back(ch->GetName());
		j["children"] = std::move(kids);
		outObject = std::move(j);
		return true;
	}

	bool SceneEditor::AgentSelectObject(const std::string& name, std::string& errOut)
	{
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		DeselectMesh();
		SelectSceneObject(obj);
		node_clicked = obj->GetID();
		if (obj->GetParentID() != 0)
			hierarchyForceOpenId = obj->GetParentID();
		return true;
	}

	bool SceneEditor::AgentViewportToScreen(const f32 vx, const f32 vy, f32 &sx, f32 &sy) const
	{
		if (dim.x < 1.f || dim.y < 1.f || viewportImgSize.x < 1.f || viewportImgSize.y < 1.f)
			return false;
		// The inverse of UpdateViewportMouse(): it scales screen deltas into
		// render-target pixels, so this scales back and re-adds the origin.
		sx = viewportImgMin.x + vx * (viewportImgSize.x / dim.x);
		sy = viewportImgMin.y + vy * (viewportImgSize.y / dim.y);
		return true;
	}

	json SceneEditor::AgentSceneState()
	{
		json out;
		out["name"] = GetSceneDisplayName();
		out["scenePath"] = scenePath;
		out["dirty"] = sceneDirty;
		out["playing"] = playMode;

		// Recursive: GetAllGameObjectList() holds only what was added to the
		// scene, and a child attached with GameObject::Add() is not in it.
		// The tree assembled below therefore reported every layer as empty -
		// an agent or the MCP server looking at a 2D scene saw two layer
		// roots and none of the objects inside them.
		std::vector<GameObject*> all;
		scene->CollectGameObjectsRecursive(all);

		// Collect user objects (skip editor furniture).
		std::vector<GameObject*> order;
		std::map<GameObject*, size_t> idx;
		for (auto& go : all)
		{
			if (!go || IsInternalGameObject(go)) continue;
			idx[go] = order.size();
			order.push_back(go);
		}

		std::vector<json> nodeJson(order.size());
		for (size_t i = 0; i < order.size(); i++)
		{
			GameObject* go = order[i];
			json j;
			j["name"] = go->GetName();
			const Vec3& p = go->GetPosition();
			const Vec3& r = go->GetRotation();
			const Vec3& s = go->GetScale();
			j["position"] = { (double)p.x, (double)p.y, (double)p.z };
			j["rotation"] = { (double)r.x, (double)r.y, (double)r.z };
			j["scale"] = { (double)s.x, (double)s.y, (double)s.z };
			json tags = json::array();
			for (auto& t : go->GetTags()) tags.push_back(t.second);
			j["tags"] = std::move(tags);
			json comps = json::array();
			for (auto& c : go->GetComponents())
			{
				if (!c) continue;
				json cj = AgentComponentToJson(c.get(), project);
				if (!cj.is_object()) continue;
				RelativizeAgentAssetPaths(cj);
				comps.push_back(std::move(cj));
			}
			j["components"] = std::move(comps);
			nodeJson[i] = std::move(j);
		}

		// Recursively assemble the tree (cycle-guarded).
		std::map<GameObject*, bool> visiting;
		std::function<json(GameObject*)> build;
		build = [&](GameObject* go) -> json
		{
			auto it = idx.find(go);
			if (it == idx.end() || visiting.count(go))
				return json();
			visiting[go] = true;
			json& base = nodeJson[it->second];
			json arr = json::array();
			for (auto& child : go->GetChildren())
			{
				json cj = build(child.get());
				if (cj.is_object()) arr.push_back(std::move(cj));
			}
			base["children"] = std::move(arr);
			visiting.erase(go);
			return std::move(base);
		};

		json roots = json::array();
		for (auto* go : order)
		{
			GameObject* parent = go->GetParent();
			if (parent == NULL || IsInternalGameObject(parent))
			{
				json cj = build(go);
				if (cj.is_object()) roots.push_back(std::move(cj));
			}
		}
		out["objects"] = std::move(roots);
		return out;
	}

	bool SceneEditor::AgentSave(std::string& errOut)
	{
		if (scenePath.empty())
		{
			errOut = "scene has no path yet — use saveAs with a path";
			return false;
		}
		if (SaveSceneToFile(scenePath))
			return true;
		errOut = "save failed (a Save As dialog may have opened)";
		return false;
	}

	bool SceneEditor::AgentSaveAs(const std::string& path, std::string& errOut)
	{
		if (path.empty()) { errOut = "path is required"; return false; }
		if (!SaveSceneToFile(path))
		{
			errOut = "save failed";
			return false;
		}
		if (project && project->IsOpen())
		{
			std::string rel = project->RelativePath(path);
			if (!rel.empty())
			{
				project->SetActiveSceneRel(rel);
				project->Save();
			}
		}
		return true;
	}

	bool SceneEditor::AgentLoadScene(const std::string& path, std::string& errOut)
	{
		if (path.empty()) { errOut = "path is required"; return false; }
		std::error_code ec;
		if (!std::filesystem::exists(path, ec))
			{ errOut = "scene file not found: " + path; return false; }
		if (sceneDirty)
		{
			errOut = "current scene has unsaved changes — save before loading";
			return false;
		}
		if (!LoadSceneFromFile(path))
		{
			errOut = "failed to load scene: " + path;
			return false;
		}
		if (project && project->IsOpen())
		{
			std::string rel = project->RelativePath(path);
			if (!rel.empty())
			{
				project->SetActiveSceneRel(rel);
				project->Save();
			}
		}
		return true;
	}

	bool SceneEditor::AgentPlay(std::string& errOut)
	{
		if (playMode) return true;
		EnterPlayMode();
		return true;
	}

	bool SceneEditor::AgentStopPlay(std::string& errOut)
	{
		(void)errOut;
		if (!playMode) return true;
		StopPlayMode();
		return true;
	}

	// Reads back the exact texture ShowViewport() hands to ImGui: for
	// Deferred that is the renderer's own composite (its final pass targets
	// framebuffer 0, so EffectsManager's capture holds only the overlay -
	// see ShowViewport()'s comment on the same two lines), for Forward the
	// captured viewport colour. Nothing is re-rendered, so this reports what
	// is on screen rather than what a second renderer would have drawn.
	//
	// The GL caveat on the offscreen path applies here too - reading the
	// live target is what crashes macOS's GLImage pixel processor - so this
	// waits for the device to go idle first and is only offered on the
	// explicit `live` opt-in.
	std::string SceneEditor::AgentScreenshotLiveViewport()
	{
		try
		{
			if (!EffectsManager || !Renderer) return std::string();

			GetActiveRenderDevice().WaitIdle();

			Texture* src = usingDeferredRenderer
				? static_cast<DeferredRenderer*>(Renderer)->GetColorTexture()
				: EffectsManager->GetViewportColor();
			if (!src) return std::string();
			const uint32 w = src->GetWidth();
			const uint32 h = src->GetHeight();
			if (w == 0 || h == 0) return std::string();

			std::vector<uchar> pixels = src->GetTextureData();
			std::vector<unsigned char> rgba;
			if (!ConvertPreviewPixelsToRGBA8(pixels, src->GetDataType(), w, h, rgba))
				return std::string();
#if !defined(_SDL2VULKAN) && !defined(_SDL2METAL)
			FlipRGBA8Vertically(rgba, w, h);
#endif
			int len = 0;
			unsigned char* png = stbi_write_png_to_mem(rgba.data(), (int)(w * 4), (int)w, (int)h, 4, &len);
			if (!png || len <= 0)
			{
				if (png) STBIW_FREE(png);
				return std::string();
			}
			std::string b64 = AgentServer::Base64Encode(png, (size_t)len);
			STBIW_FREE(png);
			return b64;
		}
		catch (const std::exception& e)
		{
			echo(std::string("AGENT: live screenshot failed: ") + e.what());
			return std::string();
		}
		catch (...)
		{
			return std::string();
		}
	}

	std::string SceneEditor::AgentScreenshot(bool liveViewport)
	{
		if (liveViewport)
			return AgentScreenshotLiveViewport();
		try
		{
			// Render the current view into the dedicated offscreen preview
			// targets (the same proven path as RenderCameraPreview / model
			// thumbnails), then read back. Reading the *live* main viewport
			// texture mid-frame crashes macOS's GLImage pixel processor.
			GameObject* viewCam = GetViewCameraGO();
			if (!viewCam || !previewRenderer || !previewEffects)
				return std::string();

			Projection proj;
			proj.Perspective(GetViewFovDeg(), (f32)previewWidth / (f32)previewHeight, 0.1f, 100000.f);

			IRenderer::InvalidateSharedUniformCaches();
			previewEffects->ProcessPostEffects(&proj);
			previewEffects->Resize(previewWidth, previewHeight);
			previewRenderer->Resize(previewWidth, previewHeight);
			previewRenderer->ResetViewPort();
			previewRenderer->SetViewPort(0, 0, previewWidth, previewHeight);
			previewRenderer->PreRender(viewCam, scene);
			previewRenderer->ApplyBackgroundClearColor();
			previewEffects->CaptureFrame();
			previewRenderer->RenderScene(proj, viewCam, scene);
			previewEffects->EndCapture();
			GetActiveRenderDevice().WaitIdle();
			IRenderer::InvalidateSharedUniformCaches();

			Texture* src = previewEffects->GetViewportColor();
			if (!src) return std::string();
			const uint32 w = src->GetWidth();
			const uint32 h = src->GetHeight();
			if (w == 0 || h == 0) return std::string();
			const uint32 srcType = src->GetDataType();
			std::vector<uchar> pixels = src->GetTextureData();
			std::vector<unsigned char> rgba;
			if (!ConvertPreviewPixelsToRGBA8(pixels, srcType, w, h, rgba))
				return std::string();
#if !defined(_SDL2VULKAN) && !defined(_SDL2METAL)
			FlipRGBA8Vertically(rgba, w, h);
#endif
			int len = 0;
			unsigned char* png = stbi_write_png_to_mem(rgba.data(), (int)(w * 4), (int)w, (int)h, 4, &len);
			if (!png || len <= 0)
			{
				if (png) STBIW_FREE(png);
				return std::string();
			}
			std::string b64 = AgentServer::Base64Encode(png, (size_t)len);
			STBIW_FREE(png);
			return b64;
		}
		catch (const std::exception& e)
		{
			echo(std::string("AGENT: screenshot failed: ") + e.what());
			return std::string();
		}
		catch (...)
		{
			return std::string();
		}
	}

	std::string SceneEditor::AgentLogTail(int maxLines)
	{
		std::string buf;
		const unsigned int count = p3d::LOG::_LOG::MessageCount();
		const unsigned int start = (maxLines > 0 && count > (unsigned)maxLines) ? count - (unsigned)maxLines : 0;
		for (unsigned int i = start; i < count; i++)
		{
			const p3d::LOG::Entry& e = p3d::LOG::_LOG::MessageAt(i);
			buf += e.text;
			buf += "\n";
		}
		return buf;
	}

	time_t SceneEditor::FileMtime(const std::string& path)
	{
		std::error_code ec;
		std::filesystem::file_time_type lwt = std::filesystem::last_write_time(path, ec);
		if (ec) return 0;
		auto dur = std::chrono::duration_cast<std::chrono::seconds>(lwt.time_since_epoch());
		return (time_t)dur.count();
	}

	bool SceneEditor::AgentReloadIfChanged()
	{
		if (scenePath.empty()) return false;
		const time_t m = FileMtime(scenePath);
		if (m == 0) return false;
		if (m == lastLoadMtime) return false;
		if (sceneDirty)
		{
			echo("AGENT: reload skipped — unsaved editor changes");
			return false;
		}
		if (playMode)
		{
			echo("AGENT: reload skipped — play mode is active");
			return false;
		}
		// NOTE: pass a COPY — LoadSceneFromFile() calls NewScene(), which does
		// scenePath.clear(); binding its const reference to the member would
		// wipe the path mid-load (serializer then gets an empty file name).
		const std::string pathCopy = scenePath;
		const bool ok = LoadSceneFromFile(pathCopy);
		echo(ok ? "AGENT: scene reloaded from disk" : "AGENT: scene reload failed");
		return ok;
	}
