"""Builds the Archipelago item icons in the style of SPM's wicon.tpl item icons (thick black outline, light shading).

Outputs (in <out dir>):
  ap_progression.png / ap_filler.png          40x40, the size of the game's item icons
  ap_progression_160.png / ap_filler_160.png  4x masters for previews and the tracker
  ap_icons_preview.png                         both icons next to real game icons, 3x zoom

Usage: py -3 -E make_ap_icons.py <logo png> <wicon.tpl> <out dir>
"""
import os, sys
from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter, ImageOps

sys.path.insert(0, os.path.dirname(__file__))
from tpl import images, decode_rgb5a3, encode_rgb5a3

LOGO, WICON, OUT = sys.argv[1:4]
SIZE = 40          # in-game icon size
WORK = 320         # size the icon is built at before downscaling
OUTLINE = 13       # outline thickness at WORK size (~1.6px at 40px, like the game's icons)
MARGIN = 6         # transparent border at WORK size so the outline isn't clipped

def fit_logo(path):
    """The logo centred on a square transparent canvas, leaving room for the outline."""
    logo = Image.open(path).convert('RGBA')
    logo = logo.crop(logo.getbbox())
    inner = WORK - 2 * (OUTLINE + MARGIN)
    scale = inner / max(logo.size)
    logo = logo.resize((round(logo.width * scale), round(logo.height * scale)), Image.LANCZOS)
    canvas = Image.new('RGBA', (WORK, WORK))
    canvas.alpha_composite(logo, ((WORK - logo.width) // 2, (WORK - logo.height) // 2))
    return canvas

def shade(im):
    """Brighter, more saturated colours plus top-left light, to sit with the game's vivid icons."""
    rgb = ImageEnhance.Color(im.convert('RGB')).enhance(1.45)
    rgb = ImageEnhance.Contrast(rgb).enhance(1.1)
    # diagonal light: full brightness at the top-left, 80% at the bottom-right
    grad = Image.linear_gradient('L').rotate(45, expand=True)
    grad = grad.crop(((grad.width - 256) // 2,) * 2 + ((grad.width + 256) // 2,) * 2).resize((WORK, WORK))
    light = grad.point(lambda v: 255 - v * 51 // 255)
    rgb = ImageChops.multiply(ImageEnhance.Brightness(rgb).enhance(1.1), Image.merge('RGB', [light] * 3))
    out = rgb.convert('RGBA')
    out.putalpha(im.getchannel('A'))
    return out

def outline(im):
    """Black outline: the alpha mask grown by OUTLINE px, behind the art. Also fills the gaps between the circles."""
    a = im.getchannel('A').point(lambda v: 255 if v > 96 else 0)
    grown = a.filter(ImageFilter.MaxFilter(OUTLINE * 2 + 1))
    # fill enclosed gaps (the middle of the logo): anything the flood fill from the corner can't reach
    outside = grown.copy()
    ImageDraw.floodfill(outside, (0, 0), 128)
    grown = outside.point(lambda v: 0 if v == 128 else 255)
    grown = grown.filter(ImageFilter.GaussianBlur(1.5))
    base = Image.new('RGBA', im.size, (0, 0, 0, 0))
    base.putalpha(grown)
    base.alpha_composite(im)
    return base

def gray(im):
    """Filler version: the same icon desaturated, kept a little lighter so it still reads on dark backgrounds."""
    g = ImageOps.grayscale(im.convert('RGB'))
    g = ImageEnhance.Contrast(g).enhance(0.9)
    out = Image.merge('RGB', [g] * 3).convert('RGBA')
    out.putalpha(im.getchannel('A'))
    return out

def downscale(im, size):
    small = im.resize((size, size), Image.LANCZOS)
    # RGB5A3 only has 8 alpha levels; snap nearly-opaque edges to fully opaque so they get the 5-bit colour path
    a = small.getchannel('A').point(lambda v: 255 if v >= 0xE0 else (0 if v < 0x10 else v))
    small.putalpha(a)
    return small

def roundtrip(im):
    """What the icon will actually look like after RGB5A3 encoding."""
    data = encode_rgb5a3(im)
    return decode_rgb5a3(data, 0, im.width, im.height)

os.makedirs(OUT, exist_ok=True)
colour = outline(shade(fit_logo(LOGO)))
filler = outline(gray(shade(fit_logo(LOGO))))

icons = {}
for name, im in (('ap_progression', colour), ('ap_filler', filler)):
    small = roundtrip(downscale(im, SIZE))
    small.save(os.path.join(OUT, name + '.png'))
    im.resize((160, 160), Image.LANCZOS).save(os.path.join(OUT, name + '_160.png'))
    icons[name] = small

# preview: our two icons, then some real item icons, on the dark and light backgrounds the game uses
data = open(WICON, 'rb').read()
real = [decode_rgb5a3(data, off, w, h) for i, w, h, f, off in images(data)
        if f == 5 and (w, h) == (40, 40)][40:48]
row = [icons['ap_progression'], icons['ap_filler']] + real
sheet = Image.new('RGBA', (len(row) * 48, 96))
for bg, y in (((60, 60, 60, 255), 0), ((235, 225, 200, 255), 48)):
    sheet.paste(Image.new('RGBA', (sheet.width, 48), bg), (0, y))
    for k, im in enumerate(row):
        sheet.alpha_composite(im, (k * 48 + 4, y + 4))
sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save(os.path.join(OUT, 'ap_icons_preview.png'))
print('wrote', OUT)
