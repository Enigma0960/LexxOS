#!/usr/bin/env python3
"""LexxOS Black Ice boot v2.

Timeline (seconds):
    0.30–1.70 clean, smooth, solid white ring fades in from black;
    1.70–2.20 hold the untouched ring;
    2.20–3.48 ring acquires fractures, horizontal displacements and red glitches;
    3.40–4.22 central LEXXOS wordmark materializes;
    4.62–5.15 two final digital interference bursts;
    5.30–8.00 hold the original approved logo.

Requires numpy, Pillow, OpenCV and ffmpeg in PATH. Run in the same
folder as a_clean_high_resolution_dark_sci_fi_graphic_logo.png.
"""
from pathlib import Path
import subprocess
import cv2
import numpy as np
from PIL import Image, ImageOps, ImageDraw

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "a_clean_high_resolution_dark_sci_fi_graphic_logo.png"
OUT_VIDEO = ROOT / "LexxOS_Black_Ice_Boot_V2_720.mp4"
OUT_VIDEO_320 = ROOT / "LexxOS_Black_Ice_Boot_V2_320.mp4"
OUT_GIF = ROOT / "LexxOS_Black_Ice_Boot_V2_Preview.gif"
OUT_SHEET = ROOT / "LexxOS_Black_Ice_Boot_V2_Stages.jpg"

SIZE = 720
FPS = 30
DURATION = 8.0
W = H = SIZE

# Preserve original final image, without introducing any added text or icon.
original = cv2.imread(str(SOURCE), cv2.IMREAD_COLOR)
if original is None:
    raise FileNotFoundError(SOURCE)
base = cv2.resize(
    cv2.cvtColor(original, cv2.COLOR_BGR2RGB), (W, H),
    interpolation=cv2.INTER_AREA,
).astype(np.float32)
base = np.clip((base - 3.2) * (255.0 / (255.0 - 3.2)), 0, 255)

YY, XX = np.mgrid[:H, :W].astype(np.float32)
scale = SIZE / 1254

# The image contains scattered glitches outside the circle; isolate the
# central wordmark so it does not become visible before the ring is distorted.
font_mask = np.zeros((H, W), np.float32)
x0, x1 = int(306 * scale), int(1017 * scale)
y0, y1 = int(553 * scale), int(671 * scale)
font_mask[y0:y1, x0:x1] = 1.0
font_mask = cv2.GaussianBlur(font_mask, (0, 0), 0.8)
wordmark = base * font_mask[:, :, None]
source_ring = base * (1.0 - font_mask[:, :, None])

# Clean, unbroken ring; no noise, no scanlines, no highlights, no typography.
# Its center and silhouette approximately match the approved rough ring.
cx, cy = 359.0, 351.0
radius = np.sqrt((XX - cx) ** 2 + (YY - cy) ** 2)
inner, outer = 230.0, 280.0
# Soft 1px antialiasing only; maintains a crisp edge on 320x320 display.
def smoothstep(x):
    v = np.clip(x, 0.0, 1.0)
    return v * v * (3.0 - 2.0 * v)

shape = smoothstep((radius - inner + 0.8) / 1.6) * smoothstep((outer - radius + 0.8) / 1.6)
clean_ring = shape[:, :, None] * np.array([251.0, 251.0, 251.0], dtype=np.float32)


def interpolate(a: np.ndarray, b: np.ndarray, amount: float):
    return a * (1.0 - amount) + b * amount


def distort_ring(frame: np.ndarray, t: float, amount: float, frame_no: int):
    """Localized, progressively stronger horizontal scanline displacements."""
    if amount <= 0.002:
        return frame
    rng = np.random.default_rng(7000 + frame_no)
    # Coherent horizontal bands, not full-frame randomized TV noise.
    bands = int(3 + amount * 20)
    for _ in range(bands):
        y = int(rng.integers(46, H - 46))
        thickness = int(rng.integers(1, 3 + int(13 * amount)))
        offset = int(rng.integers(-int(3 + 36 * amount), int(4 + 36 * amount)))
        ys = slice(y, min(y + thickness, H))
        frame[ys] = np.roll(frame[ys], offset, axis=1)
        # Fragmentary red/white edges that stay near actual image data.
        if rng.random() < 0.42:
            row = frame[ys]
            lit = np.max(row, axis=2) > 28
            row[:, :, 0] = np.maximum(row[:, :, 0], lit * (50 + 135 * amount))
    # Occasional strong short local strip, like image buffer corruption.
    if amount > 0.38 and int(t * 18) % 4 == 0:
        y = int(150 + 340 * ((np.sin(t * 9) + 1) * 0.5))
        height = int(3 + 12 * amount)
        frame[y:y + height] = np.roll(frame[y:y + height], int(15 + 45 * amount), axis=1)
    return frame


