//============================================================================
// Name        : PyrosLuaCore.cpp
// Description : Scene, GameObject, Projection, ForwardRenderer.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>

namespace p3d {
	// Descending the hierarchy from a script. getParent() has always existed;
	// its counterpart did not, which meant a script could walk up out of a
	// subtree and never back down into one. That is fine while every object
	// is a root, and stops being fine the moment layers exist - a Layer2D
	// root *is* a subtree, so without this a script can find a layer and
	// still not reach a single thing inside it.
	//
	// Returns a plain 1-based table rather than the vector so it is ipairs-able
	// on the Lua side like scene:getAllGameObjects() already is.
	static sol::table GameObject_GetChildren(GameObject* go, sol::this_state ts)
	{
		sol::state_view lua(ts);
		sol::table out = lua.create_table();
		if (go == NULL) return out;
		const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
		for (uint32 i = 0; i < kids.size(); i++)
			out[i + 1] = kids[i];
		return out;
	}

	// Depth-first by name, this object's subtree, excluding itself. The lookup
	// a script actually wants: the editor uniquifies names scene-wide, so an
	// exact match is safe here even though it is not for widgets.
	static sol::object GameObject_FindChild(GameObject* go, const std::string &name, sol::this_state ts)
	{
		sol::state_view lua(ts);
		if (go == NULL) return sol::make_object(lua, sol::nil);
		const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
		for (uint32 i = 0; i < kids.size(); i++)
		{
			if (kids[i] == NULL) continue;
			if (kids[i]->GetName() == name) return sol::make_object(lua, kids[i]);
			sol::object deep = GameObject_FindChild(kids[i].get(), name, ts);
			if (deep.valid() && deep != sol::nil) return deep;
		}
		return sol::make_object(lua, sol::nil);
	}


