"""Read-only deployed user-state audit; snapshots are generated test output."""
import argparse
import hashlib
import json
import os
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("mode", choices=("snapshot", "verify"))
parser.add_argument("root", type=Path)
parser.add_argument("snapshot", type=Path)
args = parser.parse_args()
root = args.root.resolve(strict=True)

def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

if args.mode == "snapshot":
    files = {}
    preserved = {"bettergps_assets", "googlephonegps_assets", "device-settings",
                 "display-settings", "generated-displays", "layouts"}
    for directory, folders, names in os.walk(root):
        parent = Path(directory)
        if parent == root:
            folders[:] = [name for name in folders if name.lower() in preserved]
        for name in names:
            path = parent / name
            if path.suffix.lower() in {".dll", ".pdb", ".log"} or name == "MANIFEST.json":
                continue
            files[path.relative_to(root).as_posix()] = digest(path)
    args.snapshot.parent.mkdir(parents=True, exist_ok=True)
    args.snapshot.write_text(json.dumps(files, indent=2), encoding="utf-8")
    print(f"Snapshot: {len(files)} installed configuration/resource files")
else:
    files = json.loads(args.snapshot.read_text(encoding="utf-8"))
    changed = [name for name, expected in files.items()
               if not (root / name).is_file() or digest(root / name) != expected]
    if changed:
        raise SystemExit("Installed user state changed: " + ", ".join(changed))
    print(f"PASS: all {len(files)} existing installed configuration/resource files unchanged")
