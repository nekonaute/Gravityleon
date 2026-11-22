# The engraving's clouds, from the pen drawings nuages-1.jpg and nuages-2.jpg: their
# ink cleaned, the inside filled with paper, engraver's hatching added in the shade (level strokes,
# swelling and breaking as a burin cuts them, closer and heavier low down, crossed in the deepest
# shade; the light from above left), the drawing's own strokes on top. RGBA PNGs.
# Needs numpy and Pillow. From the game's folder:
#   python3 tools/make_clouds.py            (writes assets/clouds/cloud-cumulus.png -- the one the game uses)
#   python3 tools/make_clouds.py --all DIR  (also the billowing one and the streaked one, taken out of the game)
import os, sys, math
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from collections import deque

# Each cloud, cut out of its photo: the photo, and the box round it (left, top, right, bottom,
# in the photo's pixels).
DRAWINGS = Path(__file__).resolve().parents[1] / 'workshop' / 'specs and corrections'
SOURCES = {
    'billow':  ('nuages-1.jpg', (1238, 57, 2764, 1900)),
    'streak':  ('nuages-2.jpg', (172, 201, 2822, 662)),
    'cumulus': ('nuages-2.jpg', (43, 806, 3801, 1800)),
}

def crop(name):
    filename, box = SOURCES[name]
    with Image.open(DRAWINGS / filename) as photo:
        return photo.crop(box)

# ---- the ink: lighting flattened, a soft alpha (0 paper .. 1 ink), specks dropped ----
from PIL import Image, ImageFilter
from collections import deque

def ink(im, width, cut=0.80, soft=0.22, speck=40):
    im = im.convert('L')
    bg = im.filter(ImageFilter.GaussianBlur(45))
    g = np.asarray(im, dtype=np.float32) / np.maximum(np.asarray(bg, dtype=np.float32), 1)
    h = int(round(im.height * width / im.width))
    g = np.asarray(Image.fromarray(np.clip(g * 255, 0, 255).astype(np.uint8)).resize((width, h), Image.LANCZOS), dtype=np.float32) / 255
    a = np.clip((cut - g) / soft, 0, 1)
    return drop_specks(a, speck)

def components(mask):
    H, W = mask.shape
    lab = np.zeros((H, W), np.int32)
    comps = []
    ys, xs = np.nonzero(mask)
    for y0, x0 in zip(ys, xs):
        if lab[y0, x0]: continue
        n = len(comps) + 1
        q = deque([(y0, x0)]); lab[y0, x0] = n; pts = []
        while q:
            y, x = q.popleft(); pts.append((y, x))
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < H and 0 <= xx < W and mask[yy, xx] and not lab[yy, xx]:
                        lab[yy, xx] = n; q.append((yy, xx))
        comps.append(np.array(pts))
    return lab, comps

