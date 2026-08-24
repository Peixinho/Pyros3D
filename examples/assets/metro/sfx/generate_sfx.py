#!/usr/bin/env python3
"""Generates the METRO sfx/*.wav files. Pure stdlib, run with python3.

Every file is 16-bit PCM mono @ 44100 Hz.

The palette is deliberately dry and mechanical: a metro station is a
concrete box, so the weapon is mostly noise with a short body thump and
a metallic tail, and the creatures are pitched-down noise rather than
anything tonal. `ambience.wav` is the one true loop - its partials are
integer multiples of 1/loop_len so the seam is mathematically silent.
"""
import math
import os
import random
import struct
import wave

SR = 44100
OUT = os.path.dirname(os.path.abspath(__file__))


def write_wav(name, samples, normalize=True):
    path = os.path.join(OUT, name)
    if normalize:
        peak = max(abs(s) for s in samples) or 1.0
        if peak > 1.0:
            samples = [s / peak for s in samples]
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        frames = bytearray()
        for s in samples:
            frames += struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32000))
        w.writeframes(bytes(frames))
    print(f"  {name:16s} {len(samples)/SR:6.2f}s  {len(samples):8d} samples")


# ----------------------------------------------------------------- tools

def onepole(samples, coeff):
    """Simple low-pass. coeff near 0 = heavy filtering, 1 = none."""
    out = []
    z = 0.0
    for s in samples:
        z += (s - z) * coeff
        out.append(z)
    return out


def highpass(samples, coeff):
    out = []
    z = 0.0
    for s in samples:
        z += (s - z) * coeff
        out.append(s - z)
    return out


def noise(n, seed=0):
    rng = random.Random(seed)
    return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def decay_env(n, curve=6.0, attack=0.002):
    a = max(1, int(SR * attack))
    out = []
    for i in range(n):
        t = i / n
        e = math.exp(-curve * t)
        if i < a:
            e *= i / a
        out.append(e)
    return out


def sweep(n, f0, f1, shape=1.0, kind="sine"):
    out = []
    phase = 0.0
    for i in range(n):
        t = (i / n) ** shape
        f = f0 + (f1 - f0) * t
        phase += 2 * math.pi * f / SR
        if kind == "square":
            out.append(0.5 if math.sin(phase) > 0 else -0.5)
        elif kind == "saw":
            out.append((phase / (2 * math.pi) % 1.0) * 2 - 1)
        else:
            out.append(math.sin(phase))
    return out


def mix(*layers):
    n = max(len(l) for l in layers)
    out = [0.0] * n
    for l in layers:
        for i, s in enumerate(l):
            out[i] += s
    return out


def pad(samples, n):
    return samples + [0.0] * max(0, n - len(samples))


# ---------------------------------------------------------------- weapon

def gen_shoot():
    """A 9mm in a tiled tunnel: crack, body thump, metallic ring-out."""
    n = int(SR * 0.34)

    crack = noise(n, seed=11)
    crack = highpass(crack, 0.55)
    env = decay_env(n, curve=42.0, attack=0.0004)
    crack = [s * e * 0.95 for s, e in zip(crack, env)]

    body = sweep(int(SR * 0.09), 190, 55, shape=0.5)
    body = [s * e for s, e in zip(body, decay_env(len(body), curve=16.0))]
    body = pad(body, n)

    # The tail is what places the shot indoors - a filtered noise
    # decay long enough to read as a room, short enough not to smear
    # into the next round at 640 rpm.
    tail = noise(n, seed=12)
    tail = onepole(tail, 0.16)
    tenv = decay_env(n, curve=7.0, attack=0.004)
    tail = [s * e * 0.55 for s, e in zip(tail, tenv)]

    write_wav("shoot.wav", mix(crack, body, tail))


