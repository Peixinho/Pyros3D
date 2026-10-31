-- Ambient occlusion: a grid of teapots on a floor, the renderer's ground-truth AO (GTAO) darkening the creases
-- between them and where they stand - always on, from the depth prepass; nothing for the scene to set.
local SSAOSetup = class('SSAOSetup')

function SSAOSetup:initialize()
	self.owned = {}
	self.keep = {}
end

function SSAOSetup:init(owner)
	self.owner = owner
	if not scene or not ASSETS_PATH then return end

	local usage = ShaderUsage.Diffuse
	local teapot = Model.new(ASSETS_PATH .. "teapotLOD1.p3dm", false)
	self.keep[#self.keep + 1] = teapot
	for j = 0, 9 do
		for i = 0, 9 do
			local go = GameObject.new()
			local rc = RenderingComponent.new(teapot, usage)
			go:addComponent(rc)
			go:setPosition(Vec3.new(-5 + i, 0.4, -5 + j))
			go:setScale(Vec3.new(0.01, 0.01, 0.01))
			go:setRotation(Vec3.new(0, math.rad(33), 0))
			scene:add(go)
			self.owned[#self.owned + 1] = go
			self.keep[#self.keep + 1] = rc
		end
	end

	local floor = Plane.new(10, 10)
	local gFloor = GameObject.new()
	gFloor:setRotation(Vec3.new(math.rad(-90), 0, 0))
	local rFloor = RenderingComponent.new(floor, usage)
	gFloor:addComponent(rFloor)
	scene:add(gFloor)
	self.owned[#self.owned + 1] = gFloor
	self.keep[#self.keep + 1] = floor
	self.keep[#self.keep + 1] = rFloor
end

function SSAOSetup:destroy()
	if scene then
		for _, go in ipairs(self.owned) do
			pcall(function() scene:remove(go) end)
		end
	end
	self.owned = {}
	self.keep = {}
end

function SSAOSetup:serialize()
	return {}
end

function SSAOSetup.deserialize(data)
	return SSAOSetup:new()
end

return SSAOSetup
