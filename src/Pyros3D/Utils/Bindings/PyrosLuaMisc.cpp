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
#include <Pyros3D/Utils/Navigation/PathGrid.h>
#include <Pyros3D/Utils/Navigation/Crowd.h>
#include <Pyros3D/Utils/Navigation/NavMesh.h>
#include <Pyros3D/Utils/Localization/Localization.h>
#include <Pyros3D/Utils/Navigation/NavAgentComponent.h>
#include <Pyros3D/Physics/CharacterController.h>
#include <Pyros3D/Rendering/GI/ReflectionProbes.h>
#include <Pyros3D/Rendering/Culling/OcclusionBuffer.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Core/InputManager/InputActions.h>
#include <Pyros3D/Physics/Cloth/ClothComponent.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Utils/View/ViewFovScale.h>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <string>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <Pyros3D/Rendering/Terrain/TerrainHorizon.h>
#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

namespace p3d {

	void LuaClearTasks(sol::state* lua)
	{
		if (lua == NULL) return;
		sol::protected_function clear = (*lua)["tasks"]["clear"];
		if (clear.valid()) clear();
		// (and what was easing or waiting: it was easing things that are gone)
		sol::protected_function clearTweens = (*lua)["tween"]["clear"];
		if (clearTweens.valid()) clearTweens();
	}

	// PYROS_LUA_PROFILE=1: where the scripts' time goes. Every call a script
	// makes and every return is timed, and each function - a script's own, by
	// file and line, or one of the engine's it calls, by name - is given the
	// time spent in it and not in what it called. The busiest are printed
	// every twenty seconds, in milliseconds a frame. (It slows the scripts
	// down by what timing every call costs: for comparing functions with one
	// another, not for measuring the frame.)
	namespace {
		struct LuaOpen { std::string key; std::chrono::steady_clock::time_point start; f64 inside; };
		std::map<lua_State*, std::vector<LuaOpen> > g_luaOpen;
		std::map<std::string, f64> g_luaSelf;
		std::map<std::string, uint64> g_luaCalls;
		std::map<std::string, uint64> g_luaEngineCalls;        // an engine function, by the script function that called it
		uint64 g_luaFrames = 0;
		void LuaProfileHook(lua_State* L, lua_Debug* ar)
		{
			std::vector<LuaOpen> &open = g_luaOpen[L];
			const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
			if (ar->event == LUA_HOOKRET || ar->event == LUA_HOOKTAILCALL)
			{
				if (!open.empty())
				{
					const LuaOpen o = open.back();
					open.pop_back();
					const f64 all = std::chrono::duration<f64, std::milli>(now - o.start).count();
					g_luaSelf[o.key] += all - o.inside;
					g_luaCalls[o.key]++;
					if (!open.empty()) open.back().inside += all;
				}
				if (ar->event == LUA_HOOKRET) return;
			}
			if (open.size() > 200) return;
			LuaOpen o;
			o.start = now; o.inside = 0.0;
			if (lua_getinfo(L, "Sn", ar))
			{
				char key[200];
				if (ar->what && ar->what[0] == 'C') snprintf(key, sizeof(key), "[engine] %s", ar->name ? ar->name : "?");
				if (ar->what && ar->what[0] == 'C' && !open.empty() && open.back().key.compare(0, 8, "[engine]") != 0)
					g_luaEngineCalls[open.back().key + "  ->  " + (ar->name ? ar->name : "?")]++;
				else
				{
					const char* file = ar->short_src;
					for (const char* c = ar->short_src; *c; c++) if (*c == '/') file = c + 1;
					snprintf(key, sizeof(key), "%s:%d %s", file, ar->linedefined, ar->name ? ar->name : "");
				}
				o.key = key;
			}
			open.push_back(o);
		}
		void LuaProfileTick(lua_State* L)
		{
			static const bool on = std::getenv("PYROS_LUA_PROFILE") != NULL;
			if (!on) return;
			static std::map<lua_State*, bool> hooked;
			if (!hooked[L]) { hooked[L] = true; lua_sethook(L, LuaProfileHook, LUA_MASKCALL | LUA_MASKRET, 0); }
			// (between frames nothing of a script is running: what an error left open is dropped)
			for (std::map<lua_State*, std::vector<LuaOpen> >::iterator i = g_luaOpen.begin(); i != g_luaOpen.end(); ++i) i->second.clear();
			g_luaFrames++;
			static std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
			if (std::chrono::duration<f64>(std::chrono::steady_clock::now() - last).count() < 20.0) return;
			last = std::chrono::steady_clock::now();
			std::vector<std::pair<f64, std::string> > top;
			f64 total = 0.0;
			for (std::map<std::string, f64>::iterator i = g_luaSelf.begin(); i != g_luaSelf.end(); ++i) { top.push_back(std::make_pair(i->second, i->first)); total += i->second; }
			std::sort(top.rbegin(), top.rend());
			fprintf(stderr, "[lua profile] %.3f ms a frame in scripts, over %llu frames; by function, its own time:\n", total / (f64)g_luaFrames, (unsigned long long)g_luaFrames);
			for (size_t i = 0; i < top.size() && i < 36; i++)
				fprintf(stderr, "[lua profile]   %.4f ms  %5.1f%%  %6.1f calls a frame  %s\n", top[i].first / (f64)g_luaFrames, 100.0 * top[i].first / total,
					(f64)g_luaCalls[top[i].second] / (f64)g_luaFrames, top[i].second.c_str());
			{
				std::vector<std::pair<uint64, std::string> > most;
				for (std::map<std::string, uint64>::iterator i = g_luaEngineCalls.begin(); i != g_luaEngineCalls.end(); ++i) most.push_back(std::make_pair(i->second, i->first));
				std::sort(most.rbegin(), most.rend());
				fprintf(stderr, "[lua profile] calls into the engine, a frame, by who makes them:\n");
				for (size_t i = 0; i < most.size() && i < 30; i++)
					fprintf(stderr, "[lua profile]   %7.1f  %s\n", (f64)most[i].first / (f64)g_luaFrames, most[i].second.c_str());
				g_luaEngineCalls.clear();
			}
			g_luaSelf.clear(); g_luaCalls.clear(); g_luaFrames = 0;
		}
	}

