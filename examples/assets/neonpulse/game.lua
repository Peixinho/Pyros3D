-- ****************************************************************
-- NEON PULSE - rules, collision and the state machine.
-- ****************************************************************

local C = import("config")
local Arena = import("arena")
local E = import("entities")
local Audio = import("audio")

local Game = {}

local abs, min, max = math.abs, math.min, math.max
local sin, cos, sqrt = math.sin, math.cos, math.sqrt

local function clamp(v, lo, hi)
	if v < lo then return lo end
	if v > hi then return hi end
	return v
end

-- ****************************** Game state *****************************

Game.state = "title"
Game.stateTime = 0
Game.score = 0
Game.lives = C.score.lives
Game.level = 1
Game.remaining = 0
Game.speedScale = 1
Game.slowTimer = 0
Game.shake = 0

-- Held-key flags. The input bridge is event-driven (press/release), so
-- continuous movement means tracking the held state ourselves.
Game.keys = { left = false, right = false }
Game.mouseX = nil       -- world x requested by the mouse, nil until it moves

function Game.setState(state)
	Game.state = state
	Game.stateTime = 0
end

-- ****************************** Bootstrapping **************************

function Game.start()
	Game.score = 0
	Game.lives = C.score.lives
	Game.level = 1
	Game.speedScale = 1
	Game.slowTimer = 0
	Game.startLevel()
end

function Game.startLevel()
	Game.remaining = E.loadLevel(Game.level)
	for _, p in ipairs(E.powerups) do E.despawnPowerup(p) end
	for _, b in ipairs(E.balls) do E.deactivateBall(b) end
	E.setPaddleWidth(1)
	E.paddle.wideTimer = 0
	Game.slowTimer = 0
	E.movePaddle(0)
	Game.setState("serve")
end

function Game.ballSpeed()
	local s = C.ball.baseSpeed * Game.speedScale
	if Game.slowTimer > 0 then s = s * C.ball.slowMul end
	return min(s, C.ball.maxSpeed)
end

function Game.launch()
	local speed = Game.ballSpeed()
	-- Always launch upward, with a slight lean so the first bounce is not
	-- a perfectly vertical rally.
	local ang = (math.random() - 0.5) * 0.5
	E.activateBall(E.balls[1],
		E.paddle.x,
		C.paddle.y + E.paddle.halfH + C.ball.radius + 0.5,
		sin(ang) * speed, cos(ang) * speed)
	Audio.playAt("launch", E.paddle.x, C.paddle.y)
	Game.setState("play")
end

-- ******************************* Collision *****************************

-- Ball (circle) against an axis-aligned box, as the box expanded by the
-- ball radius. Returns the side that was crossed and how deep, or nil.
--
-- The expanded-box test treats the ball as a square near a box corner.
-- At this ball-to-brick size ratio the difference is well under a pixel
-- on screen, and it has the property that matters here: it never reports
-- a side the ball did not actually come from, which is what makes
-- corner hits behave predictably.
local function hitBox(cx, cy, r, bx, by, hw, hh)
	local dx, dy = cx - bx, cy - by
	local px = hw + r - abs(dx)
	local py = hh + r - abs(dy)
	if px <= 0 or py <= 0 then return nil end
	if px < py then
		return (dx < 0) and "left" or "right", px
	end
	return (dy < 0) and "bottom" or "top", py
end

-- Keep the ball from settling into a near-horizontal rally it can never
-- escape, without changing its speed.
local function enforceVertical(b)
	local speed = sqrt(b.vx * b.vx + b.vy * b.vy)
	if speed <= 0 then return end
	local minVY = speed * C.ball.minVertical
	if abs(b.vy) < minVY then
		b.vy = (b.vy < 0 and -minVY or minVY)
		local vx2 = speed * speed - b.vy * b.vy
		b.vx = (b.vx < 0 and -1 or 1) * sqrt(max(vx2, 0))
	end
end

