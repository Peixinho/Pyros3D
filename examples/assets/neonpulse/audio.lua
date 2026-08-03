-- ****************************************************************
-- NEON PULSE - sound.
--
-- Effects are positioned in the arena rather than played flat, so the
-- side a brick breaks on is the side you hear it: the listener rides the
-- camera, and every trigger passes the world position of whatever caused
-- it. The ambience is the one deliberate exception - a bed that has no
-- location and must not move (see A.startAmbience).
--
-- Every sound is generated, not sampled - see sfx/generate_sfx.py.
-- ****************************************************************

local C = import("config")

local A = { sounds = {}, keep = {} }

local SFX_PATH = GAME_PATH .. "sfx/"

-- Same lifetime rule as everything else in this game: sol owns these
-- objects, so anything the audio engine still points at has to stay
-- reachable from Lua.
local function keep(obj)
	A.keep[#A.keep + 1] = obj
	return obj
end

function A.build()
	G.audio = AudioManager.new()
	keep(G.audio)

	if not G.audio:isInitialized() then
		-- No output device (headless, or one held exclusively elsewhere).
		-- The engine already logged it; the game just runs silent, so this
		-- is not worth a second warning or any special-casing below.
		A.enabled = false
		return
	end

	A.enabled = true
	G.audio:setMasterVolume(C.audio.master)

	local function load(name)
		local voices = C.audio.voices[name] or C.audio.voices.default
		local s = keep(Sound.new(SFX_PATH .. name .. ".wav", voices))
		if s:isLoaded() then
			s:setAttenuation(AttenuationModel.Linear, C.audio.minDistance, C.audio.maxDistance)
		end
		A.sounds[name] = s
	end

	load("paddle"); load("wall"); load("brick"); load("armoured")
	load("powerup"); load("lost"); load("levelclear"); load("launch")

	-- Wall hits get a short slap-back echo - the one sound that reads as
	-- bouncing off the arena's own boundary, so a little of the boundary's
	-- "size" coming back on the repeat sells that without needing reverb.
	local wall = A.sounds.wall
	if wall ~= nil and wall:isLoaded() then
		wall:setDelay(0.11, 0.32, 0.45, 1.0)
	end

	A.startAmbience()
end

-- The ambience is an AudioSource rather than a Sound: it loops, it needs to
-- persist and be faded, and it must NOT be positioned - a background bed
-- that pans and attenuates as the camera drifts would read as a bug.
-- Spatialization off makes it a constant, locationless layer.
function A.startAmbience()
	local obj = GameObject.new()
	local src = AudioSource.new(SFX_PATH .. "ambience.wav", true)
	obj:addComponent(src)
	G.scene:add(obj)
	keep(obj); keep(src)
	A.ambienceObject, A.ambience = obj, src

	if not src:isLoaded() then return end
	src:setSpatialization(false)
	src:setLooping(true)
	src:setVolume(C.audio.ambienceVolume)
	-- A gentle low-shelf lift keeps the bed feeling like it has body without
	-- turning it up - a flat hum reads as thin under everything else's
	-- sharper, generated tones.
	src:setEQ(AudioEQType.LowShelf, 220.0, 4.0, 1.0)
	-- Faded in so the very first frame does not start with an abrupt hum.
	src:fadeIn(1500)
end

-- Builds the ball's spatialized loop. Returned rather than attached here so
-- entities.lua can own it alongside the rest of the ball's components.
function A.newBallHum()
	if not A.enabled then return nil end
	local src = keep(AudioSource.new(SFX_PATH .. "hum.wav", false))
	if not src:isLoaded() then return nil end
	src:setSpatialization(true)
	src:setLooping(true)
	src:setVolume(0)
	src:setAttenuation(AttenuationModel.Linear, C.audio.minDistance, C.audio.maxDistance)
	return src
end

-- Maps ball speed onto the hum's pitch, so a ball that has sped up through a
-- level is audibly faster as well as visually.
function A.setHumSpeed(src, speed01)
	if src == nil then return end
	local a = C.audio.humPitchAtBaseSpeed
	local b = C.audio.humPitchAtMaxSpeed
	src:setPitch(a + (b - a) * speed01)
end

-- Positioned trigger. `x`/`y` are arena coordinates; the play plane's z is
-- filled in here so no caller has to think about it.
function A.playAt(name, x, y, pitch)
	if not A.enabled then return end
	local s = A.sounds[name]
	if s == nil or not s:isLoaded() then return end
	s:playAt(Vec3.new(x, y, C.arena.playZ), C.audio.volume[name] or 1.0, pitch or 1.0)
end

-- Flat trigger, for events that belong to the game rather than to a place
-- in the arena (level cleared, ball lost).
function A.play(name, pitch)
	if not A.enabled then return end
	local s = A.sounds[name]
	if s == nil or not s:isLoaded() then return end
	s:play(C.audio.volume[name] or 1.0, pitch or 1.0)
end

-- Called once per frame, after the scene update, so the listener reads a
-- current world transform rather than last frame's. `dt` is this frame's
-- real time step - AudioManager finite-differences it against last frame's
-- listener position to get a real velocity, which is what makes
-- AudioSource:setDopplerFactor() on any moving source actually audible; with
-- no dt (or an unmoving listener) Doppler is silently a no-op regardless of
-- the factor.
function A.update(dt)
	if not A.enabled then return end
	G.audio:setListenerFromGameObject(G.camera, dt)
end

return A
