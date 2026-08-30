-- ****************************************************************
-- METRO - the HUD.
--
-- Real UI now, not immediate-mode drawing: the elements live in
-- Metro.json as a UICanvas of UIImage/UIText components, authored
-- where they can be seen and edited, and this file only pushes state
-- into them. That is the whole difference from the previous version,
-- which drew the HUD every frame into ImGui's foreground draw list at
-- absolute pixel coordinates and was therefore correct at exactly one
-- window size, invisible to the editor, and impossible to save.
--
-- Anchors do the layout, so nothing here computes a position. The two
-- bars are a track with a fill inside it, and ui.setFill moves one
-- anchor - "0.62 of the way across my parent" - which is why the
-- vitals need no arithmetic either.
-- ****************************************************************

local C = import("config")

local H = {}

-- Resolved once. ui.find walks every canvas in the scene by name, and
-- doing that for twenty elements every frame would be the one wasteful
-- thing in an otherwise free HUD.
local E = nil

local function bind()
	local names = {
		"HitFlash", "EdgeTop", "EdgeBottom",
		"Wave", "Score", "Contacts",
		"HealthLabel", "HealthBar", "HealthBarFill", "HealthValue",
		"BatteryLabel", "BatteryBar", "BatteryBarFill", "BatteryValue",
		"WeaponName", "Ammo", "ReloadBar", "ReloadBarFill",
		"Message", "Submessage", "Prompt", "Notice", "BatteryWarn",
		"Controls1", "Controls2", "Controls3",
	}
	local t = {}
	for _, n in ipairs(names) do
		local e = ui.find(scene, n)
		if not e then return nil end
		t[n] = e
	end
	return t
end

-- A text element with nothing to say is hidden rather than set to "":
-- an empty string still costs a mesh rebuild every time it changes.
local function say(el, s)
	if s and s ~= "" then
		ui.setVisible(el, true)
		ui.setText(el, s)
	else
		ui.setVisible(el, false)
	end
end

local function wash(el, r, g, b, a)
	if a and a > 0.001 then
		ui.setVisible(el, true)
		ui.setTint(el, Vec4.new(r, g, b, a))
	else
		ui.setVisible(el, false)
	end
end

function H.draw(P, W, Game, time)
	if not ui or not ui.find then return end
	if not E then
		E = bind()
		if not E then return end   -- scene has no HUD canvas
	end

	if imgui and imgui.drawCrosshair then imgui.drawCrosshair() end

	-- ======================= full-screen states =======================

	-- Damage vignette: the only feedback that something behind you is
	-- landing hits, since there is no view-kick on damage.
	wash(E.HitFlash, 0.55, 0.02, 0.02, (P.hitFlash or 0) * 0.32)

	local hpFrac = P.health / C.player.maxHealth
	local edge = 0
	if hpFrac < 0.35 then
		local pulse = 0.5 + 0.5 * math.sin(time * 4.0)
		edge = (0.35 - hpFrac) / 0.35 * 0.30 * (0.6 + 0.4 * pulse)
	end
	wash(E.EdgeTop, 0.5, 0.0, 0.0, edge)
	wash(E.EdgeBottom, 0.5, 0.0, 0.0, edge)

	-- ============================ the run =============================

	say(E.Wave, string.format("WAVE %d", Game.wave))
	say(E.Score, string.format("SCORE %d      KILLS %d", Game.score, Game.kills))
	if Game.state == "wave" then
		local left = math.max(0, Game.waveTotal - Game.waveKilled)
		say(E.Contacts, string.format("CONTACTS %d / %d", left, Game.waveTotal))
	else
		say(E.Contacts, nil)
	end

	-- ============================= vitals =============================

	ui.setFill(E.HealthBarFill, hpFrac)
	ui.setTint(E.HealthBarFill, hpFrac > 0.5
		and Vec4.new(0.35, 0.85, 0.40, 0.9)
		or  Vec4.new(0.85, 0.25, 0.40, 0.9))
	say(E.HealthValue, tostring(math.floor(P.health + 0.5)))

	local batFrac = P.battery / C.flashlight.battery
	ui.setFill(E.BatteryBarFill, batFrac)
	say(E.BatteryLabel, P.flashOn and "FLASHLIGHT" or "FLASHLIGHT  (OFF)")
	say(E.BatteryValue, string.format("%d%%", math.floor(P.battery + 0.5)))

	-- ============================= weapon =============================

	say(E.WeaponName, C.weapon.name)
	if W.reloading then
		say(E.Ammo, "RELOADING")
		ui.setTextColor(E.Ammo, Vec4.new(0.85, 0.88, 0.92, 0.95))
		ui.setVisible(E.ReloadBar, true)
		ui.setFill(E.ReloadBarFill, 1 - (W.reloadEnd - time) / C.weapon.reloadTime)
	else
		say(E.Ammo, string.format("%d / %d", W.mag, W.reserve))
		local low = W.mag <= 5
		ui.setTextColor(E.Ammo, low
			and Vec4.new(0.95, 0.35, 0.25, 0.95)
			or  Vec4.new(0.85, 0.88, 0.92, 0.95))
		ui.setVisible(E.ReloadBar, false)
	end

	-- ========================== centre banners ========================

	local banner = (Game.state == "briefing" or Game.state == "rest" or Game.state == "dead")
	if banner then
		local dead = (Game.state == "dead")
		say(E.Message, Game.message)
		ui.setTextColor(E.Message, dead
			and Vec4.new(0.90, 0.20, 0.18, 0.95)
			or  Vec4.new(0.85, 0.88, 0.92, 0.95))
		say(E.Submessage, Game.submessage)
		say(E.Prompt, dead and "[ENTER] to go back down" or nil)
	else
		say(E.Message, nil); say(E.Submessage, nil); say(E.Prompt, nil)
	end

	say(E.Notice, Game.notice)

	if P.battery <= 0 then
		say(E.BatteryWarn, "BATTERY DEAD")
		ui.setTextColor(E.BatteryWarn, Vec4.new(0.85, 0.35, 0.30, 0.9))
	elseif P.battery < C.flashlight.lowAt and P.flashOn and math.sin(time * 5.0) > 0 then
		say(E.BatteryWarn, "BATTERY LOW")
		ui.setTextColor(E.BatteryWarn, Vec4.new(0.85, 0.65, 0.25, 0.9))
	else
		say(E.BatteryWarn, nil)
	end

	-- ============================= controls ===========================
	-- Only while the mouse is loose - once captured the player is playing
	-- and does not need the key list burned into the middle of the frame.

	if not P.captured then
		say(E.Controls1, "[TAB] capture the mouse and play")
		say(E.Controls2, "WASD move   SHIFT sprint   CTRL crouch   SPACE jump")
		say(E.Controls3, "LMB fire   RMB aim   R reload   F flashlight")
	else
		say(E.Controls1, nil); say(E.Controls2, nil); say(E.Controls3, nil)
	end
end

return H