function Game.hitBrick(brick, ball)
	brick.hp = brick.hp - 1
	local rowColor = C.color.rows[brick.row + 1]

	if brick.hp > 0 then
		-- Armoured brick surviving its first hit: flash, then settle into
		-- its real row colour so its remaining hit is readable.
		brick.mat:setColor(C.color.flash)
		brick.flash = C.bricks.flashTime
		E.burst(brick.x, brick.y, C.color.armoured, 0.6)
		Audio.playAt("armoured", brick.x, brick.y)
		Game.score = Game.score + C.score.armouredBonus
		Game.shake = max(Game.shake, 0.5)
		return
	end

	brick.alive = false
	brick.pop = C.bricks.popTime
	Game.remaining = Game.remaining - 1
	Game.score = Game.score + C.score.perRow[brick.row + 1]
	Game.shake = max(Game.shake, 1.0)
	E.burst(brick.x, brick.y, rowColor, 1.0)
	-- Higher rows ring higher: digging up through the wall reads as rising
	-- pitch, which makes progress audible as well as visible.
	Audio.playAt("brick", brick.x, brick.y, 0.92 + 0.05 * (C.bricks.rows - brick.row))

	Game.speedScale = min(Game.speedScale * C.ball.speedPerHit, C.ball.maxSpeed / C.ball.baseSpeed)

	if math.random() < C.powerup.chance then
		E.spawnPowerup(brick.x, brick.y)
	end
end

-- Advances one ball by dt, resolving every collision along the way.
-- Returns false if the ball fell out of the arena.
function Game.stepBall(b, dt)
	local a = C.arena
	local r = C.ball.radius

	-- Re-scale to the current target speed so power-ups and the per-brick
	-- ramp take effect without changing direction.
	local speed = sqrt(b.vx * b.vx + b.vy * b.vy)
	local want = Game.ballSpeed()
	if speed > 0.001 and abs(speed - want) > 0.5 then
		b.vx, b.vy = b.vx / speed * want, b.vy / speed * want
		speed = want
	end

	-- Substep so a fast ball cannot step straight through a brick.
	local steps = math.ceil((speed * dt) / (r * 0.7))
	steps = clamp(steps, 1, 8)
	local sdt = dt / steps

	for _ = 1, steps do
		b.x = b.x + b.vx * sdt
		b.y = b.y + b.vy * sdt

		-- Side and top walls
		if b.x < -a.halfW + r then
			b.x = -a.halfW + r; b.vx = abs(b.vx)
			Audio.playAt("wall", b.x, b.y)
		elseif b.x > a.halfW - r then
			b.x = a.halfW - r; b.vx = -abs(b.vx)
			Audio.playAt("wall", b.x, b.y)
		end
		if b.y > a.topY - r then
			b.y = a.topY - r; b.vy = -abs(b.vy)
			Audio.playAt("wall", b.x, b.y)
		end

		-- Lost below the dead line
		if b.y < a.deadY then
			return false
		end

		-- Paddle. Only ever deflects a descending ball, so a ball that
		-- clips the paddle's side on the way up is not yanked back down.
		if b.vy < 0 then
			local side = hitBox(b.x, b.y, r, E.paddle.x, C.paddle.y, E.paddle.halfW, E.paddle.halfH)
			if side then
				local t = clamp((b.x - E.paddle.x) / (E.paddle.halfW + r), -1, 1)
				local ang = t * C.paddle.maxBounce
				b.vx = sin(ang) * speed
				b.vy = cos(ang) * speed
				b.y = C.paddle.y + E.paddle.halfH + r + 0.02
				E.burst(b.x, C.paddle.y + E.paddle.halfH, C.color.paddleGlow, 0.45)
				Game.shake = max(Game.shake, 0.35)
				-- Pitch follows the contact point, so an edge hit (which
				-- throws the steepest angle) sounds different from a centre one.
				Audio.playAt("paddle", b.x, C.paddle.y, 1.0 + t * 0.18)
			end
		end

		-- Bricks. One per substep: resolving two in the same step would
		-- reflect the ball twice and send it back the way it came.
		for _, brick in ipairs(E.bricks) do
			if brick.alive then
				local side, depth = hitBox(b.x, b.y, r, brick.x, brick.y, brick.halfW, brick.halfH)
				if side then
					if side == "left" then
						b.x = b.x - depth; b.vx = -abs(b.vx)
					elseif side == "right" then
						b.x = b.x + depth; b.vx = abs(b.vx)
					elseif side == "bottom" then
						b.y = b.y - depth; b.vy = -abs(b.vy)
					else
						b.y = b.y + depth; b.vy = abs(b.vy)
					end
					Game.hitBrick(brick, b)
					break
				end
			end
		end

		enforceVertical(b)
	end

	b.go:setPosition(Vec3.new(b.x, b.y, a.ballZ))
	return true
