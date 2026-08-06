//============================================================================
// Name        : PyrosBindings.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : lua Bindings
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosBindings.h>
#include <Pyros3D/Utils/Mouse3D/Mouse3D.h>
#include <memory>
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
	void SkeletonAnimationInstance_AddBoneAndChilds(SkeletonAnimationInstance &a, const uint32 LayerID, const std::string &bone, bool inclusive = true)
	{
		a.AddBoneAndChilds(LayerID, bone, inclusive);
	}
	void SkeletonAnimationInstance_AddBoneAndChildsSTR(SkeletonAnimationInstance &a, const std::string &LayerName, const std::string &bone, bool inclusive = true)
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
	void SkeletonAnimationInstance_RemoveBoneAndChilds(SkeletonAnimationInstance &a, const uint32 LayerID, const std::string &bone, bool inclusive = true)
	{
		a.AddBoneAndChilds(LayerID, bone, inclusive);
	}
	void SkeletonAnimationInstance_RemoveBoneAndChildsSTR(SkeletonAnimationInstance &a, const std::string &LayerName, const std::string &bone, bool inclusive = true)
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
	void Text_UpdateText(Text &t, const std::string &text, const Vec4 &color = Vec4(1, 1, 1, 1))
	{
		t.UpdateText(text, color);
	}
	void Text_UpdateTextColors(Text &t, const std::string &text, const std::vector<Vec4> &color)
	{
		t.UpdateText(text, color);
	}
	// IPhysics
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateTriangleMeshRCOMP(IPhysics &p, RenderingComponent* rcomp, const f32 mass = 0.f)
	{
		return p.CreateTriangleMesh(rcomp, mass);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateTriangleMesh(IPhysics &p, const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass = 0.f)
	{
		return p.CreateTriangleMesh(index, vertex, mass);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateConvexTriangleMeshRCOMP(IPhysics &p, RenderingComponent* rcomp, const f32 mass = 0.f)
	{
		return p.CreateConvexTriangleMesh(rcomp, mass);
	}
	std::shared_ptr<IPhysicsComponent> IPhysics_CreateConvexTriangleMesh(IPhysics &p, const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass = 0.f)
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
	static std::vector<std::unique_ptr<DecalGeometry> > g_LuaDecalGeometries;

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
		g_LuaDecalGeometries.push_back(std::move(geom));
		return true;
	}
	// ******************************* OVERLOADS *******************************

	void GenerateBindings(sol::state* lua)
	{
		lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::coroutine, sol::lib::table, sol::lib::string);

		// ******************************* ENUMS *******************************
		{
			// Shader Usage
			lua->new_enum("ShaderUsage",
				"Color", ShaderUsage::Color,
				"Texture", ShaderUsage::Texture,
				"EnvMap", ShaderUsage::EnvMap,
				"SkyBox", ShaderUsage::Skybox,
				"Refraction", ShaderUsage::Refraction,
				"Skinning", ShaderUsage::Skinning,
				"CellShading", ShaderUsage::CellShading,
				"BumpMapping", ShaderUsage::BumpMapping,
				"SpecularMap", ShaderUsage::SpecularMap,
				"SpecularColor", ShaderUsage::SpecularColor,
				"DirectionalShadow", ShaderUsage::DirectionalShadow,
				"PointShadow", ShaderUsage::PointShadow,
				"SpotShadow", ShaderUsage::SpotShadow,
				"CastShadows", ShaderUsage::CastShadows,
				"Diffuse", ShaderUsage::Diffuse,
				"TextRendering", ShaderUsage::TextRendering,
				"DebugRendering", ShaderUsage::DebugRendering,
				"ClipPlane", ShaderUsage::ClipPlane,
				// The remaining ShaderLib.h flags. Without these a Lua scene
				// could construct a DeferredRenderer but had no way to build
				// a material that writes the G-buffer, so every object was
				// silently missing from the deferred pass - the enum stopped
				// at ClipPlane while the C++ enum carried six more values.
				"DeferredRenderer_Gbuffer", ShaderUsage::DeferredRenderer_Gbuffer,
				"ParallaxMapping", ShaderUsage::ParallaxMapping,
				"InstancedRendering", ShaderUsage::InstancedRendering,
				"VelocityRendering", ShaderUsage::VelocityRendering,
				"PBR", ShaderUsage::PBR,
				"PBRMap", ShaderUsage::PBRMap
			);

			lua->new_enum("TextureTransparency",
				"Opaque", TextureTransparency::Opaque,
				"Transparent", TextureTransparency::Transparent
			);

			lua->new_enum("TextureFilter",
				"Nearest", TextureFilter::Nearest,
				"Linear", TextureFilter::Linear,
				"LinearMipmapLinear", TextureFilter::LinearMipmapLinear,
				"LinearMipmapNearest", TextureFilter::LinearMipmapNearest,
				"NearestMipmapNearest", TextureFilter::NearestMipmapNearest,
				"NearestMipmapLinear", TextureFilter::NearestMipmapLinear
			);

			lua->new_enum("TextureRepeat",
				"Clapm", TextureRepeat::Clamp,
				"ClampToBorder", TextureRepeat::ClampToBorder,
				"ClampToEdge", TextureRepeat::ClampToEdge,
				"Repeat", TextureRepeat::Repeat
			);

			lua->new_enum("TextureDataType",
				"RGBA", TextureDataType::RGBA,
				"BGR", TextureDataType::BGR,
				"BGRA", TextureDataType::BGRA,
				"DepthComponent", TextureDataType::DepthComponent,
				"DepthComponent16", TextureDataType::DepthComponent16,
				"DepthComponent24", TextureDataType::DepthComponent24,
				"DepthComponent32", TextureDataType::DepthComponent32,
				"R", TextureDataType::R8,
				"R16F", TextureDataType::R16F,
				"R32F", TextureDataType::R32F,
				"R16I", TextureDataType::R16I,
				"R32I", TextureDataType::R32I,
				"RG", TextureDataType::RG8,
				"RG16F", TextureDataType::RG16F,
				"RG32F", TextureDataType::RG32F,
				"RG16I", TextureDataType::RG16I,
				"RG32I", TextureDataType::RG32I,
				"RGB", TextureDataType::RGB8,
				"RGB16F", TextureDataType::RGB16F,
				"RGB32F", TextureDataType::RGB32F,
				"RGB16I", TextureDataType::RGB16I,
				"RGB32I", TextureDataType::RGB32I,
				"RGBA16F", TextureDataType::RGBA16F,
				"RGBA32F", TextureDataType::RGBA32F,
				"RGBA16I", TextureDataType::RGBA16I,
				"RGBA32I", TextureDataType::RGBA32I,
				"LUMINANCE", TextureDataType::LUMINANCE,
				"LUMINANCE_ALPHA", TextureDataType::LUMINANCE_ALPHA
			);

			lua->new_enum("TextureType",
				"CubemapPositive_X", TextureType::CubemapPositive_X,
				"CubemapNegative_X", TextureType::CubemapNegative_X,
				"CubemapPositive_Y", TextureType::CubemapPositive_Y,
				"CubemapNegative_Y", TextureType::CubemapNegative_Y,
				"CubemapPositive_Z", TextureType::CubemapPositive_Z,
				"CubemapNegative_Z", TextureType::CubemapNegative_Z,
				"Texture_Multisample", TextureType::Texture_Multisample,
				"Texture", TextureType::Texture
			);

			lua->new_enum("FrameBufferAttachmentFormat",
				"Color_Attachment0", FrameBufferAttachmentFormat::Color_Attachment0,
				"Color_Attachment1", FrameBufferAttachmentFormat::Color_Attachment1,
				"Color_Attachment2", FrameBufferAttachmentFormat::Color_Attachment2,
				"Color_Attachment3", FrameBufferAttachmentFormat::Color_Attachment3,
				"Color_Attachment4", FrameBufferAttachmentFormat::Color_Attachment4,
				"Color_Attachment5", FrameBufferAttachmentFormat::Color_Attachment5,
				"Color_Attachment6", FrameBufferAttachmentFormat::Color_Attachment6,
				"Color_Attachment7", FrameBufferAttachmentFormat::Color_Attachment7,
				"Color_Attachment8", FrameBufferAttachmentFormat::Color_Attachment8,
				"Color_Attachment9", FrameBufferAttachmentFormat::Color_Attachment9,
				"Color_Attachment10", FrameBufferAttachmentFormat::Color_Attachment10,
				"Color_Attachment11", FrameBufferAttachmentFormat::Color_Attachment11,
				"Color_Attachment12", FrameBufferAttachmentFormat::Color_Attachment12,
				"Color_Attachment13", FrameBufferAttachmentFormat::Color_Attachment13,
				"Color_Attachment14", FrameBufferAttachmentFormat::Color_Attachment14,
				"Color_Attachment15", FrameBufferAttachmentFormat::Color_Attachment15,
				"Depth_Attachment", FrameBufferAttachmentFormat::Depth_Attachment,
				"Stencil_Attachment", FrameBufferAttachmentFormat::Stencil_Attachment
			);

			lua->new_enum("RenderBufferDataType",
				"RGBA", RenderBufferDataType::RGBA,
				"Depth", RenderBufferDataType::Depth,
				"Stencil", RenderBufferDataType::Stencil,
				"RGBA_Multisample", RenderBufferDataType::RGBA_Multisample,
				"Depth_Multisample", RenderBufferDataType::Depth_Multisample,
				"Stencil_Multisample", RenderBufferDataType::Stencil_Multisample
			);

			lua->new_enum("FBOAttachmentType",
				"Texture", FBOAttachmentType::Texture,
				"RenderBuffer", FBOAttachmentType::RenderBuffer
			);

			lua->new_enum("FBOAccess",
				"Read_Write", FBOAccess::Read_Write,
				"Read", FBOAccess::Read,
				"Write", FBOAccess::Write
			);

			lua->new_enum("FBOBufferBit",
				"Color", FBOBufferBit::Color,
				"Depth", FBOBufferBit::Depth,
				"Stencil", FBOBufferBit::Stencil
			);

			// Distinct from FBOBufferBit above despite the similar name:
			// these are real OR-able mask bits (0x10/0x20/0x40), whereas
			// FBOBufferBit is a 0/1/2 index used to pick an attachment.
			// clearBufferBit()/renderScene()'s BufferOptions take THESE -
			// previously unreachable from Lua, so any script calling
			// clearBufferBit(FBOBufferBit.Color) was passing 0 (None).
			lua->new_enum("BufferBit",
				"None", Buffer_Bit::None,
				"Color", Buffer_Bit::Color,
				"Depth", Buffer_Bit::Depth,
				"Stencil", Buffer_Bit::Stencil
			);
			lua->new_enum("BlendFunc",
				"Zero", BlendFunc::Zero,
				"One", BlendFunc::One,
				"Src_Alpha", BlendFunc::Src_Alpha,
				"One_Minus_Src_Alpha", BlendFunc::One_Minus_Src_Alpha,
				"Dst_Alpha", BlendFunc::Dst_Alpha,
				"One_Minus_Dst_Alpha", BlendFunc::One_Minus_Dst_Alpha
			);
			lua->new_enum("CullFace",
				"BackFace", CullFace::BackFace,
				"FrontFace", CullFace::FrontFace,
				"DoubleSided", CullFace::DoubleSided
			);

			lua->new_enum("FBOFilter",
				"Linear", FBOFilter::Linear,
				"Nearest", FBOFilter::Nearest
			);

			// Drawing Type
			lua->new_enum("DrawingType",
				"Triangles", DrawingType::Triangles,
				"Lines", DrawingType::Lines,
				"Line_Loop", DrawingType::Line_Loop,
				"Line_Strip", DrawingType::Line_Strip,
				"Triangle_Fan", DrawingType::Triangle_Fan,
				"Triangle_Strip", DrawingType::Triangle_Strip,
				"Quads", DrawingType::Quads,
				"Points", DrawingType::Points,
				"Polygons", DrawingType::Polygons
			);
		}

		// ******************************* ENUMS *******************************

		// ******************************* CLASS *******************************
		{
			// VEC2
			sol::constructors<sol::types<>, sol::types<float, float>> con;
			lua->new_usertype<Math::Vec2>("Vec2",
				con,
				"x", &Math::Vec2::x,
				"y", &Math::Vec2::y,
				"dotProduct", &Math::Vec2::dotProduct,
				"magnitude", &Math::Vec2::magnitude,
				"magnitudeSQR", &Math::Vec2::magnitudeSQR,
				"distance", &Math::Vec2::distance,
				"distanceSQR", &Math::Vec2::distanceSQR,
				"normalize", &Math::Vec2::normalize,
				"negate", &Math::Vec2::negate,
				"abs", &Math::Vec2::Abs,
				"__add", sol::overload(
					&Vec2_operator_add,
					&Vec2_operator_addS
				),
				"__sub", sol::overload(
					&Vec2_operator_sub,
					&Vec2_operator_subS
				),
				"__mul", sol::overload(
					&Vec2_operator_mul,
					&Vec2_operator_mulS
				),
				"__div", sol::overload(
					&Vec2_operator_div,
					&Vec2_operator_divS
				),
				"__eq", &Math::Vec2::operator==,
				"__lt", &Math::Vec2::operator<,
				"__le", &Math::Vec2::operator<=
				);
		}

		{
			// VEC3
			sol::constructors<sol::types<>, sol::types<float, float, float>> con;
			lua->new_usertype<Math::Vec3>("Vec3",
				con,
				"x", &Math::Vec3::x,
				"y", &Math::Vec3::y,
				"z", &Math::Vec3::z,
				"dotProduct", &Math::Vec3::dotProduct,
				"magnitude", &Math::Vec3::magnitude,
				"magnitudeSQR", &Math::Vec3::magnitudeSQR,
				"distance", &Math::Vec3::distance,
				"distanceSQR", &Math::Vec3::distanceSQR,
				"normalize", &Math::Vec3::normalize,
				"normalizeSelft", &Math::Vec3::normalizeSelf,
				"negate", &Math::Vec3::negate,
				"negateSelf", &Math::Vec3::negateSelf,
				"abs", &Math::Vec3::Abs,
				"cross", &Math::Vec3::cross,
				"__add", sol::overload(
					&Vec3_operator_add,
					&Vec3_operator_addS
				),
				"__sub", sol::overload(
					&Vec3_operator_sub,
					&Vec3_operator_subS
				),
				"__mul", sol::overload(
					&Vec3_operator_mul,
					&Vec3_operator_mulS
				),
				"__div", sol::overload(
					&Vec3_operator_div,
					&Vec3_operator_divS
				),
				"__eq", &Math::Vec3::operator==,
				"__lt", &Math::Vec3::operator<,
				"__le", &Math::Vec3::operator<=
				);
		}

		{
			// VEC4
			sol::constructors<sol::types<>, sol::types<float, float, float, float>> con;
			lua->new_usertype<Math::Vec4>("Vec4",
				con,
				"x", &Math::Vec4::x,
				"y", &Math::Vec4::y,
				"z", &Math::Vec4::z,
				"w", &Math::Vec4::w,
				"dotProduct", &Math::Vec4::dotProduct,
				"magnitude", &Math::Vec4::magnitude,
				"magnitudeSQR", &Math::Vec4::magnitudeSQR,
				"abs", &Math::Vec4::Abs,
				"__add", sol::overload(
					&Vec4_operator_add,
					&Vec4_operator_addS
				),
				"__sub", sol::overload(
					&Vec4_operator_sub,
					&Vec4_operator_subS
				),
				"__mul", sol::overload(
					&Vec4_operator_mul,
					&Vec4_operator_mulS
				),
				"__div", sol::overload(
					&Vec4_operator_div,
					&Vec4_operator_divS
				),
				"__eq", &Math::Vec4::operator==,
				"__lt", &Math::Vec4::operator<,
				"__le", &Math::Vec4::operator<=
				);
		}

		{
			// Quaternion
			sol::constructors<sol::types<>, sol::types<float, float, float>, sol::types<float, float, float, float>, sol::types<Vec3, float>> con;
			lua->new_usertype<Math::Quaternion>("Quaternion",
				con,
				"x", &Math::Quaternion::x,
				"y", &Math::Quaternion::y,
				"z", &Math::Quaternion::z,
				"w", &Math::Quaternion::w,
				"convertToMatrix", &Math::Quaternion::ConvertToMatrix,
				"magnitude", &Math::Quaternion::Magnitude,
				"dot", &Math::Quaternion::Dot,
				"abs", &Math::Quaternion::Normalize,
				"rotation", &Math::Quaternion::Rotation,
				"setRotationFromEuler", &Math::Quaternion::SetRotationFromEuler,
				"getEulerRotation", &Math::Quaternion::GetEulerFromQuaternion,
				"axisToQuaternion", &Math::Quaternion::AxisToQuaternion,
				"slerp", &Math::Quaternion::Slerp,
				"nlerp", &Math::Quaternion::Nlerp,
				"inverse", &Math::Quaternion::Inverse,
				"__add", &Quaternion::operator+,
				"__sub", &Quaternion_operator_negate,
				"__mul", sol:: overload(
					&Quaternion_operator_mul,
					&Quaternion_operator_mulS,
					&Quaternion_operator_mulVec3
				)
				);
		}

		{
			// Matrix
			sol::constructors<sol::types<>, sol::types<float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float>> con;
			lua->new_usertype<Math::Matrix>("Matrix",
				con,
				"identity", &Math::Matrix::identity,
				"lookAt", sol::overload(&Matrix_lookAt, &Matrix_lookAt2),
				"translate", sol::overload(&Matrix_translateXYZ, &Matrix_translateVec3),
				"translateX", &Math::Matrix::TranslateX,
				"translateY", &Math::Matrix::TranslateY,
				"translateZ", &Math::Matrix::TranslateZ,
				"getTranslation", &Math::Matrix::GetTranslation,
				"rotationX", &Math::Matrix::RotationX,
				"rotationY", &Math::Matrix::RotationY,
				"rotationZ", &Math::Matrix::RotationZ,
				"setRotationFromEuler", &Math::Matrix::SetRotationFromEuler,
				"getEuler", &Math::Matrix::GetEulerFromRotationMatrix,
				"getRotation", &Math::Matrix::GetRotation,
				"scale", sol::overload(&Matrix_scaleXYZ, &Matrix_scaleVec3),
				"scaleX", &Math::Matrix::ScaleX,
				"scaleY", &Math::Matrix::ScaleY,
				"scaleZ", &Math::Matrix::ScaleZ,
				"getScale", &Math::Matrix::GetScale,
				"getDeterminant", &Math::Matrix::Determinant,
				"transpose", &Math::Matrix::Transpose,
				"inverse", &Math::Matrix::Inverse,
				"perspectiveMatrix", &Math::Matrix::PerspectiveMatrix,
				"orthoMatrix", &Math::Matrix::OrthoMatrix,
				"convertToQuaternion", &Math::Matrix::ConvertToQuaternion,
				"__mul", sol::overload(
					&Matrix_operator_mul,
					&Matrix_operator_mulS,
					&Matrix_operator_mulVec3,
					&Matrix_operator_mulVec4
				),
				"__eq", &Math::Matrix::operator==
				);
		}

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

		{
			// DeferredRenderer
			sol::constructors<sol::types<float, float, FrameBuffer*>> con;
			lua->new_usertype<DeferredRenderer>("DeferredRenderer",
				con,
				"clearBufferBit", &DeferredRenderer_ClearBufferBit,
				"enableClearDepthBuffer", &DeferredRenderer::EnableClearDepthBuffer,
				"disableClearDepthBuffer", &DeferredRenderer::DisableClearDepthBuffer,
				"clearDepthBuffer", &DeferredRenderer::ClearDepthBuffer,
				"enableClipPlane", sol::overload(&DeferredRenderer_EnableClipPlane, &DeferredRenderer_EnableClipPlaneDefault),
				"disableClipPlane", &DeferredRenderer::DisableClipPlane,
				"setClipPlane0", &DeferredRenderer::SetClipPlane0,
				"setClipaPlane0", &DeferredRenderer::SetClipPlane0,
				"setClipaPlane1", &DeferredRenderer::SetClipPlane1,
				"setClipaPlane2", &DeferredRenderer::SetClipPlane2,
				"setClipaPlane3", &DeferredRenderer::SetClipPlane3,
				"setClipaPlane4", &DeferredRenderer::SetClipPlane4,
				"setClipaPlane5", &DeferredRenderer::SetClipPlane5,
				"setClipaPlane6", &DeferredRenderer::SetClipPlane6,
				"setClipaPlane7", &DeferredRenderer::SetClipPlane7,
				"enableStencil", &DeferredRenderer::EnableStencil,
				"disableStencil", &DeferredRenderer::DisableStencil,
				"clearStencilBuffer", &DeferredRenderer::ClearStencilBuffer,
				"stencilFunction", &DeferredRenderer::StencilFunction,
				"stencilOperation", &DeferredRenderer::StencilOperation,
				"enableScissorTest", &DeferredRenderer::EnableScissorTest,
				"disableScissorTest", &DeferredRenderer::DisableScissorTest,
				"scissorTestRect", &DeferredRenderer::ScissorTestRect,
				"enableWireFrame", &DeferredRenderer::EnableWireFrame,
				"disableWireFrame", &DeferredRenderer::DisableWireFrame,
				"colorMask", &DeferredRenderer::ColorMask,
				"enableSorting", &DeferredRenderer::EnableSorting,
				"disableSorting", &DeferredRenderer::DisableSorting,
				"enableLOD", &DeferredRenderer::EnableLOD,
				"disableLOD", &DeferredRenderer::DisableLOD,
				"isUsingLOD", &DeferredRenderer::IsUsingLOD,
				"setBackground", &DeferredRenderer::SetBackground,
				"unsetBackground", &DeferredRenderer::UnsetBackground,
				"setGlobalLight", &DeferredRenderer::SetGlobalLight,
				"enableDepthBias", &DeferredRenderer::EnableDepthBias,
				"disableDepthBias", &DeferredRenderer::DisableDepthBias,
				"setViewPort", &DeferredRenderer::SetViewPort,
				"resetViewPort", &DeferredRenderer::ResetViewPort,
				"resize", &DeferredRenderer::Resize,
				"activateCulling", &DeferredRenderer::ActivateCulling,
				"deactivateCulling", &DeferredRenderer::DeactivateCulling,
				"renderScene", sol::overload(&DeferredRenderer_RenderScene, &DeferredRenderer_RenderSceneOptions),
				"preRender", sol::overload(&DeferredRenderer_PreRender, &DeferredRenderer_PreRenderTag),
				"setSSRDistances", &DeferredRenderer::SetSSRDistances,
				"enableSSR", &DeferredRenderer::EnableSSR,
				"disableSSR", &DeferredRenderer::DisableSSR
				);
		}

		{
			// VelocityRenderer - used by DemoLauncher motion-blur demos.
			sol::constructors<sol::types<uint32, uint32>> con;
			lua->new_usertype<VelocityRenderer>("VelocityRenderer",
				con,
				"renderVelocityMap", &VelocityRenderer::RenderVelocityMap,
				"resize", &VelocityRenderer::Resize,
				"getTexture", &VelocityRenderer::GetTexture
				);
		}

		{
			// CubemapRenderer
			sol::constructors<sol::types<float, float>> con;
			lua->new_usertype<CubemapRenderer>("CubemapRenderer",
				con,
				"renderCubeMap", &CubemapRenderer::RenderCubeMap
				);
		}

		{
			// Frame Buffer
			sol::constructors<sol::types<>> con;
			lua->new_usertype<FrameBuffer>("FrameBuffer",
				con,
				"init", sol::overload(
					&FrameBuffer_InitTex,
					&FrameBuffer_InitRenderBuffer4,
					&FrameBuffer_InitRenderBuffer5
				),
				"addAttach", sol::overload(
					&FrameBuffer_AddAttachTex,
					&FrameBuffer_AddAttachRenderBuffer4,
					&FrameBuffer_AddAttachRenderBuffer5
				),
				"resize", &FrameBuffer::Resize,
				"bind", sol::overload(&FrameBuffer_Bind, &FrameBuffer_BindDefault),
				"isBinded", &FrameBuffer::IsBinded,
				"getBindID", &FrameBuffer::GetBindID,
				"unbind", &FrameBuffer::UnBind,
				"getAttachments", &FrameBuffer::GetAttachments,
				"getFrameBufferFormat", &FrameBuffer::GetFrameBufferFormat,
				"isInitialized", &FrameBuffer::IsInitialized,
				"enableMultiSample", &FrameBuffer::EnableMultisample,
				"disableMultiSample", &FrameBuffer::DisableMultisample,
				"blitFrameBuffer", &FrameBuffer::BlitFrameBuffer
				);
		}

		// Base usertypes for mesh/material hierarchies.
		lua->new_usertype<Renderable>("Renderable");

		{
			// IMaterial - shared_ptr via sol::factories
			lua->new_usertype<IMaterial>("IMaterial",
				sol::factories(
					[]() { return std::make_shared<IMaterial>(); }
				),
				"preRender", &IMaterial::PreRender,
				"render", &IMaterial::Render,
				"afterRender", &IMaterial::AfterRender,
				"setCullFace", &IMaterial::SetCullFace,
				"getCullFace", &IMaterial::GetCullFace,
				"isWireFrame", &IMaterial::IsWireFrame,
				"getOpacity", &IMaterial::GetOpacity,
				"isTransparent", &IMaterial::IsTransparent,
				"setOpacity", &IMaterial::SetOpacity,
				"setTransparencyFlag", &IMaterial::SetTransparencyFlag,
				"enableDepthBias", &IMaterial::EnableDethBias,
				"disableDepthBias", &IMaterial::DisableDethBias,
				"enableBlending", &IMaterial::EnableBlending,
				"disableBlending", &IMaterial::DisableBlending,
				"blendingFunction", &IMaterial::BlendingFunction,
				"blendingEquation", &IMaterial::BlendingEquation,
				"addUniform", &IMaterial::AddUniform,
				"startRenderWireFrame", &IMaterial::StartRenderWireFrame,
				"stopRenderWireFrame", &IMaterial::StopRenderWireFrame,
				"enableCastingShadows", &IMaterial::EnableCastingShadows,
				"disableCastingShadows", &IMaterial::DisableCastingShadows,
				"isCastingShadows", &IMaterial::IsCastingShadows,
				"destroy", &IMaterial::Destroy,
				"getShader", &IMaterial::GetShader,
				"getInternalID", &IMaterial::GetInternalID,
				"enableDepthTest", &IMaterial::EnableDepthTest,
				"disableDepthTest", &IMaterial::DisableDepthTest,
				"enableDepthWrite", &IMaterial::EnableDepthWrite,
				"disableDepthWrite", &IMaterial::DisableDepthWrite,
				"isDepthWritting", &IMaterial::IsDepthWritting,
				"isDepthTesting", &IMaterial::IsDepthTesting
				);
		}



		{
			sol::constructors<sol::types<int>, sol::types<>> con;
			lua->new_usertype<RenderingMesh>("RenderingMesh",
				con,
				"getDrawingType", &RenderingMesh::GetDrawingType,
				"geometry", &RenderingMesh::Geometry,
				"material", &RenderingMesh::Material,
				"getGenericMaterial", &RenderingMesh_GetGenericMaterial,
				"drawingType", &RenderingMesh::drawingType,
				"renderingComponent", &RenderingMesh::renderingComponent,
				"cullingGeometry", &RenderingMesh::CullingGeometry,
				"active", &RenderingMesh::Active,
				"clickable", &RenderingMesh::Clickable
				);
		}

		lua->new_usertype<IComponent>("IComponent",
			"getOwner", &IComponent::GetOwner,
			"enable", &IComponent::Enable,
			"disable", &IComponent::Disable,
			"isActive", &IComponent::IsActive
			);
		lua->new_usertype<ILightComponent>("IlightComponent",
			sol::base_classes, sol::bases<IComponent>()
			);
		// Real read/write physics API - was previously registered with
		// zero bound methods (only IPhysics::CreateBox/CreateSphere/...,
		// which return this type, were bound), meaning physics bodies
		// were write-only from Lua: creatable but unqueryable and
		// undrivable by force/impulse. OnCollisionEnter/OnCollisionExit
		// are plain fields here (like onUpdate/onInit elsewhere) - sol2
		// auto-converts an assigned Lua closure to std::function, same
		// proven pattern, no extra binding plumbing needed.
		lua->new_usertype<IPhysicsComponent>("IPhysicsComponent",
			"getMass", &IPhysicsComponent::GetMass,
			"getShape", &IPhysicsComponent::GetShape,
			"setPosition", &IPhysicsComponent::SetPosition,
			"setRotation", &IPhysicsComponent::SetRotation,
			"cleanForces", &IPhysicsComponent::CleanForces,
			"setAngularVelocity", &IPhysicsComponent::SetAngularVelocity,
			"setLinearVelocity", &IPhysicsComponent::SetLinearVelocity,
			"getLinearVelocity", &IPhysicsComponent::GetLinearVelocity,
			"getAngularVelocity", &IPhysicsComponent::GetAngularVelocity,
			"applyCentralForce", &IPhysicsComponent::ApplyCentralForce,
			"applyCentralImpulse", &IPhysicsComponent::ApplyCentralImpulse,
			"setMass", &IPhysicsComponent::SetMass,
			"activate", &IPhysicsComponent::Activate,
			"isGhost", &IPhysicsComponent::IsGhost,
			"onCollisionEnter", &IPhysicsComponent::OnCollisionEnter,
			"onCollisionExit", &IPhysicsComponent::OnCollisionExit,
			sol::base_classes, sol::bases<IComponent>()
			);

		{
			// RayCastHit / RayCast - real raycasting from Lua, e.g. for
			// click-picking or ground checks. hasHit gates whether the
			// rest of the fields are meaningful (mirrors the real C++
			// struct exactly - no Lua-side reinterpretation).
			sol::constructors<sol::types<>> con;
			lua->new_usertype<RayCastHit>("RayCastHit",
				con,
				"hasHit", &RayCastHit::hasHit,
				"point", &RayCastHit::point,
				"normal", &RayCastHit::normal,
				"distance", &RayCastHit::distance,
				"component", &RayCastHit::component
				);
		}

		{
			// Real, generic Lua-side "attach arbitrary behavior to any
			// GameObject" component - see PyrosBindings.h's LuaComponent
			// class comment. Attach via the already-bound
			// GameObject::addComponent(), same as any other component.
			lua->new_usertype<LuaComponent>("LuaComponent",
				sol::factories(
					[]() { return std::make_shared<LuaComponent>(); }
				),
				"init", &LuaComponent::Init,
				"update", &LuaComponent::Update,
				"destroy", &LuaComponent::Destroy,
				"onUpdate", &LuaComponent::on_update,
				"onInit", &LuaComponent::on_init,
				"onDestroy", &LuaComponent::on_destroy,
				"scriptFile", &LuaComponent::scriptFile,
				"data", &LuaComponent::data,
				sol::base_classes, sol::bases<IComponent>()
				);
			// Real, serializable behavior - see PyrosBindings.h's
			// LuaComponent_FromFile comment. Lower-level building block;
			// GameObject:attachScript() (below, in the GameObject
			// usertype) is the convenient one-call version most scripts
			// want.
			lua->set_function("LuaComponent_fromFile", [lua](const std::string &scriptFile) {
				return LuaComponent_FromFile(*lua, scriptFile);
			});
		}


		{
			// Directional Light - shared_ptr via sol::factories
			lua->new_usertype<LUA_DirectionalLight>("DirectionalLight",
				sol::factories(
					[]() { return std::make_shared<LUA_DirectionalLight>(); },
					[](const Vec4 &color) { return std::make_shared<LUA_DirectionalLight>(color); },
					[](const Vec4 &color, const Vec3 &direction) { return std::make_shared<LUA_DirectionalLight>(color, direction); }
				),
				"start", &LUA_DirectionalLight::Start,
				"update", &LUA_DirectionalLight::Update,
				"destroy", &LUA_DirectionalLight::Destroy,
				"enableShadows", &LUA_DirectionalLight::EnableCastShadows,
				"getLightProjection", &LUA_DirectionalLight::GetLightProjection,
				"updateCascadeFrustumPoints", &LUA_DirectionalLight::UpdateCascadeFrustumPoints,
				"getNumberCascades", &LUA_DirectionalLight::GetNumberCascades,
				"getCascade", &LUA_DirectionalLight::GetCascade,
				"getLightDirection", &LUA_DirectionalLight::GetLightDirection,
				"setLightDirection", &LUA_DirectionalLight::SetLightDirection,
				"getLightColor", &LUA_DirectionalLight::GetLightColor,
				"setLightColor", &LUA_DirectionalLight::SetLightColor,
				"getLightIntensity", &LUA_DirectionalLight::GetLightIntensity,
				"setLightIntensity", &LUA_DirectionalLight::SetLightIntensity,
				"onUpdate", &LUA_DirectionalLight::on_update,
				"onInit", &LUA_DirectionalLight::on_init,
				"onDestroy", &LUA_DirectionalLight::on_destroy,
				"setShadowPCFTexelSize", &LUA_DirectionalLight::SetShadowPCFTexelSize,
				sol::base_classes, sol::bases<IComponent>()
				);
		}

		{
			// Point Light - shared_ptr via sol::factories
			lua->new_usertype<LUA_PointLight>("PointLight",
				sol::factories(
					[]() { return std::make_shared<LUA_PointLight>(); },
					[](const Vec4 &color, float radius) { return std::make_shared<LUA_PointLight>(color, radius); }
				),
				"start", &LUA_PointLight::Start,
				"update", &LUA_PointLight::Update,
				"destroy", &LUA_PointLight::Destroy,
				"enableShadows", &LUA_PointLight::EnableCastShadows,
				"getShadowFar", &LUA_PointLight::GetShadowFar,
				"getLightRadius", &LUA_PointLight::GetLightRadius,
				"setLightRadius", &LUA_PointLight::SetLightRadius,
				"getLightColor", &LUA_PointLight::GetLightColor,
				"setLightColor", &LUA_PointLight::SetLightColor,
				"getLightIntensity", &LUA_PointLight::GetLightIntensity,
				"setLightIntensity", &LUA_PointLight::SetLightIntensity,
				"onUpdate", &LUA_PointLight::on_update,
				"onInit", &LUA_PointLight::on_init,
				"onDestroy", &LUA_PointLight::on_destroy,
				"setShadowPCFTexelSize", &LUA_PointLight::SetShadowPCFTexelSize,
				sol::base_classes, sol::bases<IComponent>()
				);
		}

		{
			// Spot Light - was previously two separate registrations
			// under the same Lua name "SpotLight" (the second silently
			// clobbering the first's method table in sol2's per-type
			// metatable, leaving onUpdate/onInit as effectively the only
			// reachable methods); merged into one, matching the
			// DirectionalLight/PointLight pattern. Also fixes a
			// copy-paste bug: "getLightProjection"/"setLightProjection"
			// were mapped to GetLightDirection/SetLightDirection (no
			// light-projection accessor exists on SpotLight) - renamed
			// to their real names. shared_ptr via sol::factories.
			lua->new_usertype<LUA_SpotLight>("SpotLight",
				sol::factories(
					[]() { return std::make_shared<LUA_SpotLight>(); },
					[](const Vec4 &color, float radius, const Vec3 &direction, float OutterCone, float InnerCone) { return std::make_shared<LUA_SpotLight>(color, radius, direction, OutterCone, InnerCone); }
				),
				"start", &LUA_SpotLight::Start,
				"update", &LUA_SpotLight::Update,
				"destroy", &LUA_SpotLight::Destroy,
				"enableShadows", &LUA_SpotLight::EnableCastShadows,
				"getShadowFar", &LUA_SpotLight::GetShadowFar,
				"getLightDirection", &LUA_SpotLight::GetLightDirection,
				"setLightDirection", &LUA_SpotLight::SetLightDirection,
				"getLightRadius", &LUA_SpotLight::GetLightRadius,
				"setLightRadius", &LUA_SpotLight::SetLightRadius,
				"getLightInnerCone", &LUA_SpotLight::GetLightInnerCone,
				"setLightInnerCone", &LUA_SpotLight::SetLightInnerCone,
				"getLightOutterCone", &LUA_SpotLight::GetLightOutterCone,
				"setLightOutterCone", &LUA_SpotLight::SetLightOutterCone,
				"getLightColor", &LUA_SpotLight::GetLightColor,
				"setLightColor", &LUA_SpotLight::SetLightColor,
				"getLightIntensity", &LUA_SpotLight::GetLightIntensity,
				"setLightIntensity", &LUA_SpotLight::SetLightIntensity,
				"setShadowPCFTexelSize", &LUA_SpotLight::SetShadowPCFTexelSize,
				"onUpdate", &LUA_SpotLight::on_update,
				"onInit", &LUA_SpotLight::on_init,
				"onDestroy", &LUA_SpotLight::on_destroy,
				sol::base_classes, sol::bases<IComponent>()
				);
		}

		{
			// Uniform
			sol::constructors<sol::types<>, sol::types<std::string, int, int>, sol::types<std::string, int, void*, int>> con;
			lua->new_usertype<Uniform>("Uniform",
				con,
				"setValue", &Uniform::SetValue
				);
		}

		{
			// Texture - shared_ptr via sol::factories (Stage 2 / LoadedSceneAssets)
			lua->new_usertype<Texture>("Texture",
				sol::factories(
					[]() { return std::make_shared<Texture>(); }
				),
				"loadTexture", &Texture::LoadTexture,
				"loadTextureFromMemory", &Texture::LoadTextureFromMemory,
				"createEmptyTexture", &Texture::CreateEmptyTexture,
				"setMinMagFilter", &Texture::SetMinMagFilter,
				"setRepeat", &Texture::SetRepeat,
				"enableCompareMode", &Texture::EnableCompareMode,
				"setTransparency", &Texture::SetTransparency,
				"resize", sol::overload(&Texture_Resize3, &Texture_Resize2),
				"updateData", &Texture::UpdateData,
				"UpdateMipmap", &Texture::UpdateMipmap,
				"setTextureByteAlignment", &Texture::SetTextureByteAlignment,
				"getBindID", &Texture::GetBindID,
				"getWidth", &Texture::GetWidth,
				"getHeight", &Texture::GetHeight,
				"bind", &Texture::Bind,
				"unbind", &Texture::Unbind,
				"deleteTexture", &Texture::DeleteTexture,
				"getLastBindedUnit", &Texture::GetLastBindedUnit
				);
		}

		{
			// Shader
			sol::constructors<sol::types<>> con;
			lua->new_usertype<Shader>("Shader",
				con,
				"loadShaderFile", &Shader::LoadShaderFile,
				"loadShaderText", &Shader::LoadShaderText,
				"compileShader", &Shader::CompileShader,
				"deleteShader", &Shader::DeleteShader,
				"linkProgram", &Shader::LinkProgram,
				"shaderProgram", &Shader::ShaderProgram,
				"getUniformLocation", &Shader::GetUniformLocation,
				"getAttributeLocation", &Shader::GetAttributeLocation,
				"SendUniform", sol::overload(
					sol::resolve<void(const Uniform&, const int)>(&Shader::SendUniform),
					sol::resolve<void(const Uniform&, void*, const int, const unsigned int)>(&Shader::SendUniform)
				)
				);
		}

		{
			// GenericShaderMaterial - shared_ptr via sol::factories
			lua->new_usertype<GenericShaderMaterial>("GenericShaderMaterial",
				sol::factories(
					[](int options) { return std::make_shared<GenericShaderMaterial>(options); }
				),
				"setColor", &GenericShaderMaterial::SetColor,
				"setSpecular", &GenericShaderMaterial::SetSpecular,
				"setColorMap", &GenericShaderMaterial::SetColorMap,
				"setSpecularMap", &GenericShaderMaterial::SetSpecularMap,
				"setNormalMap", &GenericShaderMaterial::SetNormalMap,
				"setDisplacementMap", &GenericShaderMaterial::SetDisplacementMap,
				"setDisplacementHeight", &GenericShaderMaterial::SetDisplacementHeight,
				"setEnvMap", &GenericShaderMaterial::SetEnvMap,
				"setSkyboxMap", &GenericShaderMaterial::SetSkyboxMap,
				"setRefractMap", &GenericShaderMaterial::SetRefractMap,
				"addTexture", &GenericShaderMaterial::AddTexture,
				"setTextFont", &GenericShaderMaterial::SetTextFont,
				"setReflectivity", &GenericShaderMaterial::SetReflectivity,
				"setShininess", &GenericShaderMaterial::SetShininess,
				"setMetallic", &GenericShaderMaterial::SetMetallic,
				"setRoughness", &GenericShaderMaterial::SetRoughness,
				"setMetallicRoughnessMap", &GenericShaderMaterial::SetMetallicRoughnessMap,
				"setSSREnabled", &GenericShaderMaterial::SetSSREnabled,
				"bindTextures", &GenericShaderMaterial::BindTextures,
				"unbindTextures", &GenericShaderMaterial::UnbindTextures,
				sol::base_classes, sol::bases<IMaterial>()
				);
		}

		{
			// CustomShaderMaterial - shared_ptr via sol::factories
			lua->new_usertype<CustomShaderMaterial>("CustomShaderMaterial",
				sol::factories(
					[](const std::string &shaderFile) { return std::make_shared<CustomShaderMaterial>(shaderFile); },
					[](Shader* shader) { return std::make_shared<CustomShaderMaterial>(shader); }
				),
				"setShader", &CustomShaderMaterial::SetShader,
				"addSampler", &CustomShaderMaterial::AddSampler,
				sol::base_classes, sol::bases<IMaterial>()
				);
		}
		{
			lua->new_enum("UniformUsage",
				"ProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix,
				"ViewMatrix", Uniforms::DataUsage::ViewMatrix,
				"ModelMatrix", Uniforms::DataUsage::ModelMatrix,
				"CameraPosition", Uniforms::DataUsage::CameraPosition,
				"Timer", Uniforms::DataUsage::Timer,
				"NearFarPlane", Uniforms::DataUsage::NearFarPlane,
				"Other", Uniforms::DataUsage::Other
			);
			lua->new_enum("UniformDataType",
				"Int", Uniforms::DataType::Int,
				"Float", Uniforms::DataType::Float,
				"Vec2", Uniforms::DataType::Vec2,
				"Vec3", Uniforms::DataType::Vec3,
				"Vec4", Uniforms::DataType::Vec4,
				"Matrix", Uniforms::DataType::Matrix
			);
			// Configure IMaterial::extraUniforms[index] from Lua (WaterShader blocks, etc.).
			lua->set_function("setMaterialExtraUniformBlock",
				[](IMaterial &m, int index, uint32 binding, const std::string &blockName, uint32 size, sol::table offsets) {
					std::map<std::string, uint32> off;
					offsets.for_each([&](sol::object key, sol::object value) {
						if (key.get_type() == sol::type::string && value.get_type() == sol::type::number)
							off[key.as<std::string>()] = (uint32)value.as<double>();
					});
					m.SetExtraUniformBlock(index, binding, blockName, size, off);
				});
		}

		{
			// Cube - shared_ptr via sol::factories
			lua->new_usertype<Cube>("Cube",
				sol::factories(
					[](float width, float height, float depth, bool smooth, bool flip, bool TangentBitangent) { return std::make_shared<Cube>(width, height, depth, smooth, flip, TangentBitangent); },
					[](float width, float height, float depth, bool smooth, bool flip) { return std::make_shared<Cube>(width, height, depth, smooth, flip); },
					[](float width, float height, float depth, bool smooth) { return std::make_shared<Cube>(width, height, depth, smooth); },
					[](float width, float height, float depth) { return std::make_shared<Cube>(width, height, depth); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Capsule - shared_ptr via sol::factories
			lua->new_usertype<Capsule>("Capsule",
				sol::factories(
					[](float radius, float height, int numRings, int segmentsW, int segmentsH, bool smooth, bool flip, bool TangentBitangent) { return std::make_shared<Capsule>(radius, height, numRings, segmentsW, segmentsH, smooth, flip, TangentBitangent); },
					[](float radius, float height, int numRings, int segmentsW, int segmentsH, bool smooth, bool flip) { return std::make_shared<Capsule>(radius, height, numRings, segmentsW, segmentsH, smooth, flip); },
					[](float radius, float height, int numRings, int segmentsW, int segmentsH, bool smooth) { return std::make_shared<Capsule>(radius, height, numRings, segmentsW, segmentsH, smooth); },
					[](float radius, float height, int numRings, int segmentsW, int segmentsH) { return std::make_shared<Capsule>(radius, height, numRings, segmentsW, segmentsH); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Cone - shared_ptr via sol::factories
			lua->new_usertype<Cone>("Cone",
				sol::factories(
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded, bool smooth, bool flip, bool TangentBitangent) { return std::make_shared<Cone>(radius, height, segmentsW, segmentsH, openEnded, smooth, flip, TangentBitangent); },
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded, bool smooth, bool flip) { return std::make_shared<Cone>(radius, height, segmentsW, segmentsH, openEnded, smooth, flip); },
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded, bool smooth) { return std::make_shared<Cone>(radius, height, segmentsW, segmentsH, openEnded, smooth); },
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded) { return std::make_shared<Cone>(radius, height, segmentsW, segmentsH, openEnded); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Cylinder - shared_ptr via sol::factories
			lua->new_usertype<Cylinder>("Cylinder",
				sol::factories(
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded, bool smooth, bool flip, bool TangentBitangent) { return std::make_shared<Cylinder>(radius, height, segmentsW, segmentsH, openEnded, smooth, flip, TangentBitangent); },
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded, bool smooth, bool flip) { return std::make_shared<Cylinder>(radius, height, segmentsW, segmentsH, openEnded, smooth, flip); },
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded, bool smooth) { return std::make_shared<Cylinder>(radius, height, segmentsW, segmentsH, openEnded, smooth); },
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded) { return std::make_shared<Cylinder>(radius, height, segmentsW, segmentsH, openEnded); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Plane - shared_ptr via sol::factories
			lua->new_usertype<Plane>("Plane",
				sol::factories(
					[](float width, float height, bool smooth, bool flip, bool TangentBitangent) { return std::make_shared<Plane>(width, height, smooth, flip, TangentBitangent); },
					[](float width, float height, bool smooth, bool flip) { return std::make_shared<Plane>(width, height, smooth, flip); },
					[](float width, float height, bool smooth) { return std::make_shared<Plane>(width, height, smooth); },
					[](float width, float height) { return std::make_shared<Plane>(width, height); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Sphere - shared_ptr via sol::factories
			lua->new_usertype<Sphere>("Sphere",
				sol::factories(
					[](float radius, int segmentsW, int segmentsH, bool smooth, bool HalfSphere, bool flip, bool TangentBitangent) { return std::make_shared<Sphere>(radius, segmentsW, segmentsH, smooth, HalfSphere, flip, TangentBitangent); },
					[](float radius, int segmentsW, int segmentsH, bool smooth, bool HalfSphere, bool flip) { return std::make_shared<Sphere>(radius, segmentsW, segmentsH, smooth, HalfSphere, flip); },
					[](float radius, int segmentsW, int segmentsH, bool smooth, bool HalfSphere) { return std::make_shared<Sphere>(radius, segmentsW, segmentsH, smooth, HalfSphere); },
					[](float radius, int segmentsW, int segmentsH, bool smooth) { return std::make_shared<Sphere>(radius, segmentsW, segmentsH, smooth); },
					[](float radius, int segmentsW, int segmentsH) { return std::make_shared<Sphere>(radius, segmentsW, segmentsH); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Torus - shared_ptr via sol::factories
			lua->new_usertype<Torus>("Torus",
				sol::factories(
					[](float radius, float tube, int segmentsW, int segmentsH, bool smooth, bool flip, bool TangentBitangent) { return std::make_shared<Torus>(radius, tube, segmentsW, segmentsH, smooth, flip, TangentBitangent); },
					[](float radius, float tube, int segmentsW, int segmentsH, bool smooth, bool flip) { return std::make_shared<Torus>(radius, tube, segmentsW, segmentsH, smooth, flip); },
					[](float radius, float tube, int segmentsW, int segmentsH, bool smooth) { return std::make_shared<Torus>(radius, tube, segmentsW, segmentsH, smooth); },
					[](float radius, float tube, int segmentsW, int segmentsH) { return std::make_shared<Torus>(radius, tube, segmentsW, segmentsH); },
					[](float radius, float tube, int segmentsW) { return std::make_shared<Torus>(radius, tube, segmentsW); },
					[](float radius, float tube) { return std::make_shared<Torus>(radius, tube); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// TorusKnot - shared_ptr via sol::factories
			lua->new_usertype<TorusKnot>("TorusKnot",
				sol::factories(
					[](float radius, float tube, int segmentsW, int segmentsH, float p, float q, int heightscale, bool smooth, bool flip, bool TangentBitangent) { return std::make_shared<TorusKnot>(radius, tube, segmentsW, segmentsH, p, q, heightscale, smooth, flip, TangentBitangent); },
					[](float radius, float tube, int segmentsW, int segmentsH, float p, float q, int heightscale, bool smooth, bool flip) { return std::make_shared<TorusKnot>(radius, tube, segmentsW, segmentsH, p, q, heightscale, smooth, flip); },
					[](float radius, float tube, int segmentsW, int segmentsH, float p, float q, int heightscale, bool smooth) { return std::make_shared<TorusKnot>(radius, tube, segmentsW, segmentsH, p, q, heightscale, smooth); },
					[](float radius, float tube, int segmentsW, int segmentsH, float p, float q, int heightscale) { return std::make_shared<TorusKnot>(radius, tube, segmentsW, segmentsH, p, q, heightscale); },
					[](float radius, float tube, int segmentsW, int segmentsH, float p, float q) { return std::make_shared<TorusKnot>(radius, tube, segmentsW, segmentsH, p, q); },
					[](float radius, float tube, int segmentsW, int segmentsH, float p) { return std::make_shared<TorusKnot>(radius, tube, segmentsW, segmentsH, p); },
					[](float radius, float tube, int segmentsW, int segmentsH) { return std::make_shared<TorusKnot>(radius, tube, segmentsW, segmentsH); },
					[](float radius, float tube, int segmentsW) { return std::make_shared<TorusKnot>(radius, tube, segmentsW); },
					[](float radius, float tube) { return std::make_shared<TorusKnot>(radius, tube); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Model - shared_ptr via sol::factories
			lua->new_usertype<Model>("Model",
				sol::factories(
					[](const std::string &path) { return std::make_shared<Model>(path); },
					[](const std::string &path, bool mergeMeshes) { return std::make_shared<Model>(path, mergeMeshes); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Decals - shared_ptr via sol::factories
			lua->new_usertype<Decal>("Decal",
				sol::factories(
					[](std::vector<DecalVertex> vertices, bool haveBones) { return std::make_shared<Decal>(vertices, haveBones); },
					[](std::vector<DecalVertex> vertices) { return std::make_shared<Decal>(vertices); }
				),
				// Renderable listed explicitly alongside Model: sol does not
				// walk a transitive base chain, so listing only Model left
				// Decal unusable everywhere a Renderable* is expected -
				// including RenderingComponent's own constructor.
				sol::base_classes, sol::bases<Model, Renderable>()
				);
		}

		{
			// Font
			sol::constructors<sol::types<std::string, float>> con;
			lua->new_usertype<Font>("Font",
				con,
				"createText", &Font::CreateText,
				"getTexture", &Font::GetTexture,
				"getFontSize", &Font::GetFontSize,
				"getGlyphs", &Font::GetGlyphs
				);
		}

		{
			// Text - shared_ptr via sol::factories
			lua->new_usertype<Text>("Text",
				sol::factories(
					[](Font* font, const std::string &text, float charWidth, float charHeight, const Vec4 &color, bool DynamicText) { return std::make_shared<Text>(font, text, charWidth, charHeight, color, DynamicText); }
				),
				"updateText", sol::overload(
					&Text_UpdateText,
					&Text_UpdateTextColors),
				// Text IS a Renderable, but sol only performs the
				// derived->base pointer conversion for bases listed here -
				// without this, RenderingComponent.new(text, material)
				// failed with "no matching function call takes this number
				// of arguments and the specified types", which made it
				// impossible to draw text at all from Lua. Same class of
				// bug as the SkeletonAnimation one recorded on
				// LUA_RenderingComponent above.
				sol::base_classes, sol::bases<Renderable>()
				);
		}


		// RenderingComponent.new(mesh, material) - factories take sol::object
		// and convert explicitly (see LuaObjectToRenderable). SOL_CHECK_ARGUMENTS
		// will not match shared_ptr<Cube> to shared_ptr<Renderable> on its own.
		{
			lua->new_usertype<LUA_RenderingComponent>("RenderingComponent",
				sol::factories(
					&LuaNewRenderingComponentDist,
					&LuaNewRenderingComponent
				),
				"addLOD", sol::overload(
					&RenderingComponent_ADDLOD,
					&RenderingComponent_ADDLOD_DistOnly
				),
				"enable", &IComponent::Enable,
				"disable", &IComponent::Disable,
				"isActive", &IComponent::IsActive,
				"init", &LUA_RenderingComponent::Init,
				"update", &LUA_RenderingComponent::Update,
				"destroy", &LUA_RenderingComponent::Destroy,
				"setCullingGeometry", &LUA_RenderingComponent::SetCullingGeometry,
				"enableCullTest", &LUA_RenderingComponent::EnableCullTest,
				"disableCullTest", &LUA_RenderingComponent::DisableCullTest,
				"isCullTesting", &LUA_RenderingComponent::IsCullTesting,
				"enableCastShadows", &LUA_RenderingComponent::EnableCastShadows,
				"disableCastShadows", &LUA_RenderingComponent::DisableCastShadows,
				"isCastingShadows", &LUA_RenderingComponent::IsCastingShadows,
				"getRenderable", &LUA_RenderingComponent::GetRenderable,
				"getSkeleton", &LUA_RenderingComponent::GetSkeleton,
				"hasBones", &LUA_RenderingComponent::HasBones,
				"getMeshes", [](RenderingComponent &rc) -> std::vector<RenderingMesh*> & {
					// sol2 does not honour C++ default args - GetMeshes(LOD=0)
					// called as getMeshes() from Lua was failing with
					// "expected number, received no value", which aborted
					// Island's setIslandCull every frame (reflection cull)
					// and texture_anim.lua's init.
					return rc.GetMeshes(0);
				},
				"getMeshesLOD", &RenderingComponent::GetMeshes,
				"getLODSize", &LUA_RenderingComponent::GetLODSize,
				"getLODByDistance", &LUA_RenderingComponent::GetLODByDistance,
				"updateLOD", &LUA_RenderingComponent::UpdateLOD,
				"getComponents", &LUA_RenderingComponent::GetComponents,
				"getActiveSkeletonAnimation", &RenderingComponent_GetActiveSkeletonAnimation,
				"getActiveTextureAnimation", &RenderingComponent_GetActiveTextureAnimation,
				"onUpdate", &LUA_RenderingComponent::on_update,
				"onInit", &LUA_RenderingComponent::on_init,
				"onDestroy", &LUA_RenderingComponent::on_destroy,
				// RenderingComponent, not just IComponent: sol only performs
				// a derived->base pointer conversion for bases listed here,
				// so with IComponent alone every member bound as a free
				// function taking RenderingComponent& (getActiveSkeletonAnimation
				// / addLOD) rejected its own object with "expected userdata,
				// received sol.p3d::LUA_RenderingComponent *: value at this
				// index does not properly reflect the desired type". That is
				// what left the Skeleton Animation demo unanimated - its
				// script could never obtain the SkeletonAnimationInstance, so
				// SkeletonAnimation::Update() never ran and the model
				// rendered from uninitialised bone matrices.
				sol::base_classes, sol::bases<RenderingComponent, IComponent>()
				);
		}

        {
            // LUA RenderingInstancedComponent - shared_ptr via sol::factories
            lua->new_usertype<LUA_RenderingInstancedComponent>("RenderingInstancedComponent",
                sol::factories(
                    [](const std::shared_ptr<Renderable>& renderable, const std::shared_ptr<IMaterial>& Material, int nrInstances, float boundingSphere) { return std::make_shared<LUA_RenderingInstancedComponent>(renderable, Material, nrInstances, boundingSphere); },
                    [](const std::shared_ptr<Renderable>& renderable, int MaterialProperties, int nrInstances, float boundingSphere) { return std::make_shared<LUA_RenderingInstancedComponent>(renderable, MaterialProperties, nrInstances, boundingSphere); }
                ),
                "init", &LUA_RenderingInstancedComponent::Init,
                "update", &LUA_RenderingInstancedComponent::Update,
                "destroy", &LUA_RenderingInstancedComponent::Destroy,
                "setCullingGeometry", &LUA_RenderingInstancedComponent::SetCullingGeometry, "enableCullTest", &LUA_RenderingInstancedComponent::EnableCullTest,
                "disableCullTest", &LUA_RenderingInstancedComponent::DisableCullTest,
                "isCullTesting", &LUA_RenderingInstancedComponent::IsCullTesting,
                "enableCastShadows", &LUA_RenderingInstancedComponent::EnableCastShadows,
                "disableCastShadows", &LUA_RenderingInstancedComponent::DisableCastShadows,
                "isCastingShadows", &LUA_RenderingInstancedComponent::IsCastingShadows,
                "getRenderable", &LUA_RenderingInstancedComponent::GetRenderable,
                "getSkeleton", &LUA_RenderingInstancedComponent::GetSkeleton,
                "hasBones", &LUA_RenderingInstancedComponent::HasBones,
                "getMeshes", &LUA_RenderingInstancedComponent::GetMeshes,
                "getLODSize", &LUA_RenderingInstancedComponent::GetLODSize,
                "getLODByDistance", &LUA_RenderingInstancedComponent::GetLODByDistance,
                "updateLOD", &LUA_RenderingInstancedComponent::UpdateLOD,
                "getComponents", &LUA_RenderingInstancedComponent::GetComponents,
                "addBuffer", &LUA_RenderingInstancedComponent::AddBuffer,
                "removeBuffer", &LUA_RenderingInstancedComponent::RemoveBuffer,
                "numberOfInstances", &LUA_RenderingInstancedComponent::NumberOfInstances,
                "setNumberInstances", &LUA_RenderingInstancedComponent::SetNumberInstances,
                "onUpdate", &LUA_RenderingInstancedComponent::on_update,
                "onInit", &LUA_RenderingInstancedComponent::on_init,
                "onDestroy", &LUA_RenderingInstancedComponent::on_destroy,
                sol::base_classes, sol::bases<IComponent>()
                );
        }

		{
			// Skeleton Animation
			sol::constructors<sol::types<>> con;
			lua->new_usertype<SkeletonAnimation>("SekeletonAnimation",
				con,
				"loadAnimation", &SkeletonAnimation::LoadAnimation,
				"getNumberAnimatons", &SkeletonAnimation::GetNumberAnimations,
				"update", &SkeletonAnimation::Update,
				"createInstance", &SkeletonAnimation::CreateInstance,
				"destroyInstance", &SkeletonAnimation::DestroyInstance,
				"getAnimationIDByName", &SkeletonAnimation::GetAnimationIDByName
				);
		}

		{
			// Skeleton Animation Instance
			sol::constructors<sol::types<SkeletonAnimation*, RenderingComponent*>> con;
			lua->new_usertype<SkeletonAnimationInstance>("SekeletonAnimationInstance",
				con,
				"getOwner", &SkeletonAnimationInstance::GetOwner,
				"play", &SkeletonAnimationInstance::Play,
				"changeProperties", &SkeletonAnimationInstance::ChangeProperties,
				"pause", &SkeletonAnimationInstance::Pause,
				"PauseAnimation", &SkeletonAnimationInstance::PauseAnimation,
				"resumeAnimation", &SkeletonAnimationInstance::ResumeAnimation,
				"resume", &SkeletonAnimationInstance::Resume,
				"stopAnimation", &SkeletonAnimationInstance::StopAnimation,
				"stop", &SkeletonAnimationInstance::Stop,
				"getAnimationCurrentProgress", &SkeletonAnimationInstance::GetAnimationCurrentProgress,
				"getAnimationDuration", &SkeletonAnimationInstance::GetAnimationDuration,
				"getAnimationCurrentTime", &SkeletonAnimationInstance::GetAnimationCurrentTime,
				"getAniamtionSpeed", &SkeletonAnimationInstance::GetAnimationSpeed,
				"getAnimationStartTime", &SkeletonAnimationInstance::GetAnimationStartTime,
				"getAnimationID", &SkeletonAnimationInstance::GetAnimationID,
				"getAnimationScale", &SkeletonAnimationInstance::GetAnimationScale,
				"createLayer", &SkeletonAnimationInstance::CreateLayer,
				"addBone", sol::overload(
					&SkeletonAnimationInstance_AddBone,
					&SkeletonAnimationInstance_AddBoneSTR
				),
				"addBoneAndChilds", sol::overload(
					&SkeletonAnimationInstance_AddBoneAndChilds,
					&SkeletonAnimationInstance_AddBoneAndChildsSTR
				),
				"removeBone", sol::overload(
					&SkeletonAnimationInstance_RemoveBone,
					&SkeletonAnimationInstance_RemoveBoneSTR
				),
				"removeBoneAndChilds", sol::overload(
					&SkeletonAnimationInstance_RemoveBoneAndChilds,
					&SkeletonAnimationInstance_RemoveBoneAndChildsSTR
				),
				"destroyLayer", sol::overload(
					&SkeletonAnimationInstance_DestroyLayer,
					&SkeletonAnimationInstance_DestroyLayerSTR
				),
				"isPaused", sol::overload(
					&SkeletonAnimationInstance_IsPaused,
					&SkeletonAnimationInstance_IsPausedID
				)
				);
		}

		{
			// Texture Animation
			sol::constructors<sol::types<>> con;
			lua->new_usertype<TextureAnimation>("TextureAnimation",
				con,
				"getFrame", &TextureAnimation::GetFrame,
				"getFrameShared", &TextureAnimation::GetFrameShared,
				"getNumberFrames", &TextureAnimation::GetNumberFrames,
				"addFrame", &TextureAnimation::AddFrame,
				"update", &TextureAnimation::Update,
				"createInstance", &TextureAnimation::CreateInstance,
				"destroyInstance", &TextureAnimation::DestroyInstance
				);
		}

		{
			// Texture Animation Instance
			sol::constructors<sol::types<TextureAnimation*, float>> con;
			lua->new_usertype<TextureAnimationInstance>("TextureAnimationInstance",
				con,
				"play", &TextureAnimationInstance::Play,
				"pause", &TextureAnimationInstance::Pause,
				"stop", &TextureAnimationInstance::Stop,
				"isPlaying", &TextureAnimationInstance::IsPlaying,
				"yoyo", &TextureAnimationInstance::YoYo,
				// shared_ptr - SetColorMap/etc. expect shared ownership;
				// binding the raw GetTexture() made sol invent a bogus
				// shared_ptr from Texture* and SEGV in SetColorMap.
				"getTexture", &TextureAnimationInstance::GetTextureShared,
				"getFrame", &TextureAnimationInstance::GetFrame,
				"getOwner", &TextureAnimationInstance::GetOwner
				);
		}

		{
			// IPhysics
			lua->new_usertype<IPhysics>("IPhysics",
				"initPhysics", &IPhysics::InitPhysics,
				"enableDebugDraw", &IPhysics::EnableDebugDraw,
				"renderDebugDraw", &IPhysics::RenderDebugDraw,
				"disableDebugDraw", &IPhysics::DisableDebugDraw,
				"update", &IPhysics::Update,
				"endPhysics", &IPhysics::EndPhysics,
				"RemovePhysicsComponent", &IPhysics::RemovePhysicsComponent,
				"UpdateTransformations", &IPhysics::UpdateTransformations,
				"UpdatePosition", &IPhysics::UpdatePosition,
				"UpdateRotation", &IPhysics::UpdateRotation,
				"CleanForces", &IPhysics::CleanForces,
				"SetAngularVelocity", &IPhysics::SetAngularVelocity,
				"SetLinearVelocity", &IPhysics::SetLinearVelocity,
				"Activate", &IPhysics::Activate,
				"rayCast", &IPhysics::RayCast,
				"createBox", &IPhysics::CreateBox,
				"createCapsule", &IPhysics::CreateCapsule,
				"createCone", &IPhysics::CreateCone,
				"createConvexHull", &IPhysics::CreateConvexHull,
				"createCylinder", &IPhysics::CreateCylinder,
				"createMultiplerSphere", &IPhysics::CreateMultipleSphere,
				"createSphere", &IPhysics::CreateSphere,
				"createStaticPlane", &IPhysics::CreateStaticPlane,
				"createVehicle", &IPhysics::CreateVehicle,
				"addWheel", &IPhysics::AddWheel,
				"createTriangleMesh", sol::overload(
					&IPhysics_CreateTriangleMesh,
					&IPhysics_CreateTriangleMeshRCOMP
				),
				"createConvexTriangleMesh", sol::overload(
					&IPhysics_CreateConvexTriangleMesh,
					&IPhysics_CreateConvexTriangleMeshRCOMP
				)
				);
		}

		{
			// Bullet Physics
			sol::constructors<sol::types<>> con;
			lua->new_usertype<BulletPhysics>("BulletPhysics",
				con,
				sol::base_classes, sol::bases<IPhysics>()
				);
		}

		{
			// Post Effects Manager
			sol::constructors<sol::types<int, int>> con;
			lua->new_usertype<PostEffectsManager>("PostEffectsManager",
				con,
				"resize", &PostEffectsManager::Resize,
				"captureFrame", &PostEffectsManager::CaptureFrame,
				"endCapture", &PostEffectsManager::EndCapture,
				"processPostEffects", &PostEffectsManager::ProcessPostEffects,
				"addEffect", &PostEffectsManager::AddEffect,
				"removeEffect", &PostEffectsManager::RemoveEffect,
				"removeAllEffects", &PostEffectsManager::RemoveAllEffects,
				"getNumberEffects", &PostEffectsManager::GetNumberEffects,
				"getExternalFrameBuffer", &PostEffectsManager::GetExternalFrameBuffer,
				"getColor", &PostEffectsManager::GetColor,
				"getDepth", &PostEffectsManager::GetDepth,
				"getLastRTT", &PostEffectsManager::GetLastRTT
				);
		}
		{
			// RTT source flags for IEffect constructors (TonemapEffect, etc.)
			lua->new_enum("RTT",
				"Color", RTT::Color,
				"Depth", RTT::Depth,
				"LastRTT", RTT::LastRTT,
				"CustomTexture", RTT::CustomTexture
			);
		}
		{
			// Named post-effect factory - AddEffect takes ownership (see
			// PostEffectsManager::RemoveAllEffects), so Lua must not also
			// own a TonemapEffect userdata that would double-delete on GC.
			lua->set_function("addPostEffect", [](PostEffectsManager &m, const std::string &name, int width, int height) {
				if (name == "tonemap")
					m.AddEffect(new TonemapEffect(RTT::Color, width, height));
				else
					echo("ERROR: addPostEffect - unknown effect '" + name + "'");
			});

			// SSAO full chain (ssao → blur → composite). Returns nothing;
			// per-frame view matrix via ssaoSetViewMatrix. Handles cleared
			// by clearPostEffectHandles() when the chain is torn down.
			static SSAOEffect *g_ssao = nullptr;
			static BlurSSAOEffect *g_ssaoBlur = nullptr;
			static VelocityRenderer *g_velocityRenderer = nullptr;
			static MotionBlurEffect *g_motionBlur = nullptr;
			lua->set_function("clearPostEffectHandles", []() {
				g_ssao = nullptr;
				g_ssaoBlur = nullptr;
				// Null the MotionBlurEffect handle only - the effect itself is
				// owned by PostEffectsManager and deleted in removeAllEffects.
				// VelocityRenderer owns the velocity texture the effect samples,
				// so it must outlive that delete (see destroyMotionBlurVelocity).
				g_motionBlur = nullptr;
				Texture::ResetUnitCounter();
			});
			lua->set_function("destroyMotionBlurVelocity", []() {
				delete g_velocityRenderer;
				g_velocityRenderer = nullptr;
			});
			lua->set_function("buildMotionBlurPostChain", [](PostEffectsManager &m, int width, int height) {
				delete g_velocityRenderer;
				g_velocityRenderer = new VelocityRenderer((uint32)width, (uint32)height);
				g_motionBlur = new MotionBlurEffect(RTT::Color, g_velocityRenderer->GetTexture(), (uint32)width, (uint32)height);
				g_motionBlur->SetTargetFPS(60.0f);
				g_motionBlur->SetCurrentFPS(60.0f);
				m.AddEffect(g_motionBlur);
			});
			lua->set_function("motionBlurRenderVelocity", [](const Projection &proj, GameObject *cam, SceneGraph *scene) {
				if (g_velocityRenderer && cam && scene)
					g_velocityRenderer->RenderVelocityMap(proj, cam, scene);
			});
			lua->set_function("motionBlurSetFPS", [](float currentFps) {
				if (g_motionBlur)
					g_motionBlur->SetCurrentFPS(currentFps);
			});
			lua->set_function("motionBlurResize", [](int width, int height) {
				if (g_velocityRenderer)
					g_velocityRenderer->Resize((uint32)width, (uint32)height);
				if (g_motionBlur)
					g_motionBlur->Resize((uint32)width, (uint32)height);
			});
			lua->set_function("motionBlurActive", []() {
				return g_velocityRenderer != nullptr && g_motionBlur != nullptr;
			});
			lua->set_function("buildSSAOPostChain", [](PostEffectsManager &m, int width, int height) {
				g_ssao = new SSAOEffect(RTT::Depth, width, height);
				g_ssao->SetRadius(0.2f);
				g_ssao->SetStrength(1.5f);
				g_ssao->SetTreshOld(2.0f);
				g_ssao->SetScale(1.0f);
				g_ssaoBlur = new BlurSSAOEffect(RTT::LastRTT, width, height);
				m.AddEffect(g_ssao);
				m.AddEffect(g_ssaoBlur);
				m.AddEffect(new SSAOCompositeEffect(RTT::Color, RTT::LastRTT, width, height));
			});
			lua->set_function("ssaoSetViewMatrix", [](const Matrix &m) {
				if (g_ssao) g_ssao->SetViewMatrix(m);
			});
			lua->set_function("ssaoSetParams", [](float radius, float strength, float threshold, float scale, float blurIntensity) {
				if (g_ssao)
				{
					g_ssao->SetRadius(radius);
					g_ssao->SetStrength(strength);
					g_ssao->SetTreshOld(threshold);
					g_ssao->SetScale(scale);
				}
				if (g_ssaoBlur) g_ssaoBlur->SetIntensity(blurIntensity);
			});
			lua->set_function("buildDOFPostChain", [](PostEffectsManager &m, int width, int height) {
				auto *blurX = new BlurXEffect(RTT::Color, width, height);
				auto *blurY = new BlurYEffect(RTT::LastRTT, width, height);
				Texture *fullRes = blurY->GetTexture();
				const int lw = (int)(width * 0.25f), lh = (int)(height * 0.25f);
				auto *resize = new ResizeEffect(RTT::Color, lw, lh);
				auto *blurXlow = new BlurXEffect(RTT::LastRTT, lw, lh);
				auto *blurYlow = new BlurYEffect(RTT::LastRTT, lw, lh);
				Texture *lowRes = blurYlow->GetTexture();
				m.AddEffect(blurX);
				m.AddEffect(blurY);
				m.AddEffect(resize);
				m.AddEffect(blurXlow);
				m.AddEffect(blurYlow);
				m.AddEffect(new DepthOfFieldEffect(lowRes, fullRes, width, height));
			});

			lua->new_enum("CullingMode",
				"FrustumCulling", CullingMode::FrustumCulling
			);
		}
		{
			// IEffect
			sol::constructors<sol::types<int, int>> con;
			lua->new_usertype<IEffect>("IEffect",
				con,
				"compileShaders", &IEffect::CompileShaders,
				"resize", &IEffect::Resize,
				"getWidth", &IEffect::GetWidth,
				"getHeight", &IEffect::GetHeight,
				"getTexture", &IEffect::GetTexture
				);
		}
		{
			// Bloom
			sol::constructors<sol::types<int, int, int>> con;
			lua->new_usertype<BloomEffect>("BloomEffect",
				con,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// BlurXEffect
			sol::constructors<sol::types<int, int, int>> con;
			lua->new_usertype<BlurXEffect>("BlurXEffect",
				con,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// BlurYEffect
			sol::constructors<sol::types<int, int, int>> con;
			lua->new_usertype<BlurYEffect>("BlurYEffect",
				con,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// Resize Effect
			sol::constructors<sol::types<int, int, int>> con;
			lua->new_usertype<ResizeEffect>("ResizeEffect",
				con,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// RTT Debug
			sol::constructors<sol::types<int, int, int, int>> con;
			lua->new_usertype<RTTDebug>("RTTDebug",
				con,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// SSAO Effect
			sol::constructors<sol::types<int, int, int>> con;
			lua->new_usertype<SSAOEffect>("SSAOEffect",
				con,
				"setViewMatrix", &SSAOEffect::SetViewMatrix,
				"setRadius", &SSAOEffect::SetRadius,
				"setStrength", &SSAOEffect::SetStrength,
				"setScale", &SSAOEffect::SetScale,
				"setTreshOld", &SSAOEffect::SetTreshOld,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// VigneteEffec
			sol::constructors<sol::types<int, int, int, float, float>> con;
			lua->new_usertype<VignetteEffect>("VignetEffect",
				con,
				"setRadius", &VignetteEffect::SetRadius,
				"setSoftness", &VignetteEffect::SetSoftness,
				sol::base_classes, sol::bases<IEffect>()
				);
		}

		{
			// MotionBlur
			sol::constructors<sol::types<int, Texture*, int, int>> con;
			lua->new_usertype<MotionBlurEffect>("MotionBlur",
				con,
				"setTargetFPS", &MotionBlurEffect::SetTargetFPS,
				"setCurrentFPS", &MotionBlurEffect::SetCurrentFPS,
				sol::base_classes, sol::bases<IEffect>()
				);
		}

		{
			lua->new_enum("ParticleBlendMode",
				"AlphaBlend", ParticleBlendMode::AlphaBlend,
				"Additive", ParticleBlendMode::Additive
			);

			// ParticleSystemDesc - plain data, bound as a field-assignable
			// usertype (construct with ParticleSystemDesc.new(), set the
			// fields that matter, pass to ParticleSystem.new(desc)) rather
			// than a giant constructor overload - matches how the C++ side
			// itself is designed (every field has a sane default from
			// ParticleSystemDesc()).
			sol::constructors<sol::types<>> descCon;
			lua->new_usertype<ParticleSystemDesc>("ParticleSystemDesc",
				descCon,
				"maxParticles", &ParticleSystemDesc::maxParticles,
				"texture", &ParticleSystemDesc::texture,
				"looping", &ParticleSystemDesc::looping,
				"emissionRate", &ParticleSystemDesc::emissionRate,
				"burstCount", &ParticleSystemDesc::burstCount,
				"minLifetime", &ParticleSystemDesc::minLifetime,
				"maxLifetime", &ParticleSystemDesc::maxLifetime,
				"direction", &ParticleSystemDesc::direction,
				"spreadAngle", &ParticleSystemDesc::spreadAngle,
				"minSpeed", &ParticleSystemDesc::minSpeed,
				"maxSpeed", &ParticleSystemDesc::maxSpeed,
				"gravity", &ParticleSystemDesc::gravity,
				"damping", &ParticleSystemDesc::damping,
				"startSize", &ParticleSystemDesc::startSize,
				"endSize", &ParticleSystemDesc::endSize,
				"sizeRandomJitter", &ParticleSystemDesc::sizeRandomJitter,
				"startColor", &ParticleSystemDesc::startColor,
				"endColor", &ParticleSystemDesc::endColor,
				"fadeInFraction", &ParticleSystemDesc::fadeInFraction,
				"fadeOutFraction", &ParticleSystemDesc::fadeOutFraction,
				"minRotationSpeed", &ParticleSystemDesc::minRotationSpeed,
				"maxRotationSpeed", &ParticleSystemDesc::maxRotationSpeed,
				"blendMode", &ParticleSystemDesc::blendMode,
				"boundingSphereRadius", &ParticleSystemDesc::boundingSphereRadius
				);

			lua->new_usertype<ParticleSystem>("ParticleSystem",
				sol::factories(
					[](const ParticleSystemDesc &desc) { return std::make_shared<ParticleSystem>(desc); }
				),
				"update", &ParticleSystem::Update,
				"play", &ParticleSystem::Play,
				"stop", &ParticleSystem::Stop,
				"clear", &ParticleSystem::Clear,
				"getLiveParticleCount", &ParticleSystem::GetLiveParticleCount,
				"isPlaying", &ParticleSystem::IsPlaying,
				"setEmissionRate", &ParticleSystem::SetEmissionRate,
				"setBurstCount", &ParticleSystem::SetBurstCount,
				"setLifetime", &ParticleSystem::SetLifetime,
				"setDirection", &ParticleSystem::SetDirection,
				"setSpread", &ParticleSystem::SetSpread,
				"setSpeed", &ParticleSystem::SetSpeed,
				"setGravity", &ParticleSystem::SetGravity,
				"setDamping", &ParticleSystem::SetDamping,
				"setSizes", &ParticleSystem::SetSizes,
				"setColors", &ParticleSystem::SetColors,
				"setFade", &ParticleSystem::SetFade,
				"setRotationSpeed", &ParticleSystem::SetRotationSpeed,
				"setBlendMode", &ParticleSystem::SetBlendMode,
				sol::base_classes, sol::bases<IComponent>()
				);
		}

		{
			// ******************************* Audio *******************************

			lua->new_enum("AttenuationModel",
				"None", AttenuationModel::None,
				"Inverse", AttenuationModel::Inverse,
				"Linear", AttenuationModel::Linear,
				"Exponential", AttenuationModel::Exponential
			);

			lua->new_enum("AudioFilterType",
				"None", AudioFilterType::None,
				"LowPass", AudioFilterType::LowPass,
				"HighPass", AudioFilterType::HighPass,
				"BandPass", AudioFilterType::BandPass
			);

			lua->new_enum("AudioEQType",
				"None", AudioEQType::None,
				"Peak", AudioEQType::Peak,
				"Notch", AudioEQType::Notch,
				"LowShelf", AudioEQType::LowShelf,
				"HighShelf", AudioEQType::HighShelf
			);

			// AudioManager - construct exactly one and keep it alive; see the
			// class comment for the active-manager registration this relies on.
			sol::constructors<sol::types<>> audioCon;
			lua->new_usertype<AudioManager>("AudioManager",
				audioCon,
				"isInitialized", &AudioManager::IsInitialized,
				"setMasterVolume", &AudioManager::SetMasterVolume,
				"getMasterVolume", &AudioManager::GetMasterVolume,
				// `dt` overloads spelled out: sol binds a function's full
				// arity, so SetListener()/SetListenerFromGameObject()'s C++
				// default (dt=0, "no Doppler this call") is not optional from
				// Lua - same reason Sound's play()/playAt() below do this.
				// The real per-frame call is the 4-argument one; the shorter
				// ones are for a one-off placement/teleport with no velocity.
				"setListener", sol::overload(
					[](AudioManager &a, const Vec3 &position, const Vec3 &forward) { a.SetListener(position, forward); },
					[](AudioManager &a, const Vec3 &position, const Vec3 &forward, const Vec3 &up) { a.SetListener(position, forward, up); },
					[](AudioManager &a, const Vec3 &position, const Vec3 &forward, const Vec3 &up, const f32 dt) { a.SetListener(position, forward, up, dt); }
				),
				// The common case - point the listener at the camera object.
				// Call it after scene:update(), which is what refreshes the
				// world transform this reads, with dt as this frame's real
				// time step so Doppler has something to compute from.
				"setListenerFromGameObject", sol::overload(
					[](AudioManager &a, GameObject* object) { a.SetListenerFromGameObject(object); },
					[](AudioManager &a, GameObject* object, const f32 dt) { a.SetListenerFromGameObject(object, dt); }
				),
				"getListenerPosition", &AudioManager::GetListenerPosition
				);

			// AudioBus - a named submix ("Music", "SFX"). Always shared_ptr-
			// managed (sol::factories, not sol::constructors) - see the class
			// comment for why: a Sound/AudioSource routed through a bus keeps
			// its own shared_ptr to it, so the bus can't be destroyed out
			// from under something still playing through it. `AudioBus.new()`
			// takes an optional parent bus (also a shared_ptr<AudioBus>) to
			// nest submixes.
			lua->new_usertype<AudioBus>("AudioBus",
				sol::factories(
					[]() { return std::make_shared<AudioBus>(); },
					[](std::shared_ptr<AudioBus> parent) { return std::make_shared<AudioBus>(parent); }
				),
				"isValid", &AudioBus::IsValid,
				"setVolume", &AudioBus::SetVolume,
				"getVolume", &AudioBus::GetVolume,
				"setPitch", &AudioBus::SetPitch,
				"getPitch", &AudioBus::GetPitch,
				"pause", &AudioBus::Pause,
				"resume", &AudioBus::Resume,
				"fadeIn", &AudioBus::FadeIn,
				"fadeOut", &AudioBus::FadeOut
				);

			// Sound - pooled one-shot effects.
			sol::constructors<
				sol::types<std::string>,
				sol::types<std::string, uint32>,
				sol::types<std::string, uint32, std::shared_ptr<AudioBus>>
			> soundCon;
			lua->new_usertype<Sound>("Sound",
				soundCon,
				"isLoaded", &Sound::IsLoaded,
				"getFile", &Sound::GetFile,
				// Defaults spelled out as overloads: sol binds a function's
				// full arity, so C++ default arguments are not optional from
				// Lua (the same reason DeferredRenderer_RenderScene above
				// needs its 3-argument wrapper).
				"play", sol::overload(
					[](Sound &s) { s.Play(); },
					[](Sound &s, const f32 volume) { s.Play(volume); },
					[](Sound &s, const f32 volume, const f32 pitch) { s.Play(volume, pitch); },
					[](Sound &s, const f32 volume, const f32 pitch, const f32 pan) { s.Play(volume, pitch, pan); }
				),
				"playAt", sol::overload(
					[](Sound &s, const Vec3 &position) { s.PlayAt(position); },
					[](Sound &s, const Vec3 &position, const f32 volume) { s.PlayAt(position, volume); },
					[](Sound &s, const Vec3 &position, const f32 volume, const f32 pitch) { s.PlayAt(position, volume, pitch); }
				),
				"stop", &Sound::Stop,
				"getPlayingCount", &Sound::GetPlayingCount,
				"setAttenuation", &Sound::SetAttenuation,
				"setFilter", sol::overload(
					[](Sound &s, const uint32 type, const f32 cutoffHz) { s.SetFilter(type, cutoffHz); },
					[](Sound &s, const uint32 type, const f32 cutoffHz, const uint32 order) { s.SetFilter(type, cutoffHz, order); }
				),
				"clearFilter", &Sound::ClearFilter,
				"getFilterType", &Sound::GetFilterType,
				"getFilterCutoff", &Sound::GetFilterCutoff,
				"getFilterOrder", &Sound::GetFilterOrder,
				"setEQ", sol::overload(
					[](Sound &s, const uint32 type, const f32 frequencyHz, const f32 gainDB) { s.SetEQ(type, frequencyHz, gainDB); },
					[](Sound &s, const uint32 type, const f32 frequencyHz, const f32 gainDB, const f32 q) { s.SetEQ(type, frequencyHz, gainDB, q); }
				),
				"clearEQ", &Sound::ClearEQ,
				"getEQType", &Sound::GetEQType,
				"getEQFrequency", &Sound::GetEQFrequency,
				"getEQGain", &Sound::GetEQGain,
				"getEQQ", &Sound::GetEQQ,
				"setDelay", sol::overload(
					[](Sound &s, const f32 delaySeconds, const f32 decay) { s.SetDelay(delaySeconds, decay); },
					[](Sound &s, const f32 delaySeconds, const f32 decay, const f32 wet) { s.SetDelay(delaySeconds, decay, wet); },
					[](Sound &s, const f32 delaySeconds, const f32 decay, const f32 wet, const f32 dry) { s.SetDelay(delaySeconds, decay, wet, dry); }
				),
				"clearDelay", &Sound::ClearDelay,
				"hasDelay", &Sound::HasDelay,
				"getDelaySeconds", &Sound::GetDelaySeconds,
				"getDelayDecay", &Sound::GetDelayDecay,
				"getDelayWet", &Sound::GetDelayWet,
				"getDelayDry", &Sound::GetDelayDry
				);

			// AudioSource - a positional emitter component. shared_ptr via
			// sol::factories (same pattern as AudioBus / Stage 1 components).
			lua->new_usertype<AudioSource>("AudioSource",
				sol::factories(
					[](const std::string &file) { return std::make_shared<AudioSource>(file); },
					[](const std::string &file, bool stream) { return std::make_shared<AudioSource>(file, stream); },
					[](const std::string &file, bool stream, std::shared_ptr<AudioBus> bus) { return std::make_shared<AudioSource>(file, stream, bus); }
				),
				"isLoaded", &AudioSource::IsLoaded,
				"getFile", &AudioSource::GetFile,
				"play", &AudioSource::Play,
				"pause", &AudioSource::Pause,
				"stop", &AudioSource::Stop,
				"isPlaying", &AudioSource::IsPlaying,
				"setLooping", &AudioSource::SetLooping,
				"isLooping", &AudioSource::IsLooping,
				"setVolume", &AudioSource::SetVolume,
				"getVolume", &AudioSource::GetVolume,
				"setPitch", &AudioSource::SetPitch,
				"getPitch", &AudioSource::GetPitch,
				"setPan", &AudioSource::SetPan,
				"getPan", &AudioSource::GetPan,
				"fadeIn", &AudioSource::FadeIn,
				"fadeOut", &AudioSource::FadeOut,
				"setSpatialization", &AudioSource::SetSpatialization,
				"isSpatialized", &AudioSource::IsSpatialized,
				"setAttenuation", &AudioSource::SetAttenuation,
				"setCone", &AudioSource::SetCone,
				"clearCone", &AudioSource::ClearCone,
				"setDirectionalAttenuation", &AudioSource::SetDirectionalAttenuation,
				"setDopplerFactor", &AudioSource::SetDopplerFactor,
				"resetVelocityTracking", &AudioSource::ResetVelocityTracking,
				"setFilter", sol::overload(
					[](AudioSource &a, const uint32 type, const f32 cutoffHz) { a.SetFilter(type, cutoffHz); },
					[](AudioSource &a, const uint32 type, const f32 cutoffHz, const uint32 order) { a.SetFilter(type, cutoffHz, order); }
				),
				"clearFilter", &AudioSource::ClearFilter,
				"getFilterType", &AudioSource::GetFilterType,
				"getFilterCutoff", &AudioSource::GetFilterCutoff,
				"getFilterOrder", &AudioSource::GetFilterOrder,
				"setEQ", sol::overload(
					[](AudioSource &a, const uint32 type, const f32 frequencyHz, const f32 gainDB) { a.SetEQ(type, frequencyHz, gainDB); },
					[](AudioSource &a, const uint32 type, const f32 frequencyHz, const f32 gainDB, const f32 q) { a.SetEQ(type, frequencyHz, gainDB, q); }
				),
				"clearEQ", &AudioSource::ClearEQ,
				"getEQType", &AudioSource::GetEQType,
				"getEQFrequency", &AudioSource::GetEQFrequency,
				"getEQGain", &AudioSource::GetEQGain,
				"getEQQ", &AudioSource::GetEQQ,
				"setDelay", sol::overload(
					[](AudioSource &a, const f32 delaySeconds, const f32 decay) { a.SetDelay(delaySeconds, decay); },
					[](AudioSource &a, const f32 delaySeconds, const f32 decay, const f32 wet) { a.SetDelay(delaySeconds, decay, wet); },
					[](AudioSource &a, const f32 delaySeconds, const f32 decay, const f32 wet, const f32 dry) { a.SetDelay(delaySeconds, decay, wet, dry); }
				),
				"clearDelay", &AudioSource::ClearDelay,
				"hasDelay", &AudioSource::HasDelay,
				"getDelaySeconds", &AudioSource::GetDelaySeconds,
				"getDelayDecay", &AudioSource::GetDelayDecay,
				"getDelayWet", &AudioSource::GetDelayWet,
				"getDelayDry", &AudioSource::GetDelayDry,
				"getLengthSeconds", &AudioSource::GetLengthSeconds,
				"getCursorSeconds", &AudioSource::GetCursorSeconds,
				"seekSeconds", &AudioSource::SeekSeconds,
				"atEnd", &AudioSource::AtEnd,
				sol::base_classes, sol::bases<IComponent>()
				);
		}

		{
			// Input - real keyboard/mouse enums plus the LuaInputBridge
			// registration API (see PyrosBindings.h's LuaInputBridge
			// class comment for why a bridge object is needed instead of
			// binding InputManager directly).
			lua->new_enum("Key",
				"A", Event::Input::Keyboard::A, "B", Event::Input::Keyboard::B, "C", Event::Input::Keyboard::C,
				"D", Event::Input::Keyboard::D, "E", Event::Input::Keyboard::E, "F", Event::Input::Keyboard::F,
				"G", Event::Input::Keyboard::G, "H", Event::Input::Keyboard::H, "I", Event::Input::Keyboard::I,
				"J", Event::Input::Keyboard::J, "K", Event::Input::Keyboard::K, "L", Event::Input::Keyboard::L,
				"M", Event::Input::Keyboard::M, "N", Event::Input::Keyboard::N, "O", Event::Input::Keyboard::O,
				"P", Event::Input::Keyboard::P, "Q", Event::Input::Keyboard::Q, "R", Event::Input::Keyboard::R,
				"S", Event::Input::Keyboard::S, "T", Event::Input::Keyboard::T, "U", Event::Input::Keyboard::U,
				"V", Event::Input::Keyboard::V, "W", Event::Input::Keyboard::W, "X", Event::Input::Keyboard::X,
				"Y", Event::Input::Keyboard::Y, "Z", Event::Input::Keyboard::Z,
				"Num0", Event::Input::Keyboard::Num0, "Num1", Event::Input::Keyboard::Num1,
				"Num2", Event::Input::Keyboard::Num2, "Num3", Event::Input::Keyboard::Num3,
				"Num4", Event::Input::Keyboard::Num4, "Num5", Event::Input::Keyboard::Num5,
				"Num6", Event::Input::Keyboard::Num6, "Num7", Event::Input::Keyboard::Num7,
				"Num8", Event::Input::Keyboard::Num8, "Num9", Event::Input::Keyboard::Num9,
				"Escape", Event::Input::Keyboard::Escape,
				"LControl", Event::Input::Keyboard::LControl, "LShift", Event::Input::Keyboard::LShift,
				"LAlt", Event::Input::Keyboard::LAlt, "RControl", Event::Input::Keyboard::RControl,
				"RShift", Event::Input::Keyboard::RShift, "RAlt", Event::Input::Keyboard::RAlt,
				"Space", Event::Input::Keyboard::Space, "Return", Event::Input::Keyboard::Return,
				"Back", Event::Input::Keyboard::Back, "Tab", Event::Input::Keyboard::Tab,
				"Left", Event::Input::Keyboard::Left, "Right", Event::Input::Keyboard::Right,
				"Up", Event::Input::Keyboard::Up, "Down", Event::Input::Keyboard::Down,
				"F1", Event::Input::Keyboard::F1, "F2", Event::Input::Keyboard::F2,
				"F3", Event::Input::Keyboard::F3, "F4", Event::Input::Keyboard::F4,
				"F5", Event::Input::Keyboard::F5, "F6", Event::Input::Keyboard::F6,
				"F7", Event::Input::Keyboard::F7, "F8", Event::Input::Keyboard::F8,
				"F9", Event::Input::Keyboard::F9, "F10", Event::Input::Keyboard::F10,
				"F11", Event::Input::Keyboard::F11, "F12", Event::Input::Keyboard::F12
			);
			lua->new_enum("MouseButton",
				"Left", Event::Input::Mouse::Left,
				"Middle", Event::Input::Mouse::Middle,
				"Right", Event::Input::Mouse::Right
			);

			sol::constructors<sol::types<>> con;
			lua->new_usertype<LuaInputBridge>("Input",
				con,
				"onKeyPressed", &LuaInputBridge::OnKeyPressed,
				"onKeyReleased", &LuaInputBridge::OnKeyReleased,
				"onMouseButtonPressed", &LuaInputBridge::OnMouseButtonPressed,
				"onMouseButtonReleased", &LuaInputBridge::OnMouseButtonReleased,
				"onMouseMoved", &LuaInputBridge::OnMouseMoved,
				"onMouseWheelMoved", &LuaInputBridge::OnMouseWheelMoved
				);
		}

		{
			//File
			sol::constructors<sol::types<>> con;
			lua->new_usertype<File>("File",
				con,
				"open", &File::Open,
				"write", &File::Write,
				"read", &File::Read,
				"rewind", &File::Rewind,
				"close", &File::Close,
				"size", &File::Size,
				"getData", &File::GetData
				);
		}

		lua->set_function("getMousePosition", []() {
			Vec2 p = InputManager::GetMousePosition();
			return std::make_tuple(p.x, p.y);
		});
		lua->set_function("placeDecalAtCursor", &PlaceDecalAtCursor);
		// ******************************* CLASS *******************************
}

};

#endif
