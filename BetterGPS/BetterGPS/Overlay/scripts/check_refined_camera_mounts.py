"""Check refined camera geometry at the recorded ATS mount poses.

Background Blender only: native PMG geometry, BVH collisions, stock grille
alpha holes and mirror symmetry. No game launch or user desktop interaction.
"""
import argparse,json,sys
from pathlib import Path
import bpy
from mathutils import Quaternion,Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree
sys.path.insert(0,str(Path(__file__).resolve().parent))
from scs_camera_mounts import geometry,read
from render_pp807032_mounts import visible_geometry,grid_hole


def transformed(points,row):
    q=Quaternion(row['rotation_wxyz']);t=Vector(row['position_scs'])
    return [t+q@Vector(p) for p in points]


def tree(points,faces):return BVHTree.FromPolygons(points,faces,all_triangles=True)


def run(stock,mod,authoring,output):
    models={name:geometry(mod/f'vehicle/truck/upgrade/bettergps/{name}/{file}.pmg') for name,file in
        (('pp804395','pp804395_lod0'),('pp804396','pp804396_truck'),('pp804397','pp804397_truck'),('pp807032','pp807032_lod0'),('pp807032_low','pp807032_low_lod0'))
        if (mod/f'vehicle/truck/upgrade/bettergps/{name}/{file}.pmg').exists()}
    results=[]
    for row in json.loads((authoring/'pp804395/scs-light/placement/mounts.json').read_text()):
        cabin=row['cabin'];points,faces=geometry(stock/f'cabin/{cabin}.pmg');cab=tree(points,faces)
        vertices,triangles=models['pp804395'];world=transformed(vertices,row)
        rear_max=read(stock/f'cabin/{cabin}.pmg')['bounds'][5];penetrations=[]
        for p in world:
            hit,_,_,_=cab.ray_cast(Vector((p.x,p.y,rear_max+2)),Vector((0,0,-1)),5)
            if hit and p.z<hit.z-.0001:penetrations.append(tuple(p))
        assert not penetrations,(cabin,'rear wall penetrations',len(penetrations))
        locator=next(l for part in read(stock/f'cabin/{cabin}.pmg')['parts'] for l in part['locators'] if l['name']=='rear_cab')
        rear='day_standard2' if cabin=='dc' else 'standard_aircon'
        ev,ef=geometry(stock/f'accessory/rear_cab/{rear}.pmg')
        overlaps=tree(world,triangles).overlap(tree(transformed(ev,{'position_scs':locator['position'],'rotation_wxyz':locator['rotation_wxyz']}),ef))
        assert not overlaps,(cabin,'rear equipment',len(overlaps))
        results.append({'cabin':cabin,'camera':'rear','wall_penetrations':0,'equipment_intersections':0})
    side_rows=json.loads((authoring/'pp804396/placement/mounts.json').read_text())
    for row in side_rows:
        cabin=row['cabin'];part=row['part_number'].lower();vertices,faces=models[part];world=transformed(vertices,row)
        cabpath=stock/f'cabin/{cabin}.pmg';cv,cf=geometry(cabpath);cab=tree(cv,cf)
        sign=1 if row['side']=='right' else -1
        penetrations=[]
        for point in world:
            hit,_,_,_=cab.ray_cast(Vector((sign*3,point.y,point.z)),Vector((-sign,0,0)),6)
            if hit and sign*(point.x-hit.x)<-.0001:penetrations.append(tuple(point))
        assert not penetrations,(cabin,part,'side wall penetrations',len(penetrations))
        loc=next(l for part in read(cabpath)['parts'] for l in part['locators'] if l['name']=='mirror')
        mv,mf=geometry(stock/'accessory/mirror/standard_paint.pmg')
        mirror=tree(transformed(mv,{'position_scs':loc['position'],'rotation_wxyz':loc['rotation_wxyz']}),mf)
        intersections=tree(world,faces).overlap(mirror)
        assert not intersections,(cabin,part,'mirror intersections',len(intersections))
        results.append({'cabin':cabin,'camera':part,'wall_penetrations':0,'mirror_intersections':0})
    for cabin in ('dc','s52m','s76h'):
        poses=[r for r in side_rows if r['cabin']==cabin]
        rw=transformed(models['pp804396'][0],next(r for r in poses if r['side']=='right'))
        lw=transformed(models['pp804397'][0],next(r for r in poses if r['side']=='left'))
        kd=KDTree(len(lw))
        for i,p in enumerate(lw):kd.insert(p,i)
        kd.balance();error=max(kd.find(Vector((-p.x,p.y,p.z)))[2] for p in rw)
        assert error<.000003,(cabin,'mirrored geometry differs',error)
        results.append({'cabin':cabin,'side_symmetry_max_error_m':error})
    extracted=stock.parents[2]
    for row in json.loads((authoring/'pp807032/placement/mounts.json').read_text()):
        vertices,faces=models[row['model']];vertices=[Vector(p) for p in vertices]
        width,thickness,spacing=(row['flat_section_m'][k] for k in ('width','thickness','spacing'))
        # Two full-length rectangular flats: no narrowed insertion tips.
        strap_faces=[f for f in faces if all(
            abs(abs(abs(vertices[i].x)-spacing/2)-thickness/2)<.000001
            and abs(abs(vertices[i].y)-width/2)<.000001
            and min(abs(vertices[i].z),abs(vertices[i].z-.0890166722))<.000001 for i in f)]
        body_faces=[f for f in faces if f not in strap_faces]
        assert len(strap_faces)==24,(row['bumper'],'expected two constant-section cubes',len(strap_faces))
        accepts_clipping=row.get('accepted_grille_flat_clipping',False)
        assert not accepts_clipping or row['bumper'].startswith('low_'),'Only low-bumper flat clipping is accepted'
        path=stock/f"accessory/bumper/{row['bumper']}.pmg"
        v,f,_,grids=visible_geometry(path,extracted)
        if not grids:
            iv,iff,_,_=visible_geometry(stock/'accessory/f_intake_chs/standard_plastic.pmg',extracted)
            start=len(v);v.extend((p[0],p[1]-.2395932674407959,p[2]-.0023708343505859375) for p in iv)
            f.extend(tuple(i+start for i in triangle) for triangle in iff)
        solid=tree(v,f);world=transformed(vertices,row)
        body=tree(world,body_faces);straps=tree(world,strap_faces)
        intersections=body.overlap(solid)
        assert not intersections,(row['bumper'],'camera body intersects solid grille',
                                  [{'camera':[tuple(round(n,5) for n in vertices[i]) for i in body_faces[pair[0]]],
                                    'truck':[tuple(round(v[i][k]-row['position_scs'][k],5) for k in range(3)) for i in f[pair[1]]]} for pair in intersections[:3]])
        flat_intersections=len(straps.overlap(solid));alpha_strikes=0
        if not accepts_clipping:assert not flat_intersections,(row['bumper'],'widened flats intersect solid grille')
        for grid in grids:
            assert not body.overlap(grid['tree']),(row['bumper'],'body intersects mesh surface')
            for triangle in strap_faces:
                cuts=[]
                for i,j in zip(triangle,triangle[1:]+triangle[:1]):
                    direction=world[j]-world[i];length=direction.length
                    if length<1e-8:continue
                    hit,_,_,_=grid['tree'].ray_cast(world[i],direction.normalized(),length)
                    if hit:cuts.append(hit)
                for hit in cuts:
                    alpha_strikes+=int(not grid_hole(grid,hit.x,hit.y))
                if len(cuts)==2:
                    for t in (.25,.5,.75):
                        hit=cuts[0].lerp(cuts[1],t)
                        alpha_strikes+=int(not grid_hole(grid,hit.x,hit.y))
            # Sample the entire constant section through the alpha grille.
            for side in (-1,1):
                for dx in (-thickness/2,0,thickness/2):
                    for dy in (-width/2,0,width/2):
                        alpha_strikes+=int(not grid_hole(grid,row['position_scs'][0]+side*spacing/2+dx,row['position_scs'][1]+dy))
        if not accepts_clipping:assert not alpha_strikes,(row['bumper'],'full flat section crosses alpha grille strands',alpha_strikes)
        ids=set(i for f in body_faces for i in f)
        gap=min(t.find_nearest(world[i])[3] for t in [solid]+[g['tree'] for g in grids] for i in ids)
        assert gap<.003,(row['bumper'],'body no longer against grille',gap)
        bearing_gaps=[]
        for grid in grids:
            for x in (-.0332,-.031,-.0288,.0288,.031,.0332):
                for y in ((.0005,.0025,.0045) if x<0 else (-.002,0,.002)):
                    origin=Vector((row['position_scs'][0]+x,row['position_scs'][1]+y,-1))
                    hit,_,_,_=grid['tree'].ray_cast(origin,Vector((0,0,1)),2)
                    if hit:
                        plate_front=row['position_scs'][2]+.0264-.09*x-.005*y
                        bearing_gaps.append(plate_front-hit.z)
        if bearing_gaps:assert min(bearing_gaps)>=0 and max(bearing_gaps)<.0006,(row['bumper'],'backing bridge is not against mesh',bearing_gaps)
        rear_bearing_gaps=[]
        if not grids:
            for x in (-.035,-.031,-.02,-.01,0,.01,.02,.031,.035):
                for y in (-.004,-.002,0,.002,.004):
                    z=row['position_scs'][2]
                    origin=Vector((row['position_scs'][0]+x,row['position_scs'][1]+y,z+.06))
                    hit,_,_,_=solid.ray_cast(origin,Vector((0,0,-1)),.06)
                    if hit and .032<hit.z-z<.055:
                        rear_bearing_gaps.append(z+.0435-.125*x-.005*y-hit.z)
            # A rigid crossbar bears at several points on the curved ribs;
            # intermediate curvature may leave clearance between those points.
            assert rear_bearing_gaps and 0<=min(rear_bearing_gaps)<.0003 and max(rear_bearing_gaps)<.0035,(row['bumper'],'rear bridge not bearing against 3D grille',rear_bearing_gaps)
        results.append({'bumper':row['bumper'],'model':row['model'],'body_intersections':0,'flat_intersections':flat_intersections,'alpha_grille_blocked_samples':alpha_strikes,
                        'low_bumper_flat_clipping_accepted':accepts_clipping,'body_gap_m':gap,'constant_section_clear':not(flat_intersections or alpha_strikes),
                        'backing_bridge_contact_max_gap_m':max(bearing_gaps) if bearing_gaps else None,
                        'backing_bridge_3d_grille_max_gap_m':max(rear_bearing_gaps) if rear_bearing_gaps else None,
                        'backing_bridge_3d_grille_contact_gap_m':min(rear_bearing_gaps) if rear_bearing_gaps else None,
                        'backing_bridge_mesh_present':bool(grids)})
    output.write_text(json.dumps({'status':'PASS','rear_and_side_poses_unchanged':True,'checks':results},indent=2)+'\n')
    print('REFINED_MOUNTS_PASS',len(results),'checks: rear walls/equipment, side mirrors/symmetry, 8 bumper poses.')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('stock','mod','authoring','output'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);run(a.stock.resolve(),a.mod.resolve(),a.authoring.resolve(),a.output.resolve())
