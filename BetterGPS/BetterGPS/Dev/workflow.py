"""BetterGPS development commands. Full release packaging remains separate."""
from __future__ import annotations
import argparse
import concurrent.futures
import datetime as dt
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
PROFILES = json.loads((HERE / "profiles.json").read_text(encoding="utf-8"))
TARGETS = {"overlay": "BetterGPSOverlay", "preview": "OverlayPreviewTest", "touch": "TouchPhoneTest", "checks": "BetterGPSChecks", "all": "BetterGPSDev"}
BINARIES = {"overlay": "BetterGPSOverlay.dll", "preview": "OverlayPreviewTest.exe", "touch": "TouchPhoneTest.exe"}

def digest(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()

def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    os.replace(temporary, path)

def contained(root, relative):
    """Validate a literal relative path and every existing path component."""
    root = Path(root).absolute()
    relative = str(relative).replace("\\", "/")
    parts = PurePosixPath(relative).parts
    if not parts or relative.startswith("/") or any(p in ("..", ".") or ":" in p for p in parts):
        raise ValueError(f"Unsafe relative path: {relative}")
    current = root
    for part in ("", *parts):
        if part:
            current /= part
        if current.exists() and (current.is_symlink() or (getattr(current.lstat(), "st_file_attributes", 0) & 0x400)):
            raise ValueError(f"Reparse point forbidden: {current}")
    resolved = current.resolve()
    if not resolved.is_relative_to(root.resolve()):
        raise ValueError(f"Path escapes root: {relative}")
    return current

def execute(arguments, cwd=ROOT, timeout=None):
    result = subprocess.run([str(a) for a in arguments], cwd=cwd, capture_output=True, text=True, errors="replace", timeout=timeout)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {arguments}\n{result.stdout[-5000:]}\n{result.stderr[-2000:]}")
    return result.stdout.strip()

def settings():
    value = {"ats_root": "", "asset_source_root": "", "jobs": 4}
    local = ROOT / ".bettergps-dev.json"
    if local.exists():
        value.update(json.loads(local.read_text(encoding="utf-8-sig")))
    if not value["ats_root"]:
        # Prefer the game's actual existing installation; never create one.
        for drive in ("H", "D", "C", "E", "F", "G"):
            for folder in ("SteamLibrary/steamapps/common", "Program Files (x86)/Steam/steamapps/common"):
                candidate = Path(f"{drive}:/") / folder / "American Truck Simulator"
                if (candidate / "bin/win_x64/amtrucks.exe").is_file():
                    value["ats_root"] = str(candidate)
                    break
            if value["ats_root"]:
                break
    if not value["asset_source_root"]:
        candidate = ROOT.parent / "assets"
        if candidate.is_dir():
            value["asset_source_root"] = str(candidate)
        for folder in (() if value["asset_source_root"] else ("Desktop", "Downloads")):
            candidate = Path.home() / folder / "newgpsoverlay"
            if candidate.is_dir():
                value["asset_source_root"] = str(candidate)
                break
    return value

def cmake_path():
    found = shutil.which("cmake")
    if found:
        return found
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    if vswhere.is_file():
        found = execute([vswhere, "-latest", "-products", "*", "-property", "installationPath"])
        candidate = Path(found) / "Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
        if candidate.is_file():
            return str(candidate)
    raise RuntimeError("CMake not found. Install Visual Studio C++/CMake or add cmake to PATH.")

def ats_processes(game):
    if os.name != "nt":
        return []
    # Query actual PID/path on every install, not an earlier status snapshot.
    script = "@(Get-Process -Name amtrucks -ErrorAction SilentlyContinue | Select-Object Id,Path) | ConvertTo-Json -Compress"
    output = execute(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script])
    rows = json.loads(output) if output else []
    if isinstance(rows, dict):
        rows = [rows]
    expected = os.path.normcase(str(Path(game).resolve() / "bin/win_x64/amtrucks.exe"))
    return [row for row in rows if not row.get("Path") or os.path.normcase(str(Path(row["Path"]).resolve())) == expected]

