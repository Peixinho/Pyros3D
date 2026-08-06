/** Neon Pulse audio — port of assets/neonpulse/audio.lua
 *
 * Browser autoplay policy: do not construct AudioManager / AudioSource until
 * the first user gesture (space / click). ScriptProcessorNode warnings come
 * from miniaudio's WebAudio backend and are harmless.
 */

export function createAudio(P, G, C, sfxPath) {
  const A = {
    sounds: {},
    keep: [],
    enabled: false,
    built: false,
    ambienceStarted: false,
  };

  function keep(obj) {
    A.keep.push(obj);
    return obj;
  }

  /** No-op at boot — real init is ensureReady() after a gesture. */
  function build() {}

  function ensureReady(entities) {
    if (A.built) return A.enabled;
    A.built = true;

    G.audio = keep(new P.AudioManager());
    if (!G.audio.isInitialized()) {
      A.enabled = false;
      return false;
    }
    A.enabled = true;
    G.audio.setMasterVolume(C.audio.master);

    const load = (name) => {
      const voices = C.audio.voices[name] ?? C.audio.voices.default;
      const s = keep(new P.Sound(`${sfxPath}${name}.wav`, voices));
      if (s.isLoaded()) {
        s.setAttenuation(P.AttenuationModel_Linear, C.audio.minDistance, C.audio.maxDistance);
      }
      A.sounds[name] = s;
    };

    ["paddle", "wall", "brick", "armoured", "powerup", "lost", "levelclear", "launch"].forEach(load);

    const wall = A.sounds.wall;
    if (wall && wall.isLoaded()) wall.setDelayFull(0.11, 0.32, 0.45, 1.0);

    if (entities && entities.balls) {
      for (const b of entities.balls) {
        if (!b.hum) {
          const hum = newBallHum();
          if (hum) {
            b.go.addAudioSource(hum);
            b.hum = hum;
          }
        }
      }
    }

    startAmbience();
    return true;
  }

  function startAmbience() {
    if (!A.enabled || A.ambienceStarted) return;
    A.ambienceStarted = true;

    const obj = new P.GameObject();
    const src = new P.AudioSource(`${sfxPath}ambience.wav`, true);
    obj.addAudioSource(src);
    G.scene.add(obj);
    keep(obj);
    keep(src);
    A.ambienceObject = obj;
    A.ambience = src;

    if (!src.isLoaded()) return;
    src.setSpatialization(false);
    src.setLooping(true);
    src.setVolume(C.audio.ambienceVolume);
    src.setEQQ(P.AudioEQType_LowShelf, 220.0, 4.0, 1.0);
    src.fadeIn(1500);
  }

  function newBallHum() {
    if (!A.enabled) return null;
    const src = keep(new P.AudioSource(`${sfxPath}hum.wav`, false));
    if (!src.isLoaded()) return null;
    src.setSpatialization(true);
    src.setLooping(true);
    src.setVolume(0);
    src.setAttenuation(P.AttenuationModel_Linear, C.audio.minDistance, C.audio.maxDistance);
    return src;
  }

  function setHumSpeed(src, speed01) {
    if (!src) return;
    const a = C.audio.humPitchAtBaseSpeed;
    const b = C.audio.humPitchAtMaxSpeed;
    src.setPitch(a + (b - a) * speed01);
  }

  function playAt(name, x, y, pitch = 1.0) {
    if (!A.enabled) return;
    const s = A.sounds[name];
    if (!s || !s.isLoaded()) return;
    s.playAtVolumePitch(
      new P.Vec3(x, y, C.arena.playZ),
      C.audio.volume[name] ?? 1.0,
      pitch
    );
  }

  function play(name, pitch = 1.0) {
    if (!A.enabled) return;
    const s = A.sounds[name];
    if (!s || !s.isLoaded()) return;
    s.playVolumePitch(C.audio.volume[name] ?? 1.0, pitch);
  }

  function update(dt) {
    if (!A.enabled) return;
    G.audio.setListenerFromGameObjectDt(G.camera, dt);
  }

  A.build = build;
  A.ensureReady = ensureReady;
  A.startAmbience = startAmbience;
  A.newBallHum = newBallHum;
  A.setHumSpeed = setHumSpeed;
  A.playAt = playAt;
  A.play = play;
  A.update = update;
  return A;
}
