-- DemoLauncher render stack - built from each manifest entry's `render`
-- object (type, globalLight, effects, projection, optional ssr). The C++
-- shell only calls setup/resize/draw/destroy; it does not own a
-- DeferredRenderer or PostEffectsManager anymore.
--
-- Globals set for scene scripts: renderer, effectManager (nil if none).

RenderHost = {}

local state = {
	width = 0,
	height = 0,
	gbuffer = nil,
	fbo = nil,
	effectManager = nil,
	effectCount = 0,
	projFov = 70,
	projNear = 0.1,
	projFar = 2000,
	drawOverride = nil,
	onResize = nil,
}

local function makeTarget(dataType, width, height)
	-- Widest CreateEmptyTexture overload - mipmap/level/msaa are required
	-- from Lua (same as neonpulse/main.lua's buildDeferredRenderer).
	local t = Texture.new()
	t:createEmptyTexture(TextureType.Texture, dataType, width, height, false, 0, 0)
	t:setRepeat(TextureRepeat.ClampToEdge, TextureRepeat.ClampToEdge, TextureRepeat.ClampToEdge)
	return t
end

local function clear()
	-- Ordered teardown: PostFX → renderer → FBO → gbuffer textures.
	if clearPostEffectHandles then clearPostEffectHandles() end
	if state.effectManager then
		pcall(function() state.effectManager:removeAllEffects() end)
	end
	effectManager = nil
	state.effectManager = nil
	state.effectCount = 0
	collectgarbage()
	collectgarbage()

	renderer = nil
	collectgarbage()
	collectgarbage()

	state.fbo = nil
	collectgarbage()
	collectgarbage()

	state.gbuffer = nil
	collectgarbage()
	collectgarbage()
	state.drawOverride = nil
	state.onResize = nil
end

function RenderHost.setDrawOverride(fn)
	state.drawOverride = fn
end

function RenderHost.clearDrawOverride()
	state.drawOverride = nil
end

function RenderHost.setResizeHook(fn)
	state.onResize = fn
end

function RenderHost.clearResizeHook()
	state.onResize = nil
end

function RenderHost.setup(cfg, width, height)
	cfg = cfg or {}
	clear()

	state.width = width
	state.height = height
	local rtype = cfg.type or "deferred"

	if rtype == "forward" then
		renderer = ForwardRenderer.new(width, height)
	else
		local g = {
			depth = makeTarget(TextureDataType.DepthComponent, width, height),
			albedo = makeTarget(TextureDataType.RGBA, width, height),
			specular = makeTarget(TextureDataType.RGBA, width, height),
			normal = makeTarget(TextureDataType.RGBA32F, width, height),
			matrough = makeTarget(TextureDataType.RGBA, width, height),
		}
		local fbo = FrameBuffer.new()
		fbo:init(FrameBufferAttachmentFormat.Depth_Attachment, TextureType.Texture, g.depth)
		fbo:addAttach(FrameBufferAttachmentFormat.Color_Attachment0, TextureType.Texture, g.albedo)
		fbo:addAttach(FrameBufferAttachmentFormat.Color_Attachment1, TextureType.Texture, g.specular)
		fbo:addAttach(FrameBufferAttachmentFormat.Color_Attachment2, TextureType.Texture, g.normal)
		fbo:addAttach(FrameBufferAttachmentFormat.Color_Attachment3, TextureType.Texture, g.matrough)
		state.gbuffer = g
		state.fbo = fbo
		renderer = DeferredRenderer.new(width, height, fbo)
	end

	local gl = cfg.globalLight or { 0.12, 0.12, 0.14, 1.0 }
	renderer:setGlobalLight(Vec4.new(gl[1], gl[2], gl[3], gl[4] or 1.0))

	if cfg.ssr and renderer.enableSSR then
		renderer:enableSSR()
		local d = cfg.ssrDistances
		if d then renderer:setSSRDistances(d[1], d[2]) end
	end

	local effects = cfg.effects or {}
	state.effectCount = #effects
	if #effects > 0 then
		local em = PostEffectsManager.new(width, height)
		for _, name in ipairs(effects) do
			if name == "ssao" then
				buildSSAOPostChain(em, width, height)
			elseif name == "dof" then
				buildDOFPostChain(em, width, height)
			else
				addPostEffect(em, name, width, height)
			end
		end
		state.effectManager = em
		effectManager = em
	end

	if cfg.lod and renderer.enableLOD then renderer:enableLOD() end
	if cfg.culling and renderer.activateCulling then
		local mode = CullingMode and CullingMode[cfg.culling] or nil
		if mode then renderer:activateCulling(mode) end
	end
	if cfg.background and renderer.setBackground then
		local b = cfg.background
		renderer:setBackground(Vec4.new(b[1], b[2], b[3], b[4] or 1))
	end

	local proj = cfg.projection or {}
	state.projFov = proj.fov or 70
	state.projNear = proj.near or 0.1
	state.projFar = proj.far or 2000
	if projection then
		projection:perspective(state.projFov, width / height, state.projNear, state.projFar)
	end
end

function RenderHost.resize(width, height)
	if not renderer then return end
	state.width = width
	state.height = height

	if state.gbuffer then
		state.gbuffer.albedo:resize(width, height)
		state.gbuffer.specular:resize(width, height)
		state.gbuffer.depth:resize(width, height)
		state.gbuffer.normal:resize(width, height)
		state.gbuffer.matrough:resize(width, height)
	end
	if state.fbo then state.fbo:resize(width, height) end
	renderer:resize(width, height)
	if state.effectManager then state.effectManager:resize(width, height) end

	if projection then
		projection:perspective(state.projFov, width / height, state.projNear, state.projFar)
	end
	if state.onResize then state.onResize(width, height) end
end

function RenderHost.draw(camera, scene, projection)
	if not renderer or not camera or not scene or not projection then return end
	if state.drawOverride then
		state.drawOverride(camera, scene, projection)
		return
	end

	local em = state.effectManager
	if em and state.effectCount > 0 then
		em:captureFrame()
		renderer:preRender(camera, scene)
		renderer:renderScene(projection, camera, scene)
		em:endCapture()
		em:processPostEffects(projection)
	else
		renderer:preRender(camera, scene)
		renderer:renderScene(projection, camera, scene)
	end
end

function RenderHost.destroy()
	clear()
end

return RenderHost
