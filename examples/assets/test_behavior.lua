-- test_behavior.lua - minimal real example of a serializable LuaComponent
-- behavior, per PyrosBindings.h's LuaComponent_FromFile contract: this
-- file must `return` a middleclass class (class() comes from
-- middleclass.lua, already loaded before this file is required).

local TestBehavior = class('TestBehavior')

function TestBehavior:initialize()
	self.hp = 100
	self.name = "default"
end

function TestBehavior:serialize()
	return { hp = self.hp, name = self.name }
end

function TestBehavior.deserialize(data)
	local inst = TestBehavior:new()
	inst.hp = data.hp
	inst.name = data.name
	return inst
end

return TestBehavior
