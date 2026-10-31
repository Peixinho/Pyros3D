//============================================================================
// Name        : PyrosEmbind.cpp
// Description : Core Embind — Scene, GameObject, Projection, Renderer.
//               Other modules: Enums/Math/Render/Assets/Physics/Audio/Misc/UI.
//============================================================================

#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

#include <emscripten/bind.h>

#include <Pyros3D/Utils/Bindings/PyrosEmbindHelpers.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Physics/PhysicsEngines/IPhysics.h>
#include <Pyros3D/Physics/Components/IPhysicsComponent.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Renderer/SceneRenderer.h>
#include <Pyros3D/Rendering/Renderer/SceneSettings.h>
#include <Pyros3D/Utils/Bindings/PyrosEmbind.h>

#include <map>
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
	void PyrosEmbindAudioForceLink();
	void PyrosEmbindMiscForceLink();
	void PyrosEmbindUIForceLink();

	void EnsurePyrosEmbindLinked()
	{
		// Pull every Embind TU out of the static archive so EMSCRIPTEN_BINDINGS run.
		PyrosEmbindEnumsForceLink();
		PyrosEmbindMathForceLink();
		PyrosEmbindRenderForceLink();
		PyrosEmbindAssetsForceLink();
		PyrosEmbindPhysicsForceLink();
		PyrosEmbindAudioForceLink();
		PyrosEmbindMiscForceLink();
		PyrosEmbindUIForceLink();
	}
}

namespace {

	// ---- A scene made in the editor, read from the page ----------------
	//
	// SceneSerializer does not need Lua to read a scene (scripts are simply
	// not attached): the page gets the objects, the models and materials,
	// the lights, the physics bodies - and drives them itself. What one load
	// allocated is kept per scene, for unload().
	struct LoadedFromJS { LoadedSceneAssets assets; SceneMeta meta; };
	std::map<SceneGraph*, std::unique_ptr<LoadedFromJS> > &LoadedScenes()
	{
		static std::map<SceneGraph*, std::unique_ptr<LoadedFromJS> > all;
		return all;
	}
	LoadedFromJS* FreshLoad(SceneGraph &s)
	{
		std::unique_ptr<LoadedFromJS> &slot = LoadedScenes()[&s];
		if (slot) SceneSerializer::UnloadScene(&s, slot->assets);
		slot.reset(new LoadedFromJS());
		return slot.get();
	}
	bool Scene_Load(SceneGraph &s, const std::string &path, IPhysics* physics)
	{
		LoadedFromJS* l = FreshLoad(s);
		if (!SceneSerializer::LoadScene(&s, path, physics, NULL, &l->assets, &l->meta)) return false;
		// (components are known to the graph, and objects findable, only once it has updated)
		s.Update(s.GetTime());
		return true;
	}
	bool Scene_LoadText(SceneGraph &s, const std::string &json, const std::string &pathForAssets, IPhysics* physics)
	{
		LoadedFromJS* l = FreshLoad(s);
		if (!SceneSerializer::LoadSceneFromText(&s, json, pathForAssets, physics, NULL, &l->assets, &l->meta)) return false;
		s.Update(s.GetTime());
		return true;
	}
	void Scene_Unload(SceneGraph &s)
	{
		std::map<SceneGraph*, std::unique_ptr<LoadedFromJS> >::iterator it = LoadedScenes().find(&s);
		if (it == LoadedScenes().end()) return;
		SceneSerializer::UnloadScene(&s, it->second->assets);
		LoadedScenes().erase(it);
	}
	// What the scene file says about itself: { ambient, ambientIntensity, background, twoD }.
	emscripten::val Scene_Meta(SceneGraph &s)
	{
		emscripten::val o = emscripten::val::object();
		std::map<SceneGraph*, std::unique_ptr<LoadedFromJS> >::iterator it = LoadedScenes().find(&s);
		if (it == LoadedScenes().end()) return o;
		const SceneMeta &m = it->second->meta;
		o.set("ambient", m.ambientLight);
		o.set("ambientIntensity", m.ambientIntensity);
		o.set("background", m.background);
		o.set("twoD", m.twoD);
		return o;
	}
	std::shared_ptr<GameObject> FindIn(const std::vector<std::shared_ptr<GameObject> > &list, const std::string &name)
	{
		for (size_t i = 0; i < list.size(); i++)
		{
			if (!list[i]) continue;
			if (list[i]->GetName() == name) return list[i];
			std::shared_ptr<GameObject> below = FindIn(list[i]->GetChildren(), name);
			if (below) return below;
		}
		return std::shared_ptr<GameObject>();
	}
	// The first object of that name, anywhere in the scene (null if none).
	std::shared_ptr<GameObject> Scene_Find(SceneGraph &s, const std::string &name)
	{
		return FindIn(s.GetAllGameObjectList(), name);
	}
	emscripten::val ListOf(const std::vector<std::shared_ptr<GameObject> > &list)
	{
		emscripten::val a = emscripten::val::array();
		for (size_t i = 0; i < list.size(); i++) if (list[i]) a.call<void>("push", list[i]);
		return a;
	}
	emscripten::val Scene_Objects(SceneGraph &s) { return ListOf(s.GetAllGameObjectList()); }
	emscripten::val GameObject_Children(GameObject &go) { return ListOf(go.GetChildren()); }
	// A page cannot ask for a component by type the way Lua's getComponent
	// does; these are the kinds it most needs, each null if the object has none.
	std::shared_ptr<IPhysicsComponent> GameObject_Physics(GameObject &go)
	{
		const std::vector<std::shared_ptr<IComponent> > &c = go.GetComponents();
		for (size_t i = 0; i < c.size(); i++)
			if (std::shared_ptr<IPhysicsComponent> p = std::dynamic_pointer_cast<IPhysicsComponent>(c[i])) return p;
		return std::shared_ptr<IPhysicsComponent>();
	}
	std::shared_ptr<RenderingComponent> GameObject_Rendering(GameObject &go)
	{
		const std::vector<std::shared_ptr<IComponent> > &c = go.GetComponents();
		for (size_t i = 0; i < c.size(); i++)
			if (std::shared_ptr<RenderingComponent> p = std::dynamic_pointer_cast<RenderingComponent>(c[i])) return p;
		return std::shared_ptr<RenderingComponent>();
	}

