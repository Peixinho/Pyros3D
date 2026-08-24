-- ****************************************************************
-- METRO - sound.
--
-- Effects are positioned in the station rather than played flat, so a
-- crawler coming up the tunnel behind you is heard behind you. The
-- listener rides the camera. Two loops sit under everything: the room
-- tone (locationless) and the tunnel draught.
--
-- All sounds are generated - see sfx/generate_sfx.py.
-- ****************************************************************

local C = import("config")

local A = { sounds = {}, keep = {} }

local SFX_PATH = GAME_PATH .. "sfx/"

-- sol owns these; anything the audio engine still points at has to stay
-- reachable from Lua or it is collected out from under the mixer.
local function keep(obj)
	A.keep[#A.keep + 1] = obj
	return obj
end

function A.build()
	G.audio = AudioManager.new()
	keep(G.audio)

	if not G.audio:isInitialized() then
		A.enabled = false
		echo("[METRO] audio unavailable - running silent")
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

	load("shoot"); load("reload"); load("empty"); load("hit")
	load("enemyHit"); load("enemyDie"); load("enemyAttack"); load("spawn")
	load("hurt"); load("pickup"); load("wave"); load("gameover"); load("step")

	A.startAmbience()
end

-- The room tone: a locationless bed that must not pan or attenuate as
-- the player turns. Low-shelved so it sits under the action as air
-- rather than as noise.
function A.startAmbience()
	local obj = GameObject.new()
	local src = AudioSource.new(SFX_PATH .. "ambience.wav", true)
	obj:addComponent(src)
	G.scene:add(obj)
	keep(obj); keep(src)
	A.ambience = src

	if not src:isLoaded() then return end
	src:setSpatialization(false)
	src:setLooping(true)
	src:setVolume(C.audio.ambienceVolume)
	src:setEQ(AudioEQType.LowShelf, 180.0, 5.0, 1.0)
	src:fadeIn(2000)
end

-- One entry point for every effect. With coordinates it is positioned;
-- without them it is a flat UI-level cue (wave start, game over).
function A.play(name, x, y, z, pitch)
	if not A.enabled then return end
	local s = A.sounds[name]
	if s == nil or not s:isLoaded() then return end
	local vol = C.audio.volume[name] or 1.0
	if x then
		s:playAt(Vec3.new(x, y or 0, z or 0), vol, pitch or 1.0)
	else
		s:play(vol, pitch or 1.0)
	end
end

function A.update(dt, camera)
	if not A.enabled then return end
	G.audio:setListenerFromGameObject(camera, dt)
end

function A.destroy()
	if not A.enabled then return end
	if A.ambience then pcall(function() A.ambience:stop() end) end
	for _, s in pairs(A.sounds) do
		pcall(function() s:stop() end)
	end
	A.sounds = {}
	A.enabled = false
	A.keep = {}
end

return A
