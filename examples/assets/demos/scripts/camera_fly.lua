-- DemoLauncher's persistent shell camera behavior - attached once to the
-- launcher's own FPSCamera GameObject (never part of any demo's saved
-- scene), so this only ever runs once for the whole app lifetime. WASD
-- movement + mouse-look, functionally equivalent to BaseExample's C++
-- FPS camera, but real Lua behavior per the "move it all to Lua" goal
-- instead of a launcher-owned C++ camera controller. Registers real
-- input callbacks via Input.new() - safe here specifically because this
-- script is never re-attached/re-loaded across demo switches (unlike
-- per-demo content), so there's no callback-accumulation risk.
local CameraFly = class('CameraFly')

function CameraFly:initialize()
	self.moveFront = false
	self.moveBack = false
	self.strafeLeft = false
	self.strafeRight = false
	self.lastMouseX = nil
	self.lastMouseY = nil
	self.yaw = 0
	self.pitch = 0
	self.lastTime = nil
end

function CameraFly:init(owner)
	print("DEBUG: CameraFly:init called")
	self.owner = owner

	local input = Input.new()
	self.input = input

	input:onKeyPressed(Key.W, function() print("DEBUG: W pressed"); self.moveFront = true end)
	input:onKeyReleased(Key.W, function() self.moveFront = false end)
	input:onKeyPressed(Key.S, function() self.moveBack = true end)
	input:onKeyReleased(Key.S, function() self.moveBack = false end)
	input:onKeyPressed(Key.A, function() self.strafeLeft = true end)
	input:onKeyReleased(Key.A, function() self.strafeLeft = false end)
	input:onKeyPressed(Key.D, function() self.strafeRight = true end)
	input:onKeyReleased(Key.D, function() self.strafeRight = false end)

	input:onMouseMoved(function(x, y)
		if self.lastMouseX ~= nil then
			local dx = x - self.lastMouseX
			local dy = y - self.lastMouseY
			self.yaw = self.yaw - dx / 10.0
			self.pitch = self.pitch - dy / 10.0
			if self.pitch < -80 then self.pitch = -80 end
			if self.pitch > 80 then self.pitch = 80 end
			self.owner:setRotation(Vec3.new(math.rad(self.pitch), math.rad(self.yaw), 0))
		end
		self.lastMouseX = x
		self.lastMouseY = y
	end)
end

function CameraFly:update(time)
	if self.lastTime == nil then self.lastTime = time end
	local dt = time - self.lastTime
	self.lastTime = time

	local speed = dt * 20.0
	local dir = self.owner:getDirection()
	local pos = self.owner:getPosition()
	local move = Vec3.new(0, 0, 0)

	if self.moveFront then move = move - dir * speed end
	if self.moveBack then move = move + dir * speed end

	local right = dir:cross(Vec3.new(0, 1, 0)):normalize()
	if self.strafeLeft then move = move + right * speed end
	if self.strafeRight then move = move - right * speed end

	self.owner:setPosition(pos + move)
end

return CameraFly
