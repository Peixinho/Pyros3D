-- Opt the shared DemoLauncher DeferredRenderer into material-aware SSR
-- for the lifetime of this GameObject. Matches standalone SSRTest:
-- EnableSSR + the proven human-scale march distances (0.35 / 12).
local EnableSSR = class('EnableSSR')

function EnableSSR:initialize()
end

function EnableSSR:init(owner)
	if not renderer then return end
	if renderer.enableSSR then renderer:enableSSR() end
	if renderer.setSSRDistances then renderer:setSSRDistances(0.35, 12.0) end
end

function EnableSSR:destroy()
	if renderer and renderer.disableSSR then
		renderer:disableSSR()
	end
end

function EnableSSR:serialize()
	return {}
end

function EnableSSR.deserialize(data)
	return EnableSSR:new()
end

return EnableSSR
