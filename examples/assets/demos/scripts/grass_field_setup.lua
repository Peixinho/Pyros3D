-- Instanced, alpha-cutout grass with distance density LOD.
--
-- One RenderingInstancedComponent per chunk, each drawing its blades in a
-- single DrawElementsInstanced call. Every blade is a pair of crossed
-- quads sharing one mesh and one alpha-tested material.
--
-- Cutout rather than blending, because a deferred renderer cannot blend
-- into a G-buffer at all: the blade texture's alpha decides which
-- fragments exist (ShaderUsage.AlphaTest + setAlphaCutoff), and everything
-- downstream - lighting, shadows, SSR - then treats what survives as
-- ordinary opaque geometry.
--
-- Three things scale this to a large field, all per chunk:
--
--   visibility  the component's bounding sphere, frustum-culled for free
--   density     setNumberInstances(), continuous, nothing rebuilt
--   shadows     dropped past a radius, since the shadow pass redraws them
--
-- Density works only because scatterInstances() lays the buffer out
-- quad-major and in hash order, which makes any prefix of it a valid,
-- spatially even, lower-quality version of the same field. Half the
-- buffer is one quad per blade instead of a cross; a quarter also thins
-- the blades themselves. Nothing is rebuilt when the count changes - the
-- draw call reads the live instance count every frame.
local GrassFieldSetup = class('GrassFieldSetup')

local CHUNKS = 6             -- CHUNKS x CHUNKS components
local PER_CHUNK = 700        -- blades per component
local QUADS_PER_BLADE = 2
local CHUNK_SIZE = 26.0
local BLADE_W = 2.6
local BLADE_H = 3.4
local GROUND_SIZE = CHUNKS * CHUNK_SIZE * 1.8

-- Distance bands, measured to the chunk centre. Full detail near, one
-- quad per blade beyond the first, thinning after that, gone at the end.
local LOD_FULL = 55.0
local LOD_HALF = 95.0
local LOD_THIN = 170.0
local SHADOW_RADIUS = 70.0

function GrassFieldSetup:initialize()
	self.owned = {}
	self.keep = {}
	self.chunks = {}
	self.cutoff = 0.5
	self.wind = 0.16
	self.lodEnabled = true
	self.drawnInstances = 0
	self.drawnChunks = 0
	self.totalInstances = 0
end

function GrassFieldSetup:init(owner)
	self.owner = owner
	if not scene then error("GrassFieldSetup:init - scene is nil") end
	if not ASSETS_PATH then error("GrassFieldSetup:init - ASSETS_PATH is nil") end

	self:buildGround()
	self:buildGrass()
	self:buildSun()
end