def require_closed(game):
    rows = ats_processes(game)
    if rows:
        time.sleep(0.5)
        rows = ats_processes(game)
    if rows:
        raise RuntimeError("ATS still running; install deferred: " + json.dumps(rows))

def fingerprint(target, root=ROOT):
    overlay = root / "BetterGPS/Overlay"
    paths = list(overlay.glob("*.h")) + list(overlay.glob("*.cpp")) + list(overlay.glob("*.rc"))
    paths += list((root / "BetterGPS/Probe/third_party/scs_sdk/include").rglob("*.h"))
    paths += list((root / "BetterGPS/Shared").glob("*.h"))
    paths += list((overlay / "third_party/minhook").rglob("*.h"))
    paths += list((overlay / "third_party/minhook/src").glob("*.c"))
    # RC embeds fonts/images/shaders. Include only its actual dependencies,
    # not the unrelated editable 0.8-GiB runtime asset tree.
    pending = [overlay / "BetterGPSResources.rc"]
    visited = set()
    while pending:
        resource = pending.pop()
        if resource in visited:
            continue
        visited.add(resource)
        paths.append(resource)
        for name in re.findall(r'(?:#include|\bRCDATA)\s+"([^"]+)"', resource.read_text(encoding="utf-8")):
            dependency = overlay / name
            if not dependency.exists():
                dependency = resource.parent / name
            if dependency.suffix.lower() == ".rc":
                pending.append(dependency)
            else:
                paths.append(dependency)
    paths += [overlay / "CMakeLists.txt", root / "CMakeLists.txt"]
    if target in ("preview", "touch"):
        paths += list((overlay / "tests").glob("*.h"))
        paths += [overlay / "tests" / ("overlay_preview.cpp" if target == "preview" else "touch_phone.cpp")]
    result = hashlib.sha256()
    for path in sorted(set(paths)):
        result.update(str(path.relative_to(root)).encode())
        result.update(digest(path).encode())
    return result.hexdigest()

class Run:
    def __init__(self, command):
        stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8]
        self.path = ROOT / "build/dev-runs" / stamp
        self.path.mkdir(parents=True)
        self.report = {"schema": 1, "owner": "bettergps-dev", "command": command, "started_utc": stamp, "phases": [], "status": "running"}
        self.save()

    def save(self):
        write_json(self.path / "run.json", self.report)

    def phase(self, name, function):
        started = time.perf_counter()
        try:
            value = function()
        except Exception:
            self.report["phases"].append({"name": name, "seconds": round(time.perf_counter() - started, 3), "status": "failed"})
            self.save()
            raise
        self.report["phases"].append({"name": name, "seconds": round(time.perf_counter() - started, 3), "status": "passed"})
        self.save()
        return value

def binary(target, configuration="Release"):
    return ROOT / "build/BetterGPS/Overlay" / configuration / BINARIES[target]

def build(run, target, configuration, jobs):
    cmake = cmake_path()
    if not (ROOT / "build/CMakeCache.txt").exists():
        run.phase("configure", lambda: execute([cmake, "-S", ROOT, "-B", ROOT / "build", "-A", "x64"]))
    affected = list(BINARIES) if target == "all" else ["preview", "touch"] if target == "checks" else [target]
    before = {name: fingerprint(name) for name in affected}
    output = run.phase("build:" + target, lambda: execute([cmake, "--build", ROOT / "build", "--config", configuration, "--target", TARGETS[target], "--parallel", str(jobs)]))
    (run.path / "build.log").write_text(output + "\n", encoding="utf-8")
    state_path = ROOT / "build/dev-state/builds.json"
    state = json.loads(state_path.read_text()) if state_path.exists() else {}
    for name in affected:
        if fingerprint(name) != before[name]:
            raise RuntimeError("Sources changed during compilation; build again: " + name)
        state[configuration + ":" + name] = {"fingerprint": before[name], "sha256": digest(binary(name, configuration)), "run": str(run.path)}
    write_json(state_path, state)
    print(f"Build {target}: passed ({run.report['phases'][-1]['seconds']} s)")

