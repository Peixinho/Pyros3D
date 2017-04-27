//============================================================================
// Name        : PyrosBindings.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Lua Bindings
//============================================================================

#ifdef LUA_BINDINGS
#ifndef PYROSBINDINGS_H
#define PYROSBINDINGS_H
#define SOL_CHECK_ARGUMENTS

#include <Pyros3D/Ext/Sol/sol.hpp>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Assets/Sounds/ISound.h>
#include <Pyros3D/Assets/Sounds/Music.h>
#include <Pyros3D/Assets/Sounds/Sound.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Capsule.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cone.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cylinder.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Torus.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/TorusKnot.h>
#include <Pyros3D/Assets/Renderable/Decals/Decals.h>
#include <Pyros3D/Assets/Renderable/Text/Text.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/AnimationManager/TextureAnimation.h>
#include <Pyros3D/Physics/Components/IPhysicsComponent.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BloomEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurXEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurYEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/ResizeEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/RTTDebug.h>
#include <Pyros3D/Rendering/PostEffects/Effects/SSAOEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/VignetteEffect.h>

namespace p3d {

	class PYROS3D_API LUA_GameObject : public p3d::GameObject
	{
	public:

		// Constructor
		LUA_GameObject(bool isStatic = false) : GameObject(isStatic) {}
		LUA_GameObject(sol::state* lua, const std::string name, bool isStatic = false) : GameObject(isStatic)
		{
			// Register object in Lua
			(*lua)[name.c_str()] = this;
		}
		// On Init Virtual Function
		virtual void Init()
		{
			GameObject::Init();
			if (on_init) { on_init(*this); }
		}
		// Virtual Function To Update GameObject
		virtual void Update(const p3d::f64 time)
		{
			GameObject::Update(time);
			if (on_update) { on_update(*this, time); }
		}
		// Destroy Function
		virtual void Destroy()
		{
			GameObject::Destroy();
			//(*this->lua)[name] = sol::nil;
		}
		std::function<void(LUA_GameObject&, p3d::f64)> on_update;
		std::function<void(LUA_GameObject&)> on_init;

	private:
		sol::state *lua;
		std::string name;
	};

	class PYROS3D_API LUA_DirectionalLight : public p3d::DirectionalLight
	{
	public:

		LUA_DirectionalLight() : DirectionalLight() {}
		LUA_DirectionalLight(const Vec4 &color) : DirectionalLight(color) {}
		LUA_DirectionalLight(const Vec4 &color, const Vec3 &direction) : DirectionalLight(color, direction) {}

		// Lua Instantiation from c++
		LUA_DirectionalLight(sol::state* lua, const std::string name, const Vec4 &color) : DirectionalLight(color)
		{
			// Register object in Lua
			(*lua)[name.c_str()] = this;
		}
		LUA_DirectionalLight(sol::state* lua, const std::string name, const Vec4 &color, const Vec3 &direction) : DirectionalLight(color, direction)
		{
			// Register object in Lua
			(*lua)[name.c_str()] = this;
		}

		virtual void Init()
		{
			DirectionalLight::Init();
			if (on_init) { on_init(*this); }
		}
		// Virtual Function To Update GameObject
		virtual void Update(const p3d::f64 time)
		{
			DirectionalLight::Update(time);
			if (on_update) { on_update(*this, time); }
		}
		// Destroy Function
		virtual void Destroy()
		{
			DirectionalLight::Destroy();
			//(*this->lua)[name] = sol::nil;
		}

		std::function<void(LUA_DirectionalLight&, p3d::f64)> on_update;
		std::function<void(LUA_DirectionalLight&)> on_init;
	private:
		std::string name;

	};
	class PYROS3D_API LUA_PointLight : public p3d::PointLight
	{
	public:

		LUA_PointLight() : PointLight() {}
		LUA_PointLight(const Vec4 &color, const p3d::f32 radius) : PointLight(color, radius) {}

		// Lua Instantiation
		LUA_PointLight(sol::state* lua, const std::string name, const Vec4 &color, const p3d::f32 radius) : PointLight(color, radius)
		{
			// Register object in Lua
			(*lua)[name.c_str()] = this;
		}

		virtual void Init()
		{
			PointLight::Init();
			if (on_init) { on_init(*this); }
		}
		// Virtual Function To Update GameObject
		virtual void Update(const p3d::f64 time)
		{
			PointLight::Update(time);
			if (on_update) { on_update(*this, time); }
		}
		// Destroy Function
		virtual void Destroy()
		{
			PointLight::Destroy();
			//(*this->lua)[name] = sol::nil;
		}

		std::function<void(LUA_PointLight&, p3d::f64)> on_update;
		std::function<void(LUA_PointLight&)> on_init;
	private:
		std::string name;

	};
	class PYROS3D_API LUA_SpotLight : public p3d::SpotLight
	{
	public:

		LUA_SpotLight() : SpotLight() {}
		LUA_SpotLight(const p3d::Vec4 &color, const p3d::f32 radius, const p3d::Vec3 &direction, const p3d::f32 OutterCone, const p3d::f32 InnerCone) : SpotLight(color, radius, direction, OutterCone, InnerCone) {}

		// Lua Instantiation
		LUA_SpotLight(sol::state* lua, const std::string name, const p3d::Vec4 &color, const p3d::f32 radius, const p3d::Vec3 &direction, const p3d::f32 OutterCone, const p3d::f32 InnerCone) : SpotLight(color, radius, direction, OutterCone, InnerCone)
		{
			// Register object in Lua
			(*lua)[name.c_str()] = this;
		}


		virtual void Init()
		{
			SpotLight::Init();
			if (on_init) { on_init(*this); }
		}
		// Virtual Function To Update GameObject
		virtual void Update(const p3d::f64 time)
		{
			SpotLight::Update(time);
			if (on_update) { on_update(*this, time); }
		}
		// Destroy Function
		virtual void Destroy()
		{
			SpotLight::Destroy();
			//(*this->lua)[name] = sol::nil;
		}

		std::function<void(LUA_SpotLight&, p3d::f64)> on_update;
		std::function<void(LUA_SpotLight&)> on_init;
	private:
		std::string name;

	};
	class LUA_RenderingComponent : public p3d::RenderingComponent
	{
	public:

		LUA_RenderingComponent(p3d::Renderable* renderable, p3d::IMaterial* Material = NULL) : p3d::RenderingComponent(renderable, Material) {}

		// Lua Instantiation
		LUA_RenderingComponent(sol::state* lua, const std::string name, p3d::Renderable* renderable, p3d::IMaterial* Material = NULL) : p3d::RenderingComponent(renderable, Material)
		{
			// Register object in Lua
			(*lua)[name.c_str()] = this;
		}

		virtual void Init()
		{
			p3d::RenderingComponent::Init();
			if (on_init) { on_init(*this); }
		}
		// Virtual Function To Update GameObject
		virtual void Update(const p3d::f64 time)
		{
			p3d::RenderingComponent::Update(time);
			if (on_update) { on_update(*this, time); }
		}
		// Destroy Function
		virtual void Destroy()
		{
			p3d::RenderingComponent::Destroy();
		}

		std::function<void(LUA_RenderingComponent&, p3d::f64)> on_update;
		std::function<void(LUA_RenderingComponent&)> on_init;
	private:
		std::string name;

	};

	void GenerateBindings(sol::state* lua);

};
#endif /* PYROSBINDINGS_H */
#endif