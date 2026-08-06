-- ****************************************************************
--
--    N E O N   P U L S E        a Pyros3D game, written in Lua
--
--    Break the wall. The ball carries the only real light in the
--    arena, so the room is lit by how well you are playing.
--
--    Arrows / A,D / mouse   move the paddle
--    Space / click          launch, and restart after a game over
--
--    The C++ side of this game (examples/NeonPulse) is a window, a
--    sol::state and a call to update(). Everything else is here.
--
--    DemoLauncher hosts the same files through a scene LuaComponent
--    (demos/scripts/neonpulse.lua): when the host already exposes
--    scene/renderer/camera/projection globals, this script reuses them
--    instead of building a second stack, and skips owning the frame.
--
-- ****************************************************************

-- ****************************** Module loading *************************
--
-- `package` is not among the libraries GenerateBindings() opens, so
-- require() does not exist. loadfile() does, and GAME_PATH is handed to
-- us by the host - that is enough for a small module system with the one
-- property that matters: each file runs once and returns its table.

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

-- Shared engine-level handles. Deliberately a single named global rather
-- than a scatter of loose ones, so it is obvious at a glance what the
-- modules share.
G = {}

-- ******************************** Startup ******************************

local C, Arena, E, Game, Audio

function init()
	C = import("config")

	-- Hosted by DemoLauncher (or any host that already owns the stack):
	-- reuse its Scene/DeferredRenderer/shell camera/projection. Standalone
	-- NeonPulse.cpp leaves these globals unset and we build our own.
	local hosted = (scene ~= nil and renderer ~= nil and camera ~= nil and projection ~= nil)
	G._hosted = hosted

	if hosted then
		-- Proxy Scene:add so destroy() can Scene:remove exactly what we
		-- created, without touching the host's shell or other demos'
		-- leftover state (UnloadScene only knows about JSON roots).
		local realScene = scene
		G._scene = realScene
		G.owned = {}
		G.scene = {
			add = function(_, go)
				G.owned[#G.owned + 1] = go
				realScene:add(go)
			end,
			remove = function(_, go) realScene:remove(go) end,
			update = function(_, t) realScene:update(t) end,
		}
		G.renderer = renderer
		G.camera = camera
		G.projection = projection
		-- JSON camera_none leaves rotation at identity; Neon needs the
		-- designed lookAt or the arena sits off-frustum / empty-looking.
		if G.camera and C.camera then
			G.camera:setPosition(C.camera.pos)
			G.camera:lookAtVec(C.camera.lookAt)
			if G.camera.refreshTransformation then G.camera:refreshTransformation() end
		end
	else
		G.scene = Scene.new()
		G.projection = Projection.new()

		if C.renderer == "deferred" then
			buildDeferredRenderer(SCREEN_W, SCREEN_H)
		else
			G.renderer = ForwardRenderer.new(SCREEN_W, SCREEN_H)
		end

		G.camera = GameObject.new()
		G.camera:setPosition(C.camera.pos)
		G.camera:lookAtVec(C.camera.lookAt)
		G.scene:add(G.camera)
	end

	-- One soft sprite shared by the ball trails and every impact burst.
	G.sprite = Texture.new()
	G.sprite:loadTexture(ASSETS_PATH .. "smoke.png", TextureType.Texture, true, 0)

	Arena = import("arena")
	E = import("entities")
	Audio = import("audio")
	Game = import("game")

	Arena.build()
	-- Before E.build(): the entities' sounds are looked up through this.
	Audio.build()
	E.build()
	Game.bindInput()

	-- Fill the wall before the title screen so there is something to look
	-- at while the banner is up; Game.start() reloads it on launch.
	E.loadLevel(1)
	Game.setState("title")

	fitProjection(SCREEN_W, SCREEN_H)
end

-- ***************************** Deferred setup **************************
--
-- The G-buffer DeferredRenderer expects: depth first (its own forward pass
-- copies attachment 0 as depth), then albedo / specular / normal /
-- metallic-roughness as colour attachments 0..3. Normals need the float
-- format - packing them into RGBA8 quantises them badly enough to band
-- the falloff of the ball lights across the flat back panel.
function buildDeferredRenderer(width, height)
	-- All seven arguments spelled out: the binding exposes the single
	-- widest C++ overload, so its default arguments (mipmapping, level,
	-- msaa) are not optional from Lua.
	local function target(dataType)
		local t = Texture.new()
		t:createEmptyTexture(TextureType.Texture, dataType, width, height, false, 0, 0)
		t:setRepeat(TextureRepeat.ClampToEdge, TextureRepeat.ClampToEdge, TextureRepeat.ClampToEdge)
		return t
	end

	G.gbuffer = {
		depth    = target(TextureDataType.DepthComponent),
		albedo   = target(TextureDataType.RGBA),
		specular = target(TextureDataType.RGBA),
		normal   = target(TextureDataType.RGBA32F),
		matrough = target(TextureDataType.RGBA),
	}

	G.gbufferFBO = FrameBuffer.new()
	G.gbufferFBO:init(FrameBufferAttachmentFormat.Depth_Attachment, TextureType.Texture, G.gbuffer.depth)
	G.gbufferFBO:addAttach(FrameBufferAttachmentFormat.Color_Attachment0, TextureType.Texture, G.gbuffer.albedo)
	G.gbufferFBO:addAttach(FrameBufferAttachmentFormat.Color_Attachment1, TextureType.Texture, G.gbuffer.specular)
	G.gbufferFBO:addAttach(FrameBufferAttachmentFormat.Color_Attachment2, TextureType.Texture, G.gbuffer.normal)
	G.gbufferFBO:addAttach(FrameBufferAttachmentFormat.Color_Attachment3, TextureType.Texture, G.gbuffer.matrough)

	G.renderer = DeferredRenderer.new(width, height, G.gbufferFBO)
end

-- The arena is a fixed-size rectangle, so a narrow window would crop it.
-- Rather than letting that happen, widen the field of view until the
-- arena's full width fits again.
function fitProjection(width, height)
	local aspect = width / height
	local cam = C.camera
	local fov = cam.fov
	local halfW = math.tan(math.rad(fov * 0.5)) * cam.planeDist * aspect
	if halfW < cam.minHalfW then
		fov = math.deg(math.atan(cam.minHalfW / (cam.planeDist * aspect))) * 2
	end
	G.projection:perspective(fov, aspect, cam.near, cam.far)
end

-- ******************************* Game loop *****************************

function update(time, dt)
	-- A hitch (window drag, first-frame compile stall) must not teleport
	-- the ball through the wall.
	if dt > 0.05 then dt = 0.05 end

	Game.update(dt, time)

	if G._hosted then
		-- Host SceneGraph::Update is already mid-pass (we are a
		-- LuaComponent on it) - do not recurse into scene:update.
		-- Refresh the shell camera so the audio listener (and the host's
		-- post-Update RefreshTransformation / RenderScene) see shake.
		if G.camera and G.camera.refreshTransformation then
			G.camera:refreshTransformation()
		end
		E.fireQueuedBursts()
		Audio.update(dt)
		return
	end

	G.scene:update(time)

	-- Burst emitters read their owner's *world* position, which the scene
	-- update above is what refreshes - so queued bursts are fired here,
	-- after it, and land where the brick actually was.
	E.fireQueuedBursts()

	-- Same reason: the listener rides the camera, whose world transform the
	-- scene update is what refreshes.
	Audio.update(dt)

	G.renderer:preRender(G.camera, G.scene)
	G.renderer:renderScene(G.projection, G.camera, G.scene)
end

function resize(width, height)
	if not C then return end
	fitProjection(width, height)
	if G._hosted then
		-- Host owns renderer + G-buffer resize (DemoLauncher::OnResize).
		return
	end
	G.renderer:resize(width, height)
	-- The renderer resizes its own internal targets, but the G-buffer
	-- textures are ours - miss these and the deferred pass keeps sampling
	-- a stale, wrongly-sized buffer after every window change.
	if G.gbuffer then
		-- Trailing mip level is not optional from Lua (see the seven-argument
		-- createEmptyTexture call above for the same reason).
		for _, tex in pairs(G.gbuffer) do tex:resize(width, height, 0) end
	end
end

-- Standalone: optional. Hosted: required so the next demo does not inherit
-- Neon Pulse's dynamically-added GameObjects / Input bridge / lights.
function destroy()
	if G then
		if G._hosted and G._scene and G.owned then
			for _, go in ipairs(G.owned) do
				pcall(function() G._scene:remove(go) end)
			end
			-- Restore launcher defaults Arena.build() overwrote.
			if G.renderer then
				G.renderer:setGlobalLight(Vec4.new(0.12, 0.12, 0.14, 1))
				G.renderer:unsetBackground()
			end
		end
		-- Drop Input.new() so ~LuaInputBridge unregisters from InputManager.
		G.input = nil
		G.scene = nil
		G.renderer = nil
		G.camera = nil
		G.gbuffer = nil
		G.gbufferFBO = nil
		G.projection = nil
		G.sprite = nil
		G.owned = nil
		G._scene = nil
	end
	for k in pairs(loaded) do loaded[k] = nil end
	G = nil
	C = nil
	Arena = nil
	E = nil
	Game = nil
	Audio = nil
end
