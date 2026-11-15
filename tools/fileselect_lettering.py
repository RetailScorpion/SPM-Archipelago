"""The file select's Options button lettering, ready for the patcher.

The button itself is built by the patcher from the player's own disc: it takes the game's Back button
(filesel.tpl picture 7), clears its text and draws this lettering on it (patcher.build_filesel_button).
So only the lettering ships: RetailScorpion's "Options" (assets/fileselect/options_text.png, Gamtex
font by FG Studios), spaced out, shrunk to 22 pixels high, with a shadow, placed where it goes on the
148x40 button.

Writes apworld/spm/bin/fileselect_options_lettering.bin: 148x40 pixels, 4 bytes each (red, green, blue,
alpha), row by row.

Usage: py -3 -E tools/fileselect_lettering.py [preview.png]
"""

import os
import sys
from collections import deque

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEXT = os.path.join(ROOT, "assets", "fileselect", "options_text.png")
OUT = os.path.join(ROOT, "apworld", "spm", "bin", "fileselect_options_lettering.bin")

BUTTON_SIZE = (148, 40)
TEXT_HEIGHT = 22       # pixels, on the button
LETTER_GAP = 4         # extra pixels between letters, on the button
SHADOW = (90, 8, 99)   # the shadow's colour, 2 pixels right and down
TOP = 7


def glyphs(text: Image.Image) -> list:
    """The lettering's letters, left to right: each a list of its pixels. A letter is a connected shape,
    joined with any shape over it (the i and its dot)."""
    alpha = text.getchannel("A").load()
    width, height = text.size
    label = [[-1] * width for _ in range(height)]
    shapes = []
    for y in range(height):
        for x in range(width):
            if alpha[x, y] > 20 and label[y][x] < 0:
                queue, points = deque([(x, y)]), []
                label[y][x] = len(shapes)
                while queue:
                    cx, cy = queue.popleft()
                    points.append((cx, cy))
                    for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                        if 0 <= nx < width and 0 <= ny < height and alpha[nx, ny] > 20 and label[ny][nx] < 0:
                            label[ny][nx] = len(shapes)
                            queue.append((nx, ny))
                shapes.append(points)
    shapes = [shape for shape in shapes if len(shape) > 15]
    boxes = [(min(p[0] for p in shape), max(p[0] for p in shape)) for shape in shapes]
    letters = []
    for i in sorted(range(len(shapes)), key=lambda i: boxes[i][0]):
        x0, x1 = boxes[i]
        if letters:
            last = letters[-1]
            lx0, lx1 = last["box"]
            if min(x1, lx1) - max(x0, lx0) > 0.5 * min(x1 - x0, lx1 - lx0):
                last["points"] += shapes[i]
                last["box"] = (min(x0, lx0), max(x1, lx1))
                continue
        letters.append({"points": list(shapes[i]), "box": (x0, x1)})
    return letters


def build() -> Image.Image:
    """The lettering on a transparent 148x40 picture, where it goes on the button."""
    text = Image.open(TEXT).convert("RGBA")
    width, height = text.size
    alpha, src = text.getchannel("A").load(), text.load()
    letters = glyphs(text)
    scale = TEXT_HEIGHT / height
    gap = round(LETTER_GAP / scale)
    spaced = Image.new("RGBA", (width + gap * (len(letters) - 1) + 4, height), (255, 255, 255, 0))
    out = spaced.load()
    owner = {}
    for k, letter in enumerate(letters):
        for x, y in letter["points"]:
            out[x + gap * k, y] = src[x, y]
            owner[(x, y)] = k
    # Soft edge pixels belong to no shape: each goes with the nearest letter by column
    for y in range(height):
        for x in range(width):
            if alpha[x, y] and (x, y) not in owner:
                k = min(range(len(letters)), key=lambda k: 0 if letters[k]["box"][0] <= x <= letters[k]["box"][1]
                        else min(abs(x - letters[k]["box"][0]), abs(x - letters[k]["box"][1])))
                out[x + gap * k, y] = src[x, y]
    spaced = spaced.crop(spaced.getchannel("A").getbbox())
    w = round(spaced.width * scale)
    small = spaced.resize((w, TEXT_HEIGHT), Image.LANCZOS)
    shadow = Image.new("RGBA", small.size, SHADOW + (255,))
    shadow.putalpha(small.getchannel("A"))
    layer = Image.new("RGBA", BUTTON_SIZE, (0, 0, 0, 0))
    x0 = (BUTTON_SIZE[0] - w) // 2 - 2
    layer.alpha_composite(shadow, (x0 + 2, TOP + 2))
    layer.alpha_composite(small, (x0, TOP))
    return layer


def main():
    """Writes the lettering for the patcher (and a preview picture if a path is given)."""
    layer = build()
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, "wb").write(layer.tobytes())
    print(f"Wrote {OUT}")
    if len(sys.argv) > 1:
        layer.save(sys.argv[1])


if __name__ == "__main__":
    main()
