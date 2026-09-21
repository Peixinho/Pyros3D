-- Drives DDGI for the Cornell scene: one initial solve, then a bounded
-- refresh every frame.
--
-- The per-frame part is the point. The light below moves, and the
-- indirect light follows it - the red wall's bounce slides across the
-- floor as the light crosses the room. Nothing here is precomputed
-- except the BVH, which is rebuilt only when the geometry changes
-- (never, in this scene).
--
-- Why a probe budget rather than all of them: tracing 294 probes x 160
-- rays every frame is not affordable on the CPU. 12 per frame is, and
-- every probe still refreshes on a fixed 25-frame cycle - so the
-- response lags by a few hundred milliseconds rather than not
-- happening. That budget is what the compute port raises.
local GIDrive = class('GIDrive')

function GIDrive:initialize()
	self.ready = false
	self.t = 0.0
end

function GIDrive:init(owner)
	self.owner = owner
end

function GIDrive:update(time)
	if renderer == nil or scene == nil then return end

	if not self.ready then
		self.ready = true
		-- The initial solve, so the first visible frame is already lit
		-- rather than converging up from black.
		local ok = renderer:bakeGI(scene, {
			counts = { 7, 6, 7 },
			rays   = 160,
			passes = 6,
			sky    = { 0.0, 0.0, 0.0 },
		})
		print(ok and "CornellGI: initial solve done" or "CornellGI: solve failed")
		self.t = time
		return
	end

	-- Move the light. This component is attached TO the light, so
	-- `owner` is it - SceneGraph has no lookup-by-name in Lua, and
	-- putting the driver on the thing it drives needs none.
	--
	-- GI is not told about this in any way. UpdateSceneGI re-reads the
	-- scene's lights on every refresh, so the bounce follows on its own.
	if self.owner then
		local a = (time - self.t) * 0.6
		self.owner:setPosition(Vec3.new(math.sin(a) * 3.2, 4.0, math.cos(a) * 2.0))
	end

	-- 12 probes per frame, hysteresis 0.9: fast enough to visibly
	-- follow the light, damped enough not to boil.
	renderer:updateGI(scene, 12, 0.9)
end

return GIDrive
