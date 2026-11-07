#!/usr/bin/env python3
"""Captures the four companion app tabs from the attached phone for the wiki.

Phone preconditions: app installed (make companion-install), light theme with the app theme
on "System default", app connected to a device, firmware current so About shows device details.
"""
import io
import subprocess
import sys
import time
from pathlib import Path

APP_ID = "com.pgpemu.companion"
TABS = ("dashboard", "settings", "diagnostics", "about")
OUT_DIR = Path(__file__).resolve().parent.parent / "docs" / "wiki" / "screenshots"
WIDTH, HEIGHT = 540, 955
TOP_CROP = 0.05
BOTTOM_CROP = 0.115
TAB_Y = 0.93
SETTLE_SECONDS = 1.0


def tab_tap_point(index: int, width: int, height: int) -> tuple[int, int]:
    return int((index + 0.5) / len(TABS) * width), int(TAB_Y * height)


def process(png: bytes) -> bytes:
    from PIL import Image

    img = Image.open(io.BytesIO(png)).convert("RGB")
    w, h = img.size
    img = img.crop((0, int(TOP_CROP * h), w, h - int(BOTTOM_CROP * h)))
    img = img.resize((WIDTH, round(img.height * WIDTH / w)), Image.LANCZOS)
    if img.height < HEIGHT:
        raise ValueError(f"capture too short after crop: {img.height} < {HEIGHT}")
    out = io.BytesIO()
    img.crop((0, 0, WIDTH, HEIGHT)).save(out, "PNG")
    return out.getvalue()


def adb(*args: str) -> bytes:
    return subprocess.run(["adb", *args], check=True, capture_output=True).stdout


def main() -> int:
    try:
        import PIL  # noqa: F401
    except ImportError:
        print("Pillow missing: pip install pillow", file=sys.stderr)
        return 1
    from PIL import Image

    try:
        adb("shell", "am", "start", "-n", f"{APP_ID}/.MainActivity")
        time.sleep(SETTLE_SECONDS * 2)
        width, height = Image.open(io.BytesIO(adb("exec-out", "screencap", "-p"))).size
        OUT_DIR.mkdir(parents=True, exist_ok=True)
        for i, tab in enumerate(TABS):
            x, y = tab_tap_point(i, width, height)
            adb("shell", "input", "tap", str(x), str(y))
            time.sleep(SETTLE_SECONDS)
            path = OUT_DIR / f"app-{tab}.png"
            path.write_bytes(process(adb("exec-out", "screencap", "-p")))
            print(f"Saved {path}")
    except (subprocess.CalledProcessError, FileNotFoundError, ValueError) as e:
        print(e, file=sys.stderr)
        return 1
    print("Review each image for device addresses and personal data before commit.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