def gen_reload():
    """Magazine out, magazine in, bolt release - three mechanical clicks."""
    n = int(SR * 0.85)
    out = [0.0] * n

    def click(at, dur, f, seed, vol):
        start = int(SR * at)
        m = int(SR * dur)
        c = noise(m, seed=seed)
        c = highpass(c, 0.4)
        e = decay_env(m, curve=55.0, attack=0.0002)
        tone = sweep(m, f, f * 0.6, shape=0.6)
        for i in range(m):
            if start + i < n:
                out[start + i] += (c[i] * 0.8 + tone[i] * 0.35) * e[i] * vol

    click(0.02, 0.09, 900, 21, 0.8)     # magazine release
    click(0.30, 0.10, 620, 22, 0.9)     # fresh magazine seated
    click(0.60, 0.12, 1400, 23, 1.0)    # bolt
    write_wav("reload.wav", out)


def gen_empty():
    n = int(SR * 0.10)
    c = highpass(noise(n, seed=31), 0.5)
    e = decay_env(n, curve=70.0, attack=0.0002)
    t = sweep(n, 2200, 1400, shape=0.5)
    write_wav("empty.wav", [(a * 0.7 + b * 0.4) * ev for a, b, ev in zip(c, t, e)])


def gen_hit():
    """The hitmarker: short, bright, unmistakably feedback rather than world."""
    n = int(SR * 0.07)
    t = sweep(n, 1800, 2600, shape=0.4)
    e = decay_env(n, curve=45.0, attack=0.0005)
    write_wav("hit.wav", [s * ev * 0.55 for s, ev in zip(t, e)])


# -------------------------------------------------------------- creatures

def gen_enemy_hit():
    n = int(SR * 0.16)
    wet = onepole(noise(n, seed=41), 0.30)
    e = decay_env(n, curve=26.0, attack=0.001)
    body = sweep(n, 260, 90, shape=0.6)
    write_wav("enemyHit.wav",
              [(a * 0.9 + b * 0.5) * ev for a, b, ev in zip(wet, body, e)])


def gen_enemy_die():
    """A wet collapse with a descending growl under it."""
    n = int(SR * 0.75)
    growl = sweep(n, 180, 42, shape=0.8, kind="saw")
    genv = decay_env(n, curve=6.0, attack=0.01)
    growl = [s * e * 0.6 for s, e in zip(growl, genv)]

    # Slow amplitude wobble so the growl sounds like something breathing
    # out rather than a synth tone sliding down.
    growl = [s * (0.7 + 0.3 * math.sin(2 * math.pi * 11.0 * i / SR))
             for i, s in enumerate(growl)]

    wet = onepole(noise(n, seed=42), 0.22)
    wet = [s * e * 0.7 for s, e in zip(wet, decay_env(n, curve=9.0, attack=0.005))]
    write_wav("enemyDie.wav", mix(growl, wet))


def gen_enemy_attack():
    n = int(SR * 0.28)
    hiss = highpass(noise(n, seed=43), 0.35)
    henv = decay_env(n, curve=14.0, attack=0.006)
    snarl = sweep(n, 420, 150, shape=0.7, kind="saw")
    write_wav("enemyAttack.wav",
              [(a * 0.75 + b * 0.45) * e for a, b, e in zip(hiss, snarl, henv)])


def gen_spawn():
    """Something dropping onto the rail bed, far off down the tunnel."""
    n = int(SR * 0.55)
    thud = sweep(n, 90, 34, shape=0.5)
    tenv = decay_env(n, curve=11.0, attack=0.003)
    scrape = onepole(noise(n, seed=44), 0.09)
    senv = decay_env(n, curve=5.0, attack=0.02)
    write_wav("spawn.wav",
              [a * e1 * 0.85 + b * e2 * 0.4
               for a, b, e1, e2 in zip(thud, scrape, tenv, senv)])


# ----------------------------------------------------------------- player

def gen_hurt():
    n = int(SR * 0.40)
    thump = sweep(n, 140, 48, shape=0.5)
    tenv = decay_env(n, curve=13.0, attack=0.001)
    # A short burst of filtered noise on top reads as impact rather than
    # as a bass note.
    imp = onepole(noise(n, seed=51), 0.35)
    ienv = decay_env(n, curve=30.0, attack=0.0008)
    write_wav("hurt.wav",
              [a * e1 * 0.9 + b * e2 * 0.6
               for a, b, e1, e2 in zip(thump, imp, tenv, ienv)])


