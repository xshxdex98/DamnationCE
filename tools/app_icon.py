#!/usr/bin/env python3
"""Makes DamnationCE's icons from its logo, port/assets/logo.png.

    python tools/app_icon.py

Writes port/android/art/android-icon.png (tools/android_icon.py makes the
Android launcher icon from it; run that next), port/macos/AppIcon.icns,
port/windows/damnationce.ico (halo.exe's icon: port/windows/halo.rc),
port/assets/icon/window-icon-256.png (the desktop window's: embedded by
tools/embed_assets.py, set by sdl_platform.c), and docs/icon-160.png (the
README's logo). Needs Pillow.

The logo is square, on a plain background, which android_icon.py uses as
the launcher icon's background layer. Larger icons are scaled up from it,
so a bigger logo makes sharper icons.
"""

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
LOGO = ROOT / "port/assets/logo.png"
ICNS_SIZES = [(16, 16), (32, 32), (64, 64), (128, 128), (256, 256), (512, 512), (1024, 1024)]
ICO_SIZES = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]


def main():
    logo = Image.open(LOGO).convert("RGB")
    if logo.width != logo.height:
        raise SystemExit(f"{LOGO} must be square, not {logo.width}x{logo.height}")

    def sized(size):
        return logo.resize((size, size), Image.Resampling.LANCZOS)

    sized(512).save(ROOT / "port/android/art/android-icon.png")
    sized(160).save(ROOT / "docs/icon-160.png")
    sized(1024).save(ROOT / "port/macos/AppIcon.icns", sizes=ICNS_SIZES)
    sized(256).save(ROOT / "port/windows/damnationce.ico", sizes=ICO_SIZES)
    (ROOT / "port/assets/icon").mkdir(exist_ok=True)
    sized(256).save(ROOT / "port/assets/icon/window-icon-256.png")
    print("android-icon.png, icon-160.png, AppIcon.icns, damnationce.ico, window-icon-256.png")


if __name__ == "__main__":
    main()