def suites():
    source = (ROOT / "BetterGPS/Overlay/tests/overlay_preview.cpp").read_text(encoding="utf-8")
    return sorted(set(re.findall(r'"--([a-z0-9-]+)"', source)) | {"TouchPhoneTest"})

def test(run, cases, configuration, jobs, no_build):
    unknown = set(cases) - set(suites())
    if unknown:
        raise ValueError("Unknown suites: " + ", ".join(sorted(unknown)) + ". Use test --list.")
    target = "checks" if "TouchPhoneTest" in cases and len(cases) > 1 else "touch" if cases == ["TouchPhoneTest"] else "preview"
    if not no_build:
        build(run, target, configuration, jobs)
    required = {"touch" if case == "TouchPhoneTest" else "preview" for case in cases}
    state_path = ROOT / "build/dev-state/builds.json"
    state = json.loads(state_path.read_text()) if state_path.exists() else {}
    verified = {}
    for name in required:
        receipt = state.get(configuration + ":" + name, {})
        actual = {"fingerprint": fingerprint(name), "sha256": digest(binary(name, configuration))}
        if any(receipt.get(key) != value for key, value in actual.items()):
            raise RuntimeError("Test executable stale/unverified; run build --target " + name)
        verified[name] = actual
    def one(case):
        target = "touch" if case == "TouchPhoneTest" else "preview"
        command = [binary(target, configuration), run.path / case]
        if target == "preview":
            command.append("--" + case)
        started = time.perf_counter()
        try:
            output = execute(command, timeout=180)
            result = {"suite": case, "seconds": round(time.perf_counter() - started, 3), "status": "passed"}
        except Exception as error:
            output = str(error)
            result = {"suite": case, "seconds": round(time.perf_counter() - started, 3), "status": "failed"}
        (run.path / (case + ".log")).write_text(output + "\n", encoding="utf-8")
        return result
    independent = [case for case in cases if case not in PROFILES["exclusive"]]
    exclusive = [case for case in cases if case in PROFILES["exclusive"]]
    def checks():
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
            results = list(pool.map(one, independent))
        results += [one(case) for case in exclusive]
        for name, before in verified.items():
            if before != {"fingerprint": fingerprint(name), "sha256": digest(binary(name, configuration))}:
                raise RuntimeError("Sources/binary changed during tests: " + name)
        run.report["tests"] = results
        run.save()
        for result in results:
            print(f"{result['suite']}: {result['status']} ({result['seconds']} s)")
        if any(r["status"] != "passed" for r in results):
            raise RuntimeError("Tests failed; inspect logs in " + str(run.path))
    run.phase("tests", checks)

def source_file(relative, configuration):
    if relative == "BetterGPSOverlay.dll":
        state_file = ROOT / "build/dev-state/builds.json"
        state = json.loads(state_file.read_text()) if state_file.exists() else {}
        receipt = state.get(configuration + ":overlay", {})
        path = binary("overlay", configuration)
        if receipt.get("fingerprint") != fingerprint("overlay") or receipt.get("sha256") != digest(path):
            raise RuntimeError("DLL stale/unverified. Run build --target overlay first.")
        return path
    if relative.startswith("BetterGPS_assets/") or relative.startswith("layouts/"):
        return contained(ROOT / "BetterGPS/Overlay", relative)
    if relative.startswith("docs/"):
        return contained(ROOT, relative)
    raise ValueError("Unsupported deployment path (settings are never overwritten): " + relative)

