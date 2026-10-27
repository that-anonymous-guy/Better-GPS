"""Geometry policy tests; no renderer, game process or external packages required."""
from pathlib import Path
import math, struct, sys, unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from audit_truck_display_roles import components, physical_region

class DisplayRoles(unittest.TestCase):
    def test_internal_buffer_is_not_physical_aspect(self):
        # A 2:1 UV exposure rendered on a 5:3 physical panel, tilted in XYZ.
        points=[(0,0,0),(.15,0,0),(.15,.072,.054),(0,.072,.054)]
        uv=[(3,3),(4,3),(4,4),(3,4)]
        result=physical_region(points,uv,[(0,1,2),(0,2,3)])
        self.assertAlmostEqual(result['widthMm'],150,places=5)
        self.assertAlmostEqual(result['heightMm'],90,places=5)
        self.assertAlmostEqual(result['physicalAspect'],5/3,places=6)
        self.assertLess(result['planeErrorMm'],1e-6)

    def test_rounded_corners_do_not_overweight_tiny_triangles(self):
        outline=[(.01,0),(.14,0),(.15,.01),(.15,.08),(.14,.09),(.01,.09),(0,.08),(0,.01)]
        points=[(x,y,0) for x,y in outline]+[(.075,.045,0)]
        uv=[(3+x/.15,3+y/.09) for x,y,_ in points]
        triangles=[(i,(i+1)%8,8) for i in range(8)]
        result=physical_region(points,uv,triangles)
        self.assertAlmostEqual(result['physicalAspect'],5/3,places=6)

    def test_disconnected_dashboard_lcds_are_not_one_screen(self):
        # Two physical quads share material, UV tile and duplicated seam vertices.
        points=[(0,0,0),(.1,0,0),(.1,.05,0),(0,.05,0),
                (.5,0,0),(.6,0,0),(.6,.05,0),(.5,.05,0)]
        uv=[(3,3),(4,3),(4,4),(3,4)]*2
        triangles=[(0,1,2),(0,2,3),(4,5,6),(4,6,7)]
        po=128;vertices=po+100;indices=vertices+8*20
        data=bytearray(indices+len(triangles)*6)
        data[:4]=b'\x15gmP';struct.pack_into('<I',data,84,po)
        struct.pack_into('<II',data,po,len(triangles)*3,len(points))
        struct.pack_into('<IIII',data,po+60,20,vertices,0,vertices+12)
        struct.pack_into('<I',data,po+96,indices)
        for i,(point,coord) in enumerate(zip(points,uv)):struct.pack_into('<5f',data,vertices+i*20,*point,*coord)
        struct.pack_into('<'+'H'*12,data,indices,*(v for tri in triangles for v in tri))
        regions=list(components(data,0,(3,3)))
        self.assertEqual(len(regions),2)
        self.assertTrue(all(abs(r['physicalAspect']-2)<1e-5 for r in regions))
        self.assertGreater(abs(regions[0]['center'][0]-regions[1]['center'][0]),.4)

    def test_nonplanar_surface_is_flagged(self):
        result=physical_region([(0,0,0),(.15,0,0),(.15,.09,.004),(0,.09,0)],
           [(3,3),(4,3),(4,4),(3,4)],[(0,1,2),(0,2,3)])
        self.assertGreater(result['planeErrorMm'],1)

if __name__=='__main__':unittest.main()
