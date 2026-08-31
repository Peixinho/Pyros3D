//============================================================================
// Name        : PyrosLuaHelpers.cpp
// Description : Shared free functions used by split Lua binding modules.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>
#include <Pyros3D/AnimationManager/Components/IKComponent.h>
#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <Pyros3D/Rendering/Components/Layer2D/Layer2D.h>

namespace p3d {


	// ******************************* OVERLOADS *******************************
	// Vec2
	Vec2 Vec2_operator_add(Vec2 &v1, const Vec2 &v2)
	{
		return v1 + v2;
	}
	Vec2 Vec2_operator_sub(Vec2 &v1, const Vec2 &v2)
	{
		return v1 - v2;
	}
	Vec2 Vec2_operator_mul(Vec2 &v1, const Vec2 &v2)
	{
		return v1 * v2;
	}
	Vec2 Vec2_operator_div(Vec2 &v1, const Vec2 &v2)
	{
		return v1 / v2;
	}
	Vec2 Vec2_operator_addS(Vec2 &v1, float f)
	{
		return v1 + f;
	}
	Vec2 Vec2_operator_subS(Vec2 &v1, float f)
	{
		return v1 - f;
	}
	Vec2 Vec2_operator_mulS(Vec2 &v1, float f)
	{
		return v1 * f;
	}
	Vec2 Vec2_operator_divS(Vec2 &v1, float f)
	{
		return v1 / f;
	}
	void Vec2_operator_Eadd(Vec2 &v1, const Vec2 &v2)
	{
		return v1 += v2;
	}
	void Vec2_operator_Esub(Vec2 &v1, const Vec2 &v2)
	{
		return v1 -= v2;
	}
	void Vec2_operator_Emul(Vec2 &v1, const Vec2 &v2)
	{
		return v1 *= v2;
	}
	void Vec2_operator_Ediv(Vec2 &v1, const Vec2 &v2)
	{
		return v1 /= v2;
	}
	void Vec2_operator_EaddS(Vec2 &v1, float f)
	{
		return v1 += f;
	}
	void Vec2_operator_EsubS(Vec2 &v1, float f)
	{
		return v1 -=  f;
	}
	void Vec2_operator_EmulS(Vec2 &v1, float f)
	{
		return v1 *= f;
	}
	void Vec2_operator_EdivS(Vec2 &v1, float f)
	{
		v1 /= f;
	}

	// Vec3
	Vec3 Vec3_operator_add(Vec3 &v1, const Vec3 &v2)
	{
		return v1 + v2;
	}
	Vec3 Vec3_operator_sub(Vec3 &v1, const Vec3 &v2)
	{
		return v1 - v2;
	}
	Vec3 Vec3_operator_mul(Vec3 &v1, const Vec3 &v2)
	{
		return v1 * v2;
	}
	Vec3 Vec3_operator_div(Vec3 &v1, const Vec3 &v2)
	{
		return v1 / v2;
	}
	Vec3 Vec3_operator_addS(Vec3 &v1, float f)
	{
		return v1 + f;
	}
	Vec3 Vec3_operator_subS(Vec3 &v1, float f)
	{
		return v1 - f;
	}
	Vec3 Vec3_operator_mulS(Vec3 &v1, float f)
	{
		return v1 * f;
	}
	Vec3 Vec3_operator_divS(Vec3 &v1, float f)
	{
		return v1 / f;
	}
	void Vec3_operator_Eadd(Vec3 &v1, const Vec3 &v2)
	{
		return v1 += v2;
	}
	void Vec3_operator_Esub(Vec3 &v1, const Vec3 &v2)
	{
		return v1 -= v2;
	}
	void Vec3_operator_Emul(Vec3 &v1, const Vec3 &v2)
	{
		return v1 *= v2;
	}
	void Vec3_operator_Ediv(Vec3 &v1, const Vec3 &v2)
	{
		return v1 /= v2;
	}
	void Vec3_operator_EaddS(Vec3 &v1, float f)
	{
		return v1 += f;
	}
	void Vec3_operator_EsubS(Vec3 &v1, float f)
	{
		return v1 -= f;
	}
	void Vec3_operator_EmulS(Vec3 &v1, float f)
	{
		return v1 *= f;
	}
	void Vec3_operator_EdivS(Vec3 &v1, float f)
	{
		v1 /= f;
	}

	// Vec4
	Vec4 Vec4_operator_add(Vec4 &v1, const Vec4 &v2)
	{
		return v1 + v2;
	}
	Vec4 Vec4_operator_sub(Vec4 &v1, const Vec4 &v2)
	{
		return v1 - v2;
	}
	Vec4 Vec4_operator_mul(Vec4 &v1, const Vec4 &v2)
	{
		return v1 * v2;
	}
	Vec4 Vec4_operator_div(Vec4 &v1, const Vec4 &v2)
	{
		return v1 / v2;
	}
	Vec4 Vec4_operator_addS(Vec4 &v1, float f)
	{
		return v1 + f;
	}
	Vec4 Vec4_operator_subS(Vec4 &v1, float f)
	{
		return v1 - f;
	}
	Vec4 Vec4_operator_mulS(Vec4 &v1, float f)
	{
		return v1 * f;
	}
	Vec4 Vec4_operator_divS(Vec4 &v1, float f)
	{
		return v1 / f;
	}
	void Vec4_operator_Eadd(Vec4 &v1, const Vec4 &v2)
	{
		return v1 += v2;
	}
	void Vec4_operator_Esub(Vec4 &v1, const Vec4 &v2)
	{
		return v1 -= v2;
	}
	void Vec4_operator_Emul(Vec4 &v1, const Vec4 &v2)
	{
		return v1 *= v2;
	}
	void Vec4_operator_Ediv(Vec4 &v1, const Vec4 &v2)
	{
		return v1 /= v2;
	}
	void Vec4_operator_EaddS(Vec4 &v1, float f)
	{
		return v1 += f;
	}
	void Vec4_operator_EsubS(Vec4 &v1, float f)
	{
		return v1 -= f;
	}
	void Vec4_operator_EmulS(Vec4 &v1, float f)
	{
		return v1 *= f;
	}
	void Vec4_operator_EdivS(Vec4 &v1, float f)
	{
		v1 /= f;
	}

