-- Boots METRO as a normal DemoLauncher scene script: loads
-- assets/metro/main.lua and drives its init/update/drawOverlay/destroy
-- against the host's scene/camera/projection globals.
--
-- Mirrors neonpulse.lua; the only addition is drawOverlay, which the
-- launcher calls outside its own ImGui window so the HUD can draw over
-- the 3D view.
local Metro = class('Metro')

function Metro:initialize()
	self.time = 0
	self.screenW = nil
	self.screenH = nil
end

function Metro:init(owner)
	self.owner = owner

	if not EXAMPLES_PATH then
		error("Metro: EXAMPLES_PATH global missing (DemoLauncher should set it)")
	end

	GAME_PATH = EXAMPLES_PATH .. "/assets/metro/"
	if not ASSETS_PATH then
		ASSETS_PATH = EXAMPLES_PATH .. "/assets/"
	end

	local w, h = getWindowSize()
	SCREEN_W, SCREEN_H = w, h
	self.screenW, self.screenH = w, h

	local chunk, err = loadfile(GAME_PATH .. "main.lua")
	if not chunk then error("Metro: cannot load main.lua: " .. tostring(err)) end
	chunk()

	-- Explicit refs: the bare init/update/destroy globals main.lua defines
	-- collide with this component's own methods under middleclass.
	self.gameInit = init
	self.gameUpdate = update
	self.gameResize = resize
	self.gameOverlay = drawOverlay
	self.gameDestroy = destroy

	self.gameInit()
end

-- `dt` arrives as the frame delta already - see WireLuaComponentLifecycle.
function Metro:update(dt)
	if not self.gameUpdate then return end
	self.time = self.time + dt

	local w, h = getWindowSize()
	if w ~= self.screenW or h ~= self.screenH then
		self.screenW, self.screenH = w, h
		SCREEN_W, SCREEN_H = w, h
		if self.gameResize then self.gameResize(w, h) end
	end

	self.gameUpdate(self.time, dt)
end

function Metro:drawOverlay()
	if self.gameOverlay then self.gameOverlay() end
end

function Metro:destroy()
	if self.gameDestroy then self.gameDestroy() end
	self.gameInit = nil
	self.gameUpdate = nil
	self.gameResize = nil
	self.gameOverlay = nil
	self.gameDestroy = nil
end

function Metro:serialize()
	return {}
end

function Metro.deserialize(data)
	return Metro:new()
end

return Metro
