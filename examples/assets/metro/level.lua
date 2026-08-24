-- ****************************************************************
-- METRO - the station: geometry, collision volumes and lighting.
--
-- Built once in L.build() and then static, apart from the failing
-- ceiling lamps (L.update flickers them) and the two tunnel glows.
--
-- Every box added here is both a drawn Cube and, unless `solid` is
-- false, an entry in L.colliders - the axis-aligned world the player
-- and the enemies actually move against. The station is a corridor of
-- boxes on purpose: no pathfinding or physics bodies are needed, and
-- shooting is a ray against this same list.
--
-- Lifetime: every userdata goes into L.keep, because the SceneGraph
-- holds raw pointers and sol owns these objects - anything only
-- reachable from a local would be collected out from under the scene.
-- ****************************************************************

local C = import("config")

local L = { keep = {}, mat = {}, colliders = {}, lamps = {}, spawns = {} }

local function keep(obj)
	L.keep[#L.keep + 1] = obj
	return obj
end

local function vec4(c, a)
	return Vec4.new(c[1], c[2], c[3], a or 1)
end

-- ****************************** Materials ******************************
--
-- Deferred only: every lit surface writes the G-buffer, so the station
-- can carry far more lights than the forward path's MAX_LIGHTS of 4.

local LIT_USAGE = nil

function L.pbr(color, roughness, metallic)
	local m = GenericShaderMaterial.new(LIT_USAGE)
	m:setColor(vec4(color))
	m:setRoughness(roughness or 0.85)
	m:setMetallic(metallic or 0.0)
	return keep(m)
end

-- Unlit surfaces for anything that is supposed to *be* a light source
-- (lamp housings, signage, the emergency strips). Colour only, no
-- lighting, so they stay at full brightness in a pitch-dark station.
function L.emissive(color)
	local m = GenericShaderMaterial.new(ShaderUsage.Color)
	m:setColor(vec4(color))
	m:setTransparencyFlag(true)
	m:enableDepthTest(0)
	return keep(m)
end

-- ******************************* Building ******************************

-- Cube's constructor takes half-extents; everything below is written in
-- full sizes, which is how the station is actually measured.
local function cube(w, h, d)
	return keep(Cube.new(w * 0.5, h * 0.5, d * 0.5))
end

function L.addCollider(cx, cy, cz, w, h, d)
	L.colliders[#L.colliders + 1] = {
		x0 = cx - w * 0.5, x1 = cx + w * 0.5,
		y0 = cy - h * 0.5, y1 = cy + h * 0.5,
		z0 = cz - d * 0.5, z1 = cz + d * 0.5,
	}
end

-- solid defaults to true. Static GameObjects are transformed once when
-- the SceneGraph takes them, so setPosition must happen before add().
function L.box(cx, cy, cz, w, h, d, mat, solid)
	local go = GameObject.new(true)
	local rc = RenderingComponent.new(cube(w, h, d), mat)
	go:addComponent(rc)
	go:setPosition(Vec3.new(cx, cy, cz))
	G.scene:add(go)
	keep(go); keep(rc)
	if solid ~= false then L.addCollider(cx, cy, cz, w, h, d) end
	return go
end

-- ******************************** Lights *******************************

function L.pointLight(x, y, z, color, radius, intensity)
	local go = GameObject.new()
	local light = PointLight.new(vec4(color), radius)
	light:setLightIntensity(intensity)
	go:addComponent(light)
	go:setPosition(Vec3.new(x, y, z))
	G.scene:add(go)
	keep(go); keep(light)
	return light, go
end

-- ========================== The station shell ==========================

function L.buildShell()
	local s = C.station
	local col = C.color

	local len      = s.halfLen * 2
	local tunnelTo = s.tunnelMouth + s.tunnelLen
	local bodyTop  = s.trenchFloor + s.tunnelHalfH * 2   -- top of the tunnel bore

	L.mat.concrete   = L.pbr(col.concrete, 0.92, 0.0)
	L.mat.concreteDk = L.pbr(col.concreteDk, 0.95, 0.0)
	L.mat.tile       = L.pbr(col.tile, 0.42, 0.0)        -- glazed, catches highlights
	L.mat.tileGrime  = L.pbr(col.tileGrime, 0.78, 0.0)
	L.mat.steel      = L.pbr(col.steel, 0.38, 0.85)
	L.mat.rail       = L.pbr(col.rail, 0.28, 0.95)       -- polished by the trains
	L.mat.sleeper    = L.pbr(col.sleeper, 0.95, 0.0)
	L.mat.ballast    = L.pbr(col.ballast, 0.98, 0.0)
	L.mat.bench      = L.pbr(col.bench, 0.7, 0.0)
	L.mat.edge       = L.emissive(col.platformEdge)
	L.mat.lampBody   = L.pbr(col.steel, 0.5, 0.7)
	L.mat.lampGlass  = L.emissive(col.lampWarm)
	L.mat.lampDead   = L.pbr({ 0.20, 0.20, 0.20 }, 0.6, 0.0)
	L.mat.emergency  = L.emissive(col.emergency)
	L.mat.sign       = L.pbr(col.sign, 0.55, 0.0)

	-- Island platform: one solid block from the rail bed up to the deck.
	local deckH = s.platformTop - s.trenchFloor
	L.box(0, s.trenchFloor + deckH * 0.5, 0, len, deckH, s.platformHalf * 2, L.mat.tileGrime)

	-- The deck surface itself, a thin tiled slab sitting on that block so
	-- the walking surface is a cleaner material than the platform face.
	-- Raised 10mm clear of the platform block's own top face. Sharing that
	-- plane exactly is what streaked the floor with z-fighting.
	L.box(0, s.platformTop - 0.02, 0, len, 0.06, s.platformHalf * 2 - 0.5, L.mat.tile, false)

	-- Painted safety line down both edges.
	for _, sz in ipairs({ -1, 1 }) do
		L.box(0, s.platformTop + 0.035, sz * (s.platformHalf - 0.35), len, 0.02, 0.30, L.mat.edge, false)
	end

	-- Track trenches: ballast bed either side, running the full length of
	-- the station and on through both tunnels.
	local trenchW = s.trenchOut - s.trenchIn
	local trenchCz = (s.trenchIn + s.trenchOut) * 0.5
	for _, sz in ipairs({ -1, 1 }) do
		L.box(0, s.trenchFloor - 0.25, sz * trenchCz, tunnelTo * 2, 0.5, trenchW, L.mat.ballast)
	end

	-- Side walls, station and tunnels alike.
	for _, sz in ipairs({ -1, 1 }) do
		local h = s.ceiling - s.trenchFloor
		L.box(0, s.trenchFloor + h * 0.5, sz * (s.trenchOut + 0.3), tunnelTo * 2, h, 0.6, L.mat.concrete)
	end

	-- Ceiling over the station hall.
	L.box(0, s.ceiling + 0.25, 0, len, 0.5, s.trenchOut * 2, L.mat.concreteDk)

	-- Wall tiling: a band of glazed tile from the deck up to head height
	-- on the inside face of both side walls. Non-solid, it sits just
	-- inside the wall it decorates.
	for _, sz in ipairs({ -1, 1 }) do
		L.box(0, s.platformTop + 1.3, sz * (s.trenchOut - 0.06), len, 2.6, 0.08, L.mat.tile, false)
	end

	-- ---- Both ends: the platform stops, the tracks carry on ----
	for _, sx in ipairs({ -1, 1 }) do
		local x = sx * s.tunnelMouth

		-- The end wall above and behind the platform.
		local h = s.ceiling - s.trenchFloor
		L.box(x + sx * 0.3, s.trenchFloor + h * 0.5, 0, 0.6, h, s.platformHalf * 2, L.mat.concrete)

		-- Lintels over each tunnel bore.
		for _, sz in ipairs({ -1, 1 }) do
			local lh = s.ceiling - bodyTop
			L.box(x + sx * 0.3, bodyTop + lh * 0.5, sz * trenchCz, 0.6, lh, trenchW, L.mat.concreteDk)
		end

		-- Dividing wall between the two bores, past the platform. It
		-- starts at the far face of the end wall, not at the mouth: the
		-- end wall already occupies the first 0.6 m, and overlapping the
		-- two boxes leaves coplanar faces that z-fight down the tunnel.
		local from = s.tunnelMouth + 0.6
		local span = tunnelTo - from
		local dx = (from + tunnelTo) * 0.5
		L.box(sx * dx, s.trenchFloor + h * 0.5, 0, span, h, s.platformHalf * 2, L.mat.concreteDk)

		-- Tunnel roofs, clear of the lintels for the same reason.
		for _, sz in ipairs({ -1, 1 }) do
			L.box(sx * dx, bodyTop + 0.25, sz * trenchCz, span, 0.5, trenchW, L.mat.concreteDk)
		end
	end
end

-- ============================ Track furniture ==========================

function L.buildTrack()
	local s = C.station
	local tunnelTo = s.tunnelMouth + s.tunnelLen
	local railY = s.trenchFloor + 0.10

	-- Two rails per trench. Non-solid: they are 8cm tall and stepping
	-- over them should not be a collision event.
	for _, sz in ipairs({ -1, 1 }) do
		local mid = (s.trenchIn + s.trenchOut) * 0.5
		for _, off in ipairs({ -0.72, 0.72 }) do
			L.box(0, railY, sz * (mid + off), tunnelTo * 2, 0.16, 0.09, L.mat.rail, false)
		end
	end

	-- Sleepers, only under the visible station stretch - the tunnels are
	-- too dark for anyone to notice they run out, and each one is a draw
	-- call that would otherwise buy nothing.
	local spacing = 2.6
	local n = math.floor((s.halfLen * 2) / spacing)
	for _, sz in ipairs({ -1, 1 }) do
		local mid = (s.trenchIn + s.trenchOut) * 0.5
		for i = 0, n do
			local x = -s.halfLen + i * spacing
			L.box(x, s.trenchFloor + 0.02, sz * mid, 0.26, 0.14, 2.4, L.mat.sleeper, false)
		end
	end
end

-- ============================== Colonnade ==============================

function L.buildPillars()
	local s = C.station
	local p = C.pillars
	local h = s.ceiling - s.platformTop

	for i = 0, p.count - 1 do
		local x = p.firstX + i * p.spacing
		for _, sz in ipairs({ -1, 1 }) do
			L.box(x, s.platformTop + h * 0.5, sz * p.z, p.half * 2, h, p.half * 2, L.mat.concrete)
			-- A darker collar at the base, where every station pillar is
			-- scuffed and water-stained.
			L.box(x, s.platformTop + 0.35, sz * p.z, p.half * 2 + 0.08, 0.7, p.half * 2 + 0.08,
				L.mat.concreteDk, false)
		end
	end
end

-- ============================ Platform props ===========================

function L.buildProps()
	local s = C.station
	local y = s.platformTop

	-- Benches, backed against the pillars down the centre line.
	for _, x in ipairs({ -31.5, -13.5, 4.5, 22.5 }) do
		L.box(x, y + 0.44, 0, 2.6, 0.10, 0.55, L.mat.bench)
		for _, sx in ipairs({ -1, 1 }) do
			L.box(x + sx * 1.15, y + 0.22, 0, 0.12, 0.44, 0.5, L.mat.steel, false)
		end
	end

	-- Station signage hung from the ceiling, facing along the platform.
	for _, x in ipairs({ -22.5, 22.5 }) do
		L.box(x, s.ceiling - 0.9, 0, 0.12, 0.7, 3.4, L.mat.sign)
		L.box(x, s.ceiling - 0.35, 0, 0.06, 0.5, 0.06, L.mat.steel, false)
	end

	-- A service cabinet and a dead vending machine against two pillars,
	-- so the platform silhouette is not a perfectly clean corridor.
	L.box(-40.5, y + 0.9, 2.0, 0.7, 1.8, 1.1, L.mat.steel)
	L.box(13.5, y + 0.95, -2.1, 0.8, 1.9, 1.2, L.mat.concreteDk)
	L.box(13.5, y + 1.25, -1.52, 0.6, 1.0, 0.06, L.mat.emergency, false)
end

-- ========================== Track access steps =========================
--
-- Without these the trench is a trap: it is 1.7 m below the deck, and
-- the player's step-up is 0.45 m and their jump clears 0.59 m. Four
-- maintenance flights - one into each trench - make the whole station
-- traversable, and give the fight somewhere to go besides the platform.

function L.buildStairs()
	local s = C.station
	local rise = s.platformTop - s.trenchFloor
	local steps = 5
	local stepH = rise / steps
	local stepD = 0.42

	for _, sx in ipairs({ -1, 1 }) do
		for _, sz in ipairs({ -1, 1 }) do
			local x = sx * 20.5
			-- Each tread is a box whose top is one step up; they march
			-- out from the platform edge into the trench.
			for i = 1, steps do
				local top = s.trenchFloor + stepH * i
				local z = sz * (s.platformHalf + stepD * (steps - i) + stepD * 0.5)
				L.box(x, top - (top - s.trenchFloor) * 0.5, z,
					1.6, top - s.trenchFloor, stepD, L.mat.concreteDk)
			end
			-- A handrail post either side, so the flight reads as a way
			-- down rather than as rubble.
			for _, ox in ipairs({ -0.9, 0.9 }) do
				L.box(x + ox, s.platformTop + 0.45, sz * (s.platformHalf + 0.2),
					0.08, 0.9, 0.08, L.mat.steel, false)
			end
		end
	end
end

-- =============================== Lighting ==============================
--
-- Three sodium lamps in the middle of the hall are authored in
-- Metro.json instead of here, as shadow-casting volumetric spots -
-- there is no Lua binding for the volumetric parameters, and they are
-- the shafts of dusty light the station is built around. Everything
-- below is the failing lighting: lamps that flicker, stutter or are
-- dead outright, plus the emergency strips.

function L.buildLights()
	local s = C.station
	local col = C.color

	-- Lamp housings hang between the volumetric three. Each is a body,
	-- a glass panel and a point light; `state` decides how it behaves.
	-- "fixed" lamps are the two authored in Metro.json as shadow-casting
	-- volumetric spots. Only their housing is built here - the light
	-- itself has to live in the scene file, because the volumetric
	-- parameters have no Lua binding and are only read by the scene
	-- deserializer.
	local lamps = {
		{ x = -40.5, state = "flicker" },
		{ x = -27.0, state = "on" },
		{ x = -20.25, state = "fixed" },
		{ x = -13.5, state = "dead" },
		{ x =   0.0, state = "stutter" },
		{ x =  13.5, state = "on" },
		{ x =  20.25, state = "fixed" },
		{ x =  27.0, state = "flicker" },
		{ x =  40.5, state = "dead" },
	}

	for _, spec in ipairs(lamps) do
		local y = s.ceiling - 0.55
		L.box(spec.x, y + 0.18, 0, 1.8, 0.22, 0.5, L.mat.lampBody, false)

		-- The tube gets its own material instance rather than the shared
		-- one: the housing is dimmed by writing its colour every frame,
		-- which a static GameObject allows and a setScale would not (the
		-- SceneGraph transforms statics once, when it takes them).
		local glassMat = spec.state == "dead" and L.mat.lampDead or L.emissive(col.lampWarm)
		L.box(spec.x, y, 0, 1.7, 0.10, 0.42, glassMat, false)

		if spec.state ~= "dead" and spec.state ~= "fixed" then
			local light = L.pointLight(spec.x, y - 0.3, 0, col.lampWarm, 13.0, 1.0)
			L.lamps[#L.lamps + 1] = {
				light = light, glassMat = glassMat, state = spec.state,
				base = 1.0,
				-- Each lamp gets its own phase so the hall never pulses in
				-- unison, which reads as a rendering bug rather than as
				-- failing hardware.
				phase = math.random() * 10.0,
				nextEvent = 0,
				on = true,
			}
		end
	end

	-- Emergency strips along both side walls: dim, red, and the only
	-- thing still working reliably. These never flicker - they are the
	-- light you fall back to when the lamps drop out.
	for _, x in ipairs({ -36, -18, 0, 18, 36 }) do
		for _, sz in ipairs({ -1, 1 }) do
			L.box(x, s.platformTop + 2.5, sz * (s.trenchOut - 0.17), 0.9, 0.09, 0.06,
				L.mat.emergency, false)
			L.pointLight(x, s.platformTop + 2.5, sz * (s.trenchOut - 0.6),
				col.emergency, 6.5, 0.30)
		end
	end

	-- A cold glow deep in each tunnel: it gives the bores a readable
	-- depth instead of a black hole, and silhouettes whatever walks out.
	local deep = s.tunnelMouth + s.tunnelLen - 4
	for _, sx in ipairs({ -1, 1 }) do
		for _, sz in ipairs({ -1, 1 }) do
			L.pointLight(sx * deep, s.trenchFloor + 2.2, sz * (s.trenchIn + s.trenchOut) * 0.5,
				col.tunnelGlow, 16.0, 0.5)
		end
	end
end

-- ============================= Spawn points ============================
--
-- Where enemies come from: the four tunnel bores. Each is a position on
-- the rail bed just inside the mouth, so they climb onto the platform
-- in front of the player rather than appearing on it.

function L.buildSpawns()
	local s = C.station
	local mid = (s.trenchIn + s.trenchOut) * 0.5

	for _, sx in ipairs({ -1, 1 }) do
		for _, sz in ipairs({ -1, 1 }) do
			L.spawns[#L.spawns + 1] = {
				x = sx * (s.tunnelMouth + 3.5),
				y = s.trenchFloor,
				z = sz * mid,
				-- A dim light at each mouth that pulses just before a
				-- spawn, so the player gets a moment of warning.
				light = select(1, L.pointLight(sx * (s.tunnelMouth + 2.0), s.trenchFloor + 1.8,
					sz * mid, C.color.crawler, 7.0, 0.0)),
			}
		end
	end
end

-- ================================ Build ================================

function L.build()
	LIT_USAGE = ShaderUsage.Color + ShaderUsage.Diffuse + ShaderUsage.PBR
		+ ShaderUsage.DeferredRenderer_Gbuffer

	L.buildShell()
	L.buildTrack()
	L.buildPillars()
	L.buildStairs()
	L.buildProps()
	L.buildLights()
	L.buildSpawns()

	echo("[METRO] level: " .. #L.colliders .. " colliders, " .. #L.lamps .. " live lamps")
end

-- ================================ Update ===============================

function L.update(time, dt)
	for _, lamp in ipairs(L.lamps) do
		local v
		if lamp.state == "on" then
			-- Not actually steady: a slow mains ripple, just enough that
			-- the highlight on the tiles is never perfectly still.
			v = 0.92 + 0.08 * math.sin(time * 2.3 + lamp.phase)
		elseif lamp.state == "flicker" then
			-- Mostly on, with brief dropouts at irregular intervals.
			if time > lamp.nextEvent then
				lamp.on = not lamp.on
				lamp.nextEvent = time + (lamp.on and (0.6 + math.random() * 3.4)
					or (0.03 + math.random() * 0.12))
			end
			v = lamp.on and (0.85 + 0.15 * math.random()) or 0.05
		else -- stutter: a failing tube, buzzing on and off continuously
			local n = math.sin(time * 27.0 + lamp.phase) * math.sin(time * 11.3 + lamp.phase * 2)
			v = n > 0.1 and (0.7 + 0.3 * math.random()) or 0.08
		end

		lamp.light:setLightIntensity(lamp.base * v)
		-- The tube tracks its own light, so a dropped lamp goes dark
		-- rather than staying a bright rectangle on a black ceiling.
		local w = C.color.lampWarm
		lamp.glassMat:setColor(Vec4.new(w[1] * v, w[2] * v, w[3] * v, 1))
	end
end

-- ============================== Collision ==============================
--
-- Everything below is the shared query surface: the player, the enemies
-- and every bullet test against the same L.colliders list.

-- Swept resolution is not needed at these speeds - the movers are
-- resolved axis by axis per frame, which cannot tunnel at 7 m/s with a
-- 0.38 m radius against a 60 Hz step.
function L.overlapsAABB(x0, y0, z0, x1, y1, z1)
	for _, c in ipairs(L.colliders) do
		if x1 > c.x0 and x0 < c.x1 and y1 > c.y0 and y0 < c.y1 and z1 > c.z0 and z0 < c.z1 then
			return c
		end
	end
	return nil
end

-- An upright box mover: centre (x,z), feet at y, of the given radius and
-- height. Returns the collider it hit, or nil.
function L.overlapsBody(x, y, z, radius, height)
	return L.overlapsAABB(x - radius, y + 0.02, z - radius,
		x + radius, y + height, z + radius)
end

-- Highest solid surface directly under a body, searching downward from
-- the feet. Returns the support height, or nil if there is nothing
-- within `reach`.
function L.groundUnder(x, y, z, radius, reach)
	local best = nil
	local lo = y - reach
	for _, c in ipairs(L.colliders) do
		if x + radius > c.x0 and x - radius < c.x1
			and z + radius > c.z0 and z - radius < c.z1
			and c.y1 <= y + 0.06 and c.y1 >= lo then
			if not best or c.y1 > best then best = c.y1 end
		end
	end
	return best
end

-- Ray against the static world. Slab test per box, nearest hit wins.
-- Returns distance, and the surface normal of the face that was hit.
function L.rayCast(ox, oy, oz, dx, dy, dz, maxDist)
	local bestT, bestNx, bestNy, bestNz = maxDist, 0, 0, 0

	for _, c in ipairs(L.colliders) do
		local t0, t1 = 0.0, bestT
		local nx, ny, nz = 0, 0, 0
		local hit = true

		-- X slab
		if math.abs(dx) < 1e-8 then
			if ox < c.x0 or ox > c.x1 then hit = false end
		else
			local inv = 1.0 / dx
			local ta, tb = (c.x0 - ox) * inv, (c.x1 - ox) * inv
			local sign = -1
			if ta > tb then ta, tb = tb, ta; sign = 1 end
			if ta > t0 then t0 = ta; nx, ny, nz = sign, 0, 0 end
			if tb < t1 then t1 = tb end
			if t0 > t1 then hit = false end
		end

		-- Y slab
		if hit then
			if math.abs(dy) < 1e-8 then
				if oy < c.y0 or oy > c.y1 then hit = false end
			else
				local inv = 1.0 / dy
				local ta, tb = (c.y0 - oy) * inv, (c.y1 - oy) * inv
				local sign = -1
				if ta > tb then ta, tb = tb, ta; sign = 1 end
				if ta > t0 then t0 = ta; nx, ny, nz = 0, sign, 0 end
				if tb < t1 then t1 = tb end
				if t0 > t1 then hit = false end
			end
		end

		-- Z slab
		if hit then
			if math.abs(dz) < 1e-8 then
				if oz < c.z0 or oz > c.z1 then hit = false end
			else
				local inv = 1.0 / dz
				local ta, tb = (c.z0 - oz) * inv, (c.z1 - oz) * inv
				local sign = -1
				if ta > tb then ta, tb = tb, ta; sign = 1 end
				if ta > t0 then t0 = ta; nx, ny, nz = 0, 0, sign end
				if tb < t1 then t1 = tb end
				if t0 > t1 then hit = false end
			end
		end

		if hit and t0 > 0.0 and t0 < bestT then
			bestT, bestNx, bestNy, bestNz = t0, nx, ny, nz
		end
	end

	if bestT < maxDist then return bestT, bestNx, bestNy, bestNz end
	return nil
end

function L.destroy()
	L.lamps = {}
	L.spawns = {}
	L.colliders = {}
	L.mat = {}
	L.keep = {}
end

return L
