#!/usr/bin/env python3
"""Draws DamnationCE's icon, in the menus' Glassed look: a dark glass planet
with a ring of glass around it, lit from above, on a dark ground.

    python tools/app_icon.py

Writes port/android/art/android-icon.png (tools/android_icon.py makes the
launcher icon from it), port/macos/AppIcon.icns and docs/icon-160.png.
Needs Pillow.
"""

import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
SIZE = 1024
SMOOTH = 2                  # drawn this many times larger, then shrunk

GROUND = (10, 14, 20)       # solid: tools/android_icon.py takes it as the background layer
WHITE = (255, 255, 255)
GLASS_TINT = (150, 190, 230)

PLANET_RADIUS = 0.25        # of the icon's size
RING = (0.43, 0.15)         # the ring's half width and half height, of the icon's size
RING_THICKNESS = 0.045
RING_TILT = -18             # degrees


def blank(size):
    return Image.new("L", (size, size), 0)


def paint(image, color, mask, strength=255):
    if strength != 255:
        mask = mask.point(lambda value: value * strength // 255)
    image.alpha_composite(Image.merge("RGBA", [Image.new("L", image.size, part) for part in color] + [mask]))


def vertical_ramp(size, top, bottom):
    ramp = Image.linear_gradient("L").resize((size, size))
    return ramp.point(lambda value: round(top + (bottom - top) * value / 255))


def ring_band(size):
    """The ring's band, and the half of the icon above its tilted middle line,
    where it passes behind the planet."""
    center = size / 2
    band = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(band)
    width, height, thickness = RING[0] * size, RING[1] * size, RING_THICKNESS * size
    draw.ellipse([center - width, center - height, center + width, center + height], fill=255)
    draw.ellipse([center - width + thickness, center - height + thickness * 0.55,
                  center + width - thickness, center + height - thickness * 0.55], fill=0)
    band = band.rotate(RING_TILT, resample=Image.Resampling.BICUBIC, center=(center, center))
    behind = Image.new("L", (size, size), 0)
    ImageDraw.Draw(behind).rectangle([0, 0, size, center], fill=255)
    behind = behind.rotate(RING_TILT, resample=Image.Resampling.BICUBIC, center=(center, center))
    return band, behind


def glass_layer(mask, size, tint_strength, light, edge_strength):
    """Glass over the mask, as a layer: a cool tint, lit from above (light:
    the top's and the bottom's strength), a bright edge and a faint glow."""
    layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    paint(layer, WHITE, mask.filter(ImageFilter.GaussianBlur(size * 0.012)), 40)
    paint(layer, GLASS_TINT, mask, tint_strength)
    paint(layer, WHITE, ImageChops.multiply(mask, vertical_ramp(size, *light)))
    edge = ImageChops.subtract(mask.filter(ImageFilter.MaxFilter(5)), mask.filter(ImageFilter.MinFilter(5)))
    paint(layer, WHITE, ImageChops.multiply(edge, vertical_ramp(size, 255, 60)), edge_strength)
    return layer


def clipped(layer, mask):
    """The layer where the mask is."""
    alpha = ImageChops.multiply(layer.getchannel("A"), mask)
    result = layer.copy()
    result.putalpha(alpha)
    return result


def draw_icon(size):
    big = size * SMOOTH
    image = Image.new("RGBA", (big, big), GROUND + (255,))
    center = big / 2
    radius = PLANET_RADIUS * big
    band, behind = ring_band(big)
    ring = glass_layer(band, big, 70, (150, 30), 220)

    planet = blank(big)
    ImageDraw.Draw(planet).ellipse([center - radius, center - radius, center + radius, center + radius], fill=255)
    # (where the ring passes behind the planet, the planet hides it)
    image.alpha_composite(clipped(ring, ImageChops.subtract(behind, planet)))
    paint(image, (16, 24, 36), planet)
    image.alpha_composite(glass_layer(planet, big, 30, (95, 0), 210))
    image.alpha_composite(clipped(ring, ImageChops.invert(behind)))

    # a glint where the light catches the planet's upper left rim
    glint = blank(big)
    angle = math.radians(135)
    x, y = center + radius * 0.93 * math.cos(angle), center - radius * 0.93 * math.sin(angle)
    ImageDraw.Draw(glint).ellipse([x - big * 0.018, y - big * 0.018, x + big * 0.018, y + big * 0.018], fill=255)
    paint(image, WHITE, glint.filter(ImageFilter.GaussianBlur(big * 0.022)))
    paint(image, WHITE, glint.filter(ImageFilter.GaussianBlur(big * 0.004)), 230)

    return image.resize((size, size), Image.Resampling.LANCZOS).convert("RGB")


def main():
    icon = draw_icon(SIZE)
    icon.resize((512, 512), Image.Resampling.LANCZOS).save(ROOT / "port/android/art/android-icon.png")
    icon.resize((160, 160), Image.Resampling.LANCZOS).save(ROOT / "docs/icon-160.png")
    icon.save(ROOT / "port/macos/AppIcon.icns", sizes=[(16, 16), (32, 32), (64, 64), (128, 128), (256, 256),
                                                      (512, 512), (1024, 1024)])
    print("android-icon.png, icon-160.png, AppIcon.icns")


if __name__ == "__main__":
    main()
