-- Destruction: every couple of seconds the next target (scene objects named in data.targets) is
-- broken where it faces the camera (physics:fracture - Voronoi pieces packed round the hit, each
-- with its own convex body), and its pieces are thrown out from the hit the frame after. A left
-- click breaks whatever is under the cursor instead (where the launcher offers picking).
local Destruction = class('Destruction')

function Destruction:initialize() self.data = {} end
function Destruction.deserialize(data) local d = Destruction:new(); d.data = data or {}; return d end
function Destruction:serialize() return self.data end

function Destruction:init(owner)
	self.owner = owner
	self.t, self.next, self.idx = 0, 1.5, 1
	self.pending, self.pieces = {}, {}
	self.targets = {}
	for _, name in ipairs(self.data.targets or {}) do
		local go = scene:find(name)
		if go then self.targets[#self.targets + 1] = go end
	end
end

function Destruction:breakAt(go, hit)
	if not physics or not go then return end
	local parts = physics:fracture(go, self.data.pieces or 14, hit, self.data.mass or 60, math.random(1, 100000))
	local thrown = {}
	for _, p in ipairs(parts) do
		scene:add(p.go)
		self.pieces[#self.pieces + 1] = p.go
		thrown[#thrown + 1] = p
	end
	-- (a body is registered on the scene's next update: velocities given before then are lost)
	self.pending[#self.pending + 1] = { parts = thrown, hit = hit, frames = 0 }
end

function Destruction:update(dt)
	self.t = self.t + dt
	-- the pieces of the last break, kicked out from where it was hit
	for i = #self.pending, 1, -1 do
		local b = self.pending[i]
		b.frames = b.frames + 1
		if b.frames >= 2 then
			for _, p in ipairs(b.parts) do
				if p.body then
					local c = p.go:getPosition()
					local d = Vec3.new(c.x - b.hit.x, c.y - b.hit.y + 0.4, c.z - b.hit.z)
					local l = math.max(0.2, math.sqrt(d.x * d.x + d.y * d.y + d.z * d.z))
					local k = (self.data.blast or 7.0) / l
					pcall(function()
						p.body:activate()
						p.body:setLinearVelocity(Vec3.new(d.x / l * k, d.y / l * k, d.z / l * k))
						p.body:setAngularVelocity(Vec3.new(math.random() * 6 - 3, math.random() * 6 - 3, math.random() * 6 - 3))
					end)
				end
			end
			table.remove(self.pending, i)
		end
	end
	-- the next target, on the clock
	if self.t >= self.next and self.idx <= #self.targets then
		local go = self.targets[self.idx]
		self.idx = self.idx + 1
		self.next = self.t + (self.data.every or 2.2)
		local p = go:getWorldPosition()
		local cam = camera and camera:getWorldPosition() or Vec3.new(0, 2, 10)
		local d = Vec3.new(cam.x - p.x, 0, cam.z - p.z)
		local l = math.max(0.01, math.sqrt(d.x * d.x + d.z * d.z))
		local face = self.data.face or 0.6
		self:breakAt(go, Vec3.new(p.x + d.x / l * face, p.y + 0.3, p.z + d.z / l * face))
	end
	-- a click: what is under the cursor
	if isMouseDown and isMouseDown(1) and not self.wasDown and screenPick then
		local ok, hitGo, hp = pcall(function() return screenPick() end)
		if ok and hitGo then self:breakAt(hitGo, hp or hitGo:getWorldPosition()) end
	end
	self.wasDown = isMouseDown and isMouseDown(1) or false
end

return Destruction