	void LuaCollectWithinBudget(sol::state* lua, const f64 budgetMs)
	{
		if (lua != NULL) LuaProfileTick(lua->lua_state());
		static const bool automatic = []() { const char* v = std::getenv("PYROS_LUA_GC"); return v != NULL && std::string(v) == "auto"; }();
		if (lua == NULL) return;
		// (this is the scripts' once-a-frame housekeeping: what they have
		// spread over frames is given its share first - see `tasks`)
		{
			sol::protected_function pump = (*lua)["tasks"]["_pump"];
			if (pump.valid()) pump();
			sol::protected_function ease = (*lua)["tween"]["_pump"];
			if (ease.valid()) ease();
		}
		if (automatic) return;
		lua_State* L = lua->lua_state();
		// PYROS_LUA_GC=gen: by generations instead of the way below (a whole cycle
		// spread over frames within a budget). Tried as the rule and not kept:
		// over two minutes it cost nothing where the cycles cost a quarter of a
		// millisecond a frame, but over five the heap had grown to 28 MB where
		// the cycles hold it at 18, the cost had come back (0.22 ms against
		// 0.34) and one frame took 60 ms. Left to be tried again with a game
		// whose scripts make less that lives a few frames.
		{
			static const bool steps = []() { const char* v = std::getenv("PYROS_LUA_GC"); return !(v != NULL && std::string(v) == "gen"); }();
			if (!steps)
			{
				static std::map<lua_State*, f64> last;
				std::map<lua_State*, f64>::iterator at = last.find(L);
				if (at == last.end())
				{
#if LUA_VERSION_NUM >= 505
					lua_gc(L, LUA_GCGEN);
#else
					lua_gc(L, LUA_GCGEN, 20, 100);
#endif
					lua_gc(L, LUA_GCSTOP);
					at = last.insert(std::make_pair(L, (f64)lua_gc(L, LUA_GCCOUNT))).first;
				}
				const f64 kb = (f64)lua_gc(L, LUA_GCCOUNT);
				FrameProfiler::Instance().Counter("Lua.HeapMb", kb / 1024.0);
				uint32 collected = 0;
				if (kb > at->second + 512.0)
				{
#if LUA_VERSION_NUM >= 505
					lua_gc(L, LUA_GCSTEP, (size_t)0);
#else
					lua_gc(L, LUA_GCSTEP, 0);
#endif
					at->second = (f64)lua_gc(L, LUA_GCCOUNT);
					collected = 1;
				}
				FrameProfiler::Instance().Counter("Lua.GcSteps", (f64)collected);
				return;
			}
		}
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
		// setLODDistanceScale(k): every level of detail is kept k times further out (2: the finer
		// models twice as far). For a magnified view - a scope at 4x sees a thing four times nearer
		// than it is, and with the distances as they were it saw the coarse model of something that
		// filled the lens. One scale for every view, so two views do not fight over which level a
		// thing is drawn at (each change rebuilds what the scene draws). 1 by default.
		lua->set_function("setLODDistanceScale", [](const f32 k) { RenderingComponent::SetLODDistanceScale(k); });
		// Seconds a change of level of detail takes to dissolve one level into the other (0: at once).
		lua->set_function("setLODCrossfade", [](const f32 s) { RenderingComponent::SetLODCrossfade(s); });
		lua->set_function("getLODCrossfade", []() { return RenderingComponent::GetLODCrossfade(); });
		// What the big still things in view hide is not drawn (OcclusionBuffer); on unless said.
		lua->set_function("setOcclusionCulling", [](const bool on) { OcclusionBuffer::SetEnabled(on); });
		// setShadowBudget(pointLights [, spotLights]) - how many of each may cast shadows in a frame (0: no limit)
		lua->set_function("setShadowBudget", [](const uint32 point, sol::optional<uint32> spot) { IRenderer::SetShadowBudget(point, spot.value_or(point)); });
		lua->set_function("getOcclusionCulling", []() { return OcclusionBuffer::GetEnabled(); });
		lua->set_function("getLODDistanceScale", []() { return RenderingComponent::GetLODDistanceScale(); });
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
		// lang: a game in more than one language. Its words are in files -
		// assets/lang/en.json, assets/lang/pt.json - each a flat table of
		// "key": "words", and everything shown is asked for by key:
		//
		//   lang.load("pt")                    false if there is no such file
		//   lang.loadBest({ "pt", "en" })      the device's language if it is one of these, else the first
		//   tr("level_clear")                  the words, or the key if there are none
		//   tr("score", 1200)                  {1}, {2}... in the words are what follows the key
		//   lang.apply(scene)                  every text in the scene whose words start with "@" is that
		//                                      key's words: a label authored "@play" reads PLAY, or JOGAR
		//
		// lang.apply remembers which key each text had, so calling it again
		// after another lang.load changes the language where it stands.
		// Accented letters of the Latin alphabets are drawn; other scripts
		// are not yet.
		{
			sol::table lang = lua->create_named_table("lang");
			lang.set_function("_readText", [](const std::string &path, sol::this_state ts) -> sol::object {
				sol::state_view L(ts);
				std::ifstream in(path.c_str(), std::ios::binary);
				if (!in.is_open()) return sol::make_object(L, sol::lua_nil);
				std::stringstream ss;
				ss << in.rdbuf();
				return sol::make_object(L, ss.str());
			});
			// The device's language as two letters ("pt", "en"), or "".
			lang.set_function("system", []() -> std::string {
				std::string code;
#ifdef EMSCRIPTEN
				char* text = (char*)EM_ASM_PTR({ return stringToNewUTF8((navigator.language || "").toString()); });
				if (text) { code = text; free(text); }
#else
				const char* names[] = { "LC_ALL", "LC_MESSAGES", "LANG" };
				for (int i = 0; i < 3 && code.empty(); i++)
					if (const char* v = std::getenv(names[i])) code = v;
#endif
				std::string two;
				for (size_t i = 0; i < code.size() && two.size() < 2; i++)
				{
					const char c = code[i];
					if (c >= 'A' && c <= 'Z') two += (char)(c - 'A' + 'a');
					else if (c >= 'a' && c <= 'z') two += c;
					else break;
				}
				return two.size() == 2 ? two : std::string();
			});
		}
		lua->script(R"LUA(
lang.code, lang.strings = "", {}
local keys = {}
function lang.load(code, path)
	local where = path or (ASSETS_PATH .. "lang/" .. code .. ".json")
	local text = lang._readText(where)
	if not text then return false end
	local t, err = json.decode(text)
	if type(t) ~= "table" then echo("ERROR: lang - " .. where .. ": " .. tostring(err)) return false end
	lang.code, lang.strings = code, t
	return true
end
function lang.loadBest(codes)
	local want = lang.system()
	for _, c in ipairs(codes) do if c == want and lang.load(c) then return c end end
	for _, c in ipairs(codes) do if lang.load(c) then return c end end
	return nil
end
function tr(key, ...)
	local s = lang.strings[key] or key
	local n = select("#", ...)
	if n > 0 then
		local args = { ... }
		s = s:gsub("{(%d+)}", function(i) local v = args[tonumber(i)] if v == nil then return "{" .. i .. "}" end return tostring(v) end)
	end
	return s
end
function lang.apply(scene)
	local function visit(go)
		local name = go:getName()
		local world = go:getComponent("Text2D")
		local text = world and world:getText() or ui.getText(go)
		local key = (text and text:sub(1, 1) == "@") and text:sub(2) or keys[name]
		if key and key ~= "" then
			keys[name] = key
			if world then world:setText(tr(key)) else ui.setText(go, tr(key)) end
		end
		for _, child in ipairs(go:getChildren()) do visit(child) end
	end
	for _, go in ipairs(scene:getAllGameObjects()) do visit(go) end
end
)LUA");

		// tween: a value taken from where it is to somewhere else over a time,
		// by the engine, a frame at a time - what every script otherwise
		// writes by hand with a timer and a lerp.
		//
		//   tween.to(t, seconds, { x = 4, alpha = 0 }, "out")    numbers in a table
		//   tween.value(0, 1, seconds, "inout", function(v) ... end)
		//   tween.move(go, x, y, seconds, "out")                 an object's position (z kept)
		//   tween.scale(go, s, seconds, "back")                  its scale, the same in x and y
		//   tween.rotate(go, radians, seconds)                   about z
		//   tween.alpha(sprite, a, seconds)                      anything with setAlpha / getColor
		//   tween.tint(element, r, g, b, a, seconds)             a UI element's tint - a fade to black
		//                                                        is a black image over the canvas, eased
		//   tween.after(seconds, function() ... end)             just the wait
		//
		// Each takes a last argument, a function called when it is done, and
		// returns a handle: tween.cancel(handle). Eases: "linear", "in",
		// "out", "inout", "back", "bounce", "elastic". tween.clear() stops
		// them all (the engine does when a scene goes). Run by the host once
		// a frame, on the wall clock.
		lua->script(R"LUA(
tween = {}
local list, last = {}, nil
local ease = {
	linear = function(t) return t end,
	["in"] = function(t) return t * t * t end,
	out = function(t) local u = 1 - t return 1 - u * u * u end,
	inout = function(t) if t < 0.5 then return 4 * t * t * t end local u = -2 * t + 2 return 1 - u * u * u / 2 end,
	back = function(t) local c = 1.70158 local u = t - 1 return 1 + (c + 1) * u * u * u + c * u * u end,
	bounce = function(t)
		local n, d = 7.5625, 2.75
		if t < 1 / d then return n * t * t
		elseif t < 2 / d then t = t - 1.5 / d return n * t * t + 0.75
		elseif t < 2.5 / d then t = t - 2.25 / d return n * t * t + 0.9375 end
		t = t - 2.625 / d return n * t * t + 0.984375
	end,
	elastic = function(t)
		if t <= 0 then return 0 elseif t >= 1 then return 1 end
		return 2 ^ (-10 * t) * math.sin((t * 10 - 0.75) * (2 * math.pi) / 3) + 1
	end,
}
tween.ease = ease
local function add(seconds, name, step, done)
	local h = { t = 0, time = math.max(seconds or 0, 0), ease = ease[name or "out"] or ease.out, step = step, onDone = done, done = false }
	list[#list + 1] = h
	return h
end
-- (the function that ends a call may be given in place of the ease's name)
local function tail(a, b) if type(a) == "function" then return nil, a end return a, b end
function tween.value(from, to, seconds, name, fn, done)
	if type(name) == "function" then name, fn, done = nil, name, fn end
	return add(seconds, name, function(k) fn(from + (to - from) * k) end, done)
end
function tween.to(target, seconds, goal, name, done)
	name, done = tail(name, done)
	local from = {}
	for key, v in pairs(goal) do from[key] = target[key] or 0 end
	return add(seconds, name, function(k)
		for key, v in pairs(goal) do target[key] = from[key] + (v - from[key]) * k end
	end, done)
end
function tween.move(go, x, y, seconds, name, done)
	name, done = tail(name, done)
	local p = go:getPosition()
	local x0, y0, z = p.x, p.y, p.z
	return add(seconds, name, function(k) go:setPosition(Vec3.new(x0 + (x - x0) * k, y0 + (y - y0) * k, z)) end, done)
end
function tween.scale(go, to, seconds, name, done)
	name, done = tail(name, done)
	local s = go:getScale()
	local x0, y0, z = s.x, s.y, s.z
	return add(seconds, name, function(k) go:setScale(Vec3.new(x0 + (to - x0) * k, y0 + (to - y0) * k, z)) end, done)
end
function tween.rotate(go, to, seconds, name, done)
	name, done = tail(name, done)
	local r = go:getRotation()
	local x, y, z0 = r.x, r.y, r.z
	return add(seconds, name, function(k) go:setRotation(Vec3.new(x, y, z0 + (to - z0) * k)) end, done)
end
function tween.alpha(thing, to, seconds, name, done)
	name, done = tail(name, done)
	local from = thing.getColor and thing:getColor().w or 1
	return add(seconds, name, function(k) thing:setAlpha(from + (to - from) * k) end, done)
end
function tween.tint(element, r, g, b, a, seconds, name, done)
	name, done = tail(name, done)
	local from = { 0, 0, 0, 0 }
	local started = false
	return add(seconds, name, function(k)
		if not started then
			started = true
			local c = ui.getTint and ui.getTint(element)
			if c then from = { c.x, c.y, c.z, c.w } else from = { r, g, b, 0 } end
		end
		ui.setTint(element, Vec4.new(from[1] + (r - from[1]) * k, from[2] + (g - from[2]) * k, from[3] + (b - from[3]) * k, from[4] + (a - from[4]) * k))
	end, done)
end
function tween.after(seconds, fn) return add(seconds, "linear", function() end, fn) end
function tween.cancel(h) if h then h.done = true end end
function tween.clear() for i = #list, 1, -1 do list[i].done = true list[i] = nil end last = nil end
function tween.count() return #list end
function tween._pump()
	local now = getClock()
	local dt = last and math.min(now - last, 0.1) or 0
	last = now
	local i = 1
	while i <= #list do
		local h = list[i]
		if not h.done then
			h.t = h.t + dt
			local k = (h.time <= 0) and 1 or math.min(h.t / h.time, 1)
			local ok, err = pcall(h.step, h.ease(k))
			if not ok then echo("ERROR: tween - " .. tostring(err)) h.done = true
			elseif k >= 1 then
				h.done = true
				if h.onDone then
					local ok2, err2 = pcall(h.onDone)
					if not ok2 then echo("ERROR: tween - " .. tostring(err2)) end
				end
			end
		end
		if h.done then table.remove(list, i) else i = i + 1 end
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
				"Numpad0", Event::Input::Keyboard::Numpad0, "Numpad1", Event::Input::Keyboard::Numpad1, "Numpad2", Event::Input::Keyboard::Numpad2,
				"Numpad3", Event::Input::Keyboard::Numpad3, "Numpad4", Event::Input::Keyboard::Numpad4, "Numpad5", Event::Input::Keyboard::Numpad5,
				"Numpad6", Event::Input::Keyboard::Numpad6, "Numpad7", Event::Input::Keyboard::Numpad7, "Numpad8", Event::Input::Keyboard::Numpad8,
				"Numpad9", Event::Input::Keyboard::Numpad9,
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

		// Held state, by asking. The Input object above is event-based - it
		// says when a key goes down and when it comes up - and every game
		// with a character in it then keeps its own table of which keys are
		// held, built from those two events. This is that table.
		lua->set_function("isKeyDown", [](const uint32 key) { return InputManager::IsKeyDown(key); });
		lua->set_function("isMouseButtonDown", [](const uint32 button) { return InputManager::IsMouseButtonDown(button); });
		// Fingers on the screen. touch.count() of them; touch.get(i) is the
		// i-th (from 1): id, x, y, began, ended - x and y in the pointer's
		// pixels (view.screenToWorld takes them), began and ended true for
		// the one frame it came down or was lifted. The first finger also
		// moves the pointer, so a game written for a mouse needs none of this.
		{
			sol::table touch = lua->create_named_table("touch");
			touch.set_function("count", []() { return (int)InputManager::GetTouches().size(); });
			touch.set_function("get", [](const int i) {
				const std::vector<InputManager::Touch> &t = InputManager::GetTouches();
				if (i < 1 || (size_t)i > t.size()) return std::make_tuple((f64)-1, 0.f, 0.f, false, false);
				const InputManager::Touch &f = t[(size_t)i - 1];
				return std::make_tuple((f64)f.id, f.position.x, f.position.y, f.began, f.ended);
			});
			// Two fingers, as the gesture they make this frame:
			//
			//   local scale, cx, cy, dx, dy, turn = touch.pinch()
			//   if scale then view.setZoom(view.getZoom() / scale) end
			//
			// scale is how much further apart they are than a frame ago (1 is
			// no change, above 1 spreading), cx, cy the point midway between
			// them, dx, dy how far that point moved, and turn the angle, in
			// radians, the pair turned through. Nothing with fewer than two
			// fingers down, or in the frame the second one lands.
			touch.set_function("pinch", [](sol::this_state ts) -> sol::variadic_results {
				sol::variadic_results out;
				const std::vector<InputManager::Touch> &t = InputManager::GetTouches();
				const InputManager::Touch* a = NULL; const InputManager::Touch* b = NULL;
				for (size_t i = 0; i < t.size(); i++)
				{
					if (t[i].ended || t[i].began) continue;
					if (!a) a = &t[i]; else if (!b) { b = &t[i]; break; }
				}
				if (!a || !b) { out.push_back(sol::make_object(ts, sol::lua_nil)); return out; }
				const Vec2 now = b->position - a->position, was = b->previous - a->previous;
				const f32 dNow = sqrtf(now.x * now.x + now.y * now.y), dWas = sqrtf(was.x * was.x + was.y * was.y);
				const Vec2 mid = (a->position + b->position) * 0.5f, midWas = (a->previous + b->previous) * 0.5f;
				f32 turn = atan2f(now.y, now.x) - atan2f(was.y, was.x);
				if (turn > 3.14159265f) turn -= 6.2831853f;
				if (turn < -3.14159265f) turn += 6.2831853f;
				out.push_back(sol::make_object(ts, (dWas > 1.f && dNow > 1.f) ? dNow / dWas : 1.f));
				out.push_back(sol::make_object(ts, mid.x));
				out.push_back(sol::make_object(ts, mid.y));
				out.push_back(sol::make_object(ts, mid.x - midWas.x));
				out.push_back(sol::make_object(ts, mid.y - midWas.y));
				out.push_back(sol::make_object(ts, turn));
				return out;
			});
		}

		// Game controllers. A pad is 0-7 (0 is the first plugged in); buttons
		// and axes use one layout for every recognised pad:
		//
		//   local x = gamepad.axis(0, GamepadAxis.LeftX)       -- -1..1
		//   if gamepad.isDown(0, GamepadButton.A) then jump() end
		//
		// Sticks read +1 UP and +1 RIGHT; triggers 0..1. A stick at rest is
		// rarely exactly 0, so axis() takes an optional dead zone (0.15 by
		// default) below which it reads 0.
		lua->create_named_table("GamepadButton",
			"A", 0, "B", 1, "X", 2, "Y", 3, "Back", 4, "Guide", 5, "Start", 6,
			"LeftStick", 7, "RightStick", 8, "LeftShoulder", 9, "RightShoulder", 10,
			"DpadUp", 11, "DpadDown", 12, "DpadLeft", 13, "DpadRight", 14);
		lua->create_named_table("GamepadAxis",
			"LeftX", 0, "LeftY", 1, "RightX", 2, "RightY", 3, "LeftTrigger", 4, "RightTrigger", 5);
		{
			sol::table g = lua->create_named_table("gamepad");
			g.set_function("isConnected", [](sol::optional<uint32> pad) { return InputManager::IsJoypadConnected(pad ? *pad : 0u); });
			g.set_function("name", [](sol::optional<uint32> pad) { return InputManager::GetJoypadName(pad ? *pad : 0u); });
			g.set_function("isDown", [](const uint32 pad, const uint32 button) { return InputManager::IsJoypadButtonDown(pad, button); });
			g.set_function("axis", [](const uint32 pad, const uint32 axis, sol::optional<f32> deadZone) {
				const f32 v = InputManager::GetJoypadAxis(pad, axis);
				const f32 dz = deadZone ? *deadZone : 0.15f;
				if (fabsf(v) <= dz) return 0.f;
				// Rescaled, so the value still reaches from 0 at the edge of
				// the dead zone to 1 at the stop instead of jumping to 0.15.
				const f32 k = (fabsf(v) - dz) / (1.f - dz);
				return v < 0.f ? -k : k;
			});
			g.set_function("count", []() { uint32 n = 0; for (uint32 i = 0; i < 8; i++) if (InputManager::IsJoypadConnected(i)) n++; return n; });
		}

		// Cloth: the mesh an object renders, simulated (ClothComponent). From a scene it is a
		// "Cloth" component (obj:getComponent("Cloth")); from a script:
		//
		//   local flag = Cloth.attach(obj)             -- obj renders a subdivided Plane, say
		//   flag:pinSide("-x")                         -- held along the pole side
		//   flag:setWind(6, 0, 1)
		//   local s = flag:addSphere(x, y, z, 0.5)     -- and flag:setSphere(s, ...) to move it
		{
			lua->new_usertype<ClothComponent>("ClothComponent", sol::no_constructor,
				sol::base_classes, sol::bases<IComponent>(),
				"pinSide", [](ClothComponent &c, const std::string &side, sol::optional<f32> band) { c.PinSide(side, band ? *band : 0.01f); },
				"pinVertex", &ClothComponent::PinVertex,
				"clearPins", &ClothComponent::ClearPins,
				"setStretch", &ClothComponent::SetStretch, "setBend", &ClothComponent::SetBend,
				"setDamping", &ClothComponent::SetDamping, "setThickness", &ClothComponent::SetThickness, "setDensity", &ClothComponent::SetDensity,
				"setFriction", &ClothComponent::SetFriction, "setDrag", &ClothComponent::SetDrag,
				"setSubsteps", &ClothComponent::SetSubsteps, "setTurbulence", &ClothComponent::SetTurbulence,
				"setGravity", [](ClothComponent &c, f32 x, f32 y, f32 z) { c.SetGravity(Vec3(x, y, z)); },
				"setWind", [](ClothComponent &c, f32 x, f32 y, f32 z) { c.SetWind(Vec3(x, y, z)); },
				"getWind", [](ClothComponent &c) { const Vec3 &w = c.GetWind(); return std::make_tuple(w.x, w.y, w.z); },
				"setGround", [](ClothComponent &c, bool on, sol::optional<f32> y) { c.SetGround(on, y ? *y : 0.f); },
				// space: "world" (default), "object" or "parent"
				"addSphere", [](ClothComponent &c, f32 x, f32 y, f32 z, f32 r, sol::optional<std::string> sp) {
					const uint32 s = !sp ? ClothComponent::World : (*sp == "object" ? ClothComponent::Owner : (*sp == "parent" ? ClothComponent::Parent : ClothComponent::World));
					return c.AddSphere(Vec3(x, y, z), r, s); },
				"addCapsule", [](ClothComponent &c, f32 ax, f32 ay, f32 az, f32 bx, f32 by, f32 bz, f32 r, sol::optional<std::string> sp) {
					const uint32 s = !sp ? ClothComponent::World : (*sp == "object" ? ClothComponent::Owner : (*sp == "parent" ? ClothComponent::Parent : ClothComponent::World));
					return c.AddCapsule(Vec3(ax, ay, az), Vec3(bx, by, bz), r, s); },
				"setSphere", [](ClothComponent &c, uint32 i, f32 x, f32 y, f32 z, f32 r) { c.SetSphere(i, Vec3(x, y, z), r); },
				"setCapsule", [](ClothComponent &c, uint32 i, f32 ax, f32 ay, f32 az, f32 bx, f32 by, f32 bz, f32 r) { c.SetCapsule(i, Vec3(ax, ay, az), Vec3(bx, by, bz), r); },
				"clearColliders", &ClothComponent::ClearColliders,
				"reset", &ClothComponent::Reset,
				"setRunning", &ClothComponent::SetRunning, "isRunning", &ClothComponent::IsRunning,
				"getParticleCount", &ClothComponent::GetParticleCount,
				"getPinnedCount", &ClothComponent::GetPinnedCount,
				"getConstraintCount", &ClothComponent::GetConstraintCount,
				"getParticlePosition", [](ClothComponent &c, uint32 i) { const Vec3 p = c.GetParticlePosition(i); return std::make_tuple(p.x, p.y, p.z); });
			sol::table cl = lua->create_named_table("Cloth");
			cl.set_function("attach", [](GameObject* go) -> std::shared_ptr<ClothComponent> {
				if (!go) return nullptr;
				const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
				for (size_t i = 0; i < comps.size(); i++)
					if (comps[i] && comps[i]->GetComponentType() == ComponentType::Cloth) return std::static_pointer_cast<ClothComponent>(comps[i]);
				std::shared_ptr<ClothComponent> c = std::make_shared<ClothComponent>();
				c->SetRunning(true);
				go->Add(c);
				return c; });
		}

		// What the game says, in the player's language (Localization: assets/strings/<code>.json).
		//   label:setText(tr("menu.play"))           or give a UI text "@menu.play" and it follows
		//   tr("hud.ammo", { count = 30 })           -- "{count} rounds" -> "30 rounds"
		//   localization.setLanguage("pt")           -- every "@key" text redraws itself
		lua->set_function("tr", [](const std::string &key, sol::optional<sol::table> args) {
			if (!args) return Localization::Get(key);
			std::map<std::string, std::string> a;
			for (std::pair<sol::object, sol::object> kv : *args)
			{
				if (!kv.first.is<std::string>()) continue;
				const std::string k = kv.first.as<std::string>();
				if (kv.second.is<std::string>()) a[k] = kv.second.as<std::string>();
				else if (kv.second.get_type() == sol::type::number)
				{
					const f64 v = kv.second.as<f64>();
					char buf[64];
					if (v == (f64)(int64)v) snprintf(buf, sizeof(buf), "%lld", (long long)v); else snprintf(buf, sizeof(buf), "%g", v);
					a[k] = buf;
				}
				else if (kv.second.is<bool>()) a[k] = kv.second.as<bool>() ? "true" : "false";
			}
			return Localization::Format(key, a);
		});
		{
			sol::table l = lua->create_named_table("localization");
			l.set_function("setLanguage", [](const std::string &code) { return Localization::SetLanguage(code); });
			l.set_function("getLanguage", []() { return Localization::GetLanguage(); });
			l.set_function("setFallback", [](const std::string &code) { Localization::SetFallback(code); });
			l.set_function("languages", [](sol::this_state ts) {
				sol::state_view L(ts); sol::table t = L.create_table();
				std::vector<std::string> v = Localization::GetLanguages();
				for (size_t i = 0; i < v.size(); i++) t[i + 1] = v[i];
				return t; });
			l.set_function("has", [](const std::string &key) { return Localization::Has(key); });
			l.set_function("reload", []() { Localization::Reload(); });
		}

		// What the player means, bound to what they press (InputActions):
		//
		//   controls.bind("jump", "key:Space"); controls.bind("jump", "pad:A")
		//   controls.bind("moveRight", "key:D"); controls.bind("moveRight", "padaxis:LeftX+")
		//   if controls.pressed("jump") then ... end
		//   local x = controls.axis("moveLeft", "moveRight")          -- -1..1, keys or stick
		//
		// A remap screen: controls.beginCapture(), then each frame controls.capture() until it gives
		// the binding pressed ("" until then). controls.save() is every binding as one string, and
		// controls.load(text) puts them back (keep it in the game's settings file).
		{
			sol::table c = lua->create_named_table("controls");
			c.set_function("bind", [](const std::string &a, const std::string &b) { InputActions::Bind(a, b); });
			c.set_function("unbind", [](const std::string &a, sol::optional<std::string> b) { InputActions::Unbind(a, b ? *b : std::string()); });
			c.set_function("bindings", [](const std::string &a, sol::this_state L) {
				sol::state_view lv(L); sol::table t = lv.create_table();
				std::vector<std::string> b = InputActions::GetBindings(a);
				for (size_t i = 0; i < b.size(); i++) t[i + 1] = b[i];
				return t; });
			c.set_function("actions", [](sol::this_state L) {
				sol::state_view lv(L); sol::table t = lv.create_table();
				std::vector<std::string> b = InputActions::GetActions();
				for (size_t i = 0; i < b.size(); i++) t[i + 1] = b[i];
				return t; });
			c.set_function("value", [](const std::string &a) { return InputActions::Value(a); });
			c.set_function("down", [](const std::string &a) { return InputActions::Down(a); });
			c.set_function("pressed", [](const std::string &a) { return InputActions::Pressed(a); });
			c.set_function("released", [](const std::string &a) { return InputActions::Released(a); });
			c.set_function("axis", [](const std::string &n, const std::string &p) { return InputActions::Axis(n, p); });
			c.set_function("beginCapture", []() { InputActions::BeginCapture(); });
			c.set_function("capture", []() { return InputActions::Capture(); });
			c.set_function("save", []() { return InputActions::Serialize(); });
			c.set_function("load", [](const std::string &t) { return InputActions::Deserialize(t); });
			c.set_function("setDeadZone", [](const f32 d) { InputActions::SetDeadZone(d); });
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
			int32 mesh = -1;
			bool transparent = false;
			const bool ok = ScreenPick(winW, winH, mx, my, camera, projection, scene,
				&p, &n, &name, &dist, &mesh, &transparent);
			// (then the submesh hit - its place in rc:getMeshes(), from 1 - and whether its material
			// is a transparent one: a window)
			return std::make_tuple(ok, p.x, p.y, p.z, n.x, n.y, n.z, name, dist, mesh + 1, transparent);
		});
		// placeDecal(w, h, x, y, camera, projection, scene, material, size): placeDecalAtCursor that
		// says what it made - the decal's component (nil if nothing was hit) and the submesh it is on
		// (from 1). removeDecal(object, component) takes that one off again.
		lua->set_function("placeDecal", [](float winW, float winH, float mx, float my, GameObject* camera,
			Projection* projection, SceneGraph* scene, sol::object materialObj, const Vec3 &dimensions) {
			std::shared_ptr<IComponent> comp;
			int32 mesh = -1;
			const bool ok = PlaceDecalAt(winW, winH, mx, my, camera, projection, scene, LuaObjectToMaterial(materialObj), dimensions, &comp, &mesh);
			return std::make_tuple(ok ? comp : std::shared_ptr<IComponent>(), mesh + 1);
		});
		lua->set_function("removeDecal", &RemoveLuaDecal);
		lua->set_function("clearDecals", &ClearLuaDecals);
		// The view's zoom, as a factor on the active camera's field of view (1 = as the camera
		// is set; 0.8 = a fifth narrower): a soft aim, binoculars. The host applies it.
		lua->set_function("setViewFovScale", [](f32 k) { SetViewFovScale(k); });
		lua->set_function("getViewFovScale", []() { return GetViewFovScale(); });

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
		lua->set_function("setIKConstraintMatchRotation", &SetIKConstraintMatchRotation);
		// The object's animation state machine (an Animator component).
		lua->set_function("setAnimatorFloat", &SetAnimatorFloat);
		lua->set_function("setAnimatorBool", &SetAnimatorBool);
		lua->set_function("setAnimatorTrigger", &SetAnimatorTrigger);
		lua->set_function("getAnimatorState", &GetAnimatorState);
		lua->set_function("addAnimator", &AddAnimator);
		lua->set_function("playAnimatorState", sol::overload(&PlayAnimatorState,
			[](GameObject* go, const std::string &state) { return PlayAnimatorState(go, state, 0.2f); }));

		// terrain - ground height queries and runtime sculpting/painting
		// (see TerrainEditor.h). One editor per process: a stroke's state
		// (which tiles need collision and foliage rebuilt) lives in it
		// between calls, until terrain.finishStroke().
		{
			static TerrainEditor editor;
			// People steering round each other (Crowd - local avoidance on the ground plane):
			//   local crowd = Crowd.new([neighbourRadius])
			//   local id = crowd:add(x, z [, radius, maxSpeed])      crowd:remove(id)
			//   crowd:setPreferredVelocity(id, vx, vz)   where it wants to go, per second
			//   crowd:setPosition(id, x, z)   crowd:setMaxSpeed(id, s)   crowd:setFixed(id, true)
			//   crowd:update(dt)   then   local x, z = crowd:getPosition(id)   crowd:getVelocity(id)
			// A navigation mesh (NavMesh.h): built from the level, asked the way.
			//
			//   local nav = NavMesh.new()
			//   nav:addScene(scene)                       -- the static objects' meshes
			//   nav:build({ agentRadius = 0.4, agentMaxClimb = 0.4 })
			//   local reached, pts = nav:findPath(x0, y0, z0, x1, y1, z1)
			//   for i, p in ipairs(pts) do ... p.x, p.y, p.z, p.link (a jump starts here) end
			lua->new_usertype<NavMesh>("NavMesh",
				sol::constructors<NavMesh()>(),
				"addScene", [](NavMesh &n, SceneGraph* sc, sol::optional<bool> staticOnly) { n.AddScene(sc, staticOnly.value_or(true)); },
				"addObject", [](NavMesh &n, GameObject* go, sol::optional<bool> kids) { n.AddObject(go, kids.value_or(true)); },
				"clearGeometry", &NavMesh::ClearGeometry,
				"addLink", [](NavMesh &n, f32 ax, f32 ay, f32 az, f32 bx, f32 by, f32 bz, sol::optional<f32> r, sol::optional<bool> both) {
					n.AddLink(Vec3(ax, ay, az), Vec3(bx, by, bz), r.value_or(0.5f), both.value_or(false)); },
				"clearLinks", &NavMesh::ClearLinks,
				"build", [](NavMesh &n, sol::optional<sol::table> t) {
					NavMeshSettings st;
					if (t)
					{
						sol::table o = *t;
						st.agentRadius = o.get_or("agentRadius", st.agentRadius); st.agentHeight = o.get_or("agentHeight", st.agentHeight);
						st.agentMaxClimb = o.get_or("agentMaxClimb", st.agentMaxClimb); st.agentMaxSlope = o.get_or("agentMaxSlope", st.agentMaxSlope);
						st.cellSize = o.get_or("cellSize", st.cellSize); st.cellHeight = o.get_or("cellHeight", st.cellHeight);
						st.tileSize = o.get_or("tileSize", st.tileSize); st.regionMinSize = o.get_or("regionMinSize", st.regionMinSize);
					}
					return n.Build(st); },
				"findPath", [](NavMesh &n, f32 fx, f32 fy, f32 fz, f32 tx, f32 ty, f32 tz, sol::this_state L) {
					std::vector<Vec3> pts;
					const bool reached = n.FindPath(Vec3(fx, fy, fz), Vec3(tx, ty, tz), pts);
					sol::state_view lv(L); sol::table t = lv.create_table();
					for (size_t i = 0; i < pts.size(); i++)
					{
						sol::table p = lv.create_table();
						p["x"] = pts[i].x; p["y"] = pts[i].y; p["z"] = pts[i].z; p["link"] = n.IsLinkPoint((uint32)i);
						t[i + 1] = p;
					}
					return std::make_tuple(reached, t); },
				"nearest", [](NavMesh &n, f32 x, f32 y, f32 z) { Vec3 o; const bool ok = n.NearestPoint(Vec3(x, y, z), o); return std::make_tuple(ok, o.x, o.y, o.z); },
				"raycast", [](NavMesh &n, f32 fx, f32 fy, f32 fz, f32 tx, f32 ty, f32 tz) { Vec3 h; const bool clear = n.Raycast(Vec3(fx, fy, fz), Vec3(tx, ty, tz), h); return std::make_tuple(clear, h.x, h.y, h.z); },
				"randomPoint", [](NavMesh &n, sol::optional<f32> x, sol::optional<f32> y, sol::optional<f32> z, sol::optional<f32> r) {
					Vec3 o; const bool ok = n.RandomPoint(o, Vec3(x.value_or(0.f), y.value_or(0.f), z.value_or(0.f)), r.value_or(0.f)); return std::make_tuple(ok, o.x, o.y, o.z); },
				"save", &NavMesh::Save,
				"load", &NavMesh::Load,
				"isBuilt", &NavMesh::IsBuilt,
				"triangles", &NavMesh::GetTriangleCount,
				"polygons", &NavMesh::GetPolygonCount,
				"tiles", &NavMesh::GetTileCount,
				"buildMs", &NavMesh::GetBuildMilliseconds);
			// The scene's own, baked in the editor and loaded with it (nil: it has none).
			//   local nav = getNavMesh(scene)
			// An agent that walks its object on the scene's navigation mesh (NavAgentComponent):
			//   local a = NavAgent.attach(obj)   (or obj:getComponent("NavAgent") from a scene)
			//   a:setDestination(x, y, z)        a:isMoving()   a:getSpeed() -> the walk blend
			lua->new_usertype<NavAgentComponent>("NavAgentComponent", sol::no_constructor,
				sol::base_classes, sol::bases<IComponent>(),
				"setDestination", [](NavAgentComponent &a, f32 x, f32 y, f32 z) { return a.SetDestination(Vec3(x, y, z)); },
				"stop", &NavAgentComponent::Stop,
				"isMoving", &NavAgentComponent::IsMoving,
				"isReachable", &NavAgentComponent::IsReachable,
				"isOnLink", &NavAgentComponent::IsOnLink,
				"getSpeed", &NavAgentComponent::GetSpeed,
				"getRemainingDistance", &NavAgentComponent::GetRemainingDistance,
				"getDestination", [](NavAgentComponent &a) { const Vec3 &d = a.GetDestination(); return std::make_tuple(d.x, d.y, d.z); },
				"setMaxSpeed", &NavAgentComponent::SetMaxSpeed, "getMaxSpeed", &NavAgentComponent::GetMaxSpeed,
				"setRadius", &NavAgentComponent::SetRadius,
				"setAcceleration", &NavAgentComponent::SetAcceleration,
				"setStoppingDistance", &NavAgentComponent::SetStoppingDistance,
				"setFaceMovement", [](NavAgentComponent &a, bool on, sol::optional<f32> off) { a.SetFaceMovement(on, off.value_or(0.f)); },
				"setTurnSpeed", &NavAgentComponent::SetTurnSpeed,
				"setRunning", &NavAgentComponent::SetRunning, "isRunning", &NavAgentComponent::IsRunning);
			sol::table nat = lua->create_named_table("NavAgent");
			nat.set_function("attach", [](GameObject* go) -> std::shared_ptr<NavAgentComponent> {
				if (!go) return nullptr;
				const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
				for (size_t i = 0; i < comps.size(); i++)
					if (comps[i] && comps[i]->GetComponentType() == ComponentType::NavAgent) return std::static_pointer_cast<NavAgentComponent>(comps[i]);
				std::shared_ptr<NavAgentComponent> a = std::make_shared<NavAgentComponent>();
				a->SetRunning(true);
				go->Add(a);
				return a; });
			// A kinematic character (CharacterController): walks where it is told, stopped and slid by
			// what is solid, up steps and slopes, gravity and jumps.
			//   local cc = CharacterController.attach(obj, physics)   (or obj:getComponent("CharacterController"))
			//   cc:setMoveVelocity(vx, vz)   cc:jump(5)   cc:isGrounded()   cc:getVelocity() -> x, y, z
			lua->new_usertype<CharacterController>("CharacterControllerComponent", sol::no_constructor,
				sol::base_classes, sol::bases<IComponent>(),
				"setMoveVelocity", [](CharacterController &c, f32 x, f32 z) { c.SetMoveVelocity(Vec3(x, 0.f, z)); },
				"jump", &CharacterController::Jump,
				"teleport", [](CharacterController &c, f32 x, f32 y, f32 z) { c.Teleport(Vec3(x, y, z)); },
				"isGrounded", &CharacterController::IsGrounded,
				"getVelocity", [](CharacterController &c) { const Vec3 &v = c.GetVelocity(); return std::make_tuple(v.x, v.y, v.z); },
				"getGroundNormal", [](CharacterController &c) { const Vec3 &v = c.GetGroundNormal(); return std::make_tuple(v.x, v.y, v.z); },
				"setShape", &CharacterController::SetShape,
				"setStepHeight", &CharacterController::SetStepHeight,
				"setMaxSlope", &CharacterController::SetMaxSlope,
				"setGravity", &CharacterController::SetGravity,
				"setRunning", &CharacterController::SetRunning, "isRunning", &CharacterController::IsRunning);
			// A reflection probe (ReflectionProbe): what its box looks like from inside, for what shines
			// there. Captured when the scene loads; capture() sees it again (a door opened, a light switched).
			//   local p = obj:getComponent("ReflectionProbe")   p:capture()   p:setExtents(5, 3, 5)
			//   reflectionProbes.captureAll()
			lua->new_usertype<ReflectionProbe>("ReflectionProbeComponent", sol::no_constructor,
				sol::base_classes, sol::bases<IComponent>(),
				"capture", &ReflectionProbe::Capture,
				"isCaptured", &ReflectionProbe::IsCaptured,
				"setExtents", [](ReflectionProbe &p, f32 x, f32 y, f32 z) { p.SetExtents(Vec3(x, y, z)); },
				"getExtents", [](ReflectionProbe &p) { const Vec3 &e = p.GetExtents(); return std::make_tuple(e.x, e.y, e.z); },
				"setBlend", &ReflectionProbe::SetBlend, "getBlend", &ReflectionProbe::GetBlend);
			{
				sol::table rpt = lua->create_named_table("reflectionProbes");
				rpt.set_function("captureAll", []() { ReflectionProbes::CaptureAll(NULL); });
				rpt.set_function("count", []() { return ReflectionProbes::Count(); });
				rpt.set_function("pending", []() { return ReflectionProbes::Pending(); });
			}
			sol::table cct = lua->create_named_table("CharacterController");
			cct.set_function("attach", [](GameObject* go, IPhysics* phys) -> std::shared_ptr<CharacterController> {
				if (!go) return nullptr;
				const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
				for (size_t i = 0; i < comps.size(); i++)
					if (comps[i] && comps[i]->GetComponentType() == ComponentType::CharacterController) return std::static_pointer_cast<CharacterController>(comps[i]);
				std::shared_ptr<CharacterController> c = std::make_shared<CharacterController>(phys);
				c->SetRunning(true);
				go->Add(c);
				return c; });
			lua->set_function("getNavMesh", [](SceneGraph* sc) -> NavMesh* { return sc ? sc->GetNavMesh() : nullptr; });

			lua->new_usertype<Crowd>("Crowd",
				sol::constructors<Crowd(), Crowd(f32)>(),
				"add", [](Crowd &c, f32 x, f32 z, sol::optional<f32> r, sol::optional<f32> s) { return c.Add(Vec2(x, z), r.value_or(0.3f), s.value_or(1.5f)); },
				"remove", &Crowd::Remove,
				"has", &Crowd::Has,
				"count", &Crowd::Count,
				"setPreferredVelocity", [](Crowd &c, uint32 id, f32 vx, f32 vz) { c.SetPreferredVelocity(id, Vec2(vx, vz)); },
				"setPosition", [](Crowd &c, uint32 id, f32 x, f32 z) { c.SetPosition(id, Vec2(x, z)); },
				"setMaxSpeed", &Crowd::SetMaxSpeed,
				"setFixed", &Crowd::SetFixed,
				"setTimeHorizon", &Crowd::SetTimeHorizon,
				"update", &Crowd::Update,
				"getPosition", [](Crowd &c, uint32 id) { const Vec2 p = c.GetPosition(id); return std::make_tuple(p.x, p.y); },
				"getVelocity", [](Crowd &c, uint32 id) { const Vec2 v = c.GetVelocity(id); return std::make_tuple(v.x, v.y); });
			// Ways across a grid, found on the job system (PathGrid):
			//   local grid = pathGrid.new(rows)        rows: one string a row, a character a cell,
			//                                          '0' cannot be crossed, '1'..'9' what it costs
			//   local ticket = grid:request(si, sj, ti, tj [, maxCells])     cells count from 0
			//   grid:poll(ticket)     nil: not yet. false: no way. Otherwise the way, start to
			//                         goal, each cell as j * width + i
			//   grid:cancel(ticket)   grid:cost(i, j)   grid:width()   grid:height()
			lua->new_usertype<PathGrid>("PathGrid", sol::no_constructor,
				"request", [](PathGrid &g, const int32 si, const int32 sj, const int32 ti, const int32 tj, sol::optional<uint32> maxCells) {
					return g.Request(si, sj, ti, tj, maxCells.value_or(60000u));
				},
				"poll", [](PathGrid &g, const uint32 ticket, sol::this_state L) -> sol::object {
					std::vector<uint32> cells;
					const uint32 state = g.Poll(ticket, cells);
					if (state == PathGrid::Pending) return sol::make_object(L, sol::lua_nil);
					if (state != PathGrid::Found) return sol::make_object(L, false);
					sol::state_view lv(L);
					sol::table out = lv.create_table((int)cells.size(), 0);
					for (size_t i = 0; i < cells.size(); i++) out[i + 1] = cells[i];
					return out;
				},
				"cancel", &PathGrid::Cancel,
				"cost", &PathGrid::Cost,
				"width", &PathGrid::Width,
				"height", &PathGrid::Height
			);
			{
				sol::table pathGrid = lua->create_named_table("pathGrid");
				pathGrid.set_function("new", [](sol::table rows) -> std::shared_ptr<PathGrid> {
					const uint32 h = (uint32)rows.size();
					uint32 w = 0;
					std::vector<std::string> text(h);
					for (uint32 j = 0; j < h; j++)
					{
						sol::optional<std::string> row = rows[j + 1];
						if (row) text[j] = *row;
						w = std::max(w, (uint32)text[j].size());
					}
					std::vector<uint8> cost((size_t)w * h, 0);
					for (uint32 j = 0; j < h; j++)
					{
						const std::string &row = text[j];
						for (size_t i = 0; i < row.size(); i++)
							cost[(size_t)j * w + i] = (row[i] >= '0' && row[i] <= '9') ? (uint8)(row[i] - '0') : (uint8)0;
					}
					return std::make_shared<PathGrid>(w, h, cost);
				});
			}

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
			// terrain.erode(scene, x, z, radius, drops[, seed[, below]]): rain on the ground there (TerrainEditor::Erode)
			terrain.set_function("erode", [](SceneGraph* scene, const f32 x, const f32 z, const f32 radius, const uint32 drops,
				sol::optional<uint32> seed, sol::optional<f32> below) -> uint32 {
				return editor.Erode(scene, x, z, radius, drops, seed.value_or(1u), below.value_or(-1e9f));
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
#ifdef EMSCRIPTEN
		// A page's files are gone when it is closed: in a browser the same
		// two calls keep each name in localStorage instead, under the
		// folder the desktop would have used - so one game's are not
		// another's, and a script is the same on both.
		const std::string prefix = "pyros:" + directory;
		store.set_function("read", [prefix, safe](const std::string &name, sol::this_state ts) -> sol::object {
			sol::state_view L(ts);
			if (!safe(name)) return sol::make_object(L, sol::lua_nil);
			char* text = (char*)EM_ASM_PTR({
				var v = null;
				try { v = localStorage.getItem(UTF8ToString($0)); } catch (e) {}
				return v === null ? 0 : stringToNewUTF8(v);
			}, (prefix + name).c_str());
			if (!text) return sol::make_object(L, sol::lua_nil);
			const std::string out(text);
			free(text);
			return sol::make_object(L, out);
		});
		store.set_function("write", [prefix, safe](const std::string &name, const std::string &text) {
			if (!safe(name)) return false;
			// (private browsing, or storage full: it throws, and says so)
			return EM_ASM_INT({
				try { localStorage.setItem(UTF8ToString($0), UTF8ToString($1)); return 1; } catch (e) { return 0; }
			}, (prefix + name).c_str(), text.c_str()) != 0;
		});
		store.set_function("exists", [prefix, safe](const std::string &name) {
			if (!safe(name)) return false;
			return EM_ASM_INT({ try { return localStorage.getItem(UTF8ToString($0)) === null ? 0 : 1; } catch (e) { return 0; } }, (prefix + name).c_str()) != 0;
		});
		store.set_function("remove", [prefix, safe](const std::string &name) {
			if (!safe(name)) return false;
			return EM_ASM_INT({ try { localStorage.removeItem(UTF8ToString($0)); return 1; } catch (e) { return 0; } }, (prefix + name).c_str()) != 0;
		});
		store.set_function("list", [prefix](sol::this_state ts) {
			sol::state_view L(ts);
			sol::table t = L.create_table();
			char* names = (char*)EM_ASM_PTR({
				// (no bare commas in here: EM_ASM is a macro, and they split its arguments)
				var p = UTF8ToString($0);
				var out = [];
				try { for (var i = 0; i < localStorage.length; i++) { var k = localStorage.key(i); if (k.indexOf(p) === 0) out.push(k.substring(p.length)); } } catch (e) {}
				return stringToNewUTF8(out.sort().join("\n"));
			}, prefix.c_str());
			if (names)
			{
				std::stringstream ss(names); std::string n; int i = 1;
				while (std::getline(ss, n)) if (!n.empty()) t[i++] = n;
				free(names);
			}
			return t;
		});
		return;
#endif
		store.set_function("read", [dir, safe](const std::string &name, sol::this_state ts) -> sol::object {
			sol::state_view L(ts);
			if (!safe(name)) return sol::make_object(L, sol::lua_nil);
			std::ifstream in((dir / name).string().c_str(), std::ios::binary);
			// (the one before, if a write was cut off between the two renames)
			if (!in.is_open()) in.open((dir / (name + ".bak")).string().c_str(), std::ios::binary);
			if (!in.is_open()) return sol::make_object(L, sol::lua_nil);
			std::stringstream ss;
			ss << in.rdbuf();
			return sol::make_object(L, ss.str());
		});
		// Written whole or not at all: to a ".tmp" beside it, flushed, then put in its place - a
		// crash or a power cut mid-save left half a file, and the game's save was gone. The one
		// it replaces is kept as ".bak", and read() falls back to it if the file is missing.
		store.set_function("write", [dir, safe](const std::string &name, const std::string &text) {
			if (!safe(name)) return false;
			std::error_code ec;
			std::filesystem::create_directories(dir, ec);
			const std::filesystem::path target = dir / name, tmp = dir / (name + ".tmp"), bak = dir / (name + ".bak");
			{
				std::ofstream out(tmp.string().c_str(), std::ios::binary | std::ios::trunc);
				if (!out.is_open()) return false;
				out << text;
				out.flush();
				if (!out.good()) { out.close(); std::filesystem::remove(tmp, ec); return false; }
			}
			if (std::filesystem::exists(target, ec)) { std::filesystem::remove(bak, ec); std::filesystem::rename(target, bak, ec); }
			std::filesystem::rename(tmp, target, ec);
			if (ec) { std::filesystem::remove(tmp, ec); return false; }
			return true;
		});
		store.set_function("exists", [dir, safe](const std::string &name) {
			std::error_code ec;
			return safe(name) && std::filesystem::exists(dir / name, ec);
		});
		store.set_function("remove", [dir, safe](const std::string &name) {
			if (!safe(name)) return false;
			std::error_code ec;
			const bool had = std::filesystem::remove(dir / name, ec);
			std::filesystem::remove(dir / (name + ".bak"), ec);
			return had;
		});
		// Every name written, sorted (not the .tmp/.bak beside them).
		store.set_function("list", [dir](sol::this_state ts) {
			sol::state_view L(ts);
			sol::table t = L.create_table();
			std::vector<std::string> names;
			std::error_code ec;
			for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
			{
				const std::string n = it->path().filename().string();
				if (!it->is_regular_file(ec)) continue;
				if (n.size() > 4 && (n.compare(n.size() - 4, 4, ".tmp") == 0 || n.compare(n.size() - 4, 4, ".bak") == 0)) continue;
				names.push_back(n);
			}
			std::sort(names.begin(), names.end());
			for (size_t i = 0; i < names.size(); i++) t[i + 1] = names[i];
			return t;
		});
	}

} // namespace p3d

#endif
