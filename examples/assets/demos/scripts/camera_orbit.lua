-- Automatic orbit camera (SSRTest). Sets global `camera` to this owner.
local CameraOrbit = class('CameraOrbit')
local dTime = 0

function CameraOrbit:initialize()
	self.radius = 18.0
	self.height = 7.0
	self.yawSpeed = 8.0
	self.basePitch = -18.0
	self.pitchAmp = 10.0
	self.pitchFreq = 0.5
end

function CameraOrbit:init(owner)
	self.owner = owner
	camera = owner
end

function CameraOrbit:update(time)
  dTime = time + dTime
	local orbitAngle = dTime * self.yawSpeed
	local rad = math.rad(orbitAngle)
	local pitchDeg = self.basePitch + self.pitchAmp * math.sin(dTime * self.pitchFreq)
	self.owner:setPosition(Vec3.new(math.sin(rad) * self.radius, self.height, math.cos(rad) * self.radius))
	self.owner:setRotation(Vec3.new(math.rad(pitchDeg), math.rad(orbitAngle), 0))
	-- Scene::Update calls InternalUpdate after components, but keep the
	-- world matrix explicit so SSR's view/reprojection matches the orbit
	-- pose the same frame (standalone sets the camera after Scene::Update).
	if self.owner.refreshTransformation then
		self.owner:refreshTransformation()
	end
end

function CameraOrbit:destroy()
	if camera == self.owner then camera = nil end
end

function CameraOrbit:serialize()
	return {
		radius = self.radius,
		height = self.height,
		yawSpeed = self.yawSpeed,
		basePitch = self.basePitch,
		pitchAmp = self.pitchAmp,
		pitchFreq = self.pitchFreq,
	}
end

function CameraOrbit.deserialize(data)
	local o = CameraOrbit:new()
	if data then
		if data.radius then o.radius = data.radius end
		if data.height then o.height = data.height end
		if data.yawSpeed then o.yawSpeed = data.yawSpeed end
		if data.basePitch then o.basePitch = data.basePitch end
		if data.pitchAmp then o.pitchAmp = data.pitchAmp end
		if data.pitchFreq then o.pitchFreq = data.pitchFreq end
	end
	return o
end

return CameraOrbit
