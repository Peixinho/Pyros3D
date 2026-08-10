-- Bisection harness for the "grass flickers on Vulkan and Metal when the
-- camera gets close" report.
--
-- The grass demo changes five things at once as you fly toward it: chunks
-- re-scatter, instance counts change, shadow casting toggles, the renderer
-- swaps in an LOD mesh, and that LOD mesh brings a second material into the
-- same frame. Any of those could be the trigger, and the flicker is only
-- visible live, so the point of this scene is to let each one be switched
-- off independently until it stops.
--
-- The camera drives itself in and out through the LOD switch distance, so
-- the trigger condition repeats continuously without needing input. Press
-- TAB to take the mouse and fly manually; the auto-drive stands down while
-- the mouse is captured (camera_fly only writes position then, so the two
-- never fight).
--
-- Turn things ON one at a time from the all-off state. The first one that
-- brings the flicker back is the culprit.
local LodFlickerTest = class('LodFlickerTest')

-- Cell counts. GRID_SMALL is comfortable to watch; GRID_LARGE matches the
-- grass demo's 11x11 pool, because scale is one of the few differences
-- left between this scene (which does not flicker) and that one (which
-- does), and a pool four times bigger changes how far streaming lags and
-- how many pipelines exist.
local GRID_SMALL = 5
local GRID_LARGE = 11
local GRID = GRID_SMALL
local PER_CELL = 520         -- items per component (dense enough that a
                             -- one-frame glitch is obvious, not a judgement call)
local QUADS_PER_ITEM = 2
local CELL_SIZE = 26.0
local ITEM_W = 2.6
local ITEM_H = 3.4

-- The LOD switch and the density bands both straddle this, so the auto
-- camera sweeps from well inside to well outside it.
local LOD_MESH_SWAP = 70.0
local LOD_FULL = 60.0
local LOD_THIN = 170.0
local SHADOW_RADIUS = 70.0
local CLUMP_ITEMS = 3

function LodFlickerTest:initialize()
	self.owned = {}
	self.keep = {}
	self.cells = {}
	self.cellKeep = {}

	-- Every suspect, all off. Rebuild-scoped ones are marked; the rest take
	-- effect immediately.
	self.optMeshLOD = false        -- rebuild: give each component an AddLOD level
	self.optLodMaterial = false    -- rebuild: that level gets its OWN material
	self.optDensity = false        -- per frame: setNumberInstances by distance
	self.optStreaming = false      -- per frame: re-scatter cells as they move
	-- Streaming does two separable things. These pick which half runs, so
	-- "streaming causes it" can be narrowed to one of them:
	--   move    the cell's GameObject teleports to its new position
	--   rewrite scatterInstances() rewrites the transform+colour buffers
	self.streamMove = true
	self.streamRewrite = true
	self.optShadowToggle = false   -- per frame: enable/disableCastShadows
	-- rebuild: compile the materials with ShaderUsage.VertexWind. The grass
	-- demo has this and this scene did not, which is the largest single
	-- difference between the one that flickers and the one that does not.
	-- Wind writes uWind into ObjectMatrixUniforms and uTimeParams into
	-- VertexFrameUniforms - both per-object dynamic UBOs - and makes the
	-- latter re-upload every frame instead of on its dirty check.
	self.optWind = false
	-- rebuild: switch the pool to the grass demo's 11x11
	self.optLargeField = false
	self.autoCamera = true

	self.t = 0
	self.drawn = 0
	self.rebuilds = 0
	self.lastLodMeshLOD = false
	self.lastLodMaterial = false
	self.lastWind = false
	self.lastLargeField = false
end

function LodFlickerTest:init(owner)
	self.owner = owner
	if not scene then error("LodFlickerTest:init - scene is nil") end
	if not ASSETS_PATH then error("LodFlickerTest:init - ASSETS_PATH is nil") end

	if renderer and renderer.enableLOD then renderer:enableLOD() end

	self:buildShared()
	self:buildGround()
	self:buildField()
	self:buildSun()
end

local function cellSeed(cx, cz)
	local s = (cx * 374761393 + cz * 668265263) % 2147483647
	if s < 0 then s = s + 2147483647 end
	return s
end

