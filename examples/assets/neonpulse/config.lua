-- ****************************************************************
-- NEON PULSE - tunables and palette.
--
-- The arena lives in the XY plane at z=0: x grows right, y grows up,
-- the camera sits in front of it on +Z looking slightly down. Every
-- number below is in those world units.
-- ****************************************************************

local C = {}

-- ******************************* Renderer ******************************
--
-- "deferred" or "forward". Both are fully supported; the arena is built
-- the same way either side of the switch, only the materials and the
-- light intensities change (see arena.lua).
--
-- Deferred is the interesting one here: the lit geometry goes through the
-- G-buffer and gets Cook-Torrance shading from every ball light at once,
-- while the unlit neon (trim, grid, ball cores, HUD) rides the deferred
-- renderer's forward/transparent pass so it stays full-brightness.
C.renderer = "deferred"

-- DeferredRenderer's second pass is PBR-only: its diffuse lobe is
-- albedo/PI where ForwardRenderer's is a bare Lambert, so the same light
-- lands roughly PI times dimmer. Light intensities are scaled by this on
-- the deferred path to bring the two back into line.
C.deferredLightGain = 3.14159

-- ******************************** Arena ********************************

C.arena = {
	halfW   = 84,     -- playable x range is [-halfW, halfW]
	topY    = 124,    -- ceiling the ball bounces off
	deadY   = -14,    -- below this the ball is lost
	wallT   = 6,      -- wall thickness
	depth   = 12,     -- wall depth, gives the arena a real 3D lip
	-- Back panel sits behind the play plane. The gap is larger than it
	-- needs to be visually, on purpose: the translucent bucket is sorted by
	-- object ORIGIN distance, so a horizontal grid bar centred near the
	-- camera's own height could otherwise score as marginally nearer than a
	-- ball out in the play plane and get drawn over its trail. Pushing the
	-- panel back puts every grid bar unambiguously behind everything that
	-- moves.
	panelZ  = -14,
	playZ   = 0,      -- ...which is where bricks/paddle/walls live
	ballZ   = 2,      -- ball floats slightly in front so it never z-fights
	hudZ    = 6,
}

-- ******************************** Paddle *******************************

C.paddle = {
	y        = 11,
	width    = 30,
	height   = 4.4,
	depth    = 7,
	speed    = 165,   -- units/second under keyboard control
	wideMul  = 1.6,   -- WIDE power-up multiplier
	-- How far off-centre a hit can throw the ball, in radians from
	-- straight up. Anything past ~70 degrees makes the ball crawl
	-- horizontally forever, which is not fun.
	maxBounce = 1.15,
}

-- ********************************* Ball ********************************

C.ball = {
	radius     = 3.0,
	baseSpeed  = 92,
	speedPerHit = 1.006,   -- compounding, per brick destroyed
	maxSpeed   = 190,
	slowMul    = 0.72,     -- SLOW power-up
	maxCount   = 3,        -- pre-allocated ball pool (MULTI power-up)
	-- Refuse to let the ball travel closer than this to horizontal:
	-- a near-flat ball can ping between the side walls for a very long
	-- time without ever threatening a brick.
	minVertical = 0.22,
}

-- ******************************** Bricks *******************************

C.bricks = {
	cols   = 11,
	rows   = 6,
	width  = 13,
	height = 6,
	depth  = 6,
	gapX   = 2.2,
	gapY   = 2.6,
	topRowY = 118,
	-- Rows 0 and 1 (the top two) take two hits. They render in the
	-- armoured material until the first hit knocks them down to their
	-- normal row colour.
	armouredRows = 2,
	popTime = 0.16,   -- destruction shrink, seconds
	flashTime = 0.09, -- hit flash, seconds
}

-- ****************************** Power-ups ******************************

C.powerup = {
	size     = 7,
	fallSpeed = 46,
	chance   = 0.16,   -- probability a destroyed brick drops one
	poolSize = 6,
	wideTime = 14,
	slowTime = 9,
}

-- ******************************* Palette *******************************
--
-- Cool end of the spectrum only: indigo through cyan through violet.
-- Nothing here is warmer than a soft mint, deliberately.