def make_plan(files, destination, configuration="Release", replace_custom=False, resolver=source_file):
    previous_file = destination / "BetterGPSPackageManifest.json"
    previous = json.loads(previous_file.read_text(encoding="utf-8-sig")) if previous_file.exists() else {"name": "BetterGPS", "files": []}
    known = {r["path"]: r.get("sha256", "").lower() for r in previous.get("files", [])}
    plan = []
    for relative in sorted(set(files)):
        relative = relative.replace("\\", "/")
        target = contained(destination, relative)
        source = Path(resolver(relative, configuration))
        if not source.is_file():
            raise FileNotFoundError(source)
        if relative.endswith("/import-manifest.json"):
            raise ValueError("Shared import manifests require a selective merge; use --merge-manifest with the selected assets.")
        new = digest(source)
        old = digest(target) if target.exists() else None
        if old == new:
            continue
        editable = relative.startswith(("BetterGPS_assets/", "layouts/"))
        if editable and old and old != known.get("plugins/BetterGPS/" + relative) and not replace_custom:
            raise RuntimeError("Customized/untracked installed asset preserved: " + relative + ". Explicit replacement requires --replace-custom.")
        plan.append({"relative": relative, "source": str(source.resolve()), "sha256": new, "before": old, "bytes": source.stat().st_size})
    return plan

def merge_manifest(relative, selected, destination, run):
    source = source_file(relative, "Release")
    target = contained(destination, relative)
    base = json.loads(target.read_text(encoding="utf-8-sig")) if target.exists() else {"files": {}}
    incoming = json.loads(source.read_text(encoding="utf-8-sig"))
    parent = PurePosixPath(relative).parent
    for name, value in incoming.get("files", {}).items():
        if str(parent / name) in selected:
            base.setdefault("files", {})[name] = digest(source_file(str(parent / name), "Release"))
    for name in selected:
        path = PurePosixPath(name)
        if path.parent == parent and path.suffix.lower() in (".dds", ".bga"):
            base.setdefault("files", {})[path.name] = digest(source_file(name, "Release"))
    for name, value in incoming.items():
        if name.endswith("_manifest") and isinstance(value, str) and str(parent / value) in selected:
            base[name] = value
    generated = run.path / "merged" / relative
    write_json(generated, base)
    old = digest(target) if target.exists() else None
    if digest(generated) == old:
        return None
    return {"relative": relative, "source": str(generated), "sha256": digest(generated), "before": old, "bytes": generated.stat().st_size}

def atomic_copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    temp = target.with_name(target.name + ".bettergps-" + uuid.uuid4().hex + ".tmp")
    try:
        shutil.copy2(source, temp)
        os.replace(temp, target)
    finally:
        if temp.exists():
            temp.unlink()

def install_plan(plan, destination, backup, guard=lambda: None):
    """All stale-plan checks precede writes; every changed byte has a rollback."""
    destination, backup = Path(destination), Path(backup)
    guard()
    for row in plan:
        target = contained(destination, row["relative"])
        if (digest(target) if target.exists() else None) != row["before"] or digest(row["source"]) != row["sha256"]:
            raise RuntimeError("Source/installed file changed since plan: " + row["relative"])
    previous_path = destination / "BetterGPSPackageManifest.json"
    previous = json.loads(previous_path.read_text(encoding="utf-8-sig")) if previous_path.exists() else {"name": "BetterGPS", "files": []}
    by_path = {row["path"]: row for row in previous.get("files", [])}
    for row in plan:
        key = "plugins/BetterGPS/" + row["relative"]
        by_path[key] = {"path": key, "bytes": row["bytes"], "sha256": row["sha256"]}
    previous["files"] = sorted(by_path.values(), key=lambda row: row["path"])
    backup.mkdir(parents=True, exist_ok=False)
    originals, original_hashes = {}, {}
    for relative in [r["relative"] for r in plan] + ["MANIFEST.json", "BetterGPSPackageManifest.json"]:
        target = contained(destination, relative)
        saved = contained(backup / "originals", relative)
        originals[relative] = target.exists()
        if target.exists():
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(target, saved)
            original_hashes[relative] = digest(target)
            if digest(saved) != original_hashes[relative]:
                raise RuntimeError("Backup verification failed: " + relative)
    # Capture configuration for recovery from later runtime migrations, without
    # changing it or restoring it over newer user settings during rollback.
    for name in ("BetterGPSDevices.ini", "BetterGPSDisplayPower.ini", "BetterGPSClock.ini"):
        source = destination / name
        if source.exists():
            (backup / "configuration-snapshot").mkdir(exist_ok=True)
            shutil.copy2(source, backup / "configuration-snapshot" / name)
    transaction = {"schema": 1, "owner": "bettergps-dev", "destination": str(destination.resolve()), "plan": plan, "originals": originals, "original_hashes": original_hashes, "status": "prepared"}
    write_json(backup / "transaction.json", transaction)
    touched = []
    try:
        guard()
        for row in plan:
            target = contained(destination, row["relative"])
            touched.append(row["relative"])
            atomic_copy(row["source"], target)
            if digest(target) != row["sha256"]:
                raise RuntimeError("Installed hash mismatch: " + row["relative"])
        for name in ("MANIFEST.json", "BetterGPSPackageManifest.json"):
            touched.append(name)
            write_json(destination / name, previous)
        transaction.update(status="installed", installed_hashes={relative: digest(contained(destination, relative)) for relative in originals})
        write_json(backup / "transaction.json", transaction)
    except Exception:
        for relative in reversed(touched):
            target = contained(destination, relative)
            if originals[relative]:
                atomic_copy(contained(backup / "originals", relative), target)
            elif target.exists():
                target.unlink()
        raise

