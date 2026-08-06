-- No-op camera controller (NeonPulse owns view via shake/drift).
-- Still registers global `camera` so RenderHost.draw has a viewpoint.
local CameraNone = class('CameraNone')

function CameraNone:initialize()
end

function CameraNone:init(owner)
	self.owner = owner
	camera = owner
	allowMouseCapture = false
	if setMouseCaptured then setMouseCaptured(false) end
end

function CameraNone:update(time)
end

function CameraNone:destroy()
	if camera == self.owner then camera = nil end
end

function CameraNone:serialize()
	return {}
end

function CameraNone.deserialize(data)
	return CameraNone:new()
end

return CameraNone
