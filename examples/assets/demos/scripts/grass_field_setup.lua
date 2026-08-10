-- Instanced, alpha-cutout grass: streamed, with distance density LOD.
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
-- The field is unbounded. A fixed RING x RING pool of chunks follows the
-- camera; when one falls off the back it is moved to the front and
-- re-scattered for its new world cell. Nothing is allocated or destroyed
-- after startup - the pool is the entire memory and draw-call budget, no
-- matter how far you fly. That works because scatterInstances() is
-- deterministic from a seed, so a given world cell always regenerates the
-- same grass: walk away and come back and it is unchanged.
--
-- Three things then scale what the pool costs, all per chunk per frame:
--
--   visibility  the component's bounding sphere, frustum-culled for free
--   density     setNumberInstances(), continuous, nothing rebuilt
--   shadows     dropped past a radius, since the shadow pass redraws them
--
-- Density works only because scatterInstances() lays the buffer out
-- quad-major and in hash order, which makes any prefix of it a valid,
-- spatially even, lower-quality version of the same field. Half the
-- buffer is one quad per blade instead of a cross; a quarter also thins
-- the blades. The draw call reads the live instance count every frame, so
-- changing it rebuilds nothing.
local GrassFieldSetup = class('GrassFieldSetup')

local RING = 11              -- RING x RING chunks kept alive around the camera
local PER_CHUNK = 700        -- blades per chunk
local QUADS_PER_BLADE = 2
local CHUNK_SIZE = 26.0
local BLADE_W = 2.6
local BLADE_H = 3.4

-- Re-scatters allowed per frame.
--
-- This was 4, which was far too low and was the whole of the "grass
-- flickers when the camera moves" bug. Crossing a cell boundary diagonally
-- retires a row *and* a column - 21 chunks on an 11x11 pool, and every
-- chunk on a teleport - so a budget of 4 left a backlog draining over six
-- or more frames, with chunks visibly popping to their new positions a few
-- at a time. It read as flickering rather than as lag because each chunk
-- jumps the full width of the field.
--
-- It looked like a Vulkan/Metal bug only because those are vsync-capped at
-- 60fps here while GL runs at 300+: the same six-frame backlog is 100ms on
-- one and 20ms on the other. Nothing about it was GPU-side.
--
-- Sized to drain a boundary crossing in a single frame. ScatterInstances()
-- is C++ and costs roughly 1400 matrices per chunk, so a full row+column is
-- well under a millisecond - cheap enough that spreading it was never worth
-- the artifact it caused.
local RESCATTER_BUDGET = 2 * RING + 4

