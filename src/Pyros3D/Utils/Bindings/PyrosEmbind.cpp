//============================================================================
// Name        : PyrosEmbind.cpp
// Description : Core Embind — Scene, GameObject, Projection, ForwardRenderer.
//               Other modules: Enums/Math/Render/Assets/Physics/PostFX/Audio/Misc.
//============================================================================

#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

#include <emscripten/bind.h>

#include <Pyros3D/Utils/Bindings/PyrosEmbindHelpers.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Components/IComponent.h>

#include <memory>

using namespace emscripten;
using namespace p3d;
using namespace p3d::Math;
using namespace p3d::embind_helpers;

namespace p3d {
	void PyrosEmbindEnumsForceLink();
	void PyrosEmbindMathForceLink();
	void PyrosEmbindRenderForceLink();
	void PyrosEmbindAssetsForceLink();
	void PyrosEmbindPhysicsForceLink();
	void PyrosEmbindPostFXForceLink();
	void PyrosEmbindAudioForceLink();
	void PyrosEmbindMiscForceLink();

	void EnsurePyrosEmbindLinked()
	{
		// Pull every Embind TU out of the static archive so EMSCRIPTEN_BINDINGS run.
		PyrosEmbindEnumsForceLink();
		PyrosEmbindMathForceLink();
		PyrosEmbindRenderForceLink();
		PyrosEmbindAssetsForceLink();
		PyrosEmbindPhysicsForceLink();
		PyrosEmbindPostFXForceLink();
		PyrosEmbindAudioForceLink();
		PyrosEmbindMiscForceLink();
	}
}

namespace {

	std::shared_ptr<GameObject> MakeGameObject()
	{
		return std::make_shared<GameObject>();
	}
	std::shared_ptr<GameObject> MakeGameObjectStatic(bool isStatic)
	{
		return std::make_shared<GameObject>(isStatic);
	}