def rollback(backup, destination, apply=False, guard=lambda: None):
    backup, destination = Path(backup), Path(destination)
    transaction = json.loads((backup / "transaction.json").read_text(encoding="utf-8"))
    if transaction.get("owner") != "bettergps-dev" or transaction.get("status") != "installed" or transaction.get("destination") != str(destination.resolve()):
        raise ValueError("Not an installed BetterGPS development transaction for this destination.")
    for relative, expected in transaction["installed_hashes"].items():
        target = contained(destination, relative)
        if not target.is_file() or digest(target) != expected:
            raise RuntimeError("Rollback would replace newer/user changes: " + relative)
    for relative, existed in transaction["originals"].items():
        if existed and (not contained(backup / "originals", relative).is_file() or digest(contained(backup / "originals", relative)) != transaction["original_hashes"].get(relative)):
            raise RuntimeError("Backup incomplete: " + relative)
    if apply:
        guard()
        attempt = contained(backup, "rollback-attempt-" + uuid.uuid4().hex)
        for relative in transaction["originals"]:
            saved = contained(attempt, relative)
            saved.parent.mkdir(parents=True, exist_ok=True)
            atomic_copy(contained(destination, relative), saved)
        touched = []
        try:
            guard()
            for relative, existed in transaction["originals"].items():
                target = contained(destination, relative)
                touched.append(relative)
                if existed:
                    atomic_copy(contained(backup / "originals", relative), target)
                else:
                    target.unlink()
            transaction["status"] = "rolled-back"
            write_json(backup / "transaction.json", transaction)
        except Exception:
            for relative in reversed(touched):
                atomic_copy(contained(attempt, relative), contained(destination, relative))
            raise
    return list(transaction["originals"])

def deploy(run, args, env):
    game = Path(env["ats_root"])
    if not env["ats_root"] or not (game / "bin/win_x64/amtrucks.exe").is_file():
        raise RuntimeError("ATS installation not found; set ats_root in .bettergps-dev.json.")
    destination = game / "bin/win_x64/plugins/BetterGPS"
    if not destination.is_dir():
        raise RuntimeError("Use the full package installer for the first installation.")
    plan = run.phase("plan", lambda: make_plan(args.file, destination, args.configuration, args.replace_custom))
    for name in args.merge_manifest:
        if not name.startswith("BetterGPS_assets/") or not name.endswith("/import-manifest.json"):
            raise ValueError("Only shared asset import manifests can be selectively merged.")
        row = merge_manifest(name, {p.replace("\\", "/") for p in args.file}, destination, run)
        if row:
            plan.append(row)
    write_json(run.path / "deploy-plan.json", {"destination": str(destination), "files": plan})
    print(json.dumps({"changes": len(plan), "bytes": sum(r["bytes"] for r in plan), "files": [r["relative"] for r in plan]}, indent=2))
    if not plan or not args.apply:
        print("No changes." if not plan else "Plan only. Add --apply to install these exact files.")
        return
    backup = contained(destination, "backups/dev-" + run.path.name)
    run.phase("install", lambda: install_plan(plan, destination, backup, lambda: require_closed(game)))
    run.report["backup"] = str(backup)
    run.save()
    print("Installed and verified. Backup: " + str(backup))

