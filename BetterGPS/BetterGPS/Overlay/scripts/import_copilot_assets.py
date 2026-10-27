"""Import the supplied NavPlus HD artwork and all frames of its 30 fps intro.

No runtime video thread: a bounded, deduplicated BC1 timeline is sampled by
each device's existing power clock. Original DDS files are copied byte-exact.
"""
from pathlib import Path
from PIL import Image
import argparse, hashlib, io, json, re, shutil, struct, subprocess

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('--ffmpeg', default='ffmpeg')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
target = root/'BetterGPS_assets/material/copilot/t680'
target.mkdir(parents=True, exist_ok=True)
files = ['navplushd_bottom_5by3.dds', 'navplushd_copilot_nav_5by3.dds',
         'navplushd_copilot_notrip_5by3.dds', 'navplushd_homescreen_5by3.dds']
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
manifest = {'source': 'User supplied NavPlus HD artwork / CoPilot V9 references', 'files': {}}
for name in files:
    source = args.source/name
    with Image.open(source) as image:
        assert image.size == (1324, 800), (name, image.size)
    shutil.copy2(source, target/name)
    manifest['files'][name] = digest(source)
video = args.source/'navplushd_kenworthintro_5by3.mp4'
manifest['video_sha256'] = digest(video)
proc = subprocess.Popen([args.ffmpeg, '-v', 'error', '-i', str(video), '-map', '0:v:0',
    '-vf', 'scale=900:544:flags=lanczos', '-fps_mode', 'passthrough',
    '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], stdout=subprocess.PIPE)
timeline, blocks, stored = [], [], {}
while True:
    frame = proc.stdout.read(900*544*3)
    if not frame: break
    assert len(frame) == 900*544*3, 'Incomplete decoded video frame'
    image = Image.frombytes('RGB', (900, 544), frame)
    padded = Image.new('RGB', (904, 544))
    padded.paste(image, (4, 0))
    padded.paste(image.crop((0, 0, 1, 544)).resize((4, 544)), (0, 0))
    buffer = io.BytesIO()
    padded.save(buffer, format='DDS', pixel_format='DXT1')
    data = buffer.getvalue()[128:]
    assert len(data) == 904*544//2
    key = hashlib.sha256(data).digest()
    if key not in stored:
        stored[key] = len(blocks)
        blocks.append(data)
    timeline.append(stored[key])
assert proc.wait() == 0
assert len(timeline) == 98, f'Expected updated 98-frame, 30fps intro, got {len(timeline)}'
encoded = struct.pack('<5I', 0x32534742, 904, 544, len(timeline), len(blocks))
encoded += struct.pack('<'+'H'*len(timeline), *timeline) + b''.join(blocks)
output = target/'navplushd_kenworthintro_5by3.bga'
output.write_bytes(encoded)
manifest.update(fps=30, frames=len(timeline), unique_frames=len(blocks),
                duration_ms=3267, boot_duration_ms=4517, bga_sha256=digest(output))
(target/'import-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')

# SCS packed colors are AABBGGRR. Keep all reflected field names and the
# native map-area index order, including ATS 1.61 additions.
def color(rgb, alpha=255):
    r,g,b = bytes.fromhex(rgb)
    return f'0x{alpha:02X}{b:02X}{g:02X}{r:02X}'
base = (root/'BetterGPS_assets/native_themes/light/colour_data.sui').read_text()
for dark in (False, True):
    road, obstacle, grass = ('45484A','393C3E','283B2B') if dark else ('FFFFFF','D9D5CD','D6E4B7')
    route, outline = ('60C53D','29551F') if dark else ('67BC38','437F21')
    palette = {
        'road_color': road, 'road_discovered_color': road,
        'prefab_color': road, 'prefab_discovered_color': road,
        'prefab_obstacle_color': obstacle, 'prefab_obstacle_discovered_color': obstacle,
        'prefab_grass_color': grass, 'prefab_grass_discovered_color': grass,
        'outline_color': outline, 'navigation_color': route,
        'navigation_current_color': route, 'navigation_highlight_color': route,
        'navigation_fade_color': route, 'navigation_arrow_color': route,
    }
    def field(match):
        key = match[1].strip()
        return f'{match[1]}: {color(palette[key])}' if key in palette else match[0]
    text = re.sub(r'(?m)^(\s*\w+):\s*0x[0-9a-fA-F]+', field, base)
    # Area index semantics come from the mounted map_data, not a flattened image.
    def area(match):
        name = match[1]
        rgb = route if name.startswith('Nav-') else ('B75821' if dark else 'F2B429') if name == 'Road' else grass if name == 'Green' else obstacle if name == 'Dark' else road
        return f'map_area_color_name[]: "{name}"\n map_area_color[]: {color(rgb)}\n map_area_discovered_color[]: {color(rgb)}'
    text = re.sub(r'map_area_color_name\[\]:\s*"([^"]+)"\s*map_area_color\[\]:\s*0x[0-9a-fA-F]+[^\n]*\s*map_area_discovered_color\[\]:\s*0x[0-9a-fA-F]+', area, text)
    text = re.sub(r'^//[^\n]*', '// CoPilot V9-inspired palette; green route requested for both modes.', text, count=1)
    folder = root/'BetterGPS_assets/native_themes'/('copilot_dark' if dark else 'copilot_light')
    folder.mkdir(parents=True, exist_ok=True)
    text = text.replace('ATS 1.61, native Google blue', 'ATS 1.61, CoPilot green')
    (folder/'colour_data.sui').write_text('\n'.join(line.rstrip() for line in text.splitlines())+'\n')
print(json.dumps(manifest, indent=2))
