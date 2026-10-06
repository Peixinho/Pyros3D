//============================================================================
// Name        : PyrosLuaMisc.cpp
// Description : Input bridge, File, placeDecalAtCursor.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>
#include <Pyros3D/Assets/Renderable/Terrains/TerrainEditor.h>
#include <Pyros3D/Rendering/PostEffects/VolumetricSmoke.h>
#include <Pyros3D/Physics/PhysicsEngines/IPhysics.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <map>
#include <string>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <Pyros3D/Rendering/Terrain/TerrainHorizon.h>

namespace p3d {

	void LuaClearTasks(sol::state* lua)
	{
		if (lua == NULL) return;
		sol::protected_function clear = (*lua)["tasks"]["clear"];
		if (clear.valid()) clear();
	}

	void LuaCollectWithinBudget(sol::state* lua, const f64 budgetMs)
	{
		static const bool automatic = []() { const char* v = std::getenv("PYROS_LUA_GC"); return v != NULL && std::string(v) == "auto"; }();
		if (lua == NULL) return;
		// (this is the scripts' once-a-frame housekeeping: what they have
		// spread over frames is given its share first - see `tasks`)
		{
			sol::protected_function pump = (*lua)["tasks"]["_pump"];
			if (pump.valid()) pump();
		}
		if (automatic) return;
		lua_State* L = lua->lua_state();
		// (per state: the one taken over, and how big its heap was when a
		// cycle last finished)
		static std::map<lua_State*, f64> afterCycleKb;
		static std::map<lua_State*, bool> midCycle;
		std::map<lua_State*, f64>::iterator known = afterCycleKb.find(L);
		if (known == afterCycleKb.end())
		{
			lua_gc(L, LUA_GCSTOP);
			known = afterCycleKb.insert(std::make_pair(L, (f64)lua_gc(L, LUA_GCCOUNT))).first;
			midCycle[L] = false;
		}
		const f64 nowKb = (f64)lua_gc(L, LUA_GCCOUNT);
		FrameProfiler::Instance().Counter("Lua.HeapMb", nowKb / 1024.0);
		// Between cycles nothing is collected until the heap has grown a
		// quarter over what the last one left: a collector that starts again
		// the frame after it finishes spends its whole budget every frame
		// going over a heap with almost nothing new in it.
		if (!midCycle[L] && nowKb < known->second * 1.25 + 256.0)
		{
			FrameProfiler::Instance().Counter("Lua.GcSteps", 0.0);
			return;
		}
		midCycle[L] = true;
		// Falling behind - the heap half as big again as a finished cycle
		// left it, and more - buys a longer look, up to four times the budget.
		f64 budget = budgetMs;
		const f64 grown = known->second > 1.0 ? nowKb / known->second : 1.0;
		if (grown > 1.5) budget *= (grown > 3.0 ? 4.0 : 2.0);
		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		uint32 steps = 0;
		for (;;)
		{
#if LUA_VERSION_NUM >= 505
			const int finished = lua_gc(L, LUA_GCSTEP, (size_t)0);
#else
			const int finished = lua_gc(L, LUA_GCSTEP, 0);
#endif
			steps++;
			if (finished) { known->second = (f64)lua_gc(L, LUA_GCCOUNT); midCycle[L] = false; break; }
			if (std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count() >= budget) break;
		}
		FrameProfiler::Instance().Counter("Lua.GcSteps", (f64)steps);
	}