end

-- ******************************* Power-ups *****************************

function Game.applyPowerup(kind)
	Game.score = Game.score + C.score.powerupCaught
	if kind == "wide" then
		E.setPaddleWidth(C.paddle.wideMul)
		E.paddle.wideTimer = C.powerup.wideTime
	elseif kind == "slow" then
		Game.slowTimer = C.powerup.slowTime
	elseif kind == "multi" then
		-- Split off up to two extra balls, fanned out from a live one.
		local source
		for _, b in ipairs(E.balls) do
			if b.active then source = b; break end
		end
		if source then
			local speed = Game.ballSpeed()
			-- Angles are measured from straight up, matching how the paddle
			-- bounce builds its velocity.
			local base = math.atan(source.vx, source.vy)
			local spread = { 0.55, -0.55 }
			local slot = 1
			for _, b in ipairs(E.balls) do
				if not b.active and slot <= #spread then
					local ang = base + spread[slot]
					E.activateBall(b, source.x, source.y, sin(ang) * speed, cos(ang) * speed)
					slot = slot + 1
				end
			end
		end
	end
end

function Game.updatePowerups(dt)
	for _, p in ipairs(E.powerups) do
		if p.active then
			p.y = p.y - C.powerup.fallSpeed * dt
			p.spin = p.spin + dt * 3.0
			p.go:setPosition(Vec3.new(p.x, p.y, C.arena.playZ))
			p.go:setRotation(Vec3.new(p.spin * 0.7, p.spin, 0))

			local half = C.powerup.size * 0.5
			local caughtX = abs(p.x - E.paddle.x) < (E.paddle.halfW + half)
			local caughtY = abs(p.y - C.paddle.y) < (E.paddle.halfH + half)
			if caughtX and caughtY then
				E.burst(p.x, p.y, C.color.powerup[p.kind], 1.2)
				Game.shake = max(Game.shake, 0.8)
				Audio.playAt("powerup", p.x, p.y)
				Game.applyPowerup(p.kind)
				E.despawnPowerup(p)
			elseif p.y < C.arena.deadY then
				E.despawnPowerup(p)
			end
		end
	end
end

-- ********************************* Input *******************************

function Game.bindInput()
	G.input = Input.new()

	G.input:onKeyPressed(Key.Left,  function() Game.keys.left = true end)
	G.input:onKeyPressed(Key.A,     function() Game.keys.left = true end)
	G.input:onKeyReleased(Key.Left, function() Game.keys.left = false end)
	G.input:onKeyReleased(Key.A,    function() Game.keys.left = false end)

	G.input:onKeyPressed(Key.Right,  function() Game.keys.right = true end)
	G.input:onKeyPressed(Key.D,      function() Game.keys.right = true end)
	G.input:onKeyReleased(Key.Right, function() Game.keys.right = false end)
	G.input:onKeyReleased(Key.D,     function() Game.keys.right = false end)

	local function confirm()
		if Game.state == "title" then
			Game.start()
		elseif Game.state == "serve" then
			Game.launch()
		elseif Game.state == "over" then
			Game.start()
		end
	end
	G.input:onKeyPressed(Key.Space, confirm)
	G.input:onKeyPressed(Key.Return, confirm)
	G.input:onMouseButtonPressed(MouseButton.Left, confirm)

	-- Mouse steering. Window pixels map onto the arena's playable width;
	-- the edges of the window put the paddle against the walls.
	G.input:onMouseMoved(function(x, y)
		local t = clamp(x / SCREEN_W, 0, 1)
		Game.mouseX = (t - 0.5) * (C.arena.halfW * 2.15)
	end)
end

function Game.updatePaddle(dt)
	local moved = false
	local x = E.paddle.x

	if Game.keys.left ~= Game.keys.right then
		x = x + (Game.keys.left and -1 or 1) * C.paddle.speed * dt
		moved = true
		Game.mouseX = nil
	elseif Game.mouseX then
		x = Game.mouseX
		moved = true
	end

	if moved then E.movePaddle(x) end
