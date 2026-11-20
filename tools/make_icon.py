# The app's icon artwork, rendered by the game itself at 1024 x 1024 -- after
# workshop/gravityleon_icon.png (180 x 180): Leon in GOD's gold, his hat's bands the red key's, standing on
# a pillar one tile wide, on the paper of easy mode, framed as that picture is. In four tiles of 512 x 512, each
# from the middle of an ordinary 1280 x 720 test window, the camera moved between them (and held still meanwhile),
# then put together: the game's own lines at full size, nothing enlarged -- and set in a rounded square, as
# macOS's app icons are, transparent round it.
# From the game's folder, after `make`:
#   python3 tools/make_icon.py                 (renders tools/icon.png and exports tools/icon-macos.png)
#   python3 tools/make_icon.py --from-artwork  (only exports the rounded icon from the saved artwork)
import argparse, os, shutil, subprocess, glob
import io
import struct
from PIL import Image, ImageChops, ImageDraw

ROOT = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
WORK = os.path.join(ROOT, '.scratch', 'icon-build')
ARTWORK = os.path.join(ROOT, 'tools', 'icon.png')
MAC_ICON = os.path.join(ROOT, 'tools', 'icon-macos.png')
SIZE, TILE = 1024, 512
SCREEN_H = 720          # (the test window's height)
SCALE = 12.164          # screen pixels a world pixel: his body, ink and all (39 world pixels), 0.485 of the icon wide, as in the picture
PLAYER = SCALE * 40 / SCREEN_H   # player_screen_height giving that scale (the game: screen height x player_screen_height / 40)
FEET_BELOW = 30.6       # world pixels from the icon's middle down to his feet: his hat's top 7 % of the way down, as in the picture
LEFT = 0.5              # ...and from his middle to the icon's: the picture's Leon a hair right of its middle

# A map: a pillar one tile wide, from the bottom up to his feet, Leon ('P') on it.
W, H, X, TOP = 21, 24, 10, 12
rows = [[' '] * W for _ in range(H)]
for y in range(TOP, H): rows[y][X] = '#'
for x in range(W): rows[H - 1][x] = '#'
rows[TOP - 1][X] = 'P'
rows[2][1] = rows[2][2] = '#'      # (the map's highest ground, far out of sight: Anna's place, the ending's -- not his pillar)
FEET = (X + 0.5) * 40, TOP * 40

