-- Motion blur showcase: fast orbiting / spinning coloured objects so the
-- velocity-map post effect is obvious. The velocity pass itself is owned by
-- RenderHost when manifest effects includes "motionblur".
local MotionBlurSetup = class('MotionBlurSetup')

function MotionBlurSetup:initialize()
	self.owned = {}
	self.keep = {}
	self.movers = {}
end

local function keep(self, obj)
	self.keep[#self.keep + 1] = obj
	return obj
end

local function own(self, go)
	self.owned[#self.owned + 1] = go
	return go
end

function MotionBlurSetup:init(owner)
	self.owner = owner
	if not scene then error("MotionBlurSetup:init - global scene is nil") end
	if not ASSETS_PATH then error("MotionBlurSetup:init - global ASSETS_PATH is nil") end

	if renderer and renderer.setBackground then
		renderer:setBackground(Vec4.new(0.04, 0.04, 0.06, 1))
	end

	local usage = ShaderUsage.Color + ShaderUsage.Diffuse
	local teapot = Model.new(ASSETS_PATH .. "teapotLOD1.p3dm", false)
	keep(self, teapot)

	local cubeMesh = Cube.new(12, 12, 12)
	keep(self, cubeMesh)

	local floorMesh = Plane.new(200, 200)
	keep(self, floorMesh)
	local floorMat = GenericShaderMaterial.new(usage)
	floorMat:setColor(Vec4.new(0.18, 0.18, 0.2, 1))
	keep(self, floorMat)
	local floor = GameObject.new()
	floor:setPosition(Vec3.new(0, -20, 0))
	floor:setRotation(Vec3.new(math.rad(-90), 0, 0))
	local rFloor = RenderingComponent.new(floorMesh, floorMat)
	keep(self, rFloor)
	floor:addComponent(rFloor)
	scene:add(floor)
	own(self, floor)

	-- Bright static markers so blur streaks read against sharp references.
	local markerMesh = Cube.new(6, 6, 6)
	keep(self, markerMesh)
	for i = 0, 3 do
		local mat = GenericShaderMaterial.new(usage)
		mat:setColor(Vec4.new(0.9, 0.9, 0.95, 1))
		keep(self, mat)
		local go = GameObject.new()
		local a = i * (math.pi * 0.5)
		go:setPosition(Vec3.new(math.cos(a) * 70, -14, math.sin(a) * 70))
		local rc = RenderingComponent.new(markerMesh, mat)
		keep(self, rc)
		go:addComponent(rc)
		scene:add(go)
		own(self, go)
	end

	local palette = {
		{ 1.0, 0.2, 0.15 },
		{ 0.2, 0.85, 0.35 },
		{ 0.2, 0.45, 1.0 },
		{ 1.0, 0.85, 0.15 },
		{ 0.95, 0.3, 0.9 },
		{ 0.2, 0.95, 0.95 },
	}

	for i, rgb in ipairs(palette) do
		local mat = GenericShaderMaterial.new(usage)
		mat:setColor(Vec4.new(rgb[1], rgb[2], rgb[3], 1))
		keep(self, mat)

		local go = GameObject.new()
		local mesh = (i % 2 == 0) and teapot or cubeMesh
		local rc = RenderingComponent.new(mesh, mat)
		keep(self, rc)
		go:addComponent(rc)
		if mesh == teapot then
			go:setScale(Vec3.new(1.2, 1.2, 1.2))
		end
		scene:add(go)
		own(self, go)

		self.movers[#self.movers + 1] = {
			go = go,
			radius = 35 + i * 18,
			speed = 1.2 + i * 0.55,
			spin = 2.0 + i * 0.7,
			phase = i * 0.9,
			height = -5 + (i % 3) * 12,
		}
	end
end

function MotionBlurSetup:update(time)
	for _, m in ipairs(self.movers) do
		local a = time * m.speed + m.phase
		m.go:setPosition(Vec3.new(
			math.sin(a) * m.radius,
			m.height + math.cos(a * 1.7) * 8,
			math.cos(a) * m.radius
		))
		m.go:setRotation(Vec3.new(time * m.spin * 0.4, time * m.spin, time * m.spin * 0.25))
	end
end

function MotionBlurSetup:drawUI()
	if not imgui then return end
	imgui.text("Motion Blur")
	imgui.text("Coloured objects orbit + spin quickly.")
	imgui.text("Static white cubes stay sharp for contrast.")
	imgui.text("TAB / WASD / mouse to move the camera.")
end

function MotionBlurSetup:destroy()
	if scene then
		for _, go in ipairs(self.owned) do
			pcall(function() scene:remove(go) end)
		end
	end
	self.owned = {}
	self.keep = {}
	self.movers = {}
end

function MotionBlurSetup:serialize()
	return {}
end

function MotionBlurSetup.deserialize(data)
	return MotionBlurSetup:new()
end

return MotionBlurSetup
