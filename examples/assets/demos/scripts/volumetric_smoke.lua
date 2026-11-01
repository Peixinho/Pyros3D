-- Volumetric smoke and fire (VolumetricSmoke): a grenade's wall of smoke filling the colonnade - stopped by its
-- columns and the floor, which have colliders - a red signal smoke kept low along the ground, a fire burning beside
-- the slab, and now and then an explosion's ball of flame. Each comes back when it has gone.
local VolumetricSmokeDemo = class('VolumetricSmokeDemo')

function VolumetricSmokeDemo:initialize()
	self.clock = 0
	self.slots = {}
	self.nextBlast = 3.0
end

function VolumetricSmokeDemo:init(owner)
	self.owner = owner
	if not VolumetricSmoke then return end
	VolumetricSmoke.clear()
	VolumetricSmoke.useSceneLighting()
	VolumetricSmoke.setWind(Vec3.new(0.35, 0, 0.1))
	-- (a cloud's field is 32 x 16 x 32 cells: 0.8 m ones give a grenade's smoke 25 m to fill)
	VolumetricSmoke.setCellSize(0.8)
end

-- (each kind: where, what colour, how big; spawned again once its cloud is gone)
local KINDS = {
	grenade = function() return VolumetricSmoke.spawn(physics, Vec3.new(-1.5, 0.3, -10.0), Vec3.new(0.78, 0.78, 0.8), 7.0, 3.0, 16.0, 4.0) end,
	signal = function() return VolumetricSmoke.spawnEx(physics, Vec3.new(8.0, 0.2, -6.5), Vec3.new(0.78, 0.26, 0.2), 3.5, 3.0, 12.0, 4.0, 1.3, 0.0) end,
	fire = function() return VolumetricSmoke.spawnEx(physics, Vec3.new(-3.0, 0.2, 1.0), Vec3.new(1.0, 1.0, 1.0), 1.3, 1.0, 30.0, 2.0, 0.3, 1.0, 0.0) end,
}

function VolumetricSmokeDemo:update(dt)
	if not VolumetricSmoke then return end
	dt = math.max(0.0, math.min(0.05, dt or 0.016))
	self.clock = self.clock + dt
	for name, spawn in pairs(KINDS) do
		local slot = self.slots[name]
		if slot == nil or slot < 0 or not VolumetricSmoke.active(slot) then
			self.slots[name] = spawn()
		end
	end
	-- an explosion: a ball of flame, gone in a few seconds, and the flash of it over the whole picture
	if self.clock >= self.nextBlast then
		self.nextBlast = self.clock + 12.0
		VolumetricSmoke.spawnEx(physics, Vec3.new(3.5, 0.5, -4.0), Vec3.new(1.0, 0.85, 0.7), 2.6, 0.35, 8.0, 4.0, 0.9, 1.6, 1.0)
		self.flash = 0.35
	end
	if self.flash then
		VolumetricSmoke.setScreenFlash(Vec3.new(1.0, 0.85, 0.6), self.flash)
		self.flash = self.flash - dt * 0.9
		if self.flash <= 0 then VolumetricSmoke.setScreenFlash(Vec3.new(1, 1, 1), 0); self.flash = nil end
	end
	VolumetricSmoke.update(dt)
end

function VolumetricSmokeDemo:destroy()
	if VolumetricSmoke then
		VolumetricSmoke.clear()
		VolumetricSmoke.setScreenFlash(Vec3.new(1, 1, 1), 0)
	end
end

function VolumetricSmokeDemo:serialize()
	return {}
end

function VolumetricSmokeDemo.deserialize(data)
	return VolumetricSmokeDemo:new()
end

return VolumetricSmokeDemo
