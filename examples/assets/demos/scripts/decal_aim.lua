-- Decals demo: free mouse (no Tab capture), aim reticle, left-click places
-- a textured decal on the nearest hit mesh (port of examples/Decals::CreateDecal
-- via placeDecalAtCursor). allowMouseCapture=false is read by camera_fly.lua.
local DecalAim = class('DecalAim')

function DecalAim:initialize()
	self.keep = {}
end

function DecalAim:init(owner)
	self.owner = owner
	allowMouseCapture = false
	if setMouseCaptured then setMouseCaptured(false) end

	local tex = Texture.new()
	tex:loadTexture(ASSETS_PATH .. "pyros.png", TextureType.Texture, true, 0)
	self.keep[#self.keep + 1] = tex

	local mat = GenericShaderMaterial.new(ShaderUsage.Texture)
	mat:setColorMap(tex)
	mat:setTransparencyFlag(true)
	mat:enableDepthBias(-4, -4)
	mat:disableDepthWrite()
	self.material = mat
	self.keep[#self.keep + 1] = mat

	local input = Input.new()
	self.input = input
	input:onMouseButtonReleased(MouseButton.Left, function()
		if not camera or not projection or not scene or not self.material then return end
		local w, h = getWindowSize()
		local mx, my = getMousePosition()
		placeDecalAtCursor(w, h, mx, my, camera, projection, scene, self.material, Vec3.new(10, 10, 10))
	end)
end

function DecalAim:drawOverlay()
	if imgui and imgui.drawCrosshair then imgui.drawCrosshair() end
end

function DecalAim:destroy()
	allowMouseCapture = nil
	self.input = nil
	self.material = nil
	self.keep = {}
end

function DecalAim:serialize()
	return {}
end

function DecalAim.deserialize(data)
	return DecalAim:new()
end

return DecalAim
