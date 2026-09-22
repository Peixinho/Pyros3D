-- Slides the block from side to side, so the demo shows indirect light
-- following geometry rather than only following a light.
--
-- This is the moving-geometry path: the block's triangles are
-- re-transformed and the BVH refitted each time it moves, so the soft
-- shadow it casts on the floor - which is entirely indirect light -
-- travels with it. Before that existed the block would have slid
-- across a shadow that stayed where the block used to be.
local SlideBlock = class('SlideBlock')

function SlideBlock:initialize()
	-- `time` in update() is the frame DELTA, not a clock. Anything
	-- that wants elapsed time has to add it up itself - see the note
	-- in update().
	self.elapsed = 0.0
end

function SlideBlock:init(owner)
	self.owner = owner
	self.home = owner:getPosition()
end

function SlideBlock:update(time)
	if self.owner == nil then return end
	-- `time` is the delta for this frame (~0.016), NOT the absolute
	-- clock. Three demo scripts in this folder assumed otherwise and
	-- were silently frozen: `(time - self.t)` is about zero every
	-- frame, so the sine never advanced and the "watch it move" demos
	-- did not move.
	self.elapsed = self.elapsed + time
	local a = self.elapsed * 0.6
	self.owner:setPosition(Vec3.new(self.home.x + math.sin(a) * 2.2,
	                                self.home.y,
	                                self.home.z))
end

return SlideBlock