function GrassFieldSetup:buildGround()
	local mat = GenericShaderMaterial.new(
		ShaderUsage.Color + ShaderUsage.Diffuse + ShaderUsage.PBR +
		ShaderUsage.DeferredRenderer_Gbuffer)
	mat:setColor(Vec4.new(0.16, 0.21, 0.12, 1.0))
	mat:setRoughness(0.9)
	self.keep[#self.keep + 1] = mat

	-- Deliberately larger than the grass field. The blades' own shadows are
	-- only legible where they fall on something that isn't more grass, so
	-- the ground runs past the field edge and the sun is low enough to
	-- throw them out across it.
	local mesh = Plane.new(GROUND_SIZE, GROUND_SIZE)
	self.keep[#self.keep + 1] = mesh

	local go = GameObject.new()
	local rc = RenderingComponent.new(mesh, mat)
	rc:disableCastShadows()
	go:addComponent(rc)
	go:setRotation(Vec3.new(math.rad(-90), 0, 0))
	scene:add(go)
	self.owned[#self.owned + 1] = go
	self.keep[#self.keep + 1] = rc
end

function GrassFieldSetup:buildGrass()
	local tex = Texture.new()
	tex:loadTexture(ASSETS_PATH .. "textures/grass_blades.png", TextureType.Texture, true, 0)
	tex:setMinMagFilter(TextureFilter.LinearMipmapLinear, TextureFilter.Linear)
	self.keep[#self.keep + 1] = tex

	-- DoubleSided: a blade card is a flat quad seen from both sides, and
	-- back-face culling would blank half of every cross.
	local mat = GenericShaderMaterial.new(
		ShaderUsage.Texture + ShaderUsage.Diffuse + ShaderUsage.PBR +
		ShaderUsage.AlphaTest + ShaderUsage.InstancedRendering +
		ShaderUsage.InstancedColor + ShaderUsage.VertexWind +
		ShaderUsage.DeferredRenderer_Gbuffer)
	mat:setColorMap(tex)
	mat:setAlphaCutoff(self.cutoff)
	mat:setRoughness(0.85)
	mat:setCullFace(CullFace.DoubleSided)
	mat:setWind(self.wind, 1.6, 0.14)
	self.keep[#self.keep + 1] = mat
	self.grassMat = mat

	local mesh = Plane.new(BLADE_W * 0.5, BLADE_H * 0.5)
	self.keep[#self.keep + 1] = mesh

	local total = PER_CHUNK * QUADS_PER_BLADE
	local half = CHUNKS * CHUNK_SIZE * 0.5
	local cell = CHUNK_SIZE * 0.5
	for cz = 0, CHUNKS - 1 do
		for cx = 0, CHUNKS - 1 do
			-- Chunk centre in world space. The GameObject goes here and the
			-- instances stay chunk-local, because the shader does
			-- ModelMatrix *= aInstancedTransform - the component's own model
			-- matrix still applies on top of every instance. Scattering in
			-- world space instead would leave every component sitting at the
			-- origin, and the bounding sphere is taken from the component's
			-- own position, so the culler would be testing the wrong place
			-- for all but one of them.
			local centerX = -half + cx * CHUNK_SIZE + cell
			local centerZ = -half + cz * CHUNK_SIZE + cell
			local radius = cell * 1.415 + BLADE_H

			local rc = RenderingInstancedComponent.new(mesh, mat, total, radius)
			rc:enableInstanceColors()
			-- One C++ call instead of `total` bound calls - the per-instance
			-- setTransform path costs a sol2 round-trip each, which is fine
			-- for a fixed field laid out once and hopeless for streaming.
			-- Tint range kept narrow so it reads as grass, not confetti.
			rc:scatterInstances(cz * CHUNKS + cx, CHUNK_SIZE, CHUNK_SIZE, BLADE_H,
				0.75, 1.45, PER_CHUNK, QUADS_PER_BLADE,
				Vec4.new(0.60, 0.72, 0.42, 1.0), Vec4.new(1.05, 1.14, 0.72, 1.0))

			local go = GameObject.new()
			go:addComponent(rc)
			go:setPosition(Vec3.new(centerX, 0.0, centerZ))
			scene:add(go)
			self.owned[#self.owned + 1] = go
			self.keep[#self.keep + 1] = rc
			self.chunks[#self.chunks + 1] = { rc = rc, x = centerX, z = centerZ, castsShadows = true }
		end
	end
	self.totalInstances = total * CHUNKS * CHUNKS
end

function GrassFieldSetup:buildSun()
	local proj = Projection.new()
	proj:perspective(70.0, 1.7777, 1.0, 400.0)
	self.shadowProjection = proj

	local sun = DirectionalLight.new(Vec4.new(1.0, 0.96, 0.86, 1.0), Vec3.new(0.66, -0.42, -0.18))
	sun:enableShadows(2048, 2048, proj, 1.0, 400.0, 1)
	local go = GameObject.new()
	go:addComponent(sun)
	scene:add(go)
	self.owned[#self.owned + 1] = go
	self.keep[#self.keep + 1] = sun
end

function GrassFieldSetup:update(time)
	if not camera then return end
	local total = PER_CHUNK * QUADS_PER_BLADE
	local cp = camera:getWorldPosition()

	local drawn = 0
	local visible = 0
	for i = 1, #self.chunks do
		local c = self.chunks[i]
		local n = total
		if self.lodEnabled then
			local dx = cp.x - c.x
			local dz = cp.z - c.z
			local d = math.sqrt(dx * dx + dz * dz)
			if d > LOD_THIN then
				n = 0
			elseif d > LOD_HALF then
				-- Thin the blades themselves. Continuous rather than
				-- stepped, so blades wink out one at a time instead of a
				-- whole chunk changing at once - the cheapest way to stop
				-- this reading as popping.
				local t = (d - LOD_HALF) / (LOD_THIN - LOD_HALF)
				n = math.floor(PER_CHUNK * (1.0 - t))
			elseif d > LOD_FULL then
				-- One quad per blade instead of a cross. The first half of
				-- the buffer is exactly that, by construction.
				n = PER_CHUNK
			end

			-- Shadow casting is the expensive half - the shadow pass redraws
			-- every caster, and a far chunk contributes nothing legible.
			local wantsShadows = d <= SHADOW_RADIUS
			if wantsShadows ~= c.castsShadows then
				c.castsShadows = wantsShadows
				if wantsShadows then c.rc:enableCastShadows() else c.rc:disableCastShadows() end
			end
		end
		c.rc:setNumberInstances(n)
		drawn = drawn + n
		if n > 0 then visible = visible + 1 end
	end
	self.drawnInstances = drawn
	self.drawnChunks = visible
end

function GrassFieldSetup:drawUI()
	if not imgui then return end
	imgui.text(string.format("%d chunks, %d blades, %d quads each",
		CHUNKS * CHUNKS, CHUNKS * CHUNKS * PER_CHUNK, QUADS_PER_BLADE))
	imgui.text(string.format("Drawing %d / %d quads, %d / %d chunks",
		self.drawnInstances, self.totalInstances, self.drawnChunks, CHUNKS * CHUNKS))
	imgui.separator()
	imgui.text("Density LOD: setNumberInstances() per chunk per")
	imgui.text("frame. The buffer is ordered so any prefix is a")
	imgui.text("valid thinner field, so nothing is rebuilt.")
	local l = imgui.checkbox("Distance LOD", self.lodEnabled)
	if l ~= self.lodEnabled then self.lodEnabled = l end
	imgui.separator()
	local c = imgui.sliderFloat("Alpha cutoff", self.cutoff, 0.05, 0.95)
	if c ~= self.cutoff then
		self.cutoff = c
		if self.grassMat then self.grassMat:setAlphaCutoff(c) end
	end
	local w = imgui.sliderFloat("Wind", self.wind, 0.0, 0.5)
	if w ~= self.wind then
		self.wind = w
		if self.grassMat then self.grassMat:setWind(w, 1.6, 0.14) end
	end
	imgui.separator()
	imgui.text("Shadows are cutout too: the shadow pass picks an")
	imgui.text("alpha-tested override material and samples this")
	imgui.text("same map, so blades cast blade-shaped shadows -")
	imgui.text("and inherit the wind, so they sway together.")
end

function GrassFieldSetup:destroy()
	if scene then
		for _, go in ipairs(self.owned) do
			pcall(function() scene:remove(go) end)
		end
	end
	self.owned = {}
	self.keep = {}
	self.chunks = {}
end

function GrassFieldSetup:serialize() return {} end
function GrassFieldSetup.deserialize(data) return GrassFieldSetup:new() end

return GrassFieldSetup
