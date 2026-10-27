"""Packaging regressions for the exact missing-map-material failure."""
import shutil
import struct
import sys
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from managed_map_assets import OVERLAY, capacity, graph, validate

class ManagedGraphTest(unittest.TestCase):
    def setUp(self):
        self.temp=TemporaryDirectory(prefix='bettergps-managed-graph-',dir=OVERLAY.parents[1]/'build')
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        files={relative for relative,_ in graph(self.root,capacity())}
        files.add('material/ui/dashboard/bettergps_map.dds')
        for relative in files:
            target=self.root/relative;target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(OVERLAY/'BetterGPS_assets'/relative,target)

    def test_complete_128_slot_graph(self):
        self.assertEqual(capacity(),128)
        self.assertEqual(validate(self.root),640)

    def test_exact_slot_32_material_failure_is_rejected(self):
        (self.root/'material/ui/dashboard/bettergps_map_device_32.mat').unlink()
        with self.assertRaisesRegex(RuntimeError,'bettergps_map_device_32.mat'):
            validate(self.root)

    def test_last_slot_dangling_reference_is_rejected(self):
        file=self.root/'material/ui/dashboard/bettergps_map_device_127.tobj'
        data=bytearray(file.read_bytes());data[48:]=b'/missing.dds';struct.pack_into('<I',data,40,len(data)-48);file.write_bytes(data)
        with self.assertRaisesRegex(RuntimeError,'TOBJ'):
            validate(self.root)

    def test_output_material_is_checked_too(self):
        file=self.root/'material/ui/dashboard/bettergps_device_output_64.mat'
        file.write_text('effect : "ui.rfx" { texture : "texture" { source : "/wrong.tobj" } }')
        with self.assertRaisesRegex(RuntimeError,'material source'):
            validate(self.root)

if __name__=='__main__':unittest.main()
