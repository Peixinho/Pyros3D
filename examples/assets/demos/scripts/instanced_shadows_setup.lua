-- Instanced rendering, and the shadows instanced geometry casts.
--
-- One RenderingInstancedComponent draws the whole field in a single
-- DrawElementsInstanced call - one mesh, one material, one draw, N
-- per-instance mat4s in a vertex buffer with an attribute divisor.
--
-- The point of the demo is the *shadow* pass, which is where instancing
-- used to fall apart. That pass doesn't use an object's own material: it
-- overrides every caster with one shared shadow material (see
-- IRenderer::PickShadowMaterial). Until that override gained an
-- InstancedRendering variant, its shader never declared
-- aInstancedTransform, so the attribute divisor was never set and all N
-- instances rendered their shadow at the component's own model matrix -
-- one clump of shadow under the origin, and nothing under any of the
-- actual blocks. Everything else about the field looked completely
-- correct, which is what made it easy to miss.
--
-- The light sweeps so the shadows move: a static shot can hide a shadow
-- that is merely in the wrong place, a moving one cannot.
local InstancedShadowsSetup = class('InstancedShadowsSetup')

local GRID = 12            -- GRID x GRID instances
local SPACING = 11.0
local GROUND_HALF = 110.0

-- One bounding sphere covers the WHOLE batch - RenderingInstancedComponent
-- takes a single radius, not one per instance, so the field is culled
-- all-or-nothing. Fine here (it's one draw either way); a real open-world
-- field would want chunking into a component per grid cell so distant
-- chunks can drop out individually.
local FIELD_RADIUS = GRID * SPACING

function InstancedShadowsSetup:initialize()
	self.owned = {}
	self.keep = {}
	self.field = nil
	self.sun = nil
	self.sweep = true
	self.sunAngle = 0.55
	self.instances = GRID * GRID
end

function InstancedShadowsSetup:init(owner)
	self.owner = owner
	if not scene then error("InstancedShadowsSetup:init - scene is nil") end

	self:buildGround()
	self:buildField()
	self:buildSun()
end

function InstancedShadowsSetup:buildSun()
	-- Built here rather than declared in the scene JSON so the sweep below
	-- has a direct handle on the light component itself - Scene has no
	-- lookup-by-name in Lua, and reaching a sibling component through the
	-- owner GameObject is more indirection than one light is worth.
	local proj = Projection.new()
	proj:perspective(70.0, 1.7777, 1.0, 400.0)
	self.shadowProjection = proj -- outlives init(); the light keeps using it

	local sun = DirectionalLight.new(Vec4.new(1.0, 0.97, 0.92, 1.0), Vec3.new(0.8, -0.55, 0.0))
	-- sol2 does not honour C++ default arguments, so the cascade count has
	-- to be passed explicitly even though it is the default.
	sun:enableShadows(2048, 2048, proj, 1.0, 400.0, 1)

	local go = GameObject.new()
	go:addComponent(sun)
	scene:add(go)

	self.owned[#self.owned + 1] = go
	self.keep[#self.keep + 1] = sun
	self.sun = sun
end

function InstancedShadowsSetup:buildGround()
	local mat = GenericShaderMaterial.new(
		ShaderUsage.Color + ShaderUsage.Diffuse + ShaderUsage.DirectionalShadow)
	mat:setColor(Vec4.new(0.72, 0.73, 0.76, 1.0))
	self.keep[#self.keep + 1] = mat

	local mesh = Plane.new(GROUND_HALF, GROUND_HALF)
	self.keep[#self.keep + 1] = mesh

	local go = GameObject.new()
	local rc = RenderingComponent.new(mesh, mat)
	-- Receives shadows, casts none - a ground plane casting into its own
	-- shadow map is just acne.
	rc:disableCastShadows()
	go:addComponent(rc)
	go:setRotation(Vec3.new(math.rad(-90), 0, 0))
	scene:add(go)

	self.owned[#self.owned + 1] = go
	self.keep[#self.keep + 1] = rc
end

function InstancedShadowsSetup:buildField()
	-- No DirectionalShadow on the blocks themselves: they cast, they don't
	-- receive. Keeps the read unambiguous - every dark shape on screen is
	-- a shadow *on the ground*, cast by an instance.
	local mat = GenericShaderMaterial.new(
		ShaderUsage.Color + ShaderUsage.Diffuse + ShaderUsage.InstancedRendering)
	mat:setColor(Vec4.new(0.82, 0.34, 0.24, 1.0))
	self.keep[#self.keep + 1] = mat

	local mesh = Cube.new(4.0, 4.0, 4.0)
	self.keep[#self.keep + 1] = mesh

	local count = GRID * GRID
	local rc = RenderingInstancedComponent.new(mesh, mat, count, FIELD_RADIUS)

	-- Varied height/rotation/scale per instance, from a fixed pattern
	-- rather than math.random, so the demo looks the same every launch and
	-- a screenshot comparison stays meaningful.
	local n = 0
	for gx = 0, GRID - 1 do
		for gz = 0, GRID - 1 do
			n = n + 1
			local x = (gx - (GRID - 1) * 0.5) * SPACING
			local z = (gz - (GRID - 1) * 0.5) * SPACING
			-- Deterministic pseudo-noise, not math.random, so the demo looks
			-- identical every launch. Two sines whose frequency ratios are
			-- irrational to each other and which lean on gx and gz in
			-- opposite proportions - a single sin(a*gx + b*gz) is constant
			-- along the line a*gx + b*gz = k, which reads as obvious
			-- diagonal banding across a regular grid.
			local n1 = math.sin(gx * 1.7 + gz * 0.9)
			local n2 = math.sin(gx * 0.63 - gz * 2.31 + 1.7)
			local h = 0.5 + 1.1 * ((n1 + n2 + 2.0) * 0.25)
			local yaw = (n1 - n2) * math.pi * 0.5

			local m = Matrix.new()
			-- Uniform in X/Z, stretched in Y - scale first so it applies to
			-- the unrotated axes (Matrix.scale multiplies the diagonal, it
			-- is not a full compose), then rotate, then place.
			m:scale(1.0, h * 1.6, 1.0)
			m:rotationY(yaw)
			m:translate(x, h * 3.2, z)
			rc:setTransform(n, m)
		end
	end
	-- Nothing above is visible to the GPU until this uploads the vector.
	rc:updateTransforms()

	local go = GameObject.new()
	go:addComponent(rc)
	scene:add(go)

	self.owned[#self.owned + 1] = go
	self.keep[#self.keep + 1] = rc
	self.field = rc
	self.instances = count
end

function InstancedShadowsSetup:update(time)
	if not self.sweep or not self.sun then return end
	self.sunAngle = self.sunAngle + 0.0045
	-- Kept low (Y = -0.55) so the shadows stay long and clearly separated
	-- from the blocks casting them.
	self.sun:setLightDirection(Vec3.new(
		math.cos(self.sunAngle) * 0.8, -0.55, math.sin(self.sunAngle) * 0.8))
end

function InstancedShadowsSetup:drawUI()
	if not imgui then return end
	imgui.text("One instanced draw call, " .. tostring(self.instances) .. " instances.")
	imgui.text("Every block is one instance of the same cube mesh,")
	imgui.text("placed by a per-instance mat4 vertex attribute.")
	imgui.separator()
	imgui.text("The shadows are the actual test: the shadow pass")
	imgui.text("swaps in its own material, so it needs its own")
	imgui.text("instanced shader variant to place them. Without")
	imgui.text("one they all collapse onto a single instance.")
	imgui.separator()
	self.sweep = imgui.checkbox("Sweep the sun", self.sweep)
end

function InstancedShadowsSetup:destroy()
	if scene then
		for _, go in ipairs(self.owned) do
			pcall(function() scene:remove(go) end)
		end
	end
	self.owned = {}
	self.keep = {}
	self.field = nil
	self.sun = nil
end

function InstancedShadowsSetup:serialize()
	return {}
end

function InstancedShadowsSetup.deserialize(data)
	return InstancedShadowsSetup:new()
end

return InstancedShadowsSetup
