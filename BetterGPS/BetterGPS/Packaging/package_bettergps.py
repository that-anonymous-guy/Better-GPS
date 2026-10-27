"""Assemble the complete BetterGPS package from verified build outputs."""
from pathlib import Path
import hashlib, json, shutil, zipfile, sys

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "BetterGPS"
STAGE = ROOT / "package" / "BetterGPS"
# Validate before replacing staging: a permissive third-party SII parser alone
# does not catch names which the native ATS loader rejects (12-char tokens).
sys.path.insert(0,str(SOURCE/'Overlay/tools'))
from validate_sii_units import validate_sii_units
from managed_map_assets import validate as validate_managed_maps
validate_managed_maps(SOURCE/'Overlay/BetterGPS_assets')
for name in ("kenworth_t680_2022.sii","volvo_vnl2025.sii","volvo_vnl2025_gps.sii","freightliner_cascadia_2019.sii","freightliner_cascadia_2024_gauge.sii","westernstar_49x.sii","westernstar_57x.sii","westernstar_57x_gps.sii","westernstar_5700xe.sii","westernstar_5700xe_gps.sii","international_lt.sii","international_lonestar.sii"):
    validate_sii_units((SOURCE/"Overlay/BetterGPS_assets/ui/dashboard"/name).read_text(encoding="utf-8-sig"))
if STAGE.exists():
    shutil.rmtree(STAGE)
STAGE.mkdir(parents=True)

def copy(source: Path, target: str) -> None:
    if not source.exists():
        raise RuntimeError(f"Missing required package input: {source}")
    destination = STAGE / target
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.is_dir():
        shutil.copytree(source, destination, dirs_exist_ok=True, ignore=shutil.ignore_patterns("*.pdb", "*.ilk"))
    else:
        shutil.copy2(source, destination)

native = ROOT / "build" / "BetterGPS"
copy(native / "Overlay" / "Release" / "BetterGPS.dll", "plugins/BetterGPS.dll")
copy(native / "Overlay" / "Release" / "BetterGPSOverlay.dll", "plugins/BetterGPS/BetterGPSOverlay.dll")
copy(native / "Probe" / "Release" / "BetterGPSProbe.dll", "plugins/BetterGPS/BetterGPSProbe.dll")
copy(SOURCE / "Overlay" / "config", "plugins/BetterGPS")
copy(SOURCE / "Overlay" / "layouts", "plugins/BetterGPS/layouts")
copy(SOURCE / "Overlay" / "BetterGPS_assets", "plugins/BetterGPS/BetterGPS_assets")
validate_managed_maps(STAGE/'plugins/BetterGPS/BetterGPS_assets')
copy(SOURCE / "MapBridge" / "publish", "plugins/BetterGPS/runtime/engine")
copy(SOURCE / "Overlay" / "tools" / "AssetInspector" / "publish", "plugins/BetterGPS/runtime/shields")
copy(SOURCE / "Tools" / "BetterGPS.Diagnostics" / "publish", "plugins/BetterGPS/tools/diagnostics")
copy(SOURCE / "Probe" / "BetterGPSProbe.ini", "plugins/BetterGPS/BetterGPSProbe.ini")
copy(ROOT / "README.md", "README.md")
copy(ROOT / "docs" / "home-notrip-pages.md", "docs/home-notrip-pages.md")
copy(ROOT / "docs" / "touchscreen.md", "docs/touchscreen.md")
copy(ROOT / "docs" / "kenworth-t680-screen.md", "docs/kenworth-t680-screen.md")
copy(ROOT / "docs" / "kenworth-t680-2022-copilot.md", "docs/kenworth-t680-2022-copilot.md")
copy(ROOT / "docs" / "t680-camera-page.md", "docs/t680-camera-page.md")
copy(ROOT / "docs" / "t680-home-layers.md", "docs/t680-home-layers.md")
copy(ROOT / "docs" / "t680-clock-page.md", "docs/t680-clock-page.md")
copy(ROOT / "docs" / "t680-background-apps.md", "docs/t680-background-apps.md")
copy(ROOT / "docs" / "cursor-only-mode.md", "docs/cursor-only-mode.md")
copy(ROOT / "docs" / "game-timezones.md", "docs/game-timezones.md")
copy(ROOT / "docs" / "mack-tomtom.md", "docs/mack-tomtom.md")
copy(ROOT / "docs" / "volvo-vnl2014.md", "docs/volvo-vnl2014.md")
copy(ROOT / "docs" / "volvo-2018-tomtom.md", "docs/volvo-2018-tomtom.md")
copy(ROOT / "docs" / "volvo-vnl2025-sygic.md", "docs/volvo-vnl2025-sygic.md")
copy(ROOT / "docs" / "freightliner-cascadia-icc5.md", "docs/freightliner-cascadia-icc5.md")
copy(ROOT / "docs" / "freightliner-cascadia-icuc-zonar.md", "docs/freightliner-cascadia-icuc-zonar.md")
copy(ROOT / "docs" / "startup-guard-audit.md", "docs/startup-guard-audit.md")
copy(ROOT / "docs" / "international-t8520.md", "docs/international-t8520.md")
copy(ROOT / "docs" / "startup-performance.md", "docs/startup-performance.md")
copy(ROOT / "docs" / "kenworth-w900-screen.md", "docs/kenworth-w900-screen.md")
copy(ROOT / "docs" / "peterbilt-579-screen.md", "docs/peterbilt-579-screen.md")
copy(ROOT / "docs" / "peterbilt-smartnav.md", "docs/peterbilt-smartnav.md")
copy(ROOT / "docs" / "nuvi-display-publication.md", "docs/nuvi-display-publication.md")
copy(ROOT / "docs" / "truck-display-formats.md", "docs/truck-display-formats.md")
copy(SOURCE / "Packaging" / "Install-BetterGPS.ps1", "Install-BetterGPS.ps1")

files=[]
for path in sorted(p for p in STAGE.rglob("*") if p.is_file()):
    files.append({"path":path.relative_to(STAGE).as_posix(),"bytes":path.stat().st_size,"sha256":hashlib.sha256(path.read_bytes()).hexdigest()})
(STAGE/"MANIFEST.json").write_text(json.dumps({"name":"BetterGPS","files":files},indent=2),encoding="utf-8")
archive=ROOT/"package"/"BetterGPS.zip"
if archive.exists(): archive.unlink()
with zipfile.ZipFile(archive,"w",zipfile.ZIP_DEFLATED,compresslevel=9) as bundle:
    for path in sorted(p for p in STAGE.rglob("*") if p.is_file()):
        bundle.write(path,Path("BetterGPS")/path.relative_to(STAGE))
print(json.dumps({"package":str(STAGE),"archive":str(archive),"files":len(files)+1},indent=2))