def gen_pickup():
    n = int(SR * 0.30)
    out = [0.0] * n
    # A rising two-note chime - the only friendly sound in the station.
    for k, (f, at) in enumerate([(880, 0.0), (1320, 0.09)]):
        start = int(SR * at)
        m = n - start
        tone = sweep(m, f, f * 1.01, shape=1.0)
        e = decay_env(m, curve=9.0, attack=0.004)
        for i in range(m):
            out[start + i] += tone[i] * e[i] * (0.5 if k == 0 else 0.42)
    write_wav("pickup.wav", out)


def gen_step():
    n = int(SR * 0.13)
    s = onepole(noise(n, seed=61), 0.22)
    e = decay_env(n, curve=34.0, attack=0.001)
    grit = highpass(noise(n, seed=62), 0.5)
    write_wav("step.wav",
              [(a * 0.8 + b * 0.18) * ev for a, b, ev in zip(s, grit, e)])


# ------------------------------------------------------------------- game

def gen_wave():
    """PA-style two-tone, distorted by the station's dead speakers."""
    n = int(SR * 1.1)
    out = [0.0] * n
    for f, at, dur in [(392, 0.0, 0.45), (523, 0.35, 0.7)]:
        start = int(SR * at)
        m = min(int(SR * dur), n - start)
        tone = sweep(m, f, f, shape=1.0)
        e = decay_env(m, curve=4.0, attack=0.02)
        for i in range(m):
            # Soft clip: the tannoy is blown.
            v = tone[i] * e[i] * 0.6
            out[start + i] += math.tanh(v * 2.2) * 0.45
    out = onepole(out, 0.35)
    write_wav("wave.wav", out)


def gen_gameover():
    n = int(SR * 1.8)
    drone = sweep(n, 150, 38, shape=1.4, kind="saw")
    e = decay_env(n, curve=2.2, attack=0.05)
    drone = [s * ev * 0.7 for s, ev in zip(drone, e)]
    drone = onepole(drone, 0.28)
    rumble = onepole(noise(n, seed=71), 0.05)
    rumble = [s * ev * 0.8 for s, ev in zip(rumble, decay_env(n, curve=2.0, attack=0.1))]
    write_wav("gameover.wav", mix(drone, rumble))


def gen_ambience():
    """Seamless room tone: deep rumble, a mains hum, and moving air.

    Every partial's frequency is an integer multiple of 1/LOOP, so the
    last sample runs into the first with no discontinuity - the usual
    reason a generated loop ticks once per cycle.
    """
    LOOP = 8.0
    n = int(SR * LOOP)
    base = 1.0 / LOOP

    out = [0.0] * n
    # Sub-bass bed: distant trains and the weight of the earth above.
    for mult, amp, phase in [(3, 0.30, 0.0), (5, 0.20, 1.1), (8, 0.13, 2.3),
                             (13, 0.09, 0.7)]:
        f = base * mult
        for i in range(n):
            out[i] += math.sin(2 * math.pi * f * i / SR + phase) * amp

    # 50 Hz mains hum from the failing lighting, plus its third harmonic.
    # 50 and 150 are both integer multiples of 1/8, so still seamless.
    for f, amp in [(50.0, 0.05), (150.0, 0.018)]:
        for i in range(n):
            out[i] += math.sin(2 * math.pi * f * i / SR) * amp

    # Air: noise low-passed hard, then cross-faded with itself at the
    # half-way point so the noise layer loops as cleanly as the tones.
    air = onepole(noise(n, seed=81), 0.02)
    air = [s * 6.0 for s in air]
    half = n // 2
    looped = [0.0] * n
    for i in range(n):
        w = i / n
        looped[i] = air[i] * (1 - w) + air[(i + half) % n] * w
    for i in range(n):
        out[i] += looped[i] * 0.28

    peak = max(abs(s) for s in out) or 1.0
    out = [s / peak * 0.75 for s in out]
    write_wav("ambience.wav", out, normalize=False)


if __name__ == "__main__":
    print("Generating METRO sfx...")
    gen_shoot()
    gen_reload()
    gen_empty()
    gen_hit()
    gen_enemy_hit()
    gen_enemy_die()
    gen_enemy_attack()
    gen_spawn()
    gen_hurt()
    gen_pickup()
    gen_step()
    gen_wave()
    gen_gameover()
    gen_ambience()
    print("done.")
