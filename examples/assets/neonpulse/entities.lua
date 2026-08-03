-- ****************************************************************
-- NEON PULSE - everything that moves, plus the HUD.
--
-- Every dynamic object in the game is pre-allocated here at startup and
-- then recycled: balls, bricks, power-ups and the impact-burst emitters
-- are all fixed-size pools. Nothing is added to or removed from the
-- SceneGraph after build() returns.
--
-- That is not just tidiness. Objects created from Lua are owned by sol,
-- so an object the SceneGraph still points at must stay reachable from
-- Lua forever; pooling makes that automatic instead of something every
-- call site has to remember.
-- ****************************************************************

local C = import("config")
local Arena = import("arena")

local E = { keep = {} }

-- Somewhere well behind the far plane. Inactive pool members live here,
-- where the frustum cull drops them before they cost a draw call.
local PARK_Z = -4000

local function keep(obj)
	E.keep[#E.keep + 1] = obj
	return obj
end

-- ****************************** Brick layouts **************************
--
-- One string per row, one character per column: '.' empty, 'x' normal,
-- 'X' armoured (two hits). Rows are listed top-down, matching the screen.
-- Levels cycle once the list runs out, with the ball a little faster each
-- time round.

E.levels = {
	{ -- 1 - SOLID: learn the controls against a full wall
		"XXXXXXXXXXX",
		"XXXXXXXXXXX",
		"xxxxxxxxxxx",
		"xxxxxxxxxxx",
		"xxxxxxxxxxx",
		"xxxxxxxxxxx",
	},
	{ -- 2 - GATE: two channels invite a shot up the side
		"XX.XXXXX.XX",
		"XX.XXXXX.XX",
		"xxxxxxxxxxx",
		"x.x.x.x.x.x",
		"xxxxxxxxxxx",
		".xx.xxx.xx.",
	},
	{ -- 3 - PRISM: the armoured tip is the last thing you can reach
		".....X.....",
		"....XXX....",
		"...xxxxx...",
		"..xxxxxxx..",
		".xxxxxxxxx.",
		"xxxxxxxxxxx",
	},
	{ -- 4 - WEAVE: nothing but gaps, the ball rattles for a long time
		"X.X.X.X.X.X",
		".x.x.x.x.x.",
		"X.X.X.X.X.X",
		".x.x.x.x.x.",
		"xxx.xxx.xxx",
		".x.x.x.x.x.",
	},
}

-- ******************************** Build ********************************

function E.build()
	E.buildPaddle()
	E.buildBalls()
	E.buildBricks()
	E.buildPowerups()
	E.buildFX()
	E.buildHUD()
end

-- -------------------------------- Paddle -------------------------------

function E.buildPaddle()
	local p = C.paddle
	local a = C.arena

	local go = GameObject.new()
	local mesh = keep(Arena.cube(p.width, p.height, p.depth))
	local mat = Arena.lit(C.color.paddle, 70)
	local rc = RenderingComponent.new(mesh, mat)
	go:addComponent(rc)
	go:setPosition(Vec3.new(0, p.y, a.playZ))
	G.scene:add(go)
	keep(go); keep(rc)

	-- A flat unlit bar sitting on the paddle's top face: the surface the
	-- ball actually bounces off reads as a lit strip, which makes the
	-- contact point obvious at speed.
	local glowGO = GameObject.new()
	local glowMesh = keep(Arena.cube(p.width - 2, 1.1, p.depth + 0.6))
	local glowRC = RenderingComponent.new(glowMesh, Arena.unlit(C.color.paddleGlow))
	glowGO:addComponent(glowRC)
	glowGO:setPosition(Vec3.new(0, p.height * 0.5 + 0.2, 0))
	go:addGameObject(glowGO)
	keep(glowGO); keep(glowRC)

	E.paddle = {
		go = go, glow = glowGO,
		x = 0, halfW = p.width * 0.5, halfH = p.height * 0.5,
		scale = 1, wideTimer = 0,
	}
end

function E.setPaddleWidth(scale)
	E.paddle.scale = scale
	E.paddle.halfW = C.paddle.width * 0.5 * scale
	E.paddle.go:setScale(Vec3.new(scale, 1, 1))
end

function E.movePaddle(x)
	local limit = C.arena.halfW - E.paddle.halfW
	if x < -limit then x = -limit end
	if x > limit then x = limit end
	E.paddle.x = x
	E.paddle.go:setPosition(Vec3.new(x, C.paddle.y, C.arena.playZ))
end

-- --------------------------------- Balls -------------------------------

function E.buildBalls()
	E.balls = {}
	for i = 1, C.ball.maxCount do
		local go = GameObject.new()
		local mesh = keep(Sphere.new(C.ball.radius, 22, 22))
		local rc = RenderingComponent.new(mesh, Arena.unlit(C.color.ball))
		go:addComponent(rc)

		-- The ball carries its own point light. This is the single most
		-- important visual in the game: it rakes across the back panel and
		-- the brick faces as it travels, so the arena is lit by play.
		local light = PointLight.new(C.color.ballLight, 120)
		light:setLightIntensity(1.5 * Arena.lightGain)
		go:addComponent(light)

		-- Additive trail. Emission is world-space, so particles are left
		-- behind rather than dragged along - exactly what a trail wants.
		local d = ParticleSystemDesc.new()
		d.maxParticles = 96
		d.texture = G.sprite
		d.looping = true
		d.emissionRate = 55
		d.burstCount = 1
		d.minLifetime = 0.30
		d.maxLifetime = 0.48
		d.direction = Vec3.new(0, 0, 1)
		d.spreadAngle = 3.14
		d.minSpeed = 0
		d.maxSpeed = 5
		d.gravity = Vec3.new(0, 0, 0)
		d.damping = 1.6
		d.startSize = 5.5
		d.endSize = 0.4
		d.startColor = Vec4.new(0.45, 0.95, 1.0, 1)
		d.endColor = Vec4.new(0.25, 0.35, 1.0, 0)
		d.fadeInFraction = 0.05
		d.fadeOutFraction = 0.35
		d.blendMode = ParticleBlendMode.Additive
		local trail = ParticleSystem.new(d)
		go:addComponent(trail)

		G.scene:add(go)
		keep(go); keep(rc); keep(light); keep(trail); keep(d)

		E.balls[i] = {
			go = go, light = light, trail = trail,
			x = 0, y = 0, vx = 0, vy = 0, active = false,
		}
		E.deactivateBall(E.balls[i])
	end
end

function E.activateBall(b, x, y, vx, vy)
	b.active = true
	b.x, b.y, b.vx, b.vy = x, y, vx, vy
	b.go:setPosition(Vec3.new(x, y, C.arena.ballZ))
	b.light:setLightIntensity(1.5 * Arena.lightGain)
	b.trail:play()
end

function E.deactivateBall(b)
	b.active = false
	b.go:setPosition(Vec3.new(0, 0, PARK_Z))
	b.light:setLightIntensity(0)
	b.trail:stop()
	b.trail:clear()
end

function E.activeBallCount()
	local n = 0
	for _, b in ipairs(E.balls) do if b.active then n = n + 1 end end
	return n
end

-- -------------------------------- Bricks -------------------------------

function E.buildBricks()
	local b = C.bricks
	E.bricks = {}

	local stepX = b.width + b.gapX
	local stepY = b.height + b.gapY
	local originX = -((b.cols - 1) * stepX) * 0.5

	for row = 0, b.rows - 1 do
		for colIdx = 0, b.cols - 1 do
			local go = GameObject.new()
			local mesh = keep(Arena.cube(b.width, b.height, b.depth))
			-- One material per brick, not per row: hit-flashing and the
			-- armoured/normal swap both recolour a single brick, and the
			-- engine caches the compiled shader per usage-flag set, so 66
			-- materials still means one shader.
			local mat = Arena.lit(C.color.rows[row + 1], 34)
			local rc = RenderingComponent.new(mesh, mat)
			go:addComponent(rc)
			G.scene:add(go)
			keep(go); keep(rc)

			E.bricks[#E.bricks + 1] = {
				go = go, mat = mat, row = row,
				x = originX + colIdx * stepX,
				y = b.topRowY - row * stepY,
				halfW = b.width * 0.5, halfH = b.height * 0.5,
				alive = false, hp = 0, flash = 0, pop = 0,
			}
		end
	end
end

function E.loadLevel(index)
	local layout = E.levels[((index - 1) % #E.levels) + 1]
	local b = C.bricks
	local remaining = 0

	-- The pool is stored flat in row-major order, which is also how the
	-- layout strings read, so a single walk lines the two up.
	local i = 1
	for row = 0, b.rows - 1 do
		for colIdx = 0, b.cols - 1 do
			local brick = E.bricks[i]
			local ch = string.sub(layout[row + 1], colIdx + 1, colIdx + 1)
			if ch == "x" or ch == "X" then
				brick.alive = true
				brick.hp = (ch == "X") and 2 or 1
				brick.flash = 0
				brick.pop = 0
				-- Rotation as well as scale: a brick that was destroyed in a
				-- previous level kept the angle its destruction spin left it
				-- at, so every reused pool member came back visibly askew.
				brick.go:setScale(Vec3.new(1, 1, 1))
				brick.go:setRotation(Vec3.new(0, 0, 0))
				brick.go:setPosition(Vec3.new(brick.x, brick.y, C.arena.playZ))
				brick.mat:setColor(brick.hp > 1 and C.color.armoured or C.color.rows[row + 1])
				remaining = remaining + 1
			else
				brick.alive = false
				brick.pop = 0
				brick.go:setPosition(Vec3.new(0, 0, PARK_Z))
			end
			i = i + 1
		end
	end

	return remaining
end

function E.updateBricks(dt)
	for _, brick in ipairs(E.bricks) do
		if brick.flash > 0 then
			brick.flash = brick.flash - dt
			if brick.flash <= 0 then
				brick.mat:setColor(brick.hp > 1 and C.color.armoured or C.color.rows[brick.row + 1])
			end
		end
		if brick.pop > 0 then
			brick.pop = brick.pop - dt
			if brick.pop <= 0 then
				-- Park it clean, so the pool member carries no leftover
				-- transform into whatever level reuses it next.
				brick.go:setScale(Vec3.new(1, 1, 1))
				brick.go:setRotation(Vec3.new(0, 0, 0))
				brick.go:setPosition(Vec3.new(0, 0, PARK_Z))
			else
				-- Shrink and spin out rather than blinking off.
				local t = brick.pop / C.bricks.popTime
				brick.go:setScale(Vec3.new(t, t, t))
				brick.go:setRotation(Vec3.new(0, 0, (1 - t) * 2.4))
			end
		end
	end
end

-- ------------------------------- Power-ups -----------------------------

E.POWERUP_KINDS = { "wide", "slow", "multi" }

function E.buildPowerups()
	E.powerups = {}
	local s = C.powerup.size
	for i = 1, C.powerup.poolSize do
		local go = GameObject.new()
		local mesh = keep(Arena.cube(s, s, s))
		local mat = Arena.lit(C.color.powerup.wide, 90)
		local rc = RenderingComponent.new(mesh, mat)
		go:addComponent(rc)
		go:setPosition(Vec3.new(0, 0, PARK_Z))
		G.scene:add(go)
		keep(go); keep(rc)
		E.powerups[i] = { go = go, mat = mat, kind = "wide", x = 0, y = 0, spin = 0, active = false }
	end
end

function E.spawnPowerup(x, y)
	for _, p in ipairs(E.powerups) do
		if not p.active then
			p.kind = E.POWERUP_KINDS[math.random(#E.POWERUP_KINDS)]
			p.mat:setColor(C.color.powerup[p.kind])
			p.x, p.y, p.spin, p.active = x, y, 0, true
			p.go:setPosition(Vec3.new(x, y, C.arena.playZ))
			return p
		end
	end
	return nil
end

function E.despawnPowerup(p)
	p.active = false
	p.go:setPosition(Vec3.new(0, 0, PARK_Z))
end

-- ------------------------------- Impact FX -----------------------------

function E.buildFX()
	E.fx = { pool = {}, next = 1, armed = {} }

	for i = 1, 10 do
		local go = GameObject.new()
		local d = ParticleSystemDesc.new()
		d.maxParticles = 34
		d.texture = G.sprite
		d.looping = false          -- one-shot: every play() fires a burst
		d.burstCount = 26
		d.minLifetime = 0.28
		d.maxLifetime = 0.60
		d.direction = Vec3.new(0, 0, 1)
		d.spreadAngle = 3.14
		d.minSpeed = 26
		d.maxSpeed = 78
		d.gravity = Vec3.new(0, -55, 0)
		d.damping = 1.1
		d.startSize = 6.0
		d.endSize = 0.5
		d.fadeInFraction = 0.02
		d.fadeOutFraction = 0.25
		d.blendMode = ParticleBlendMode.Additive
		local ps = ParticleSystem.new(d)
		go:addComponent(ps)
		G.scene:add(go)
		keep(go); keep(ps); keep(d)
		E.fx.pool[i] = { go = go, ps = ps }
	end
end

-- Queues a burst. The emitter samples its owner's *world* position, which
-- only refreshes when the SceneGraph updates - so the position is set now
-- and play() is deferred to just after this frame's scene update (see
-- E.fireQueuedBursts). Playing it here would burst at last frame's spot.
function E.burst(x, y, color, scale)
	local slot = E.fx.pool[E.fx.next]
	E.fx.next = (E.fx.next % #E.fx.pool) + 1

	slot.go:setPosition(Vec3.new(x, y, C.arena.playZ + 3))
	slot.ps:setColors(color, Vec4.new(color.x * 0.4, color.y * 0.4, color.z * 0.9, 0))
	slot.ps:setSizes(6.0 * (scale or 1), 0.5, 0.35)
	E.fx.armed[#E.fx.armed + 1] = slot
end

function E.fireQueuedBursts()
	for i = 1, #E.fx.armed do
		E.fx.armed[i].ps:play()
		E.fx.armed[i] = nil
	end
end

-- ---------------------------------- HUD --------------------------------

function E.buildHUD()
	local a = C.arena
	G.font = Font.new(ASSETS_PATH .. "verdana.ttf", 48)
	G.font:createText("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,:!?-+*/()[]<>'")

	local mat = GenericShaderMaterial.new(ShaderUsage.TextRendering)
	mat:setTextFont(G.font)
	keep(mat)
	E.hudMat = mat

	-- Every label is created with real, non-empty text on purpose: a Text
	-- built from "" produces zero vertices, and a zero-length geometry
	-- buffer never recovers once it has been sent - the label would stay
	-- blank no matter what it was later updated to. Labels are therefore
	-- hidden by parking the GameObject, never by blanking the string.
	local function label(text, x, y, size, color, centred)
		local mesh = keep(Text.new(G.font, text, size * C.hud.pullIn, size * C.hud.pullIn, color, true))
		local go = GameObject.new()
		local rc = RenderingComponent.new(mesh, mat)
		go:addComponent(rc)
		G.scene:add(go)
		keep(go); keep(rc)
		local entry = {
			go = go, mesh = mesh, text = text, color = color,
			x = x, y = y, size = size, centred = centred, hidden = false,
		}
		E.placeLabel(entry)
		return entry
	end

	E.hud = {
		-- Laid out so the score can reach six digits without running into
		-- the level readout next to it.
		score  = label("SCORE 0", -a.halfW + 3, a.deadY - 6, 8, C.color.hud),
		level  = label("LEVEL 1", -11, a.deadY - 6, 8, C.color.hudDim),
		lives  = label("LIVES 3", a.halfW - 40, a.deadY - 6, 8, C.color.hud),
		banner = label("NEON PULSE", 0, 52, 15, C.color.hud, true),
		hint   = label("space or click to begin", 0, 38, 6, C.color.hudDim, true),
	}
end

-- The HUD is authored in arena-plane coordinates, then pulled toward the
-- camera along the eye ray before being placed.
--
-- Why: the translucent bucket is sorted back-to-front by each object's
-- ORIGIN distance from the camera, not per pixel. The camera looks down at
-- the arena, so the score row - which sits low, below the play area - is
-- genuinely farther from the eye than the grid behind it, and got drawn
-- first and then painted over by the grid lines. Scaling every HUD point
-- about the camera position is a projective no-op (the same eye ray, at a
-- shorter distance), so the HUD lands on exactly the same pixels at
-- exactly the same size, while its origin becomes unambiguously the
-- nearest thing in the scene and it sorts last. Sizes are scaled by the
-- same factor at construction, above.
local function pullToward(x, y, z, k)
	local eye = C.camera.pos
	return Vec3.new(
		eye.x + (x - eye.x) * k,
		eye.y + (y - eye.y) * k,
		eye.z + (z - eye.z) * k)
end

function E.placeLabel(entry)
	local x = entry.x
	if entry.centred then
		-- The glyph mesh advances by each glyph's bitmap width rather than
		-- by a real font advance, so there is no width to query. This
		-- factor is measured against the banner strings actually used
		-- here; it centres them to within a character.
		x = x - #entry.text * entry.size * 0.30
	end
	entry.go:setPosition(pullToward(x, entry.y, C.arena.hudZ, C.hud.pullIn))
end

-- UpdateText rebuilds the glyph mesh, so only call it when the string has
-- genuinely changed - cheap per event, not something to do every frame.
-- Passing nil or "" hides the label rather than emptying it.
function E.setLabel(entry, text)
	if text == nil or text == "" then
		if not entry.hidden then
			entry.hidden = true
			entry.go:setPosition(Vec3.new(0, 0, PARK_Z))
		end
		return
	end

	if entry.text ~= text then
		entry.text = text
		entry.mesh:updateText(text, entry.color)
	elseif not entry.hidden then
		return
	end

	entry.hidden = false
	E.placeLabel(entry)
end

return E
