-- Instanced, alpha-cutout grass.
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
-- Chunked on a grid rather than one big batch on purpose.
-- RenderingInstancedComponent carries a single bounding sphere for its
-- whole batch, and LOD switches per component, so one component holding
-- the entire field would be all-or-nothing for both. A component per cell
-- lets distant cells cull out individually.
local GrassFieldSetup = class('GrassFieldSetup')

local CHUNKS = 4            -- CHUNKS x CHUNKS components
local PER_CHUNK = 650       -- blades per component
local CHUNK_SIZE = 26.0
local BLADE_W = 2.6
local BLADE_H = 3.4

function GrassFieldSetup:initialize()
	self.owned = {}
	self.keep = {}
	self.chunks = {}
	self.cutoff = 0.5
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

	local mesh = Plane.new(CHUNKS * CHUNK_SIZE, CHUNKS * CHUNK_SIZE)
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
		ShaderUsage.DeferredRenderer_Gbuffer)
	mat:setColorMap(tex)
	mat:setAlphaCutoff(self.cutoff)
	mat:setRoughness(0.85)
	mat:setCullFace(CullFace.DoubleSided)
	self.keep[#self.keep + 1] = mat
	self.grassMat = mat

	-- Two quads at right angles, so a blade cluster reads as solid from any
	-- direction instead of vanishing edge-on.
	local mesh = Plane.new(BLADE_W * 0.5, BLADE_H * 0.5)
	self.keep[#self.keep + 1] = mesh

	local half = CHUNKS * CHUNK_SIZE * 0.5
	local cell = CHUNK_SIZE * 0.5
	for cz = 0, CHUNKS - 1 do
		for cx = 0, CHUNKS - 1 do
			-- Chunk centre in world space. The GameObject goes here and the
			-- instance transforms below stay chunk-local, because the shader
			-- does ModelMatrix *= aInstancedTransform - the component's own
			-- model matrix still applies on top of every instance. Scattering
			-- in world space instead would leave all 16 components sitting at
			-- the origin, and the bounding sphere is taken from the component's
			-- own position, so the culler would be testing the wrong place for
			-- 15 of them.
			local centerX = -half + cx * CHUNK_SIZE + cell
			local centerZ = -half + cz * CHUNK_SIZE + cell
			-- Radius covers the cell's half-diagonal plus a blade's own height,
			-- so culling can't clip a chunk whose blades still poke into view.
			local radius = cell * 1.415 + BLADE_H
			local rc = RenderingInstancedComponent.new(mesh, mat, PER_CHUNK * 2, radius)

			local n = 0
			for b = 1, PER_CHUNK do
				-- Deterministic scatter: same field every launch, so a
				-- screenshot comparison stays meaningful.
				local s1 = math.sin(b * 12.9898 + cx * 3.1 + cz * 7.7) * 43758.5453
				local s2 = math.sin(b * 78.233 + cx * 5.3 + cz * 2.9) * 43758.5453
				local s3 = math.sin(b * 39.425 + cx * 1.7 + cz * 4.1) * 43758.5453
				local rx = s1 - math.floor(s1)
				local rz = s2 - math.floor(s2)
				local rr = s3 - math.floor(s3)

				local x = (rx - 0.5) * CHUNK_SIZE
				local z = (rz - 0.5) * CHUNK_SIZE
				local yaw = rr * math.pi
				local scl = 0.75 + rr * 0.7

				for q = 0, 1 do
					n = n + 1
					local m = Matrix.new()
					m:scale(scl, scl, scl)
					m:rotationY(yaw + q * math.pi * 0.5)
					m:translate(x, BLADE_H * 0.5 * scl, z)
					rc:setTransform(n, m)
				end
			end
			rc:updateTransforms()

			local go = GameObject.new()
			go:addComponent(rc)
			go:setPosition(Vec3.new(centerX, 0.0, centerZ))
			scene:add(go)
			self.owned[#self.owned + 1] = go
			self.keep[#self.keep + 1] = rc
			self.chunks[#self.chunks + 1] = rc
		end
	end
end

function GrassFieldSetup:buildSun()
	local proj = Projection.new()
	proj:perspective(70.0, 1.7777, 1.0, 400.0)
	self.shadowProjection = proj

	local sun = DirectionalLight.new(Vec4.new(1.0, 0.96, 0.86, 1.0), Vec3.new(-0.55, -0.72, -0.42))
	sun:enableShadows(2048, 2048, proj, 1.0, 400.0, 1)
	local go = GameObject.new()
	go:addComponent(sun)
	scene:add(go)
	self.owned[#self.owned + 1] = go
	self.keep[#self.keep + 1] = sun
end

function GrassFieldSetup:drawUI()
	if not imgui then return end
	imgui.text(tostring(CHUNKS * CHUNKS) .. " instanced draw calls, " ..
		tostring(CHUNKS * CHUNKS * PER_CHUNK) .. " blades (2 quads each).")
	imgui.text("Alpha-tested cutout: the blade texture's alpha")
	imgui.text("decides which fragments exist, so a deferred")
	imgui.text("G-buffer can hold foliage at all.")
	imgui.separator()
	local c = imgui.sliderFloat("Alpha cutoff", self.cutoff, 0.05, 0.95)
	if c ~= self.cutoff then
		self.cutoff = c
		if self.grassMat then self.grassMat:setAlphaCutoff(c) end
	end
	imgui.text("Note: blades cast solid-quad shadows - the shadow")
	imgui.text("pass has no cutout variant yet.")
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
