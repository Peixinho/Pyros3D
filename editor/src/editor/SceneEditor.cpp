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
#include "ProjectManager.h"
#include "AgentServer.h"
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
		hostOpenSceneDocument = NULL;
		hostOpenLuaScript = NULL;
		hostEditMaterialInline = NULL;
		hostAssignMaterialAsset = NULL;
		previewRenderer = NULL;
		previewEffects = NULL;
		thumbRenderer = NULL;
		thumbEffects = NULL;
		Renderer = NULL;
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
		gbufferAlbedo->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, width, height, false);
		gbufferAlbedo->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		gbufferSpecular = new Texture();
		gbufferSpecular->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, width, height, false);
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
			Renderer = new DeferredRenderer(Width, Height, gbufferFBO);
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
			Renderer = new DeferredRenderer(Width, Height, gbufferFBO);
		}
		else
		{
			Renderer = new ForwardRenderer(Width, Height);
		}
		Renderer->SetViewPort(0, 0, Width, Height);
		Renderer->SetBackground(Vec4(0.2, 0.2, 0.2, 1.0));

		// Projection
		isPerspective = true;
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

		_leftMouse = _middleMouse = _rightMouse = _mousePanned = false;

		gizmo = NULL;
		localTransform = false;
		gizmoDragging = false;

		playMode = false;
		playModeSavedCameraId = 0;
		showPhysicsDebug = true;

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

		const bool scene_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
		// Escape always exits play (even with mouse capture / other panels focused).
		if (playMode && ImGui::IsKeyPressed(ImGuiKey_Escape))
			StopPlayMode();
		if (scene_focused && !playMode)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_T)) UseTranslationManipulator();
			if (ImGui::IsKeyPressed(ImGuiKey_R)) UseRotationManipulator();
			if (ImGui::IsKeyPressed(ImGuiKey_S)) UseScaleManipulator();
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
			viewportOverlayValid = false;
			if (showingAddFrom) ShowAddForm();
			else if (!playMode) editorDisabled = false;
			return;
		}

		GameObject* viewCam = GetViewCameraGO();
		const f32 viewFov = GetViewFovDeg();
		const f32 viewNear = (activeSceneCameraId != 0 && sceneCameras.count(activeSceneCameraId))
			? sceneCameras[activeSceneCameraId].nearPlane : 0.1f;
		const f32 viewFar = (activeSceneCameraId != 0 && sceneCameras.count(activeSceneCameraId))
			? sceneCameras[activeSceneCameraId].farPlane : 100000.f;

		projection.Perspective(viewFov, (f32)dim.x / (f32)dim.y, viewNear, viewFar);

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
		}

		viewportImgMin = ImGui::GetCursorScreenPos();
		viewportImgSize = ImVec2(dim.x, dim.y);
		UpdateViewportMouse();
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
		Renderer->Resize(viewW, viewH);
		Renderer->ResetViewPort();
		Renderer->SetViewPort(0, 0, viewW, viewH);
		Renderer->PreRender(viewCam, scene);
		Renderer->ApplyBackgroundClearColor();
		EffectsManager->CaptureFrame();
		if (isPerspective)
			Renderer->RenderScene(projection, viewCam, scene);
		else
			Renderer->RenderScene(projectionOrtho, viewCam, scene);

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

		// Gizmo must submit into DebugRenderer before the flush below.
		if (!playMode)
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

			// See AxisHelper::SetAmbientLight()'s comment - its internal
			// Renderer shares IRenderer's process-wide ambient UBO with
			// this SceneEditor's own Renderer, and renders every frame
			// right after it does, so keeping the two values equal is
			// what actually prevents them fighting over that shared slot
			// (not just leaving AxisHelper's alone).
			axisHelper->SetAmbientLight(ambientLightColor);
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
					sceneObjects->ReparentGameObject(draggedId, 0);
					MarkSceneDirty();
				}
				ImGui::EndDragDropTarget();
			}

			DrawNodes();

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
#ifdef LUA_BINDINGS
						if (hierarchyForceOpenId != 0 && (*i).second->GetID() == hierarchyForceOpenId)
						{
							ImGui::SetNextItemOpen(true, ImGuiCond_Always);
							hierarchyForceOpenId = 0;
						}
