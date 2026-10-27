"""Prevent a rear-camera upgrade being silently mounted at front/side pins."""
import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from scs_camera_mounts import validate_tuning_bindings


class CameraBindingTests(unittest.TestCase):
    def test_old_front_type_steals_rear_accessory(self):
        with self.assertRaisesRegex(AssertionError,'Ambiguous ATS prefix binding'):
            validate_tuning_bindings(['bgps_cam','bgps_cam_f'],['bgps_cam','bgps_cam_f'])

    def test_old_side_types_have_the_same_collision(self):
        with self.assertRaisesRegex(AssertionError,'Ambiguous ATS prefix binding'):
            validate_tuning_bindings(['bgps_cam','bgps_cam_r','bgps_cam_l'],['bgps_cam','bgps_cam_r','bgps_cam_l'])

    def test_distinct_types_and_legacy_save_definitions(self):
        active=['bgps_cam','bgps_right','bgps_left','bgps_front']
        legacy=['bgps_cam_r','bgps_cam_l','bgps_cam_f']
        result=validate_tuning_bindings(active,active+legacy)
        for name in active:self.assertEqual(result[name],[name])
        for name in legacy:self.assertEqual(result[name],[])


if __name__=='__main__':unittest.main()
