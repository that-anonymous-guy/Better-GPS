"""Import NavPlus device art and shared Garmin 1440 footers pixel-exactly.

Input is newgpsoverlay/garmin. Non-block-aligned BC7 files are stored as RGBA
DDS at their original dimensions; neither the user's files nor pixels change.
"""
from pathlib import Path
from PIL import Image
import shutil
import sys

source = Path(sys.argv[1]).resolve()
target = Path(__file__).resolve().parents[1] / "BetterGPS_assets/material/garmin"
names = [(source/model/name, target/model/name)
         for model in ("kenworth_navplus", "peterbilt_smartnav")
         for shape in ("3by2", "5by3")
         for name in (f"home_{model}_{shape}.dds", f"splash_{model}_{shape}.dds",
                      f"{model}_{shape}_below_frame.dds")]
names += [(original, target/"Nuvi1440_collab"/original.name)
          for original in sorted((source/"Nuvi1440_collab").glob("*.dds"))]
assert len(names) == 16, "Expected twelve device images and four shared footers"
for original, destination in names:
    destination.parent.mkdir(parents=True, exist_ok=True)
    with Image.open(original) as image:
        pixels = image.convert("RGBA")
        if image.height % 4 or image.width % 4:
            pixels.save(destination, format="DDS")
        else:
            shutil.copy2(original, destination)
        with Image.open(destination) as imported:
            assert imported.size == image.size
            assert imported.convert("RGBA").tobytes() == pixels.tobytes()
    print(destination.relative_to(target), "pixel-exact", pixels.size)
