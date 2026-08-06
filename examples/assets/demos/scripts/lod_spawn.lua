-- Spawns LOD teapots + point lights. Kept out of JSON (10k instances).
local LODSpawn = class('LODSpawn')

local TEAPOTS = 10000
local LIGHTS = 100

function LODSpawn:initialize()
	self.owned = {}
	self.keep = {}
end

function LODSpawn:init(owner)
	self.owner = owner
	if not scene or not renderer or not ASSETS_PATH then return end

	if renderer.enableLOD then renderer:enableLOD() end
	if renderer.activateCulling then
		renderer:activateCulling(CullingMode.FrustumCulling)
	end

	local usage = ShaderUsage.Diffuse + ShaderUsage.SpecularColor + ShaderUsage.Color
	local lod1 = Model.new(ASSETS_PATH .. "teapotLOD1.p3dm", false)
	local lod2 = Model.new(ASSETS_PATH .. "teapotLOD2.p3dm", false)
	local lod3 = Model.new(ASSETS_PATH .. "teapotLOD3.p3dm", false)
	self.keep[#self.keep + 1] = lod1
	self.keep[#self.keep + 1] = lod2
	self.keep[#self.keep + 1] = lod3

	local sphere = Sphere.new(3, 16, 10)
	self.keep[#self.keep + 1] = sphere

	math.randomseed(1)
	for i = 1, LIGHTS do
		local go = GameObject.new()
		local light = PointLight.new(Vec4.new(1, 1, 1, 1), 100)
		go:addComponent(light)
		go:addComponent(RenderingComponent.new(sphere, ShaderUsage.Color))
		go:setPosition(Vec3.new(math.random() * 1000 - 500, math.random() * 1000 - 500, math.random() * 1000 - 500))
		scene:add(go)
		self.owned[#self.owned + 1] = go
		self.keep[#self.keep + 1] = light
	end

	for i = 1, TEAPOTS do
		local mat = GenericShaderMaterial.new(usage)
		mat:setColor(Vec4.new(0.5, 0.5, 0.5, 1))
		mat:setSpecular(Vec4.new(1, 1, 1, 1))
		local rc = RenderingComponent.new(lod1, mat)
		rc:addLOD(lod2, 50, usage)
		rc:addLOD(lod3, 100, usage)
		local go = GameObject.new(true)
		go:setPosition(Vec3.new(math.random() * 1000 - 500, math.random() * 1000 - 500, math.random() * 1000 - 500))
		go:setScale(Vec3.new(0.1, 0.1, 0.1))
		go:addComponent(rc)
		scene:add(go)
		self.owned[#self.owned + 1] = go
		self.keep[#self.keep + 1] = mat
		self.keep[#self.keep + 1] = rc
	end
end

function LODSpawn:destroy()
	if scene then
		for _, go in ipairs(self.owned) do
			pcall(function() scene:remove(go) end)
		end
	end
	self.owned = {}
	self.keep = {}
end

function LODSpawn:serialize()
	return {}
end

function LODSpawn.deserialize(data)
	return LODSpawn:new()
end

return LODSpawn
