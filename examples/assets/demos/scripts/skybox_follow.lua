-- Keeps a skybox cube centered on the active demo camera (global
-- `camera`, set by camera_fly / camera_orbit / camera_none on init).
local SkyboxFollow = class('SkyboxFollow')

function SkyboxFollow:initialize()
end

function SkyboxFollow:init(owner)
	self.owner = owner
end

function SkyboxFollow:update(time)
	if camera and self.owner then
		local p = camera:getPosition()
		self.owner:setPosition(p)
	end
end

function SkyboxFollow:serialize()
	return {}
end

function SkyboxFollow.deserialize(data)
	return SkyboxFollow:new()
end

return SkyboxFollow
