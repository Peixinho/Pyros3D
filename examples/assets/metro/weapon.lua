-- ****************************************************************
-- METRO - the weapon: hitscan fire, spread, recoil, reload, ammo.
--
-- Shooting is a ray from the eye tested against the enemy list and
-- against Level.colliders; whichever is nearer is what was hit. There
-- are no projectiles, so nothing here needs pooling except the muzzle
-- flash light.
-- ****************************************************************

local C = import("config")
local Level = import("level")

local Enemies, FX, Audio

local W = { keep = {} }

local function keep(obj)
	W.keep[#W.keep + 1] = obj
	return obj
end

function W.build()
	local w = C.weapon

	Enemies = import("enemies")
	FX = import("fx")
	Audio = import("audio")

	W.mag = w.magSize
	W.reserve = w.reserve
	W.reloading = false
	W.reloadEnd = 0
	W.nextShot = 0
	W.triggerDown = false
	W.ads = false
	W.adsFactor = 0          -- 0 hip, 1 fully aimed; smoothed in update
	W.shotsFired = 0
	W.hits = 0
	W.flashUntil = 0

	-- The muzzle flash is a real light, not a sprite: in a station this
	-- dark it is what actually reveals the walls when you fire.
	local go = GameObject.new()
	local light = PointLight.new(
		Vec4.new(w.muzzleLight[1], w.muzzleLight[2], w.muzzleLight[3], 1), 14.0)
	light:setLightIntensity(0)
	go:addComponent(light)
	G.scene:add(go)
	keep(go); keep(light)
	W.flashGO = go
	W.flashLight = light

	W.buildViewmodel()
	W.buildDecals()
end

-- ============================== Viewmodel ==============================
--
-- Each part is its own TOP-LEVEL scene object, not a child of the
-- camera, and is placed by hand every frame from the camera basis.
--
-- That is not the obvious way round, but it is the only one that
-- renders: GameObject::Add(child) links the child into _Childs without
-- setting its Scene or registering its components, and SceneGraph
-- calls RegisterComponents only on the three top-level lists. A
-- RenderingComponent that lives on a child of a scene object is
-- therefore never registered with the renderer and simply never draws.

-- The gun is modelled at true size (a 55 cm SMG) but drawn in the world
-- rather than through a separate viewmodel projection, so at arm's
-- length the stock ends up centimetres from the near plane and the
-- receiver fills a quarter of the screen. Real scale is wrong here for
-- the same reason every FPS cheats it: the model is shrunk and pushed
-- out until it reads correctly at the game's field of view.
local VM_SCALE = 0.58
-- Slides the whole gun forward along its own axis, so the origin sits
-- at the muzzle end of the receiver instead of its middle and nothing
-- reaches back toward the eye.
local VM_Z_SHIFT = -0.14

local VM_PARTS = {
	-- name        w      h      d      x       y       z       rx     ry  rz
	{ "receiver",  0.075, 0.088, 0.30,  0,      0,      0,      0,     0,  0 },
	{ "handguard", 0.060, 0.058, 0.15,  0,      0.004, -0.165,  0,     0,  0 },
	{ "barrel",    0.028, 0.028, 0.20,  0,      0.019, -0.255,  0,     0,  0 },
	{ "muzzle",    0.038, 0.030, 0.045, 0,      0.019, -0.362,  0,     0,  0 },
	{ "magazine",  0.046, 0.155, 0.070, 0,     -0.112, -0.028, -0.13,  0,  0 },
	{ "grip",      0.050, 0.125, 0.062, 0,     -0.100,  0.098,  0.30,  0,  0 },
	{ "stock",     0.052, 0.062, 0.150, 0,      0.006,  0.205,  0,     0,  0 },
	{ "rearsight", 0.022, 0.026, 0.022, 0,      0.062, -0.060,  0,     0,  0 },
	{ "frontsight",0.018, 0.032, 0.018, 0,      0.060, -0.318,  0,     0,  0 },
	{ "rail",      0.070, 0.020, 0.055, 0,      0.056,  0.030,  0,     0,  0 },
}

local VM_MAT = {
	receiver = "gunmetal", handguard = "polymer", barrel = "worn",
	muzzle = "gunmetal", magazine = "polymer", grip = "polymer",
	stock = "polymer", rearsight = "gunmetal", frontsight = "gunmetal",
	rail = "gunmetal",
}

-- Parked here while a decal is being placed - see placeDecal.
local VM_PARK = Vec3.new(0, -6000, 0)

-- Where the barrel ends in the gun's own frame, shared by the flash
-- star and the muzzle flash light so both sit on the muzzle device.
local MUZZLE_Z = (-0.40 + VM_Z_SHIFT) * VM_SCALE

function W.buildViewmodel()
	local pbrUsage = ShaderUsage.Color + ShaderUsage.Diffuse + ShaderUsage.PBR
		+ ShaderUsage.DeferredRenderer_Gbuffer

	local function mat(r, g, b, rough, metal)
		local m = GenericShaderMaterial.new(pbrUsage)
		m:setColor(Vec4.new(r, g, b, 1))
		m:setRoughness(rough)
		m:setMetallic(metal)
		return keep(m)
	end

	local mats = {
		gunmetal = mat(0.20, 0.21, 0.23, 0.34, 0.90),
		polymer  = mat(0.075, 0.078, 0.085, 0.72, 0.0),
		worn     = mat(0.32, 0.33, 0.35, 0.26, 0.95),
	}

	W.vmParts = {}
	for _, spec in ipairs(VM_PARTS) do
		local name, pw, ph, pd, px, py, pz, rx, ry, rz = table.unpack(spec)
		pw, ph, pd = pw * VM_SCALE, ph * VM_SCALE, pd * VM_SCALE
		px, py, pz = px * VM_SCALE, py * VM_SCALE, (pz + VM_Z_SHIFT) * VM_SCALE

		local go = GameObject.new()
		local rc = RenderingComponent.new(
			keep(Cube.new(pw * 0.5, ph * 0.5, pd * 0.5)), mats[VM_MAT[name]])
		-- The gun must not cast into the flashlight's shadow map. It sits
		-- 25 cm in front of the light, so at that range the depth bias
		-- cannot separate it from itself and every face it presents to
		-- the camera comes back fully occluded - the whole weapon renders
		-- black. Nothing in the world should be receiving its shadow
		-- anyway, and dropping it also takes it out of the shadow pass.
		rc:disableCastShadows()

		go:addComponent(rc)
		go:setPosition(VM_PARK)
		G.scene:add(go)
		keep(go); keep(rc)
		W.vmParts[#W.vmParts + 1] = {
			go = go, px = px, py = py, pz = pz, rx = rx, ry = ry, rz = rz,
		}
	end

	-- The flash: an unlit star at the muzzle, scaled to nothing between
	-- shots. Unlit so it stays at full brightness against a black
	-- tunnel, depth-test off so it never clips into the barrel.
	local flashMat = GenericShaderMaterial.new(ShaderUsage.Color)
	flashMat:setColor(Vec4.new(1.0, 0.90, 0.62, 1))
	flashMat:setTransparencyFlag(true)
	flashMat:enableDepthTest(0)
	keep(flashMat)

	local fgo = GameObject.new()
	local frc = RenderingComponent.new(
		keep(Cube.new(0.09 * VM_SCALE, 0.09 * VM_SCALE, 0.05 * VM_SCALE)), flashMat)
	frc:disableCastShadows()
	fgo:addComponent(frc)
	fgo:setPosition(VM_PARK)
	G.scene:add(fgo)
	keep(fgo); keep(frc)
	W.vmFlash = { go = fgo, px = 0, py = 0.019 * VM_SCALE,
		pz = MUZZLE_Z, rx = 0, ry = 0, rz = 0 }

	W.vmSway = { x = 0, y = 0 }
	W.vmRecoil = 0
	W.vmParked = false
end

-- Rotate a local offset by the gun's own small pitch/yaw/roll, XYZ
-- order, so recoil and the reload tilt swing the parts about the grip
-- instead of sliding them all rigidly.
local function rotXYZ(x, y, z, rx, ry, rz)
	local cx, sx = math.cos(rx), math.sin(rx)
	local cy, sy = math.cos(ry), math.sin(ry)
	local cz, sz = math.cos(rz), math.sin(rz)

	local y1, z1 = y * cx - z * sx, y * sx + z * cx      -- about X
	local x2, z2 = x * cy + z1 * sy, -x * sy + z1 * cy   -- about Y
	local x3, y3 = x2 * cz - y1 * sz, x2 * sz + y1 * cz  -- about Z
	return x3, y3, z2
end

-- Places one part in the world: the gun's local frame, mapped onto the
-- camera basis (right, up, -forward), then offset from the eye.
local function placePart(part, gx, gy, gz, gpitch, gyaw, groll,
	ex, ey, ez, rx, ry, rz, ux, uy, uz, fx, fy, fz, camQ)
	local lx, ly, lz = rotXYZ(part.px, part.py, part.pz, gpitch, gyaw, groll)
	lx, ly, lz = lx + gx, ly + gy, lz + gz

	part.go:setPosition(Vec3.new(
		ex + rx * lx + ux * ly - fx * lz,
		ey + ry * lx + uy * ly - fy * lz,
		ez + rz * lx + uz * ly - fz * lz))

	-- Orientation: the camera's rotation, then the gun's, then the
	-- part's own tilt. Composed as quaternions and handed over as Euler
	-- for the same reason the camera is - SetRotation would apply these
	-- about world axes and gimbal-lock at yaw 90.
	local qg, qp = Quaternion.new(), Quaternion.new()
	qg:axisToQuaternion(Vec3.new(0, 1, 0), gyaw)
	qp:axisToQuaternion(Vec3.new(1, 0, 0), gpitch + part.rx)
	local qr = Quaternion.new()
	qr:axisToQuaternion(Vec3.new(0, 0, 1), groll)

	part.go:setRotation((camQ * qg * qp * qr):getEulerRotation(0))
	part.go:refreshTransformation()
end

function W.updateViewmodel(dt, time, P)
	if not W.vmParts then return end

	local vm = C.viewmodel
	local t = W.adsFactor

	-- ---- pose: hip toward aimed ----
	local x = vm.hip.x + (vm.ads.x - vm.hip.x) * t
	local y = vm.hip.y + (vm.ads.y - vm.hip.y) * t
	local z = vm.hip.z + (vm.ads.z - vm.hip.z) * t
	local pitch, yaw, roll = 0, 0, 0

	-- ---- sway: the gun lags the view ----
	-- Driven off the change in yaw/pitch rather than raw mouse deltas, so
	-- it behaves the same however the view got there.
	local dyaw = P.yaw - (W.lastYaw or P.yaw)
	local dpitch = P.pitch - (W.lastPitch or P.pitch)
	W.lastYaw, W.lastPitch = P.yaw, P.pitch

	W.vmSway.x = W.vmSway.x + dyaw * 0.004
	W.vmSway.y = W.vmSway.y + dpitch * 0.004
	local sd = math.min(vm.swayDecay * dt, 1)
	W.vmSway.x = W.vmSway.x - W.vmSway.x * sd
	W.vmSway.y = W.vmSway.y - W.vmSway.y * sd
	local swayScale = 1 - 0.75 * t          -- aiming steadies it
	local sx = math.max(-0.04, math.min(0.04, W.vmSway.x)) * swayScale
	local sy = math.max(-0.04, math.min(0.04, W.vmSway.y)) * swayScale
	x, y = x + sx, y + sy
	yaw = yaw - sx * 0.9

	-- ---- walk bob, in step with the head bob ----
	local hspeed = math.sqrt(P.vx * P.vx + P.vz * P.vz)
	if P.onGround and hspeed > 0.6 then
		local amp = vm.bobAmount * math.min(hspeed / C.player.walkSpeed, 1.4) * (1 - 0.8 * t)
		x = x + math.sin(P.bobPhase) * amp
		y = y - math.abs(math.cos(P.bobPhase)) * amp * 0.7
	end

	-- ---- sprinting: drop it out of the firing position ----
	local sprinting = P.keys.sprint and P.keys.fwd and hspeed > C.player.walkSpeed * 0.9
		and not W.ads and not W.reloading
	W.vmSprint = (W.vmSprint or 0)
		+ ((sprinting and 1 or 0) - (W.vmSprint or 0)) * math.min(8 * dt, 1)
	y = y - vm.sprintDrop * W.vmSprint
	roll = roll - vm.sprintTilt * W.vmSprint
	yaw = yaw - 0.22 * W.vmSprint

	-- ---- reload: tilt it in toward the player ----
	if W.reloading then
		local left = math.max(0, W.reloadEnd - time)
		local u = 1 - math.abs((left / C.weapon.reloadTime) * 2 - 1)
		u = u * u * (3 - 2 * u)       -- smoothstep, so it does not snap
		y = y - vm.reloadDrop * u
		pitch = pitch - vm.reloadTilt * u
		roll = roll + 0.25 * u
	end

	-- ---- recoil ----
	W.vmRecoil = W.vmRecoil - W.vmRecoil * math.min(vm.recoilDecay * dt, 1)
	z = z + vm.recoilBack * W.vmRecoil
	pitch = pitch + vm.recoilRise * W.vmRecoil

	-- ---- camera basis ----
	local ex, ey, ez = P.x, P.eyeY(), P.z
	local fx, fy, fz = P.forward()
	-- right = normalize(forward x worldUp), which for worldUp (0,1,0)
	-- reduces to (-fz, 0, fx).
	local rx, ry, rz = -fz, 0, fx
	local rl = math.sqrt(rx * rx + rz * rz)
	if rl < 1e-5 then rx, ry, rz, rl = 1, 0, 0, 1 end
	rx, ry, rz = rx / rl, ry / rl, rz / rl
	local ux, uy, uz = ry * fz - rz * fy, rz * fx - rx * fz, rx * fy - ry * fx

	local qYaw, qPitch = Quaternion.new(), Quaternion.new()
	qYaw:axisToQuaternion(Vec3.new(0, 1, 0), math.rad(P.yaw + P.recoilYaw))
	qPitch:axisToQuaternion(Vec3.new(1, 0, 0), math.rad(P.pitch + P.recoilPitch))
	local camQ = qYaw * qPitch

	W.vmParked = false
	for _, part in ipairs(W.vmParts) do
		placePart(part, x, y, z, pitch, yaw, roll,
			ex, ey, ez, rx, ry, rz, ux, uy, uz, fx, fy, fz, camQ)
	end

	-- The flash star: visible only for the few frames after a shot, and
	-- rolled to a new angle each time so a burst does not strobe one
	-- fixed shape.
	-- Always placed at the muzzle, and hidden between shots by shrinking
	-- it rather than by moving it away. Parking it at VM_PARK put a
	-- depth-test-disabled transparent quad 6 km below the world, where
	-- it lands behind the camera's near plane in view space; with no
	-- depth test to reject it, it rasterised as a large hard-edged black
	-- trapezoid lying on the floor a few metres ahead of the player.
	-- Scaled to a fraction of a millimetre it stays in front of the
	-- camera, inside the frustum, and is simply too small to see.
	placePart(W.vmFlash, x, y, z, pitch, yaw, roll,
		ex, ey, ez, rx, ry, rz, ux, uy, uz, fx, fy, fz, camQ)
	if time < (W.vmFlashUntil or 0) then
		local s = 0.85 + math.random() * 0.6
		W.vmFlash.rx = math.random() * 3.14
		W.vmFlash.go:setScale(Vec3.new(s, s, s))
	else
		W.vmFlash.go:setScale(Vec3.new(0.0001, 0.0001, 0.0001))
	end
	W.vmFlash.go:refreshTransformation()
end

-- placeDecalAtCursor walks every top-level scene object looking for the
-- nearest mesh the ray hits, and the viewmodel is now exactly that: a
-- set of top-level meshes 40 cm in front of the eye. Aimed down the
-- sights the front post sits on the view axis, so the bullet hole would
-- land on the gun. There is no exclusion argument and no handle to
-- disable a component for the call, so the parts are parked out of the
-- world for the duration and put straight back.
local function parkViewmodel(parked)
	if not W.vmParts then return end
	for _, part in ipairs(W.vmParts) do
		if parked then
			-- Remember where the part actually was rather than relying on
			-- updateViewmodel running again later in the same frame to put
			-- it back; unparking has to be self-contained.
			part.parkedFrom = part.go:getPosition()
			part.go:setPosition(VM_PARK)
		elseif part.parkedFrom then
			part.go:setPosition(part.parkedFrom)
			part.parkedFrom = nil
		end
		part.go:refreshTransformation()
	end
	W.vmParked = parked
end

-- =============================== Decals ================================

function W.buildDecals()
	local tex = Texture.new()
	tex:loadTexture(GAME_PATH .. "bullethole.png", TextureType.Texture, true, 0)
	keep(tex)

	local m = GenericShaderMaterial.new(ShaderUsage.Texture)
	m:setColorMap(tex)
	m:setTransparencyFlag(true)
	-- Pulled toward the viewer and kept out of the depth buffer, or the
	-- decal z-fights with the very surface it is lying on.
	m:enableDepthBias(-4, -4)
	m:disableDepthWrite()
	keep(m)

	W.decalMat = m
	W.decalCount = 0
end

-- placeDecalAtCursor builds the decal by re-casting a ray through a
-- screen pixel, so the bullet's own direction has to be turned back
-- into one. ox/oy are the shot's tangent offsets from the view axis, so
-- dividing by the half-frustum tangents gives normalised device
-- coordinates directly - the hole lands where the round actually went,
-- not where the crosshair is.
local function placeDecal(ox, oy)
	if W.decalCount >= C.weapon.maxDecals then return end
	if not W.decalMat or not camera or not projection or not scene then return end

	local w, h = getWindowSize()
	if w <= 0 or h <= 0 then return end

	local tanHalf = math.tan(math.rad(W.fov()) * 0.5)
	local sx = w * 0.5 + (ox / (tanHalf * (w / h))) * (w * 0.5)
	local sy = h * 0.5 - (oy / tanHalf) * (h * 0.5)

	local size = C.weapon.decalSize
	parkViewmodel(true)
	local ok = placeDecalAtCursor(w, h, sx, sy, camera, projection, scene,
		W.decalMat, Vec3.new(size, size, size))
	parkViewmodel(false)
	if ok then W.decalCount = W.decalCount + 1 end
end

-- ================================ Input ================================

function W.bindInput(input, P)
	input:onMouseButtonPressed(MouseButton.Left, function() W.triggerDown = true end)
	input:onMouseButtonReleased(MouseButton.Left, function() W.triggerDown = false end)
	input:onMouseButtonPressed(MouseButton.Right, function() W.ads = true end)
	input:onMouseButtonReleased(MouseButton.Right, function() W.ads = false end)
	input:onKeyPressed(Key.R, function() W.startReload(P) end)
end

-- =============================== Reload ================================

function W.startReload(P, time)
	local w = C.weapon
	if W.reloading or W.mag >= w.magSize or W.reserve <= 0 then return end
	W.reloading = true
	W.reloadEnd = (time or W.now or 0) + w.reloadTime
	Audio.play("reload", P.x, P.y, P.z)
end

local function finishReload()
	local w = C.weapon
	local want = w.magSize - W.mag
	local take = math.min(want, W.reserve)
	W.mag = W.mag + take
	W.reserve = W.reserve - take
	W.reloading = false
end

-- ================================ Firing ===============================

-- Current cone half-angle in radians: tightest when aimed and still,
-- widest when hip-firing on the move.
local function spread(P)
	local w = C.weapon
	local moving = math.sqrt(P.vx * P.vx + P.vz * P.vz) > 1.2
	local hip = moving and w.spreadMove or w.spreadHip
	return math.rad(hip + (w.spreadADS - hip) * W.adsFactor)
end

local function fire(P, time)
	local w = C.weapon

	if W.mag <= 0 then
		Audio.play("empty", P.x, P.y, P.z)
		W.nextShot = time + 0.25
		return
	end

	W.mag = W.mag - 1
	W.shotsFired = W.shotsFired + 1
	W.nextShot = time + 60.0 / w.rpm

	local ex, ey, ez = P.x, P.eyeY(), P.z
	local fx, fy, fz = P.forward()

	-- Perturb the ray inside the cone: an orthonormal basis around the
	-- view direction, then a uniform disc sample on it. sqrt() on the
	-- radius keeps the distribution even rather than centre-heavy.
	local ux, uy, uz = 0, 1, 0
	local rx, ry, rz = fy * uz - fz * uy, fz * ux - fx * uz, fx * uy - fy * ux
	local rl = math.sqrt(rx * rx + ry * ry + rz * rz)
	if rl < 1e-5 then rx, ry, rz, rl = 1, 0, 0, 1 end
	rx, ry, rz = rx / rl, ry / rl, rz / rl
	local vx, vy, vz = ry * fz - rz * fy, rz * fx - rx * fz, rx * fy - ry * fx

	local ang = spread(P)
	local rad = math.tan(ang) * math.sqrt(math.random())
	local th = math.random() * math.pi * 2
	local ox, oy = math.cos(th) * rad, math.sin(th) * rad

	local dx = fx + rx * ox + vx * oy
	local dy = fy + ry * ox + vy * oy
	local dz = fz + rz * ox + vz * oy
	local dl = math.sqrt(dx * dx + dy * dy + dz * dz)
	dx, dy, dz = dx / dl, dy / dl, dz / dl

	-- Nearest of: an enemy, or the static world.
	local enemyT, enemy, headshot = Enemies.rayCast(ex, ey, ez, dx, dy, dz, w.range)
	local worldT, nx, ny, nz = Level.rayCast(ex, ey, ez, dx, dy, dz, w.range)

	if enemyT and (not worldT or enemyT < worldT) then
		local dmg = w.damage * (headshot and w.headshotMul or 1)
		Enemies.damage(enemy, dmg, dx, dy, dz, time)
		W.hits = W.hits + 1
		FX.bloodHit(ex + dx * enemyT, ey + dy * enemyT, ez + dz * enemyT, -dx, -dy, -dz)
		Audio.play("hit", ex + dx * enemyT, ey + dy * enemyT, ez + dz * enemyT)
	elseif worldT then
		FX.sparkHit(ex + dx * worldT, ey + dy * worldT, ez + dz * worldT, nx, ny, nz)
		placeDecal(ox, oy)
	end

	-- Recoil: mostly up, with a smaller random horizontal component so a
	-- held burst walks off the target instead of climbing in a line.
	P.addRecoil(w.recoilKick * (0.8 + 0.4 * math.random()),
		w.recoilYaw * (math.random() * 2 - 1))

	-- Muzzle flash light, placed at the actual muzzle rather than on the
	-- view axis: offset along the camera's right and up by wherever the
	-- viewmodel currently is, so aiming down the sights walks the flash
	-- into the centre of the screen the way it should.
	local vm = C.viewmodel
	local t = W.adsFactor
	local mx = vm.hip.x + (vm.ads.x - vm.hip.x) * t
	local my = vm.hip.y + (vm.ads.y - vm.hip.y) * t
	local mz = vm.hip.z + (vm.ads.z - vm.hip.z) * t + MUZZLE_Z

	W.flashUntil = time + w.muzzleTime
	W.flashGO:setPosition(Vec3.new(
		ex + rx * mx + vx * my - fx * mz,
		ey + ry * mx + vy * my - fy * mz,
		ez + rz * mx + vz * my - fz * mz))
	W.flashGO:refreshTransformation()
	W.flashLight:setLightIntensity(4.5)

	-- Kick the gun back and up; W.update decays it.
	W.vmRecoil = math.min(1.6, W.vmRecoil + 1.0)
	W.vmFlashUntil = time + w.muzzleTime

	Audio.play("shoot", ex, ey, ez)
end

-- ================================ Update ===============================

function W.update(dt, time, P)
	local w = C.weapon
	W.now = time

	local target = W.ads and 1 or 0
	W.adsFactor = W.adsFactor + (target - W.adsFactor) * math.min(w.adsSpeed * dt, 1)
	P.adsFactor = W.adsFactor

	if W.reloading and time >= W.reloadEnd then finishReload() end

	if P.alive and W.triggerDown and not W.reloading and time >= W.nextShot then
		fire(P, time)
	end

	-- Auto-reload the moment the magazine runs dry, so the player is
	-- never left clicking on empty without knowing why.
	if W.mag <= 0 and not W.reloading and W.reserve > 0 and P.alive then
		W.startReload(P, time)
	end

	if time >= W.flashUntil then
		W.flashLight:setLightIntensity(0)
	end

	W.updateViewmodel(dt, time, P)
end

function W.fov()
	local w = C.weapon
	return w.hipFov + (w.adsFov - w.hipFov) * W.adsFactor
end

function W.addAmmo(rounds)
	W.reserve = math.min(C.weapon.reserve, W.reserve + rounds)
end

function W.reset()
	W.mag = C.weapon.magSize
	W.reserve = C.weapon.reserve
	W.reloading = false
	W.triggerDown = false
	W.ads = false
	W.adsFactor = 0
	W.shotsFired = 0
	W.hits = 0
	if W.flashLight then W.flashLight:setLightIntensity(0) end
end

function W.destroy()
	if W.flashLight then W.flashLight:setLightIntensity(0) end
	W.flashGO = nil
	W.flashLight = nil
	W.vmParts = nil
	W.vmFlash = nil
	W.decalMat = nil
	W.keep = {}
end

return W
