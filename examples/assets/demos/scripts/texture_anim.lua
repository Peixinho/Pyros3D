-- Ticks a RenderingComponent's active TextureAnimation and pushes the
-- current frame onto the mesh material's color map - same per-frame work
-- the original RotatingTextureAnimatedCube C++ demo did manually. Also
-- spins the owner on Y (spin_y.lua's job) so one attach covers both.
local TextureAnim = class('TextureAnim')

function TextureAnim:initialize()
end

function TextureAnim:init(owner)
	self.owner = owner
	local rc = owner:getComponent("RenderingComponent")
	if not rc then return end

	self.instance = rc:getActiveTextureAnimation()
	if self.instance then
		self.anim = self.instance:getOwner()
		-- Ensure looping playback (JSON repeat=0 → Play(0) already loops,
		-- but re-play in case init order left the instance stopped).
		if self.instance.play and not self.instance:isPlaying() then
			self.instance:play(0)
		end
	end

	-- getMeshes() must be the no-arg binding (sol ignores C++ defaults).
	-- Prefer getMeshes(); fall back to getMeshesLOD(0) on older binaries.
	local ok, meshes = pcall(function() return rc:getMeshes() end)
	if not ok or not meshes then
		ok, meshes = pcall(function() return rc:getMeshesLOD(0) end)
	end
	if ok and meshes and #meshes > 0 and meshes[1].getGenericMaterial then
		self.material = meshes[1]:getGenericMaterial()
	end
end

function TextureAnim:update(time)
	if self.anim then
		self.anim:update(time)
	end
	if self.instance and self.material then
		local tex = self.instance:getTexture()
		if tex then
			self.material:setColorMap(tex)
		end
	end
	if self.owner then
		local r = self.owner:getRotation()
		self.owner:setRotation(Vec3.new(r.x, time, r.z))
	end
end

function TextureAnim:serialize()
	return {}
end

function TextureAnim.deserialize(data)
	return TextureAnim:new()
end

return TextureAnim