function LodFlickerTest:buildShared()
	local tex = Texture.new()
	tex:loadTexture(ASSETS_PATH .. "textures/grass_blades.png", TextureType.Texture, true, 0)
	tex:setMinMagFilter(TextureFilter.LinearMipmapLinear, TextureFilter.Linear)
	self.keep[#self.keep + 1] = tex

	local clump = Texture.new()
	clump:loadTexture(ASSETS_PATH .. "textures/grass_clump.png", TextureType.Texture, true, 0)
	clump:setMinMagFilter(TextureFilter.LinearMipmapLinear, TextureFilter.Linear)
	self.keep[#self.keep + 1] = clump

	local usage = ShaderUsage.Texture + ShaderUsage.Diffuse + ShaderUsage.PBR +
		ShaderUsage.AlphaTest + ShaderUsage.InstancedRendering +
		ShaderUsage.InstancedColor + ShaderUsage.DeferredRenderer_Gbuffer
	if self.optWind then usage = usage + ShaderUsage.VertexWind end

	local mat = GenericShaderMaterial.new(usage)
	mat:setColorMap(tex)
	mat:setAlphaCutoff(0.5)
	mat:setRoughness(0.85)
	mat:setCullFace(CullFace.DoubleSided)
	if self.optWind then mat:setWind(0.16, 1.6, 0.14) end
	self.keep[#self.keep + 1] = mat
	self.mat = mat

	-- Only used when optLodMaterial is on. Identical apart from the map, so
	-- "second material in the same frame" is the only variable it adds.
	local lodMat = GenericShaderMaterial.new(usage)
	lodMat:setColorMap(clump)
	lodMat:setAlphaCutoff(0.5)
	lodMat:setRoughness(0.85)
	lodMat:setCullFace(CullFace.DoubleSided)
	if self.optWind then lodMat:setWind(0.16, 1.6, 0.14) end
	self.keep[#self.keep + 1] = lodMat
	self.lodMat = lodMat

	self.mesh = Plane.new(ITEM_W * 0.5, ITEM_H * 0.5)
	self.keep[#self.keep + 1] = self.mesh
	self.lodMesh = Plane.new(ITEM_W * 0.5 * CLUMP_ITEMS, ITEM_H * 0.5)
	self.keep[#self.keep + 1] = self.lodMesh
end

function LodFlickerTest:buildGround()
	local mat = GenericShaderMaterial.new(
		ShaderUsage.Color + ShaderUsage.Diffuse + ShaderUsage.PBR +
		ShaderUsage.DeferredRenderer_Gbuffer)
	mat:setColor(Vec4.new(0.16, 0.21, 0.12, 1.0))
	mat:setRoughness(0.9)
	self.keep[#self.keep + 1] = mat

	local mesh = Plane.new(GRID * CELL_SIZE * 3.0, GRID * CELL_SIZE * 3.0)
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

function LodFlickerTest:scatterCell(c, cellX, cellZ, force)
	c.cellX, c.cellZ = cellX, cellZ
	if force or self.streamMove then
		c.x, c.z = cellX * CELL_SIZE, cellZ * CELL_SIZE
		c.go:setPosition(Vec3.new(c.x, 0.0, c.z))
	end
	if force or self.streamRewrite then
		c.rc:scatterInstances(cellSeed(cellX, cellZ), CELL_SIZE, CELL_SIZE, ITEM_H,
			0.75, 1.45, PER_CELL, QUADS_PER_ITEM,
			Vec4.new(0.60, 0.72, 0.42, 1.0), Vec4.new(1.05, 1.14, 0.72, 1.0))
	end
end

-- Destroys and recreates the field. Needed for the two rebuild-scoped
-- options: AddLOD cannot be undone on a live component, so switching it
-- means building fresh ones.
function LodFlickerTest:buildField()
	for _, c in ipairs(self.cells) do
		pcall(function() scene:remove(c.go) end)
	end
	self.cells = {}
	self.cellKeep = {}
	self.rebuilds = self.rebuilds + 1
	self.lastLodMeshLOD = self.optMeshLOD
	self.lastLodMaterial = self.optLodMaterial
	self.lastWind = self.optWind
	self.lastLargeField = self.optLargeField
	GRID = self.optLargeField and GRID_LARGE or GRID_SMALL

	local total = PER_CELL * QUADS_PER_ITEM
	local half = math.floor(GRID / 2)
	local radius = CELL_SIZE * 0.5 * 1.415 + ITEM_H
	for oz = -half, half do
		for ox = -half, half do
			local rc = RenderingInstancedComponent.new(self.mesh, self.mat, total, radius)
			if self.optMeshLOD then
				rc:addLOD(self.lodMesh, LOD_MESH_SWAP,
					self.optLodMaterial and self.lodMat or self.mat)
			end
			rc:enableInstanceColors()

			local go = GameObject.new()
			go:addComponent(rc)
			scene:add(go)
			-- Deliberately not in self.owned: these are rebuilt whenever a
			-- rebuild-scoped option changes, and destroy() would otherwise
			-- accumulate a dead handle per toggle.
			local c = { rc = rc, go = go, ox = ox, oz = oz, casts = true }
			self:scatterCell(c, ox, oz, true)
			self.cells[#self.cells + 1] = c
			self.cellKeep[#self.cellKeep + 1] = rc
		end
	end
	self.total = total
end

function LodFlickerTest:buildSun()
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

function LodFlickerTest:update(time)
	if not camera then return end

	-- Sweep in and out across LOD_MESH_SWAP so the reported trigger repeats
	-- on its own. Stands down while the mouse is captured, since that is
	-- when camera_fly owns the position.
	if self.autoCamera and not SDL_RelativeMouseActive then
		self.t = self.t + 0.016
		local z = 40.0 + (math.sin(self.t * 0.45) * 0.5 + 0.5) * 120.0
		camera:setPosition(Vec3.new(0.0, 8.0, z))
	end

	local cp = camera:getWorldPosition()
	local camCellX = math.floor(cp.x / CELL_SIZE + 0.5)
	local camCellZ = math.floor(cp.z / CELL_SIZE + 0.5)

	local budget = 4
	local drawn = 0
	for i = 1, #self.cells do
		local c = self.cells[i]

		if self.optStreaming then
			local wantX, wantZ = camCellX + c.ox, camCellZ + c.oz
			if (c.cellX ~= wantX or c.cellZ ~= wantZ) and budget > 0 then
				budget = budget - 1
				self:scatterCell(c, wantX, wantZ)
			end
		end

		local n = self.total
		local dx, dz = cp.x - c.x, cp.z - c.z
		local d = math.sqrt(dx * dx + dz * dz)

		if self.optDensity then
			if d > LOD_THIN then
				n = 0
			elseif d > LOD_MESH_SWAP then
				local t = (d - LOD_MESH_SWAP) / (LOD_THIN - LOD_MESH_SWAP)
				n = math.floor((PER_CELL / CLUMP_ITEMS) * (1.0 - t))
			elseif d > LOD_FULL then
				n = PER_CELL
			end
		end
		c.rc:setNumberInstances(n)
		drawn = drawn + n

		if self.optShadowToggle then
			local want = d <= SHADOW_RADIUS
			if want ~= c.casts then
				c.casts = want
				if want then c.rc:enableCastShadows() else c.rc:disableCastShadows() end
			end
		end
	end
	self.drawn = drawn
	self.camZ = cp.z
end

function LodFlickerTest:drawUI()
	if not imgui then return end

	-- Options first and always visible. They were previously below a block
	-- of explanatory text in a narrow docked panel, and the two streaming
	-- sub-toggles were nested behind the streaming checkbox - so the two
	-- controls that matter most could not be found.
	local a = imgui.checkbox("1 Mesh LOD (AddLOD)", self.optMeshLOD)
	if a ~= self.optMeshLOD then self.optMeshLOD = a end
	local b = imgui.checkbox("2 ...LOD gets its own material", self.optLodMaterial)
	if b ~= self.optLodMaterial then self.optLodMaterial = b end
	if self.optMeshLOD ~= self.lastLodMeshLOD or self.optLodMaterial ~= self.lastLodMaterial then
		self:buildField()
	end

	local c = imgui.checkbox("3 Density (setNumberInstances)", self.optDensity)
	if c ~= self.optDensity then self.optDensity = c end

	local d = imgui.checkbox("4 Streaming (re-scatter)", self.optStreaming)
	if d ~= self.optStreaming then self.optStreaming = d end

	-- Always shown, not nested: these are the decisive pair. They only do
	-- anything while 4 is on, which the label says rather than the UI
	-- hiding them.
	local m = imgui.checkbox("4a  ...move cell GameObject", self.streamMove)
	if m ~= self.streamMove then self.streamMove = m end
	local w = imgui.checkbox("4b  ...rewrite instance buffers", self.streamRewrite)
	if w ~= self.streamRewrite then self.streamRewrite = w end

	local e = imgui.checkbox("5 Shadow toggling", self.optShadowToggle)
	if e ~= self.optShadowToggle then self.optShadowToggle = e end
	local g = imgui.checkbox("6 Wind (VertexWind)", self.optWind)
	if g ~= self.optWind then self.optWind = g end
	local h = imgui.checkbox("7 Large field (11x11, as grass demo)", self.optLargeField)
	if h ~= self.optLargeField then self.optLargeField = h end
	if self.optLargeField ~= self.lastLargeField then
		self:buildField()
	end
	if self.optWind ~= self.lastWind then
		-- Wind is a ShaderUsage flag, fixed when the material is built, so
		-- toggling it rebuilds the materials and the field.
		self:buildShared()
		self:buildField()
	end

	imgui.separator()
	local f = imgui.checkbox("Auto camera sweep", self.autoCamera)
	if f ~= self.autoCamera then self.autoCamera = f end
	imgui.text(string.format("camera z %.0f  (LOD swaps at %.0f)", self.camZ or 0, LOD_MESH_SWAP))
	imgui.text(string.format("%d cells, %d / %d quads, %d rebuild(s)",
		#self.cells, self.drawn, (self.total or 0) * #self.cells, self.rebuilds))

	imgui.separator()
	imgui.text("All off = should be rock solid. Turn ONE on at")
	imgui.text("a time; the first that flickers is the cause.")
	imgui.text("If 4 flickers, use 4a/4b to say which half:")
	imgui.text("4b only = a buffer is rewritten while in use.")
	imgui.text("4a only = moving a component mid-frame.")
end

function LodFlickerTest:destroy()
	if scene then
		for _, go in ipairs(self.owned) do
			pcall(function() scene:remove(go) end)
		end
	end
	self.owned = {}
	self.keep = {}
	self.cells = {}
	self.cellKeep = {}
end

function LodFlickerTest:serialize() return {} end
function LodFlickerTest.deserialize(data) return LodFlickerTest:new() end

return LodFlickerTest
