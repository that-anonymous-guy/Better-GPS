"""Safety and isolation tests for the development installer (no live ATS writes)."""
import importlib.util
import json
from pathlib import Path
import tempfile
import time
import unittest
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[1] / "workflow.py"
spec = importlib.util.spec_from_file_location("bettergps_dev_workflow", SCRIPT)
w = importlib.util.module_from_spec(spec)
spec.loader.exec_module(w)

class WorkflowTests(unittest.TestCase):
    def setUp(self):
        temp_root = w.ROOT / "build/dev-unit-tests"
        temp_root.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=temp_root)
        w.contained(temp_root, Path(self.temp.name).name)
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.source = self.base / "source"
        self.destination = self.base / "installed"
        self.source.mkdir()
        self.destination.mkdir()
        self.backup = self.destination / "backups/dev-test"
        (self.source / "BetterGPSOverlay.dll").write_bytes(b"new DLL")
        (self.destination / "BetterGPSOverlay.dll").write_bytes(b"old DLL")
        (self.destination / "BetterGPSDevices.ini").write_bytes(b"personal settings")
        self.manifest = {"name": "BetterGPS", "files": [{"path": "plugins/BetterGPS/BetterGPSOverlay.dll", "sha256": w.digest(self.destination / "BetterGPSOverlay.dll"), "bytes": 7}, {"path": "plugins/BetterGPS/untouched.dds", "sha256": "unchanged", "bytes": 1}]}
        for name in ("MANIFEST.json", "BetterGPSPackageManifest.json"):
            w.write_json(self.destination / name, self.manifest)

    def resolve(self, relative, configuration):
        return w.contained(self.source, relative)

    def plan(self, files=None, replace=False):
        return w.make_plan(files or ["BetterGPSOverlay.dll"], self.destination, replace_custom=replace, resolver=self.resolve)

    def test_incremental_backup_verify_and_rollback(self):
        plan = self.plan()
        w.install_plan(plan, self.destination, self.backup)
        self.assertEqual((self.destination / "BetterGPSOverlay.dll").read_bytes(), b"new DLL")
        self.assertEqual((self.destination / "BetterGPSDevices.ini").read_bytes(), b"personal settings")
        self.assertEqual((self.backup / "originals/BetterGPSOverlay.dll").read_bytes(), b"old DLL")
        self.assertFalse(list(self.backup.rglob("*.zip")))
        installed = json.loads((self.destination / "BetterGPSPackageManifest.json").read_text())
        self.assertEqual(next(r for r in installed["files"] if r["path"].endswith("untouched.dds"))["sha256"], "unchanged")
        self.assertEqual(self.plan(), [])
        w.rollback(self.backup, self.destination, apply=True)
        self.assertEqual((self.destination / "BetterGPSOverlay.dll").read_bytes(), b"old DLL")
        self.assertEqual(json.loads((self.destination / "BetterGPSPackageManifest.json").read_text()), self.manifest)

    def test_stale_plan_refused_before_any_write(self):
        plan = self.plan()
        (self.destination / "BetterGPSOverlay.dll").write_bytes(b"newer user version")
        with self.assertRaisesRegex(RuntimeError, "changed since plan"):
            w.install_plan(plan, self.destination, self.backup)
        self.assertFalse(self.backup.exists())

    def test_game_process_guard_prevents_install(self):
        with self.assertRaisesRegex(RuntimeError, "game is open"):
            w.install_plan(self.plan(), self.destination, self.backup, lambda: (_ for _ in ()).throw(RuntimeError("game is open")))
        self.assertFalse(self.backup.exists())

    def test_failure_restores_all_touched_files_and_metadata(self):
        extra = "BetterGPS_assets/new.dds"
        path = w.contained(self.source, extra)
        path.parent.mkdir()
        path.write_bytes(b"asset")
        plan = self.plan(["BetterGPSOverlay.dll", extra])
        original = w.atomic_copy
        calls = 0
        def fail_second(source, target):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise OSError("injected failure")
            original(source, target)
        with mock.patch.object(w, "atomic_copy", fail_second):
            with self.assertRaisesRegex(OSError, "injected failure"):
                w.install_plan(plan, self.destination, self.backup)
        self.assertEqual((self.destination / "BetterGPSOverlay.dll").read_bytes(), b"old DLL")
        self.assertFalse((self.destination / extra).exists())
        self.assertEqual(json.loads((self.destination / "BetterGPSPackageManifest.json").read_text()), self.manifest)

    def test_custom_asset_protection_and_explicit_replacement(self):
        relative = "BetterGPS_assets/icon.dds"
        for root, contents in ((self.source, b"new default"), (self.destination, b"personal icon")):
            file = w.contained(root, relative)
            file.parent.mkdir(exist_ok=True)
            file.write_bytes(contents)
        with self.assertRaisesRegex(RuntimeError, "Customized"):
            self.plan([relative])
        w.install_plan(self.plan([relative], replace=True), self.destination, self.backup)
        self.assertEqual((self.backup / "originals" / relative).read_bytes(), b"personal icon")

    def test_traversal_and_reparse_rejected(self):
        for relative in ("../escape", "folder/../../escape", "C:/escape", "/escape", ""):
            with self.subTest(relative=relative), self.assertRaises(ValueError):
                w.contained(self.destination, relative)
        outside = self.base / "outside"
        outside.mkdir()
        try:
            (self.destination / "linked").symlink_to(outside, target_is_directory=True)
        except OSError:
            return  # Windows may require developer mode; traversal checks ran.
        with self.assertRaisesRegex(ValueError, "Reparse"):
            w.contained(self.destination, "linked/file")

    def test_rollback_rejects_newer_changes(self):
        w.install_plan(self.plan(), self.destination, self.backup)
        (self.destination / "BetterGPSOverlay.dll").write_bytes(b"later patch")
        with self.assertRaisesRegex(RuntimeError, "newer/user"):
            w.rollback(self.backup, self.destination, apply=True)

    def test_manifest_merge_does_not_publish_unrelated_pending_mpg(self):
        name = "BetterGPS_assets/material/copilot/t680/import-manifest.json"
        source = w.contained(self.source, name)
        target = w.contained(self.destination, name)
        w.write_json(source, {"files": {"clock.dds": "new", "mpg.dds": "pending"}, "clock_manifest": "clock-import.json"})
        w.write_json(target, {"files": {"clock.dds": "old", "mpg.dds": "personal"}, "custom": 42})
        (source.parent / "clock.dds").write_bytes(b"clock pixels")
        run = mock.Mock(path=self.base / "run")
        with mock.patch.object(w, "source_file", self.resolve):
            row = w.merge_manifest(name, {"BetterGPS_assets/material/copilot/t680/clock.dds", "BetterGPS_assets/material/copilot/t680/clock-import.json"}, self.destination, run)
        merged = json.loads(Path(row["source"]).read_text())
        self.assertEqual(merged, {"files": {"clock.dds": w.digest(source.parent / "clock.dds"), "mpg.dds": "personal"}, "custom": 42, "clock_manifest": "clock-import.json"})

    def test_rollback_rejects_damaged_backup(self):
        w.install_plan(self.plan(), self.destination, self.backup)
        (self.backup / "originals/BetterGPSOverlay.dll").write_bytes(b"damaged backup")
        with self.assertRaisesRegex(RuntimeError, "Backup incomplete"):
            w.rollback(self.backup, self.destination, apply=True)
        self.assertEqual((self.destination / "BetterGPSOverlay.dll").read_bytes(), b"new DLL")

    def test_failed_rollback_preserves_the_current_installation(self):
        w.install_plan(self.plan(), self.destination, self.backup)
        before = (self.destination / "BetterGPSPackageManifest.json").read_bytes()
        original = w.atomic_copy
        calls = 0
        def fail_during_restore(source, target):
            nonlocal calls
            calls += 1
            if calls == 5:  # Three snapshots, first restore, second restore fails.
                raise OSError("rollback copy failure")
            original(source, target)
        with mock.patch.object(w, "atomic_copy", fail_during_restore):
            with self.assertRaisesRegex(OSError, "rollback copy failure"):
                w.rollback(self.backup, self.destination, apply=True)
        self.assertEqual((self.destination / "BetterGPSOverlay.dll").read_bytes(), b"new DLL")
        self.assertEqual((self.destination / "BetterGPSPackageManifest.json").read_bytes(), before)

    def test_asset_conversion_is_exact_and_dry_run_leaves_source_alone(self):
        from PIL import Image
        source = self.source / "image.dds"
        image = Image.new("RGBA", (47, 46), (200, 12, 31, 128))
        image.save(source, format="DDS")
        run = mock.Mock(path=self.base / "run")
        run.path.mkdir()
        with mock.patch("builtins.print"):
            w.asset(run, mock.Mock(source=str(source), destination="material/test.dds", apply=False), {"asset_source_root": ""})
        with Image.open(run.path / "converted.dds") as decoded:
            self.assertEqual(decoded.convert("RGBA").tobytes(), image.tobytes())
        self.assertFalse((w.ROOT / "BetterGPS/Overlay/BetterGPS_assets/material/test.dds").exists())

    def test_profile_names_exist_in_actual_test_dispatch(self):
        known = set(w.suites())
        for profile, cases in w.PROFILES["profiles"].items():
            self.assertFalse(set(cases) - known, profile)

    def test_process_guard_rechecks_stale_process_and_matches_exact_game(self):
        game = self.base / "game"
        expected = str(game.resolve() / "bin/win_x64/amtrucks.exe")
        with mock.patch.object(w, "execute", return_value=json.dumps([{"Id": 123, "Path": expected}, {"Id": 456, "Path": str(self.base / "other/amtrucks.exe") }])):
            self.assertEqual(w.ats_processes(game), [{"Id": 123, "Path": expected}])
        with mock.patch.object(w, "ats_processes", side_effect=[[{"Id": 123}], []]) as processes, mock.patch.object(w.time, "sleep"):
            w.require_closed(game)
            self.assertEqual(processes.call_count, 2)

    def test_cleanup_only_removes_old_completed_owned_runs(self):
        runs = self.base / "build/dev-runs"
        for name, owner, status in (("old", "bettergps-dev", "passed"), ("foreign", "other", "passed"), ("running", "bettergps-dev", "running")):
            receipt = runs / name / "run.json"
            w.write_json(receipt, {"owner": owner, "status": status})
            old = time.time() - 30 * 86400
            w.os.utime(receipt, (old, old))
        with mock.patch.object(w, "ROOT", self.base), mock.patch("builtins.print"):
            w.cleanup(mock.Mock(path=runs / "current"), 14, False)
            self.assertTrue((runs / "old").exists())
            w.cleanup(mock.Mock(path=runs / "current"), 14, True)
        self.assertFalse((runs / "old").exists())
        self.assertTrue((runs / "foreign").exists())
        self.assertTrue((runs / "running").exists())

if __name__ == "__main__":
    unittest.main()
