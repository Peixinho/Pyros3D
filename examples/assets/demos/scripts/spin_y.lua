-- Shared, reusable behavior - continuously rotates its owner around Y
-- using absolute simulation time (matches the original C++ demos'
-- `SetRotation(Vec3(0, GetTime(), 0))` exactly, no per-frame dt
-- integration/drift). Attached to more than one GameObject at once is
-- fine - require_file() caches the class, not the instance, so each
-- attach gets its own independent `self`.
local SpinY = class('SpinY')
local dTime = 0

function SpinY:initialize()
end

function SpinY:init(owner)
	self.owner = owner
end

function SpinY:update(time)
	local r = self.owner:getRotation()
  dTime=dTime + time
	self.owner:setRotation(Vec3.new(r.x, dTime, r.z))
end

-- No real state to persist (the owner reference is re-derived by init()
-- on load, not from saved data) - but SceneSerializer only preserves
-- `scriptFile`/`data` at all if both serialize()/deserialize() exist
-- (see SceneSerializer.cpp's LuaComponent save case), so this pair is
-- required for the attached behavior to survive a save/load round trip,
-- not just decoration.
function SpinY:serialize()
	return {}
end

function SpinY.deserialize(data)
	return SpinY:new()
end

return SpinY
