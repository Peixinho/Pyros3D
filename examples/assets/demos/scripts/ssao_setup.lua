-- Ambient occlusion: a room's corner under soft light - two walls and a floor, boxes stacked against them, balls resting
-- on the floor and on one another, columns close to the wall, a knot and a teapot - lit mostly by the sky, so what the
-- renderer's ground-truth AO (GTAO, from the depth prepass) takes away is most of the shading: dark where things meet
-- the floor, the walls and each other, open where they face the room. (The light that matters to AO is the ambient:
-- under a hard sun alone there is little for it to darken - the SSAO effect's uDirect gives it a share of that too.)
local SSAOSetup = class('SSAOSetup')

function SSAOSetup:initialize()
	self.owned = {}
	self.keep = {}
end

function SSAOSetup:add(mesh, mat, x, y, z, ry)
	local go = GameObject.new()
	local rc = RenderingComponent.new(mesh, mat)
	go:addComponent(rc)
	go:setPosition(Vec3.new(x, y, z))
	if ry then go:setRotation(Vec3.new(0, math.rad(ry), 0)) end
	scene:add(go)
	self.owned[#self.owned + 1] = go
	self.keep[#self.keep + 1] = rc
	return go
end

function SSAOSetup:material(r, g, b, rough)
	local m = GenericShaderMaterial.new(ShaderUsage.Color + ShaderUsage.Diffuse + ShaderUsage.PBR)
	m:setColor(Vec4.new(r, g, b, 1.0))
	m:setRoughness(rough or 0.8)
	self.keep[#self.keep + 1] = m
	return m
end

function SSAOSetup:init(owner)
	self.owner = owner
	if not scene or not ASSETS_PATH then return end
	local plaster = self:material(0.78, 0.76, 0.72, 0.9)
	local clay = self:material(0.72, 0.52, 0.40, 0.75)
	local stone = self:material(0.62, 0.62, 0.60, 0.7)

	-- the room's corner: a floor and two walls meeting it (the walls' boxes a little into the floor - no seam of light)
	local floor = Cube.new(12, 0.2, 12); self.keep[#self.keep + 1] = floor
	self:add(floor, plaster, 0, -0.1, 0)
	local back = Cube.new(12, 5, 0.2); self.keep[#self.keep + 1] = back
	self:add(back, plaster, 0, 2.4, -3.0)
	local side = Cube.new(0.2, 5, 12); self.keep[#self.keep + 1] = side
	self:add(side, plaster, -3.6, 2.4, 0)

	-- boxes stacked into the corner, touching the walls and one another
	local box = Cube.new(0.8, 0.8, 0.8); self.keep[#self.keep + 1] = box
	self:add(box, clay, -3.1, 0.4, -2.5)
	self:add(box, clay, -2.3, 0.4, -2.5, 8)
	self:add(box, clay, -3.1, 0.4, -1.7, -6)
	self:add(box, clay, -2.75, 1.2, -2.45, 20)
	local small = Cube.new(0.45, 0.45, 0.45); self.keep[#self.keep + 1] = small
	self:add(small, stone, -1.6, 0.225, -2.65, 30)
	self:add(small, stone, -2.85, 1.825, -2.6, -12)

	-- columns a hand's width from the back wall
	local column = Cylinder.new(0.22, 4.8, 24, 1, false); self.keep[#self.keep + 1] = column
	for k = 0, 2 do self:add(column, stone, 0.2 + k * 1.3, 2.4, -2.55) end

	-- balls on the floor, two of them touching, one against the side wall
	local ball = Sphere.new(0.45, 40, 28, true, false, false); self.keep[#self.keep + 1] = ball
	self:add(ball, stone, -0.6, 0.45, -0.6)
	self:add(ball, clay, 0.28, 0.45, -0.45)
	self:add(ball, stone, -3.05, 0.45, 0.4)
	local pebble = Sphere.new(0.18, 24, 16, true, false, false); self.keep[#self.keep + 1] = pebble
	for k = 0, 5 do self:add(pebble, clay, -1.2 + k * 0.36, 0.18, 0.55 + (k % 2) * 0.3) end

	-- a knot - its tube passing close to itself, AO in every crossing - and a teapot on the floor
	local knot = TorusKnot.new(0.42, 0.13, 160, 18); self.keep[#self.keep + 1] = knot
	self:add(knot, clay, 1.7, 0.75, -0.9, 25)
	local teapot = Model.new(ASSETS_PATH .. "teapotLOD1.p3dm", false); self.keep[#self.keep + 1] = teapot
	local t = GameObject.new()
	local trc = RenderingComponent.new(teapot, stone)
	t:addComponent(trc)
	t:setPosition(Vec3.new(1.0, 0.31, 0.6))     -- (its origin is mid-body)
	t:setScale(Vec3.new(0.008, 0.008, 0.008))
	t:setRotation(Vec3.new(0, math.rad(-35), 0))
	scene:add(t)
	self.owned[#self.owned + 1] = t
	self.keep[#self.keep + 1] = trc
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