#endif
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
				{"fov", i->second.fov},
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
				s.fov = it.value().value("fov", 70.f);
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
		if (!viewCam || imgSize.x < 1.f || imgSize.y < 1.f) return;

		ImDrawList* dl = ImGui::GetWindowDrawList();
		const Matrix viewM = viewCam->GetWorldTransformation().Inverse();
		const Matrix projM = (isPerspective ? projection : projectionOrtho).GetProjectionMatrix();

		auto projectToImage = [&](const Vec3& wp, ImVec2& out) -> bool {
			Vec4 clip = projM * (viewM * Vec4(wp.x, wp.y, wp.z, 1.f));
			if (clip.w <= 0.0001f) return false;
			const f32 ndcX = clip.x / clip.w;
			const f32 ndcY = clip.y / clip.w;
			const f32 ndcZ = clip.z / clip.w;
			if (ndcZ < -1.f || ndcZ > 1.f) return false;
			const f32 u = ndcX * 0.5f + 0.5f;
			const f32 v = 1.f - (ndcY * 0.5f + 0.5f);
			out.x = imgMin.x + u * imgSize.x;
			out.y = imgMin.y + v * imgSize.y;
			return true;
		};

		auto drawIconAt = [&](const char* icon, const Vec3& wp, ImU32 color, f32 pxSize) {
			ImVec2 p;
			if (!projectToImage(wp, p)) return;
			ImVec2 sz = ImGui::CalcTextSize(icon);
			const f32 scale = pxSize / (sz.y > 0.f ? sz.y : 1.f);
			dl->AddText(NULL, pxSize, ImVec2(p.x - (sz.x * scale) * 0.5f, p.y - (sz.y * scale) * 0.5f), color, icon);
		};

		std::vector<std::shared_ptr<GameObject>>& all = scene->GetAllGameObjectList();
		for (std::vector<std::shared_ptr<GameObject>>::iterator it = all.begin(); it != all.end(); ++it)
		{
			GameObject* go = (*it).get();
			if (!go || IsInternalGameObject(go)) continue;

			uint32 goId = sceneObjects->GetSceneObjectID(go);
			if (IsSceneCamera(goId) && go != viewCam && editorDebugDraw->IsCameraOn(go))
				drawIconAt(u8"\uf030", go->GetWorldPosition(), IM_COL32(0, 255, 255, 255), 22.f);

			const std::vector<std::shared_ptr<IComponent>>& comps = go->GetComponents();
			for (std::vector<std::shared_ptr<IComponent>>::const_iterator ci = comps.begin(); ci != comps.end(); ++ci)
			{
				IComponent* c = (*ci).get();
				if (!c || !editorDebugDraw->IsOn(c)) continue;
				if (dynamic_cast<DirectionalLight*>(c))
					drawIconAt(u8"\uf185", go->GetWorldPosition(), IM_COL32(255, 220, 0, 255), 18.f);
				else if (dynamic_cast<PointLight*>(c) || dynamic_cast<SpotLight*>(c))
					drawIconAt(u8"\uf0eb", go->GetWorldPosition(), IM_COL32(255, 220, 0, 255), 18.f);
			}
		}
	}

	void SceneEditor::CreateSceneCamera()
	{
		SceneObject* obj = sceneObjects->CreateGameObject("Camera");
		if (obj != NULL)
		{
			RegisterSceneCamera(obj->GetID());
			MarkSceneDirty();
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
			echo("ERROR: failed writing model thumbnail: " + thumbPath);
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
				echo("Thumbnail: " + path);
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

		// Expose echo() so Lua scripts can write to the editor log window.
		(*sharedLua)["echo"] = [](const std::string& msg) { p3d::LOG::_LOG::_echo(msg); };

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
					echo("ERROR: Script must return a middleclass class with :new() — " + absoluteScriptPath);
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
		echo("SUCCESS: Attached script " + absoluteScriptPath + " to " + goObj->GetName());
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
			ImGui::TextWrapped("File: %s", scenePath.c_str());
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
			const std::string companion = ProjectManager::SceneScriptPathForSceneJson(scenePath);
			std::string rel = companion;
			if (project && project->IsOpen())
			{
				const std::string r = project->RelativePath(companion);
				if (!r.empty()) rel = r;
			}
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
				if (ImGui::MenuItem("Plane")) { OpenAddFormOnGameObject(goId, 4); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Cone")) { OpenAddFormOnGameObject(goId, 5); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Cylinder")) { OpenAddFormOnGameObject(goId, 6); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Torus")) { OpenAddFormOnGameObject(goId, 7); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Capsule")) { OpenAddFormOnGameObject(goId, 3); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Torus Knot")) { OpenAddFormOnGameObject(goId, 8); ImGui::CloseCurrentPopup(); }
				ImGui::Separator();
				if (ImGui::MenuItem("Import Model")) { OpenAddFormOnGameObject(goId, 9); ImGui::CloseCurrentPopup(); }
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Physics"))
			{
				if (ImGui::MenuItem("Box")) { OpenAddFormOnGameObject(goId, 13); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Sphere")) { OpenAddFormOnGameObject(goId, 17); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Capsule")) { OpenAddFormOnGameObject(goId, 14); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Cone")) { OpenAddFormOnGameObject(goId, 15); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Cylinder")) { OpenAddFormOnGameObject(goId, 16); ImGui::CloseCurrentPopup(); }
				if (ImGui::MenuItem("Static Plane")) { OpenAddFormOnGameObject(goId, 18); ImGui::CloseCurrentPopup(); }
				ImGui::EndMenu();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Sound"))
			{
				AddForm_soundPath.clear();
				AddForm_stream = false;
				AddForm_loop = false;
				AddForm_spatialized = true;
				AddForm_volume = 1.0f;
				OpenAddFormOnGameObject(goId, 19);
				ImGui::CloseCurrentPopup();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Directional Light")) { AddQuickLightOnGameObject(goId, 10); ImGui::CloseCurrentPopup(); }
			if (ImGui::MenuItem("Point Light")) { AddQuickLightOnGameObject(goId, 11); ImGui::CloseCurrentPopup(); }
			if (ImGui::MenuItem("Spot Light")) { AddQuickLightOnGameObject(goId, 12); ImGui::CloseCurrentPopup(); }
			ImGui::EndMenu();
		}
	}

	bool SceneEditor::TryPickViewportIcon(const Vec2& viewportMouse, uint32& outSceneObjectId)
	{
		outSceneObjectId = 0;
		if (!viewportOverlayValid || viewportImgSize.x < 1.f || viewportImgSize.y < 1.f || dim.x < 1.f || dim.y < 1.f)
			return false;

		GameObject* viewCam = GetViewCameraGO();
		if (!viewCam) return false;

		const Matrix viewM = viewCam->GetWorldTransformation().Inverse();
		const Matrix projM = (isPerspective ? projection : projectionOrtho).GetProjectionMatrix();

		auto projectToImage = [&](const Vec3& wp, ImVec2& out) -> bool {
			Vec4 clip = projM * (viewM * Vec4(wp.x, wp.y, wp.z, 1.f));
			if (clip.w <= 0.0001f) return false;
			const f32 ndcX = clip.x / clip.w;
			const f32 ndcY = clip.y / clip.w;
			const f32 ndcZ = clip.z / clip.w;
			if (ndcZ < -1.f || ndcZ > 1.f) return false;
			const f32 u = ndcX * 0.5f + 0.5f;
			const f32 v = 1.f - (ndcY * 0.5f + 0.5f);
			out.x = viewportImgMin.x + u * viewportImgSize.x;
			out.y = viewportImgMin.y + v * viewportImgSize.y;
			return true;
		};

		const ImVec2 mouseScreen(
			viewportImgMin.x + (viewportMouse.x / dim.x) * viewportImgSize.x,
			viewportImgMin.y + (viewportMouse.y / dim.y) * viewportImgSize.y);

		auto hitIcon = [&](const char* icon, const Vec3& wp, f32 pxSize) -> bool {
			ImVec2 p;
			if (!projectToImage(wp, p)) return false;
			ImVec2 sz = ImGui::CalcTextSize(icon);
			const f32 scale = pxSize / (sz.y > 0.f ? sz.y : 1.f);
			const f32 hw = (sz.x * scale) * 0.5f;
			const f32 hh = (sz.y * scale) * 0.5f;
			return mouseScreen.x >= p.x - hw && mouseScreen.x <= p.x + hw
				&& mouseScreen.y >= p.y - hh && mouseScreen.y <= p.y + hh;
		};

		std::vector<std::shared_ptr<GameObject>>& all = scene->GetAllGameObjectList();
		for (std::vector<std::shared_ptr<GameObject>>::iterator it = all.begin(); it != all.end(); ++it)
		{
			GameObject* go = (*it).get();
			if (!go || IsInternalGameObject(go)) continue;
			const uint32 goId = sceneObjects->GetSceneObjectID(go);
			const Vec3 pos = go->GetWorldPosition();

			if (IsSceneCamera(goId) && go != viewCam && editorDebugDraw->IsCameraOn(go))
			{
				if (hitIcon(u8"\uf030", pos, 22.f))
				{
					outSceneObjectId = goId;
					return true;
				}
			}

			const std::vector<std::shared_ptr<IComponent>>& comps = go->GetComponents();
			for (std::vector<std::shared_ptr<IComponent>>::const_iterator ci = comps.begin(); ci != comps.end(); ++ci)
			{
				IComponent* c = (*ci).get();
				if (!c || !editorDebugDraw->IsOn(c)) continue;
				const char* icon = NULL;
				uint32 compType = 0;
				if (dynamic_cast<DirectionalLight*>(c)) { icon = u8"\uf185"; compType = SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT; }
				else if (dynamic_cast<PointLight*>(c)) { icon = u8"\uf0eb"; compType = SceneObjectTypes::POINTLIGHT_COMPONENT; }
				else if (dynamic_cast<SpotLight*>(c)) { icon = u8"\uf0eb"; compType = SceneObjectTypes::SPOTLIGHT_COMPONENT; }
				if (!icon) continue;
				if (hitIcon(icon, pos, 18.f))
				{
					outSceneObjectId = sceneObjects->GetSceneObjectID(c);
					if (outSceneObjectId == 0)
						outSceneObjectId = goId;
					return true;
				}
				(void)compType;
			}
		}
		return false;
	}

	void SceneEditor::ShowViewOptions()
	{
		bool frustum = editorDebugDraw->IsCameraFrustumOn();
		if (ImGui::MenuItem("Show Camera Frustums", NULL, frustum))
			editorDebugDraw->ToggleCameraFrustum(!frustum);
		if (ImGui::MenuItem("Show Physics Debug", NULL, showPhysicsDebug))
			showPhysicsDebug = !showPhysicsDebug;
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
						sceneObjects->ReparentGameObject(obj->GetID(), 0);
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
					DeleteComponentById(obj->GetID());
					ImGui::EndPopup();
					return;
				}
			}
			if (obj->GetType() == SceneObjectTypes::GAMEOBJECT && ImGui::MenuItem("Delete GameObject"))
			{
				DeleteGameObjectById(obj->GetID());
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
							sceneObjects->ReparentGameObject(draggedId, obj->GetID());
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

	void SceneEditor::UpdateViewportMouse()
	{
		viewportMouseValid = false;
		viewportHovered = false;
		if (!viewportOverlayValid || dim.x < 1.f || dim.y < 1.f
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

		if (playMode)
			physics->Update(time, 10);

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
			UpdateSceneMainScript(time);
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

	void SceneEditor::EnterPlayMode()
	{
		if (playMode) return;
		scriptRenderCamera = nullptr;
		echo("SUCCESS: Entering play mode");
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
		int luaInited = 0;
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin();
			i != sceneObjects->GetList().end(); ++i)
		{
			if (!i->second || i->second->GetType() != SceneObjectTypes::LUA_COMPONENT) continue;
			LuaComponent* lc = (LuaComponent*)i->second->GetPTR();
			if (!lc) continue;
			try {
				lc->Init();
				++luaInited;
				if (!lc->scriptFile.empty())
					echo(std::string("SUCCESS: Lua init — ") + lc->scriptFile);
			}
			catch (const std::exception& e) { echo(std::string("ERROR: LuaComponent::Init - ") + e.what()); }
			catch (...) { echo("ERROR: LuaComponent::Init - unknown exception"); }
		}
		if (luaInited == 0)
			echo("WARNING: Play mode — no LuaComponent on any GameObject (attach a script and Save scene)");
		else
			echo("SUCCESS: Play mode — Tab toggles mouse capture; Esc stops play");
		ResetSceneMainScriptLifecycle();
		InitSceneMainScript();
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

	void SceneEditor::CaptureGizmoBaseline()
	{
		if (!SelectedSceneObject || SelectedSceneObject->GetType() != SceneObjectTypes::GAMEOBJECT) return;
		GameObject* go = (GameObject*)SelectedSceneObject->GetPTR();
		if (!go) return;
		gizmoBaselineLocal = SelectedSceneObject->LocalTransform;
		gizmoBaselineParentWorld = go->HaveParent() ? go->GetParent()->GetWorldTransformation() : Matrix();
		gizmoBaselineWorld = gizmoBaselineParentWorld * gizmoBaselineLocal;
	}

	void SceneEditor::PrepareGizmoForDraw(GameObject* viewCam)
	{
		if (!viewCam || playMode || !gizmo || !SelectedSceneObject
			|| SelectedSceneObject->GetType() != SceneObjectTypes::GAMEOBJECT)
			return;

		GameObject* selGo = (GameObject*)SelectedSceneObject->GetPTR();
		const bool hasParent = selGo->HaveParent();
		Matrix liveWorld = selGo->GetWorldTransformation();
		Matrix liveParentWorld = hasParent ? selGo->GetParent()->GetWorldTransformation() : Matrix();
		Matrix anchorWorld = gizmoDragging ? gizmoBaselineWorld : liveWorld;
		Matrix parentWorld = gizmoDragging ? gizmoBaselineParentWorld : liveParentWorld;

		gizmo->SetDisplayScale(isPerspective ? .15f : .22f);

		Mouse3D mray;
		mray.GenerateRay(dim.x, dim.y, viewportMouse.x, viewportMouse.y, Matrix(),
			viewCam->GetWorldTransformation().Inverse(), projectionOrtho.GetProjectionMatrix());
		gizmo->SetOrthoMouse(mray.GetOrigin().x, mray.GetOrigin().y, mray.GetOrigin().z,
			mray.GetDirection().x, mray.GetDirection().y, mray.GetDirection().z);
		gizmo->SetScreenDimension(dim.x, dim.y, isPerspective, l, r, b, t);

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
			(isPerspective ? projection : projectionOrtho).GetProjectionMatrix().m);
	}

	void SceneEditor::ApplyGizmoTransformToObject()
	{
		if (!SelectedSceneObject || SelectedSceneObject->GetType() != SceneObjectTypes::GAMEOBJECT)
			return;

		// World rotate: libgizmo writes only a delta into globalRotation.
		// Bake it onto the *orientation* of LocalTransform and keep translation
		// — Pyros Matrix multiply would otherwise rotate the position around
		// the world origin (R * T).
		Matrix baked = SelectedSceneObject->LocalTransform;
		if (GizmoInUse == GizmoFunction::ROTATION && !localTransform)
		{
			const Vec3 pos = SelectedSceneObject->LocalTransform.GetTranslation();
			Matrix orient = SelectedSceneObject->LocalTransform;
			orient.Translate(Vec3(0, 0, 0));
			baked = SelectedSceneObject->globalRotation * orient;
			baked.Translate(pos);
		}

		Vec3 pos = baked.GetTranslation();
		Vec3 scl = SelectedSceneObject->ScaleTransform.GetScale();
		Vec3 basisScale = baked.GetScale();
		if (fabs(basisScale.x) < 0.0001f) basisScale.x = 1.0f;
		if (fabs(basisScale.y) < 0.0001f) basisScale.y = 1.0f;
		if (fabs(basisScale.z) < 0.0001f) basisScale.z = 1.0f;
		Vec3 rot = baked.GetRotation(basisScale).GetEulerFromRotationMatrix();

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
		uint32 iconPickId = 0;
		if (TryPickViewportIcon(viewportMouse, iconPickId))
		{
			DeselectMesh();
			SelectSceneObject(sceneObjects->GetSceneObject(iconPickId));
			node_clicked = iconPickId;
			return;
		}

		Picking->Resize(dim.x, dim.y);
		Picking->ResetViewPort();
		Picking->SetViewPort(0, 0, dim.x, dim.y);
		RenderingMesh* rm = Picking->PickObject(viewportMouse.x, viewportMouse.y,
			(isPerspective ? projection : projectionOrtho), GetViewCameraGO(), scene);

		if (rm != NULL && rm->renderingComponent != rGrid.get())
		{
			bool helper = false;
			for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin();
				i != sceneObjects->GetList().end(); i++)
			{
				if ((*i).second->Helper)
				{
					if ((*i).second->Helper.get() == rm->renderingComponent->GetOwner())
					{
						node_clicked = sceneObjects->GetSceneObjectID(((IHelper*)(*i).second->Helper.get())->owner);
						helper = true;
						break;
					}
				}
			}

			if (!helper)
				node_clicked = sceneObjects->GetSceneObjectID(rm->renderingComponent->GetOwner());

			DeselectMesh();
			SelectSceneObject(sceneObjects->GetSceneObject(node_clicked));
		}
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
				CaptureGizmoBaseline();
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

			GameObject* selGo = (GameObject*)SelectedSceneObject->GetPTR();
			if (selGo->HaveParent())
				gizmo->SetLocalTransform((float*)&selGo->GetWorldTransformation().m);
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
					// Fold world delta into LocalTransform (orientation only).
					const Vec3 trans = SelectedSceneObject->LocalTransform.GetTranslation();
					Matrix orient = SelectedSceneObject->LocalTransform;
					orient.Translate(Vec3(0, 0, 0));
					Matrix baked = SelectedSceneObject->globalRotation * orient;
					Vec3 basisScale = baked.GetScale();
					if (fabs(basisScale.x) < 0.0001f) basisScale.x = 1.0f;
					if (fabs(basisScale.y) < 0.0001f) basisScale.y = 1.0f;
					if (fabs(basisScale.z) < 0.0001f) basisScale.z = 1.0f;
					const Vec3 rot = baked.GetRotation(basisScale).GetEulerFromRotationMatrix();
					SelectedSceneObject->LocalTransform.identity();
					SelectedSceneObject->LocalTransform.Translate(trans);
					SelectedSceneObject->LocalTransform.SetRotationFromEuler(rot);
					SelectedSceneObject->globalRotation.identity();
				}
				ApplyGizmoTransformToObject();
				SyncTransformFromGameObject(SelectedSceneObject);
				CaptureGizmoBaseline();
			}
			gizmoDragging = false;
			_leftMouse = false;
		}
	}

	void SceneEditor::DeleteComponentById(uint32 objId)
	{
		SceneObject* obj = sceneObjects->GetSceneObject(objId);
		if (!obj || obj->GetType() == SceneObjectTypes::GAMEOBJECT) return;
		editorDebugDraw->ForgetComponent((IComponent*)obj->GetPTR());
		if (SelectedSceneObject == obj)
		{
			DeselectMesh();
			DeselectSceneObject();
			selection.clear();
		}
		sceneObjects->DestroySceneObject(objId);
		MarkSceneDirty();
	}

	void SceneEditor::DeleteGameObjectById(uint32 objId)
	{
		SceneObject* obj = sceneObjects->GetSceneObject(objId);
		if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return;
		GameObject* go = (GameObject*)obj->GetPTR();
		if (IsInternalGameObject(go)) return;

		const bool wasCamera = IsSceneCamera(objId);
		if (wasCamera)
			editorDebugDraw->ForgetCamera(go);
		for (std::map<uint32, SceneObject*>::const_iterator j = sceneObjects->GetList().begin(); j != sceneObjects->GetList().end(); j++)
		{
			if (j->second == NULL || j->second->GetType() == SceneObjectTypes::GAMEOBJECT) continue;
			uint32 pid = j->second->GetParentID();
			while (pid != 0)
			{
				if (pid == objId)
				{
					editorDebugDraw->ForgetComponent((IComponent*)j->second->GetPTR());
					break;
				}
				SceneObject* parent = sceneObjects->GetSceneObject(pid);
				if (parent == NULL || parent->GetType() != SceneObjectTypes::GAMEOBJECT) break;
				pid = parent->GetParentID();
			}
		}
		if (SelectedSceneObject == obj)
		{
			DeselectMesh();
			DeselectSceneObject();
			selection.clear();
		}
		// Clear scriptRenderCamera before destruction to avoid dangling pointer.
		if (scriptRenderCamera == go)
			scriptRenderCamera = nullptr;
		sceneObjects->DestroySceneObject(objId);
		if (wasCamera)
		{
			if (activeSceneCameraId == objId)
				activeSceneCameraId = 0;
			UnregisterSceneCamera(objId);
		}
		node_clicked = -1;
		MarkSceneDirty();
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
		GameObject* go = (GameObject*)SelectedSceneObject->GetPTR();
		if (IsInternalGameObject(go)) return 0;

		const uint32 srcId = SelectedSceneObject->GetID();
		const bool wasCamera = IsSceneCamera(srcId);
		EditorCameraSettings camSettings;
		if (wasCamera)
			camSettings = sceneCameras[srcId];

		SceneObject* dup = sceneObjects->DuplicateGameObject(srcId, physics);
		if (!dup) return 0;

		if (wasCamera)
			RegisterSceneCamera(dup->GetID(), camSettings);

		SelectSceneObject(dup);
		node_clicked = dup->GetID();
		MarkSceneDirty();
		return dup->GetID();
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
				CaptureGizmoBaseline();
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
		}
	}

	void SceneEditor::NewScene(bool applyProjectDefaults)
	{
		(void)applyProjectDefaults;
		if (playMode)
			StopPlayMode();
		DeselectMesh();
		DeselectSceneObject();
		selection.clear();
		node_clicked = -1;

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
#ifdef LUA_BINDINGS
			PushLuaHostGlobals();
			meta.mainScript = sceneMainScriptPath;
			ok = SceneSerializer::SaveScene(scene, path, sharedLua, &meta);
#else
			ok = SceneSerializer::SaveScene(scene, path, NULL, &meta);
#endif
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
#ifdef LUA_BINDINGS
			PushLuaHostGlobals();
			ok = SceneSerializer::LoadScene(scene, path, physics, sharedLua, NULL, &meta);
			if (ok)
			{
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
				RebuildSceneMainScriptInstance();
			}
#else
			ok = SceneSerializer::LoadScene(scene, path, physics, NULL, NULL, &meta);
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

		AttachEditorObjects(furniture);
		if (ok) RebuildHelpers();
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

		if (!parentGo)
		{
			CreateGameObject(name.empty() ? (isSound ? "Sound" : "Model") : name);
			if (!SelectedSceneObject) return false;
			parentGo = (GameObject*)SelectedSceneObject->GetPTR();
		}

		if (isModel)
		{
			sceneObjects->CreateRenderingModel(parentGo, absolutePath);
			MarkSceneDirty();
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
                    if (ImGui::InputText("Name", &SelectedSceneObject->Name))
					{
						sceneObjects->SetName(SelectedSceneObject->GetID(), SelectedSceneObject->Name);
						MarkSceneDirty();
					}
					else
						sceneObjects->SetName(SelectedSceneObject->GetID(), SelectedSceneObject->Name);
					if (ImGui::DragFloat3("Position", (float *)&_translation, 0.1f, 0.0f, 0.0f))
						MarkSceneDirty();
					if (ImGui::DragFloat3("Rotation", (float *)&_rotation, 0.1f, 0.0f, 0.0f))
						MarkSceneDirty();
					if (IsSceneCamera(SelectedSceneObject->GetID()))
					{
						if (ImGui::DragFloat3("Scale", (float *)&_scale, 0.1f, 0.001f, 0.0f))
							MarkSceneDirty();
					}
					else if (ImGui::DragFloat3("Scale", (float *)&_scale, 0.1f, 0.0f, 0.0f))
						MarkSceneDirty();
					if (IsSceneCamera(SelectedSceneObject->GetID()))
					{
						EditorCameraSettings& cam = sceneCameras[SelectedSceneObject->GetID()];
						ImGui::Separator();
						ImGui::Text("Camera");
						ImGui::DragFloat("FOV", &cam.fov, 0.5f, 10.f, 170.f);
						ImGui::DragFloat("Near", &cam.nearPlane, 0.01f, 0.001f, 100.f);
						ImGui::DragFloat("Far", &cam.farPlane, 1.f, 1.f, 100000.f);
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
#ifdef LUA_BINDINGS
					DrawGameObjectScriptProperties(SelectedSceneObject->GetID());
#endif
				}
				break;
				case SceneObjectTypes::RENDERING_COMPONENT:
				{
					RenderingComponent* r = (RenderingComponent*)SelectedSceneObject->GetPTR();
					std::vector<RenderingMesh*>& meshes = r->GetMeshes();
					if (meshes.empty())
						ImGui::TextDisabled("(no submeshes)");
					for (size_t m = 0; m < meshes.size(); ++m)
					{
						ImGui::PushID((int)m);
						ImGui::Text("Submesh %zu", m);
						if (meshes[m]->Material)
						{
							ImGui::SameLine();
							if (ImGui::SmallButton("Edit Material") && hostEditMaterialInline)
								hostEditMaterialInline(meshes[m]->Material,
									SelectedSceneObject->Name + " / Submesh " + std::to_string(m));
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
										propertiesMaterialAssignError = hostAssignMaterialAsset(SelectedSceneObject->Name, (int)m, e.relativePath);
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
									propertiesMaterialAssignError = hostAssignMaterialAsset(SelectedSceneObject->Name, (int)m, rel);
							}
							ImGui::EndDragDropTarget();
						}
						if (!propertiesMaterialAssignError.empty())
							ImGui::TextColored(ImVec4(1.f, 0.4f, 0.35f, 1.f), "%s", propertiesMaterialAssignError.c_str());

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
						ImGui::TextWrapped("%s", lc->scriptFile.c_str());
					else
						ImGui::TextDisabled("(no script file)");
					if (lc && !lc->scriptFile.empty() && hostOpenLuaScript
						&& ImGui::Button("Open Script", ImVec2(140, 0)))
						hostOpenLuaScript(ResolveScriptPath(lc->scriptFile));
				}
				else
				{
					if (lc && !lc->scriptFile.empty())
						ImGui::TextWrapped("%s", lc->scriptFile.c_str());
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
					if (ImGui::ColorEdit4("Color", (float*)&PropertiesLightColor))
					{
						l->SetLightColor(PropertiesLightColor);
						MarkSceneDirty();
					}
					if (ImGui::DragFloat3("Direction", (float *)&PropertiesLightDirection, 0.01f, -1.0f, 1.0f))
					{
						l->SetLightDirection(PropertiesLightDirection);
						MarkSceneDirty();
					}
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
					if (ImGui::ColorEdit4("Color", (float*)&PropertiesLightColor))
					{
						l->SetLightColor(PropertiesLightColor);
						MarkSceneDirty();
					}
					if (ImGui::DragFloat("Radius", (float *)&PropertiesLightRadius, 0.01f, 0.001f, 0.0f))
					{
						l->SetLightRadius(PropertiesLightRadius);
						MarkSceneDirty();
					}
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
					if (ImGui::ColorEdit4("Color", (float*)&PropertiesLightColor))
					{
						l->SetLightColor(PropertiesLightColor);
						MarkSceneDirty();
					}
					if (ImGui::DragFloat("Radius", (float *)&PropertiesLightRadius, 0.01f, 0.001f, 0.0f))
					{
						l->SetLightRadius(PropertiesLightRadius);
						MarkSceneDirty();
					}
					if (ImGui::DragFloat3("Direction", (float *)&PropertiesLightDirection, 0.01f, -1.0f, 1.0f))
					{
						l->SetLightDirection(PropertiesLightDirection);
						MarkSceneDirty();
					}
					if (ImGui::DragFloat("Outter Cone", (float *)&PropertiesLightOutterCone, 0.01f, 0.002f, 0.0f))
					{
						l->SetLightOutterCone(PropertiesLightOutterCone);
						MarkSceneDirty();
					}
					if (ImGui::DragFloat("Inner Cone", (float *)&PropertiesLightInnerCone, 0.01f, 0.001f, 0.0f))
					{
						l->SetLightInnerCone(PropertiesLightInnerCone);
						MarkSceneDirty();
					}
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
					ImGui::Text("File: %s", asrc->GetFile().c_str());
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
		const bool needsOwner = (showingAddFormType >= 1 && showingAddFormType <= 19);
		if (needsOwner && ownerGO == NULL)
		{
			echo("ERROR: Select a GameObject (or one of its components) before adding.");
			return;
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
				std::string imported;
				std::string err;
				if (project->ImportModel(AddForm_modelPath, imported, &err))
					modelPath = imported;
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
		default:
			break;
		}
		MarkSceneDirty();
	}

	void SceneEditor::ShowMenubarOptions()
	{
		if (ImGui::BeginMenu("Scene", ""))
		{
			if (ImGui::MenuItem("New Scene", ""))
			{
				if (hostNewSceneDocument)
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

			if (ImGui::MenuItem("Open Scene", ""))
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

			if (ImGui::MenuItem("Open Scene Script", NULL, false, !scenePath.empty() && !playMode))
			{
				EnsureAndBindSceneCompanionScript();
				if (hostOpenLuaScript && !sceneMainScriptPath.empty())
					hostOpenLuaScript(sceneMainScriptPath);
			}

			ImGui::Separator();

			if (playMode)
			{
				if (ImGui::MenuItem("Stop Play", "Esc"))
					StopPlayMode();
			}
			else if (ImGui::MenuItem("Play"))
				EnterPlayMode();

			ShowRightMenu();

			ImGui::EndMenu();
		}
	}

	void SceneEditor::ShowRightMenu()
	{
		if (playMode) return;
		if (ImGui::BeginMenu("Add", ""))
		{
            if (ImGui::MenuItem("Game Object", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 0; AddForm_go = "GameObject"; }
            if (ImGui::MenuItem("Camera", "")) CreateSceneCamera();
			ImGui::Separator();
			if (ImGui::BeginMenu("Mesh", ""))
			{
				if (ImGui::BeginMenu("Primitives"))
				{
                    if (ImGui::MenuItem("Cube", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 1; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_d = 1.0; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                    if (ImGui::MenuItem("Sphere", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 2; AddForm_w = 1.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_cgo = false; AddForm_hs = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                    if (ImGui::MenuItem("Capsule", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 3; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_r = 8.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                    if (ImGui::MenuItem("Plane", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 4; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                    if (ImGui::MenuItem("Cone", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 5; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_oe = false; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                    if (ImGui::MenuItem("Cylinder", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 6; AddForm_w = 1.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_oe = false; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                    if (ImGui::MenuItem("Torus", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 7; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
					if (ImGui::MenuItem("Torus Knot", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 8; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_p = 1.0; AddForm_q = 1.0; AddForm_hscale = 1.0; AddForm_cgo = false; AddForm_go.clear(); AddForm_sn = false; AddForm_fn = false; }
					ImGui::EndMenu();
				}
				ImGui::Separator();
                if (ImGui::MenuItem("Import Model")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 9; AddForm_modelPath.clear(); AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; AddForm_cgo = false; }
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Lights", ""))
			{
                if (ImGui::MenuItem("Directional", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 10; AddForm_color = Vec4(1, 1, 1, 1); AddForm_dir = Vec3(0, -1, 0); AddForm_cs = false; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                if (ImGui::MenuItem("Point", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 11; AddForm_w = 10.0; AddForm_color = Vec4(1, 1, 1, 1); AddForm_cs = false; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                if (ImGui::MenuItem("Spot", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 12; AddForm_w = 10.0; AddForm_color = Vec4(1, 1, 1, 1); AddForm_dir = Vec3(0, -1, 0); AddForm_cs = false; AddForm_oc = 45.f; AddForm_ic = 30.f; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Physics", ""))
			{
				if (ImGui::MenuItem("Box", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 13; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_d = 1.0; AddForm_mass = 1.0f; AddForm_ghost = false; AddForm_cgo = false; AddForm_go = ""; }
				if (ImGui::MenuItem("Capsule", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 14; AddForm_w = 0.5f; AddForm_h = 1.0; AddForm_mass = 1.0f; AddForm_ghost = false; AddForm_cgo = false; AddForm_go = ""; }
				if (ImGui::MenuItem("Cone", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 15; AddForm_w = 0.5f; AddForm_h = 1.0; AddForm_mass = 1.0f; AddForm_ghost = false; AddForm_cgo = false; AddForm_go = ""; }
				if (ImGui::MenuItem("Cylinder", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 16; AddForm_w = 0.5f; AddForm_h = 1.0; AddForm_mass = 1.0f; AddForm_ghost = false; AddForm_cgo = false; AddForm_go = ""; }
				if (ImGui::MenuItem("Sphere", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 17; AddForm_w = 0.5f; AddForm_mass = 1.0f; AddForm_ghost = false; AddForm_cgo = false; AddForm_go = ""; }
				if (ImGui::MenuItem("Static Plane", "")) { showingAddFrom = true; openAddFormTrigger = true; showingAddFormType = 18; AddForm_dir = Vec3(0, 1, 0); AddForm_w = 0.0f; AddForm_mass = 0.0f; AddForm_ghost = false; AddForm_cgo = false; AddForm_go = ""; }
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Sound", ""))
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

		json AgentComponentToJson(IComponent* c)
		{
			if (!c) return json();
			json j;
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
			std::string outAbs, err;
			if (!project->ImportModel(modelFile, outAbs, &err))
			{
				errOut = "model import failed: " + err;
				return false;
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
		return true;
	}

	bool SceneEditor::AgentSetTransform(const std::string& name, const json& t, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();
		if (t.is_object())
		{
			if (t.contains("position") && t["position"].is_array() && t["position"].size() == 3)
				go->SetPosition(Vec3((f32)t["position"][0].get<double>(), (f32)t["position"][1].get<double>(), (f32)t["position"][2].get<double>()));
			if (t.contains("rotation") && t["rotation"].is_array())
			{
				const auto& r = t["rotation"];
				if (r.size() == 4)
					go->SetRotation(AgentQuatToEuler({ (f32)r[0].get<double>(), (f32)r[1].get<double>(), (f32)r[2].get<double>(), (f32)r[3].get<double>() }));
				else if (r.size() == 3)
					go->SetRotation(Vec3((f32)r[0].get<double>(), (f32)r[1].get<double>(), (f32)r[2].get<double>()));
			}
			if (t.contains("scale") && t["scale"].is_array() && t["scale"].size() == 3)
				go->SetScale(Vec3((f32)t["scale"][0].get<double>(), (f32)t["scale"][1].get<double>(), (f32)t["scale"][2].get<double>()));
		}
		MarkSceneDirty();
		return true;
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
		sceneObjects->SetName(obj->GetID(), newName);
		MarkSceneDirty();
		return true;
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
		if (!sceneObjects->ReparentGameObject(child->GetID(), parentId))
			{ errOut = "reparent failed (cycle or invalid)"; return false; }
		MarkSceneDirty();
		return true;
	}

	bool SceneEditor::AgentDuplicate(const std::string& name, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		if (!sceneObjects->DuplicateGameObject(obj->GetID(), physics))
			{ errOut = "duplicate failed"; return false; }
		MarkSceneDirty();
		return true;
	}

	bool SceneEditor::AgentDeleteObject(const std::string& name, std::string& errOut)
	{
		if (playMode) { errOut = "editor is in play mode"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, name);
		if (!obj) { errOut = "object '" + name + "' not found"; return false; }
		sceneObjects->DestroySceneObject(obj->GetID());
		MarkSceneDirty();
		return true;
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
		if (!mat) { errOut = "no material to assign"; return false; }
		SceneObject* obj = AgentFindGameObjectByName(sceneObjects, objectName);
		if (!obj) { errOut = "object '" + objectName + "' not found"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();
		RenderingComponent* rc = NULL;
		for (auto& c : go->GetComponents())
			if ((rc = dynamic_cast<RenderingComponent*>(c.get()))) break;
		if (!rc) { errOut = "object '" + objectName + "' has no RenderingComponent"; return false; }
		std::vector<RenderingMesh*>& meshes = rc->GetMeshes(0);
		if (submeshIndex < 0 || (size_t)submeshIndex >= meshes.size())
			{ errOut = "submesh index " + std::to_string(submeshIndex) + " out of range (object has " + std::to_string(meshes.size()) + ")"; return false; }
		meshes[submeshIndex]->Material = mat;
		MarkSceneDirty();
		return true;
	}

	json SceneEditor::AgentSceneState()
	{
		json out;
		out["name"] = GetSceneDisplayName();
		out["scenePath"] = scenePath;
		out["dirty"] = sceneDirty;
		out["playing"] = playMode;

		std::vector<std::shared_ptr<GameObject>>& all = scene->GetAllGameObjectList();

		// Collect user objects (skip editor furniture).
		std::vector<GameObject*> order;
		std::map<GameObject*, size_t> idx;
		for (auto& goPtr : all)
		{
			GameObject* go = goPtr.get();
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
				json cj = AgentComponentToJson(c.get());
				if (cj.is_object()) comps.push_back(std::move(cj));
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

	std::string SceneEditor::AgentScreenshot()
	{
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