def prepare(run, env):
    result = {"repo": str(ROOT), "head": execute(["git", "rev-parse", "--short", "HEAD"]), "dirty": execute(["git", "status", "--short"]).splitlines(), "python": sys.executable, "cmake": cmake_path(), "build_cache": (ROOT / "build/CMakeCache.txt").exists(), "ats_root": env["ats_root"], "ats_processes": ats_processes(env["ats_root"]) if env["ats_root"] else [], "asset_source_root": env["asset_source_root"], "Pillow": importlib.util.find_spec("PIL") is not None, "profiles": list(PROFILES["profiles"])}
    pending = ROOT / ".bettergps-dev-pending.json"
    if pending.exists():
        rows = json.loads(pending.read_text(encoding="utf-8"))["files"]
        result["preserve_pending"] = [{**row, "changed_since_record": digest(contained(ROOT, row["path"])) != row["sha256"] if row.get("sha256") else None} for row in rows]
    write_json(run.path / "preflight.json", result)
    print(json.dumps(result, indent=2, ensure_ascii=False))

def asset(run, args, env):
    from PIL import Image
    source = Path(args.source)
    if not source.is_file() and env["asset_source_root"] and not source.is_absolute():
        source = contained(env["asset_source_root"], args.source)
    with Image.open(source) as image:
        rgba = image.convert("RGBA")
        report = {"source": str(source.resolve()), "size": list(image.size), "format": image.format, "mode": image.mode, "source_sha256": digest(source)}
    if not 0 < rgba.width <= 4096 or not 0 < rgba.height <= 4096:
        raise ValueError("Asset exceeds CPU decoder's 4096-pixel limit.")
    if args.destination:
        if not args.destination.lower().endswith(".dds"):
            raise ValueError("Destination must be a relative DDS path inside BetterGPS_assets.")
        target = contained(ROOT / "BetterGPS/Overlay/BetterGPS_assets", args.destination)
        report["destination"] = str(target)
        candidate = run.path / "converted.dds"
        rgba.save(candidate, format="DDS")
        with Image.open(candidate) as decoded:
            if decoded.convert("RGBA").tobytes() != rgba.tobytes():
                raise RuntimeError("DDS conversion altered decoded RGBA pixels.")
        report.update(sha256=digest(candidate), encoding="uncompressed RGBA8", applied=args.apply)
        if args.apply:
            if target.exists():
                saved = contained(run.path / "original-assets", args.destination)
                saved.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(target, saved)
            atomic_copy(candidate, target)
        rgba.save(run.path / "preview.png")
    elif args.apply:
        raise ValueError("--apply requires --destination.")
    write_json(run.path / "asset.json", report)
    print(json.dumps(report, indent=2))