def render_artwork():
    if os.path.exists(WORK): shutil.rmtree(WORK)
    os.makedirs(os.path.join(WORK, 'data'))
    shutil.copy(os.path.join(ROOT, 'gravityleon'), WORK)
    for d in ('levels', 'assets'): shutil.copytree(os.path.join(ROOT, d), os.path.join(WORK, d))
    with open(os.path.join(WORK, 'icon map.txt'), 'w') as f: f.write('\n'.join(''.join(r) for r in rows) + '\n')
    params = open(os.path.join(ROOT, 'params.txt')).read().split('\n')
    over = {'screen_ratio': 'off', 'player_screen_height': f'{PLAYER:.6f}', 'view_zoom': '1.0', 'look': 'sketch',
            'background_pattern_easy': 'asanoha', 'background_pattern_parallax': '1.0',   # (the pattern on the world: the tiles meet)
            'camera_zone_area': '16', 'camera_delay_x': '1000', 'camera_delay_y': '1000',      # (the camera held where it is put)
            'start_level': 'EASY MODE', 'fullscreen': 'False'}
    out = []
    for line in params:
        key = line.split('=')[0].strip()
        out.append(f'{key} = {over.pop(key)}' if key in over and '=' in line else line)
    out += [f'{k} = {v}' for k, v in over.items()]
    open(os.path.join(WORK, 'params.txt'), 'w').write('\n'.join(out))
    # The moment: GOD on (gold, his hat too), the red key the last one taken (red bands), his hat on, facing
    # right, the camera on the tile's middle -- a bug report's state, read back as one.
    icon = Image.new('RGB', (SIZE, SIZE))
    cx, cy = FEET[0] - LEFT, FEET[1] - FEET_BELOW  # (the icon's middle, in the world)
    for k, (tx, ty) in enumerate(((0, 0), (1, 0), (0, 1), (1, 1))):
        off = (TILE / 2) / SCALE
        cam = (cx + (off if tx else -off), cy + (off if ty else -off))
        state = ['=== GRAVITY LEON REPORT DATA (tools/make_icon.py) ===', '', '[state]', 'level.file: icon map.txt', 'leon.god: 1',
                 'key.last: 0', 'leon.hat: 1 -1.000', 'hat.taken: 1', 'leon.facing: 1', f'camera: {cam[0]:.3f} {cam[1]:.3f}',
                 '', '[params.txt]'] + out + ['[end params.txt]']
        open(os.path.join(WORK, 'icon state.txt'), 'w').write('\n'.join(state) + '\n')
        subprocess.run(['./gravityleon', '--from-report', 'icon state.txt', '--smoke-name', f'tile{k}', '--smoke-time', '0.05',
                        '--smoke-run', '0', '--smoke-jumps', '0', '--no-hud', '--seed', '1', '--audio-muted',
                        '--reports-dir', os.path.join(WORK, 'Reports')], cwd=WORK, check=True, capture_output=True)
        shot = Image.open(sorted(glob.glob(os.path.join(WORK, 'screenshots', f'*_tile{k}.png')))[-1]).convert('RGB')
        assert shot.size[1] == SCREEN_H, shot.size
        mx, my = shot.width // 2, shot.height // 2
        icon.paste(shot.crop((mx - TILE // 2, my - TILE // 2, mx + TILE // 2, my + TILE // 2)), (tx * TILE, ty * TILE))
    icon.save(ARTWORK)

def export_mac_icon():
    # As macOS's app icons: the picture in a rounded square -- Apple's grid: 824 of 1024, its corners 185
    # round -- centred, the rest transparent (what is behind it shows).
    BOX, R = 824, 185
    with Image.open(ARTWORK) as icon:
        art = icon.convert('RGBA').resize((BOX, BOX), Image.LANCZOS)
    mask = Image.new('L', (BOX * 4, BOX * 4), 0)                      # (drawn 4 times as big, then shrunk: smooth edges)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, BOX * 4 - 1, BOX * 4 - 1), radius=R * 4, fill=255)
    mask = mask.resize((BOX, BOX), Image.LANCZOS)
    out = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    art.putalpha(ImageChops.multiply(art.getchannel('A'), mask))
    out.paste(art, ((SIZE - BOX) // 2, (SIZE - BOX) // 2))
    out.save(MAC_ICON)

def icns_entry(tag, data):
    return struct.pack('>4sI', tag, 8 + len(data)) + data

def export_icns(filename):
    entries = bytearray()
    with Image.open(MAC_ICON) as icon:
        # The two small 1x icons use separate RGB planes and an 8-bit alpha mask.
        for size, rgb_tag, mask_tag in ((16, b'is32', b's8mk'), (32, b'il32', b'l8mk')):
            small = icon.resize((size, size), Image.LANCZOS)
            rgb = bytearray()
            for channel in 'RGB':
                plane = small.getchannel(channel).tobytes()
                for offset in range(0, len(plane), 128):
                    block = plane[offset:offset + 128]
                    rgb.append(len(block) - 1)  # ICNS literal run, up to 128 bytes
                    rgb.extend(block)
            entries.extend(icns_entry(rgb_tag, rgb))
            entries.extend(icns_entry(mask_tag, small.getchannel('A').tobytes()))
        # The larger 1x icons and all Retina icons accept PNG data directly.
        for tag, size in ((b'ic07', 128), (b'ic08', 256), (b'ic09', 512),
                          (b'ic11', 32), (b'ic12', 64), (b'ic13', 256), (b'ic14', 512), (b'ic10', 1024)):
            png = io.BytesIO()
            icon.resize((size, size), Image.LANCZOS).save(png, format='PNG')
            entries.extend(icns_entry(tag, png.getvalue()))
    with open(filename, 'wb') as bundle:
        bundle.write(icns_entry(b'icns', entries))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Build the Gravity Leon macOS icon.')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--from-artwork', action='store_true', help='use tools/icon.png without running the game')
    mode.add_argument('--icns', metavar='FILE', help='package tools/icon-macos.png as a macOS icon file')
    args = parser.parse_args()
    if args.icns:
        export_icns(args.icns)
    else:
        if not args.from_artwork:
            render_artwork()
        export_mac_icon()
