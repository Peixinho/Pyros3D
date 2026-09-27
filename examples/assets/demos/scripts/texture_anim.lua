-- Makes sure the cube's TextureAnimation is playing, and spins the owner on
-- Y (spin_y.lua's job) so one attach covers both.
--
-- It must NOT tick the animation or push frames onto the material itself:
-- RenderingComponent::Update has done both since 2026-08-31, from the
-- scene's absolute clock. This script used to do it too, from its own
-- accumulated clock, so the animation saw two different times every frame
-- and its elapsed time jumped between them - a static first frame, three
-- frames, or all six far too fast, depending on how far apart the clocks
-- happened to be that run.
local TextureAnim = class('TextureAnim')
local dTime = 0

function TextureAnim:initialize()
end

function TextureAnim:init(owner)
	self.owner = owner
	local rc = owner:getComponent("RenderingComponent")
	if not rc then return end

	self.instance = rc:getActiveTextureAnimation()
	if self.instance then
		-- Ensure looping playback (JSON repeat=0 → Play(0) already loops,
		-- but re-play in case init order left the instance stopped).
		if self.instance.play and not self.instance:isPlaying() then
			self.instance:play(0)
		end
	end
end

function TextureAnim:update(time)
	dTime = time + dTime
	if self.owner then
		local r = self.owner:getRotation()
		self.owner:setRotation(Vec3.new(r.x, dTime, r.z))
	end
end

function TextureAnim:serialize()
	return {}
end

function TextureAnim.deserialize(data)
	return TextureAnim:new()
end

return TextureAnim
