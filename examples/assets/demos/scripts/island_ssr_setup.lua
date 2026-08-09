-- Island water, screen-space reflections variant.
--
-- Same island as island_setup.lua, but the water is an ordinary opaque
-- G-buffer surface with the DeferredRenderer's material-aware SSR doing
-- the reflection, instead of the planar reflection/refraction multipass.
-- No mirror camera, no clip planes, no reflection/refraction FBOs and no
-- draw override at all - the whole reflection is
-- DeferredRenderer::EnableSSR() plus GenericShaderMaterial::SetSSREnabled()
-- on the water material (see lastPass.glsl's TraceSSR).
--
-- The trade-off vs the planar version is the usual SSR one: only what is
-- already on screen can be reflected, so the island reflects and the sky
-- cannot (the skybox is transparent, drawn in the deferred translucent
-- pass after the SSR composite, so it never reaches the G-buffer at all).
-- The water therefore also carries an ENVMAP cubemap fallback underneath
-- the traced reflection - see buildWater(). That is how SSR is shipped in
-- practice, not a workaround, but it does mean the calm open sea here is
-- mostly cubemap and only the island's reflection is genuinely traced.
local IslandSSRSetup = class('IslandSSRSetup')

local WATER_Y = 6.8

-- Plane.new(h, h) spans -h..h, so a tile is TILE_HALF*2 across, and its
-- UVs run 0..1 over that whole quad. PyrosShader.glsl has no UV-scale
-- uniform anywhere, so the only way to tile the wave normal map across a
-- big water surface is to tile the geometry: GRID x GRID identical quads
-- sharing one mesh and one material.
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
	if renderer.enableSSR then renderer:enableSSR() end
	if renderer.setSSRDistances then renderer:setSSRDistances(SSR_STEP, SSR_MAX) end
	self.ssrOn = true

	local island = Model.new(ASSETS_PATH .. "island.p3dm", true)
	local gIsland = GameObject.new()
	local rIsland = RenderingComponent.new(island, ShaderUsage.Diffuse + ShaderUsage.DeferredRenderer_Gbuffer)
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

	-- PBR + a low roughness keeps the surface under lastPass.glsl's
	-- SSR_ROUGHNESS_CUTOFF, above which it skips the march entirely.
	local mat = GenericShaderMaterial.new(
		ShaderUsage.Color + ShaderUsage.BumpMapping + ShaderUsage.EnvMap +
		ShaderUsage.PBR + ShaderUsage.DeferredRenderer_Gbuffer)
	-- The cubemap under the SSR. SSR can only reflect what's in the
	-- G-buffer, and the skybox never gets there (it's a transparent
	-- material, drawn in the deferred translucent pass after the composite),
	-- so a pure-SSR ocean at sunset reflects the island and nothing else -
	-- no sky, no sun, which is most of what you actually expect to see in
	-- water. ENVMAP mixes the mirrored cubemap into `diffuse` in
	-- PyrosShader.glsl BEFORE the DEFERRED_GBUFFER write, so it lands in
	-- the albedo target and becomes exactly what lastPass.glsl composites
	-- the traced island reflection on top of. Standard SSR fallback.
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
	-- uReflectivity does double duty and there is no separating the two:
	-- PyrosShader.glsl uses it as the ENVMAP blend weight, and writes the
	-- same value into the G-buffer's mr.a, which lastPass.glsl reads as the
	-- SSR strength multiplier. So this one number is "how much sky" AND
	-- "how strong the traced reflection" at once. 0.55 keeps some water
	-- colour in the surface instead of turning it into a pure mirror; the
	-- UI slider below lets you find your own balance.
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
	local t = time * self.scroll
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
		if ssr then
			if renderer.enableSSR then renderer:enableSSR() end
		else
			if renderer.disableSSR then renderer:disableSSR() end
		end
	end

	-- Coarse march stride, in screen pixels. 128 * this is how far a
	-- reflection can reach across the frame; drag it to 1 to see the
	-- treeline drop out and only the near shoreline survive.
	local st = imgui.sliderFloat("SSR stride (px)", self.ssrStep, 1.0, 16.0)
	if st ~= self.ssrStep then
		self.ssrStep = st
		if renderer.setSSRDistances then renderer:setSSRDistances(st, SSR_MAX) end
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
	if renderer and renderer.disableSSR then renderer:disableSSR() end
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