	// Matrix
	Matrix Matrix_operator_mul(Matrix &m1, const Matrix &m2)
	{
		return m1 * m2;
	}
	Matrix Matrix_operator_mulS(Matrix &m, const f32 f) 
	{
		return m * f;
	}
	Vec3 Matrix_operator_mulVec3(Matrix &m, const Vec3 &v)
	{
		return m * v;
	}
	Vec4 Matrix_operator_mulVec4(Matrix &m, const Vec4 &v)
	{
		return m * v;
	}
	void Matrix_operator_Emul(Matrix &m1, const Matrix &m2)
	{
		m1 *= m2;
	}
	void Matrix_lookAt(Math::Matrix &m, const Math::Vec3 &eye, const Math::Vec3 &center, const Math::Vec3 &up)
	{
		m.LookAt(eye, center, up);
	}
	void Matrix_lookAt2(Math::Matrix &m, const Math::Vec3 &eye, const Math::Vec3 &center)
	{
		m.LookAt(eye, center);
	}
	void Matrix_translateXYZ(Math::Matrix &m, float x, float y, float z)
	{
		m.Translate(x, y, z);
	}
	void Matrix_translateVec3(Math::Matrix &m, const Math::Vec3 &v)
	{
		m.Translate(v);
	}
	void Matrix_scaleXYZ(Math::Matrix &m, float x, float y, float z)
	{
		m.Scale(x, y, z);
	}
	void Matrix_scaleVec3(Math::Matrix &m, const Math::Vec3 &v)
	{
		m.Scale(v);
	}
	// Quaternion
	Quaternion Quaternion_operator_mul(Quaternion &q1, const Quaternion &q2) 
	{
		return q1 * q2;
	}
	Quaternion Quaternion_operator_mulS(Quaternion &q, const f32 s)
	{
		return q * s;
	}
	Vec3 Quaternion_operator_mulVec3(Quaternion &q, const Vec3 &v)
	{
		return q * v;
	}
	Quaternion Quaternion_operator_negate(Quaternion &q)
	{
		return -q;
	}

	// SceneGraph - save/load, see Utils/Serialization/SceneSerializer.h.
	// Bound as lambdas (not plain free functions) at the usertype
	// registration site below - both need `lua` itself (to save/restore
	// real LuaComponent behavior), only available inside GenerateBindings.
	// GameObject
	bool GameObject_HaveTagSTR(GameObject &g, const std::string &tag)
	{
		return g.HaveTag(tag);
	}
	bool GameObject_HaveTagUINT(GameObject &g, const uint32 tag)
	{
		return g.HaveTag(tag);
	}

	// sol's shared_ptr<Derived> -> shared_ptr<IComponent> conversion is
	// unreliable across the engine dylib / DemoLauncher exe boundary on
	// macOS GL (typeinfo for LUA_* classes lives in the header and ends
	// up duplicated). Same pattern as LuaObjectToRenderable: pull the
	// concrete userdata out of sol::object and static_pointer_cast.
	std::shared_ptr<IComponent> LuaObjectToComponent(const sol::object &o)
	{
		if (!o.valid()) return nullptr;
		if (o.is<std::shared_ptr<LUA_RenderingComponent>>())
			return std::static_pointer_cast<IComponent>(o.as<std::shared_ptr<LUA_RenderingComponent>>());
		if (o.is<std::shared_ptr<RenderingComponent>>())
			return std::static_pointer_cast<IComponent>(o.as<std::shared_ptr<RenderingComponent>>());
		if (o.is<std::shared_ptr<LUA_RenderingInstancedComponent>>())
			return std::static_pointer_cast<IComponent>(o.as<std::shared_ptr<LUA_RenderingInstancedComponent>>());
		if (o.is<std::shared_ptr<LUA_DirectionalLight>>())
			return std::static_pointer_cast<IComponent>(o.as<std::shared_ptr<LUA_DirectionalLight>>());
		if (o.is<std::shared_ptr<DirectionalLight>>())
			return std::static_pointer_cast<IComponent>(o.as<std::shared_ptr<DirectionalLight>>());
		if (o.is<std::shared_ptr<LUA_PointLight>>())
			return std::static_pointer_cast<IComponent>(o.as<std::shared_ptr<LUA_PointLight>>());
		if (o.is<std::shared_ptr<PointLight>>())
			return std::static_pointer_cast<IComponent>(o.as<std::shared_ptr<PointLight>>());
		if (o.is<std::shared_ptr<LUA_SpotLight>>())
			return std::static_pointer_cast<IComponent>(o.as<std::shared_ptr<LUA_SpotLight>>());
		if (o.is<std::shared_ptr<SpotLight>>())
			return std::static_pointer_cast<IComponent>(o.as<std::shared_ptr<SpotLight>>());
		if (o.is<std::shared_ptr<LuaComponent>>())
			return std::static_pointer_cast<IComponent>(o.as<std::shared_ptr<LuaComponent>>());
		if (o.is<std::shared_ptr<ParticleSystem>>())
			return std::static_pointer_cast<IComponent>(o.as<std::shared_ptr<ParticleSystem>>());
		if (o.is<std::shared_ptr<AudioSource>>())
			return std::static_pointer_cast<IComponent>(o.as<std::shared_ptr<AudioSource>>());
		if (o.is<std::shared_ptr<PhysicsVehicle>>())
			return std::static_pointer_cast<IComponent>(o.as<std::shared_ptr<PhysicsVehicle>>());
		if (o.is<std::shared_ptr<IPhysicsComponent>>())
			return std::static_pointer_cast<IComponent>(o.as<std::shared_ptr<IPhysicsComponent>>());
		if (o.is<std::shared_ptr<IComponent>>())
			return o.as<std::shared_ptr<IComponent>>();
		return nullptr;
	}

