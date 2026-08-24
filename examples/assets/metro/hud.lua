-- ****************************************************************
-- METRO - the HUD.
--
-- Drawn from the component's drawOverlay hook through the launcher's
-- foreground draw list, so it sits over the 3D view at absolute pixel
-- coordinates rather than inside the launcher's control panel.
--
-- Laid out from the screen corners: the run at top-left, vitals at
-- bottom-left, the weapon at bottom-right, and anything the player has
-- to read *now* (wave banners, damage, low battery) across the middle.
-- ****************************************************************

local C = import("config")

local H = {}

-- The DemoLauncher's own control window occupies the left edge, and the
-- foreground draw list this HUD uses paints straight over it. Left-hand
-- readouts start clear of it rather than on top of the demo list.
local LEFT = 300

local function bar(x, y, w, h, frac, r, g, b, alpha)
	frac = math.max(0, math.min(1, frac))
	imgui.rectFilled(x, y, x + w, y + h, 0, 0, 0, 0.45 * alpha)
	if frac > 0 then
		imgui.rectFilled(x + 1, y + 1, x + 1 + (w - 2) * frac, y + h - 1, r, g, b, alpha)
	end
	imgui.rect(x, y, x + w, y + h, r * 0.6, g * 0.6, b * 0.6, 0.8 * alpha, 1.0)
end

local function centered(text, y, r, g, b, a)
	local w, h = imgui.displaySize()
	local tw = imgui.textSize(text)
	imgui.textAt((w - tw) * 0.5, y, r, g, b, a, text)
	return h
end

function H.draw(P, W, Game, time)
	if not imgui or not imgui.displaySize then return end

	local sw, sh = imgui.displaySize()
	imgui.drawCrosshair()

	-- ======================= full-screen states =======================

	-- Damage vignette: a red wash that spikes on a hit and fades. This is
	-- the only feedback that a crawler behind you is landing hits, since
	-- there is no view-kick on damage.
	if P.hitFlash and P.hitFlash > 0 then
		local a = P.hitFlash * 0.32
		imgui.rectFilled(0, 0, sw, sh, 0.55, 0.02, 0.02, a)
	end

	-- Health also tints the edges permanently once the player is hurt
	-- badly enough, so a low-health run reads at a glance.
	local hpFrac = P.health / C.player.maxHealth
	if hpFrac < 0.35 then
		local pulse = 0.5 + 0.5 * math.sin(time * 4.0)
		local a = (0.35 - hpFrac) / 0.35 * 0.30 * (0.6 + 0.4 * pulse)
		imgui.rectFilled(0, 0, sw, 90, 0.5, 0.0, 0.0, a)
		imgui.rectFilled(0, sh - 90, sw, sh, 0.5, 0.0, 0.0, a)
	end

	-- ============================ the run =============================

	imgui.textAt(LEFT, 22, 0.80, 0.84, 0.90, 0.95,
		string.format("WAVE %d", Game.wave))
	imgui.textAt(LEFT, 40, 0.55, 0.58, 0.64, 0.9,
		string.format("SCORE %d      KILLS %d", Game.score, Game.kills))

	if Game.state == "wave" then
		local left = math.max(0, Game.waveTotal - Game.waveKilled)
		imgui.textAt(LEFT, 58, 0.85, 0.40, 0.35, 0.9,
			string.format("CONTACTS %d / %d", left, Game.waveTotal))
	end

	-- ============================= vitals =============================

	local by = sh - 78
	imgui.textAt(LEFT, by, 0.62, 0.66, 0.72, 0.9, "HEALTH")
	bar(LEFT, by + 18, 220, 12, hpFrac,
		hpFrac > 0.5 and 0.35 or 0.85, hpFrac > 0.5 and 0.85 or 0.25, 0.40, 0.9)
	imgui.textAt(LEFT + 228, by + 17, 0.75, 0.78, 0.82, 0.9,
		tostring(math.floor(P.health + 0.5)))

	local batFrac = P.battery / C.flashlight.battery
	imgui.textAt(LEFT, by + 38, 0.62, 0.66, 0.72, 0.9,
		P.flashOn and "FLASHLIGHT" or "FLASHLIGHT  (OFF)")
	bar(LEFT, by + 56, 220, 8, batFrac, 0.35, 0.72, 1.00, 0.9)
	imgui.textAt(LEFT + 228, by + 52, 0.75, 0.78, 0.82, 0.9,
		string.format("%d%%", math.floor(P.battery + 0.5)))

	-- ============================= weapon =============================

	local ammo
	if W.reloading then
		ammo = "RELOADING"
	else
		ammo = string.format("%d / %d", W.mag, W.reserve)
	end
	local aw = imgui.textSize(ammo)
	local lowMag = (not W.reloading) and W.mag <= 5
	imgui.textAt(sw - 28 - aw, sh - 52,
		lowMag and 0.95 or 0.85, lowMag and 0.35 or 0.88, lowMag and 0.25 or 0.92, 0.95, ammo)

	local nw = imgui.textSize(C.weapon.name)
	imgui.textAt(sw - 28 - nw, sh - 72, 0.55, 0.58, 0.64, 0.85, C.weapon.name)

	if W.reloading then
		local frac = 1 - (W.reloadEnd - time) / C.weapon.reloadTime
		bar(sw - 168, sh - 32, 140, 6, frac, 0.85, 0.75, 0.35, 0.9)
	end

	-- ========================== centre banners ========================

	if Game.state == "briefing" or Game.state == "rest" or Game.state == "dead" then
		local dead = (Game.state == "dead")
		centered(Game.message, sh * 0.36,
			dead and 0.90 or 0.85, dead and 0.20 or 0.88, dead and 0.18 or 0.92, 0.95)
		centered(Game.submessage, sh * 0.36 + 22, 0.60, 0.62, 0.68, 0.85)
		if dead then
			centered("[ENTER] to go back down", sh * 0.36 + 52, 0.75, 0.78, 0.82, 0.9)
		end
	end

	if Game.notice then
		centered(Game.notice, sh * 0.62, 0.85, 0.85, 0.60, 0.9)
	end

	if P.battery <= 0 then
		centered("BATTERY DEAD", sh * 0.68, 0.85, 0.35, 0.30, 0.9)
	elseif P.battery < C.flashlight.lowAt and P.flashOn then
		if math.sin(time * 5.0) > 0 then
			centered("BATTERY LOW", sh * 0.68, 0.85, 0.65, 0.25, 0.8)
		end
	end

	-- ============================= controls ===========================
	-- Only while the mouse is loose - once captured the player is playing
	-- and does not need the key list burned into the middle of the frame.

	if not P.captured then
		local lines = {
			"[TAB] capture the mouse and play",
			"WASD move   SHIFT sprint   CTRL crouch   SPACE jump",
			"LMB fire   RMB aim   R reload   F flashlight",
		}
		for i, s in ipairs(lines) do
			centered(s, sh * 0.78 + (i - 1) * 18, 0.70, 0.72, 0.78, 0.85)
		end
	end
end

return H
