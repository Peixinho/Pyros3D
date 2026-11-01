-- Octahedral impostors: a teapot baked from a grid of directions over its upper half (the editor's
-- bake_octahedral_impostor - its colour, its normals, its own occlusion), and two thousand of them drawn as one quad
-- each - turned to the eye, the three nearest baked views blended, lit by the scene as a mesh is. The front row is the
-- real mesh, for the comparison: from a few metres the picture and the model are hard to tell apart.
local ImpostorField = class('ImpostorField')

function ImpostorField:initialize()
	self.owned = {}
	self.keep = {}
end

function ImpostorField:init(owner)
	self.owner = owner
	if not scene or not ASSETS_PATH then return end
	local base = ASSETS_PATH .. "demos/impostors/teapot"
	-- (the bake's numbers, as bake_octahedral_impostor answered them: its grid side, the teapot's radius and centre height)
	local FRAMES, RADIUS, CENTRE = 8, 103.6, 4.89
	local SCALE = 0.01

	local albedo = Texture.new(); albedo:loadTexture(base .. "_albedo.png", TextureType.Texture, true, 0)
	local normal = Texture.new(); normal:loadTexture(base .. "_normal.png", TextureType.Texture, true, 0)
	self.keep[#self.keep + 1] = albedo; self.keep[#self.keep + 1] = normal
	local mat = GenericShaderMaterial.new(ShaderUsage.Texture + ShaderUsage.Diffuse + ShaderUsage.AlphaTest
		+ ShaderUsage.InstancedRendering + ShaderUsage.InstancedColor + ShaderUsage.Impostor)
	mat:setColorMap(albedo)
	mat:setNormalMap(normal)
	mat:setImpostor(FRAMES, RADIUS, CENTRE)
	mat:setAlphaCutoff(0.5)
	mat:setCullFace(CullFace.DoubleSided)
	self.keep[#self.keep + 1] = mat

	local quad = ImpostorQuad.new(RADIUS, CENTRE)
	self.keep[#self.keep + 1] = quad
	local COLS, ROWS, GAP = 40, 50, 2.4
	local rc = RenderingInstancedComponent.new(quad, mat, COLS * ROWS, 80.0)
	rc:enableInstanceColors()
	math.randomseed(7)
	local i = 1     -- (setTransform and setInstanceColor count from 1, as Lua does)
	for z = 0, ROWS - 1 do
		for x = 0, COLS - 1 do
			local m = Matrix.new()
			m:scale(SCALE, SCALE, SCALE)
			m:rotationY(math.random() * 6.2832)
			m:translate((x - COLS * 0.5 + 0.5) * GAP, 0.0, -3.0 - z * GAP)
			rc:setTransform(i, m)
			local shade = 0.8 + 0.4 * math.random()
			rc:setInstanceColor(i, Vec4.new(shade, shade * (0.85 + 0.3 * math.random()), shade, 1.0))
			i = i + 1
		end
	end
	rc:updateTransforms()
	rc:updateInstanceColors()
	local field = GameObject.new()
	field:addComponent(rc)
	scene:add(field)
	self.owned[#self.owned + 1] = field
	self.keep[#self.keep + 1] = rc

	-- the real thing in front, for the comparison
	local teapot = Model.new(ASSETS_PATH .. "teapotLOD1.p3dm", false)
	self.keep[#self.keep + 1] = teapot
	for k = -3, 3 do
		local go = GameObject.new()
		local real = RenderingComponent.new(teapot, ShaderUsage.Diffuse)
		go:addComponent(real)
		go:setPosition(Vec3.new(k * GAP, 0.0, 0.0))
		go:setScale(Vec3.new(SCALE, SCALE, SCALE))
		go:setRotation(Vec3.new(0, math.rad(25 * k), 0))
		scene:add(go)
		self.owned[#self.owned + 1] = go
		self.keep[#self.keep + 1] = real
	end

	local ground = Plane.new(140, 160)
	local gmat = GenericShaderMaterial.new(ShaderUsage.Color + ShaderUsage.Diffuse)
	gmat:setColor(Vec4.new(0.32, 0.34, 0.30, 1.0))
	local g = GameObject.new()
	g:setRotation(Vec3.new(math.rad(-90), 0, 0))
	g:setPosition(Vec3.new(0, 0, -55))
	g:addComponent(RenderingComponent.new(ground, gmat))
	scene:add(g)
	self.owned[#self.owned + 1] = g
	self.keep[#self.keep + 1] = ground; self.keep[#self.keep + 1] = gmat
end

function ImpostorField:destroy()
	if scene then
		for _, go in ipairs(self.owned) do pcall(function() scene:remove(go) end) end
	end
	self.owned = {}
	self.keep = {}
end

function ImpostorField:serialize() return {} end
function ImpostorField.deserialize(data) return ImpostorField:new() end

return ImpostorField
