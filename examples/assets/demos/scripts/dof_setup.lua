-- Depth of field: spawn a row of spinning suzannes.
local DOFSetup = class('DOFSetup')

function DOFSetup:initialize()
	self.owned = {}
	self.keep = {}
	self.lastTime = nil
end

function DOFSetup:init(owner)
	self.owner = owner
	if not scene then error("DOFSetup:init - global scene is nil") end
	if not ASSETS_PATH then error("DOFSetup:init - global ASSETS_PATH is nil") end
	if renderer and renderer.setBackground then
		renderer:setBackground(Vec4.new(1, 0, 0, 1))
	end

	local path = ASSETS_PATH .. "suzanne.p3dm"
	local mesh = Model.new(path, false)
	if not mesh then error("DOFSetup:init - Model.new failed for " .. path) end
	self.keep[#self.keep + 1] = mesh
	for i = 0, 9 do
		local go = GameObject.new()
		go:setPosition(Vec3.new(-23 + i * 3, 0, -15 + i * 3))
		local rc = RenderingComponent.new(mesh, ShaderUsage.Diffuse)
		if not rc then error("DOFSetup:init - RenderingComponent.new failed") end
		go:addComponent(rc)
		scene:add(go)
		self.owned[#self.owned + 1] = go
		self.keep[#self.keep + 1] = rc
	end
	if #self.owned ~= 10 then
		error("DOFSetup:init - expected 10 owned gos, got " .. tostring(#self.owned))
	end
end

function DOFSetup:update(time)
	if self.lastTime == nil then self.lastTime = time end
	for _, go in ipairs(self.owned) do
		go:setRotation(Vec3.new(0, time, 0))
	end
end

function DOFSetup:destroy()
	if scene then
		for _, go in ipairs(self.owned) do
			pcall(function() scene:remove(go) end)
		end
	end
	self.owned = {}
	self.keep = {}
end

function DOFSetup:serialize()
	return {}
end

function DOFSetup.deserialize(data)
	return DOFSetup:new()
end

return DOFSetup
