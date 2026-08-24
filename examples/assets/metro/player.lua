-- ****************************************************************
-- METRO - the player: look, movement, collision, flashlight, health.
--
-- Drives the scene's Camera GameObject directly. Movement is a box
-- mover resolved axis by axis against Level.colliders, with a step-up
-- so the platform lip and the sleepers are walked over rather than
-- bumped into.
-- ****************************************************************

local C = import("config")
local Level = import("level")

-- Resolved in P.build rather than at load time: audio imports config
-- too, and requiring it here would fix the module order for no reason.
local Audio

local P = { keep = {} }

local function keep(obj)
	P.keep[#P.keep + 1] = obj
	return obj
end

-- ================================ State ================================

function P.build(cameraObject)
	local p = C.player

	Audio = import("audio")
	P.cam = cameraObject
	P.yaw = 90.0          -- facing down the platform toward +X
	P.pitch = 0.0
	P.recoilPitch = 0.0
	P.recoilYaw = 0.0

	P.x = p.spawn.x
	P.z = p.spawn.z
	P.y = C.station.platformTop      -- feet
	P.vx, P.vy, P.vz = 0, 0, 0
	P.onGround = true
	P.crouching = false
	P.height = p.height
	P.bobPhase = 0
	P.bobOffset = 0
	P.stepAccum = 0

	P.health = p.maxHealth
	P.lastDamage = -999
	P.alive = true

	P.keys = { fwd = false, back = false, left = false, right = false,
		sprint = false, crouch = false, jump = false }

	P.buildFlashlight()
	-- Take the spawn pose now rather than on the first captured frame:
	-- until the player presses TAB, P.update does not run, and without
	-- this the view would sit at whatever the scene file seeded.
	P.apply()
	P.cam:setPosition(Vec3.new(P.x, P.eyeY(), P.z))
	P.cam:refreshTransformation()
end

function P.buildFlashlight()
	local f = C.flashlight

	P.flashOn = true
	P.battery = f.battery
	P.flashStutter = 0

	local go = GameObject.new()
	local light = SpotLight.new(
		Vec4.new(f.color[1], f.color[2], f.color[3], 1),
		f.radius,
		Vec3.new(0, 0, -1),
		f.outterCone, f.innerCone)
	light:setLightIntensity(f.intensity)

	-- The beam casts. This is the single most important thing the light
	-- does: without it the flashlight washes straight through the
	-- pillars and the colonnade stops reading as geometry at all.
	-- All three arguments are passed explicitly - sol does not apply
	-- C++ default arguments, so EnableCastShadows(w, h) alone raises.
	light:enableShadows(f.shadowSize, f.shadowSize, f.shadowNear)
	light:setShadowPCFTexelSize(f.shadowPCF)
	light:setShadowBias(f.shadowBiasFactor, f.shadowBiasUnits)

	go:addComponent(light)
	G.scene:add(go)
	keep(go); keep(light)

	P.flashGO = go
	P.flashLight = light

	-- A short-range fill light riding the eye, purely so the weapon is
	-- lit. The gun sits 50 cm out and 20 cm off the view axis, which
	-- from a light source that close is nearly 40 degrees off the cone
	-- axis - outside the beam entirely, which is why the viewmodel
	-- rendered as a black cut-out against the station. Its radius is
	-- shorter than the player's eye height, so it reaches the gun and
	-- almost nothing else; what little it spills onto a wall the player
	-- is pressed against reads as bounce.
	local fillGO = GameObject.new()
	local fill = PointLight.new(Vec4.new(0.80, 0.83, 0.92, 1), C.flashlight.fillRadius)
	fill:setLightIntensity(C.flashlight.fillIntensity)
	fillGO:addComponent(fill)
	G.scene:add(fillGO)
	keep(fillGO); keep(fill)
	P.fillGO = fillGO
	P.fillLight = fill
end

-- ================================ Input ================================

function P.bindInput(input)
	local function set(k, v) return function() P.keys[k] = v end end

	input:onKeyPressed(Key.W, set("fwd", true))
	input:onKeyReleased(Key.W, set("fwd", false))
	input:onKeyPressed(Key.S, set("back", true))
	input:onKeyReleased(Key.S, set("back", false))
	input:onKeyPressed(Key.A, set("left", true))
	input:onKeyReleased(Key.A, set("left", false))
	input:onKeyPressed(Key.D, set("right", true))
	input:onKeyReleased(Key.D, set("right", false))
	input:onKeyPressed(Key.LShift, set("sprint", true))
	input:onKeyReleased(Key.LShift, set("sprint", false))
	input:onKeyPressed(Key.LControl, set("crouch", true))
	input:onKeyReleased(Key.LControl, set("crouch", false))
	input:onKeyPressed(Key.Space, set("jump", true))
	input:onKeyReleased(Key.Space, set("jump", false))

	input:onKeyPressed(Key.F, function()
		if P.battery > 0 then P.flashOn = not P.flashOn end
	end)
end

-- Mouse look. The warp-to-centre dance and the math.floor on the centre
-- are load-bearing: warpMouseToCenter() targets (int)(Width / 2), so on
-- an odd window width recording w * 0.5 leaves a permanent half-pixel
-- delta and the view rotates on its own with the mouse still. This is
-- the same fix camera_fly.lua carries, and for the same reason.
function P.onMouseMoved(x, y)
	if not P.captured then
		P.lastMouseX, P.lastMouseY = nil, nil
		return
	end
	if P.ignoreNextDelta then
		P.ignoreNextDelta = false
		P.lastMouseX, P.lastMouseY = x, y
		return
	end
	if P.lastMouseX ~= nil then
		local dx = x - P.lastMouseX
		local dy = y - P.lastMouseY
		if dx ~= 0 or dy ~= 0 then
			local sens = C.player.mouseSens * (P.adsFactor or 0) * -0.45 + C.player.mouseSens
			P.yaw = P.yaw - dx * sens
			P.pitch = P.pitch - dy * sens
			local lim = C.player.pitchLimit
			if P.pitch < -lim then P.pitch = -lim end
			if P.pitch > lim then P.pitch = lim end
			P.apply()
			warpMouseToCenter()
			local w, h = getWindowSize()
			P.lastMouseX = math.floor(w / 2)
			P.lastMouseY = math.floor(h / 2)
			return
		end
	end
	P.lastMouseX, P.lastMouseY = x, y
end

function P.setCaptured(on)
	P.captured = on
	if setMouseCaptured then setMouseCaptured(on) end
	if on then
		warpMouseToCenter()
		P.lastMouseX, P.lastMouseY = nil, nil
		P.ignoreNextDelta = true
	else
		P.lastMouseX, P.lastMouseY = nil, nil
		for k in pairs(P.keys) do P.keys[k] = false end
	end
end

-- ============================== Orientation ============================

-- View basis from yaw/pitch. At yaw 0 the camera looks down -Z, which
-- is what the composed quaternion below produces, so these two must
-- stay in step - forward() is what movement, the flashlight and every
-- shot are aimed along.
function P.forward()
	local y = math.rad(P.yaw + P.recoilYaw)
	local p = math.rad(P.pitch + P.recoilPitch)
	local cp = math.cos(p)
	return -cp * math.sin(y), math.sin(p), -cp * math.cos(y)
end

function P.flatForward()
	local y = math.rad(P.yaw)
	return -math.sin(y), -math.cos(y)
end

-- Composed as quaternions, yaw then pitch, rather than handed over as a
-- Euler triple: SetRotation would apply pitch about the world X axis,
-- which gimbal-locks at yaw 90 and inverts past it. camera_fly.lua's
-- comment documents the measurement.
function P.apply()
	local qPitch = Quaternion.new()
	local qYaw = Quaternion.new()
	qPitch:axisToQuaternion(Vec3.new(1, 0, 0), math.rad(P.pitch + P.recoilPitch))
	qYaw:axisToQuaternion(Vec3.new(0, 1, 0), math.rad(P.yaw + P.recoilYaw))
	P.cam:setRotation((qYaw * qPitch):getEulerRotation(0))
end

function P.eyeY()
	return P.y + P.height - C.player.eyeDrop + P.bobOffset
end

function P.addRecoil(pitch, yaw)
	P.recoilPitch = P.recoilPitch + pitch
	P.recoilYaw = P.recoilYaw + yaw
end

-- ============================== Movement ===============================

local function moveAxis(dx, dz)
	local p = C.player
	local r, h = p.radius, P.height

	local nx, nz = P.x + dx, P.z + dz

	-- X first, then Z, so sliding along a wall keeps the other component.
	if dx ~= 0 then
		if Level.overlapsBody(nx, P.y, P.z, r, h) then
			-- Try stepping up onto whatever is in the way before giving up.
			local stepY = P.y + p.stepHeight
			local ground = Level.groundUnder(nx, stepY, P.z, r, p.stepHeight + 0.1)
			if P.onGround and ground and ground > P.y + 0.02 and ground <= P.y + p.stepHeight
				and not Level.overlapsBody(nx, ground, P.z, r, h) then
				P.x, P.y = nx, ground
			else
				P.vx = 0
			end
		else
			P.x = nx
		end
	end

	if dz ~= 0 then
		if Level.overlapsBody(P.x, P.y, nz, r, h) then
			local stepY = P.y + p.stepHeight
			local ground = Level.groundUnder(P.x, stepY, nz, r, p.stepHeight + 0.1)
			if P.onGround and ground and ground > P.y + 0.02 and ground <= P.y + p.stepHeight
				and not Level.overlapsBody(P.x, ground, nz, r, h) then
				P.z, P.y = nz, ground
			else
				P.vz = 0
			end
		else
			P.z = nz
		end
	end
end

function P.update(dt, time)
	local p = C.player
	if not P.alive then
		P.updateFlashlight(dt, time)
		return
	end

	-- ---- crouch ----
	local wantCrouch = P.keys.crouch
	if wantCrouch and not P.crouching then
		P.crouching = true
		P.height = p.crouchHeight
	elseif not wantCrouch and P.crouching then
		-- Only stand up if there is room to.
		if not Level.overlapsBody(P.x, P.y, P.z, p.radius, p.height) then
			P.crouching = false
			P.height = p.height
		end
	end

	-- ---- wish direction in world space ----
	local fx, fz = P.flatForward()
	local rx, rz = -fz, fx           -- right = forward rotated -90 about Y
	local wx, wz = 0, 0
	if P.keys.fwd then wx = wx + fx; wz = wz + fz end
	if P.keys.back then wx = wx - fx; wz = wz - fz end
	if P.keys.right then wx = wx + rx; wz = wz + rz end
	if P.keys.left then wx = wx - rx; wz = wz - rz end

	local wlen = math.sqrt(wx * wx + wz * wz)
	if wlen > 0 then wx, wz = wx / wlen, wz / wlen end

	local speed = p.walkSpeed
	if P.crouching then
		speed = p.crouchSpeed
	elseif P.keys.sprint and P.keys.fwd then
		speed = p.sprintSpeed
	end

	-- ---- horizontal velocity: accelerate toward the wish, else brake ----
	local accel = P.onGround and p.accel or p.airAccel
	if wlen > 0 then
		P.vx = P.vx + (wx * speed - P.vx) * math.min(accel * dt, 1)
		P.vz = P.vz + (wz * speed - P.vz) * math.min(accel * dt, 1)
	elseif P.onGround then
		local drop = math.min(p.friction * dt, 1)
		P.vx = P.vx - P.vx * drop
		P.vz = P.vz - P.vz * drop
	end

	-- ---- jump and gravity ----
	if P.keys.jump and P.onGround and not P.crouching then
		P.vy = p.jumpSpeed
		P.onGround = false
	end
	P.vy = P.vy - p.gravity * dt

	moveAxis(P.vx * dt, P.vz * dt)

	-- ---- vertical ----
	local ny = P.y + P.vy * dt
	if P.vy <= 0 then
		-- Falling: land on the highest support within the distance fallen
		-- this frame (plus a skin), so a fast fall cannot pass through a
		-- one-frame-thin gap above a floor.
		local reach = math.max(0.08, (P.y - ny) + 0.08)
		local ground = Level.groundUnder(P.x, P.y, P.z, C.player.radius, reach)
		if ground and ny <= ground then
			P.y = ground
			P.vy = 0
			if not P.onGround then Audio.play("step", P.x, P.y, P.z) end
			P.onGround = true
		else
			P.y = ny
			P.onGround = false
		end
	else
		-- Rising: stop at a ceiling.
		if Level.overlapsBody(P.x, ny, P.z, C.player.radius, P.height) then
			P.vy = 0
		else
			P.y = ny
		end
		P.onGround = false
	end

	-- The trench is survivable but you have to climb back out at the
	-- ramp; falling off the end of the world is not.
	if P.y < -30 then P.damage(1000, 0, 0, 0) end

	-- ---- head bob and footsteps ----
	local hspeed = math.sqrt(P.vx * P.vx + P.vz * P.vz)
	if P.onGround and hspeed > 0.6 then
		local rate = p.bobRate * (hspeed / p.walkSpeed)
		P.bobPhase = P.bobPhase + rate * dt
		P.bobOffset = math.sin(P.bobPhase * 2) * p.bobAmount * math.min(hspeed / p.walkSpeed, 1.4)

		P.stepAccum = P.stepAccum + hspeed * dt
		if P.stepAccum > 2.2 then
			P.stepAccum = 0
			Audio.play("step", P.x, P.y, P.z)
		end
	else
		P.bobOffset = P.bobOffset - P.bobOffset * math.min(8 * dt, 1)
	end

	-- ---- recoil recovery ----
	local decay = math.min(C.weapon.recoilDecay * dt, 1)
	P.recoilPitch = P.recoilPitch - P.recoilPitch * decay
	P.recoilYaw = P.recoilYaw - P.recoilYaw * decay

	-- ---- health regeneration ----
	if time - P.lastDamage > p.regenDelay and P.health < p.regenCap then
		P.health = math.min(p.regenCap, P.health + p.regenRate * dt)
	end

	P.cam:setPosition(Vec3.new(P.x, P.eyeY(), P.z))
	P.apply()
	P.cam:refreshTransformation()

	P.updateFlashlight(dt, time)
end

-- ============================= Flashlight ==============================

function P.updateFlashlight(dt, time)
	local f = C.flashlight

	if P.fillGO then
		P.fillGO:setPosition(Vec3.new(P.x, P.eyeY(), P.z))
		P.fillGO:refreshTransformation()
	end

	local on = P.flashOn and P.battery > 0

	if on then
		P.battery = math.max(0, P.battery - f.drain * dt)
		if P.battery <= 0 then P.flashOn = false end
	end

	local intensity = 0
	if on then
		intensity = f.intensity
		-- A dying battery does not fade smoothly, it cuts out.
		if P.battery < f.lowAt then
			local t = P.battery / f.lowAt
			local n = math.sin(time * 19.0) * math.sin(time * 7.3)
			if n > (t * 1.6 - 0.5) then
				intensity = f.intensity * (0.25 + 0.4 * t)
			end
		end
	end

	P.flashLight:setLightIntensity(intensity)
	if intensity <= 0 then return end

	local dx, dy, dz = P.forward()

	-- Mounted at the muzzle rather than at the eye - see C.flashlight's
	-- beamRight/beamUp. The weapon no longer depends on this light (the
	-- fill light handles it), so the offset costs nothing and is what
	-- makes the cone's shadows visible at all.
	local rx, ry, rz = -dz, 0, dx
	local rl = math.sqrt(rx * rx + rz * rz)
	if rl < 1e-5 then rx, ry, rz, rl = 1, 0, 0, 1 end
	rx, ry, rz = rx / rl, ry / rl, rz / rl
	local ux, uy, uz = ry * dz - rz * dy, rz * dx - rx * dz, rx * dy - ry * dx

	local ex = P.x + rx * f.beamRight + ux * f.beamUp
	local ey = P.eyeY() + ry * f.beamRight + uy * f.beamUp
	local ez = P.z + rz * f.beamRight + uz * f.beamUp
	P.flashGO:setPosition(Vec3.new(ex, ey, ez))
	P.flashGO:refreshTransformation()
	P.flashLight:setLightDirection(Vec3.new(dx, dy, dz))
end

function P.addBattery(amount)
	P.battery = math.min(C.flashlight.battery, P.battery + amount)
	if P.battery > 0 then P.flashOn = true end
end

-- ================================ Damage ===============================

-- The hit direction is kept so the HUD can point at what hurt you.
function P.damage(amount, fromX, fromY, fromZ, time)
	if not P.alive then return end
	P.health = P.health - amount
	P.lastDamage = time or 0
	P.lastHitFrom = { x = fromX, y = fromY, z = fromZ }
	P.hitFlash = 1.0
	Audio.play("hurt", P.x, P.eyeY(), P.z)

	if P.health <= 0 then
		P.health = 0
		P.alive = false
	end
end

function P.heal(amount)
	P.health = math.min(C.player.maxHealth, P.health + amount)
end

function P.reset()
	local p = C.player
	P.x, P.z = p.spawn.x, p.spawn.z
	P.y = C.station.platformTop
	P.vx, P.vy, P.vz = 0, 0, 0
	P.health = p.maxHealth
	P.alive = true
	P.crouching = false
	P.height = p.height
	P.yaw, P.pitch = 90.0, 0.0
	P.recoilPitch, P.recoilYaw = 0, 0
	P.battery = C.flashlight.battery
	P.flashOn = true
	P.hitFlash = 0
	P.apply()
	P.cam:setPosition(Vec3.new(P.x, P.eyeY(), P.z))
	P.cam:refreshTransformation()
end

function P.destroy()
	if P.flashLight then P.flashLight:setLightIntensity(0) end
	if P.fillLight then P.fillLight:setLightIntensity(0) end
	P.flashGO = nil
	P.flashLight = nil
	P.fillGO = nil
	P.fillLight = nil
	P.cam = nil
	P.keep = {}
end

return P
