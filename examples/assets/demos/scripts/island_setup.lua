-- Island multipass water: reflection FBO + refraction FBO + composite.
-- Reuses the view camera for the mirror pose (GameObject* that renderScene
-- already accepts). A dedicated GameObject.new() cam needs asGameObject and
-- was aborting the multipass into main-only (no reflections).
local IslandSetup = class('IslandSetup')

local WATER_Y = 6.8
local SKY = { 0.53, 0.81, 0.92, 1.0 }

function IslandSetup:initialize()
	self.owned = {}
	self.keep = {}
	self.gWater = nil
	self.rWater = nil
	self.rIsland = nil
	self.fboReflection = nil
	self.fboRefraction = nil
	self.reflectionTex = nil
	self.refractionTex = nil
	self.refractionDepth = nil
	self.lastError = nil
	self.errorCount = 0
end

local function makeColorTarget(w, h)
	local t = Texture.new()
	t:createEmptyTexture(TextureType.Texture, TextureDataType.RGBA, w, h, false, 0, 0)
	t:setRepeat(TextureRepeat.ClampToEdge, TextureRepeat.ClampToEdge, TextureRepeat.ClampToEdge)
	return t
end

local function makeDepthTarget(w, h)
	local t = Texture.new()
	t:createEmptyTexture(TextureType.Texture, TextureDataType.DepthComponent, w, h, false, 0, 0)
	t:setRepeat(TextureRepeat.ClampToEdge, TextureRepeat.ClampToEdge, TextureRepeat.ClampToEdge)
	t:setMinMagFilter(TextureFilter.Nearest, TextureFilter.Nearest)
	return t
end