def final_interference(img: np.ndarray, level: float, frame_no: int):
    """Digital bursts similar to the previous animation's closing glitch."""
    if level < .01:
        return img
    rng = np.random.default_rng(30000 + frame_no)
    out = img.copy()
    for _ in range(int(7 + 23 * level)):
        y = int(rng.integers(12, H - 14))
        h = int(rng.integers(1, 3 + int(19 * level)))
        xshift = int(rng.integers(-int(6 + 49 * level), int(7 + 49 * level)))
        ys = slice(y, min(H, y + h))
        out[ys] = np.roll(out[ys], xshift, axis=1)
        if rng.random() < .35:
            x0 = int(rng.integers(70, W - 90))
            span = int(rng.integers(35, 260))
            out[y:y + 1, x0:min(W, x0 + span), 0] = int(60 + 160 * level)
    if level > .24:
        # RGB separation only during the shock, never on the initial clean ring.
        out[:, :, 0] = np.roll(out[:, :, 0], int(1 + 7 * level), axis=1)
        out[:, :, 2] = np.roll(out[:, :, 2], -int(1 + 4 * level), axis=1)
    return np.clip(out * (1.0 + .11 * level), 0, 255)


def frame_at(frame_no: int) -> np.ndarray:
    t = frame_no / FPS
    appear = float(smoothstep((t - .28) / 1.37))
    corruption = float(smoothstep((t - 2.18) / 1.24))

    # Starts on true black, with only a solid clean ring fading into view.
    distorted = interpolate(clean_ring, source_ring, corruption)
    if 2.18 < t < 3.55:
        # Burst envelope peaks near the end of the transformation.
        pulse = float(np.sin(np.pi * np.clip((t - 2.18) / 1.32, 0, 1)) ** 2)
        strength = corruption * (0.33 + 0.42 * pulse)
        distorted = distort_ring(distorted, t, strength, frame_no)
    frame = distorted * appear

    # Name materializes only after the ring starts breaking up.
    reveal = float(smoothstep((t - 3.37) / .82))
    if reveal > 0:
        # Digital left-to-right reveal with tightly controlled horizontal jitter.
        sweep = x0 + (x1 - x0) * reveal
        row_jitter = 4.1 * np.sin(YY * .37 + t * 19) + 2.0 * np.sin(YY * .88 - t * 8)
        coverage = np.clip((sweep - XX + row_jitter) / 3.4 + .5, 0, 1)
        frame += wordmark * coverage[:, :, None] * (.9 + .1 * reveal)

    # Source artwork restored exactly after text completes.
    if t >= 4.26:
        frame = base.copy()

    hit1 = max(0.0, 1.0 - abs(t - 4.72) / .18)
    hit2 = max(0.0, 1.0 - abs(t - 5.03) / .13) * .56
    shock = max(hit1, hit2)
    if shock:
        frame = final_interference(frame, shock, frame_no)

    return np.clip(frame, 0, 255).astype(np.uint8)


def write():
    cmd = [
        "ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-vcodec", "rawvideo",
        "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(FPS), "-i", "-",
        "-an", "-c:v", "libx264", "-preset", "medium", "-crf", "17",
        "-pix_fmt", "yuv420p", "-movflags", "+faststart", str(OUT_VIDEO),
    ]
    process = subprocess.Popen(cmd, stdin=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        for number in range(int(FPS * DURATION)):
            process.stdin.write(frame_at(number).tobytes())
    finally:
        process.stdin.close()
        stderr = process.stderr.read().decode("utf-8", "replace")
        if process.wait() != 0:
            raise RuntimeError(stderr)

    subprocess.run([
        "ffmpeg", "-y", "-loglevel", "error", "-i", str(OUT_VIDEO),
        "-vf", "scale=320:320:flags=lanczos", "-r", str(FPS), "-an", "-c:v", "libx264",
        "-preset", "slow", "-crf", "19", "-pix_fmt", "yuv420p",
        "-movflags", "+faststart", str(OUT_VIDEO_320)
    ], check=True)
    # Smaller looping preview; only GIF fades to black at the end.
    subprocess.run([
        "ffmpeg", "-y", "-loglevel", "error", "-i", str(OUT_VIDEO),
        "-filter_complex",
        "[0:v]fps=15,scale=360:360:flags=lanczos,fade=t=out:st=7.55:d=0.45,split[s0][s1];"
        "[s0]palettegen=max_colors=192:stats_mode=diff[p];"
        "[s1][p]paletteuse=dither=sierra2_4a[v]",
        "-map", "[v]", "-loop", "0", str(OUT_GIF)
    ], check=True)

    # Visual QA storyboard, with the relevant stages labelled outside each frame.
    check_times = [0.05, 0.85, 1.82, 2.58, 3.12, 3.82, 4.28, 4.72, 5.65]
    labels = ["BLACK", "FADE IN", "CLEAN RING", "DISTORT", "GLITCH", "LEXXOS", "STABLE", "SIGNAL NOISE", "FINAL"]
    tile = 300
    band_h = 28
    board = Image.new("RGB", (tile*3, (tile+band_h)*3), "#151515")
    draw = ImageDraw.Draw(board)
    for j,(tt,label) in enumerate(zip(check_times, labels)):
        i = round(tt * FPS)
        img = Image.fromarray(frame_at(i)).resize((tile,tile),Image.Resampling.LANCZOS)
        px,py = (j%3)*tile, (j//3)*(tile+band_h)
        board.paste(img, (px,py))
        draw.text((px+10, py+tile+7), f"{tt:.2f}s  {label}", fill="white")
    board.save(OUT_SHEET, quality=91)
    for p in [OUT_VIDEO, OUT_VIDEO_320, OUT_GIF, OUT_SHEET]:
        print(f"{p.name}: {p.stat().st_size:,} bytes")


if __name__ == "__main__":
    write()