	void RegisterLuaMisc(sol::state* lua)
	{
		// Seconds on a steady clock, as finely as the machine counts them: for a
		// script that wants to know how long something of its own took (there is
		// no os library here). Only differences mean anything.
		// logSlowFrames("perf.log", 25): every frame of 25 ms or more, with what
		// took the time, to that file (FrameProfiler::LogSlowFrames). ("", 0) stops.
		lua->set_function("logSlowFrames", [](const std::string &path, const f64 ms) {
			FrameProfiler::LogSlowFrames(path.c_str(), ms);
		});
		// setSmallObjectCull(pixels): things that would be drawn smaller than
		// that (the radius of their bounding sphere, in pixels) are not drawn.
		// 1 by default; 0 draws everything. See IRenderer::TooSmallToSee.
		lua->set_function("setSmallObjectCull", [](const f32 pixels) { IRenderer::SetSmallObjectCull(pixels); });
		lua->set_function("getSmallObjectCull", []() { return IRenderer::GetSmallObjectCull(); });
		// setShadowUpdateInterval(2): the sun's shadow map every other frame.
		lua->set_function("setShadowUpdateInterval", [](const uint32 frames) { IRenderer::SetShadowUpdateInterval(frames); });
		// setShadowCasterViewCull(false): the sun's map takes every caster in
		// its box again, as it did - to compare against.
		// setParallelCulling(false): culling on the main thread alone, to compare.
		lua->set_function("setParallelLists", [](const bool on) { IRenderer::SetParallelLists(on); });
		lua->set_function("setParallelCulling", [](const bool on) { IRenderer::SetParallelCulling(on); });
		lua->set_function("setShadowCasterViewCull", [](const bool on) { IRenderer::SetShadowCasterViewCull(on); });
		// setSSAOHalfResolution(true): the deferred renderer's ambient occlusion
		// at half resolution - see DeferredRenderer::SetSSAOHalfResolution.
		lua->set_function("setSSAOHalfResolution", [](const bool half) { DeferredRenderer::SetSSAOHalfResolution(half); });
		// viewDistance(object): metres from where its scene was last looked at
		// from - the player's eye, whatever is carrying it - to the object.
		// What "do this only for what is near" is measured with; -1 before
		// anything has been drawn. (rc:wasSeenRecently() says whether a thing
		// is being drawn at all.)
		lua->set_function("viewDistance", [](GameObject* go) -> f64 {
			if (go == NULL) return -1.0;
			SceneGraph* scene = go->GetOwningScene();
			if (scene == NULL || !scene->HasBeenViewed()) return -1.0;
			return (f64)scene->GetLastViewPosition().distance(go->GetWorldPosition());
		});
		lua->set_function("getClock", []() {
			return std::chrono::duration<f64>(std::chrono::steady_clock::now().time_since_epoch()).count();
		});
		// tasks: work spread over frames. tasks.run(function() ... tasks.pause() ... end, ms)
		// starts fn as a coroutine given `ms` of each frame (0.3 if not said);
		// tasks.pause(), called from inside it as often as is convenient, hands
		// the frame back once that much has been used and carries on from there
		// the next. tasks.run returns a handle: handle.done, tasks.cancel(handle),
		// handle.onDone = function. tasks.clear() drops them all (the engine
		// does when a scene goes). Run by the host once a frame.
		lua->script(R"LUA(
tasks = {}
local list, current = {}, nil
function tasks.run(fn, ms)
	local t = { co = coroutine.create(fn), budget = (ms or 0.3) / 1000, done = false }
	list[#list + 1] = t
	return t
end
function tasks.pause()
	local t = current
	if t and getClock() - t.t0 > t.budget then coroutine.yield() end
end
function tasks.cancel(t) if t then t.done = true end end
function tasks.clear() for i = #list, 1, -1 do list[i].done = true list[i] = nil end end
function tasks.count() return #list end
function tasks._pump()
	local i = 1
	while i <= #list do
		local t = list[i]
		if not t.done then
			t.t0 = getClock()
			current = t
			local ok, err = coroutine.resume(t.co)
			current = nil
			if not ok then
				echo("ERROR: task - " .. tostring(err))
				t.done = true
			elseif coroutine.status(t.co) == "dead" then
				t.done = true
				if t.onDone then pcall(t.onDone) end
			end
		end
		if t.done then table.remove(list, i) else i = i + 1 end
	end
end
)LUA");
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

		// Returns hit, x, y, z, nx, ny, nz, name, distance - multiple returns
		// for the same reason worldToScreen does it, so the common
		// "local hit, x, y, z = screenPick(...)" reads naturally.
		lua->set_function("screenPick", [](float winW, float winH, float mx, float my,
			GameObject* camera, Projection* projection, SceneGraph* scene) {
			Vec3 p, n;
			std::string name;
			f32 dist = 0.f;
			const bool ok = ScreenPick(winW, winH, mx, my, camera, projection, scene,
				&p, &n, &name, &dist);
			return std::make_tuple(ok, p.x, p.y, p.z, n.x, n.y, n.z, name, dist);
		});
		lua->set_function("clearDecals", &ClearLuaDecals);

		// Returns x, y, visible - multiple returns rather than a table so the
		// common "local x, y = worldToScreen(...)" reads naturally.
		lua->set_function("worldToScreen", [](float winW, float winH, GameObject* camera,
			Projection* projection, const Vec3 &worldPos) {
			float x = 0.f, y = 0.f;
			const bool ok = WorldToScreen(winW, winH, camera, projection, worldPos, &x, &y);
			return std::make_tuple(x, y, ok);
		});
		lua->set_function("screenToWorldAtDepth", &ScreenToWorldAtDepth);
		lua->set_function("setIKConstraintEnabled", &SetIKConstraintEnabled);
		lua->set_function("setIKConstraintWeight", &SetIKConstraintWeight);

		// terrain - ground height queries and runtime sculpting/painting
		// (see TerrainEditor.h). One editor per process: a stroke's state
		// (which tiles need collision and foliage rebuilt) lives in it
		// between calls, until terrain.finishStroke().
		{
			static TerrainEditor editor;
			sol::table terrain = lua->create_named_table("terrain");
			// terrain.bakeShadows(scene [, resolution, reach]) works the terrain's
			// own shadow out now (TerrainHorizon) and has the scene use it - what
			// a scene with "terrainShadows" baked gets while it loads; for a
			// script that has changed the ground. terrain.clearShadows(scene)
			// goes back to the terrain casting into the shadow maps.
			terrain.set_function("bakeShadows", sol::overload(
				[](SceneGraph* scene) { if (scene) scene->SetTerrainHorizon(TerrainHorizon::Bake(scene)); return scene && scene->GetTerrainHorizon() != NULL; },
				[](SceneGraph* scene, const uint32 resolution, const f32 reach) { if (scene) scene->SetTerrainHorizon(TerrainHorizon::Bake(scene, resolution, reach)); return scene && scene->GetTerrainHorizon() != NULL; }));
			// terrain.shadeAt(scene, x, z, toSun): 1 in the sun, 0 in a hill's shade
			// (1 where nothing is baked).
			terrain.set_function("shadeAt", [](SceneGraph* scene, const f32 x, const f32 z, const Vec3 &toSun) -> f32 {
				return (scene && scene->GetTerrainHorizon()) ? scene->GetTerrainHorizon()->ShadeAt(x, z, toSun) : 1.f;
			});
			terrain.set_function("clearShadows", [](SceneGraph* scene) { if (scene) scene->SetTerrainHorizon(std::shared_ptr<TerrainHorizon>()); });
			terrain.set_function("heightAt", [](SceneGraph* scene, const f32 x, const f32 z) -> sol::optional<f32> {
				f32 h = 0.f;
				if (TerrainEditor::HeightAt(scene, x, z, h)) return h;
				return sol::nullopt;
			});
			terrain.set_function("splatAt", [](SceneGraph* scene, const f32 x, const f32 z, sol::this_state ts) -> sol::object {
				f32 w[4];
				sol::state_view lv(ts);
				if (!editor.SplatAt(scene, x, z, w)) return sol::make_object(lv, sol::nil);
				sol::table t = lv.create_table();
				for (int i = 0; i < 4; i++) t[i + 1] = w[i];
				return t;
			});
			terrain.set_function("sculpt", [](SceneGraph* scene, const f32 x, const f32 z, const f32 radius, const f32 amount,
				const f32 hardness, const std::string &mode, sol::optional<f32> target) -> uint32 {
				TerrainEditor::SculptMode m = TerrainEditor::Raise;
				if (mode == "lower") m = TerrainEditor::Lower;
				else if (mode == "smooth") m = TerrainEditor::Smooth;
				else if (mode == "flatten") m = TerrainEditor::Flatten;
				return editor.Sculpt(scene, x, z, radius, amount, hardness, m, target.value_or(0.f));
			});
			terrain.set_function("paintSplat", [](SceneGraph* scene, const f32 x, const f32 z, const f32 radius,
				const uint32 layer, const f32 strength, const f32 hardness) -> uint32 {
				return editor.PaintSplat(scene, x, z, radius, layer, strength, hardness);
			});
			terrain.set_function("paintFoliage", [](SceneGraph* scene, const f32 x, const f32 z, const f32 radius,
				const uint32 layer, const f32 target, const f32 strength, const f32 hardness) -> uint32 {
				return editor.PaintFoliage(scene, x, z, radius, layer, target, strength, hardness);
			});
			terrain.set_function("finishStroke", []() { editor.FinishStroke(); });
			terrain.set_function("setAssetRoot", [](const std::string &root) { editor.SetAssetRoot(root); });
			terrain.set_function("save", []() -> bool { return editor.Save(); });
		}
		{
			// VolumetricSmoke - smoke clouds that fill the space around
			// them and stop at colliders. Drawn by the "VolumetricSmoke"
			// post effect; a scene without it in its chain shows nothing.
			sol::table smoke = lua->create_named_table("VolumetricSmoke");
			// spawn(physics, position, color, radius, growTime, lifeTime,
			// fadeTime) -> slot, or -1 when every slot is taken. Pass nil
			// for physics to ignore the world.
			smoke.set_function("spawn", [](IPhysics* physics, const Vec3 &position, const Vec3 &color,
				const f32 radius, const f32 growTime, const f32 lifeTime, const f32 fadeTime) -> int32 {
				return VolumetricSmoke::Spawn(physics, position, color, radius, growTime, lifeTime, fadeTime);
			});
			// The same, with the two things that make it something other
			// than a smoke grenade. rise: what a step up costs the fill
			// against a step sideways (smoke is 0.85; 2 to 3 keeps it low
			// along the floor). emission: above 0 it is fire, giving off
			// `color` instead of being lit.
			// blast: for fire, 0 is flames and 1 the solid ball of an explosion.
			smoke.set_function("spawnEx", [](IPhysics* physics, const Vec3 &position, const Vec3 &color,
				const f32 radius, const f32 growTime, const f32 lifeTime, const f32 fadeTime,
				const f32 rise, const f32 emission, sol::optional<f32> blast) -> int32 {
				return VolumetricSmoke::Spawn(physics, position, color, radius, growTime, lifeTime, fadeTime, rise, emission, blast.value_or(0.f));
			});
			smoke.set_function("update", [](const f32 dt) { VolumetricSmoke::Update(dt); });
			smoke.set_function("remove", [](const int32 id) { VolumetricSmoke::Remove(id); });
			// 1 is a grenade's wall of smoke; a signal smoke is well under it.
			smoke.set_function("setThickness", [](const int32 id, const f32 thickness) { VolumetricSmoke::SetThickness(id, thickness); });
			smoke.set_function("clear", []() { VolumetricSmoke::Clear(); });
			smoke.set_function("count", []() -> uint32 { return VolumetricSmoke::GetActiveCount(); });
			smoke.set_function("capacity", []() -> uint32 { return (uint32)VolumetricSmoke::MaxClouds; });
			// How many voxels the fill reached - small means it was boxed in.
			smoke.set_function("cells", [](const int32 id) -> uint32 {
				return (id >= 0 && id < VolumetricSmoke::MaxClouds) ? VolumetricSmoke::GetCloud((uint32)id).cells : 0;
			});
			smoke.set_function("setCellSize", [](const f32 size) { VolumetricSmoke::SetCellSize(size); });
			// The smoke is lit by the scene's own lights and ambient. These
			// two override that, and useSceneLighting() undoes them.
			smoke.set_function("setLight", [](const Vec3 &towardLight, const Vec3 &color) { VolumetricSmoke::SetLight(towardLight, color); });
			smoke.set_function("setAmbient", [](const Vec3 &color) { VolumetricSmoke::SetAmbient(color); });
			smoke.set_function("useSceneLighting", []() { VolumetricSmoke::UseSceneLighting(); });
			// Washes the whole frame toward a colour: 0 none, 1 nothing else.
			smoke.set_function("setScreenFlash", [](const Vec3 &color, const f32 amount) { VolumetricSmoke::SetScreenFlash(color, amount); });
			smoke.set_function("setWind", [](const Vec3 &wind) { VolumetricSmoke::SetWind(wind); });
		}
	}

	void GenerateStoreBindings(sol::state* lua, const std::string &directory)
	{
		const std::filesystem::path dir(directory);
		const auto safe = [](const std::string &name) {
			if (name.empty() || name.size() > 64 || name[0] == '.') return false;
			for (size_t i = 0; i < name.size(); i++)
			{
				const char c = name[i];
				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
			}
			return true;
		};
		sol::table store = lua->create_named_table("store");
		store.set_function("read", [dir, safe](const std::string &name, sol::this_state ts) -> sol::object {
			sol::state_view L(ts);
			if (!safe(name)) return sol::make_object(L, sol::lua_nil);
			std::ifstream in((dir / name).string().c_str(), std::ios::binary);
			if (!in.is_open()) return sol::make_object(L, sol::lua_nil);
			std::stringstream ss;
			ss << in.rdbuf();
			return sol::make_object(L, ss.str());
		});
		store.set_function("write", [dir, safe](const std::string &name, const std::string &text) {
			if (!safe(name)) return false;
			std::error_code ec;
			std::filesystem::create_directories(dir, ec);
			std::ofstream out((dir / name).string().c_str(), std::ios::binary | std::ios::trunc);
			if (!out.is_open()) return false;
			out << text;
			return true;
		});
	}

} // namespace p3d

#endif
