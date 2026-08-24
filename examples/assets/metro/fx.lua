-- ****************************************************************
-- METRO - impacts, blood, sparks and dust.
--
-- Two pools of one-shot emitters, one additive and one alpha-blended,
-- reconfigured per use rather than one pool per effect. Bursts are
-- queued at the hit point and fired in FX.fireQueued() *after* the
-- frame's scene update, so the emitter samples its fresh world
-- position - playing on the same frame it is moved emits the burst at
-- wherever that pool slot happened to be last.
-- ****************************************************************

local C = import("config")

local FX = { keep = {} }

local function keep(obj)
	FX.keep[#FX.keep + 1] = obj
	return obj
end

local function vec4(c, a) return Vec4.new(c[1], c[2], c[3], a or 1) end

local ADDITIVE_POOL = 10
local ALPHA_POOL = 10

local function makePool(count, blend, maxParticles)
	local pool = {}
	for i = 1, count do
		local go = GameObject.new()
		local d = ParticleSystemDesc.new()
		d.maxParticles = maxParticles
		d.texture = G.sprite
		d.looping = false
		d.burstCount = 16
		d.minLifetime = 0.18
		d.maxLifetime = 0.45
		d.direction = Vec3.new(0, 1, 0)
		d.spreadAngle = 1.2
		d.minSpeed = 1.5
		d.maxSpeed = 5.0
		d.gravity = Vec3.new(0, -9.0, 0)
		d.damping = 1.6
		d.startSize = 0.16
		d.endSize = 0.02
		d.fadeInFraction = 0.05
		d.fadeOutFraction = 0.45
		d.blendMode = blend
		local ps = ParticleSystem.new(d)
		go:addComponent(ps)
		G.scene:add(go)
		keep(go); keep(ps); keep(d)
		pool[i] = { go = go, ps = ps }
	end
	return pool
end

function FX.build()
	FX.add = { pool = makePool(ADDITIVE_POOL, ParticleBlendMode.Additive, 28), next = 1 }
	FX.alpha = { pool = makePool(ALPHA_POOL, ParticleBlendMode.AlphaBlend, 28), next = 1 }
	FX.armed = {}
end

local function take(set)
	local slot = set.pool[set.next]
	set.next = (set.next % #set.pool) + 1
	return slot
end

local function arm(slot, x, y, z)
	slot.go:setPosition(Vec3.new(x, y, z))
	FX.armed[#FX.armed + 1] = slot
end

-- ============================ Bullet impacts ===========================

-- A spark shower off concrete or steel, thrown back along the surface
-- normal so it reads as coming out of the wall.
function FX.sparkHit(x, y, z, nx, ny, nz)
	local slot = take(FX.add)
	slot.ps:setBurstCount(12)
	slot.ps:setDirection(Vec3.new(nx, ny, nz))
	slot.ps:setSpread(0.9)
	slot.ps:setSpeed(2.0, 7.0)
	slot.ps:setLifetime(0.12, 0.32)
	slot.ps:setGravity(Vec3.new(0, -14.0, 0))
	slot.ps:setSizes(0.10, 0.01, 0.4)
	slot.ps:setColors(Vec4.new(1.00, 0.88, 0.55, 1), Vec4.new(1.00, 0.35, 0.05, 0))
	arm(slot, x + nx * 0.03, y + ny * 0.03, z + nz * 0.03)

	-- Plus a puff of dust in the alpha pool: the spark alone looks like
	-- a firework, the dust is what makes it concrete.
	local dust = take(FX.alpha)
	dust.ps:setBurstCount(8)
	dust.ps:setDirection(Vec3.new(nx, ny, nz))
	dust.ps:setSpread(1.1)
	dust.ps:setSpeed(0.4, 1.6)
	dust.ps:setLifetime(0.35, 0.85)
	dust.ps:setGravity(Vec3.new(0, -0.5, 0))
	dust.ps:setSizes(0.14, 0.55, 0.4)
	dust.ps:setColors(Vec4.new(0.45, 0.44, 0.42, 0.55), Vec4.new(0.30, 0.30, 0.30, 0))
	arm(dust, x + nx * 0.05, y + ny * 0.05, z + nz * 0.05)
end

function FX.bloodHit(x, y, z, nx, ny, nz)
	local slot = take(FX.alpha)
	slot.ps:setBurstCount(14)
	slot.ps:setDirection(Vec3.new(nx, ny, nz))
	slot.ps:setSpread(1.0)
	slot.ps:setSpeed(1.5, 4.5)
	slot.ps:setLifetime(0.25, 0.6)
	slot.ps:setGravity(Vec3.new(0, -12.0, 0))
	slot.ps:setSizes(0.13, 0.03, 0.5)
	slot.ps:setColors(vec4(C.color.blood, 0.95), Vec4.new(0.15, 0.02, 0.02, 0))
	arm(slot, x, y, z)
end

function FX.deathBurst(x, y, z, radius)
	local slot = take(FX.alpha)
	slot.ps:setBurstCount(26)
	slot.ps:setDirection(Vec3.new(0, 1, 0))
	slot.ps:setSpread(3.14)
	slot.ps:setSpeed(1.0, 4.0)
	slot.ps:setLifetime(0.4, 0.95)
	slot.ps:setGravity(Vec3.new(0, -7.0, 0))
	slot.ps:setSizes(radius * 0.8, radius * 0.15, 0.5)
	slot.ps:setColors(vec4(C.color.blood, 0.9), Vec4.new(0.08, 0.02, 0.02, 0))
	arm(slot, x, y, z)
end

-- A pickup being collected: a small upward flare in the pickup's colour.
function FX.pickupFlare(x, y, z, color)
	local slot = take(FX.add)
	slot.ps:setBurstCount(14)
	slot.ps:setDirection(Vec3.new(0, 1, 0))
	slot.ps:setSpread(0.7)
	slot.ps:setSpeed(1.2, 3.4)
	slot.ps:setLifetime(0.25, 0.55)
	slot.ps:setGravity(Vec3.new(0, -2.0, 0))
	slot.ps:setSizes(0.18, 0.02, 0.35)
	slot.ps:setColors(vec4(color, 1), vec4(color, 0))
	arm(slot, x, y, z)
end

-- ================================ Frame ================================

function FX.fireQueued()
	for i = 1, #FX.armed do
		FX.armed[i].go:refreshTransformation()
		FX.armed[i].ps:play()
		FX.armed[i] = nil
	end
end

function FX.destroy()
	FX.armed = {}
	FX.add = nil
	FX.alpha = nil
	FX.keep = {}
end

return FX
