-- Interactive runtime IK. Attached to IKExample's animated model.
--
-- Four limbs are pinned to four draggable handles. Left-click a handle and
-- drag it: the limb follows, while the walk clip keeps driving everything
-- else. That is the whole point of the demo - the clip rewrites the entire
-- pose every frame, and the constraint survives it because the solver runs
-- INSIDE SkeletonAnimation:update(), after the clip has posed the skeleton
-- and before the skinning matrices are uploaded. A solver run from an
-- ordinary component tick would instead be correct or not depending on
-- whether it happened to update after this script.
--
-- Things worth trying:
--   * Drag a hand out past the arm's reach. The arm straightens and stops
--     short instead of stretching - that is the reach clamp, not a bug.
--   * Drag a foot up behind the model. The knee still bends forwards,
--     because human.rig.json clamps it to 0..150 degrees. Without that limit
--     the solver would happily fold it backwards: both solutions reach the
--     target and nothing in the maths prefers the anatomical one.
--   * Press SPACE to pause the walk. The handles still work - IK does not
--     need a clip to be playing.
local IKHandles = class('IKHandles')

-- Handle name -> the IK chain it drives. The chains themselves (root bone,
-- effector bone, pole) live in human.rig.json beside the model, because they
-- describe the SKELETON and are the same for every clip played on it.
local HANDLES = {
	{ object = "Handle L Foot", chain = "LeftLeg",  bone = "Bip01_L_Foot" },
	{ object = "Handle R Foot", chain = "RightLeg", bone = "Bip01_R_Foot" },
	{ object = "Handle L Hand", chain = "LeftArm",  bone = "Bip01_L_Hand" },
	{ object = "Handle R Hand", chain = "RightArm", bone = "Bip01_R_Hand" },
}

-- Pixels. Generous, because the handles are small on screen and this is a
-- demo rather than a precision tool.
local GRAB_RADIUS = 40

function IKHandles:initialize()
	-- SkeletonAnimation:update() wants a clock counting up from the first
	-- call - it derives each clip's position by subtracting the time it first
	-- saw. Per instance, not a file-local: two animated objects sharing this
	-- script would otherwise add into the same counter and run at double speed.
	self.clock = 0
	self.paused = false
	self.grabbed = nil
end

function IKHandles:init(owner)
	self.owner = owner

	local rc = owner:getComponent("RenderingComponent")
	if rc then
		self.instance = rc:getActiveSkeletonAnimation()
		if self.instance then self.anim = self.instance:getOwner() end
	end

	-- Free cursor: this demo is click-and-drag, not mouse-look.
	allowMouseCapture = false
	if setMouseCaptured then setMouseCaptured(false) end

	-- The handle objects are siblings in the scene. They are found by walking
	-- the scene's object list, since there is no lookup-by-name binding.
	self.handles = {}
	if scene then
		for _, h in ipairs(HANDLES) do
			for _, go in ipairs(scene:getAllGameObjects()) do
				if go:getName() == h.object then
					local id = self.instance and self.instance:getBoneIdByName(h.bone) or -1
					self.handles[#self.handles + 1] = { go = go, chain = h.chain, bone = id }
					break
				end
			end
		end
	end

	-- Every constraint starts OFF. A constraint whose handle is riding its
	-- own limb looks harmless but is not: the handle follows the bone, the
	-- solver drags the bone back onto the handle, and between them the limb
	-- freezes exactly where it started - the walk cannot move it at all. Only
	-- the limb actually being dragged should be constrained.
	for _, h in ipairs(HANDLES) do
		setIKConstraintEnabled(owner, h.chain, false)
	end

	local input = Input.new()
	self.input = input

	input:onMouseButtonPressed(MouseButton.Left, function()
		self.grabbed = self:pick()
		if self.grabbed then
			setIKConstraintEnabled(self.owner, self.grabbed.chain, true)
		end
	end)
	input:onMouseButtonReleased(MouseButton.Left, function()
		-- Released: hand the limb back to the animation.
		if self.grabbed then
			setIKConstraintEnabled(self.owner, self.grabbed.chain, false)
		end
		self.grabbed = nil
	end)
	input:onKeyPressed(Key.Space, function()
		self.paused = not self.paused
	end)
end

-- Nearest handle to the cursor in SCREEN space, within GRAB_RADIUS. Screen
-- space rather than a world-space ray so a handle hidden behind the mesh is
-- still grabbable - the handles are manipulators, not scene geometry.
function IKHandles:pick()
	if not camera or not projection or not scene then return nil end
	local w, h = getWindowSize()
	local mx, my = getMousePosition()

	local best, bestDist = nil, GRAB_RADIUS * GRAB_RADIUS
	for _, handle in ipairs(self.handles) do
		local sx, sy, visible = worldToScreen(w, h, camera, projection, handle.go:getWorldPosition())
		if visible then
			local dx, dy = sx - mx, sy - my
			local d2 = dx * dx + dy * dy
			if d2 < bestDist then
				best, bestDist = handle, d2
			end
		end
	end
	return best
end

function IKHandles:update(dt)
	-- Dragging first, so the solve later in this same frame already sees the
	-- new target position.
	if self.grabbed and camera and projection then
		local w, h = getWindowSize()
		local mx, my = getMousePosition()
		-- Keeps the handle's own distance from the camera and only follows the
		-- mouse across the screen, which is what dragging should feel like.
		-- Push/pull along the view axis is deliberately not offered - it has no
		-- unambiguous mouse mapping.
		local p = screenToWorldAtDepth(w, h, mx, my, camera, projection,
			self.grabbed.go:getWorldPosition())
		self.grabbed.go:setPosition(p)
	end

	if not self.paused then
		self.clock = self.clock + dt
	end
	-- Ticked even while paused: this is the call the IK solve rides inside,
	-- so skipping it would freeze the handles too.
	if self.anim then
		self.anim:update(self.clock)
	end

	-- An ungrabbed handle rides its own limb, so it is always sitting exactly
	-- where you would want to grab it. Its constraint is off while it does
	-- this, so the limb animates completely freely.
	if self.instance and self.owner then
		local toWorld = self.owner:getWorldTransformation()
		for _, handle in ipairs(self.handles) do
			if handle ~= self.grabbed and handle.bone >= 0 then
				handle.go:setPosition(toWorld * self.instance:getBonePosition(handle.bone))
			end
		end
	end
end

function IKHandles:destroy()
	allowMouseCapture = nil
	self.input = nil
	self.handles = {}
end

function IKHandles:serialize() return {} end
function IKHandles.deserialize(data) return IKHandles:new() end

return IKHandles
