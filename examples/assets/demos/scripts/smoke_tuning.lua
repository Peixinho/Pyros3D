-- Attached to ParticlesExample's smoke GameObject (the one holding a
-- ParticleSystem component). DemoLauncher's ImGui panel writes the 3
-- tunables (emissionRate/spread/riseSpeed) into this instance's own
-- table every frame (no ImGui<->Lua binding exists - see the plan), and
-- this script pushes them into the real ParticleSystem each update, the
-- same 3 setters the original C++ ParticlesExample called directly.
local SmokeTuning = class('SmokeTuning')

function SmokeTuning:initialize()
	self.emissionRate = 15.0
	self.spread = 1.0
	self.riseSpeed = 2.0
end

function SmokeTuning:init(owner)
	self.particleSystem = owner:getComponent("ParticleSystem")
end

function SmokeTuning:update(time)
	if self.particleSystem then
		self.particleSystem:setEmissionRate(self.emissionRate)
		self.particleSystem:setSpeed(self.riseSpeed * 0.8, self.riseSpeed * 1.2)
		self.particleSystem:setSpread(math.rad(20.0 * self.spread))
	end
end

-- Required for scriptFile/data (hence real behavior) to survive a save/
-- load round trip at all - see SceneSerializer.cpp's LuaComponent save
-- case; without these two, this component would load back as an inert
-- existence-only marker.
function SmokeTuning:serialize()
	return { emissionRate = self.emissionRate, spread = self.spread, riseSpeed = self.riseSpeed }
end

function SmokeTuning.deserialize(data)
	local inst = SmokeTuning:new()
	inst.emissionRate = data.emissionRate
	inst.spread = data.spread
	inst.riseSpeed = data.riseSpeed
	return inst
end

return SmokeTuning
