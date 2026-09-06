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
	uiRenderer = nil,
	projFov = 70,
	projNear = 0.1,
	projFar = 2000,
	drawOverride = nil,
	onResize = nil,
	motionBlur = false,
	lastDrawTime = nil,
}

local function makeTarget(dataType, width, height)
	-- Widest CreateEmptyTexture overload - mipmap/level/msaa are required
	-- from Lua (same as neonpulse/main.lua's buildDeferredRenderer).
	local t = Texture.new()
	t:createEmptyTexture(TextureType.Texture, dataType, width, height, false, 0, 0)
	t:setRepeat(TextureRepeat.ClampToEdge, TextureRepeat.ClampToEdge, TextureRepeat.ClampToEdge)
	return t
end

local function makeDepthTarget(width, height)
	local t = makeTarget(TextureDataType.DepthComponent, width, height)
	t:setMinMagFilter(TextureFilter.Nearest, TextureFilter.Nearest)
	return t
end

local function clear()
	-- Ordered teardown: PostFX → renderer → FBO → gbuffer textures.
	if clearPostEffectHandles then clearPostEffectHandles() end
	if state.effectManager then
		pcall(function() state.effectManager:removeAllEffects() end)
	end
	-- VelocityRenderer owns the velocity texture MotionBlur sampled; delete
	-- only after the effect has been destroyed above.
	if destroyMotionBlurVelocity then destroyMotionBlurVelocity() end
	effectManager = nil
	state.effectManager = nil
	state.effectCount = 0
	state.motionBlur = false
	state.lastDrawTime = nil
	-- Before the renderer below, same ordering rule as everything else here:
	-- it holds GPU state and must not outlive the device teardown.
	state.uiRenderer = nil
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
			-- Nearest, not makeTarget's default Linear: this is the depth
			-- every secondpass*.glsl samples as tDepth, and a Linear-filtered
			-- depth texture is not filterable - WebGL2 calls it incomplete and
			-- samples 0, Apple's GL substitutes the zero texture. tDepth then
			-- never trips `>= 1.0`, the sky is never discarded and every light
			-- shades the whole screen. See PostEffectsManager::Init().
			depth = makeDepthTarget(width, height),
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
	state.motionBlur = false
	if #effects > 0 then
		local em = PostEffectsManager.new(width, height)
		for _, name in ipairs(effects) do
			if name == "ssao" then
				buildSSAOPostChain(em, width, height)
			elseif name == "dof" then
				buildDOFPostChain(em, width, height)
			elseif name == "motionblur" then
				buildMotionBlurPostChain(em, width, height)
				state.motionBlur = true
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
	elseif renderer.unsetBackground then
		-- Clear a previous demo's SetBackground (e.g. Island sky / DoF red)
		-- so FBO clears don't stay stuck on that colour.
		renderer:unsetBackground()
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
	if state.motionBlur and motionBlurResize then
		motionBlurResize(width, height)
	end

	if projection then
		projection:perspective(state.projFov, width / height, state.projNear, state.projFar)
	end
	if state.onResize then state.onResize(width, height) end
end

-- Screen-space UI, last and over everything. Built lazily because most demos
-- have no canvas and there is no reason to hold a renderer for them; once one
-- exists it costs a scan of an empty canvas list.
local function drawUI(scene)
	if not state.uiRenderer then
		state.uiRenderer = UIRenderer.new(state.width, state.height)
	end
	state.uiRenderer:resize(state.width, state.height)
	state.uiRenderer:renderUI(scene)
end

function RenderHost.draw(camera, scene, projection)
	if not renderer or not camera or not scene or not projection then return end
	if state.drawOverride then
		state.drawOverride(camera, scene, projection)
		-- A demo that owns its draw still gets its UI composited: overriding
		-- the 3D pass is not a statement about the HUD.
		drawUI(scene)
		return
	end

	local em = state.effectManager
	if em and state.effectCount > 0 then
		-- Motion blur needs a velocity pass between PreRender (which
		-- latches previous-frame matrices) and the colour capture.
		if state.motionBlur and motionBlurRenderVelocity then
			renderer:preRender(camera, scene)
			motionBlurRenderVelocity(projection, camera, scene)
			em:captureFrame()
			renderer:renderScene(projection, camera, scene)
			em:endCapture()
			if motionBlurSetFPS then
				local now = scene.getTime and scene:getTime() or nil
				local fps = 60.0
				if now and state.lastDrawTime and now > state.lastDrawTime then
					fps = 1.0 / (now - state.lastDrawTime)
				end
				state.lastDrawTime = now
				motionBlurSetFPS(fps)
			end
			em:processPostEffects(projection)
		else
			em:captureFrame()
			renderer:preRender(camera, scene)
			renderer:renderScene(projection, camera, scene)
			em:endCapture()
			em:processPostEffects(projection)
		end
	else
		renderer:preRender(camera, scene)
		renderer:renderScene(projection, camera, scene)
	end

	drawUI(scene)
end

function RenderHost.destroy()
	clear()
end

return RenderHost