end

-- ******************************** HUD text *****************************

local BANNERS = {
	title = { "NEON PULSE", "arrows or mouse to move - space to begin" },
	serve = { "READY", "space to launch" },
	over  = { "GAME OVER", "space to play again" },
}

function Game.updateHUD()
	E.setLabel(E.hud.score, "SCORE " .. Game.score)
	E.setLabel(E.hud.level, "LEVEL " .. Game.level)
	E.setLabel(E.hud.lives, "LIVES " .. Game.lives)

	if Game.state == "clear" then
		E.setLabel(E.hud.banner, "LEVEL " .. Game.level .. " CLEAR")
		E.setLabel(E.hud.hint, nil)
	elseif Game.state == "lost" then
		E.setLabel(E.hud.banner, "BALL LOST")
		E.setLabel(E.hud.hint, Game.lives .. " left")
	else
		local banner = BANNERS[Game.state]
		E.setLabel(E.hud.banner, banner and banner[1] or nil)
		E.setLabel(E.hud.hint, banner and banner[2] or nil)
	end
end

-- ******************************** Camera *******************************

function Game.updateCamera(dt, time)
	Game.shake = max(0, Game.shake - dt * 4.0)
	local base = C.camera.pos
	local s = Game.shake * Game.shake * 2.2
	local ox = (math.random() - 0.5) * s
	local oy = (math.random() - 0.5) * s
	-- A slow drift on top of the shake keeps a still frame from looking
	-- like a screenshot.
	local dx = sin(time * 0.23) * 1.6
	local dy = cos(time * 0.17) * 1.0
	G.camera:setPosition(Vec3.new(base.x + ox + dx, base.y + oy + dy, base.z))
	G.camera:lookAtVec(C.camera.lookAt)
end

-- ******************************** Update *******************************

function Game.update(dt, time)
	Game.stateTime = Game.stateTime + dt

	Game.updatePaddle(dt)

	if E.paddle.wideTimer > 0 then
		E.paddle.wideTimer = E.paddle.wideTimer - dt
		if E.paddle.wideTimer <= 0 then E.setPaddleWidth(1) end
	end
	if Game.slowTimer > 0 then Game.slowTimer = Game.slowTimer - dt end

	if Game.state == "serve" then
		-- Ball rides the paddle until launch.
		local b = E.balls[1]
		if not b.active then
			E.activateBall(b, E.paddle.x, C.paddle.y + E.paddle.halfH + C.ball.radius + 0.5, 0, 0)
		end
		b.x = E.paddle.x
		b.y = C.paddle.y + E.paddle.halfH + C.ball.radius + 0.5
		b.go:setPosition(Vec3.new(b.x, b.y, C.arena.ballZ))

	elseif Game.state == "play" then
		for _, b in ipairs(E.balls) do
			if b.active then
				if not Game.stepBall(b, dt) then
					E.burst(b.x, C.arena.deadY + 4, Vec4.new(0.45, 0.55, 1.0, 1), 1.4)
					E.deactivateBall(b)
					Audio.play("lost")
				end
			end
		end

		if Game.remaining <= 0 then
			for _, b in ipairs(E.balls) do E.deactivateBall(b) end
			Game.shake = 1.4
			Audio.play("levelclear")
			Game.setState("clear")
		elseif E.activeBallCount() == 0 then
			Game.lives = Game.lives - 1
			Game.shake = 1.2
			Game.setState(Game.lives > 0 and "lost" or "over")
		end

	elseif Game.state == "lost" then
		if Game.stateTime > 1.1 then
			E.setPaddleWidth(1)
			E.paddle.wideTimer = 0
			Game.slowTimer = 0
			Game.setState("serve")
		end

	elseif Game.state == "clear" then
		if Game.stateTime > 1.8 then
			Game.level = Game.level + 1
			-- Every full pass through the level list raises the floor speed.
			Game.speedScale = 1 + 0.12 * math.floor((Game.level - 1) / #E.levels)
			Game.startLevel()
		end
	end

	Game.updatePowerups(dt)
	E.updateBricks(dt)
	Game.updateHUD()
	Game.updateCamera(dt, time)
	Arena.update(time)
end

return Game
