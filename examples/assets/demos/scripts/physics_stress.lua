-- Continuous sphere rain into an open-top walled box (Box3D stress test).
-- Floor + walls live in PhysicsStress.json (no ceiling); this script spawns
-- up to maxBodies small dynamic spheres and holds at the cap (no cull).
local PhysicsStress = class('PhysicsStress')

function PhysicsStress:initialize()
	self.maxBodies = 20000
	self.spawnPerSecond = 200
	self.radius = 0.2
	self.mass = 0.15
	self.arenaHalf = 10.0
	self.dropHeight = 22.0
	self.maxSpawnPerFrame = 12
	self.owned = {}
	self.keep = {}
	self.accumulator = 0
	self.lastTime = nil
	self.mesh = nil
	self.material = nil
end

function PhysicsStress:init(owner)
	self.owner = owner
	if not scene or not physics then
		print("PhysicsStress: scene/physics globals missing")
		return
	end

	math.randomseed(42)
	self.mesh = Sphere.new(self.radius, 8, 6, false)
	local usage = ShaderUsage.Color + ShaderUsage.Diffuse + ShaderUsage.DirectionalShadow
	self.material = GenericShaderMaterial.new(usage)
	self.material:setColor(Vec4.new(0.25, 0.65, 1.0, 1.0))
	self.keep[#self.keep + 1] = self.mesh
	self.keep[#self.keep + 1] = self.material
end

function PhysicsStress:spawnOne()
	local half = self.arenaHalf - self.radius * 2.0
	local x = (math.random() * 2.0 - 1.0) * half
	local z = (math.random() * 2.0 - 1.0) * half
	local y = self.dropHeight + math.random() * 6.0

	local go = GameObject.new()
	go:setPosition(Vec3.new(x, y, z))

	local rc = RenderingComponent.new(self.mesh, self.material)
	rc:enableCastShadows()
	go:addComponent(rc)

	local body = physics:createSphere(self.radius, self.mass, false)
	go:addComponent(body)

	scene:add(go)
	self.owned[#self.owned + 1] = go
end

function PhysicsStress:update(time)
	if not scene or not physics or not self.mesh then return end
	if #self.owned >= self.maxBodies then return end

	local dt = 0.016
	if self.lastTime then
		dt = math.max(0.0, math.min(0.05, time - self.lastTime))
	end
	self.lastTime = time

	self.accumulator = self.accumulator + self.spawnPerSecond * dt
	local spawned = 0
	while self.accumulator >= 1.0 and #self.owned < self.maxBodies and spawned < self.maxSpawnPerFrame do
		self.accumulator = self.accumulator - 1.0
		self:spawnOne()
		spawned = spawned + 1
	end
end

function PhysicsStress:drawUI()
	if not imgui then return end
	imgui.text("Box3D Physics Stress")
	imgui.text(string.format("Bodies: %d / %d", #self.owned, self.maxBodies))
	self.spawnPerSecond = imgui.sliderFloat("Spawn / sec", self.spawnPerSecond, 10.0, 1000.0)
	self.maxBodies = math.floor(imgui.sliderFloat("Max bodies", self.maxBodies, 100.0, 20000.0) + 0.5)
	imgui.text("Spawns until max, then holds (no cull)")
end

function PhysicsStress:destroy()
	if scene then
		for _, go in ipairs(self.owned) do
			pcall(function() scene:remove(go) end)
		end
	end
	self.owned = {}
	self.keep = {}
	self.mesh = nil
	self.material = nil
end

function PhysicsStress:serialize()
	return {
		maxBodies = self.maxBodies,
		spawnPerSecond = self.spawnPerSecond,
		radius = self.radius,
		mass = self.mass,
	}
end

function PhysicsStress.deserialize(data)
	local inst = PhysicsStress:new()
	if data then
		inst.maxBodies = data.maxBodies or inst.maxBodies
		inst.spawnPerSecond = data.spawnPerSecond or inst.spawnPerSecond
		inst.radius = data.radius or inst.radius
		inst.mass = data.mass or inst.mass
	end
	return inst
end

return PhysicsStress
