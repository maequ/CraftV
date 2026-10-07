"""Builds the CraftV setup video (dist/CraftV-setup-video.mp4): slides drawn with Pillow (deck.py), narration read by
Windows' built-in voice (System.Speech, slides.py), put together with ffmpeg. Nothing is downloaded.

    python tools/tutorial/make_video.py
"""
import json
import pathlib
import shutil
import subprocess
import sys
import wave

ROOT = pathlib.Path(__file__).resolve().parents[2]
WORK = ROOT / "build" / "tutorial"
OUT = ROOT / "dist" / "CraftV-setup-video.mp4"
FFMPEG = shutil.which("ffmpeg") or str(pathlib.Path.home() / "bin" / "ffmpeg.exe")
VOICE = "Microsoft David Desktop"
W, H = 1920, 1080
PAUSE = 1.0  # seconds of quiet after each slide's narration

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from deck import DECK  # noqa: E402
from slides import SLIDES  # noqa: E402


def render(i: int) -> pathlib.Path:
    png = WORK / f"slide{i:02d}.png"
    DECK[i]().save(png)
    return png


def speak(i: int, text: str) -> pathlib.Path:
    wav = WORK / f"voice{i:02d}.wav"
    ps = f"""Add-Type -AssemblyName System.Speech
$s = New-Object System.Speech.Synthesis.SpeechSynthesizer
$s.SelectVoice('{VOICE}')
$s.Rate = 0
$s.SetOutputToWaveFile('{wav}')
$s.Speak({json.dumps(text)})
$s.Dispose()"""
    script = WORK / f"voice{i:02d}.ps1"
    script.write_text(ps, encoding="utf-8-sig")
    subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(script)], check=True, capture_output=True)
    return wav


def seconds(wav: pathlib.Path) -> float:
    with wave.open(str(wav)) as w:
        return w.getnframes() / w.getframerate()


def main() -> None:
    if len(DECK) != len(SLIDES):
        raise SystemExit(f"deck.py has {len(DECK)} slides but slides.py has {len(SLIDES)} narrations")
    WORK.mkdir(parents=True, exist_ok=True)
    clips = []
    total = 0.0
    for i, slide in enumerate(SLIDES):
        png = render(i)
        wav = speak(i, slide["say"])
        length = seconds(wav) + PAUSE
        total += length
        clip = WORK / f"clip{i:02d}.mp4"
        fade_out = max(length - 0.35, 0)
        subprocess.run([FFMPEG, "-y", "-loglevel", "error", "-loop", "1", "-framerate", "30", "-i", str(png), "-i", str(wav),
                        "-filter_complex", f"[0:v]scale={W}:{H},format=yuv420p,fade=t=in:st=0:d=0.35,fade=t=out:st={fade_out:.2f}:d=0.35[v];"
                        f"[1:a]apad=pad_dur={PAUSE},aresample=48000[a]",
                        "-map", "[v]", "-map", "[a]", "-t", f"{length:.2f}", "-c:v", "libx264", "-preset", "veryfast", "-tune", "stillimage",
                        "-crf", "18", "-c:a", "aac", "-b:a", "160k", "-ar", "48000", "-ac", "2", str(clip)], check=True)
        clips.append(clip)
        print(f"slide {i + 1}/{len(SLIDES)}: {length:.1f} s", flush=True)
    listing = WORK / "clips.txt"
    listing.write_text("".join(f"file '{c.as_posix()}'\n" for c in clips), encoding="utf-8")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([FFMPEG, "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", str(listing), "-c", "copy", "-movflags", "+faststart", str(OUT)],
                   check=True)
    print(f"wrote {OUT} ({total / 60:.1f} min)")


if __name__ == "__main__":
    main()