	void RegisterLuaCore(sol::state* lua)
	{
		{
			// SceneGraph
			lua->new_usertype<SceneGraph>("Scene",
				sol::constructors<sol::types<>>(),
				"update", &SceneGraph::Update,
				"add", &SceneGraph_AddObj,
				"remove", &SceneGraph_RemoveObj,
				"removeAll", &SceneGraph::RemoveAll,
				"addGameObject", &SceneGraph_AddGameObjectObj,
				"removeGameobject", &SceneGraph_RemoveGameObjectObj,
				"getTime", &SceneGraph::GetTime,
				"getAllGameObjects", &SceneGraph::GetAllGameObjectList,
				"save", [lua](SceneGraph &scene, const std::string &path) {
					return SceneSerializer::SaveScene(&scene, path, lua);
				},
				"load", sol::overload(
					[lua](SceneGraph &scene, const std::string &path) {
						return SceneSerializer::LoadScene(&scene, path, NULL, lua);
					},
					[lua](SceneGraph &scene, const std::string &path, IPhysics* physics) {
						return SceneSerializer::LoadScene(&scene, path, physics, lua);
					}
				)
				);
		};

		{
			// GameObject - shared_ptr via sol::factories (same pattern as AudioBus)
			lua->new_usertype<LUA_GameObject>("GameObject",
				sol::factories(
					[]() { return std::make_shared<LUA_GameObject>(); },
					[](bool isStatic) { return std::make_shared<LUA_GameObject>(isStatic); }
				),
				"init", &LUA_GameObject::Init,
				"update", &LUA_GameObject::Update,
				"destroy", &LUA_GameObject::Destroy,
				"getLocalTransformation", &LUA_GameObject::GetLocalTransformation,
				"getPosition", &LUA_GameObject::GetPosition,
				"getRotation", &LUA_GameObject::GetRotation,
				"getScale", &LUA_GameObject::GetScale,
				"getDirection", &LUA_GameObject::GetDirection,
				"getWorldTransformation", &LUA_GameObject::GetWorldTransformation,
				"getWorldPosition", &LUA_GameObject::GetWorldPosition,
				"getWorldRotation", &LUA_GameObject::GetWorldRotation,
				"setPosition", &LUA_GameObject::SetPosition,
				"setRotation", &LUA_GameObject::SetRotation,
				"setScale", &LUA_GameObject::SetScale,
				"getName", &LUA_GameObject::GetName,
				"setName", &LUA_GameObject::SetName,
				"setTransformationMatrix", &LUA_GameObject::SetTransformationMatrix,
				// SetPosition()/etc only flip a dirty flag - GetWorldPosition()
				// stays stale until something calls this (normally
				// SceneGraph::Update()'s own InternalUpdate() pass, which for a
				// dynamic object runs AFTER that same pass's component Update()
				// calls - see GameObject.h's comment). A script that
				// repositions an object and needs a component reading its
				// world position THIS SAME FRAME (e.g. an AudioSource
				// finite-differencing velocity, right after teleporting a
				// pooled object) must call this in between, or that read sees
				// last frame's position instead.
				"refreshTransformation", &LUA_GameObject::RefreshTransformation,
				"lookAtGameObject", &LUA_GameObject::LookAtGameObject,
				"lookAtVec", &LUA_GameObject::LookAtVec,
				"addComponent", &GameObject_AddComponentObj,
				"removeComponent", &GameObject_RemoveComponentObj,
				"addGameObject", &GameObject_AddGameObjectObj,
				"removeGameObject", &GameObject_RemoveGameObjectObj,
				"getParent", &LUA_GameObject::GetParent,
				"getChildren", &GameObject_GetChildren,
				"findChild", &GameObject_FindChild,
				"haveParent", &LUA_GameObject::HaveParent,
				"addTag", &LUA_GameObject::AddTag,
				"removeTag", &LUA_GameObject::RemoveTag,
				"haveTag", sol::overload(&GameObject_HaveTagSTR, &GameObject_HaveTagUINT),
				"isStatic", &LUA_GameObject::IsStatic,
				"onUpdate", &LUA_GameObject::on_update,
				"onInit", &LUA_GameObject::on_init,
				"onDestroy", &LUA_GameObject::on_destroy,
				// Convenience: load scriptFile (a .lua file returning a
				// middleclass class - see LuaComponent_FromFile's
				// comment), instantiate it, wrap it in a LuaComponent,
				// and attach it to this GameObject in one call - the
				// "drag a behavior script onto an object" pattern. Returns
				// the LuaComponent (nil if scriptFile didn't return a
				// usable class) so the caller can still reach `.data` if
				// needed.
				"attachScript", [lua](LUA_GameObject &go, const std::string &scriptFile) -> std::shared_ptr<LuaComponent> {
					auto comp = LuaComponent_FromFile(*lua, scriptFile);
					if (comp) go.AddComponent(comp);
					return comp;
				},
				"getComponent", &GameObject_GetComponent,
				sol::base_classes, sol::bases<GameObject>()
				);
		}

		{
			// GameObject (base) - registered separately from LUA_GameObject
			// above. sol2 selects push/index behavior from a pushed
			// value's STATIC C++ type, not its runtime type, so a plain
			// GameObject* (e.g. IComponent::GetOwner()'s declared return
			// type - the case for every attachScript()'d script's
			// self.owner) always resolved via this type even when the
			// actual object is a LUA_GameObject. With only LUA_GameObject
			// registered, GameObject* had no known metatable and sol2
			// fell back to an empty, method-less userdata wrapper -
			// "attempt to index a sol.p3d::GameObject * value" on every
			// self.owner:method() call. Mirrors the base-only subset of
			// LUA_GameObject's bindings (excludes the Lua-scripting-only
			// hooks onUpdate/onInit/onDestroy/attachScript, which have no
			// base-class equivalent - getComponent is included since it
			// only needs GameObject::GetComponents(), shared via
			// GameObject_GetComponent above).
			lua->new_usertype<GameObject>("GameObjectBase",
				"getLocalTransformation", &GameObject::GetLocalTransformation,
				"getPosition", &GameObject::GetPosition,
				"getRotation", &GameObject::GetRotation,
				"getScale", &GameObject::GetScale,
				"getDirection", &GameObject::GetDirection,
				"getWorldTransformation", &GameObject::GetWorldTransformation,
				"getWorldPosition", &GameObject::GetWorldPosition,
				"getWorldRotation", &GameObject::GetWorldRotation,
				"setPosition", &GameObject::SetPosition,
				"setRotation", &GameObject::SetRotation,
				"setScale", &GameObject::SetScale,
				"getName", &GameObject::GetName,
				"setName", &GameObject::SetName,
				"setTransformationMatrix", &GameObject::SetTransformationMatrix,
				// See LUA_GameObject's identical binding above for why this
				// exists.
				"refreshTransformation", &GameObject::RefreshTransformation,
				"lookAtGameObject", &GameObject::LookAtGameObject,
				"lookAtVec", &GameObject::LookAtVec,
				"addComponent", &GameObject_AddComponentObj,
				"removeComponent", &GameObject_RemoveComponentObj,
				"addGameObject", &GameObject_AddGameObjectObj,
				"removeGameObject", &GameObject_RemoveGameObjectObj,
				"getParent", &GameObject::GetParent,
				"getChildren", &GameObject_GetChildren,
				"findChild", &GameObject_FindChild,
				"haveParent", &GameObject::HaveParent,
				"addTag", &GameObject::AddTag,
				"removeTag", &GameObject::RemoveTag,
				"haveTag", sol::overload(&GameObject_HaveTagSTR, &GameObject_HaveTagUINT),
				"isStatic", &GameObject::IsStatic,
				"getComponent", &GameObject_GetComponent
				);
			lua->set_function("asGameObject", &AsGameObject);
		}

		{
			// Projection
			lua->new_usertype<Projection>("Projection",
				"perspective", &Projection::Perspective,
				"ortho", &Projection::Ortho,
				"getProjectionMatrix", &Projection::GetProjectionMatrix
				);
		};

		{
			// ForwardRenderer
			sol::constructors<sol::types<float, float>> con;
			lua->new_usertype<ForwardRenderer>("ForwardRenderer",
				con,
				"clearBufferBit", &ForwardRenderer_ClearBufferBit,
				"enableClearDepthBuffer", &ForwardRenderer::EnableClearDepthBuffer,
				"disableClearDepthBuffer", &ForwardRenderer::DisableClearDepthBuffer,
				"clearDepthBuffer", &ForwardRenderer::ClearDepthBuffer,
				// EnableClipPlane takes const uint32& with a C++ default -
				// sol drops/mangles that and Lua's enableClipPlane(1) then
				// throws "expected number, received no value". Bind by-value
				// wrappers (with a no-arg default) instead.
				"enableClipPlane", sol::overload(&ForwardRenderer_EnableClipPlane, &ForwardRenderer_EnableClipPlaneDefault),
				"disableClipPlane", &ForwardRenderer::DisableClipPlane,
				"setClipPlane0", &ForwardRenderer::SetClipPlane0,
				"setClipaPlane0", &ForwardRenderer::SetClipPlane0,
				"setClipaPlane1", &ForwardRenderer::SetClipPlane1,
				"setClipaPlane2", &ForwardRenderer::SetClipPlane2,
				"setClipaPlane3", &ForwardRenderer::SetClipPlane3,
				"setClipaPlane4", &ForwardRenderer::SetClipPlane4,
				"setClipaPlane5", &ForwardRenderer::SetClipPlane5,
				"setClipaPlane6", &ForwardRenderer::SetClipPlane6,
				"setClipaPlane7", &ForwardRenderer::SetClipPlane7,
				"enableStencil", &ForwardRenderer::EnableStencil,
				"disableStencil", &ForwardRenderer::DisableStencil,
				"clearStencilBuffer", &ForwardRenderer::ClearStencilBuffer,
				"stencilFunction", &ForwardRenderer::StencilFunction,
				"stencilOperation", &ForwardRenderer::StencilOperation,
				"enableScissorTest", &ForwardRenderer::EnableScissorTest,
				"disableScissorTest", &ForwardRenderer::DisableScissorTest,
				"scissorTestRect", &ForwardRenderer::ScissorTestRect,
				"enableWireFrame", &ForwardRenderer::EnableWireFrame,
				"disableWireFrame", &ForwardRenderer::DisableWireFrame,
				"colorMask", &ForwardRenderer::ColorMask,
				"enableSorting", &ForwardRenderer::EnableSorting,
				"disableSorting", &ForwardRenderer::DisableSorting,
				"enableLOD", &ForwardRenderer::EnableLOD,
				"disableLOD", &ForwardRenderer::DisableLOD,
				"isUsingLOD", &ForwardRenderer::IsUsingLOD,
				"setBackground", &ForwardRenderer::SetBackground,
				"unsetBackground", &ForwardRenderer::UnsetBackground,
				"setGlobalLight", &ForwardRenderer::SetGlobalLight,
				"enableDepthBias", &ForwardRenderer::EnableDepthBias,
				"disableDepthBias", &ForwardRenderer::DisableDepthBias,
				"setViewPort", &ForwardRenderer::SetViewPort,
				"resetViewPort", &ForwardRenderer::ResetViewPort,
				"resize", &ForwardRenderer::Resize,
				"activateCulling", &ForwardRenderer::ActivateCulling,
				"deactivateCulling", &ForwardRenderer::DeactivateCulling,
				"renderScene", &ForwardRenderer_RenderScene,
				"preRender", sol::overload(&ForwardRenderer_PreRender, &ForwardRenderer_PreRenderTag)
				);
		}

	}

} // namespace p3d

#endif