	void GameObject_AddComponentObj(GameObject &go, sol::object compObj)
	{
		std::shared_ptr<IComponent> c = LuaObjectToComponent(compObj);
		if (!c)
			throw std::runtime_error("GameObject:addComponent: argument is not a Component");
		go.AddComponent(c);
	}
	void GameObject_RemoveComponentObj(GameObject &go, sol::object compObj)
	{
		std::shared_ptr<IComponent> c = LuaObjectToComponent(compObj);
		if (!c)
			throw std::runtime_error("GameObject:removeComponent: argument is not a Component");
		go.RemoveComponent(c);
	}

	// Same shared_ptr<Derived>→shared_ptr<Base> hole as components: GameObject.new()
	// returns shared_ptr<LUA_GameObject>, but Scene:add / addGameObject take
	// shared_ptr<GameObject>. sol will not convert those; without an explicit
	// cast, Lua spawn demos die on scene:add(go) with
	// "unrecognized userdata ... LUA_GameObject ... shared_ptr<GameObject>".
	std::shared_ptr<GameObject> LuaObjectToGameObject(const sol::object &o)
	{
		if (!o.valid()) return nullptr;
		if (o.is<std::shared_ptr<LUA_GameObject>>())
			return std::static_pointer_cast<GameObject>(o.as<std::shared_ptr<LUA_GameObject>>());
		if (o.is<std::shared_ptr<GameObject>>())
			return o.as<std::shared_ptr<GameObject>>();
		return nullptr;
	}

	// Resolve a Lua camera argument to GameObject*. Scene scripts pass either
	// a raw GameObject* (camera_fly's `camera = owner` from GetOwner()) or a
	// shared_ptr from GameObject.new() (Island reflection cam). sol will not
	// coerce shared_ptr<LUA_GameObject> into GameObject* for a bound
	// &ForwardRenderer::RenderScene - the call throws and DemoLauncher's
	// protected draw aborts the whole frame (full black Island on GL).
	//
	// Check shared_ptr FIRST: on some sol2/build combos, is<T*>() can
	// spuriously succeed for shared_ptr userdata and as<T*>() then yields
	// a garbage pointer (Island reflection cam → black / crash).
	GameObject* LuaObjectToGameObjectPtr(const sol::object &o)
	{
		if (!o.valid() || o.get_type() == sol::type::lua_nil)
			return nullptr;
		std::shared_ptr<GameObject> owned = LuaObjectToGameObject(o);
		if (owned)
			return owned.get();
		if (o.is<LUA_GameObject*>())
			return o.as<LUA_GameObject*>();
		if (o.is<GameObject*>())
			return o.as<GameObject*>();
		return nullptr;
	}

	// Expose the same resolution to Lua so scripts can coerce GameObject.new()
	// (shared_ptr<LUA_GameObject>) into a raw GameObject* for renderScene /
	// preRender. Returning GameObject* pushes the GameObjectBase usertype,
	// which is exactly what camera_fly's `camera = owner` already is.
	GameObject* AsGameObject(sol::object o)
	{
		GameObject* go = LuaObjectToGameObjectPtr(o);
		if (!go)
			throw std::runtime_error("asGameObject: argument is not a GameObject");
		return go;
	}

	void SceneGraph_AddObj(SceneGraph &scene, sol::object goObj)
	{
		std::shared_ptr<GameObject> go = LuaObjectToGameObject(goObj);
		if (!go)
			throw std::runtime_error("Scene:add: argument is not a GameObject");
		scene.Add(go);
	}
	void SceneGraph_RemoveObj(SceneGraph &scene, sol::object goObj)
	{
		std::shared_ptr<GameObject> go = LuaObjectToGameObject(goObj);
		if (!go)
			throw std::runtime_error("Scene:remove: argument is not a GameObject");
		scene.Remove(go);
	}
	void SceneGraph_AddGameObjectObj(SceneGraph &scene, sol::object goObj)
	{
		std::shared_ptr<GameObject> go = LuaObjectToGameObject(goObj);
		if (!go)
			throw std::runtime_error("Scene:addGameObject: argument is not a GameObject");
		scene.AddGameObject(go);
	}
	void SceneGraph_RemoveGameObjectObj(SceneGraph &scene, sol::object goObj)
	{
		std::shared_ptr<GameObject> go = LuaObjectToGameObject(goObj);
		if (!go)
			throw std::runtime_error("Scene:removeGameobject: argument is not a GameObject");
		scene.RemoveGameObject(go);
	}
	void GameObject_AddGameObjectObj(GameObject &parent, sol::object childObj)
	{
		std::shared_ptr<GameObject> child = LuaObjectToGameObject(childObj);
		if (!child)
			throw std::runtime_error("GameObject:addGameObject: argument is not a GameObject");
		parent.AddGameObject(child);
	}
	void GameObject_RemoveGameObjectObj(GameObject &parent, sol::object childObj)
	{
		std::shared_ptr<GameObject> child = LuaObjectToGameObject(childObj);
		if (!child)
			throw std::runtime_error("GameObject:removeGameObject: argument is not a GameObject");
		parent.RemoveGameObject(child);
	}

