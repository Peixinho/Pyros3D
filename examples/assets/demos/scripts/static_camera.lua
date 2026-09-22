-- Registers the global `camera` and does nothing else.
--
-- The demo host renders through whatever `camera` points at, and
-- camera_fly.lua is what normally sets it - so a scene that drops the
-- fly controls to get a fixed viewpoint silently loses its camera and
-- renders black. This is the two lines of camera_fly that actually
-- matter, without the mouse look that makes a comparison shot
-- different every run.
local StaticCamera = class('StaticCamera')

function StaticCamera:initialize() end

function StaticCamera:init(owner)
	camera = owner
	-- Positioned here rather than relying on the transform in the scene
	-- file: whatever the loader does with a Camera root's position, it
	-- was not surviving to the first frame - the view came up at the
	-- origin, inside the box. Setting it explicitly is unambiguous.
	-- 23 was framing a box whose walls were accidentally twice their
	-- intended size (Cube takes HALF-extents, and CornellGI.json was
	-- passing full ones). With that fixed the room is 10 units across
	-- and this is the distance that frames it.
	owner:setPosition(Vec3.new(0.0, 0.0, 15.0))
end

return StaticCamera
