-- Island multipass water: reflection FBO + refraction FBO + composite.
-- FrontFace cull only while drawing the reflection FBO (mirror winding).
local IslandSetup = class('IslandSetup')

local WATER_Y = 6.8

function IslandSetup:initialize()
	self.owned = {}
	self.keep = {}
	self.cameraReflection = nil
	self.gWater = nil
	self.rWater = nil
	self.rIsland = nil
	self.fboReflection = nil
	self.fboRefraction = nil
	self.reflectionTex = nil
	self.refractionTex = nil
	self.refractionDepth = nil
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
	if not scene or not renderer or not ASSETS_PATH or not EXAMPLES_PATH then return end

	local w, h = getWindowSize()

	local island = Model.new(ASSETS_PATH .. "island.p3dm", true)
	local gIsland = GameObject.new()
	self.rIsland = RenderingComponent.new(island, ShaderUsage.Diffuse + ShaderUsage.ClipPlane)
	self.rIsland:disableCullTest()
	gIsland:addComponent(self.rIsland)
	scene:add(gIsland)
	self.owned[#self.owned + 1] = gIsland
	self.keep[#self.keep + 1] = island
	self.keep[#self.keep + 1] = self.rIsland

	self.cameraReflection = GameObject.new()
	scene:add(self.cameraReflection)
	self.owned[#self.owned + 1] = self.cameraReflection

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
		selfRef:draw(cam, mainScene, proj)
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

function IslandSetup:setIslandCull(mode)
	if not self.rIsland then return end
	for _, mesh in ipairs(self.rIsland:getMeshes()) do
		if mesh.material then mesh.material:setCullFace(mode) end
	end
end

function IslandSetup:draw(cam, mainScene, proj)
	if not renderer or not cam or not self.gWater or not self.rWater then return end

	local waterY = self.gWater:getPosition().y
	local camPos = cam:getPosition()
	local pitch = math.rad(cameraFlyPitch or 0)
	local yaw = math.rad(cameraFlyYaw or 0)
	local distance = 2 * (camPos.y - waterY)
	self.cameraReflection:setPosition(Vec3.new(camPos.x, camPos.y - distance, camPos.z))
	self.cameraReflection:setRotation(Vec3.new(-pitch, yaw, 0))
	self.cameraReflection:refreshTransformation()

	self.rWater:disable()

	self:setIslandCull(CullFace.FrontFace)
	self.fboReflection:bind()
	renderer:enableClipPlane(1)
	renderer:setClipPlane0(Vec4.new(0, 1, 0, -waterY))
	renderer:clearBufferBit(BufferBit.Depth + BufferBit.Color)
	renderer:preRender(self.cameraReflection, mainScene)
	renderer:renderScene(proj, self.cameraReflection, mainScene)
	renderer:disableClipPlane()
	self.fboReflection:unbind()

	self:setIslandCull(CullFace.BackFace)
	self.fboRefraction:bind()
	renderer:clearBufferBit(BufferBit.Depth + BufferBit.Color)
	renderer:enableClipPlane(1)
	renderer:setClipPlane0(Vec4.new(0, -1, 0, waterY))
	renderer:preRender(cam, mainScene)
	renderer:renderScene(proj, cam, mainScene)
	renderer:disableClipPlane()
	self.fboRefraction:unbind()

	self.rWater:enable()
	renderer:clearBufferBit(BufferBit.Depth + BufferBit.Color)
	renderer:enableClearDepthBuffer()
	renderer:preRender(cam, mainScene)
	renderer:renderScene(proj, cam, mainScene)
end

function IslandSetup:destroy()
	if RenderHost then
		RenderHost.clearDrawOverride()
		RenderHost.clearResizeHook()
	end
	if scene then
		for _, go in ipairs(self.owned) do
			pcall(function() scene:remove(go) end)
		end
	end
	self.owned = {}
	self.keep = {}
	self.cameraReflection = nil
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
