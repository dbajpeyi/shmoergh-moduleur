# Composites "HOST L" / "HOST R" onto the 3x stage render using letters cut
# from the panel's own labels (grey print on flat panel -> exact alpha mask).
import sys
import numpy as np
from PIL import Image

# Usage: python3 host-labels.py <stage-raw.png> <out.png> [dark|light]
src, dst = sys.argv[1], sys.argv[2]
mode = sys.argv[3] if len(sys.argv) > 3 else 'dark'
K = 3.0                                      # render scale (px per pt)
img = np.asarray(Image.open(src).convert('RGB')).astype(np.float32)
rgb = lambda h: np.array([int(h[i:i + 2], 16) for i in (1, 3, 5)], np.float32)
if mode == 'light':
    BG, INK = rgb('#EEEEEC'), rgb('#13192B')           # letters are cut from dark-on-white labels
    DST_BG, DST_INK = rgb('#0B1A36'), rgb('#F4F4F2')   # set white on the navy strip
else:
    BG, INK = rgb('#252527'), rgb('#9A9A9C')
    DST_BG, DST_INK = BG, INK                          # knockout on the hatching

# letter -> (glyph x, baseline y, advance) taken from labels on plain panel areas
letters = {
    'H': (875.169922, 687.0, 7.872),   # HPF IN
    'O': (231.5, 1455.0, 8.076),       # ON/OFF
    'S': (164.4, 1071.0, 7.3),         # SUSTAIN
    'T': (611.1, 1071.0, 7.0),         # TRIGGER
    'L': (113.7, 1071.0, 6.6),         # RELEASE
    'R': (99.4, 1071.0, 7.6),          # RELEASE
}
ASC, DESC = 11.0, 2.0

def alpha_of(ch):
    x, base, adv = letters[ch]
    x0, x1 = int(round(x * K)), int(round((x + adv) * K))
    y0, y1 = int(round((base - ASC) * K)), int(round((base + DESC) * K))
    patch = img[y0:y1, x0:x1]
    a = ((patch - BG) / (INK - BG)).mean(axis=2)
    return np.clip(a, 0.0, 1.0)

out = img.copy()
def put(text, centre_x, base):
    space = 3.06
    width = sum(space if c == ' ' else letters[c][2] for c in text)
    x = centre_x - width / 2
    # plain knockout behind labels on hatching: 2 pt either side, baseline-14 .. +5
    kx0, kx1 = int(round((x - 2) * K)), int(round((x + width + 2) * K))
    ky0, ky1 = int(round((base - 14) * K)), int(round((base + 5) * K))
    out[ky0:ky1, kx0:kx1] = DST_BG
    for c in text:
        if c == ' ':
            x += space
            continue
        a = alpha_of(c)
        x0, y0 = int(round(x * K)), int(round((base - ASC) * K))
        h, w = a.shape
        region = out[y0:y0 + h, x0:x0 + w]
        region[:] = region * (1 - a[..., None]) + DST_INK * a[..., None]
        x += letters[c][2]

put('HOST L', 380.0, 1455.0)
put('HOST R', 444.0, 1455.0)
Image.fromarray(np.clip(out + 0.5, 0, 255).astype(np.uint8)).save(dst, optimize=True)
