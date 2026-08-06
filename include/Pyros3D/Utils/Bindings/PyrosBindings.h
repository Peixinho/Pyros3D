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

#include <Pyros3D/Ext/sol/sol.hpp>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/VelocityRenderer/VelocityRenderer.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/CubemapRenderer/CubemapRenderer.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
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
#include <Pyros3D/Physics/PhysicsEngines/Box3D/Box3DPhysics.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BloomEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurXEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurYEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/ResizeEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/RTTDebug.h>
#include <Pyros3D/Rendering/PostEffects/Effects/SSAOEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/VignetteEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/MotionBlurEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/TonemapEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurSSAOEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/SSAOCompositeEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/DepthOfFieldEffect.h>
#include <Pyros3D/Rendering/Components/Particles/ParticleSystem.h>
#include <Pyros3D/Audio/AudioManager.h>
#include <Pyros3D/Audio/AudioBus.h>
#include <Pyros3D/Audio/Sound.h>
#include <Pyros3D/Audio/AudioSource.h>
#include <Pyros3D/Core/InputManager/InputManager.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <map>
#include <vector>

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
		// On Init Virtual Function - kept overridden even though nothing
		// in the engine actually calls IComponent::Init()/GameObject::Init()
		// automatically (confirmed by grep - zero call sites anywhere).
		// on_init's *real* firing happens lazily from Update() below,
		// guarded by `initialized` so it's still exactly-once even if a
		// script manually calls :init() before the first real Update().
		virtual void Init()
		{
			GameObject::Init();
			FireInit();
		}
		// Virtual Function To Update GameObject
		virtual void Update(const p3d::f64 time)
		{
			FireInit();
			GameObject::Update(time);
			if (on_update) { on_update(*this, time); }
		}
		// Destroy Function - like Init(), nothing in the engine actually
		// calls IComponent::Destroy() automatically either (also
		// confirmed by grep), so on_destroy only fires if a script calls
		// :destroy() itself. Kept available rather than removed - honest
		// about when it fires, not claiming automatic invocation.
		virtual void Destroy()
		{
			GameObject::Destroy();
			if (on_destroy) { on_destroy(*this); }
			//(*this->lua)[name] = sol::nil;
		}
		std::function<void(LUA_GameObject&, p3d::f64)> on_update;
		std::function<void(LUA_GameObject&)> on_init;
		std::function<void(LUA_GameObject&)> on_destroy;

	private:
		void FireInit() { if (!initialized) { initialized = true; if (on_init) on_init(*this); } }
		bool initialized = false;
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

		// See LUA_GameObject's identical comment on why on_init fires
		// lazily from Update(), not Init().
		virtual void Init()
		{
			DirectionalLight::Init();
			FireInit();
		}
		// Virtual Function To Update GameObject
		virtual void Update(const p3d::f64 time)
		{
			FireInit();
			DirectionalLight::Update(time);
			if (on_update) { on_update(*this, time); }
		}
		// Destroy Function
		virtual void Destroy()
		{
			DirectionalLight::Destroy();
			if (on_destroy) { on_destroy(*this); }
			//(*this->lua)[name] = sol::nil;
		}

		std::function<void(LUA_DirectionalLight&, p3d::f64)> on_update;
		std::function<void(LUA_DirectionalLight&)> on_init;
		std::function<void(LUA_DirectionalLight&)> on_destroy;
	private:
		void FireInit() { if (!initialized) { initialized = true; if (on_init) on_init(*this); } }
		bool initialized = false;
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

		// See LUA_GameObject's identical comment on why on_init fires
		// lazily from Update(), not Init().
		virtual void Init()
		{
			PointLight::Init();
			FireInit();
		}
		// Virtual Function To Update GameObject
		virtual void Update(const p3d::f64 time)
		{
			FireInit();
			PointLight::Update(time);
			if (on_update) { on_update(*this, time); }
		}
		// Destroy Function
		virtual void Destroy()
		{
			PointLight::Destroy();
			if (on_destroy) { on_destroy(*this); }
			//(*this->lua)[name] = sol::nil;
		}

		std::function<void(LUA_PointLight&, p3d::f64)> on_update;
		std::function<void(LUA_PointLight&)> on_init;
		std::function<void(LUA_PointLight&)> on_destroy;
	private:
		void FireInit() { if (!initialized) { initialized = true; if (on_init) on_init(*this); } }
		bool initialized = false;
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


		// See LUA_GameObject's identical comment on why on_init fires
		// lazily from Update(), not Init().
		virtual void Init()
		{
			SpotLight::Init();
			FireInit();
		}
		// Virtual Function To Update GameObject
		virtual void Update(const p3d::f64 time)
		{
			FireInit();
			SpotLight::Update(time);
			if (on_update) { on_update(*this, time); }
		}
		// Destroy Function
		virtual void Destroy()
		{
			SpotLight::Destroy();
			if (on_destroy) { on_destroy(*this); }
			//(*this->lua)[name] = sol::nil;
		}

		std::function<void(LUA_SpotLight&, p3d::f64)> on_update;
		std::function<void(LUA_SpotLight&)> on_init;
		std::function<void(LUA_SpotLight&)> on_destroy;
	private:
		void FireInit() { if (!initialized) { initialized = true; if (on_init) on_init(*this); } }
		bool initialized = false;
		std::string name;

	};
	class LUA_RenderingComponent : public p3d::RenderingComponent
	{
	public:

		LUA_RenderingComponent(const std::shared_ptr<p3d::Renderable>& renderable, const std::shared_ptr<p3d::IMaterial>& Material, const p3d::f32 Distance = 0.f) : p3d::RenderingComponent(renderable, Material, Distance) {}
		LUA_RenderingComponent(const std::shared_ptr<p3d::Renderable>& renderable, const uint32 MaterialOptions, const p3d::f32 Distance = 0.f) : p3d::RenderingComponent(renderable, MaterialOptions, Distance) {}

		// Lua Instantiation
		LUA_RenderingComponent(sol::state* lua, const std::string name, const std::shared_ptr<p3d::Renderable>& renderable, const std::shared_ptr<p3d::IMaterial>& Material, const f32 Distance = 0.f) : p3d::RenderingComponent(renderable, Material, Distance)
		{
			// Register object in Lua
			(*lua)[name.c_str()] = this;
		}
		LUA_RenderingComponent(sol::state* lua, const std::string name, const std::shared_ptr<p3d::Renderable>& renderable, const p3d::f32 MaterialOptions, const f32 Distance = 0.f) : p3d::RenderingComponent(renderable, MaterialOptions, Distance)
		{
			// Register object in Lua
			(*lua)[name.c_str()] = this;
		}

		// See LUA_GameObject's identical comment on why on_init fires
		// lazily from Update(), not Init().
		virtual void Init()
		{
			p3d::RenderingComponent::Init();
			FireInit();
		}
		// Virtual Function To Update GameObject
		virtual void Update(const p3d::f64 time)
		{
			FireInit();
			p3d::RenderingComponent::Update(time);
			if (on_update) { on_update(*this, time); }
		}
		// Destroy Function
		virtual void Destroy()
		{
			p3d::RenderingComponent::Destroy();
			if (on_destroy) { on_destroy(*this); }
		}

		std::function<void(LUA_RenderingComponent&, p3d::f64)> on_update;
		std::function<void(LUA_RenderingComponent&)> on_init;
		std::function<void(LUA_RenderingComponent&)> on_destroy;
	private:
		void FireInit() { if (!initialized) { initialized = true; if (on_init) on_init(*this); } }
		bool initialized = false;
		std::string name;

	};

    class LUA_RenderingInstancedComponent : public p3d::RenderingInstancedComponent
    {
    public:

        LUA_RenderingInstancedComponent(const std::shared_ptr<p3d::Renderable>& renderable, const std::shared_ptr<p3d::IMaterial>& Material, const uint32 nrInstances, const p3d::f32 &boundingSphere) : p3d::RenderingInstancedComponent(renderable, Material, nrInstances, boundingSphere) {}
        LUA_RenderingInstancedComponent(const std::shared_ptr<p3d::Renderable>& renderable, const uint32 MaterialProperties, const p3d::uint32 nrInstances, const p3d::f32 boundingSphere) : p3d::RenderingInstancedComponent(renderable, MaterialProperties, nrInstances, boundingSphere) {}

        // Lua Instantiation
        LUA_RenderingInstancedComponent(sol::state* lua, const std::string name, const std::shared_ptr<p3d::Renderable>& renderable, const std::shared_ptr<p3d::IMaterial>& Material, const p3d::uint32 nrInstances, const p3d::f32 boundingSphere) : p3d::RenderingInstancedComponent(renderable, Material, nrInstances, boundingSphere)
        {
            // Register object in Lua
            (*lua)[name.c_str()] = this;
        }
        LUA_RenderingInstancedComponent(sol::state* lua, const std::string name, const std::shared_ptr<p3d::Renderable>& renderable, const p3d::f32 MaterialOptions, const p3d::uint32 nrInstances, const p3d::f32 boundingSphere) : p3d::RenderingInstancedComponent(renderable, MaterialOptions, nrInstances, boundingSphere)
        {
            // Register object in Lua
            (*lua)[name.c_str()] = this;
        }

        // See LUA_GameObject's identical comment on why on_init fires
        // lazily from Update(), not Init().
        virtual void Init()
        {
            p3d::RenderingInstancedComponent::Init();
            FireInit();
        }
        // Virtual Function To Update GameObject
        virtual void Update(const p3d::f64 time)
        {
            FireInit();
            p3d::RenderingInstancedComponent::Update(time);
            if (on_update) { on_update(*this, time); }
        }
        // Destroy Function
        virtual void Destroy()
        {
            p3d::RenderingInstancedComponent::Destroy();
            if (on_destroy) { on_destroy(*this); }
        }
        virtual void AddBuffer(p3d::AttributeBuffer* buffer)
        {
            RenderingInstancedComponent::AddBuffer(buffer);
        }
        virtual void RemoveBuffer(AttributeBuffer* buffer)
        {
            RenderingInstancedComponent::RemoveBuffer(buffer);
        }
        virtual const uint32 NumberOfInstances() const
        {
            return RenderingInstancedComponent::nrInstances;
        }
        virtual void SetNumberInstances(const uint32 instances) {
            RenderingInstancedComponent::nrInstances = instances;
        }

        std::function<void(LUA_RenderingInstancedComponent&, p3d::f64)> on_update;
        std::function<void(LUA_RenderingInstancedComponent&)> on_init;
        std::function<void(LUA_RenderingInstancedComponent&)> on_destroy;
    private:
        void FireInit() { if (!initialized) { initialized = true; if (on_init) on_init(*this); } }
        bool initialized = false;
        std::string name;

    };

    // Real, generic "attach arbitrary Lua behavior to any GameObject"
    // component - see VULKAN_ROADMAP.md's Lua-scripting-overhaul section
    // for why this is a genuinely new pattern, not a duplicate of the
    // LUA_* wrappers above: those only cover their own specific engine
    // type (GameObject/RenderingComponent/lights), each hand-duplicated.
    // LuaComponent needs no rendering/physics payload of any kind -
    // IComponent itself has none - so it works for pure gameplay logic
    // attached via the already-bound GameObject::AddComponent(), the
    // same way any other component is attached.
    class PYROS3D_API LuaComponent : public p3d::IComponent
    {
    public:

        LuaComponent() {}
        LuaComponent(sol::state* lua, const std::string name)
        {
            (*lua)[name.c_str()] = this;
        }

        virtual void Register(SceneGraph* Scene) {}
        virtual void Unregister(SceneGraph* Scene) {}
        virtual p3d::uint32 GetComponentType() const { return p3d::ComponentType::LuaComponent; }
        // See LUA_GameObject's identical comment - Init() is kept for
        // scripts that want to call it explicitly, but on_init's real
        // firing is lazy, from the first real Update().
        virtual void Init()
        {
            FireInit();
        }
        virtual void Update(const p3d::f64 time)
        {
            FireInit();
            if (on_update) { on_update(*this, time); }
        }
        virtual void Destroy()
        {
            if (on_destroy) { on_destroy(*this); }
        }

        std::function<void(LuaComponent&, p3d::f64)> on_update;
        std::function<void(LuaComponent&)> on_init;
        std::function<void(LuaComponent&)> on_destroy;

        // Real, serializable behavior. Empty scriptFile means "an
        // anonymous ad-hoc component" (the pre-existing on_init/
        // on_update/on_destroy closures above, assigned directly from a
        // script) - SceneSerializer can only round-trip THAT as an
        // existence marker, same as before. A non-empty scriptFile means
        // this component was built from a .lua file (via
        // LuaComponent_fromFile()/GameObject:attachScript() below) that
        // `return`s a middleclass class (same class() convention
        // main.lua already uses via middleclass.lua) - the file itself
        // IS the identity, no separate name/registry needed. The class's
        // real Lua instance table (`data`) can be handed to
        // data:serialize()/ClassName.deserialize() for a working save/
        // load contract, but it's opt-in per script, not automatic for
        // every LuaComponent.
        std::string scriptFile;
        sol::table data;
    private:
        void FireInit() { if (!initialized) { initialized = true; if (on_init) on_init(*this); } }
        bool initialized = false;
    };

    // Wires a LuaComponent's on_init/on_update/on_destroy to its Lua
    // instance's own init(self, owner)/update(self, time)/destroy(self)
    // methods, if defined - without this, attachScript()'d behavior
    // (and behavior reconstructed by SceneSerializer::LoadScene) never
    // actually runs, since LuaComponent_FromFile/the load path only
    // ever populate `data` (the instance), not these hooks. `owner` is
    // passed into init() (in addition to self) so a script can reach
    // its own GameObject/sibling components (e.g.
    // owner:getComponent("RenderingComponent")) without a separate
    // binding - update()/destroy() only ever need `self`. `time` is the
    // same absolute simulation time every other Update(time) call in
    // this engine already passes (SceneGraph::Update(GetTime()), etc),
    // not a per-frame delta.
    inline void WireLuaComponentLifecycle(LuaComponent* comp)
    {
        sol::table instance = comp->data;
        if (!instance.valid()) return;
        if (sol::function f = instance["init"]; f.valid())
            comp->on_init = [](LuaComponent& c) {
                sol::protected_function pf = sol::table(c.data)["init"];
                sol::protected_function_result r = pf(c.data, c.GetOwner());
                if (!r.valid()) {
                    sol::error err = r;
                    const std::string msg = std::string("ERROR: LuaComponent init - ") + err.what();
                    echo(msg);
                    // Re-throw so DemoLauncher SwitchDemo can record the
                    // failure (meshes=0) instead of silently continuing.
                    throw std::runtime_error(msg);
                }
            };
        if (sol::function f = instance["update"]; f.valid())
            comp->on_update = [](LuaComponent& c, p3d::f64 time) {
                sol::protected_function pf = sol::table(c.data)["update"];
                sol::protected_function_result r = pf(c.data, time);
                if (!r.valid()) {
                    sol::error err = r;
                    echo(std::string("ERROR: LuaComponent update - ") + err.what());
                }
            };
        if (sol::function f = instance["destroy"]; f.valid())
            comp->on_destroy = [](LuaComponent& c) {
                sol::protected_function pf = sol::table(c.data)["destroy"];
                sol::protected_function_result r = pf(c.data);
                if (!r.valid()) {
                    sol::error err = r;
                    echo(std::string("ERROR: LuaComponent destroy - ") + err.what());
                }
            };
    }

    // Loads scriptFile (expected to `return` a middleclass class table,
    // e.g. `local Foo = class('Foo'); ... return Foo`), instantiates it
    // via Foo:new(), and wraps the result in a LuaComponent. Uses sol2's
    // require_file()'s own module caching (keyed by scriptFile itself),
    // so attaching the same script to many GameObjects only ever runs the
    // file once. Returns nullptr (nil in Lua) if the file doesn't return a
    // usable class table. Lifecycle hooks (init/update/destroy) are
    // wired automatically - see WireLuaComponentLifecycle. Returns
    // shared_ptr so the result can be handed straight to
    // GameObject::AddComponent (Stage 1 ownership).
    inline std::shared_ptr<LuaComponent> LuaComponent_FromFile(sol::state& lua, const std::string &scriptFile)
    {
        sol::object result = lua.require_file(scriptFile, scriptFile);
        if (!result.valid() || result.get_type() != sol::type::table) return nullptr;
        sol::table cls = result;
        sol::function newFn = cls["new"];
        if (!newFn.valid()) return nullptr;
        sol::table instance = newFn(cls);
        auto comp = std::make_shared<LuaComponent>();
        comp->scriptFile = scriptFile;
        comp->data = instance;
        WireLuaComponentLifecycle(comp.get());
        return comp;
    }

	// Real keyboard/mouse input for Lua - InputManager itself is
	// 100% C++-only (AddEvent<X,Y> is a compile-time
	// member-function-pointer template, can't bind to sol2 directly,
	// and its Gallant::Signal backend has no std::function overload
	// either - see IPhysicsComponent.h's OnCollisionEnter comment for
	// the same reasoning applied there). This bridge is the one real
	// trampoline layer: it registers itself against every keyboard code
	// and the 3 mouse buttons once at construction (a fixed, one-time
	// loop - not a new per-key concept), and fans each real
	// InputManager callback out to however many Lua closures a script
	// registered for that specific code. Key/mouse-button press-release
	// callbacks take no parameters (confirmed via InputManager.cpp:
	// Info.Value == Info.Input for these events, i.e. no extra data
	// beyond "which code" - already known from registration); mouse
	// move/wheel callbacks pass real data (Vec2 position / f32 delta).
	class PYROS3D_API LuaInputBridge
	{
	public:

		LuaInputBridge()
		{
			for (uint32 i = 0; i < Event::Input::Keyboard::Count; i++)
			{
				InputManager::AddEvent(Event::Type::OnPress, i, this, &LuaInputBridge::OnKeyPress);
				InputManager::AddEvent(Event::Type::OnRelease, i, this, &LuaInputBridge::OnKeyRelease);
			}
			const uint32 mouseButtons[3] = { Event::Input::Mouse::Left, Event::Input::Mouse::Middle, Event::Input::Mouse::Right };
			for (uint32 i = 0; i < 3; i++)
			{
				InputManager::AddEvent(Event::Type::OnPress, mouseButtons[i], this, &LuaInputBridge::OnMouseButtonPress);
				InputManager::AddEvent(Event::Type::OnRelease, mouseButtons[i], this, &LuaInputBridge::OnMouseButtonRelease);
			}
			InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &LuaInputBridge::OnMouseMove);
			InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Wheel, this, &LuaInputBridge::OnMouseWheel);
		}

		~LuaInputBridge()
		{
			for (uint32 i = 0; i < Event::Input::Keyboard::Count; i++)
			{
				InputManager::RemoveEvent(Event::Type::OnPress, i, this, &LuaInputBridge::OnKeyPress);
				InputManager::RemoveEvent(Event::Type::OnRelease, i, this, &LuaInputBridge::OnKeyRelease);
			}
			const uint32 mouseButtons[3] = { Event::Input::Mouse::Left, Event::Input::Mouse::Middle, Event::Input::Mouse::Right };
			for (uint32 i = 0; i < 3; i++)
			{
				InputManager::RemoveEvent(Event::Type::OnPress, mouseButtons[i], this, &LuaInputBridge::OnMouseButtonPress);
				InputManager::RemoveEvent(Event::Type::OnRelease, mouseButtons[i], this, &LuaInputBridge::OnMouseButtonRelease);
			}
			InputManager::RemoveEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &LuaInputBridge::OnMouseMove);
			InputManager::RemoveEvent(Event::Type::OnMove, Event::Input::Mouse::Wheel, this, &LuaInputBridge::OnMouseWheel);
			keyPressCallbacks.clear();
			keyReleaseCallbacks.clear();
			mousePressCallbacks.clear();
			mouseReleaseCallbacks.clear();
			mouseMoveCallbacks.clear();
			mouseWheelCallbacks.clear();
		}

		// Lua-facing registration API
		void OnKeyPressed(uint32 key, sol::function callback) { keyPressCallbacks[key].push_back(callback); }
		void OnKeyReleased(uint32 key, sol::function callback) { keyReleaseCallbacks[key].push_back(callback); }
		void OnMouseButtonPressed(uint32 button, sol::function callback) { mousePressCallbacks[button].push_back(callback); }
		void OnMouseButtonReleased(uint32 button, sol::function callback) { mouseReleaseCallbacks[button].push_back(callback); }
		void OnMouseMoved(sol::function callback) { mouseMoveCallbacks.push_back(callback); }
		void OnMouseWheelMoved(sol::function callback) { mouseWheelCallbacks.push_back(callback); }

	private:

		// Real trampolines - one registration per code, dispatch by
		// Info.Input to whichever Lua closures were registered for it.
		void OnKeyPress(Event::Input::Info info) { Fire(keyPressCallbacks, info.Input); }
		void OnKeyRelease(Event::Input::Info info) { Fire(keyReleaseCallbacks, info.Input); }
		void OnMouseButtonPress(Event::Input::Info info) { Fire(mousePressCallbacks, info.Input); }
		void OnMouseButtonRelease(Event::Input::Info info) { Fire(mouseReleaseCallbacks, info.Input); }
		void OnMouseMove(Event::Input::Info info)
		{
			Vec2 pos = info.Value;
			for (auto &cb : mouseMoveCallbacks) { if (cb.valid()) cb(pos.x, pos.y); }
		}
		void OnMouseWheel(Event::Input::Info info)
		{
			f32 delta = info.Value;
			for (auto &cb : mouseWheelCallbacks) { if (cb.valid()) cb(delta); }
		}

		void Fire(std::map<uint32, std::vector<sol::function> > &callbacks, uint32 code)
		{
			std::map<uint32, std::vector<sol::function> >::iterator it = callbacks.find(code);
			if (it == callbacks.end()) return;
			for (auto &cb : it->second) { if (cb.valid()) cb(); }
		}

		std::map<uint32, std::vector<sol::function> > keyPressCallbacks;
		std::map<uint32, std::vector<sol::function> > keyReleaseCallbacks;
		std::map<uint32, std::vector<sol::function> > mousePressCallbacks;
		std::map<uint32, std::vector<sol::function> > mouseReleaseCallbacks;
		std::vector<sol::function> mouseMoveCallbacks;
		std::vector<sol::function> mouseWheelCallbacks;
	};

	void GenerateBindings(sol::state* lua);

};
#endif /* PYROSBINDINGS_H */
#endif
