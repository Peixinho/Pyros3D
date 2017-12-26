function init()
	scene = Scene.new()
	renderer = ForwardRenderer.new(1024,768)
	projection = Projection.new()
	projection:perspective(70, 1024 / 768, 1, 100)
	camera = GameObject.new()
	camera:setPosition(Vec3.new(0,10,80))
	scene:add(camera)
	texture = Texture.new()
	texture:loadTexture('../examples/LuaScripting/assets/Texture.DDS',TextureType.Texture,false,0)
	CubeObject = GameObject.new()
	cubeMesh = Cube.new(15, 15, 15)
	light = DirectionalLight.new(Vec4.new(1,1,1,1),Vec3.new(-1,-1,0))
	Diffuse = GenericShaderMaterial.new(ShaderUsage.Texture + ShaderUsage.Diffuse)
	Diffuse:setColorMap(texture)
	rCube = RenderingComponent.new(cubeMesh, Diffuse)
	rCube.onUpdate = function(self, time)
		print(time)
	end

	--rCube:getMeshes(0)[1].material = Diffuse

	CubeObject:addComponent(rCube)
	CubeObject:addComponent(light)
	scene:add(CubeObject)

	CubeObject.onUpdate = function(self, time)
		rot = Vec3.new(0,time*.5,0)
		rot2 = Vec3.new(time,0,0)
		rot = rot + rot2
		self:setRotation(rot)
	end
end
function update(time)
	scene:update(time)
	renderer:preRender(camera, scene)
	renderer:renderScene(projection, camera, scene)
end
function resize(width, height)
	projection:perspective(70, width/ height, 1 , 100)
	renderer:resize(width, height)
end
