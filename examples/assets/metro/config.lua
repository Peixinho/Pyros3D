-- ****************************************************************
-- METRO - tunables, level dimensions and palette.
--
-- One unit is one metre. The station runs along X, the platform is an
-- island in the middle at z ~ 0 with a track trench either side, and Y
-- is up with the platform deck at y = PLATFORM_TOP.
-- ****************************************************************

local C = {}

-- ******************************** Station ******************************
--
--        z = -TRENCH_OUT ................ far wall
--   -TRENCH_IN .. -PLATFORM_HALF   track trench (north)
--   -PLATFORM_HALF .. +PLATFORM_HALF   island platform
--   +PLATFORM_HALF .. +TRENCH_IN   track trench (south)
--        z = +TRENCH_OUT ................ near wall
--
-- The hall is open at both ends of X into tunnel mouths; enemies come
-- out of those and out of the two service doors in the side walls.

C.station = {
	halfLen      = 46,    -- hall spans x in [-halfLen, halfLen]
	platformHalf = 5.0,   -- platform half-width in z
	platformTop  = 1.15,  -- deck height
	trenchIn     = 5.0,   -- trench starts at the platform edge
	trenchOut    = 11.0,  -- and ends at the side wall
	trenchFloor  = -0.55, -- rail bed, below the deck
	ceiling      = 7.6,
	wallTop      = 7.6,
	tunnelMouth  = 46,    -- |x| where the tunnels begin
	tunnelLen    = 26,    -- how far the tunnels run past the mouth
	tunnelHalfH  = 3.4,   -- tunnel bore half-height above the rail bed
}

-- Pillar colonnade down both sides of the platform deck.
C.pillars = {
	z       = 3.7,        -- placed at +/- this
	spacing = 9.0,
	half    = 0.42,       -- half-width of the square section
	firstX  = -40.5,
	count   = 10,
}

-- ******************************** Player *******************************

C.player = {
	spawn      = { x = 0, z = 0 },
	radius     = 0.38,
	height     = 1.80,   -- full standing height
	crouchHeight = 1.15,
	eyeDrop    = 0.16,   -- eye sits this far below the top of the capsule
	walkSpeed  = 4.6,
	sprintSpeed= 7.4,
	crouchSpeed= 2.2,
	accel      = 42.0,   -- ground acceleration, m/s^2
	airAccel   = 6.0,
	friction   = 9.0,
	jumpSpeed  = 4.6,
	gravity    = 18.0,
	stepHeight = 0.45,   -- auto-step onto the platform lip / sleepers
	maxHealth  = 100,
	regenDelay = 6.0,    -- seconds without damage before health ticks back
	regenRate  = 6.0,    -- hp/second, only up to regenCap
	regenCap   = 100,
	mouseSens  = 0.10,   -- degrees per mouse count
	pitchLimit = 85,
	bobRate    = 10.5,
	bobAmount  = 0.045,
}

-- ****************************** Flashlight *****************************
--
-- The player's only reliable light. Battery drains while on, so the
-- station's own broken lighting still matters.

