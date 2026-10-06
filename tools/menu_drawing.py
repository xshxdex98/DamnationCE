"""Drawing helpers the menus' art tools share (shell_art.py, cairo_art.py,
shell_skin.py): smooth shapes in the units of the menus' 640x480 screen,
drawn SCALE times larger so that they stay sharp in a large window, and
colours laid through masks. Needs Pillow.
"""

from PIL import Image, ImageChops, ImageDraw

SCALE = 3
SMOOTH = 4              # shapes are drawn this many times larger, then shrunk


def new(size, scale=SCALE):
    """A clear picture size (units) large, scale pixels to a unit."""
    return Image.new("RGBA", (round(size[0] * scale), round(size[1] * scale)), (0, 0, 0, 0))


def ramp(size, start, end, across=False):
    """An image of size (pixels) whose values run from start to end, down it or across it."""
    image = Image.linear_gradient("L")
    if across:
        image = image.transpose(Image.Transpose.ROTATE_90)
    image = image.resize(size, Image.Resampling.BILINEAR)
    return image.point(lambda value: round(start + (end - start) * value / 255))


def polygon(size, points, outline=0.0, scale=SCALE, closed=True):
    """The polygon (in units of the 640x480 screen) as a mask of an image of
    size (units), scale pixels to a unit: filled, or its outline that many
    units wide (not closed: the line through the points alone)."""
    factor = scale * SMOOTH
    mask = Image.new("L", (round(size[0] * factor), round(size[1] * factor)), 0)
    scaled = [(x * factor, y * factor) for x, y in points]
    draw = ImageDraw.Draw(mask)
    if outline:
        line = scaled + scaled[:1] if closed else scaled
        draw.line(line, fill=255, width=max(1, round(outline * factor)), joint="curve")
    else:
        draw.polygon(scaled, fill=255)
    return mask.resize((round(size[0] * scale), round(size[1] * scale)), Image.Resampling.LANCZOS)


def box(left, top, right, bottom):
    return [(left, top), (right, top), (right, bottom), (left, bottom)]


def paint(image, color, mask, strength=None):
    """Lays the color over the image through the mask, and through strength
    (a ramp, or one value of 255) if given."""
    if isinstance(strength, int):
        mask = mask.point(lambda value: value * strength // 255)
    elif strength is not None:
        mask = ImageChops.multiply(mask, strength)
    image.alpha_composite(Image.merge("RGBA", [Image.new("L", image.size, part) for part in color] + [mask]))
