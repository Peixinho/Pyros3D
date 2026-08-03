-- ****************************************************************
-- NEON PULSE - materials, static arena geometry and lighting.
--
-- Nothing here moves during play except the two rim lights, so it is
-- all built once in build() and then left alone.
--
-- Lifetime note that applies to this whole game: every userdata this
-- file creates (materials, meshes, GameObjects, components) is stored
-- in a table that stays reachable for the lifetime of the process.
-- sol owns these objects, so dropping the last Lua reference would
-- collect something the C++ SceneGraph still points at.
-- ****************************************************************

local C = import("config")

local A = { keep = {}, mat = {} }

-- Anything with no other reason to be referenced goes here purely so the
-- Lua GC can never reach it.
local function keep(obj)
	A.keep[#A.keep + 1] = obj
	return obj
end

-- ****************************** Materials ******************************

A.deferred = false   -- set by A.build() from C.renderer

-- Lit: shaded by the directional fill and, more importantly, by the point
-- light riding on the ball - that moving pool of light across the back
-- panel and the brick faces is most of what sells the depth here.
--
-- On the deferred path the same material additionally writes the G-buffer.
-- Diffuse (not PBR) is kept deliberately: the deferred pass derives
-- roughness from uShininess for non-PBR materials, so the per-material
-- shininess below still controls the highlight, and the colours stay the
-- literal palette values rather than becoming metal/roughness inputs.
function A.lit(color, shininess)
	local usage = ShaderUsage.Color + ShaderUsage.Diffuse
	if A.deferred then usage = usage + ShaderUsage.DeferredRenderer_Gbuffer end
	local m = GenericShaderMaterial.new(usage)
	m:setColor(color)
	m:setSpecular(Vec4.new(0.55, 0.75, 0.95, 1))
	m:setShininess(shininess or 26)
	return keep(m)
end

-- Unlit: flat full-brightness colour, no lighting term at all. This is the
-- "neon tube" material - trim strips, the ball core, the grid.
--
-- These deliberately do NOT write the G-buffer. A deferred G-buffer has
-- nowhere to put "this surface emits light and must not be shaded", so an
-- unlit material pushed through it would come back shaded like everything
-- else and the neon would go dark. Flagging them transparent instead puts
-- them in DeferredRenderer's forward pass, which runs after the lighting
-- composite and depth-tests against the G-buffer depth.
--
-- The catch: IMaterial::IsDepthWritting() reports false for *any*
-- transparent material unless depth testing was explicitly enabled, and
-- the translucent bucket is sorted back-to-front by object origin - so
-- these solid, fully-opaque neon pieces stopped writing depth the moment
-- they were flagged, and started resolving against each other by sort
-- order instead of per pixel. That is what put the grid on top of the ball
-- trail. EnableDepthTest() sets the forceDepthWrite flag that overrides
-- the transparent default, which is exactly right here: nothing about
-- these is actually translucent, they only ride the transparent pass to
-- escape the G-buffer.
function A.unlit(color)
	local m = GenericShaderMaterial.new(ShaderUsage.Color)
	m:setColor(color)
	if A.deferred then
		m:setTransparencyFlag(true)
		m:enableDepthTest(0)
	end
	return keep(m)
end

-- ******************************* Helpers *******************************

-- Cube's constructor arguments are named width/height/depth but are used
-- as HALF-extents (Cube.cpp builds its corners at +/-width, not
-- +/-width/2), so Cube.new(13,6,6) is a 26x12x12 box. Every size in this
-- game is a real full size, so all of them go through here.
function A.cube(w, h, d)
	return Cube.new(w * 0.5, h * 0.5, d * 0.5)
end

-- A box in the scene. Returns the GameObject so callers can move it.
function A.box(w, h, d, x, y, z, material)
	local mesh = keep(A.cube(w, h, d))
	local go = GameObject.new()
	local rc = RenderingComponent.new(mesh, material)
	go:addComponent(rc)
	go:setPosition(Vec3.new(x, y, z))
	keep(go); keep(rc)
	G.scene:add(go)
	return go
end

-- ******************************** Build ********************************

function A.build()
	local a = C.arena
	local col = C.color

	A.deferred = (C.renderer == "deferred")
	A.lightGain = A.deferred and C.deferredLightGain or 1.0

	A.mat.panel  = A.lit(col.panel, 8)
	A.mat.wall   = A.lit(col.wall, 40)
	A.mat.trim   = A.unlit(col.trim)
	A.mat.grid   = A.unlit(col.grid)

	G.renderer:setBackground(col.background)
	-- Kept low on purpose: the arena should read as a dark room lit by the
	-- ball, not a flat-lit diagram. Anything above ~0.35 washes that out.
	G.renderer:setGlobalLight(Vec4.new(0.24, 0.26, 0.36, 1))

	-- Back panel ---------------------------------------------------------
	local panelW = a.halfW * 2 + a.wallT * 2
	local panelH = a.topY + 46
	A.box(panelW, panelH, 4, 0, panelH * 0.5 - 24, a.panelZ, A.mat.panel)

	-- Grid on the panel. Thin unlit bars just in front of it; they pick up
	-- no lighting, so they stay a constant faint glow while the ball's
	-- light sweeps over the panel behind them.
	local gz = a.panelZ + 2.3
	local gridTop, gridBottom = a.topY + 4, -24
	for i = 0, 12 do
		local x = -a.halfW + (a.halfW * 2) * (i / 12)
		A.box(0.5, gridTop - gridBottom, 0.5, x, (gridTop + gridBottom) * 0.5, gz, A.mat.grid)
	end
	for i = 0, 9 do
		local y = gridBottom + (gridTop - gridBottom) * (i / 9)
		A.box(a.halfW * 2, 0.5, 0.5, 0, y, gz, A.mat.grid)
	end

	-- Walls --------------------------------------------------------------
	local wallX = a.halfW + a.wallT * 0.5
	local wallH = a.topY + 32
	local wallCY = (a.topY - 20) * 0.5 + 4
	A.box(a.wallT, wallH, a.depth, -wallX, wallCY, a.playZ, A.mat.wall)
	A.box(a.wallT, wallH, a.depth,  wallX, wallCY, a.playZ, A.mat.wall)
	A.box(a.halfW * 2 + a.wallT * 2, a.wallT, a.depth, 0, a.topY + a.wallT * 0.5, a.playZ, A.mat.wall)

	-- Neon trim on the inner lip of each wall.
	local trimZ = a.playZ + a.depth * 0.5 + 0.4
	A.box(1.4, wallH, 1.4, -a.halfW + 0.7, wallCY, trimZ, A.mat.trim)
	A.box(1.4, wallH, 1.4,  a.halfW - 0.7, wallCY, trimZ, A.mat.trim)
	A.box(a.halfW * 2, 1.4, 1.4, 0, a.topY - 0.7, trimZ, A.mat.trim)

	-- A dimmer strip along the dead line, so the bottom of the arena reads
	-- as an edge rather than as the screen running out.
	A.box(a.halfW * 2, 0.8, 0.8, 0, a.deadY + 2, trimZ, A.mat.grid)

	-- Lighting -----------------------------------------------------------
	-- One directional key for overall form. Only faintly cool: a strongly
	-- blue key multiplies into every material and drags the whole palette
	-- toward one colour - the six row colours stop being distinguishable
	-- and near-white armoured bricks read as just another blue.
	local keyObj = GameObject.new()
	local key = DirectionalLight.new(Vec4.new(0.82, 0.88, 1.00, 1), Vec3.new(-0.35, -0.75, -0.55))
	key:setLightIntensity(1.0 * A.lightGain)
	keyObj:addComponent(key)
	G.scene:add(keyObj)
	keep(keyObj); keep(key)
	A.keyLight = key

	-- ...and two violet rim lights up in the corners that breathe slowly,
	-- which keeps the arena from ever looking completely static.
	A.rims = {}
	for i = 1, 2 do
		local obj = GameObject.new()
		local light = PointLight.new(Vec4.new(0.42, 0.24, 0.85, 1), 150)
		obj:addComponent(light)
		obj:setPosition(Vec3.new(i == 1 and -a.halfW or a.halfW, a.topY - 10, 40))
		G.scene:add(obj)
		keep(obj); keep(light)
		A.rims[i] = light
	end
end

-- Called every frame. The only animated thing in the static arena.
function A.update(time)
	for i, light in ipairs(A.rims) do
		local phase = time * 0.9 + (i - 1) * 2.1
		light:setLightIntensity((0.55 + 0.30 * math.sin(phase)) * A.lightGain)
	end
end

return A