C.flashlight = {
	color      = { 0.92, 0.94, 1.00 },
	intensity  = 1.95,
	radius     = 22.0,
	innerCone  = 11.0,   -- degrees, matches the SpotLight ctor's units
	outterCone = 26.0,
	battery    = 100.0,
	drain      = 1.05,   -- %/second while on
	lowAt      = 25.0,   -- below this the beam starts stuttering
	pickup     = 45.0,   -- % restored by a battery pickup
	-- The beam's shadow map, re-rendered from the player's eye every
	-- frame over the whole station. 768 rather than 1024 because it
	-- is the most expensive single thing in the frame and the beam is
	-- soft enough that the difference does not read.
	shadowSize = 1024,
	-- A wider PCF kernel than one texel. The beam is broad and lands on
	-- walls a couple of metres away, where a single-texel comparison
	-- shows the map's own grid as blocky stair-stepping along every
	-- shadow edge.
	shadowPCF  = 1.6 / 1024.0,
	-- Slope-scaled depth bias, factor/units - the pair the scene
	-- serializer writes, reaching Vulkan as depthBiasSlopeFactor and
	-- depthBiasConstantFactor. Lua-built lights default to 0/0, which is
	-- solid acne.
	--
	-- These are far larger than they look like they should be, and they
	-- were measured, not guessed: the shadow projection is perspective
	-- with a near/far of 1:30, so almost all of the depth range is spent
	-- close to the light and a floor being grazed 15 m away has a tiny
	-- NDC depth slope standing in for a large world-space error. 3.2/4.0
	-- still striped the whole platform; 40/64 was completely clean; 12/16
	-- is the lowest that stayed clean without visibly lifting shadows off
	-- the foot of what casts them.
	shadowBiasFactor = 6.0,
	shadowBiasUnits  = 8.0,
	-- The near plane of the beam's own shadow frustum. Pushed well out
	-- from 0.12: nothing within half a metre of the eye casts (the
	-- weapon has casting disabled), and a tighter near/far ratio is
	-- depth precision, which is what buys the low bias above.
	shadowNear = 2.0,
	-- The beam is mounted on the weapon, not behind the eye. A light
	-- sitting exactly at the camera cannot show you any of its own
	-- shadows - every one of them is hidden directly behind the thing
	-- casting it. Offsetting it to the muzzle gives the cone real
	-- parallax, so a pillar two metres away throws a shadow that slides
	-- across the wall behind it as you move. Right and down only; a
	-- forward offset is what used to push the weapon out of the cone.
	beamRight = 0.17,
	beamUp    = -0.11,
	-- Short-range fill that exists to light the weapon - see
	-- P.buildFlashlight. Radius is under the player's eye height on
	-- purpose, so it barely touches the floor.
	fillRadius = 1.30,
	fillIntensity = 1.5,
}

-- ****************************** Viewmodel ******************************
--
-- The gun in the player's hands. Positions are in the camera's local
-- frame, where -Z is forward, +X right and +Y up.

C.viewmodel = {
	hip      = { x = 0.155, y = -0.150, z = -0.52 },
	ads      = { x = 0.000, y = -0.070, z = -0.44 },
	sprintDrop = 0.055,   -- how far it dips while sprinting
	sprintTilt = 0.30,    -- and how far it rolls out of the way
	recoilBack = 0.055,   -- metres of kick straight back per shot
	recoilRise = 0.16,    -- radians of muzzle rise per shot
	recoilDecay = 13.0,
	swayAmount = 0.010,   -- lateral sway from mouse movement
	swayDecay  = 6.0,
	bobAmount  = 0.014,
	reloadDrop = 0.13,
	reloadTilt = 0.55,
}

-- ******************************** Weapon *******************************

C.weapon = {
	name        = "MP-9",
	damage      = 26,
	headshotMul = 2.5,
	rpm         = 640,           -- rounds per minute
	magSize     = 30,
	reserve     = 180,
	reloadTime  = 1.85,
	range       = 90,
	spreadHip   = 1.7,           -- degrees of cone half-angle
	spreadMove  = 2.6,
	spreadADS   = 0.45,
	recoilKick  = 0.85,          -- degrees of pitch per shot
	recoilYaw   = 0.28,
	recoilDecay = 7.0,
	adsFov      = 48,
	hipFov      = 70,
	adsSpeed    = 9.0,
	muzzleLight = { 1.00, 0.86, 0.55 },
	muzzleTime  = 0.045,
	-- Decals are permanent: placeDecalAtCursor attaches a new
	-- RenderingComponent to whatever it hits and hands back only a bool,
	-- so there is no handle to remove one with. Capped rather than
	-- recycled, which is the honest thing the binding allows.
	maxDecals   = 90,
	decalSize   = 0.16,
}

-- ******************************** Enemies ******************************
--
-- Two shapes of threat: the crawler is fast and weak and comes in
-- numbers, the brute soaks damage and hits hard. Both navigate the
-- platform by steering, not pathfinding - the level is a corridor.

