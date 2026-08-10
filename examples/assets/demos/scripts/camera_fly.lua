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

-- Yaw and pitch composed as quaternions, yaw first, exactly as
-- BaseExample::LookTo does in C++.
--
-- This was owner:setRotation(Vec3(pitch, yaw, 0)) - handing the two angles
-- over as Euler and letting SetRotation compose them in its own order. That
-- applies pitch about the *world* X axis rather than the camera's own right
-- axis, so the pitch response depends on where you are facing: measured by
-- sweeping yaw and comparing the view direction at pitch +30 and -30, dir.y
-- went -0.5, -0.35, 0.00, +0.35, +0.5 across yaw 0..180. Past 90 degrees of
-- yaw, looking up looked down; at exactly 90 and 270, pitch did nothing at
-- all - world X had swung onto the view axis, which is gimbal lock.
--
-- Composing the two rotations explicitly and handing the result over as
-- Euler gives a constant response at every yaw. The 0 is RotationOrder::XYZ
-- and has to be passed: sol does not apply C++ default arguments, so
-- getEulerRotation() with none raises.
function CameraFly:applyRotation()
	local qPitch = Quaternion.new()
	local qYaw = Quaternion.new()
	qPitch:axisToQuaternion(Vec3.new(1, 0, 0), math.rad(self.pitch))
	qYaw:axisToQuaternion(Vec3.new(0, 1, 0), math.rad(self.yaw))
	self.owner:setRotation((qYaw * qPitch):getEulerRotation(0))
	-- Degrees, for anything mirroring this camera (island water reflection).
	cameraFlyYaw = self.yaw
	cameraFlyPitch = self.pitch
end

function CameraFly:init(owner)
	self.owner = owner
	camera = owner
	if allowMouseCapture == nil then allowMouseCapture = true end
	self.captured = false
	if setMouseCaptured then setMouseCaptured(false) end

	-- Apply yaw/pitch from serialized data / scene rotation seed.
	self:applyRotation()

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
				self:applyRotation()
				warpMouseToCenter()
				-- math.floor, NOT w * 0.5, and this is the whole reason the
				-- camera drifted. warpMouseToCenter() targets
				-- (int)(Width / 2) - integer division - so on an odd width
				-- (1905) the cursor lands at 952 while w * 0.5 records
				-- 952.5. Every event after that carries a phantom -0.5
				-- delta, and the camera keeps rotating on its own with the
				-- mouse still. BaseExample.cpp hit exactly this and its
				-- mouseCenter comment spells it out; the Lua port did not
				-- carry the fix over.
				local w, h = getWindowSize()
				self.lastMouseX = math.floor(w / 2)
				self.lastMouseY = math.floor(h / 2)
				-- No ignoreNextMouseDelta here. The warp generates a motion
				-- event at exactly the position just recorded, so it already
				-- produces a zero delta and is ignored by the dx/dy test
				-- above. Setting the flag as well discarded the *next real*
				-- movement instead, which is why look felt like it dropped
				-- input. C++ LookTo() does not set it either - only the
				-- resize path does, where the jump is real.
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
