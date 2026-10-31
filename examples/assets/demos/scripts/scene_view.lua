-- A second camera drawn into a texture a monitor in the scene shows: a SceneView
-- (render to texture). The view has its own renderer and its own size; it is drawn
-- in preRender, ahead of the frame's own view, which then samples it as any other
-- texture. The monitor is hidden while its own picture is drawn.
local SceneViewDemo = class('SceneViewDemo')

function SceneViewDemo:initialize()
	self.owned = {}
	self.keep = {}
end

function SceneViewDemo:init(owner)
	self.owner = owner
	if not scene or not SceneView then return end
	self.view = SceneView.new(640, 400)
	if not self.view then return end
	-- (it looks as the frame's own view does - sky, light, exposure - each time it draws; a setSky, setHaze or
	-- setAutoExposure here would make it its own)

	-- the monitor: a frame and a screen showing the view's texture, lit by nothing (it glows)
	local screen = GenericShaderMaterial.new(ShaderUsage.Texture)
	screen:setColorMap(self.view:getTexture())
	local frame = GenericShaderMaterial.new(ShaderUsage.Color + ShaderUsage.Diffuse + ShaderUsage.SpecularColor)
	frame:setColor(Vec4.new(0.08, 0.08, 0.09, 1))
	frame:setSpecular(Vec4.new(0.6, 0.6, 0.6, 1))
	-- (both take half extents: a 16 x 10 screen in a frame a little larger, its face just behind the screen)
	local quad = Plane.new(8, 5)
	local box = Cube.new(8.6, 5.6, 0.3)
	self.keep = { screen, frame, quad, box, self.view }

	self.monitor = GameObject.new()
	self.monitor:setPosition(Vec3.new(13, 9, -12))
	self.monitor:setRotation(Vec3.new(0, -0.45, 0))
	local bezel = GameObject.new()
	bezel:addComponent(RenderingComponent.new(box, frame))
	bezel:setPosition(Vec3.new(0, 0, -0.35))
	self.monitor:addGameObject(bezel)
	self.screenRC = RenderingComponent.new(quad, screen)
	self.monitor:addComponent(self.screenRC)
	scene:add(self.monitor)
	self.owned[#self.owned + 1] = self.monitor

	-- the camera it sees through: circling the cubes, looking at them
	self.eye = GameObject.new()
	scene:add(self.eye)
	self.owned[#self.owned + 1] = self.eye
	self.projection = Projection.new()
	self.projection:perspective(55, 640 / 400, 0.1, 500)
	self.t = 0
end

function SceneViewDemo:update(time)
	self.t = (self.t or 0) + time
end

function SceneViewDemo:preRender()
	if not self.view then return end
	local a = 0.3 + self.t * 0.35
	self.eye:setPosition(Vec3.new(math.sin(a) * 30, 14, math.cos(a) * 30))
	self.eye:lookAtVec(Vec3.new(0, 2, 0))
	self.eye:refreshTransformation()
	self.screenRC:disable()
	self.view:render(self.projection, self.eye, scene)
	self.screenRC:enable()
end

function SceneViewDemo:destroy()
	if scene then for _, go in ipairs(self.owned) do pcall(function() scene:remove(go) end) end end
	self.owned = {}
	self.keep = {}
	self.view = nil
end

function SceneViewDemo:serialize() return {} end
function SceneViewDemo.deserialize(data) return SceneViewDemo:new() end

return SceneViewDemo
