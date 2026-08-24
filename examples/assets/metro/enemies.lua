-- ****************************************************************
-- METRO - the things in the tunnels.
--
-- Fixed-size pools, allocated once in E.build() and recycled: nothing
-- is added to or removed from the SceneGraph after the level is up.
-- Inactive members are parked far behind the far plane where the
-- frustum cull drops them and their eye light is switched off.
--
-- Navigation is steering, not pathfinding - the station is a corridor
-- of boxes, so "walk at the player, slide along what you hit, haul
-- yourself up anything short enough" covers every case the level can
-- produce, including climbing out of the track trench.
-- ****************************************************************

local C = import("config")
local Level = import("level")

local Audio, FX

local E = { keep = {}, list = {} }

local PARK = Vec3.new(0, -4000, 0)

local function keep(obj)
	E.keep[#E.keep + 1] = obj
	return obj
end

local function vec4(c, a) return Vec4.new(c[1], c[2], c[3], a or 1) end

-- How far up an enemy can drag itself in one move. Deliberately larger
-- than the player's step: it is what lets them come up out of the
-- trench and onto the platform without a navigation mesh.
local CLIMB = 2.0

-- ================================ Build ================================

local function buildOne(kind)
	local cfg = C.enemy[kind]

	local go = GameObject.new()

	-- Its own material instance, so a hit can flash this one enemy
	-- without lighting up every other member of the pool.
	local bodyMat = GenericShaderMaterial.new(
		ShaderUsage.Color + ShaderUsage.Diffuse + ShaderUsage.PBR
		+ ShaderUsage.DeferredRenderer_Gbuffer)
	bodyMat:setColor(vec4(C.color[kind]))
	bodyMat:setRoughness(0.88)
	bodyMat:setMetallic(0.0)
	keep(bodyMat)

	local bodyH = cfg.height * 0.72
	local body = keep(Capsule.new(cfg.radius, bodyH, 6, 10, 6, true))
	local brc = RenderingComponent.new(body, bodyMat)
	go:addComponent(brc)
	keep(brc)

	-- Head, carried forward of centre so the silhouette has a direction
	-- even before it moves. Child offsets are relative to the capsule's
	-- centre, not to the feet - bodyH is the measure to use here.
	local headGO = GameObject.new()
	local head = keep(Sphere.new(cfg.radius * 0.62, 10, 8))
	local hrc = RenderingComponent.new(head, bodyMat)
	headGO:addComponent(hrc)
	headGO:setPosition(Vec3.new(0, bodyH * 0.42, -cfg.radius * 0.5))
	go:addGameObject(headGO)
	keep(headGO); keep(hrc)

	-- Eyes: unlit, so they stay visible at full brightness in a tunnel
	-- with no light in it at all.
	local eyeMat = GenericShaderMaterial.new(ShaderUsage.Color)
	eyeMat:setColor(vec4(cfg.eyeColor))
	eyeMat:setTransparencyFlag(true)
	eyeMat:enableDepthTest(0)
	keep(eyeMat)

	for _, sx in ipairs({ -1, 1 }) do
		local eyeGO = GameObject.new()
		local eye = keep(Cube.new(cfg.radius * 0.10, cfg.radius * 0.07, cfg.radius * 0.06))
		local erc = RenderingComponent.new(eye, eyeMat)
		eyeGO:addComponent(erc)
		eyeGO:setPosition(Vec3.new(sx * cfg.radius * 0.28, bodyH * 0.45,
			-cfg.radius * 1.02))
		go:addGameObject(eyeGO)
		keep(eyeGO); keep(erc)
	end

	-- A small light at the eyes. Short radius so the deferred pass stays
	-- cheap, but enough that something coming down a dark tunnel is a
	-- moving glow before it is a shape.
	local light = PointLight.new(vec4(cfg.eyeColor), cfg.lightRadius)
	light:setLightIntensity(0)
	go:addComponent(light)
	keep(light)

	go:setPosition(PARK)
	G.scene:add(go)
	keep(go)

	return {
		kind = kind, cfg = cfg, go = go, mat = bodyMat, eyeMat = eyeMat, light = light,
		bodyOffset = bodyH * 0.5 + cfg.radius,   -- capsule centre above the feet
		active = false, hp = 0,
		x = 0, y = 0, z = 0, vy = 0,
		onGround = false, facing = 0,
		nextAttack = 0, hitFlash = 0, dying = 0,
	}
end

function E.build()
	Audio = import("audio")
	FX = import("fx")

	E.list = {}
	for kind, count in pairs(C.enemy.pool) do
		for _ = 1, count do
			E.list[#E.list + 1] = buildOne(kind)
		end
	end
	E.alive = 0
	echo("[METRO] enemies: pool of " .. #E.list)
end

-- =============================== Spawning ==============================

function E.countAlive()
	local n = 0
	for _, e in ipairs(E.list) do
		if e.active and e.dying <= 0 then n = n + 1 end
	end
	return n
end

function E.spawn(kind, time)
	for _, e in ipairs(E.list) do
		if e.kind == kind and not e.active then
			local sp = Level.spawns[math.random(#Level.spawns)]
			e.active = true
			e.dying = 0
			e.hp = e.cfg.hp
			e.x, e.y, e.z = sp.x + (math.random() - 0.5) * 1.5, sp.y, sp.z
			e.vy = 0
			e.onGround = false
			e.nextAttack = time + 0.5
			e.hitFlash = 0
			e.light:setLightIntensity(e.cfg.lightIntensity)
			e.mat:setColor(vec4(C.color[e.kind]))
			e.go:setPosition(Vec3.new(e.x, e.y + e.bodyOffset, e.z))
			e.go:refreshTransformation()

			-- The mouth it came out of flares briefly - the player's only
			-- warning, and the reason to keep watching the tunnels.
			sp.light:setLightIntensity(1.6)
			sp.flareUntil = time + C.enemy.spawnFlashTime
			Audio.play("spawn", e.x, e.y, e.z)
			return e
		end
	end
	return nil
end

-- ============================== Ray casting ============================

-- Ray against every live enemy's bounding box. Returns distance, the
-- enemy, and whether the hit landed in the top quarter (the head).
function E.rayCast(ox, oy, oz, dx, dy, dz, maxDist)
	local bestT, bestE, bestHead = maxDist, nil, false

	for _, e in ipairs(E.list) do
		if e.active and e.dying <= 0 then
			local r = e.cfg.radius
			local x0, x1 = e.x - r, e.x + r
			local y0, y1 = e.y, e.y + e.cfg.height
			local z0, z1 = e.z - r, e.z + r

			local t0, t1 = 0.0, bestT
			local ok = true

			-- Slab test, one axis at a time; any empty overlap rejects.
			local function slab(o, d, lo, hi)
				if math.abs(d) < 1e-8 then
					if o < lo or o > hi then return false end
					return true
				end
				local inv = 1.0 / d
				local ta, tb = (lo - o) * inv, (hi - o) * inv
				if ta > tb then ta, tb = tb, ta end
				if ta > t0 then t0 = ta end
				if tb < t1 then t1 = tb end
				return t0 <= t1
			end

			ok = slab(ox, dx, x0, x1)
			if ok then ok = slab(oy, dy, y0, y1) end
			if ok then ok = slab(oz, dz, z0, z1) end

			if ok and t0 > 0.0 and t0 < bestT then
				bestT = t0
				bestE = e
				bestHead = (oy + dy * t0) > (e.y + e.cfg.height * 0.74)
			end
		end
	end

	if bestE then return bestT, bestE, bestHead end
	return nil
end

-- ================================ Damage ===============================

function E.damage(e, amount, dx, dy, dz, time)
	if not e.active or e.dying > 0 then return end

	e.hp = e.hp - amount
	e.hitFlash = 1.0

	-- A little knockback along the shot, so hits read even when the
	-- target does not die.
	local push = (e.kind == "brute") and 0.06 or 0.18
	e.x = e.x + dx * push
	e.z = e.z + dz * push

	if e.hp <= 0 then
		e.dying = 0.45
		e.light:setLightIntensity(0)
		E.onKill(e, time)
		Audio.play("enemyDie", e.x, e.y, e.z)
		FX.deathBurst(e.x, e.y + e.cfg.height * 0.5, e.z, e.cfg.radius)
	else
		Audio.play("enemyHit", e.x, e.y, e.z)
	end
end

-- Replaced by game.lua, which owns score and drops.
function E.onKill(e, time) end

local function deactivate(e)
	e.active = false
	e.dying = 0
	e.light:setLightIntensity(0)
	e.go:setPosition(PARK)
	e.go:refreshTransformation()
end

-- =============================== Movement ==============================

-- Try to move to (nx, nz), sliding on what is hit and climbing anything
-- short enough. Mirrors the player's mover but with a much larger step.
local function step(e, nx, nz)
	local r, h = e.cfg.radius, e.cfg.height

	if not Level.overlapsBody(nx, e.y, e.z, r, h) then
		e.x = nx
	else
		local ground = Level.groundUnder(nx, e.y + CLIMB, e.z, r, CLIMB + 0.1)
		if ground and ground > e.y + 0.02 and ground <= e.y + CLIMB
			and not Level.overlapsBody(nx, ground, e.z, r, h) then
			e.x, e.y = nx, ground
			e.vy = 0
		end
	end

	if not Level.overlapsBody(e.x, e.y, nz, r, h) then
		e.z = nz
	else
		local ground = Level.groundUnder(e.x, e.y + CLIMB, nz, r, CLIMB + 0.1)
		if ground and ground > e.y + 0.02 and ground <= e.y + CLIMB
			and not Level.overlapsBody(e.x, ground, nz, r, h) then
			e.z, e.y = nz, ground
			e.vy = 0
		end
	end
end

function E.update(dt, time, P)
	local sep = C.enemy.separation
	E.alive = 0

	for _, e in ipairs(E.list) do
		if e.active then
			if e.dying > 0 then
				-- Collapse: sink into the floor over the death time, then
				-- go back to the pool.
				e.dying = e.dying - dt
				local t = math.max(0, e.dying / 0.45)
				e.go:setPosition(Vec3.new(e.x, e.y + e.bodyOffset - (1 - t) * e.cfg.height * 0.8, e.z))
				e.go:setScale(Vec3.new(1, math.max(0.05, t), 1))
				e.go:refreshTransformation()
				if e.dying <= 0 then
					e.go:setScale(Vec3.new(1, 1, 1))
					deactivate(e)
				end
			else
				E.alive = E.alive + 1

				-- ---- steer toward the player ----
				local tx, tz = P.x - e.x, P.z - e.z
				local dist = math.sqrt(tx * tx + tz * tz)
				if dist > 1e-4 then tx, tz = tx / dist, tz / dist end

				-- ---- push apart from the others ----
				local px, pz = 0, 0
				for _, o in ipairs(E.list) do
					if o ~= e and o.active and o.dying <= 0 then
						local ox, oz = e.x - o.x, e.z - o.z
						local d2 = ox * ox + oz * oz
						local want = e.cfg.radius + o.cfg.radius
						if d2 > 1e-6 and d2 < want * want then
							local d = math.sqrt(d2)
							px = px + (ox / d) * (want - d) / want
							pz = pz + (oz / d) * (want - d) / want
						end
					end
				end

				local mx = tx + px * sep
				local mz = tz + pz * sep
				local ml = math.sqrt(mx * mx + mz * mz)
				if ml > 1e-4 then mx, mz = mx / ml, mz / ml end

				-- ---- attack, or close the distance ----
				local inRange = dist <= e.cfg.attackRange
					and math.abs((P.y + C.player.height * 0.5) - (e.y + e.cfg.height * 0.5)) < 2.0

				if inRange then
					if time >= e.nextAttack then
						e.nextAttack = time + e.cfg.attackEvery
						if P.alive then
							P.damage(e.cfg.damage, e.x, e.y, e.z, time)
							Audio.play("enemyAttack", e.x, e.y, e.z)
						end
					end
				else
					local sp = e.cfg.speed
					step(e, e.x + mx * sp * dt, e.z + mz * sp * dt)
				end

				-- ---- gravity ----
				e.vy = e.vy - C.player.gravity * dt
				local ny = e.y + e.vy * dt
				local reach = math.max(0.1, (e.y - ny) + 0.1)
				local ground = Level.groundUnder(e.x, e.y, e.z, e.cfg.radius, reach)
				if e.vy <= 0 and ground and ny <= ground then
					e.y = ground
					e.vy = 0
					e.onGround = true
				else
					e.y = ny
					e.onGround = false
				end
				if e.y < -60 then deactivate(e) end

				-- ---- face the way it is going ----
				if ml > 1e-4 then
					-- The mesh's forward is -Z, matching the camera basis,
					-- so the yaw that points it at (mx, mz) is atan2 of the
					-- negated direction.
					e.facing = math.atan(-mx, -mz)
					e.go:setRotation(Vec3.new(0, e.facing, 0))
				end

				-- ---- hit flash decays back to the body colour ----
				if e.hitFlash > 0 then
					e.hitFlash = math.max(0, e.hitFlash - dt * 5.0)
					local base = C.color[e.kind]
					local f = e.hitFlash
					e.mat:setColor(Vec4.new(
						base[1] + (1 - base[1]) * f,
						base[2] + (0.3 - base[2]) * f,
						base[3] + (0.25 - base[3]) * f, 1))
				end

				local bob = math.sin(time * 7.0 + e.x) * 0.03
				e.go:setPosition(Vec3.new(e.x, e.y + e.bodyOffset + bob, e.z))
				e.go:refreshTransformation()
			end
		end
	end

	-- Spawn-mouth flares fade back down.
	for _, sp in ipairs(Level.spawns) do
		if sp.flareUntil then
			if time >= sp.flareUntil then
				sp.light:setLightIntensity(0)
				sp.flareUntil = nil
			else
				sp.light:setLightIntensity(1.6 * (sp.flareUntil - time) / C.enemy.spawnFlashTime)
			end
		end
	end
end

function E.clear()
	for _, e in ipairs(E.list) do
		if e.active then
			e.go:setScale(Vec3.new(1, 1, 1))
			deactivate(e)
		end
	end
	for _, sp in ipairs(Level.spawns) do
		sp.light:setLightIntensity(0)
		sp.flareUntil = nil
	end
	E.alive = 0
end

function E.destroy()
	E.list = {}
	E.keep = {}
end

return E