C.enemy = {
	crawler = {
		hp = 60, speed = 3.5, radius = 0.42, height = 1.25,
		damage = 9, attackRange = 1.5, attackEvery = 0.85,
		score = 100, eyeColor = { 1.00, 0.32, 0.22 },
		lightRadius = 4.5, lightIntensity = 0.5,
	},
	brute = {
		hp = 260, speed = 2.1, radius = 0.62, height = 2.15,
		damage = 26, attackRange = 2.1, attackEvery = 1.5,
		score = 350, eyeColor = { 1.00, 0.62, 0.12 },
		lightRadius = 6.5, lightIntensity = 0.8,
	},
	pool = { crawler = 26, brute = 8 },
	separation = 0.85,    -- how hard they push apart from each other
	spawnFlashTime = 0.7, -- the light at a spawn point pulses before they come
}

-- ******************************** Waves ********************************

C.wave = {
	introTime   = 3.2,
	restTime    = 6.0,
	baseCrawler = 5,
	crawlerPer  = 2,
	maxCrawler  = 22,
	bruteFrom   = 3,      -- first wave that can contain brutes
	brutePer    = 0.5,
	maxBrute    = 7,
	spawnGap    = { 0.55, 1.5 },
	liveCap     = 14,     -- at most this many alive at once
	ammoDrop    = 0.22,   -- chance an enemy drops a magazine
	batteryDrop = 0.10,
	medkitDrop  = 0.08,
}

-- ******************************** Palette ******************************
--
-- Everything reads cold and desaturated - wet concrete, dead tile - so
-- the warm sodium lamps and the red emergency strips are the only
-- colour in the frame, and the muzzle flash is the brightest thing in
-- the station.

C.color = {
	concrete   = { 0.25, 0.25, 0.27 },
	concreteDk = { 0.17, 0.17, 0.19 },
	tile       = { 0.36, 0.38, 0.37 },
	tileGrime  = { 0.19, 0.20, 0.19 },
	platformEdge = { 0.62, 0.56, 0.24 },   -- the painted safety line
	rail       = { 0.42, 0.40, 0.38 },
	sleeper    = { 0.16, 0.13, 0.11 },
	ballast    = { 0.14, 0.14, 0.15 },
	steel      = { 0.34, 0.36, 0.39 },
	bench      = { 0.22, 0.17, 0.12 },
	sign       = { 0.10, 0.22, 0.42 },

	lampWarm   = { 1.00, 0.88, 0.66 },
	lampCold   = { 0.74, 0.86, 1.00 },
	emergency  = { 1.00, 0.13, 0.10 },
	tunnelGlow = { 0.20, 0.34, 0.52 },

	crawler    = { 0.34, 0.33, 0.30 },
	brute      = { 0.28, 0.24, 0.22 },
	blood      = { 0.42, 0.06, 0.05 },

	pickupAmmo    = { 0.95, 0.78, 0.25 },
	pickupBattery = { 0.35, 0.85, 1.00 },
	pickupMedkit  = { 0.35, 1.00, 0.45 },
}

-- Ambient is deliberately almost black: the station should be lit only
-- by what is actually in it. RenderHost also applies the manifest's
-- globalLight; this is the value the game re-asserts.
C.ambient = { 0.030, 0.032, 0.042, 1.0 }

-- ******************************** Audio ********************************

C.audio = {
	master     = 0.9,
	minDistance = 6,
	maxDistance = 70,
	volume = {
		shoot = 0.34, reload = 0.5, empty = 0.4, hit = 0.5,
		enemyHit = 0.5, enemyDie = 0.6, enemyAttack = 0.6,
		hurt = 0.7, pickup = 0.55, wave = 0.7, gameover = 0.8,
		spawn = 0.5, step = 0.22,
	},
	ambienceVolume = 0.32,
	voices = { shoot = 6, enemyHit = 5, default = 3 },
}

-- ******************************** Camera *******************************

C.camera = {
	fov  = 70,
	near = 0.08,
	far  = 260,
}

return C
