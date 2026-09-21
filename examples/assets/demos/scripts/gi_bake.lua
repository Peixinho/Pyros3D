-- Bakes the scene's global illumination once, on the first update.
--
-- Not in init(): the renderer is created by RenderHost.setup and the
-- scene's own transforms have not necessarily settled when components
-- initialise, and a volume sized from unsettled bounds covers the wrong
-- space. One frame later everything exists and has moved where it is
-- going to be.
local GIBake = class('GIBake')

function GIBake:initialize()
	self.done = false
end

function GIBake:init(owner)
	self.owner = owner
end

function GIBake:update(time)
	if self.done then return end
	if renderer == nil or scene == nil then return end
	self.done = true

	-- counts is probes per axis. Small on purpose: irradiance is smooth,
	-- and every probe is 128 rays traced against the BVH on the CPU, so
	-- this is where bake time goes. passes re-traces with a rotated ray
	-- set and averages, which buys noise reduction more cheaply than
	-- raising the ray count does.
	local ok = renderer:bakeGI(scene, {
		counts  = { 7, 6, 7 },
		rays    = 160,
		passes  = 6,
		sky     = { 0.0, 0.0, 0.0 },   -- closed room: nothing outside it
	})
	if ok then
		print("CornellGI: global illumination baked")
	else
		print("CornellGI: bake failed - falling back to flat ambient")
	end
end

return GIBake
