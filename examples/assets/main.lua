-- ****************************************************************
-- Coroutine-based sequencing - see VULKAN_ROADMAP.md's Lua-scripting
-- section for why this replaces a dedicated C++ timer system: the
-- engine already calls update(time, dt) every real frame, so a plain
-- Lua coroutine driven from here gives scripts natural "wait N
-- seconds, then do the next step" behavior with zero new engine code.
-- ****************************************************************
_coroutines = {}
_currentTime = 0

-- Always yields at least once (even for wait(0)) so a sequence never
-- runs past a frame boundary synchronously inside startSequence().
function wait(seconds)
	local start = _currentTime
	repeat
		coroutine.yield()
	until (_currentTime - start) >= seconds
end

function startSequence(fn)
	table.insert(_coroutines, coroutine.create(fn))
end

function resumeSequences()
	local i = 1
	while i <= #_coroutines do
		local co = _coroutines[i]
		local ok, err = coroutine.resume(co)
		if not ok then
			print("sequence error: " .. tostring(err))
			table.remove(_coroutines, i)
		elseif coroutine.status(co) == "dead" then
			table.remove(_coroutines, i)
		else
			i = i + 1
		end
	end
end

function init()
	scene = Scene.new()
	renderer = ForwardRenderer.new(1024, 768)
	projection = Projection.new()
	projection:perspective(70, 1024 / 768, 1, 1000)

	camera = GameObject.new()
	camera:setPosition(Vec3.new(0, 60, 220))
	scene:add(camera)

	texture = Texture.new()
	texture:loadTexture('../../examples/assets/luaexample/Texture.png', TextureType.Texture, false, 0)

	Diffuse = GenericShaderMaterial.new(ShaderUsage.Texture + ShaderUsage.Diffuse)
	Diffuse:setColorMap(texture)
	Diffuse:setMetallic(0.2)
	Diffuse:setRoughness(0.6)

	-- Light
	lightObject = GameObject.new()
	light = DirectionalLight.new(Vec4.new(1, 1, 1, 1), Vec3.new(-1, -1, 0))
	lightObject:addComponent(light)
	scene:add(lightObject)

	-- Particle system smoke test - a small continuous emitter, real
	-- p3d::ParticleSystem, self-driving via its own Update() override.
	particleObject = GameObject.new()
	particleObject:setPosition(Vec3.new(0, 140, 0))
	particleDesc = ParticleSystemDesc.new()
	particleDesc.maxParticles = 64
	particleDesc.texture = texture
	particleDesc.emissionRate = 10
	particleDesc.minLifetime = 1.0
	particleDesc.maxLifetime = 2.0
	particleDesc.startSize = 4.0
	particleDesc.endSize = 0.5
	particles = ParticleSystem.new(particleDesc)
	particleObject:addComponent(particles)
	scene:add(particleObject)

	-- ************************** Physics **************************
	physics = Box3DPhysics.new()
	physics:initPhysics()

	-- Static ground
	groundMesh = Cube.new(80, 2, 80)
	GroundObject = GameObject.new()
	rGround = RenderingComponent.new(groundMesh, Diffuse)
	GroundObject:addComponent(rGround)
	groundPhysics = physics:createBox(80, 2, 80, 0, false)
	GroundObject:addComponent(groundPhysics)
	scene:add(GroundObject)

	-- Dynamic cube, dropped from above the ground - lands, fires the
	-- real OnCollisionEnter event, and bounces off the ground once via
	-- ApplyCentralImpulse().
	cubeMesh = Cube.new(15, 15, 15)
	CubeObject = GameObject.new()
	CubeObject:setPosition(Vec3.new(0, 120, 0))
	rCube = RenderingComponent.new(cubeMesh, Diffuse)
	CubeObject:addComponent(rCube)
	cubePhysics = physics:createBox(15, 15, 15, 5, false)
	CubeObject:addComponent(cubePhysics)
	scene:add(CubeObject)

	hasLanded = false
	cubePhysics.onCollisionEnter = function(other)
		if not hasLanded then
			hasLanded = true
			print("Cube landed! Applying a bounce impulse.")
			cubePhysics:applyCentralImpulse(Vec3.new(0, 4000, 0))
		end
	end
	cubePhysics.onCollisionExit = function(other)
		print("Cube left contact with the ground.")
	end

	-- Scene save/load smoke test.
	local saveOk = scene:save("lua_scene_test.json")
	print("scene:save() returned " .. tostring(saveOk))

	-- Raycast smoke test - straight down through both bodies. Physics
	-- components only actually register with the physics world on the
	-- first SceneGraph:update() pass (scene:add() alone just parents the
	-- GameObject), so this runs from a sequence one frame later rather
	-- than inline here.
	startSequence(function()
		wait(0)
		local hit = physics:rayCast(Vec3.new(0, 200, 0), Vec3.new(0, -200, 0))
		if hit.hasHit then
			print(string.format("Raycast hit at (%.2f, %.2f, %.2f), distance=%.2f", hit.point.x, hit.point.y, hit.point.z, hit.distance))
		else
			print("Raycast missed")
		end
	end)

	-- ************************** Input **************************
	input = Input.new()
	input:onKeyPressed(Key.Space, function()
		cubePhysics:applyCentralImpulse(Vec3.new(0, 2000, 0))
		print("Space pressed - impulse applied to cube")
	end)

	-- ************************** LuaComponent **************************
	-- Generic gameplay-logic component, not tied to rendering or
	-- physics - demonstrates both LuaComponent itself and the
	-- coroutine-based wait() helper above.
	heartbeat = LuaComponent.new()
	heartbeat.onInit = function(self)
		startSequence(function()
			local tinted = false
			while true do
				wait(2.0)
				tinted = not tinted
				if tinted then
					Diffuse:setColor(Vec4.new(1, 0.6, 0.6, 1))
				else
					Diffuse:setColor(Vec4.new(1, 1, 1, 1))
				end
				print("heartbeat tick at " .. string.format("%.2f", _currentTime))
			end
		end)
	end
	CubeObject:addComponent(heartbeat)
end

function update(time, dt)
	_currentTime = time
	resumeSequences()

	physics:update(dt, 10)
	scene:update(time)
	renderer:preRender(camera, scene)
	renderer:renderScene(projection, camera, scene)
end

function resize(width, height)
	projection:perspective(70, width / height, 1, 1000)
	renderer:resize(width, height)
end
