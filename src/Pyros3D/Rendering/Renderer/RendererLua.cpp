//============================================================================
// Name        : RendererLua.cpp
// Author      : Duarte Peixinho
// Description : See RendererLua.h
//============================================================================

#include <Pyros3D/Rendering/Renderer/RendererLua.h>
#if defined(LUA_BINDINGS)
#include <Pyros3D/Rendering/Renderer/SceneRenderer.h>
#include <Pyros3D/Rendering/Renderer/SceneView.h>
#include <Pyros3D/Rendering/ColourSpace.h>
#include <Pyros3D/Rendering/ShaderGlobals.h>
#include <Pyros3D/Rendering/GI/SceneGI.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>

namespace p3d {

	void BindRenderer(sol::state &lua)
	{
		if (lua["Renderer"].valid()) return;
		lua.new_usertype<SceneRenderer>("Renderer", sol::no_constructor,
			"setBackground", [](SceneRenderer &r, const Vec4 &c) { r.SetBackground(SrgbToLinear(Vec3(c.x, c.y, c.z))); },
			"unsetBackground", [](SceneRenderer &r) { r.SetBackground(Vec3(0.f, 0.f, 0.f)); },
			"setGlobalLight", [](SceneRenderer &r, const Vec4 &c) { r.SetAmbient(Vec3(c.x, c.y, c.z)); },
			"enableLOD", [](SceneRenderer &r) { r.SetLOD(true); },
			"disableLOD", [](SceneRenderer &r) { r.SetLOD(false); },
			"activateCulling", [](SceneRenderer &r, sol::optional<int>) { r.SetFrustumCulling(true); },
			"deactivateCulling", [](SceneRenderer &r) { r.SetFrustumCulling(false); },
			"setSky", [](SceneRenderer &r, bool on) { r.SetSky(on); },
			"setHaze", [](SceneRenderer &r, float density) { r.SetHaze(density); },
			"setReflections", [](SceneRenderer &r, bool on, sol::optional<float> reach) { r.SetReflections(on, reach.value_or(150.f)); },
			"setClouds", [](SceneRenderer &r, float cover, sol::optional<int> steps, sol::optional<float> shadow) { r.SetClouds(cover, (uint32)steps.value_or(40), shadow.value_or(0.45f)); },
			"setAutoExposure", [](SceneRenderer &r, bool on, sol::optional<f32> ev) { r.SetAutoExposure(on, ev.value_or(0.f)); },
			// The lens and shutter: setSunShafts(strength), setDepthOfField(focus, range, blur) - blur 0 off - setMotionBlur(shutter)
			"setSunShafts", [](SceneRenderer &r, f32 strength) { r.SetSunShafts(strength); },
			"setDepthOfField", [](SceneRenderer &r, f32 focus, f32 range, f32 blur) { r.SetDepthOfField(focus, range, blur); },
			"setMotionBlur", [](SceneRenderer &r, f32 shutter) { r.SetMotionBlur(shutter); },
			// (the sun's shadows ray-traced: setRayTracedShadows(on, sun's angular radius in degrees - 0.53 if not said))
			"setRayTracedReflections", [](SceneRenderer &r, bool on) { r.SetRayTracedReflections(on); },
			"setRayTracedShadows", [](SceneRenderer &r, bool on, sol::optional<f32> degrees) { r.SetRayTracedShadows(on, degrees.value_or(0.53f) * 0.017453293f); },
			// (fewer pixels drawn than shown, 0.5..1 - resolved up to the view by the temporal anti-aliasing)
			"setRenderScale", [](SceneRenderer &r, f32 scale) { r.SetRenderScale(scale); },
			"getRenderScale", [](SceneRenderer &r) { return r.GetRenderScale(); },
			"setShaderGlobal", [](SceneRenderer &, int index, const Vec4 &v) { if (index >= 0) ShaderGlobals::Set((uint32)index, v); },
			// The scene's light (DDGI): bakeGI(scene, { counts = {x, y, z}, rays, passes, sky = {r, g, b} }) solves it,
			// updateGI(scene, probes, hysteresis) traces a share of it again (a light that moves); on the CPU
			"bakeGI", [](SceneRenderer &r, SceneGraph* scene, sol::optional<sol::table> t) {
				SceneGISettings s;
				s.enabled = true;
				if (t)
				{
					sol::optional<sol::table> counts = (*t)["counts"];
					if (counts) for (int i = 0; i < 3; i++) s.counts[i] = (*counts)[i + 1].get_or(s.counts[i]);
					s.raysPerProbe = (*t)["rays"].get_or(s.raysPerProbe);
					s.passes = (*t)["passes"].get_or(s.passes);
					sol::optional<sol::table> sky = (*t)["sky"];
					if (sky) s.skyColor = Vec3((*sky)[1].get_or(0.f), (*sky)[2].get_or(0.f), (*sky)[3].get_or(0.f));
				}
				r.ownGIFrame = s.passes; r.ownGIRayCount = s.raysPerProbe;
				return r.SolveGlobalIllumination(scene, s);
			},
			"updateGI", [](SceneRenderer &r, SceneGraph* scene, sol::optional<int> probes, sol::optional<f32> hysteresis) {
				r.RefreshGlobalIllumination(scene, r.ownGIRayCount, (uint32)std::max(0, probes.value_or(0)), hysteresis.value_or(0.9f));
			},
			"isGIOnGPU", [](SceneRenderer &) { return false; });
	}

	void SetScriptRenderer(sol::state &lua, SceneRenderer* renderer)
	{
		BindRenderer(lua);
		lua["renderer"] = renderer;
		SceneView::SetMainRenderer(renderer);     // (what its scripts' second views are drawn beside)
	}

}
#endif
