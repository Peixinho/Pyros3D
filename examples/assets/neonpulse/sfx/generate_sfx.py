#!/usr/bin/env python3
"""
Generates NEON PULSE's sound effects.

The game's whole look is synthetic - flat neon on a dark grid - so its audio is
synthesised to match rather than sampled: short waveform blips with hard
envelopes, the way an arcade cabinet's sound chip would have made them. It also
means the assets are tiny, have no licensing attached, and can be re-tuned by
editing numbers here instead of hunting for new files.

Run from anywhere; writes the .wav files next to this script.

    python3 generate_sfx.py

Standard library only - no numpy, so this stays runnable on a bare Python.
"""

import math
import os
import struct
import wave

RATE = 44100


# ******************************** Oscillators ********************************

def square(t, freq, duty=0.5):
    phase = (t * freq) % 1.0
    return 1.0 if phase < duty else -1.0


def sine(t, freq):
    return math.sin(2.0 * math.pi * freq * t)


def triangle(t, freq):
    phase = (t * freq) % 1.0
    return 4.0 * abs(phase - 0.5) - 1.0


def noise(state):
    # Cheap deterministic LFSR - the same seed always produces the same file,
    # so regenerating never creates a spurious diff.
    state ^= (state << 13) & 0xFFFFFFFF
    state ^= state >> 17
    state ^= (state << 5) & 0xFFFFFFFF
    return ((state / 0xFFFFFFFF) * 2.0 - 1.0), state


# ********************************* Envelopes *********************************

def envelope(i, total, attack=0.01, release=0.6):
    """Linear attack, exponential-ish decay. Both expressed as fractions."""
    t = i / total
    if t < attack:
        return t / attack
    d = (t - attack) / max(1e-6, 1.0 - attack)
    return max(0.0, (1.0 - d) ** (1.0 / max(1e-6, release)))


# ********************************** Writing **********************************

def write_wav(name, samples):
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), name)
    # 16-bit mono. Mono matters: miniaudio only spatialises mono sources -
    # a stereo file is played as-is with no positioning at all.
    with wave.open(path, "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        frames = bytearray()
        for s in samples:
            v = max(-1.0, min(1.0, s))
            frames += struct.pack("<h", int(v * 32767))
        w.writeframes(bytes(frames))
    print("wrote %-18s %5.0f ms" % (name, 1000.0 * len(samples) / RATE))


def render(duration, fn):
    n = int(RATE * duration)
    return [fn(i / RATE, i, n) for i in range(n)]


# ********************************** Effects **********************************

def paddle():
    """Bounce off the paddle - low, short, with a quick upward chirp."""
    def f(t, i, n):
        freq = 220.0 + 180.0 * (i / n)
        return 0.42 * square(t, freq, 0.5) * envelope(i, n, 0.005, 0.35)
    return render(0.085, f)


def wall():
    """Bounce off a wall - duller and lower than the paddle, no chirp."""
    def f(t, i, n):
        return 0.30 * square(t, 165.0, 0.25) * envelope(i, n, 0.004, 0.28)
    return render(0.060, f)


def brick():
    """Brick destroyed - bright descending blip, the main feedback sound."""
    def f(t, i, n):
        freq = 900.0 - 420.0 * (i / n)
        return 0.38 * (0.7 * square(t, freq, 0.5) + 0.3 * sine(t, freq * 2.0)) * envelope(i, n, 0.004, 0.30)
    return render(0.110, f)


def armoured():
    """Armoured brick survived a hit - metallic, unresolved, no descent."""
    def f(t, i, n):
        return 0.30 * (0.6 * square(t, 620.0, 0.15) + 0.4 * triangle(t, 930.0)) * envelope(i, n, 0.003, 0.22)
    return render(0.070, f)


def powerup():
    """Power-up caught - rising arpeggio, unmistakably a reward."""
    steps = [523.25, 659.25, 783.99, 1046.50]  # C E G C
    def f(t, i, n):
        step = min(len(steps) - 1, int((i / n) * len(steps)))
        return 0.34 * (0.65 * square(t, steps[step], 0.5) + 0.35 * sine(t, steps[step])) * envelope(i, n, 0.01, 0.5)
    return render(0.280, f)


def lost():
    """Ball lost - falling tone, the only genuinely downbeat sound here."""
    def f(t, i, n):
        freq = 420.0 * (1.0 - 0.62 * (i / n))
        return 0.36 * (0.5 * square(t, freq, 0.5) + 0.5 * triangle(t, freq)) * envelope(i, n, 0.01, 0.75)
    return render(0.520, f)


def levelclear():
    """Level cleared - a longer major fanfare."""
    steps = [523.25, 659.25, 783.99, 1046.50, 1318.51]
    def f(t, i, n):
        step = min(len(steps) - 1, int((i / n) * len(steps)))
        base = steps[step]
        v = 0.30 * (0.55 * square(t, base, 0.5) + 0.25 * sine(t, base * 2.0) + 0.20 * triangle(t, base * 0.5))
        return v * envelope(i, n, 0.01, 0.55)
    return render(0.620, f)


def launch():
    """Ball launched - a short upward whoosh with a little noise on top."""
    st = [0x1234567]
    def f(t, i, n):
        freq = 180.0 + 620.0 * (i / n)
        nz, st[0] = noise(st[0])
        return (0.26 * sine(t, freq) + 0.06 * nz) * envelope(i, n, 0.02, 0.45)
    return render(0.180, f)


def ambience():
    """
    Looping background hum.

    Deliberately near-featureless: two slightly detuned low sines plus a slow
    beat between them, so it reads as room tone rather than as music competing
    with the effects. The length is chosen so the detune beat completes a whole
    number of cycles, which is what makes the loop point inaudible.
    """
    duration = 4.0
    def f(t, i, n):
        base = 55.0
        v = 0.10 * sine(t, base) + 0.08 * sine(t, base * 1.5) + 0.05 * sine(t, base * 2.01)
        # Slow swell - exactly 2 cycles over the loop, so it meets itself.
        v *= 0.75 + 0.25 * math.sin(2.0 * math.pi * (2.0 / duration) * t)
        # Fade the very edges into each other to kill any residual click.
        edge = 0.02
        if t < edge:
            v *= t / edge
        elif t > duration - edge:
            v *= (duration - t) / edge
        return v
    return render(duration, f)


if __name__ == "__main__":
    write_wav("paddle.wav", paddle())
    write_wav("wall.wav", wall())
    write_wav("brick.wav", brick())
    write_wav("armoured.wav", armoured())
    write_wav("powerup.wav", powerup())
    write_wav("lost.wav", lost())
    write_wav("levelclear.wav", levelclear())
    write_wav("launch.wav", launch())
    write_wav("ambience.wav", ambience())
