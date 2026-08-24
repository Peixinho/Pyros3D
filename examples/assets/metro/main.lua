-- ****************************************************************
-- METRO - module loader and the game's lifecycle.
--
-- Runs hosted by the DemoLauncher: `scene`, `camera`, `projection` and
-- `renderer` are already built by the host and RenderHost owns the
-- deferred stack, so there is no renderer construction here. This file
-- only wires the modules together and exposes the init/update/draw/
-- destroy that the scene component drives.
-- ****************************************************************

-- The editor binds `echo` into its Lua state (SceneEditor.cpp); the
-- DemoLauncher does not, and every module here logs through it. Provide
-- it when the host has not, so the same assets run under either.
if echo == nil then
	function echo(msg) print(tostring(msg)) end
end

local loaded = {}

function import(name)
	local mod = loaded[name]
	if mod == nil then
		local chunk, err = loadfile(GAME_PATH .. name .. ".lua")
		if not chunk then error("cannot load module '" .. name .. "': " .. tostring(err)) end
		mod = chunk()
		if mod == nil then error("module '" .. name .. "' returned nothing") end
		loaded[name] = mod
	end
	return mod
end

G = {}

local C, Level, P, W, E, FX, Audio, Game, HUD

-- ================================= Init ================================

function init()
	echo("[METRO] init")

	if scene == nil then error("METRO: no host scene - this demo runs in the DemoLauncher") end
	if camera == nil then error("METRO: no host camera (camera_none.lua should set it)") end

	C = import("config")

	-- Everything this game adds is tracked so destroy() can hand the
	-- scene back exactly as it found it - SwitchDemo reuses the graph.
	local realScene = scene
	G._scene = realScene
	G.owned = {}
	G.scene = {
		add = function(_, go) G.owned[#G.owned + 1] = go; realScene:add(go) end,
		remove = function(_, go) realScene:remove(go) end,
	}
	G.camera = camera
	G.projection = projection

	-- The particle sprite every FX emitter shares.
	G.sprite = Texture.new()
	G.sprite:loadTexture(ASSETS_PATH .. "smoke.png", TextureType.Texture, true, 0)

	Level = import("level")
	FX = import("fx")
	Audio = import("audio")
	E = import("enemies")
	W = import("weapon")
	P = import("player")
	Game = import("game")
	HUD = import("hud")

	local function stage(name, fn, ...)
		local ok, msg = pcall(fn, ...)
		if not ok then error("METRO: " .. name .. " failed: " .. tostring(msg)) end
	end

	stage("level", Level.build)
	stage("fx", FX.build)
	stage("audio", Audio.build)
	stage("enemies", E.build)
	stage("weapon", W.build)
	stage("player", P.build, G.camera)
	stage("game", Game.build, P, W, E, FX, Audio)

	bindInput()

	G.time = 0
	echo("[METRO] ready")
end

-- ================================ Input ================================

function bindInput()
	G.input = Input.new()

	P.bindInput(G.input)
	W.bindInput(G.input, P)

	-- The launcher's own camera scripts own Tab elsewhere; here it is the
	-- capture toggle, and capture is what turns the demo into a game.
	G.input:onKeyPressed(Key.Tab, function()
		P.setCaptured(not P.captured)
	end)
	G.input:onKeyPressed(Key.Escape, function()
		if P.captured then P.setCaptured(false) end
	end)
	G.input:onKeyPressed(Key.Return, function()
		if Game.state == "dead" then Game.restart(G.time) end
	end)

	G.input:onMouseMoved(function(x, y)
		P.onMouseMoved(x, y)
	end)

	-- camera_none.lua parks this false for the demos that own their view;
	-- METRO wants the mouse, so it takes it back.
	allowMouseCapture = true
	P.captured = false
end

-- ================================ Frame ================================

function update(time, dt)
	-- A long stall (a demo switch, a shader compile) must not teleport
	-- the player through a wall or let every enemy land a free hit.
	if dt > 0.05 then dt = 0.05 end
	G.time = time

	Level.update(time, dt)

	if P.captured then
		P.update(dt, time)
		W.update(dt, time, P)
		E.update(dt, time, P)
		Game.update(dt, time)
	else
		-- Paused: the station keeps breathing and the gun stays in shot,
		-- but nothing else moves. The viewmodel is posed here too so it
		-- is on screen before the player has ever pressed TAB.
		P.updateFlashlight(dt, time)
		W.updateViewmodel(dt, time, P)
	end

	-- Hit flash decays here rather than in the HUD, so it is frame-rate
	-- independent and keeps ticking while the game is paused.
	if P.hitFlash and P.hitFlash > 0 then
		P.hitFlash = math.max(0, P.hitFlash - dt * 2.2)
	end

	-- Aiming narrows the view. RenderHost only writes the projection on
	-- setup and resize, so driving it per-frame here is safe.
	local w, h = getWindowSize()
	if h > 0 then
		G.projection:perspective(W.fov(), w / h, C.camera.near, C.camera.far)
	end

	Audio.update(dt, G.camera)

	-- Bursts queued this frame fire only now, after the scene update has
	-- refreshed the emitters' world transforms - playing them where they
	-- were requested would emit at each pool slot's previous position.
	FX.fireQueued()
end

function drawOverlay()
	if not HUD then return end
	HUD.draw(P, W, Game, G.time or 0)
end

function resize(width, height)
	if not C then return end
	G.projection:perspective(W.fov(), width / height, C.camera.near, C.camera.far)
end

-- ================================ Teardown =============================

function destroy()
	echo("[METRO] destroy")

	if P then pcall(P.setCaptured, false) end
	if Audio then pcall(Audio.destroy) end
	if Game then pcall(Game.destroy) end
	if W then pcall(W.destroy) end
	if E then pcall(E.destroy) end
	if FX then pcall(FX.destroy) end
	if P then pcall(P.destroy) end
	if Level then pcall(Level.destroy) end

	-- Hand the SceneGraph back as it was found.
	if G and G._scene and G.owned then
		for _, go in ipairs(G.owned) do
			pcall(function() G._scene:remove(go) end)
		end
	end

	if G then
		G.input = nil
		G.scene = nil
		G.camera = nil
		G.projection = nil
		G.sprite = nil
		G.owned = nil
		G._scene = nil
		G.audio = nil
	end

	for k in pairs(loaded) do loaded[k] = nil end
	G = nil
	C, Level, P, W, E, FX, Audio, Game, HUD = nil, nil, nil, nil, nil, nil, nil, nil, nil
end
