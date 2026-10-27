"""Import supplied Nuvi assets and native palettes, without global ATS overrides."""
import argparse
from pathlib import Path
import re
import shutil
import zipfile
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument("--downloads", type=Path, required=True)
args = parser.parse_args()
assets = Path(__file__).resolve().parents[1] / "BetterGPS_assets"
source = args.downloads / "newgpsoverlay/garmin/Nuvi465T"
target = assets / "material/garmin/nuvi465t"
for path in source.rglob("*.dds"):
    if "exemple" in path.name.lower():
        continue
    destination = target / path.relative_to(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, destination)
    image = Image.open(path)
    if image.width % 4 or image.height % 4:
        # D3D11 BC textures require a block-aligned top mip. Some supplied DDS
        # canvases are 73/170/210 pixels high; retain exact pixels in RGBA DDS.
        image.convert("RGBA").save(destination)

# Keep the game's current area order and required fields, including the 1.61
# navigation_current_color absent from the supplied older map_data definition.
base = (assets / "native_themes/light/colour_data.sui").read_text()
for mode in ("light", "dark"):
    with zipfile.ZipFile(args.downloads / f"garminmap {mode}.zip") as archive:
        supplied = archive.read("latest/def/map_data.sii").decode("utf-8-sig")
    fields = dict(re.findall(r"(?m)^\s*(\w+):\s*(0x[\da-fA-F]+)", supplied))
    fields["navigation_current_color"] = fields["navigation_color"]
    areas = {m[0]: (m[1], m[2]) for m in re.findall(
        r'map_area_color_name\[\]:\s*"([^"]+)"\s*map_area_color\[\]:\s*(0x[\da-fA-F]+)[^\n]*\s*map_area_discovered_color\[\]:\s*(0x[\da-fA-F]+)', supplied)}
    palette = re.sub(r"(?m)^(\s*)(\w+):(\s*)(0x[\da-fA-F]+)",
        lambda m: f"{m[1]}{m[2]}:{m[3]}{fields.get(m[2], m[4])}", base)
    palette = re.sub(r'(map_area_color_name\[\]:\s*"([^"]+)"\s*map_area_color\[\]:\s*)(0x[\da-fA-F]+)([^\n]*\s*map_area_discovered_color\[\]:\s*)(0x[\da-fA-F]+)',
        lambda m: m[1] + areas.get(m[2], (m[3], m[5]))[0] + m[4] + areas.get(m[2], (m[3], m[5]))[1], palette)
    palette = "// Garmin supplied native palette; scoped to each device.\n" + palette[palette.index("\n")+1:]
    destination = assets / f"native_themes/garminmap_{mode}/colour_data.sui"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(palette, encoding="utf-8")

with zipfile.ZipFile(args.downloads / "garmintruckicon.zip") as archive:
    with archive.open("universal/material/ui/map/player.dds") as stream:
        icon = Image.open(stream).convert("RGBA")
        # Native icon catalog consumes explicit RGBA pixels, not BC blocks.
        destination = assets / "material/garmin/player_truck.dds"
        destination.parent.mkdir(parents=True, exist_ok=True)
        icon.save(destination)
print(f"Imported Nuvi modules, startup screens, both palettes and truck icon into {assets}")