	void ForwardRenderer_PreRender(ForwardRenderer &r, const std::shared_ptr<GameObject> &cam, SceneGraph &scene)
	{
		r.PreRender(cam.get(), &scene);
	}
	void ForwardRenderer_PreRenderTag(ForwardRenderer &r, const std::shared_ptr<GameObject> &cam, SceneGraph &scene, const std::string &tag)
	{
		r.PreRender(cam.get(), &scene, tag);
	}
	void ForwardRenderer_RenderScene(ForwardRenderer &r, const Projection &proj, const std::shared_ptr<GameObject> &cam, SceneGraph &scene)
	{
		r.RenderScene(proj, cam.get(), &scene);
	}
	void ForwardRenderer_ClearBufferBit(ForwardRenderer &r, uint32 option) { r.ClearBufferBit(option); }
	void ForwardRenderer_EnableClipPlane(ForwardRenderer &r, uint32 n) { r.EnableClipPlane(n); }
	void ForwardRenderer_EnableClipPlaneDefault(ForwardRenderer &r) { r.EnableClipPlane(1); }
	void ForwardRenderer_SetBackground(ForwardRenderer &r, const Vec4 &color) { r.SetBackground(color); }
	void ForwardRenderer_UnsetBackground(ForwardRenderer &r) { r.UnsetBackground(); }
	void ForwardRenderer_SetGlobalLight(ForwardRenderer &r, const Vec4 &light) { r.SetGlobalLight(light); }
	void ForwardRenderer_Resize(ForwardRenderer &r, uint32 w, uint32 h) { r.Resize(w, h); }
	void ForwardRenderer_EnableClearDepthBuffer(ForwardRenderer &r) { r.EnableClearDepthBuffer(); }
	void ForwardRenderer_DisableClearDepthBuffer(ForwardRenderer &r) { r.DisableClearDepthBuffer(); }
	void ForwardRenderer_ClearDepthBuffer(ForwardRenderer &r) { r.ClearDepthBuffer(); }
	void ForwardRenderer_DisableClipPlane(ForwardRenderer &r) { r.DisableClipPlane(); }
	void ForwardRenderer_SetClipPlane0(ForwardRenderer &r, const Vec4 &p) { r.SetClipPlane0(p); }
	void ForwardRenderer_SetClipPlane1(ForwardRenderer &r, const Vec4 &p) { r.SetClipPlane1(p); }
	void ForwardRenderer_SetClipPlane2(ForwardRenderer &r, const Vec4 &p) { r.SetClipPlane2(p); }
	void ForwardRenderer_SetClipPlane3(ForwardRenderer &r, const Vec4 &p) { r.SetClipPlane3(p); }
	void ForwardRenderer_SetClipPlane4(ForwardRenderer &r, const Vec4 &p) { r.SetClipPlane4(p); }
	void ForwardRenderer_SetClipPlane5(ForwardRenderer &r, const Vec4 &p) { r.SetClipPlane5(p); }
	void ForwardRenderer_SetClipPlane6(ForwardRenderer &r, const Vec4 &p) { r.SetClipPlane6(p); }
	void ForwardRenderer_SetClipPlane7(ForwardRenderer &r, const Vec4 &p) { r.SetClipPlane7(p); }
	void ForwardRenderer_EnableStencil(ForwardRenderer &r) { r.EnableStencil(); }
	void ForwardRenderer_DisableStencil(ForwardRenderer &r) { r.DisableStencil(); }
	void ForwardRenderer_ClearStencilBuffer(ForwardRenderer &r) { r.ClearStencilBuffer(); }
	void ForwardRenderer_StencilFunction(ForwardRenderer &r, uint32 func, uint32 ref, uint32 mask) { r.StencilFunction(func, ref, mask); }
	void ForwardRenderer_StencilOperation(ForwardRenderer &r, uint32 sfail, uint32 dpfail, uint32 dppass) { r.StencilOperation(sfail, dpfail, dppass); }
	void ForwardRenderer_EnableScissorTest(ForwardRenderer &r) { r.EnableScissorTest(); }
	void ForwardRenderer_DisableScissorTest(ForwardRenderer &r) { r.DisableScissorTest(); }
	void ForwardRenderer_ScissorTestRect(ForwardRenderer &r, const f32 x, const f32 y, const f32 width, const f32 height) { r.ScissorTestRect(x, y, width, height); }
	void ForwardRenderer_EnableWireFrame(ForwardRenderer &r) { r.EnableWireFrame(); }
	void ForwardRenderer_DisableWireFrame(ForwardRenderer &r) { r.DisableWireFrame(); }
	void ForwardRenderer_ColorMask(ForwardRenderer &r, bool rmask, bool g, bool b, bool a) { r.ColorMask(rmask, g, b, a); }
	void ForwardRenderer_EnableSorting(ForwardRenderer &r) { r.EnableSorting(); }
	void ForwardRenderer_DisableSorting(ForwardRenderer &r) { r.DisableSorting(); }
	void ForwardRenderer_EnableLOD(ForwardRenderer &r) { r.EnableLOD(); }
	void ForwardRenderer_DisableLOD(ForwardRenderer &r) { r.DisableLOD(); }
	bool ForwardRenderer_IsUsingLOD(ForwardRenderer &r) { return r.IsUsingLOD(); }
	void ForwardRenderer_EnableDepthBias(ForwardRenderer &r, const Vec2 &factor) { r.EnableDepthBias(factor); }
	void ForwardRenderer_DisableDepthBias(ForwardRenderer &r) { r.DisableDepthBias(); }
	void ForwardRenderer_SetViewPort(ForwardRenderer &r, uint32 initX, uint32 initY, uint32 endX, uint32 endY) { r.SetViewPort(initX, initY, endX, endY); }
	void ForwardRenderer_ResetViewPort(ForwardRenderer &r) { r.ResetViewPort(); }
	void ForwardRenderer_ActivateCulling(ForwardRenderer &r, const uint32 mode) { r.ActivateCulling(mode); }
	void ForwardRenderer_DeactivateCulling(ForwardRenderer &r) { r.DeactivateCulling(); }

} // namespace