	std::shared_ptr<GameObject> MakeGameObject()
	{
		return std::make_shared<GameObject>();
	}
	std::shared_ptr<GameObject> MakeGameObjectStatic(bool isStatic)
	{
		return std::make_shared<GameObject>(isStatic);
	}

} // namespace

namespace p3d {

	EmbindRenderer::EmbindRenderer(gfx::Device* device, const uint32 w, const uint32 h) : renderer(new p3d::SceneRenderer(device)), width(w), height(h)
	{
		renderer->SetName("Page");
	}
	EmbindRenderer::~EmbindRenderer() {}

	void EmbindRenderer::RenderScene(const Projection &projection, const std::shared_ptr<GameObject> &camera, SceneGraph &scene)
	{
		if (width == 0 || height == 0) return;
		GameObject::TakeDrawTransforms();
		p3d::View v;
		if (camera) v.cameraWorld = camera->GetDrawWorld();
		// (the projection as the page made it: perspective - its field of view - or orthographic, its height; told apart by
		// its matrix, the fields the other one sets left as they were)
		const bool ortho = projection.m.m[15] == 1.f;
		v.orthographic = ortho;
		if (ortho) { v.orthoHeight = std::max(projection.Top - projection.Bottom, 0.001f); v.nearZ = projection.Near; v.farZ = projection.Far; }
		else { v.fovY = projection.Fov > 0.f ? projection.Fov : 70.f; v.nearZ = projection.Near > 0.f ? projection.Near : 0.1f; }
		v.width = width; v.height = height;
		renderer->Render(&scene, v);
	}
	void EmbindRenderer::SetBackground(const Vec4 &c)
	{
		renderer->SetBackground(Vec3(powf(c.x, 2.2f), powf(c.y, 2.2f), powf(c.z, 2.2f)));
	}
	void EmbindRenderer::SetGlobalLight(const Vec4 &c)
	{
		renderer->SetAmbient(Vec3(powf(c.x, 2.2f), powf(c.y, 2.2f), powf(c.z, 2.2f)));
	}
	void EmbindRenderer::UseSceneSettings(SceneGraph &scene)
	{
		std::map<SceneGraph*, std::unique_ptr<LoadedFromJS> >::iterator it = LoadedScenes().find(&scene);
		if (it == LoadedScenes().end()) return;
		const SceneMeta &m = it->second->meta;
		p3d::ApplySceneSettings(*renderer, m.ambientLight, m.ambientIntensity, m.background, m.postEffects, m.twoD);
		p3d::ApplyAmbientEnvironment(*renderer, m.ambientMode, m.ambientSky, m.ambientEquator, m.ambientGround, m.ambientSH, m.ambientIntensity);
	}

}

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
		.function("getTime", &SceneGraph::GetTime)
		// A scene file made in the editor (in the module's file system - see
		// pyros-assets.js for fetching one and what it uses). Scripts are not run.
		.function("load", &Scene_Load, allow_raw_pointers())
		.function("loadText", &Scene_LoadText, allow_raw_pointers())
		.function("unload", &Scene_Unload)
		.function("getMeta", &Scene_Meta)
		.function("find", &Scene_Find)
		.function("getObjects", &Scene_Objects);

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
		.function("isStatic", &GameObject::IsStatic)
		.function("getChildren", &GameObject_Children)
		.function("getPhysicsComponent", &GameObject_Physics)
		.function("getRenderingComponent", &GameObject_Rendering);
		// onUpdate/onInit/onDestroy/attachScript — Lua-only

	class_<Projection>("Projection")
		.constructor<>()
		.function("perspective", &Projection::Perspective)
		.function("ortho", &Projection::Ortho)
		.function("getProjectionMatrix", &Projection::GetProjectionMatrix);

	class_<EmbindRenderer>("Renderer")
		.smart_ptr<std::shared_ptr<EmbindRenderer>>("RendererPtr")
		.function("resize", &EmbindRenderer::Resize)
		.function("renderScene", &EmbindRenderer::RenderScene)
		.function("setBackground", &EmbindRenderer::SetBackground)
		.function("setGlobalLight", &EmbindRenderer::SetGlobalLight)
		.function("useSceneSettings", &EmbindRenderer::UseSceneSettings)
		.function("setAutoExposure", optional_override([](EmbindRenderer &r, bool on, f32 ev) { r.Get()->SetAutoExposure(on, ev); }))
		.function("setExposure", optional_override([](EmbindRenderer &r, f32 e) { r.Get()->SetAutoExposure(false); r.Get()->SetExposure(e); }))
		.function("setSky", optional_override([](EmbindRenderer &r, bool on) { r.Get()->SetSky(on); }))
		.function("setHaze", optional_override([](EmbindRenderer &r, f32 density) { r.Get()->SetHaze(density); }))
		.function("setClouds", optional_override([](EmbindRenderer &r, f32 cover) { r.Get()->SetClouds(cover); }))
		.function("setReflections", optional_override([](EmbindRenderer &r, bool on) { r.Get()->SetReflections(on); }))
		.function("setTemporalAA", optional_override([](EmbindRenderer &r, bool on) { r.Get()->SetTemporalAA(on); }))
		.function("setToneCurve", optional_override([](EmbindRenderer &r, bool on) { r.Get()->SetToneCurve(on); }))
		.function("setFrustumCulling", optional_override([](EmbindRenderer &r, bool on) { r.Get()->SetFrustumCulling(on); }))
		.function("enableLOD", optional_override([](EmbindRenderer &r) { r.Get()->SetLOD(true); }))
		.function("disableLOD", optional_override([](EmbindRenderer &r) { r.Get()->SetLOD(false); }))
		.function("isUsingLOD", optional_override([](EmbindRenderer &r) { return r.Get()->GetLOD(); }));
}

#endif /* EMSCRIPTEN */
