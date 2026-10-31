-- The island and its water: a smooth opaque surface whose material asks to be a mirror (setSSREnabled - the
-- renderer's mirror floor), the island found in it by the screen's reflections, and the sky's own reflection where
-- they find nothing. Only what the screen shows can be reflected - the usual SSR trade-off; the renderer's
-- ray-traced reflections (setRayTracedReflections) find the rest.
local IslandSSRSetup = class('IslandSSRSetup')

local WATER_Y = 6.8

-- Plane.new(h, h) spans -h..h, so a tile is TILE_HALF*2 across, and its
-- UVs run 0..1 over that whole quad: the wave normal map tiled across the
-- water by tiling the geometry - GRID x GRID identical quads sharing one
-- mesh and one material.
local TILE_HALF = 25
local TILE_SIZE = TILE_HALF * 2
local GRID = 19 -- 19 * 50 = 950 units of water

-- Waves are scrolled by translating the whole lattice and wrapping at
-- TILE_SIZE. Every tile is identical, so a whole-tile shift is a no-op -
-- the wrap is invisible and needs no UV animation the shader can't do.
local SCROLL_X = 2.2
local SCROLL_Z = 0.9

-- Reflections here span most of the frame, so the coarse march needs a
-- stride well above 1 pixel: SSR_COARSE_STEPS (128) * stride is the hard
-- reach limit, and at stride 1 nothing further than 128px up the screen
-- can ever reflect - which left the treeline out entirely. 6px * 128 is
-- ~768px of reach, enough to pull the whole island down into the water.
-- The second value is view-space and only clips the ray's far end.
local SSR_STEP = 6.0
local SSR_MAX = 500.0

function IslandSSRSetup:initialize()
	self.owned = {}
	self.keep = {}
	self.tiles = nil
	self.waterMat = nil
	self.ssrOn = true
	self.ssrStep = SSR_STEP
	self.roughness = 0.05
	self.reflectivity = 0.55
	self.scroll = 1.0
end

function IslandSSRSetup:init(owner)
	self.owner = owner
	if not scene then error("IslandSSRSetup:init - scene is nil") end
	if not renderer then error("IslandSSRSetup:init - renderer is nil") end
	if not ASSETS_PATH then error("IslandSSRSetup:init - ASSETS_PATH is nil") end

	-- Same opt-in the SSR Test demo does (the manifest entry asks for it
	-- too; doing it here keeps the scene self-contained and gives the UI
	-- checkbox below a known starting state).
	if renderer.setReflections then renderer:setReflections(true, SSR_MAX) end
	self.ssrOn = true

	local island = Model.new(ASSETS_PATH .. "island.p3dm", true)
	local gIsland = GameObject.new()
	local rIsland = RenderingComponent.new(island, ShaderUsage.Diffuse)
	rIsland:disableCullTest()
	gIsland:addComponent(rIsland)
	scene:add(gIsland)
	self.owned[#self.owned + 1] = gIsland
	self.keep[#self.keep + 1] = island
	self.keep[#self.keep + 1] = rIsland

	self:buildWater()
end

function IslandSSRSetup:buildWater()
	-- NOT assets/normal.png (the planar demo's wave map): that one averages
	-- 19 degrees of tilt and peaks at 48. A mirror doubles normal error into
	-- ray error, so at the grazing angles water is actually seen at, ~38
	-- degrees of scatter throws most reflection rays off the island and up
	-- into empty sky, where the G-buffer has nothing to hit - which is
	-- exactly why only near-camera water was reflecting anything. It's a
	-- fine map for the planar demo, where it only perturbs a specular
	-- highlight and never steers a ray. This one is a tileable sum-of-sines
	-- swell capped at ~9 degrees (3.4 mean), gentle enough that a reflected
	-- ray still lands roughly where a flat surface would send it.
	--
	-- Mipmapped so distant tiles average their ripples back towards flat -
	-- an unfiltered wave normal map on a mirror surface turns SSR into
	-- per-pixel noise at range.
	local normalMap = Texture.new()
	normalMap:loadTexture(ASSETS_PATH .. "waterNormalSoft.png", TextureType.Texture, true, 0)
	normalMap:setMinMagFilter(TextureFilter.LinearMipmapLinear, TextureFilter.Linear)
	self.keep[#self.keep + 1] = normalMap

	-- PBR + a low roughness: the screen's reflections stop at a roughness of 0.4.
	local mat = GenericShaderMaterial.new(
		ShaderUsage.Color + ShaderUsage.BumpMapping + ShaderUsage.EnvMap +
		ShaderUsage.PBR)
	-- The sky's cubemap, as the material's environment map (the scene's
	-- sky is what the renderer reflects where the screen's reflections
	-- find nothing).
	local sky = Texture.new()
	local faces = {
		{ "posx", TextureType.CubemapPositive_X }, { "negx", TextureType.CubemapNegative_X },
		{ "posy", TextureType.CubemapPositive_Y }, { "negy", TextureType.CubemapNegative_Y },
		{ "posz", TextureType.CubemapPositive_Z }, { "negz", TextureType.CubemapNegative_Z },
	}
	for _, f in ipairs(faces) do
		sky:loadTexture(ASSETS_PATH .. "textures/skybox/" .. f[1] .. ".png", f[2], false, 0)
	end
	sky:setRepeat(TextureRepeat.ClampToEdge, TextureRepeat.ClampToEdge, TextureRepeat.ClampToEdge)
	self.keep[#self.keep + 1] = sky

	-- Still matters: it's what the env mix leaves behind (1 - reflectivity
	-- of it) and it tints the water where the sky is dim. Can't be the
	-- near-black deep-ocean value it started as - unlit, that read as a
	-- black hole under the island.
	mat:setColor(Vec4.new(0.09, 0.24, 0.30, 1.0))
	mat:setEnvMap(sky)
	mat:setNormalMap(normalMap)
	mat:setMetallic(0.0)
	mat:setRoughness(self.roughness)
	-- Reflectivity: the environment map's blend (0.55 keeps some water colour
	-- in the surface); the UI slider below lets you find your own balance.
	mat:setReflectivity(self.reflectivity)
	mat:setSSREnabled(true)
	self.waterMat = mat
	self.keep[#self.keep + 1] = mat

	-- TangentBitangent: BUMPMAPPING's vTangentMatrix needs the tangent
	-- frame attributes, and a plain Plane.new(w, h) doesn't build them.
	local tile = Plane.new(TILE_HALF, TILE_HALF, false, false, true)
	self.keep[#self.keep + 1] = tile

	-- Every tile goes into the scene itself, NOT under a shared parent
	-- GameObject. GameObject::Add(child) in this engine only records the
	-- pointer in _Childs and sets _Owner - SceneGraph's update loop walks
	-- the top-level lists only and calls RegisterComponents() on those, so
	-- a child's RenderingComponent never registers and never draws. The
	-- wave scroll below therefore has to move each tile itself.
	self.tiles = {}
	local origin = -(GRID - 1) * 0.5 * TILE_SIZE
	local rot = Vec3.new(math.rad(-90), 0, 0)
	for j = 0, GRID - 1 do
		for i = 0, GRID - 1 do
			local go = GameObject.new()
			local rc = RenderingComponent.new(tile, mat)
			rc:disableCullTest()
			go:addComponent(rc)
			go:setRotation(rot)
			local bx = origin + i * TILE_SIZE
			local bz = origin + j * TILE_SIZE
			go:setPosition(Vec3.new(bx, WATER_Y, bz))
			scene:add(go)
			self.owned[#self.owned + 1] = go
			self.keep[#self.keep + 1] = rc
			self.tiles[#self.tiles + 1] = { go = go, x = bx, z = bz }
		end
	end
end

function IslandSSRSetup:update(time)
	if not self.tiles then return end
	-- `time` is the frame delta, not a clock, so this has to add up -
	-- multiplying the delta gave a scroll offset that never advanced.
	self.elapsed = (self.elapsed or 0.0) + time
	local t = self.elapsed * self.scroll
	local ox = (t * SCROLL_X) % TILE_SIZE
	local oz = (t * SCROLL_Z) % TILE_SIZE

	-- Follow the camera, snapped to whole tiles. The lattice is periodic
	-- with TILE_SIZE, so a whole-tile jump is invisible - same trick the
	-- scroll wrap above uses. Without this the grid is a fixed 950-unit
	-- square around the origin, and flying past its edge shows the skybox's
	-- flat bottom face through the gap where water ran out.
	local cx, cz = 0, 0
	if camera then
		local p = camera:getPosition()
		cx = math.floor(p.x / TILE_SIZE) * TILE_SIZE
		cz = math.floor(p.z / TILE_SIZE) * TILE_SIZE
	end

	for i = 1, #self.tiles do
		local tl = self.tiles[i]
		tl.go:setPosition(Vec3.new(cx + tl.x + ox, WATER_Y, cz + tl.z + oz))
	end
end

function IslandSSRSetup:drawUI()
	if not imgui then return end
	imgui.text("Island water via screen-space reflections")
	imgui.text("Traced: the island. Cubemap fallback: the sky,")
	imgui.text("which SSR can't reach. Untick SSR to see which")
	imgui.text("part of the water is actually being marched.")
	imgui.separator()

	local ssr = imgui.checkbox("SSR", self.ssrOn)
	if ssr ~= self.ssrOn then
		self.ssrOn = ssr
		if renderer.setReflections then renderer:setReflections(ssr, SSR_MAX) end
	end

	-- One slider, two effects - see setReflectivity above.
	local rf = imgui.sliderFloat("Sky mix / SSR strength", self.reflectivity, 0.0, 1.0)
	if rf ~= self.reflectivity then
		self.reflectivity = rf
		if self.waterMat then self.waterMat:setReflectivity(rf) end
	end

	local r = imgui.sliderFloat("Water roughness", self.roughness, 0.0, 0.6)
	if r ~= self.roughness then
		self.roughness = r
		if self.waterMat then self.waterMat:setRoughness(r) end
	end

	self.scroll = imgui.sliderFloat("Wave speed", self.scroll, 0.0, 4.0)
end

function IslandSSRSetup:destroy()
	if scene then
		for _, go in ipairs(self.owned) do
			pcall(function() scene:remove(go) end)
		end
	end
	self.owned = {}
	self.keep = {}
	self.tiles = nil
	self.waterMat = nil
end

function IslandSSRSetup:serialize()
	return {}
end

function IslandSSRSetup.deserialize(data)
	return IslandSSRSetup:new()
end

return IslandSSRSetup