EMSCRIPTEN_BINDINGS(pyros3d_core)
{
	class_<IComponent>("IComponent")
		.smart_ptr<std::shared_ptr<IComponent>>("IComponentPtr")
		.function("getOwner", &IComponent::GetOwner, allow_raw_pointers())
		.function("enable", &IComponent::Enable)
		.function("disable", &IComponent::Disable)
		.function("isActive", &IComponent::IsActive);

	class_<SceneGraph>("Scene")
		.constructor<>()
		.function("update", &SceneGraph::Update)
		.function("add", &Scene_Add)
		.function("remove", &Scene_Remove)
		.function("addGameObject", &Scene_AddGameObject)
		.function("removeGameobject", &Scene_RemoveGameObject)
		.function("removeAll", &SceneGraph::RemoveAll)
		.function("getTime", &SceneGraph::GetTime);
		// save/load skipped — require Lua state (SceneSerializer)

	class_<GameObject>("GameObject")
		.smart_ptr<std::shared_ptr<GameObject>>("GameObjectPtr")
		.constructor(&MakeGameObject)
		.constructor(&MakeGameObjectStatic)
		.function("init", &GameObject::Init)
		.function("update", &GameObject::Update)
		.function("destroy", &GameObject::Destroy)
		.function("setPosition", &GameObject::SetPosition)
		.function("setRotation", &GameObject::SetRotation)
		.function("setScale", &GameObject::SetScale)
		.function("getPosition", &GameObject::GetPosition)
		.function("getRotation", &GameObject::GetRotation)
		.function("getScale", &GameObject::GetScale)
		.function("getDirection", &GameObject::GetDirection)
		.function("getLocalTransformation", &GameObject::GetLocalTransformation)
		.function("getWorldTransformation", &GameObject::GetWorldTransformation)
		.function("getWorldPosition", &GameObject::GetWorldPosition)
		.function("getWorldRotation", &GameObject::GetWorldRotation)
		.function("setTransformationMatrix", &GameObject::SetTransformationMatrix)
		.function("lookAt", select_overload<void(const Vec3 &)>(&GameObject::LookAt))
		.function("lookAtGameObject", &GameObject::LookAtGameObject, allow_raw_pointers())
		.function("lookAtVec", &GameObject::LookAtVec)
		.function("refreshTransformation", &GameObject::RefreshTransformation)
		.function("setName", &GameObject::SetName)
		.function("getName", &GameObject_GetName)
		.function("addComponent", &GameObject_AddComponent)
		.function("removeComponent", &GameObject_RemoveComponent)
		.function("addRenderingComponent", &GameObject_AddRenderingComponent)
		.function("addDirectionalLight", &GameObject_AddDirectionalLight)
		.function("addPointLight", &GameObject_AddPointLight)
		.function("addSpotLight", &GameObject_AddSpotLight)
		.function("addParticleSystem", &GameObject_AddParticleSystem)
		.function("addPhysicsComponent", &GameObject_AddPhysicsComponent)
		.function("addAudioSource", &GameObject_AddAudioSource)
		.function("addGameObject", &GameObject_AddChild)
		.function("removeGameObject", &GameObject_RemoveChild)
		.function("getParent", &GameObject::GetParent, allow_raw_pointers())
		.function("haveParent", &GameObject::HaveParent)
		.function("addTag", &GameObject::AddTag)
		.function("removeTag", &GameObject::RemoveTag)
		.function("haveTag", &GameObject_HaveTagStr)
		.function("haveTagId", &GameObject_HaveTagUint)
		.function("isStatic", &GameObject::IsStatic);
		// onUpdate/onInit/onDestroy/attachScript/getComponent — Lua-only

	class_<Projection>("Projection")
		.constructor<>()
		.function("perspective", &Projection::Perspective)
		.function("ortho", &Projection::Ortho)
		.function("getProjectionMatrix", &Projection::GetProjectionMatrix);

	class_<ForwardRenderer>("ForwardRenderer")
		.constructor<const uint32, const uint32>()
		.function("clearBufferBit", &ForwardRenderer_ClearBufferBit)
		.function("enableClearDepthBuffer", &ForwardRenderer_EnableClearDepthBuffer)
		.function("disableClearDepthBuffer", &ForwardRenderer_DisableClearDepthBuffer)
		.function("clearDepthBuffer", &ForwardRenderer_ClearDepthBuffer)
		.function("enableClipPlane", &ForwardRenderer_EnableClipPlane)
		.function("enableClipPlaneDefault", &ForwardRenderer_EnableClipPlaneDefault)
		.function("disableClipPlane", &ForwardRenderer_DisableClipPlane)
		.function("setClipPlane0", &ForwardRenderer_SetClipPlane0)
		.function("setClipaPlane0", &ForwardRenderer_SetClipPlane0)
		.function("setClipaPlane1", &ForwardRenderer_SetClipPlane1)
		.function("setClipaPlane2", &ForwardRenderer_SetClipPlane2)
		.function("setClipaPlane3", &ForwardRenderer_SetClipPlane3)
		.function("setClipaPlane4", &ForwardRenderer_SetClipPlane4)
		.function("setClipaPlane5", &ForwardRenderer_SetClipPlane5)
		.function("setClipaPlane6", &ForwardRenderer_SetClipPlane6)
		.function("setClipaPlane7", &ForwardRenderer_SetClipPlane7)
		.function("enableStencil", &ForwardRenderer_EnableStencil)
		.function("disableStencil", &ForwardRenderer_DisableStencil)
		.function("clearStencilBuffer", &ForwardRenderer_ClearStencilBuffer)
		.function("stencilFunction", &ForwardRenderer_StencilFunction)
		.function("stencilOperation", &ForwardRenderer_StencilOperation)
		.function("enableScissorTest", &ForwardRenderer_EnableScissorTest)
		.function("disableScissorTest", &ForwardRenderer_DisableScissorTest)
		.function("scissorTestRect", &ForwardRenderer_ScissorTestRect)
		.function("enableWireFrame", &ForwardRenderer_EnableWireFrame)
		.function("disableWireFrame", &ForwardRenderer_DisableWireFrame)
		.function("colorMask", &ForwardRenderer_ColorMask)
		.function("enableSorting", &ForwardRenderer_EnableSorting)
		.function("disableSorting", &ForwardRenderer_DisableSorting)
		.function("enableLOD", &ForwardRenderer_EnableLOD)
		.function("disableLOD", &ForwardRenderer_DisableLOD)
		.function("isUsingLOD", &ForwardRenderer_IsUsingLOD)
		.function("setBackground", &ForwardRenderer_SetBackground)
		.function("unsetBackground", &ForwardRenderer_UnsetBackground)
		.function("setGlobalLight", &ForwardRenderer_SetGlobalLight)
		.function("enableDepthBias", &ForwardRenderer_EnableDepthBias)
		.function("disableDepthBias", &ForwardRenderer_DisableDepthBias)
		.function("setViewPort", &ForwardRenderer_SetViewPort)
		.function("resetViewPort", &ForwardRenderer_ResetViewPort)
		.function("resize", &ForwardRenderer_Resize)
		.function("activateCulling", &ForwardRenderer_ActivateCulling)
		.function("deactivateCulling", &ForwardRenderer_DeactivateCulling)
		.function("preRender", &ForwardRenderer_PreRender)
		.function("preRenderTag", &ForwardRenderer_PreRenderTag)
		.function("renderScene", &ForwardRenderer_RenderScene);
}

#endif /* EMSCRIPTEN */
