-- Boots Neon Pulse as a normal DemoLauncher scene script: loads
-- assets/neonpulse/main.lua and drives its init/update/destroy against
-- the host's scene/renderer/camera/projection globals (see main.lua's
-- hosted path). No custom C++ - SwitchDemo LoadScene + LuaComponent only.
local NeonPulse = class('NeonPulse')
local dTime = 0

function NeonPulse:initialize()
	self.lastTime = nil
	self.screenW = nil
	self.screenH = nil
end

function NeonPulse:init(owner)
	self.owner = owner

	if not EXAMPLES_PATH then
		error("NeonPulse: EXAMPLES_PATH global missing (DemoLauncher should set it)")
	end

	GAME_PATH = EXAMPLES_PATH .. "/assets/neonpulse/"
	if not ASSETS_PATH then
		ASSETS_PATH = EXAMPLES_PATH .. "/assets/"
	end

	local w, h = getWindowSize()
	SCREEN_W, SCREEN_H = w, h
	self.screenW, self.screenH = w, h

	local chunk, err = loadfile(GAME_PATH .. "main.lua")
	if not chunk then error("NeonPulse: cannot load main.lua: " .. tostring(err)) end
	chunk()

	-- Keep explicit refs - bare init/update/destroy names collide with
	-- this component's own methods under middleclass.
	self.gameInit = init
	self.gameUpdate = update
	self.gameResize = resize
	self.gameDestroy = destroy

	self.gameInit()
	self.lastTime = nil
end

-- `dt` arrives as the frame delta already - see WireLuaComponentLifecycle.
function NeonPulse:update(dt)
  dTime = dt + dTime

	local w, h = getWindowSize()
	if w ~= self.screenW or h ~= self.screenH then
		self.screenW, self.screenH = w, h
		SCREEN_W, SCREEN_H = w, h
		if self.gameResize then self.gameResize(w, h) end
	end

	self.gameUpdate(dTime, dt)
end

function NeonPulse:destroy()
	if self.gameDestroy then self.gameDestroy() end
	self.gameInit = nil
	self.gameUpdate = nil
	self.gameResize = nil
	self.gameDestroy = nil
	self.lastTime = nil
end

function NeonPulse:serialize()
	return {}
end

function NeonPulse.deserialize(data)
	return NeonPulse:new()
end

return NeonPulse
