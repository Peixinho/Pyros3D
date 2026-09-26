-- SSAO: spawn teapot grid + floor, push view matrix into SSAO each frame.
local SSAOSetup = class('SSAOSetup')

function SSAOSetup:initialize()
	self.owned = {}
	self.keep = {}
	self.radius = 0.5
	self.strength = 2.0
	self.threshold = 0.5
	self.scale = 1.0
	self.blurIntensity = 1.0
	self.direct = 1.0
	self.enabled = true
end

function SSAOSetup:init(owner)
	self.owner = owner
	if not scene or not ASSETS_PATH then return end

	-- A deferred renderer only shades what writes its G-buffer.
	local usage = ShaderUsage.Diffuse
	if renderer and renderer.enableSSAO then
		usage = usage + ShaderUsage.DeferredRenderer_Gbuffer
	end

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

function SSAOSetup:update(time)
	if camera and ssaoSetViewMatrix then
		ssaoSetViewMatrix(camera:getWorldTransformation():inverse())
	end
end

function SSAOSetup:drawUI()
	if not imgui then return end
	imgui.text("SSAO")
	self.radius = imgui.sliderFloat("Radius", self.radius, 0.01, 2.0)
	self.strength = imgui.sliderFloat("Strength", self.strength, 0.1, 5.0)
	self.threshold = imgui.sliderFloat("Threshold", self.threshold, 0.1, 5.0)
	self.scale = imgui.sliderFloat("Scale", self.scale, 0.1, 5.0)
	self.blurIntensity = imgui.sliderFloat("Blur", self.blurIntensity, 0.0, 3.0)
	if renderer and renderer.setSSAOParams then
		-- Deferred: the renderer's own SSAO, on ambient light plus this
		-- share of direct light.
		self.direct = imgui.sliderFloat("Direct light", self.direct, 0.0, 1.0)
		local on = imgui.checkbox("SSAO", self.enabled)
		if on ~= self.enabled then
			self.enabled = on
			if on then renderer:enableSSAO() else renderer:disableSSAO() end
		end
		renderer:setSSAOParams(self.radius, self.strength, self.threshold)
		renderer:setSSAODirectStrength(self.direct)
	elseif ssaoSetParams then
		ssaoSetParams(self.radius, self.strength, self.threshold, self.scale, self.blurIntensity)
	end
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