def drop_specks(a, speck):
    lab, comps = components(a > 0.5)
    keep = np.zeros(len(comps) + 1, bool)
    for i, c in enumerate(comps): keep[i + 1] = len(c) >= speck
    near = keep[lab]                                     # (soft edges kept next to kept strokes)
    grow = np.asarray(Image.fromarray((near * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(5)), np.float32) / 255
    return a * grow

# ---- the clouds ----

PAPER = np.array([244, 240, 231], np.float32) / 255
INK = np.array([19, 17, 16], np.float32) / 255

def smooth(v, k):
    k = max(1, int(k)) | 1
    pad = np.pad(v, k // 2, mode='edge')
    return np.convolve(pad, np.ones(k) / k, mode='valid')

def fill_mask(a, close=9, sm=15):
    """Inside the cloud: each column filled from its topmost ink to its lowest, smoothed."""
    H, W = a.shape
    b = a > 0.5
    top = np.full(W, np.nan); bot = np.full(W, np.nan)
    for x in range(W):
        ys = np.nonzero(b[:, x])[0]
        if len(ys): top[x], bot[x] = ys[0], ys[-1]
    xs = np.arange(W); ok = ~np.isnan(top)
    if ok.sum() < 2: return np.zeros_like(a)
    top = np.interp(xs, xs[ok], top[ok]); bot = np.interp(xs, xs[ok], bot[ok])
    x0, x1 = xs[ok].min(), xs[ok].max()
    top, bot = smooth(top, sm), smooth(bot, sm)
    yy = np.arange(H)[:, None]
    m = ((yy >= top[None, :]) & (yy <= bot[None, :]) & (xs[None, :] >= x0) & (xs[None, :] <= x1)).astype(np.float32)
    im = Image.fromarray((m * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2))
    return np.asarray(im, np.float32) / 255, top, bot

def hatch(m, top, bot, spacing=10.0, heavy=4.6, seed=1):
    """Level strokes in the shade: from a third of the way down each column, heavier towards its
    bottom and its right (the light from above left); crossed where it is darkest; broken here and
    there, swelling along their length."""
    H, W = m.shape
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    v = np.clip((yy - top[None, :]) / np.maximum(bot - top, 1)[None, :], 0, 1)
    shade = np.clip((v - 0.3) / 0.7, 0, 1) ** 1.2 * (0.7 + 0.3 * xx / W)
    wave = 1.3 * np.sin(xx * 0.021 + yy * 0.004) + 0.7 * np.sin(xx * 0.057 + 2.1)
    u = (yy - wave) / spacing
    k = np.floor(u)
    d = np.abs(u - k - 0.5) * spacing
    swell = 0.65 + 0.35 * np.sin(xx * 0.03 + k * 1.7)
    breaks = np.sin(xx * 0.011 + k * 4.1) + 0.6 * np.sin(xx * 0.047 + k * 2.3)
    th = heavy * shade * swell * (breaks > -0.9)
    lines = np.clip(th / 2 - d + 0.5, 0, 1) * (th > 0.3)
    # crossed, at a slant, in the deepest shade
    u2 = (yy * math.cos(0.35) + xx * math.sin(0.35)) / (spacing * 1.3)
    d2 = np.abs(u2 - np.floor(u2) - 0.5) * spacing * 1.3
    th2 = 3.0 * np.clip((shade - 0.62) / 0.38, 0, 1)
    lines = np.maximum(lines, np.clip(th2 / 2 - d2 + 0.5, 0, 1) * (th2 > 0.3))
    edge = np.asarray(Image.fromarray((m * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(5)), np.float32) / 255
    return lines * edge

def bold(a, grow=5):
    """The pen's lines made a burin's: wider, to stay lines once the cloud is drawn small."""
    im = Image.fromarray((a * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(grow)).filter(ImageFilter.GaussianBlur(0.8))
    return np.asarray(im, np.float32) / 255

def compose(a, m, h, grow=5):
    a = bold(a, grow)
    inkA = np.maximum(a, h)
    alpha = np.maximum(m, a)
    rgb = PAPER[None, None, :] * (1 - inkA[..., None]) + INK[None, None, :] * inkA[..., None]
    out = np.dstack([rgb, alpha[..., None]])
    return Image.fromarray((np.clip(out, 0, 1) * 255).astype(np.uint8), 'RGBA')

def stroke(draw, pts, w):
    draw.line(pts, fill=255, width=w, joint='curve')
    r = w / 2
    for x, y in (pts[0], pts[-1]): draw.ellipse((x - r, y - r, x + r, y + r), fill=255)

def billow_side(a, xcut, bulge, bumps=3, w=4):
    """The billowing cloud's straight right side (it stood against the paper's edge) taken off, and
    its right closed with billows as its left has them: from its top contour to its bottom one."""
    H, W = a.shape
    a = a.copy(); a[:, xcut:] = 0
    b = a > 0.5
    col = np.nonzero(b[:, xcut - 1])[0]
    y0, y1 = col.min(), col.max()
    big = 4
    im = Image.new('L', (W * big, H * big), 0)
    dr = ImageDraw.Draw(im)
    pts = []
    for i in range(121):
        t = i / 120
        r = bulge * math.sin(math.pi * t) * (0.75 + 0.25 * abs(math.sin(bumps * math.pi * t)))
        pts.append(((xcut - 3 + r) * big, (y0 + (y1 - y0) * t) * big))
    stroke(dr, pts, w * big)
    add = np.asarray(im.resize((W, H), Image.LANCZOS), np.float32) / 255
    return np.maximum(a, add)

def main(out, all_three=False):
    os.makedirs(out, exist_ok=True)
    if all_three:
        others(out)
    # the cumulus (nuages-2, middle), without the sun's rays on its right
    a = ink(crop('cumulus'), 1300)
    lab, comps = components(a > 0.5)
    H, W = a.shape
    gone = np.zeros_like(a)
    for i, c in enumerate(comps):
        if c[:, 1].min() > 0.9 * W: gone[lab == i + 1] = 1
    gone = np.asarray(Image.fromarray((gone * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(9)), np.float32) / 255
    a = a * (1 - gone)                                  # (the rays and their soft edges)
    m, top, bot = fill_mask(a)
    compose(a, m, hatch(m, top, bot, seed=3)).save(f'{out}/cloud-cumulus.png')

def others(out):
    # the billowing cloud (nuages-1)
    a = ink(crop('billow'), 800)
    a = billow_side(a, 655, 110)
    m, top, bot = fill_mask(a)
    compose(a, m, hatch(m, top, bot, seed=1)).save(f'{out}/cloud-billow.png')
    # the long streaked cloud (nuages-2, top): its own lines are its shading -- a little hatching only
    a = ink(crop('streak'), 1100)
    m, top, bot = fill_mask(a, sm=9)
    compose(a, m, hatch(m, top, bot, spacing=9.0, heavy=2.4, seed=2), grow=3).save(f'{out}/cloud-streak.png')

if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if a != '--all']
    main(args[0] if args else 'assets/clouds', '--all' in sys.argv)
