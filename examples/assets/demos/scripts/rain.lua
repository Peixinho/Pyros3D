-- Rain over the scene, as a game sets it (setPostEffectParam("Sky", "uRain", amount) - Erangel's weather does): what
-- faces up out in it soaks and gathers puddles; what is under the roof stays dry.
local Rain = class('Rain')
function Rain:initialize() end
function Rain:init(owner)
	if setPostEffectParam then
		setPostEffectParam("Sky", "uRain", 0.85)
		setPostEffectParam("Sky", "uCloudCover", 0.9)
	end
end
function Rain:destroy()
	if setPostEffectParam then setPostEffectParam("Sky", "uRain", 0) setPostEffectParam("Sky", "uCloudCover", -1) end
end
function Rain:serialize() return {} end
function Rain.deserialize(data) return Rain:new() end
return Rain
