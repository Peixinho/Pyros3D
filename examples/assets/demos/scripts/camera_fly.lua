-- WASD + mouse-look camera. Attached to a Camera GameObject in each
-- demo scene that wants fly controls. Sets the global `camera` so
-- RenderHost.draw / skybox_follow / placeDecalAtCursor see this owner.
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
	self.ignoreNextMouseDelta = false
	self.captured = false
end

function CameraFly:init(owner)
	self.owner = owner
	camera = owner
	if allowMouseCapture == nil then allowMouseCapture = true end
	self.captured = false
	if setMouseCaptured then setMouseCaptured(false) end

	-- Apply yaw/pitch from serialized data / scene rotation seed.
	self.owner:setRotation(Vec3.new(math.rad(self.pitch), math.rad(self.yaw), 0))
	-- Stash degrees for reflection cameras (stable; avoid GetRotation Euler).
	cameraFlyYaw = self.yaw
	cameraFlyPitch = self.pitch

	local input = Input.new()
	self.input = input

	input:onKeyPressed(Key.W, function() self.moveFront = true end)
	input:onKeyReleased(Key.W, function() self.moveFront = false end)
	input:onKeyPressed(Key.S, function() self.moveBack = true end)
	input:onKeyReleased(Key.S, function() self.moveBack = false end)
	input:onKeyPressed(Key.A, function() self.strafeLeft = true end)
	input:onKeyReleased(Key.A, function() self.strafeLeft = false end)
	input:onKeyPressed(Key.D, function() self.strafeRight = true end)
	input:onKeyReleased(Key.D, function() self.strafeRight = false end)

	input:onKeyPressed(Key.Tab, function()
		if allowMouseCapture == false then return end
		self.captured = not self.captured
		setMouseCaptured(self.captured)
		if self.captured then
			warpMouseToCenter()
			self.lastMouseX = nil
			self.lastMouseY = nil
			self.ignoreNextMouseDelta = true
		else
			self.lastMouseX = nil
			self.lastMouseY = nil
			self.moveFront = false
			self.moveBack = false
			self.strafeLeft = false
			self.strafeRight = false
		end
	end)

	input:onMouseMoved(function(x, y)
		if not self.captured then
			self.lastMouseX = nil
			self.lastMouseY = nil
			return
		end
		if self.ignoreNextMouseDelta then
			self.ignoreNextMouseDelta = false
			self.lastMouseX = x
			self.lastMouseY = y
			return
		end
		if self.lastMouseX ~= nil then
			local dx = x - self.lastMouseX
			local dy = y - self.lastMouseY
			if dx ~= 0 or dy ~= 0 then
				self.yaw = self.yaw - dx / 10.0
				self.pitch = self.pitch - dy / 10.0
				if self.pitch < -80 then self.pitch = -80 end
				if self.pitch > 80 then self.pitch = 80 end
				self.owner:setRotation(Vec3.new(math.rad(self.pitch), math.rad(self.yaw), 0))
				cameraFlyYaw = self.yaw
				cameraFlyPitch = self.pitch
				warpMouseToCenter()
				local w, h = getWindowSize()
				self.lastMouseX = w * 0.5
				self.lastMouseY = h * 0.5
				self.ignoreNextMouseDelta = true
				return
			end
		end
		self.lastMouseX = x
		self.lastMouseY = y
	end)
end

function CameraFly:update(time)
	if self.lastTime == nil then self.lastTime = time end
	local dt = time - self.lastTime
	self.lastTime = time

	if not self.captured and allowMouseCapture ~= false then return end

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

function CameraFly:destroy()
	self.input = nil
	if camera == self.owner then camera = nil end
end

function CameraFly:serialize()
	return { yaw = self.yaw, pitch = self.pitch }
end

function CameraFly.deserialize(data)
	local o = CameraFly:new()
	if data then
		if data.yaw then o.yaw = data.yaw end
		if data.pitch then o.pitch = data.pitch end
	end
	return o
end

return CameraFly