function IslandSetup:buildFBOs(w, h)
	self.reflectionTex = makeColorTarget(w, h)
	self.fboReflection = FrameBuffer.new()
	self.fboReflection:init(FrameBufferAttachmentFormat.Depth_Attachment, RenderBufferDataType.Depth, w, h)
	self.fboReflection:addAttach(FrameBufferAttachmentFormat.Color_Attachment0, TextureType.Texture, self.reflectionTex)

	self.refractionTex = makeColorTarget(w, h)
	self.refractionDepth = makeDepthTarget(w, h)
	self.fboRefraction = FrameBuffer.new()
	self.fboRefraction:init(FrameBufferAttachmentFormat.Depth_Attachment, TextureType.Texture, self.refractionDepth)
	self.fboRefraction:addAttach(FrameBufferAttachmentFormat.Color_Attachment0, TextureType.Texture, self.refractionTex)

	self.keep[#self.keep + 1] = self.reflectionTex
	self.keep[#self.keep + 1] = self.refractionTex
	self.keep[#self.keep + 1] = self.refractionDepth
	self.keep[#self.keep + 1] = self.fboReflection
	self.keep[#self.keep + 1] = self.fboRefraction
end

function IslandSetup:init(owner)
	self.owner = owner
	if not scene then error("IslandSetup:init - scene is nil") end
	if not renderer then error("IslandSetup:init - renderer is nil") end
	if not ASSETS_PATH then error("IslandSetup:init - ASSETS_PATH is nil") end
	if not EXAMPLES_PATH then error("IslandSetup:init - EXAMPLES_PATH is nil") end

	if renderer.deactivateCulling then renderer:deactivateCulling() end
	if renderer.setBackground then
		renderer:setBackground(Vec4.new(SKY[1], SKY[2], SKY[3], SKY[4]))
	end

	local w, h = getWindowSize()
	if not w or not h or w < 1 or h < 1 then
		w, h = 1024, 768
	end

	local island = Model.new(ASSETS_PATH .. "island.p3dm", true)
	local gIsland = GameObject.new()
	self.rIsland = RenderingComponent.new(island, ShaderUsage.Diffuse + ShaderUsage.ClipPlane)
	self.rIsland:disableCullTest()
	gIsland:addComponent(self.rIsland)
	scene:add(gIsland)
	self.owned[#self.owned + 1] = gIsland
	self.keep[#self.keep + 1] = island
	self.keep[#self.keep + 1] = self.rIsland

	self:buildFBOs(w, h)

	local createWaterMaterial = assert(loadfile(
		(EXAMPLES_PATH or "") .. "/assets/demos/scripts/water_material.lua"
	))()

	local mat = createWaterMaterial(ASSETS_PATH .. "WaterShader.glsl")
	mat:addSampler("uReflectionMap", self.reflectionTex)
	mat:addSampler("uRefractionMap", self.refractionTex)
	mat:addSampler("uRefractionMapDepth", self.refractionDepth)

	local normalMap = Texture.new()
	normalMap:loadTexture(ASSETS_PATH .. "normal.png", TextureType.Texture, false, 0)
	local dudv = Texture.new()
	dudv:loadTexture(ASSETS_PATH .. "waterDUDV.png", TextureType.Texture, false, 0)
	mat:addSampler("uNormalmap", normalMap)
	mat:addSampler("uDUDVmap", dudv)

	local waterMesh = Plane.new(500, 500)
	self.gWater = GameObject.new()
	self.gWater:setRotation(Vec3.new(math.rad(-90), 0, 0))
	self.gWater:setPosition(Vec3.new(0, WATER_Y, 0))
	self.rWater = RenderingComponent.new(waterMesh, mat)
	self.gWater:addComponent(self.rWater)
	scene:add(self.gWater)
	self.owned[#self.owned + 1] = self.gWater

	self.keep[#self.keep + 1] = mat
	self.keep[#self.keep + 1] = normalMap
	self.keep[#self.keep + 1] = dudv
	self.keep[#self.keep + 1] = waterMesh
	self.keep[#self.keep + 1] = self.rWater

	local selfRef = self
	RenderHost.setDrawOverride(function(cam, mainScene, proj)
		selfRef:drawSafe(cam, mainScene, proj)
	end)
	RenderHost.setResizeHook(function(nw, nh)
		selfRef:onResize(nw, nh)
	end)
end

function IslandSetup:onResize(w, h)
	if self.reflectionTex then self.reflectionTex:resize(w, h, 0) end
	if self.refractionTex then self.refractionTex:resize(w, h, 0) end
	if self.refractionDepth then self.refractionDepth:resize(w, h, 0) end
	if self.fboReflection then self.fboReflection:resize(w, h) end
	if self.fboRefraction then self.fboRefraction:resize(w, h) end
end

function IslandSetup:update(time)
end

function IslandSetup:drawUI()
	if not imgui then return end
	imgui.text("Island water multipass")
	if self.lastError then
		imgui.text("LAST ERROR: " .. tostring(self.lastError))
	else
		imgui.text("multipass: ok")
	end
end

local function clearBits()
	return BufferBit.Depth + BufferBit.Color
end

function IslandSetup:drawMain(cam, mainScene, proj)
	if self.rWater then self.rWater:enable() end
	renderer:clearBufferBit(clearBits())
	renderer:enableClearDepthBuffer()
	renderer:preRender(cam, mainScene)
	renderer:renderScene(proj, cam, mainScene)
end

function IslandSetup:drawSafe(cam, mainScene, proj)
	local ok, err = pcall(function()
		self:drawMultipass(cam, mainScene, proj)
	end)
	if ok then
		self.lastError = nil
		return
	end
	self.errorCount = self.errorCount + 1
	self.lastError = tostring(err)
	if self.errorCount <= 5 then
		print("IslandSetup multipass FAIL:", self.lastError)
	end
	pcall(function()
		if renderer and renderer.disableClipPlane then renderer:disableClipPlane() end
		if self.fboReflection and self.fboReflection.isBinded and self.fboReflection:isBinded() then
			self.fboReflection:unbind()
		end
		if self.fboRefraction and self.fboRefraction.isBinded and self.fboRefraction:isBinded() then
			self.fboRefraction:unbind()
		end
		-- Restore view cam euler if multipass died mid-mirror-pose.
		if cam and cameraFlyPitch and cameraFlyYaw then
			pcall(function()
				cam:setRotation(Vec3.new(math.rad(cameraFlyPitch), math.rad(cameraFlyYaw), 0))
				cam:refreshTransformation()
			end)
		end
		self:drawMain(cam, mainScene, proj)
	end)
end

function IslandSetup:drawMultipass(cam, mainScene, proj)
	if not renderer then error("no renderer") end
	if not cam then error("no cam") end
	if not mainScene then error("no scene") end
	if not proj then error("no proj") end
	if not self.gWater or not self.rWater then error("no water") end
	if not self.fboReflection or not self.fboRefraction then error("no fbo") end

	local pitchDeg = cameraFlyPitch or 0
	local yawDeg = cameraFlyYaw or 0
	local pitch = math.rad(pitchDeg)
	local yaw = math.rad(yawDeg)

	-- Lock view cam to fly counters (standalone does this every frame).
	cam:setRotation(Vec3.new(pitch, yaw, 0))
	cam:refreshTransformation()

	local waterY = self.gWater:getPosition().y
	local camPos = cam:getPosition()
	local reflPos = Vec3.new(camPos.x, camPos.y - 2 * (camPos.y - waterY), camPos.z)

	self.rWater:disable()

	-- Mirror pose on the same GameObject* the main pass uses.
	cam:setPosition(reflPos)
	cam:setRotation(Vec3.new(-pitch, yaw, 0))
	cam:refreshTransformation()

	self.fboReflection:bind()
	renderer:enableClipPlane(1)
	renderer:setClipPlane0(Vec4.new(0, 1, 0, -waterY))
	renderer:clearBufferBit(clearBits())
	renderer:preRender(cam, mainScene)
	renderer:renderScene(proj, cam, mainScene)
	renderer:disableClipPlane()
	self.fboReflection:unbind()

	-- Restore view pose for refraction + main.
	cam:setPosition(camPos)
	cam:setRotation(Vec3.new(pitch, yaw, 0))
	cam:refreshTransformation()

	self.fboRefraction:bind()
	renderer:clearBufferBit(clearBits())
	renderer:enableClipPlane(1)
	renderer:setClipPlane0(Vec4.new(0, -1, 0, waterY))
	renderer:preRender(cam, mainScene)
	renderer:renderScene(proj, cam, mainScene)
	renderer:disableClipPlane()
	self.fboRefraction:unbind()

	self:drawMain(cam, mainScene, proj)
end

function IslandSetup:destroy()
	if RenderHost then
		RenderHost.clearDrawOverride()
		RenderHost.clearResizeHook()
	end
	if renderer and renderer.unsetBackground then
		renderer:unsetBackground()
	end
	if scene then
		for _, go in ipairs(self.owned) do
			pcall(function() scene:remove(go) end)
		end
	end
	self.owned = {}
	self.keep = {}
	self.gWater = nil
	self.rWater = nil
	self.rIsland = nil
	self.fboReflection = nil
	self.fboRefraction = nil
	self.reflectionTex = nil
	self.refractionTex = nil
	self.refractionDepth = nil
end

function IslandSetup:serialize()
	return {}
end

function IslandSetup.deserialize(data)
	return IslandSetup:new()
end

return IslandSetup
