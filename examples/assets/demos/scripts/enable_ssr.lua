-- The screen's reflections on, for the lifetime of this GameObject (the renderer's own: setReflections), with the
-- reach of a scene at human scale - as the SSR Test demo asks for them.
local EnableSSR = class('EnableSSR')

function EnableSSR:initialize()
end

function EnableSSR:init(owner)
	if renderer and renderer.setReflections then renderer:setReflections(true, 12.0) end
end

function EnableSSR:destroy()
end

function EnableSSR:serialize()
	return {}
end

function EnableSSR.deserialize(data)
	return EnableSSR:new()
end

return EnableSSR