def cleanup(run, days, apply):
    root = ROOT / "build/dev-runs"
    candidates = []
    for directory in root.iterdir():
        receipt = directory / "run.json"
        if directory == run.path or not receipt.is_file() or directory.is_symlink():
            continue
        info = json.loads(receipt.read_text())
        if info.get("owner") == "bettergps-dev" and info.get("status") in ("passed", "failed") and time.time() - receipt.stat().st_mtime > days * 86400:
            contained(root, directory.name)
            # Inspect the whole tree before recursive removal, not just root.
            for base, dirs, files in os.walk(directory, followlinks=False):
                for name in dirs + files:
                    contained(root, (Path(base) / name).relative_to(root).as_posix())
            candidates.append(directory)
    print(json.dumps({"managed_runs": [str(p) for p in candidates], "apply": apply}, indent=2))
    if apply:
        for directory in candidates:
            shutil.rmtree(directory)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("prepare")
    p = commands.add_parser("pending", help="Record explicitly selected existing changes without committing or editing them")
    p.add_argument("--file", action="append", required=True)
    b = commands.add_parser("build")
    b.add_argument("--target", choices=TARGETS, default="overlay")
    t = commands.add_parser("test")
    t.add_argument("--profile", choices=PROFILES["profiles"])
    t.add_argument("--suite", action="append", default=[])
    t.add_argument("--list", action="store_true")
    t.add_argument("--no-build", action="store_true")
    d = commands.add_parser("deploy")
    d.add_argument("--file", action="append", required=True, help="Path relative to installed BetterGPS component")
    d.add_argument("--merge-manifest", action="append", default=[])
    d.add_argument("--replace-custom", action="store_true")
    d.add_argument("--apply", action="store_true")
    r = commands.add_parser("rollback")
    r.add_argument("--backup", required=True, help="backups/dev-... path relative to installed BetterGPS")
    r.add_argument("--apply", action="store_true")
    c = commands.add_parser("cleanup")
    c.add_argument("--days", type=int, default=14)
    c.add_argument("--apply", action="store_true")
    a = commands.add_parser("asset")
    a.add_argument("--source", required=True, help="Absolute path or path relative to detected asset source root")
    a.add_argument("--destination", help="Relative path inside workspace BetterGPS_assets")
    a.add_argument("--apply", action="store_true")
    for p in (b, t, d):
        p.add_argument("--configuration", choices=["Release", "Debug"], default="Release")
    for p in (b, t):
        p.add_argument("--jobs", type=int)
    args = parser.parse_args()
    if args.command == "test" and args.list:
        print(json.dumps({"profiles": PROFILES["profiles"], "suites": suites()}, indent=2))
        return 0
    env = settings()
    jobs = getattr(args, "jobs", None) or env["jobs"]
    if not isinstance(jobs, int) or not 1 <= jobs <= 32:
        parser.error("jobs must be between 1 and 32")
    if args.command == "cleanup" and args.days < 1:
        parser.error("cleanup days must be positive")
    run = Run(args.command)
    try:
        if args.command == "prepare":
            run.phase("prepare", lambda: prepare(run, env))
        elif args.command == "pending":
            rows = []
            for name in args.file:
                path = contained(ROOT, name)
                if not path.exists():
                    raise FileNotFoundError(path)
                rows.append({"path": path.relative_to(ROOT).as_posix(), "sha256": digest(path) if path.is_file() else None, "kind": "file" if path.is_file() else "directory"})
            write_json(ROOT / ".bettergps-dev-pending.json", {"schema": 1, "head": execute(["git", "rev-parse", "HEAD"]), "files": rows})
            print("Recorded existing changes; contents and Git index unchanged.")
        elif args.command == "build":
            build(run, args.target, args.configuration, jobs)
        elif args.command == "test":
            cases = list(dict.fromkeys(PROFILES["profiles"].get(args.profile, []) + args.suite))
            if not cases:
                raise ValueError("Choose --profile or --suite, or use --list.")
            test(run, cases, args.configuration, jobs, args.no_build)
        elif args.command == "deploy":
            deploy(run, args, env)
        elif args.command == "cleanup":
            cleanup(run, args.days, args.apply)
        elif args.command == "asset":
            run.phase("asset", lambda: asset(run, args, env))
        elif args.command == "rollback":
            game = Path(env["ats_root"])
            if not env["ats_root"] or not (game / "bin/win_x64/amtrucks.exe").is_file():
                raise RuntimeError("ATS installation not found.")
            destination = game / "bin/win_x64/plugins/BetterGPS"
            if not args.backup.startswith("backups/dev-"):
                raise ValueError("Only managed backups/dev-... transactions may be restored.")
            changed = run.phase("rollback", lambda: rollback(contained(destination, args.backup), destination, args.apply, lambda: require_closed(game)))
            print(json.dumps({"restore": changed, "applied": args.apply}, indent=2))
        run.report["status"] = "passed"
        return 0
    except Exception as error:
        run.report.update(status="failed", error=str(error))
        print(str(error), file=sys.stderr)
        return 1
    finally:
        run.save()
        print("Receipt: " + str(run.path / "run.json"))

if __name__ == "__main__":
    sys.exit(main())
