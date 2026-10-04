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

namespace p3d {

	void RegisterLuaMisc(sol::state* lua)
	{
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

} // namespace p3d

#endif
