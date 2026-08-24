-- ****************************************************************
-- METRO - the run: state machine, waves, pickups and score.
--
-- States:
--   briefing  the station before anything comes out of the tunnels
--   wave      spawning and fighting
--   rest      the wave is cleared, the next is counting in
--   dead      game over, waiting for a restart
-- ****************************************************************

local C = import("config")
local Level = import("level")

local P, W, E, FX, Audio

local Game = { keep = {} }

local PARK = Vec3.new(0, -4000, 0)

local function keep(obj)
	Game.keep[#Game.keep + 1] = obj
	return obj
end

local function vec4(c, a) return Vec4.new(c[1], c[2], c[3], a or 1) end

-- ================================ Build ================================

function Game.build(player, weapon, enemies, fx, audio)
	P, W, E, FX, Audio = player, weapon, enemies, fx, audio

	Game.buildPickups()

	-- enemies.lua raises this on every kill; score and drops live here.
	E.onKill = function(e, time)
		Game.score = Game.score + e.cfg.score
		Game.kills = Game.kills + 1
		Game.waveKilled = Game.waveKilled + 1

		local roll = math.random()
		if roll < C.wave.ammoDrop then
			Game.dropPickup("ammo", e.x, e.y, e.z)
		elseif roll < C.wave.ammoDrop + C.wave.batteryDrop then
			Game.dropPickup("battery", e.x, e.y, e.z)
		elseif roll < C.wave.ammoDrop + C.wave.batteryDrop + C.wave.medkitDrop then
			Game.dropPickup("medkit", e.x, e.y, e.z)
		end
	end

	Game.reset()
end

function Game.reset()
	Game.state = "briefing"
	Game.stateTime = 0
	Game.wave = 0
	Game.score = 0
	Game.kills = 0
	Game.waveTotal = 0
	Game.waveSpawned = 0
	Game.waveKilled = 0
	Game.queue = {}
	Game.nextSpawn = 0
	Game.message = "STAND BY"
	Game.submessage = "something is moving in the tunnels"
	Game.clearPickups()
end

-- =============================== Pickups ===============================
--
-- Pooled emissive cubes that hover and spin. Deliberately unlit and
-- bright: in a station this dark a pickup lying on the platform has to
-- advertise itself or it is simply never found.

local PICKUP_KINDS = {
	ammo    = { color = "pickupAmmo",    size = 0.22 },
	battery = { color = "pickupBattery", size = 0.18 },
	medkit  = { color = "pickupMedkit",  size = 0.20 },
}

-- A separate small pool per kind, rather than one shared pool whose
-- material is swapped on drop: RenderingComponent exposes no setMaterial
-- to Lua, so the material a slot draws with is fixed when it is built.
local PER_KIND = 4

function Game.buildPickups()
	Game.pickups = {}

	for kind, spec in pairs(PICKUP_KINDS) do
		local m = GenericShaderMaterial.new(ShaderUsage.Color)
		m:setColor(vec4(C.color[spec.color]))
		m:setTransparencyFlag(true)
		m:enableDepthTest(0)
		keep(m)

		for _ = 1, PER_KIND do
			local go = GameObject.new()
			local h = spec.size * 0.5
			local mesh = keep(Cube.new(h, h, h))
			local rc = RenderingComponent.new(mesh, m)
			go:addComponent(rc)
			go:setPosition(PARK)
			G.scene:add(go)
			keep(go); keep(rc)

			local light = PointLight.new(vec4(C.color[spec.color]), 3.2)
			light:setLightIntensity(0)
			go:addComponent(light)
			keep(light)

			Game.pickups[#Game.pickups + 1] = { go = go, light = light,
				active = false, kind = kind, x = 0, y = 0, z = 0, phase = 0 }
		end
	end
end

function Game.dropPickup(kind, x, y, z)
	for _, p in ipairs(Game.pickups) do
		if p.kind == kind and not p.active then
			p.active = true
			p.x, p.y, p.z = x, y + 0.35, z
			p.phase = math.random() * 6.28
			p.light:setLightIntensity(0.7)
			p.go:setPosition(Vec3.new(p.x, p.y, p.z))
			p.go:refreshTransformation()
			return p
		end
	end
	return nil
end

function Game.clearPickups()
	if not Game.pickups then return end
	for _, p in ipairs(Game.pickups) do
		p.active = false
		p.light:setLightIntensity(0)
		p.go:setPosition(PARK)
		p.go:refreshTransformation()
	end
end

local function collect(p)
	if p.kind == "ammo" then
		W.addAmmo(C.weapon.magSize)
		Game.notify("+" .. C.weapon.magSize .. " ROUNDS")
	elseif p.kind == "battery" then
		P.addBattery(C.flashlight.pickup)
		Game.notify("BATTERY +" .. math.floor(C.flashlight.pickup) .. "%")
	else
		P.heal(35)
		Game.notify("+35 HEALTH")
	end
	FX.pickupFlare(p.x, p.y, p.z, C.color[PICKUP_KINDS[p.kind].color])
	Audio.play("pickup", p.x, p.y, p.z)
	p.active = false
	p.light:setLightIntensity(0)
	p.go:setPosition(PARK)
	p.go:refreshTransformation()
end

function Game.updatePickups(dt, time)
	for _, p in ipairs(Game.pickups) do
		if p.active then
			local bob = math.sin(time * 2.6 + p.phase) * 0.06
			p.go:setPosition(Vec3.new(p.x, p.y + bob, p.z))
			p.go:setRotation(Vec3.new(0.4, time * 1.4 + p.phase, 0))
			p.go:refreshTransformation()

			local dx, dz = P.x - p.x, P.z - p.z
			local dy = (P.y + C.player.height * 0.5) - p.y
			if dx * dx + dz * dz < 1.1 * 1.1 and math.abs(dy) < 1.6 then
				collect(p)
			end
		end
	end
end

-- ================================ Waves ================================

function Game.notify(text)
	Game.notice = text
	Game.noticeUntil = (Game.time or 0) + 2.0
end

local function waveComposition(n)
	local w = C.wave
	local crawlers = math.min(w.maxCrawler, w.baseCrawler + (n - 1) * w.crawlerPer)
	local brutes = 0
	if n >= w.bruteFrom then
		brutes = math.min(w.maxBrute, math.floor((n - w.bruteFrom + 1) * w.brutePer) + 1)
	end

	local queue = {}
	for _ = 1, crawlers do queue[#queue + 1] = "crawler" end
	for _ = 1, brutes do queue[#queue + 1] = "brute" end

	-- Shuffle so the brutes are not all at the end of the wave.
	for i = #queue, 2, -1 do
		local j = math.random(i)
		queue[i], queue[j] = queue[j], queue[i]
	end
	return queue
end

function Game.startWave(time)
	Game.wave = Game.wave + 1
	Game.queue = waveComposition(Game.wave)
	Game.waveTotal = #Game.queue
	Game.waveSpawned = 0
	Game.waveKilled = 0
	Game.nextSpawn = time + 0.8
	Game.setState("wave", time)
	Game.message = "WAVE " .. Game.wave
	Game.submessage = Game.waveTotal .. " contacts inbound"
	Audio.play("wave")
end

function Game.setState(s, time)
	Game.state = s
	Game.stateTime = time
end

-- ================================ Update ===============================

function Game.update(dt, time)
	Game.time = time

	if Game.notice and time > (Game.noticeUntil or 0) then Game.notice = nil end

	if Game.state == "briefing" then
		if time - Game.stateTime > C.wave.introTime then
			Game.startWave(time)
		end

	elseif Game.state == "wave" then
		-- Feed the queue in, respecting the live cap so the station never
		-- has more coming at once than the frame budget likes.
		if #Game.queue > 0 and time >= Game.nextSpawn
			and E.countAlive() < C.wave.liveCap then
			local kind = table.remove(Game.queue, 1)
			if E.spawn(kind, time) then
				Game.waveSpawned = Game.waveSpawned + 1
			else
				-- Pool exhausted for that kind; put it back and retry.
				table.insert(Game.queue, 1, kind)
			end
			local gap = C.wave.spawnGap
			Game.nextSpawn = time + gap[1] + math.random() * (gap[2] - gap[1])
		end

		if #Game.queue == 0 and E.countAlive() == 0 then
			Game.setState("rest", time)
			Game.message = "WAVE " .. Game.wave .. " CLEAR"
			Game.submessage = "the next one is already moving"
		end

	elseif Game.state == "rest" then
		if time - Game.stateTime > C.wave.restTime then
			Game.startWave(time)
		end
	end

	if not P.alive and Game.state ~= "dead" then
		Game.setState("dead", time)
		Game.message = "YOU DIED"
		Game.submessage = "wave " .. Game.wave .. "  -  " .. Game.score .. " points"
		E.clear()
		Audio.play("gameover")
	end

	Game.updatePickups(dt, time)
end

function Game.restart(time)
	E.clear()
	Game.reset()
	P.reset()
	W.reset()
	Game.stateTime = time
	Game.time = time
end

function Game.destroy()
	Game.pickups = nil
	Game.keep = {}
end

return Game
