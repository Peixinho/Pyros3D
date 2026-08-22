-- Attached to SkeletonAnimationExample's animated model GameObject. The
-- initial Play() calls (which clips, weights, layers) are already baked
-- into the demo's scene.json and reconstructed automatically by
-- SceneSerializer::LoadScene - this script only drives what the original
-- C++ demo did per-frame: ticking the SkeletonAnimation clip container
-- (reached via the RenderingComponent's active SkeletonAnimationInstance
-- - see PyrosBindings' getActiveSkeletonAnimation/getOwner additions),
-- and re-blending the two base-layer animations by mouse X.
local SkeletonAnimBlend = class('SkeletonAnimBlend')

function SkeletonAnimBlend:initialize()
	-- Orders returned by the Play() calls baked into scene.json, in the
	-- same order they were originally played - see the demo's scene
	-- builder for the exact sequence.
	self.animationPos = 0
	self.animationPos2 = 1
	self.mouseX = nil
	-- SkeletonAnimation:update() wants a clock that counts up from the
	-- first call - it derives each clip's position by subtracting the time
	-- it first saw. Scripts are handed the frame delta (see
	-- LuaComponent::Update), so the clock is accumulated here. Per instance,
	-- not a file-local: two animated objects sharing this script would
	-- otherwise both add into the same counter and run at double speed.
	self.clock = 0
end

function SkeletonAnimBlend:init(owner)
	local rc = owner:getComponent("RenderingComponent")
	if rc then
		self.instance = rc:getActiveSkeletonAnimation()
		if self.instance then
			self.anim = self.instance:getOwner()
		end
	end

	local input = Input.new()
	input:onMouseMoved(function(x, y)
		self.mouseX = x
	end)
end

function SkeletonAnimBlend:update(dt)
	self.clock = self.clock + dt
	if self.anim then
		self.anim:update(self.clock)
	end

	if self.instance and self.mouseX then
		local windowWidth = 1280.0
		local t = self.mouseX / windowWidth
		if t < 0 then t = 0 end
		if t > 1 then t = 1 end

		local progress = self.instance:getAnimationCurrentProgress(self.animationPos2)
		self.instance:changeProperties(self.animationPos, progress, -1, 1.0, 1.0 - t)
		self.instance:changeProperties(self.animationPos2, progress, -1, 1.0, t)
	end
end

-- Required for scriptFile/data (hence real behavior) to survive a save/
-- load round trip at all - see SceneSerializer.cpp's LuaComponent save
-- case.
function SkeletonAnimBlend:serialize()
	return { animationPos = self.animationPos, animationPos2 = self.animationPos2 }
end

function SkeletonAnimBlend.deserialize(data)
	local inst = SkeletonAnimBlend:new()
	-- The engine's bindings take these as integers; math.tointeger keeps
	-- that true even for a scene saved before the serializer preserved
	-- Lua's integer subtype (they come back as 0.0/1.0 there).
	inst.animationPos = math.tointeger(data.animationPos) or 0
	inst.animationPos2 = math.tointeger(data.animationPos2) or 1
	return inst
end

return SkeletonAnimBlend
