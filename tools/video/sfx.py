"""Sound effects for the setup video, synthesized here (no samples from anywhere): public/sfx/*.wav, 48 kHz stereo."""
import pathlib
import wave

import numpy as np

SR = 48000
OUT = pathlib.Path(__file__).parent / "public" / "sfx"
rng = np.random.default_rng(7)


def save(name, x, gain=0.8):
    x = np.asarray(x, dtype=np.float64)
    x = x / (np.max(np.abs(x)) + 1e-9) * gain
    stereo = np.stack([x, x], axis=1)
    data = (stereo * 32767).astype("<i2").tobytes()
    OUT.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUT / f"{name}.wav"), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data)


def t(seconds):
    return np.arange(int(SR * seconds)) / SR


def env(n, attack, release):
    a = int(SR * attack)
    e = np.ones(n)
    e[:a] = np.linspace(0, 1, a) if a else 1
    e *= np.exp(-np.arange(n) / (SR * release))
    return e


def lowpass(x, cutoff):
    # one-pole low-pass, cutoff in Hz (may be an array)
    a = np.exp(-2 * np.pi * np.asarray(cutoff) / SR)
    y = np.zeros_like(x)
    acc = 0.0
    a = np.broadcast_to(a, x.shape)
    for i in range(len(x)):
        acc = (1 - a[i]) * x[i] + a[i] * acc
        y[i] = acc
    return y


# whoosh: noise swept up then down through a low-pass, for scene changes
n = t(0.55)
noise = rng.standard_normal(len(n))
sweep = 300 + 5000 * np.sin(np.pi * n / n[-1]) ** 2
save("whoosh", lowpass(noise, sweep) * np.sin(np.pi * n / n[-1]) ** 1.5, 0.55)

# click: a short tick, for the mouse
c = t(0.05)
save("click", (np.sin(2 * np.pi * 2400 * c) * 0.6 + rng.standard_normal(len(c)) * 0.4) * env(len(c), 0.0005, 0.006), 0.6)

# pop: a rising blip, for things appearing
p = t(0.16)
freq = 520 + 900 * (p / p[-1])
save("pop", np.sin(2 * np.pi * np.cumsum(freq) / SR) * env(len(p), 0.003, 0.05), 0.5)

# drop: a soft thud, for files landing in a folder
d = t(0.25)
save("drop", np.sin(2 * np.pi * (140 - 60 * d / d[-1]) * d) * env(len(d), 0.002, 0.07) + lowpass(rng.standard_normal(len(d)), 900) * env(len(d), 0.001, 0.02) * 2, 0.6)

# type: one key of a keyboard
k = t(0.04)
save("type", lowpass(rng.standard_normal(len(k)), 4000) * env(len(k), 0.0003, 0.008), 0.35)

# success: two bright notes
s1, s2 = t(0.14), t(0.32)
note = lambda f, x: (np.sin(2 * np.pi * f * x) + 0.3 * np.sin(4 * np.pi * f * x)) * env(len(x), 0.004, 0.12)
save("success", np.concatenate([note(784, s1), note(1175, s2)]), 0.45)

# tick: a UI step (menu value change)
u = t(0.06)
save("tick", np.sin(2 * np.pi * 1600 * u) * env(len(u), 0.001, 0.015), 0.4)

# boom: TNT
b = t(1.6)
rumble = lowpass(rng.standard_normal(len(b)), 120 + 600 * np.exp(-b * 4)) * 6
crack = rng.standard_normal(len(b)) * np.exp(-b * 30)
save("boom", (rumble + crack) * env(len(b), 0.002, 0.45), 0.9)

# alarm: the warning stamp
a = t(0.5)
save("stamp", (np.sin(2 * np.pi * 90 * a) * 2 + lowpass(rng.standard_normal(len(a)), 1500)) * env(len(a), 0.001, 0.09), 0.8)
print("wrote", sorted(f.name for f in OUT.glob("*.wav")))