local function rgb(r, g, b, a)
	return Vec4.new(r, g, b, a or 1)
end

C.color = {
	background = rgb(0.020, 0.027, 0.075),
	panel      = rgb(0.055, 0.070, 0.150),
	grid       = rgb(0.075, 0.130, 0.265),
	wall       = rgb(0.110, 0.135, 0.290),
	trim       = rgb(0.35,  0.90,  1.00),
	paddle     = rgb(0.60,  0.92,  1.00),
	paddleGlow = rgb(0.75,  1.00,  1.00),
	ball       = rgb(0.88,  1.00,  1.00),
	ballLight  = rgb(0.45,  0.85,  1.00),
	-- Deliberately near-white: an armoured brick has to be obviously not
	-- one of the six row colours, or you cannot tell at a glance which
	-- bricks still need a second hit.
	armoured   = rgb(0.86,  0.92,  1.00),
	hud        = rgb(0.62,  0.88,  1.00),
	hudDim     = rgb(0.30,  0.44,  0.62),
	flash      = rgb(1.00,  1.00,  1.00),

	-- Top row first: violet cools down into mint as you dig toward the
	-- paddle, so the board reads as a gradient rather than a set of
	-- unrelated colours.
	rows = {
		rgb(0.72, 0.42, 1.00),   -- violet
		rgb(0.48, 0.48, 1.00),   -- periwinkle
		rgb(0.30, 0.58, 1.00),   -- azure
		rgb(0.20, 0.76, 1.00),   -- sky
		rgb(0.18, 0.92, 0.92),   -- cyan
		rgb(0.28, 1.00, 0.72),   -- mint
	},

	powerup = {
		wide  = rgb(0.30, 1.00, 0.78),
		slow  = rgb(0.40, 0.70, 1.00),
		multi = rgb(0.78, 0.48, 1.00),
	},
}

-- ********************************* Audio *******************************

C.audio = {
	master = 0.85,
	-- Effects are played positioned in the arena, so a brick breaking on the
	-- left is heard on the left. The listener sits on the camera, ~165 units
	-- back, so the falloff range has to cover the whole arena or everything
	-- lands at minimum volume: full volume out to `minDistance`, tapering to
	-- the floor at `maxDistance`.
	minDistance = 90,
	maxDistance = 340,
	-- Per-sound trims, so the mix can be balanced without regenerating the
	-- .wav files (see sfx/generate_sfx.py).
	volume = {
		paddle = 0.55, wall = 0.35, brick = 0.60, armoured = 0.50,
		powerup = 0.70, lost = 0.70, levelclear = 0.80, launch = 0.50,
	},
	ambienceVolume = 0.35,
	-- How many of each effect can overlap. Bricks need the most: a ball
	-- ripping along a row triggers several within a few frames.
	voices = { brick = 6, wall = 4, paddle = 3, default = 2 },
}

-- ********************************** HUD ********************************

C.hud = {
	-- How far along the eye ray the HUD is pulled from the arena plane
	-- toward the camera. Purely a draw-order lever - see placeLabel() in
	-- entities.lua for why, and why it does not move the HUD on screen.
	-- Must be small enough that the HUD's origin beats the grid's, and
	-- large enough to stay well clear of the near plane.
	pullIn = 0.55,
}

-- ******************************** Scoring ******************************

C.score = {
	-- Deeper rows are easier to reach, so they are worth less.
	perRow = { 90, 75, 60, 45, 30, 20 },
	armouredBonus = 25,
	powerupCaught = 50,
	lives = 3,
}

-- ******************************** Camera *******************************

C.camera = {
	-- Just enough tilt to show the top faces of the bricks and the paddle.
	-- Much more than this and the arena visibly narrows toward the top,
	-- which costs more in readability than it gains in depth.
	pos     = Vec3.new(0, 72, 165),
	lookAt  = Vec3.new(0, 57, 0),
	fov     = 55,
	near    = 1,
	far     = 600,
	-- Distance from the eye to the play plane, used by resize() to widen
	-- the FOV rather than crop the arena on a narrow window.
	planeDist = 178,
	minHalfW  = 100,
}

return C
