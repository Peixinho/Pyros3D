-- Opt the shared DemoLauncher DeferredRenderer into material-aware SSR
-- for the lifetime of the GameObject this is attached to. enableSSR/
-- disableSSR are already bound on DeferredRenderer; DemoLauncher exposes
-- the live instance as the global `renderer` (see DemoLauncher::Init).
-- destroy() must run on demo switch so SSR doesn't leak into the next
-- demo - SceneSerializer::UnloadScene alone never calls IComponent::
-- Destroy(), so DemoLauncher fires it on LuaComponents before unload.
local EnableSSR = class('EnableSSR')

function EnableSSR:initialize()
end

function EnableSSR:init(owner)
	if renderer then
		renderer:enableSSR()
	end
end

function EnableSSR:destroy()
	if renderer then
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