	// Real, minimal "find a sibling component by type" for Lua behavior
	// scripts attached via attachScript() - e.g. a skeleton-animation-
	// driving script reaching the RenderingComponent on the same
	// GameObject, or a particle-tuning script reaching its ParticleSystem.
	// Covers only the component types an attached script actually needs
	// to look up this way today; grows on demand rather than trying to
	// genericize every ComponentType up front. Takes a plain GameObject&
	// (not LUA_GameObject&) and sol::this_state instead of a captured
	// lua* so it can be bound identically on both the LUA_GameObject and
	// base GameObject usertypes - self.owner in a script is always the
	// latter's static type (see the GameObjectBase usertype's comment).
	sol::object GameObject_GetComponent(GameObject &go, const std::string &typeName, sol::this_state s)
	{
		sol::state_view lua(s);
		for (const auto& c : go.GetComponents())
		{
			if (typeName == "RenderingComponent" && c->GetComponentType() == ComponentType::RenderingComponent)
			{
				// Must be pushed as LUA_RenderingComponent, the type actually
				// registered as a sol usertype - sol picks the metatable from
				// the pointer's *static* type, so pushing a plain
				// RenderingComponent* yields a bare userdata that throws
				// "attempt to index a sol.p3d::RenderingComponent * value" on
				// the first method call. Anything built by SceneSerializer
				// with a Lua state is already a LUA_RenderingComponent (see
				// its DeserializeComponent comment); return nil rather than
				// that unusable userdata for anything else, so a script's
				// `if rc then` guard actually guards. Prefer the matching
				// shared_ptr so Lua holds a real ownership ref.
				auto lrc = std::dynamic_pointer_cast<LUA_RenderingComponent>(c);
				if (lrc) return sol::make_object(lua, lrc);
				return sol::lua_nil;
			}
			if (typeName == "ParticleSystem" && c->GetComponentType() == ComponentType::ParticleSystem)
				return sol::make_object(lua, std::static_pointer_cast<ParticleSystem>(c));
			// Box2D body. No LUA_ subclass needed - Physics2D is itself the
			// registered usertype, so the shared_ptr pushes with the right
			// metatable directly.
			if (typeName == "Layer2D" && c->GetComponentType() == ComponentType::Layer2D)
				return sol::make_object(lua, std::static_pointer_cast<Layer2D>(c));
			if (typeName == "Physics2D" && c->GetComponentType() == ComponentType::Physics2D)
				return sol::make_object(lua, std::static_pointer_cast<Physics2D>(c));
		}
		return sol::lua_nil;
	}
	// ForwardRenderer
	void ForwardRenderer_EnableClipPlane(ForwardRenderer &r, uint32 numberOfClipPlanes)
	{
		r.EnableClipPlane(numberOfClipPlanes);
	}
	void ForwardRenderer_EnableClipPlaneDefault(ForwardRenderer &r)
	{
		r.EnableClipPlane(1);
	}
	void ForwardRenderer_ClearBufferBit(ForwardRenderer &r, uint32 option)
	{
		r.ClearBufferBit(option);
	}
	void DeferredRenderer_EnableClipPlane(DeferredRenderer &r, uint32 numberOfClipPlanes)
	{
		r.EnableClipPlane(numberOfClipPlanes);
	}
	void DeferredRenderer_EnableClipPlaneDefault(DeferredRenderer &r)
	{
		r.EnableClipPlane(1);
	}
	void DeferredRenderer_ClearBufferBit(DeferredRenderer &r, uint32 option)
	{
		r.ClearBufferBit(option);
	}