-- Distance bands, measured to the chunk centre. The thinning band is kept
-- deliberately short: thinning removes whole blades, so a long band reads
-- as isolated dots on bare ground rather than as distant grass. Holding
-- half density most of the way out and cutting late looks better than a
-- gentle fade, until there is an impostor card to hand off to.
-- Distance bands, measured to the chunk centre. In order outward:
--   <= LOD_FULL        crossed quads, every blade
--   <= LOD_MESH_SWAP   one quad per blade (the buffer's first half)
--   <= LOD_THIN        clump impostor, thinning to nothing
--
-- LOD_MESH_SWAP is RenderingComponent::AddLOD's own switch, evaluated per
-- component by the renderer; the other two are instance counts this script
-- sets. They have to stay ordered - an earlier band that overlaps a later
-- one makes the later one unreachable.
local LOD_FULL = 60.0
local LOD_MESH_SWAP = 70.0
local LOD_THIN = 170.0
-- How many blades one impostor card stands in for. Drives both its width
-- and the instance count once it is in use - a clump covering three blades
-- at the same count would put three times the green on the ground.
local CLUMP_BLADES = 3
local SHADOW_RADIUS = 70.0

function GrassFieldSetup:initialize()
	self.owned = {}
	self.keep = {}
	self.chunks = {}
	self.pending = {}
	self.cutoff = 0.5
	self.wind = 0.16
	self.lodEnabled = true
	self.streaming = true
	self.drawnInstances = 0
	self.drawnChunks = 0
	self.rescatters = 0
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

-- Stable per-cell seed: the same world cell must always produce the same
-- grass, or flying out and back would reshuffle the field.
local function cellSeed(cx, cz)
	local s = (cx * 374761393 + cz * 668265263) % 2147483647
	if s < 0 then s = s + 2147483647 end
	return s
end

function GrassFieldSetup:buildGround()
	local mat = GenericShaderMaterial.new(
		ShaderUsage.Color + ShaderUsage.Diffuse + ShaderUsage.PBR +
		ShaderUsage.DeferredRenderer_Gbuffer)
	mat:setColor(Vec4.new(0.16, 0.21, 0.12, 1.0))
	mat:setRoughness(0.9)
	self.keep[#self.keep + 1] = mat

	-- Follows the camera in update() rather than being a fixed slab, since
	-- the grass above it is unbounded. Wider than the chunk ring so its
	-- edge is never the thing you notice.
	local mesh = Plane.new(RING * CHUNK_SIZE * 2.2, RING * CHUNK_SIZE * 2.2)
	self.keep[#self.keep + 1] = mesh

	local go = GameObject.new()
	local rc = RenderingComponent.new(mesh, mat)
	rc:disableCastShadows()
	go:addComponent(rc)
	go:setRotation(Vec3.new(math.rad(-90), 0, 0))
	scene:add(go)
	self.owned[#self.owned + 1] = go
	self.keep[#self.keep + 1] = rc
	self.groundGO = go
end

function GrassFieldSetup:scatterChunk(c, cellX, cellZ)
	c.cellX = cellX
	c.cellZ = cellZ
	c.x = cellX * CHUNK_SIZE
	c.z = cellZ * CHUNK_SIZE
	c.go:setPosition(Vec3.new(c.x, 0.0, c.z))
	-- One C++ call. The per-instance setTransform path costs a sol2
	-- round-trip each, which is fine for a field laid out once at load and
	-- hopeless when chunks regenerate as the camera moves - which is the
	-- whole reason ScatterInstances() exists.
	c.rc:scatterInstances(cellSeed(cellX, cellZ), CHUNK_SIZE, CHUNK_SIZE, BLADE_H,
		0.75, 1.45, PER_CHUNK, QUADS_PER_BLADE,
		Vec4.new(0.60, 0.72, 0.42, 1.0), Vec4.new(1.05, 1.14, 0.72, 1.0))
	self.rescatters = self.rescatters + 1
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

	-- Far LOD: one wider card carrying a tuft of blades instead of a single
	-- blade, so a distant chunk draws the same amount of green through far
	-- fewer fragments. This is the hand-off the thinning band alone could
	-- not do - thinning removes whole blades, which at distance reads as
	-- isolated dots on bare ground rather than as grass.
	--
	-- Mesh LOD on an instanced component only started working once AddLOD
	-- stopped clearing isInstanced; the per-instance transforms carry over
	-- to the LOD level untouched, so the clumps stand exactly where their
	-- blades did.
	local clumpTex = Texture.new()
	clumpTex:loadTexture(ASSETS_PATH .. "textures/grass_clump.png", TextureType.Texture, true, 0)
	clumpTex:setMinMagFilter(TextureFilter.LinearMipmapLinear, TextureFilter.Linear)
	self.keep[#self.keep + 1] = clumpTex

	local lodMat = GenericShaderMaterial.new(
		ShaderUsage.Texture + ShaderUsage.Diffuse + ShaderUsage.PBR +
		ShaderUsage.AlphaTest + ShaderUsage.InstancedRendering +
		ShaderUsage.InstancedColor + ShaderUsage.VertexWind +
		ShaderUsage.DeferredRenderer_Gbuffer)
	lodMat:setColorMap(clumpTex)
	lodMat:setAlphaCutoff(self.cutoff)
	lodMat:setRoughness(0.85)
	lodMat:setCullFace(CullFace.DoubleSided)
	lodMat:setWind(self.wind, 1.6, 0.14)
	self.keep[#self.keep + 1] = lodMat
	self.lodMat = lodMat

	local lodMesh = Plane.new(BLADE_W * 0.5 * CLUMP_BLADES, BLADE_H * 0.5)
	self.keep[#self.keep + 1] = lodMesh
	self.lodMesh = lodMesh

	if renderer and renderer.enableLOD then renderer:enableLOD() end

	local total = PER_CHUNK * QUADS_PER_BLADE
	local halfRing = math.floor(RING / 2)
	local radius = CHUNK_SIZE * 0.5 * 1.415 + BLADE_H
	for oz = -halfRing, halfRing do
		for ox = -halfRing, halfRing do
			-- The GameObject carries the chunk's world position and the
			-- instances stay chunk-local, because the shader does
			-- ModelMatrix *= aInstancedTransform - the component's own model
			-- matrix still applies on top of every instance. That is also
			-- what makes a chunk relocatable: moving the GameObject moves
			-- every blade with it, no transform rewrite needed.
			local rc = RenderingInstancedComponent.new(mesh, mat, total, radius)
			rc:addLOD(self.lodMesh, LOD_MESH_SWAP, self.lodMat)
			rc:enableInstanceColors()

			local go = GameObject.new()
			go:addComponent(rc)
			scene:add(go)
			self.owned[#self.owned + 1] = go
			self.keep[#self.keep + 1] = rc

			local c = { rc = rc, go = go, ox = ox, oz = oz, castsShadows = true }
			self:scatterChunk(c, ox, oz)
			self.chunks[#self.chunks + 1] = c
		end
	end
	self.totalInstances = total * RING * RING
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
	local camCellX = math.floor(cp.x / CHUNK_SIZE + 0.5)
	local camCellZ = math.floor(cp.z / CHUNK_SIZE + 0.5)

	if self.streaming and self.groundGO then
		-- Snapped to the chunk grid, not to the camera exactly, so the
		-- ground never slides underneath the grass.
		self.groundGO:setPosition(Vec3.new(camCellX * CHUNK_SIZE, 0.0, camCellZ * CHUNK_SIZE))
	end

	-- Nearest first: if the budget is ever exhausted, the chunks left over
	-- are the far ones, where being a frame late is invisible.
	if self.streaming then
		local pending = self.pending
		for i = 1, #pending do pending[i] = nil end
		local n = 0
		for i = 1, #self.chunks do
			local c = self.chunks[i]
			if c.cellX ~= camCellX + c.ox or c.cellZ ~= camCellZ + c.oz then
				n = n + 1
				pending[n] = c
				local dx, dz = cp.x - c.x, cp.z - c.z
				c.pendD = dx * dx + dz * dz
			end
		end
		if n > 1 then table.sort(pending, function(p, q) return p.pendD < q.pendD end) end
		for i = 1, math.min(n, RESCATTER_BUDGET) do
			local c = pending[i]
			self:scatterChunk(c, camCellX + c.ox, camCellZ + c.oz)
		end
	end

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
			elseif d > LOD_MESH_SWAP then
				-- Past here the renderer has swapped this chunk to the clump
				-- impostor, so each instance is worth CLUMP_BLADES blades and
				-- the count comes down to match. Still thinning to nothing by
				-- LOD_THIN, just from a lower starting point.
				local t = (d - LOD_MESH_SWAP) / (LOD_THIN - LOD_MESH_SWAP)
				n = math.floor((PER_CHUNK / CLUMP_BLADES) * (1.0 - t))
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
	imgui.text(string.format("%d chunks in a %dx%d ring, %d blades each",
		RING * RING, RING, RING, PER_CHUNK))
	imgui.text(string.format("Drawing %d / %d quads, %d / %d chunks",
		self.drawnInstances, self.totalInstances, self.drawnChunks, RING * RING))
	imgui.text(string.format("%d chunk regenerations so far", self.rescatters))
	imgui.separator()
	imgui.text("Fly in any direction - the field is unbounded. The")
	imgui.text("pool is fixed, so nothing is allocated as you go;")
	imgui.text("chunks that fall behind move to the front and are")
	imgui.text("re-scattered from a per-cell seed, so a place you")
	imgui.text("return to looks exactly as you left it.")
	imgui.separator()
	local s = imgui.checkbox("Streaming", self.streaming)
	if s ~= self.streaming then self.streaming = s end
	local l = imgui.checkbox("Distance LOD", self.lodEnabled)
	if l ~= self.lodEnabled then self.lodEnabled = l end
	imgui.separator()
	local c = imgui.sliderFloat("Alpha cutoff", self.cutoff, 0.05, 0.95)
	if c ~= self.cutoff then
		self.cutoff = c
		if self.grassMat then self.grassMat:setAlphaCutoff(c) end
		if self.lodMat then self.lodMat:setAlphaCutoff(c) end
	end
	local w = imgui.sliderFloat("Wind", self.wind, 0.0, 0.5)
	if w ~= self.wind then
		self.wind = w
		if self.grassMat then self.grassMat:setWind(w, 1.6, 0.14) end
		if self.lodMat then self.lodMat:setWind(w, 1.6, 0.14) end
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