	void ForwardRenderer_PreRender(ForwardRenderer &r, sol::object camObj, SceneGraph* Scene)
	{
		GameObject* cam = LuaObjectToGameObjectPtr(camObj);
		if (!cam) throw std::runtime_error("ForwardRenderer:preRender: camera is not a GameObject");
		r.PreRender(cam, Scene);
	}
	void ForwardRenderer_PreRenderTag(ForwardRenderer &r, sol::object camObj, SceneGraph* Scene, const std::string &tag)
	{
		GameObject* cam = LuaObjectToGameObjectPtr(camObj);
		if (!cam) throw std::runtime_error("ForwardRenderer:preRender: camera is not a GameObject");
		r.PreRender(cam, Scene, tag);
	}
	void ForwardRenderer_RenderScene(ForwardRenderer &r, sol::object projObj, sol::object camObj, SceneGraph* Scene)
	{
		GameObject* cam = LuaObjectToGameObjectPtr(camObj);
		if (!cam) throw std::runtime_error("ForwardRenderer:renderScene: camera is not a GameObject");
		// DemoLauncher stores lua["projection"] = &projection (Projection*).
		// Accept pointer or value so Island multipass matches other demos.
		if (projObj.is<Projection*>())
		{
			Projection* p = projObj.as<Projection*>();
			if (!p) throw std::runtime_error("ForwardRenderer:renderScene: null Projection*");
			r.RenderScene(*p, cam, Scene);
			return;
		}
		if (projObj.is<Projection>())
		{
			r.RenderScene(projObj.as<Projection>(), cam, Scene);
			return;
		}
		throw std::runtime_error("ForwardRenderer:renderScene: projection is not a Projection");
	}
	// DeferredRenderer
	void DeferredRenderer_PreRender(DeferredRenderer &r, sol::object camObj, SceneGraph* Scene)
	{
		GameObject* cam = LuaObjectToGameObjectPtr(camObj);
		if (!cam) throw std::runtime_error("DeferredRenderer:preRender: camera is not a GameObject");
		r.PreRender(cam, Scene);
	}
	void DeferredRenderer_PreRenderTag(DeferredRenderer &r, sol::object camObj, SceneGraph* Scene, const std::string &tag)
	{
		GameObject* cam = LuaObjectToGameObjectPtr(camObj);
		if (!cam) throw std::runtime_error("DeferredRenderer:preRender: camera is not a GameObject");
		r.PreRender(cam, Scene, tag);
	}
	// DeferredRenderer::RenderScene() carries a fourth BufferOptions
	// parameter that ForwardRenderer's does not. sol binds the function's
	// full arity, so C++ default arguments are not optional from Lua -
	// without this wrapper a script had to pass BufferOptions explicitly,
	// which it could not even spell: the values come from Buffer_Bit,
	// and only the unrelated FBOBufferBit enum (0/1/2 rather than
	// 0x10/0x20/0x40) was exposed. Restoring the default here also makes
	// renderScene() identical across both renderers, so a script can swap
	// one for the other without touching its render loop.
	void DeferredRenderer_RenderScene(DeferredRenderer &r, const p3d::Projection &projection, sol::object camObj, SceneGraph* Scene)
	{
		GameObject* cam = LuaObjectToGameObjectPtr(camObj);
		if (!cam) throw std::runtime_error("DeferredRenderer:renderScene: camera is not a GameObject");
		r.RenderScene(projection, cam, Scene);
	}
	void DeferredRenderer_RenderSceneOptions(DeferredRenderer &r, const p3d::Projection &projection, sol::object camObj, SceneGraph* Scene, const uint32 BufferOptions)
	{
		GameObject* cam = LuaObjectToGameObjectPtr(camObj);
		if (!cam) throw std::runtime_error("DeferredRenderer:renderScene: camera is not a GameObject");
		r.RenderScene(projection, cam, Scene, BufferOptions);
	}
	// Shader
	void Shader_SendUniform(Shader &s, const Uniform &uniform, int32 Handle)
	{
		Shader::SendUniform(uniform, Handle);
	}
	void Shader_SendUniformPTR(Shader &s, const Uniform &uniform, void* data, int32 Handle, uint32 elementCount)
	{
		Shader::SendUniform(uniform, data, Handle, elementCount);
	}
	// Skeleton Animation Instance
	void SkeletonAnimationInstance_AddBone(SkeletonAnimationInstance &a, const uint32 LayerID, const std::string &bone)
	{
		a.AddBone(LayerID, bone);
	}
	void SkeletonAnimationInstance_AddBoneSTR(SkeletonAnimationInstance &a, const std::string &LayerName, const std::string &bone)
	{
		a.AddBone(LayerName, bone);
	}
	void SkeletonAnimationInstance_AddBoneAndChilds(SkeletonAnimationInstance &a, const uint32 LayerID, const std::string &bone, bool inclusive)
	{
		a.AddBoneAndChilds(LayerID, bone, inclusive);
	}
	void SkeletonAnimationInstance_AddBoneAndChildsSTR(SkeletonAnimationInstance &a, const std::string &LayerName, const std::string &bone, bool inclusive)
	{
		a.AddBoneAndChilds(LayerName, bone, inclusive);
	}
	void SkeletonAnimationInstance_RemoveBone(SkeletonAnimationInstance &a, const uint32 LayerID, const std::string &bone)
	{
		a.RemoveBone(LayerID, bone);
	}
	void SkeletonAnimationInstance_RemoveBoneSTR(SkeletonAnimationInstance &a, const std::string &LayerName, const std::string &bone)
	{
		a.RemoveBone(LayerName, bone);
	}
	void SkeletonAnimationInstance_RemoveBoneAndChilds(SkeletonAnimationInstance &a, const uint32 LayerID, const std::string &bone, bool inclusive)
	{
		a.AddBoneAndChilds(LayerID, bone, inclusive);
	}
	void SkeletonAnimationInstance_RemoveBoneAndChildsSTR(SkeletonAnimationInstance &a, const std::string &LayerName, const std::string &bone, bool inclusive)
	{
		a.AddBoneAndChilds(LayerName, bone, inclusive);
	}
	bool SkeletonAnimationInstance_IsPaused(SkeletonAnimationInstance &a)
	{
		return a.IsPaused();
	}
	bool SkeletonAnimationInstance_IsPausedID(SkeletonAnimationInstance &a, int ID)
	{
		return a.IsPaused(ID);
	}
	void SkeletonAnimationInstance_DestroyLayer(SkeletonAnimationInstance &a, int id)
	{
		a.DestroyLayer(id);
	}
	void SkeletonAnimationInstance_DestroyLayerSTR(SkeletonAnimationInstance &a, const std::string &str)
	{
		a.DestroyLayer(str);
	}
	// Text
	void Text_UpdateText(Text &t, const std::string &text, const Vec4 &color)
	{
		t.UpdateText(text, color);
	}
	void Text_UpdateTextColors(Text &t, const std::string &text, const std::vector<Vec4> &color)
	{
		t.UpdateText(text, color);
	}
	// IPhysics
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateTriangleMeshRCOMP(IPhysics &p, RenderingComponent* rcomp, const f32 mass)
	{
		return p.CreateTriangleMesh(rcomp, mass);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateTriangleMesh(IPhysics &p, const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass)
	{
		return p.CreateTriangleMesh(index, vertex, mass);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateConvexTriangleMeshRCOMP(IPhysics &p, RenderingComponent* rcomp, const f32 mass)
	{
		return p.CreateConvexTriangleMesh(rcomp, mass);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateConvexTriangleMesh(IPhysics &p, const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass)
	{
		return p.CreateConvexTriangleMesh(index, vertex, mass);
	}
	// Frame Buffer — Texture args must be shared_ptr (Lua factories); sol
	// also drops C++ default args, so renderbuffer overloads need both
	// 4-arg and 5-arg wrappers.
	void FrameBuffer_InitTex(FrameBuffer &f, const uint32 attachmentFormat, const uint32 texType, const std::shared_ptr<Texture> &attachment)
	{
		f.Init(attachmentFormat, texType, attachment.get());
	}
	void FrameBuffer_InitRenderBuffer4(FrameBuffer &f, const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height)
	{
		f.Init(attachmentFormat, attachmentDataType, Width, Height, 0);
	}
	void FrameBuffer_InitRenderBuffer5(FrameBuffer &f, const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height, const uint32 msaa)
	{
		f.Init(attachmentFormat, attachmentDataType, Width, Height, msaa);
	}
	void FrameBuffer_AddAttachTex(FrameBuffer &f, const uint32 attachmentFormat, const uint32 texType, const std::shared_ptr<Texture> &attachment)
	{
		f.AddAttach(attachmentFormat, texType, attachment.get());
	}
	void FrameBuffer_AddAttachRenderBuffer4(FrameBuffer &f, const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height)
	{
		f.AddAttach(attachmentFormat, attachmentDataType, Width, Height, 0);
	}
	void FrameBuffer_AddAttachRenderBuffer5(FrameBuffer &f, const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height, const uint32 msaa)
	{
		f.AddAttach(attachmentFormat, attachmentDataType, Width, Height, msaa);
	}
	// FrameBuffer::Bind(access = Read_Write) - C++ default is invisible to
	// sol; Lua's fbo:bind() was throwing "expected number, received no
	// value" into void(unsigned int) and aborting Island multipass every
	// frame (flash via main-only fallback).
	void FrameBuffer_Bind(FrameBuffer &f, uint32 access)
	{
		f.Bind(access);
	}
	void FrameBuffer_BindDefault(FrameBuffer &f)
	{
		f.Bind();
	}
	void Texture_Resize3(Texture &t, uint32 width, uint32 height, uint32 level)
	{
		t.Resize(width, height, level);
	}
	void Texture_Resize2(Texture &t, uint32 width, uint32 height)
	{
		t.Resize(width, height, 0);
	}
	// RenderingComponent::GetActiveSkeletonAnimation() returns void* (see
	// its header comment - avoids a circular #include), so it can't be
	// bound to Lua directly; this wraps the documented, guaranteed-safe
	// cast back to SkeletonAnimationInstance* (only ever set from that
	// class's own constructor) so a Lua behavior script attached to the
	// same GameObject can reach the skeleton animation actually driving
	// its model (e.g. via owner:getComponent("RenderingComponent")).
	SkeletonAnimationInstance* RenderingComponent_GetActiveSkeletonAnimation(RenderingComponent &rc)
	{
		return static_cast<SkeletonAnimationInstance*>(rc.GetActiveSkeletonAnimation());
	}
	// Same void*-to-typed cast pattern as GetActiveSkeletonAnimation -
	// TextureAnimationInstance is the only type ever stored via
	// SetActiveTextureAnimation (see SceneSerializer load path).
	TextureAnimationInstance* RenderingComponent_GetActiveTextureAnimation(RenderingComponent &rc)
	{
		return static_cast<TextureAnimationInstance*>(rc.GetActiveTextureAnimation());
	}
	// RenderingMesh::Material is shared_ptr<IMaterial>; Lua scripts that
	// need GenericShaderMaterial::SetColorMap (e.g. texture_anim.lua)
	// can't call it through the IMaterial usertype alone.
	std::shared_ptr<GenericShaderMaterial> RenderingMesh_GetGenericMaterial(RenderingMesh &m)
	{
		return std::dynamic_pointer_cast<GenericShaderMaterial>(m.Material);
	}

	// SOL_CHECK_ARGUMENTS rejects shared_ptr<Cube> for a parameter typed
	// shared_ptr<Renderable> even when Cube lists bases<Renderable> - sol's
	// factory overload matcher does not walk that inheritance for
	// shared_ptr. Pull the concrete userdata out of sol::object instead.
	std::shared_ptr<Renderable> LuaObjectToRenderable(const sol::object &o)
	{
		if (!o.valid()) return nullptr;
		if (o.is<std::shared_ptr<Cube>>()) return o.as<std::shared_ptr<Cube>>();
		if (o.is<std::shared_ptr<Sphere>>()) return o.as<std::shared_ptr<Sphere>>();
		if (o.is<std::shared_ptr<Plane>>()) return o.as<std::shared_ptr<Plane>>();
		if (o.is<std::shared_ptr<Capsule>>()) return o.as<std::shared_ptr<Capsule>>();
		if (o.is<std::shared_ptr<Cone>>()) return o.as<std::shared_ptr<Cone>>();
		if (o.is<std::shared_ptr<Cylinder>>()) return o.as<std::shared_ptr<Cylinder>>();
		if (o.is<std::shared_ptr<Torus>>()) return o.as<std::shared_ptr<Torus>>();
		if (o.is<std::shared_ptr<TorusKnot>>()) return o.as<std::shared_ptr<TorusKnot>>();
		if (o.is<std::shared_ptr<Text>>()) return o.as<std::shared_ptr<Text>>();
		if (o.is<std::shared_ptr<Model>>()) return o.as<std::shared_ptr<Model>>();
		if (o.is<std::shared_ptr<Decal>>()) return o.as<std::shared_ptr<Decal>>();
		if (o.is<std::shared_ptr<Renderable>>()) return o.as<std::shared_ptr<Renderable>>();
		return nullptr;
	}

	std::shared_ptr<IMaterial> LuaObjectToMaterial(const sol::object &o)
	{
		if (!o.valid()) return nullptr;
		if (o.get_type() != sol::type::userdata) return nullptr;
		if (o.is<std::shared_ptr<GenericShaderMaterial>>()) return o.as<std::shared_ptr<GenericShaderMaterial>>();
		if (o.is<std::shared_ptr<CustomShaderMaterial>>()) return o.as<std::shared_ptr<CustomShaderMaterial>>();
		if (o.is<std::shared_ptr<IMaterial>>()) return o.as<std::shared_ptr<IMaterial>>();
		return nullptr;
	}

	// ShaderUsage.* from sol::new_enum is a Lua number. NEVER call
	// o.as<uint32>() on userdata (GenericShaderMaterial / CustomShaderMaterial):
	// sol reports that via lua_error/longjmp, which bypasses C++ catch and
	// aborts RenderingComponent.new(mesh, mat) with
	// "expected number, received GenericShaderMaterial" - that left LOD
	// teapots, Neon arena, and Island water uncreated (lights-only / blue /
	// no water). Only accept real Lua numbers as material-options ints.
	bool LuaObjectToMaterialOptions(const sol::object &o, uint32 &out)
	{
		if (!o.valid()) return false;
		if (o.get_type() != sol::type::number)
			return false;
		out = o.as<uint32>();
		return true;
	}

	void RenderingComponent_ADDLOD(RenderingComponent* rcomp, sol::object renderableObj, const f32 Distance, sol::object materialOrOptions)
	{
		std::shared_ptr<Renderable> renderable = LuaObjectToRenderable(renderableObj);
		if (!renderable)
			throw std::runtime_error("RenderingComponent:addLOD: first argument is not a Renderable");
		std::shared_ptr<IMaterial> material = LuaObjectToMaterial(materialOrOptions);
		if (material)
		{
			rcomp->AddLOD(renderable, Distance, material);
			return;
		}
		uint32 options = 0;
		if (LuaObjectToMaterialOptions(materialOrOptions, options))
		{
			rcomp->AddLOD(renderable, Distance, options);
			return;
		}
		throw std::runtime_error("RenderingComponent:addLOD: third argument is not an IMaterial or material options int");
	}
	void RenderingComponent_ADDLOD_DistOnly(RenderingComponent* rcomp, sol::object renderableObj, const f32 Distance)
	{
		std::shared_ptr<Renderable> renderable = LuaObjectToRenderable(renderableObj);
		if (!renderable)
			throw std::runtime_error("RenderingComponent:addLOD: first argument is not a Renderable");
		rcomp->AddLOD(renderable, Distance, 0u);
	}

	std::shared_ptr<LUA_RenderingComponent> LuaNewRenderingComponent(sol::object renderableObj, sol::object materialOrOptions)
	{
		std::shared_ptr<Renderable> renderable = LuaObjectToRenderable(renderableObj);
		if (!renderable)
			throw std::runtime_error("RenderingComponent.new: first argument is not a Renderable");
		// Prefer IMaterial when the 2nd arg is userdata - options are numbers
		// only (see LuaObjectToMaterialOptions). Checking material first keeps
		// RenderingComponent.new(mesh, mat) off the options path entirely.
		std::shared_ptr<IMaterial> material = LuaObjectToMaterial(materialOrOptions);
		if (material)
			return std::make_shared<LUA_RenderingComponent>(renderable, material);
		uint32 options = 0;
		if (LuaObjectToMaterialOptions(materialOrOptions, options))
			return std::make_shared<LUA_RenderingComponent>(renderable, (int)options);
		throw std::runtime_error("RenderingComponent.new: second argument is not an IMaterial or material options int");
	}

	std::shared_ptr<LUA_RenderingComponent> LuaNewRenderingComponentDist(sol::object renderableObj, sol::object materialOrOptions, float distance)
	{
		std::shared_ptr<Renderable> renderable = LuaObjectToRenderable(renderableObj);
		if (!renderable)
			throw std::runtime_error("RenderingComponent.new: first argument is not a Renderable");
		std::shared_ptr<IMaterial> material = LuaObjectToMaterial(materialOrOptions);
		if (material)
			return std::make_shared<LUA_RenderingComponent>(renderable, material, distance);
		uint32 options = 0;
		if (LuaObjectToMaterialOptions(materialOrOptions, options))
			return std::make_shared<LUA_RenderingComponent>(renderable, (int)options, distance);
		throw std::runtime_error("RenderingComponent.new: second argument is not an IMaterial or material options int");
	}

	// Keeps DecalGeometry (owner of GetDecal() mesh) alive for the process.
	//
	// Deliberately allocated and never freed, rather than held in a static
	// vector. A static vector is destroyed from __cxa_finalize during
	// exit(), which is long after the render device has been torn down:
	// ~DecalGeometry runs ~Model -> IGeometry::Dispose ->
	// GeometryBuffer::~GeometryBuffer -> RenderDevice::DestroyBuffer, and
	// that dereferences the dead device and segfaults. Any run that placed
	// a single decal crashed on the way out, with a stack that pointed at
	// the vector's destructor rather than at anything the frame did.
	static std::vector<std::unique_ptr<DecalGeometry> > &LuaDecalGeometries()
	{
		static std::vector<std::unique_ptr<DecalGeometry> > *v =
			new std::vector<std::unique_ptr<DecalGeometry> >();
		return *v;
	}

	static bool LuaGetIntersectedTriangle(RenderingComponent* rcomp, const Mouse3D &mouse, GameObject* camera, Vec3* intersection, Vec3* normal)
	{
		Vec3 _intersection, finalIntersection, _normal;
		f32 t = 0.f, dist = 0.f;
		bool init = false;

		for (size_t k = 0; k < rcomp->GetMeshes().size(); k++)
		{
			RenderingMesh* mesh = rcomp->GetMeshes()[k];
			if (!mesh || !mesh->Geometry) continue;
				const std::vector<Vec3> &verts = mesh->Geometry->GetVertexData();
			const std::vector<Vec3> &norms = mesh->Geometry->GetNormalData();
			const std::vector<__INDEX_C_TYPE__> &idx = mesh->Geometry->GetIndexData();
			for (size_t i = 0; i + 2 < idx.size(); i += 3)
			{
				if (!mouse.rayIntersectionTriangle(verts[idx[i]], verts[idx[i + 1]], verts[idx[i + 2]], &_intersection, &t))
					continue;

				Vec3 forward = camera->GetDirection().negate();
				if (forward.dotProduct(rcomp->GetOwner()->GetWorldTransformation() * _intersection - camera->GetWorldPosition()) < 0)
					continue;

				if (!init)
				{
					finalIntersection = _intersection;
					_normal = norms.empty() ? Vec3(0, 1, 0) : norms[idx[i]];
					dist = t;
					init = true;
					continue;
				}
				if (t < dist)
				{
					dist = t;
					finalIntersection = _intersection;
					_normal = norms.empty() ? Vec3(0, 1, 0) : norms[idx[i]];
				}
			}
		}
		if (!init) return false;
		*intersection = finalIntersection;
		*normal = _normal;
		return true;
	}

	namespace {
		// Finds a GameObject's IK component and the constraint driving `chain`.
		IKConstraint* FindIKConstraint(GameObject* go, const std::string &chain)
		{
			if (!go) return NULL;
			const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
			for (size_t i = 0; i < comps.size(); i++)
			{
				if (!comps[i] || comps[i]->GetComponentType() != ComponentType::IK) continue;
				IKComponent* ik = static_cast<IKComponent*>(comps[i].get());
				for (uint32 k = 0; k < ik->GetNumberConstraints(); k++)
					if (ik->GetConstraint(k).ChainName == chain) return &ik->GetConstraint(k);
			}
			return NULL;
		}
	}

	// Free functions rather than an IKComponent usertype: getComponent() pushes
	// a shared_ptr, which sol2 hands back as an unregistered userdata unless the
	// type is plumbed through the same LUA_* subclass dance RenderingComponent
	// needs. Two setters do not justify that.
	bool SetIKConstraintEnabled(GameObject* go, const std::string &chain, bool enabled)
	{
		IKConstraint* c = FindIKConstraint(go, chain);
		if (!c) return false;
		c->Enabled = enabled;
		return true;
	}

	// 0 leaves the animated pose alone, 1 fully honours the target, between
	// blends - which is how a foot plant fades in as the foot lands rather
	// than snapping.
	bool SetIKConstraintWeight(GameObject* go, const std::string &chain, float weight)
	{
		IKConstraint* c = FindIKConstraint(go, chain);
		if (!c) return false;
		c->Weight = weight;
		return true;
	}

	bool WorldToScreen(float winW, float winH, GameObject* camera, Projection* projection,
		const Vec3 &worldPos, float* outX, float* outY)
	{
		if (!camera || !projection || !outX || !outY) return false;

		const Matrix view = camera->GetWorldTransformation().Inverse();
		const Vec4 clip = projection->GetProjectionMatrix() * view * Vec4(worldPos.x, worldPos.y, worldPos.z, 1.f);
		// w is the view-space depth; behind the camera it is <= 0 and the
		// divide below would mirror the point back onto the screen.
		if (clip.w <= 0.0001f) return false;

		const float ndcX = clip.x / clip.w;
		const float ndcY = clip.y / clip.w;
		*outX = (ndcX * 0.5f + 0.5f) * winW;
		// Y is flipped: NDC is +up, window coordinates are +down.
		*outY = (1.f - (ndcY * 0.5f + 0.5f)) * winH;
		return true;
	}

	Vec3 ScreenToWorldAtDepth(float winW, float winH, float mouseX, float mouseY,
		GameObject* camera, Projection* projection, const Vec3 &refWorldPos)
	{
		if (!camera || !projection) return refWorldPos;

		const Matrix camWorld = camera->GetWorldTransformation();
		Mouse3D mouse;
		mouse.GenerateRay(winW, winH, mouseX, mouseY, Matrix(), camWorld.Inverse(),
			projection->GetProjectionMatrix());

		const Vec3 origin = mouse.GetOrigin();
		const Vec3 dir = mouse.GetDirection();

		// Plane through refWorldPos whose normal is the camera's forward axis.
		// Column 2 of the camera's world matrix is its local +Z, which points
		// BACKWARDS out of the screen in this engine's convention (LookAt
		// builds a view matrix, and its inverse is the camera world matrix),
		// so the sign does not matter here - a plane and its flip are the same
		// plane.
		const Vec3 fwd = Vec3(camWorld.m[8], camWorld.m[9], camWorld.m[10]).normalize();
		const float denom = dir.dotProduct(fwd);
		// Ray parallel to the plane: nothing sensible to return, so hold
		// position rather than shooting the object off to infinity.
		if (fabs(denom) < 1e-6f) return refWorldPos;

		const float t = (refWorldPos - origin).dotProduct(fwd) / denom;
		if (t < 0.f) return refWorldPos;
		return origin + dir * t;
	}

	// Port of examples/Decals::CreateDecal for DemoLauncher scene Lua.
	bool PlaceDecalAtCursor(float winW, float winH, float mouseX, float mouseY,
		GameObject* camera, Projection* projection, SceneGraph* scene,
		sol::object materialObj, const Vec3 &dimensions)
	{
		if (!camera || !projection || !scene) return false;
		std::shared_ptr<IMaterial> material = LuaObjectToMaterial(materialObj);
		if (!material) return false;

		Mouse3D mouse;
		Vec3 FinalIntersection, FinalNormal;
		f32 bestDist = 1e30f;
		RenderingComponent* bestRc = NULL;
		std::shared_ptr<GameObject> bestGo;

		const Matrix viewInv = camera->GetWorldTransformation().Inverse();
		const Matrix &proj = projection->GetProjectionMatrix();

		for (const std::shared_ptr<GameObject> &go : scene->GetAllGameObjectList())
		{
			if (!go) continue;
			RenderingComponent* rcomp = NULL;
			for (const std::shared_ptr<IComponent> &c : go->GetComponents())
			{
				if (c && c->GetComponentType() == ComponentType::RenderingComponent)
				{
					rcomp = static_cast<RenderingComponent*>(c.get());
					break;
				}
			}
			if (!rcomp || rcomp->GetMeshes().empty()) continue;

			mouse.GenerateRay(winW, winH, mouseX, mouseY, go->GetWorldTransformation(), viewInv, proj);
			f32 t = 0.f;
			if (!mouse.rayIntersectionBox(rcomp->GetBoundingMinValue(), rcomp->GetBoundingMaxValue(), &t))
				continue;

			Vec3 intersection, normal;
			if (!LuaGetIntersectedTriangle(rcomp, mouse, camera, &intersection, &normal))
				continue;

			f32 dist2 = intersection.distanceSQR(camera->GetWorldPosition());
			if (dist2 < bestDist)
			{
				bestDist = dist2;
				bestRc = rcomp;
				bestGo = go;
				FinalIntersection = intersection;
				FinalNormal = normal;
			}
		}

		if (!bestRc || !bestGo) return false;

		Matrix m;
		m.LookAt(FinalIntersection, FinalNormal.negate(), Vec3(0, 1, 0));
		m = m.Inverse();
		m.Translate(FinalIntersection);

		auto geom = std::unique_ptr<DecalGeometry>(new DecalGeometry(bestRc->GetMeshes()[0], bestGo->GetWorldTransformation(), m, dimensions));
		Renderable* decalMesh = geom->GetDecal();
		if (!decalMesh) return false;

		// DecalGeometry owns the mesh; RC holds a non-owning shared_ptr view.
		std::shared_ptr<Renderable> renderable(decalMesh, [](Renderable*) {});
		auto rc = std::make_shared<LUA_RenderingComponent>(renderable, material);
		bestGo->AddComponent(rc);
		LuaDecalGeometries().push_back(std::move(geom));
		return true;
	}
	// ******************************* OVERLOADS *******************************


} // namespace p3d

#endif
