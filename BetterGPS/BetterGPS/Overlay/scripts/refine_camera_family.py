"""Refine all four camera assets, retaining their approved origins and poses.

Run in background Blender 3.6 with official SCS Tools. A simple box-projected
eight-swatch atlas contains real microtexture and specular masks. Four shared
materials separate satin, sun-lit optics, protective glass and metal hardware;
box cameras add a fifth non-specular finish for their optical cavity.
No generated high-poly geometry, complex unwrap, or game/UI automation.
"""
import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_pp804395_scs import cube, disc
import create_pp804396_camera as side

TEXTURE_PATH = 'vehicle/truck/upgrade/bettergps/camera_finish'
FRONT_PROFILES={'standard':{'width':.0105,'thickness':.0025,'spacing':.062},
                'low':{'width':.0055,'thickness':.0015,'spacing':.062125}}
COLORS = [(25,27,29,180),(7,8,10,215),(64,69,73,225),
          (12,23,27,205),(2,3,5,210),(10,14,12,80),(76,78,73,65),(14,16,18,115)]


def textures(destination):
    """Deterministic fine powder coat, RGB normal and alpha specular masks."""
    import numpy as np
    destination.mkdir(parents=True, exist_ok=True)
    rng=np.random.default_rng(807032)
    tile=256
    height=rng.normal(0,1,(tile,tile*8))
    height=(height+np.roll(height,1,0)+np.roll(height,1,1))/3
    for kind in ('palette','micro_normal','glass'):
        width,height_px=(4,4) if kind=='glass' else (tile*8,tile)
        data=np.zeros((height_px,width,4),dtype=np.float32)
        if kind=='glass':data[:]=(.45,.50,.52,.035)
        else:
            for slot,color in enumerate(COLORS):
                grain=height[:,slot*tile:(slot+1)*tile]
                if kind=='palette':
                    strength=.8 if slot in (0,5,7) else .20
                    for channel in range(3):data[:,slot*tile:(slot+1)*tile,channel]=np.clip((color[channel]+grain*strength)/255,0,1)
                    data[:,slot*tile:(slot+1)*tile,3]=np.clip((color[3]+grain*3)/255,0,1)
                else:
                    strength=.020 if slot in (0,5,7) else .002
                    dx=(np.roll(grain,1,1)-np.roll(grain,-1,1))*strength
                    dy=(np.roll(grain,1,0)-np.roll(grain,-1,0))*strength
                    normal=np.stack((dx,dy,np.ones_like(dx)),axis=-1)
                    normal/=np.linalg.norm(normal,axis=-1,keepdims=True)
                    data[:,slot*tile:(slot+1)*tile,:3]=normal*.5+.5
                    data[:,slot*tile:(slot+1)*tile,3]=1
        image=bpy.data.images.new('camera_'+kind,width=width,height=height_px,alpha=True)
        image.colorspace_settings.name='Non-Color'
        image.pixels.foreach_set(data.ravel());image.filepath_raw=str(destination/(kind+'.tga'))
        image.file_format='TARGA';image.save()
        image.filepath_raw=str(destination/(kind+'.png'));image.file_format='PNG';image.save()
        bpy.data.images.remove(image)


def mesh(name,vertices,faces):
    data=bpy.data.meshes.new(name);data.from_pydata(vertices,[],faces);data.update()
    obj=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(obj)
    return obj


def outline(width,height,r,steps):
    points=[]
    for x,z,start in ((width/2-r,height/2-r,0),(-width/2+r,height/2-r,90),
                      (-width/2+r,-height/2+r,180),(width/2-r,-height/2+r,270)):
        for j in range(steps+1):
            a=math.radians(start+90*j/steps);points.append((x+r*math.cos(a),z+r*math.sin(a)))
    return points


def frame(name,loops,steps=4,cap_back=False,cap_inside=False):
    # Each loop is width, height, corner radius, depth Y, centre Z.
    vertices=[]
    for w,h,r,y,z in loops:vertices.extend((x,y,z+zz) for x,zz in outline(w,h,r,steps))
    n=len(vertices)//len(loops);faces=[]
    for k in range(len(loops)-1):
        for i in range(n):faces.append((k*n+i,k*n+(i+1)%n,(k+1)*n+(i+1)%n,(k+1)*n+i))
    if cap_back:faces.append(tuple(reversed(range(n))))
    if cap_inside:faces.append(tuple(range((len(loops)-1)*n,len(loops)*n)))
    obj=mesh(name,vertices,faces)
    if cap_inside:
        roles=obj.data.attributes.new('finish_role','INT','FACE')
        start=(len(loops)-3)*n
        for p in obj.data.polygons:
            if start<=p.index<(len(loops)-1)*n or p.index==len(faces)-1:roles.data[p.index].value=4
    for p in obj.data.polygons:
        ys=[obj.data.vertices[i].co.y for i in p.vertices]
        p.use_smooth=len(p.vertices)==4 and max(ys)-min(ys)>1e-7
    return obj


def glass_panel(name,w,h,y,z,lod):
    points=outline(w,h,.001,2 if lod else 3)
    # XZ counterclockwise presents its exterior normal toward -Y.
    return mesh(name,[(x,y,z+zz) for x,zz in points],[tuple(range(len(points)))])


def tube(name,points,radius,lod=0,smooth=False):
    if smooth and not lod:
        original=[Vector(p) for p in points];points=[]
        for k in range(len(original)-1):
            p0=original[max(0,k-1)];p1=original[k];p2=original[k+1];p3=original[min(len(original)-1,k+2)]
            for j in range(3):
                t=j/3
                points.append(tuple(.5*((2*p1)+(-p0+p2)*t+(2*p0-5*p1+4*p2-p3)*t*t+(-p0+3*p1-3*p2+p3)*t*t*t)))
        points.append(tuple(original[-1]))
        if name=='rear_camera_cable':points=[(min(p[0],.058),p[1],p[2]) for p in points]
    vertices=[];n=6 if lod else 8
    for i,point in enumerate(points):
        direction=Vector(points[min(i+1,len(points)-1)])-Vector(points[max(i-1,0)])
        basis=direction.to_track_quat('Z','Y')
        vertices.extend(tuple(Vector(point)+basis@Vector((radius*math.cos(j*math.tau/n),radius*math.sin(j*math.tau/n),0))) for j in range(n))
    faces=[]
    for i in range(len(points)-1):
        for j in range(n):faces.append((i*n+j,i*n+(j+1)%n,(i+1)*n+(j+1)%n,(i+1)*n+j))
    obj=mesh(name,vertices,faces)
    for p in obj.data.polygons:p.use_smooth=True
    return obj


def screw(name,location,direction,radius=.0035,depth=.0017,lod=0):
    """Low round head, bevelled rim and actual recessed hexagonal socket."""
    q=Vector(direction).to_track_quat('Z','Y');c=Vector(location)
    n=12 if lod else 24;vertices=[]
    for layer in range(5):
        for i in range(n):
            a=i*math.tau/n
            if layer<3:
                r=radius*(1 if layer<2 else .9)
                z=(-depth/2,depth*.24,depth/2)[layer]
            else:
                r=radius*.43*math.cos(math.pi/6)/math.cos(a%(math.pi/3)-math.pi/6)
                z=depth/2 if layer==3 else -depth*.18
            vertices.append(tuple(c+q@Vector((r*math.cos(a),r*math.sin(a),z))))
    faces=[(k*n+i,k*n+(i+1)%n,(k+1)*n+(i+1)%n,(k+1)*n+i) for k in range(4) for i in range(n)]
    head=mesh(name,vertices,faces)
    for i,p in enumerate(head.data.polygons):p.use_smooth=i//n<2
    floor=mesh(name+'_hex_socket_floor',vertices[4*n:5*n],[tuple(range(n))])
    return [(head,2,3),(floor,1,1)]


def drill(obj,name,location,direction,radius,depth):
    cutter=side.cylinder(name+'_cutter',location,radius,depth,0,direction,16)
    if obj.parent:cutter.matrix_world=obj.parent.matrix_world@cutter.matrix_world
    bpy.context.view_layer.update()
    bpy.context.view_layer.objects.active=obj
    modifier=obj.modifiers.new(name,'BOOLEAN');modifier.operation='DIFFERENCE';modifier.object=cutter
    bpy.ops.object.modifier_apply(modifier=modifier.name);bpy.data.objects.remove(cutter,do_unlink=True)


def bridge_depth(x,y=0):
    """Back of the stock low-bumper grille, in the approved camera frame."""
    return .0264-.09*x-.005*y


def backing_bridge(lod):
    # Universal stepped clamp: two forward bearing feet touch the thin low
    # grille; a rear crossbar bears behind the thicker standard grille ribs.
    # The existing two straight blades connect both bearing levels.
    vertices=[];faces=[]
    for cx,cy,width,height,offset in ((0,0,.070,.008,.0158),(-.031,.0025,.0044,.004,0),(.031,0,.0044,.004,0)):
        first=len(vertices)
        for back in (0,.002):
            for x,y in ((cx-width/2,cy-height/2),(cx+width/2,cy-height/2),(cx+width/2,cy+height/2),(cx-width/2,cy+height/2)):
                z=(.0435-.125*x-.005*y if offset else bridge_depth(x,y))+back
                vertices.append((-x/.625,z/.625,.072+y/.625))
        faces.extend(tuple(first+i for i in face) for face in ((0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)))
    return mesh('behind_grille_backing_bridge',vertices,faces)


def box_camera(lod,front,prefs):
    parts=[]
    def add(obj,slot=0,group=0,fixed=False):
        obj['finish_slot']=slot;obj['finish_group']=group;obj['fixed_mount']=fixed
        parts.append(obj);return obj
    steps=2 if lod else 5
    # Smooth rounded side cheeks continue into a deep bevel and an inset window.
    add(frame('rounded_camera_shell',[(.094,.058,.014,.030,.071),(.098,.062,.016,.026,.071),
        (.098,.062,.016,-.027,.071),(.095,.059,.0145,-.031,.071),
        (.078,.045,.002,-.031,.071),(.075,.042,.0015,-.026,.071),
        (.075,.042,.0015,-.0175,.071)],steps,True,True))
    add(cube('recessed_optical_board',(0,-.0185,.071),(.074,.0015,.041)),5)
    add(frame('window_seal',[(.077,.044,.0015,-.0268,.071),(.074,.041,.001,-.0268,.071)],2),1,1)
    add(glass_panel('protective_front_glass',.075,.042,-.0272,.071,lod),3,2)
    add(cube('sunshield',(0,-.003,.110),(.112,.072,.006),.0012 if not lod else .0007))
    for s in (-1,1):
        add(cube('visor_fold',(s*.052,-.031,.103),(.006,.016,.016),.0007))
        washer=disc('pivot',(s*.052,0,.072),.021,.005,32 if not lod else 16,(0,math.pi/2,0))
        bevel=washer.modifiers.new('rounded_pivot_edge','BEVEL');bevel.width=.00065;bevel.segments=2 if not lod else 1
        bpy.context.view_layer.objects.active=washer;bpy.ops.object.modifier_apply(modifier=bevel.name)
        for p in washer.data.polygons:p.use_smooth=len(p.vertices)==4
        add(washer,0,0,True)
        for obj,slot,group in screw('pivot_bolt',(s*.0565,0,.072),(s,0,0),.005,.003,lod):add(obj,slot,group,True)
        if not lod:
            # Two short relief arcs on each pivot washer.
            for start in (-.8,2.3):
                points=[(s*.0546,.014*math.cos(start+j*.16),.072+.014*math.sin(start+j*.16)) for j in range(8)]
                add(tube('pivot_relief',points,.0011),1,1,True)
    add(disc('lens_shroud',(0,-.021,.071),.0165,.004,24 if not lod else 12),1)
    add(disc('lens_retaining_ring',(0,-.0236,.071),.0098,.0015,24 if not lod else 12),1,1)
    add(disc('lens_coating',(0,-.0245,.071),.008,.0004,24 if not lod else 12),3,1)
    add(disc('lens_pupil',(0,-.0248,.071),.0048,.0002,24 if not lod else 12),4,1)
    if not lod:
        add(side.ring('lens_coating_ring',(0,-.02495,.071),.0076,.0069,.0001,3,n=24),3,1)
    # Rectangular staggered IR banks: 2+2+1+2+2 modules on each side.
    for s in (-1,1):
        for row,dz in enumerate((.016,.008,0,-.008,-.016)):
            for x in ((.029,) if row==2 else (.029,.0205)):
                xx=s*x
                if not lod:add(disc('ir_reflector_cup',(xx,-.0207,.071+dz),.0037,.0012,12),2,1)
                led=frame('ir_diode',[(.0053,.0053,.0026,-.0213,.071+dz),(.0045,.0045,.0022,-.0224,.071+dz)],1,True)
                for vertex in led.data.vertices:vertex.co.x+=xx
                # A faceted clear cap, seated in its metallic reflector cup.
                n=len(led.data.vertices)//2
                led.data.polygons[0].use_smooth=False
                cap=disc('ir_cap',(xx,-.02245,.071+dz),.0018,.0001,8 if not lod else 6)
                add(led,2,1)
                add(cap,3,1)
        if not lod:
            for dz in (-.018,.018):add(cube('board_trace',(s*.024,-.01935,.071+dz),(.013,.0001,.00035)),2,1)
    if front:
        # Retain the approved contact boss against the grille at the same depth.
        add(disc('grille_contact_boss',(0,.035,.071),.007,.012,8),1)
        length=prefs['strap_length_m']/.625
        profile=FRONT_PROFILES[prefs.get('mount_profile','standard')]
        half_spacing=profile['spacing']/2
        for s in (-1,1):
            # Full-width, full-thickness flat from the pivot to the fixing.
            # Placement must accommodate it; there is no narrowed insertion tip.
            add(cube('straight_grille_flat',(s*half_spacing/.625,length/2,.072),(profile['thickness']/.625,length,profile['width']/.625)),0,0,True)
        # A single bridge behind the grille receives both strap ends. It is
        # optional camera geometry and is never baked into the stock bumper.
        add(backing_bridge(lod),0,0,True)
        for s in (-1,1):
            y=(.0435+.125*s*half_spacing+.003)/.625
            for obj,slot,group in screw('grille_strap_fixing',(s*half_spacing/.625,y,.072),(0,1,0),.0035,.0028,lod):add(obj,slot,group,True)
    else:
        plate=add(cube('wall_plate',(0,.046,.071),(.120,.004,.045)),0,0,True)
        if not lod:drill(plate,'cable_passage',(0,.046,.055),(0,1,0),.003,.012)
        for s in (-1,1):add(cube('straight_mount_tab',(s*.052,.023,.071),(.006,.046,.018)),0,0,True)
        for s in (-1,1):
            for obj,slot,group in screw('rear_wall_fixing',(s*.052,.0429,.055),(0,-1,0),.0035,.0018,lod):add(obj,slot,group,True)
        add(side.ring('rear_cable_grommet',(0,.0435,.055),.0046,.00195,.002,1,(0,-1,0),20 if not lod else 12),1,0,True)
        add(disc('cable_gland',(.025,.031,.068),.0048,.008,10),1)
        # Compact loop exits the head and enters the fixed wall plate. It stays
        # inside the prior mounting envelope, with no dangling full OEM cable.
    pivot=Vector((0,0,.072));angle=math.radians(prefs['head_down_degrees']) if front else math.pi/4
    tilt=Matrix.Translation(pivot)@Matrix.Rotation(angle,4,'X')@Matrix.Translation(-pivot)
    bpy.context.view_layer.update()
    for obj in parts:
        if not obj['fixed_mount']:obj.matrix_world=tilt@obj.matrix_world
    if not front:
        start=tilt@Vector((.025,.035,.068))
        add(tube('rear_camera_cable',[tuple(start),(.038,.036,.107),(.055,.039,.099),(.058,.040,.087),(.058,.040,.056),(.047,.040,.044),(.022,.040,.043),(0,.041,.050),(0,.046,.055)],.0018,lod,True),1,0,True)
    basis=Matrix.Scale(.625,4)@Matrix.Rotation(math.pi,4,'Z')
    if front:basis=Matrix.Scale(.625,4)@Matrix.Translation(-pivot)@Matrix.Rotation(math.pi,4,'Z')
    bpy.context.view_layer.update()
    for obj in parts:obj.matrix_world=basis@obj.matrix_world
    return parts


def side_camera(lod,part,preferences):
    side.USER_ADJUSTMENTS=preferences
    root,ball,parts=side.make_geometry(lod)
    for obj in list(parts):
        if obj.name.startswith(('mounting_flange','mount_hole_liner','small_housing_label')):
            parts.remove(obj);bpy.data.objects.remove(obj,do_unlink=True)
    flange=side.rounded_plate('mounting_flange',.084,.106,.009,.0018,.006,0,6 if not lod else 3)
    flange.parent=root;parts.append(flange)
    bpy.context.view_layer.update()
    for x in (-.036,.036):
        for y in (-.0475,.0475):
            if not lod:
                drill(flange,'mount_hole',(x,y,.004),(0,0,1),.0023,.016)
                liner=side.ring('mount_hole_liner',(x,y,.00605),.0029,.002,.0005,3,(0,0,1),16)
                liner.parent=root;parts.append(liner)
    # Route the cable into a drilled, rubber-lined passage in the flange.
    for obj in list(parts):
        if obj.name.startswith('short_cable'):
            parts.remove(obj);bpy.data.objects.remove(obj,do_unlink=True)
    flange=next(o for o in parts if o.name.startswith('mounting_flange'))
    if not lod:drill(flange,'flange_cable_passage',(.020,.0505,.0037),(0,1,0),.0018,.014)
    for obj in (tube('side_routed_cable',[(0,.051,.004),(0,.058,.009),(.011,.060,.011),(.020,.059,.009),(.020,.056,.0045),(.020,.052,.0037)],.0017,lod,True),
                side.ring('side_cable_grommet',(.020,.053,.0037),.0026,.0018,.0016,1,(0,1,0),20 if not lod else 12)):
        obj.parent=root;obj['palette_slot']=1;parts.append(obj)
    mapping=[0,1,3,2,2,4,6,7]
    for obj in parts:
        obj['finish_slot']=mapping[obj['palette_slot']]
        obj['finish_group']=1 if obj.name.startswith(('rotating_optical_ball','lens_','inner_optical','optical_','IR_','mount_hole_liner')) else 0
        # Refined moulded edges and fixing countersinks, without changing base
        # dimensions or the mounting plane, and without subdividing the shell.
        if not lod and obj.name.startswith(('lower_housing_lip','fixed_front_rim','lens_bezel')):
            bevel=obj.modifiers.new('moulded_edge_radius','BEVEL');bevel.width=.00045;bevel.segments=2
            bevel.affect='EDGES';bevel.angle_limit=.5
            bpy.context.view_layer.objects.active=obj;bpy.ops.object.modifier_apply(modifier=bevel.name)
        if obj.name.startswith('rotating_optical_ball'):obj['finish_slot']=1
        if obj.name.startswith('mount_hole_liner'):obj['finish_group']=3
    # Four real fixing heads over the four existing flange holes. Their lower
    # faces seat on the flange; they do not alter the Z=0 mounting plane.
    for x in (-.036,.036):
        for y in (-.0475,.0475):
            for obj,slot,group in screw('flange_corner_fixing',(x,y,.00675),(0,0,1),.003,.0014,lod):
                obj.parent=root;obj['finish_slot']=slot;obj['finish_group']=group;parts.append(obj)
    # A thin clear protective lens, seated over the existing ball-local optics.
    cover=disc('side_optical_cover',(0,-.0282,0),.0091,.00015,24 if not lod else 12)
    cover.parent=ball;cover['finish_slot']=3;cover['finish_group']=2;parts.append(cover)
    if not lod:
        # Small screw heads in the optical face, placed between IR modules.
        for a in (math.pi/6,5*math.pi/6,3*math.pi/2):
            x=.014*math.cos(a);z=.014*math.sin(a)
            small_screw=disc('optical_face_screw',(x,-.0195,z),.001,.0006,6)
            small_screw.parent=ball;small_screw['finish_slot']=2;small_screw['finish_group']=3;parts.append(small_screw)
    bpy.context.view_layer.update()
    for obj in parts:
        world=obj.matrix_world.copy();obj.parent=None;obj.matrix_world=world
    for empty in (ball,root):bpy.data.objects.remove(empty,do_unlink=True)
    if part==397:
        mirror=Matrix.Diagonal((-1,1,1,1))
        for obj in parts:
            bpy.context.view_layer.objects.active=obj;obj.select_set(True)
            bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);obj.select_set(False)
            obj.data.transform(mirror);obj.data.flip_normals()
    return parts


def uv_finish(obj):
    # Simple local box projection within a padded 256px swatch. Sharing islands
    # is intentional: users can edit material swatches without a complex atlas.
    mesh=obj.data
    for layer in list(mesh.uv_layers):mesh.uv_layers.remove(layer)
    uv=mesh.uv_layers.new(name='palette');slot=obj['finish_slot']
    coords=[v.co for v in mesh.vertices]
    low=[min(v[k] for v in coords) for k in range(3)]
    span=[max(v[k] for v in coords)-low[k] for k in range(3)]
    for poly in mesh.polygons:
        face_slot=7 if mesh.attributes.get('finish_role') and mesh.attributes['finish_role'].data[poly.index].value==4 else slot
        axis=max(range(3),key=lambda k:abs(poly.normal[k]));axes=[k for k in range(3) if k!=axis]
        for i in poly.loop_indices:
            p=coords[mesh.loops[i].vertex_index]
            u=(p[axes[0]]-low[axes[0]])/max(span[axes[0]],1e-8)
            v=(p[axes[1]]-low[axes[1]])/max(span[axes[1]],1e-8)
            uv.data[i].uv=((face_slot+.075+.85*u)/8,.075+.85*v)
    # Stable pupil marker for independent direction checks, not for shading.
    if obj.name.startswith(('lens_pupil','optical_pupil')):
        for entry in uv.data:entry.uv=((slot+.5)/8,.5)
    for name in ('Col','Col_alpha'):
        existing=mesh.color_attributes.get(name)
        if existing:mesh.color_attributes.remove(existing)
        attribute=mesh.color_attributes.new(name=name,type='FLOAT_COLOR',domain='CORNER')
        for c in attribute.data:c.color=(.21404114,.21404114,.21404114,1)


def native_materials(addon):
    from io_scs_tools.internals import shader_presets
    from io_scs_tools.utils import material as material_utils
    materials=[]
    for name,preset,flavors in (('Satin_powder_coat','dif.spec','.tsnmap'),('Sun_specular_optics','dif.spec',''),('Protective_glass','glass',''),('Metal_hardware','dif.spec.add.env',''),('Dark_optical_cavity','dif','')):
        section=shader_presets.get_section(preset,flavors)
        assert section is not None,(preset,flavors)
        mat=bpy.data.materials.new(name)
        bpy.ops.mesh.primitive_plane_add();context_mesh=bpy.context.object
        context_mesh.data.materials.append(mat)
        mat.scs_props.active_shader_preset_name=preset;mat.scs_props.mat_effect_name='eut2.'+preset+flavors
        material_utils.set_shader_data_to_material(mat,section)
        index=len(materials)
        mat.scs_props.shader_attribute_diffuse=(.50,)*3 if index==4 else (1,1,1)
        mat.scs_props.shader_attribute_specular=((.48,.85,.50,.90,0)[index],)*3
        mat.scs_props.shader_attribute_shininess=(48,150,95,110,1)[index]
        mat.scs_props.shader_texture_base='//'+TEXTURE_PATH+('/glass.tga' if preset=='glass' else '/palette.tga')
        mat.scs_props.shader_attribute_add_ambient=0
        if preset=='dif.spec.add.env':mat.scs_props.shader_attribute_env_factor=(.16,)*3
        if preset=='glass':
            mat.scs_props.shader_attribute_tint=(.95,.97,1)
            mat.scs_props.shader_attribute_tint_opacity=.008
            mat.scs_props.shader_attribute_fresnel=(.2,.9)
        if flavors:mat.scs_props.shader_texture_nmap='//'+TEXTURE_PATH+'/micro_normal.tga'
        if preset in ('glass','dif.spec.add.env'):
            mat.scs_props.shader_texture_reflection_use_imported=True
            mat.scs_props.shader_texture_reflection_imported_tobj='/material/environment/vehicle_reflection' if preset=='glass' else '/material/environment/fuzzy_reflection/fuzzy_reflection'
        for tex in ('base','nmap'):
            for mapping in getattr(mat.scs_props,'shader_texture_'+tex+'_uv'):mapping.value='palette'
        materials.append(mat)
        bpy.data.objects.remove(context_mesh,do_unlink=True)
    return materials


def assign_finishes(obj,materials):
    obj.data.materials.clear()
    roles=obj.data.attributes.get('finish_role')
    if roles:
        for material in materials:obj.data.materials.append(material)
        for p in obj.data.polygons:p.material_index=roles.data[p.index].value
    else:obj.data.materials.append(materials[obj['finish_group']])


def preview_materials(project):
    mats=[]
    for i in range(5):
        mat=bpy.data.materials.new(('Satin preview','Polished preview','Clear glass preview','Metal preview','Dark cavity preview')[i]);mat.use_nodes=True
        bsdf=mat.node_tree.nodes.get('Principled BSDF')
        bsdf.inputs['Roughness'].default_value=(.46,.19,.055,.25,.95)[i]
        bsdf.inputs['Specular'].default_value=0 if i==4 else .38 if i==0 else .50
        tex=mat.node_tree.nodes.new('ShaderNodeTexImage')
        tex.image=bpy.data.images.load(str(project/TEXTURE_PATH/('glass.png' if i==2 else 'palette.png')),check_existing=True)
        mat.node_tree.links.new(tex.outputs['Color'],bsdf.inputs['Base Color'])
        if i==0:
            normal=mat.node_tree.nodes.new('ShaderNodeTexImage');normal.image=bpy.data.images.load(str(project/TEXTURE_PATH/'micro_normal.png'),check_existing=True)
            normal.image.colorspace_settings.name='Non-Color'
            node=mat.node_tree.nodes.new('ShaderNodeNormalMap');node.inputs['Strength'].default_value=1
            mat.node_tree.links.new(normal.outputs['Color'],node.inputs['Color']);mat.node_tree.links.new(node.outputs['Normal'],bsdf.inputs['Normal'])
        if i==3:bsdf.inputs['Metallic'].default_value=.9
        if i==4:
            links=bsdf.inputs['Base Color'].links
            if links:mat.node_tree.links.remove(links[0])
            bsdf.inputs['Base Color'].default_value=(.001,.0012,.0014,1)
        if i==2:
            bsdf.inputs['Base Color'].default_value=(.8,.88,.92,1)
            mat.node_tree.links.remove(bsdf.inputs['Base Color'].links[0])
            bsdf.inputs['Alpha'].default_value=.035;mat.blend_method='BLEND';mat.use_screen_refraction=True
            mat.show_transparent_back=False;mat.use_nodes=True
        mats.append(mat)
    return mats


def studio(parts,project,path,front=True):
    mats=preview_materials(project)
    for obj in parts:assign_finishes(obj,mats)
    scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=32
    scene.cycles.use_denoising=True
    scene.render.resolution_x=1100;scene.render.resolution_y=900;scene.render.resolution_percentage=100
    scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.25,.29,.35,1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value=.6
    scene.view_settings.view_transform='Standard';scene.view_settings.look='Medium High Contrast'
    points=[obj.matrix_world@v.co for obj in parts for v in obj.data.vertices]
    centre=Vector([(min(p[k] for p in points)+max(p[k] for p in points))/2 for k in range(3)])
    extent=max(max(p[k] for p in points)-min(p[k] for p in points) for k in range(3))
    lighting=[]
    for pos,power,size in (((1.1,-1.4,2),2.3,1.6),((-1.4,-.3,1.1),1.1,1.0),((.5,1.8,1.4),2,1.2)):
        bpy.ops.object.light_add(type='AREA',location=centre+Vector(pos)*extent)
        light=bpy.context.object;light.data.energy=power*(extent/.1)**2;light.data.size=size*extent
        light.rotation_euler=(centre-light.location).to_track_quat('-Z','Y').to_euler();lighting.append(light)
    # Box cameras are yawed for their approved sockets, hence look from +Y.
    direction=Vector((1.35,2.1,-.55 if path.stem=='pp804395' else .9)) if front else Vector((1.6,-2.0,1.3))
    bpy.ops.object.camera_add(location=centre+direction*extent)
    camera=bpy.context.object;scene.camera=camera;camera.data.type='ORTHO';camera.data.ortho_scale=extent*1.55
    camera.data.clip_start=.001;camera.rotation_euler=(centre-camera.location).to_track_quat('-Z','Y').to_euler()
    path.parent.mkdir(parents=True,exist_ok=True)
    scene.render.filepath=str(path);bpy.ops.render.render(write_still=True)
    for obj in lighting+[camera]:bpy.data.objects.remove(obj,do_unlink=True)
    for obj in parts:obj.data.materials.clear()


def build(root,addon):
    root=root.resolve();project=root/'project';textures(project/TEXTURE_PATH)
    authoring=root.parent
    front_prefs=json.loads((authoring/'pp807032/user-adjustments.json').read_text())
    side_prefs=json.loads((authoring/'pp804396/user-adjustments.json').read_text())
    sys.path.insert(0,str(addon.parent));import addon_utils
    addon_utils.enable('io_scs_tools',default_set=True)
    from io_scs_tools.internals.callbacks import persistent
    persistent.disable()
    from io_scs_tools.utils import get_scs_globals,save_scs_globals_to_blend
    from io_scs_tools.internals.containers import config
    from io_scs_tools.internals import looks
    from io_scs_tools.utils import printout
    from io_scs_tools.exp import pix
    printout.immediate_messenger.can_report=lambda timeout:False
    g=get_scs_globals();g.config_update_lock=True;g.config_storage_place='BlendFile';g.scs_project_path=str(project)
    g.export_output_type='5';g.export_scale=1;config.update_shader_presets_path(str(addon/'shader_presets.txt'))
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    materials=native_materials(addon);reports=[]
    for part in (395,396,397,7032,7033):
        name='pp807032' if part==7032 else 'pp807032_low' if part==7033 else f'pp804{part}'
        for lod in (0,1):
            bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
            prefs=dict(front_prefs,mount_profile='low' if part==7033 else 'standard')
            parts=box_camera(lod,part in (7032,7033),prefs) if part in (395,7032,7033) else side_camera(lod,part,side_prefs)
            for obj in parts:uv_finish(obj)
            bpy.context.view_layer.update()
            if not lod:studio(parts,project,root/f'previews/{name}.png',part in (395,7032,7033))
            for obj in parts:assign_finishes(obj,materials)
            host=bpy.data.objects.new(name+'_lod'+str(lod),None);bpy.context.collection.objects.link(host)
            bpy.context.view_layer.objects.active=host;bpy.context.view_layer.update();host.scs_props.empty_object_type='SCS_Root'
            if not host.scs_object_part_inventory:host.scs_object_part_inventory.add()['name']='defaultpart'
            for obj in parts:obj.parent=host;obj.scs_props.scs_part='defaultpart'
            looks.add_materials(host,materials);bpy.context.view_layer.update()
            save_scs_globals_to_blend()
            if not lod:
                # Editable, named individual components; root scale is already 1.
                edit_ball=None
                if part in (396,397):
                    edit_ball=bpy.data.objects.new(name.upper()+'_OPTICAL_BALL',None)
                    bpy.context.collection.objects.link(edit_ball);edit_ball.parent=host
                    edit_ball.location=(0,-.0275*.65,.0325*.65)
                    edit_ball.rotation_euler=side_prefs['optical_ball_rotation_euler']
                    edit_ball['up_degrees']=-math.degrees(side_prefs['optical_ball_rotation_euler'][0])
                    bpy.context.view_layer.update()
                    for child in parts:
                        if child.name.startswith(('rotating_optical_ball','lens_','inner_optical','optical_','IR_','side_optical')):
                            world=child.matrix_world.copy();child.parent=edit_ball;child.matrix_world=world
                for area in bpy.context.screen.areas:
                    if area.type=='VIEW_3D':area.spaces.active.region_3d.view_distance=.22;area.spaces.active.shading.type='MATERIAL'
                bpy.ops.wm.save_as_mainfile(filepath=str(root/(name+'_editable.blend')))
                if edit_ball:
                    for child in list(edit_ball.children):
                        world=child.matrix_world.copy();child.parent=host;child.matrix_world=world
                    bpy.data.objects.remove(edit_ball,do_unlink=True)
                    bpy.context.view_layer.update()
            bpy.ops.object.select_all(action='DESELECT')
            for obj in parts:obj.select_set(True)
            bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();obj=bpy.context.object
            bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
            old_materials=list(obj.data.materials)
            remap={m.name:i for i,m in enumerate(materials)}
            indices=[remap[old_materials[p.material_index].name] for p in obj.data.polygons]
            obj.data.materials.clear()
            for material in materials:obj.data.materials.append(material)
            for polygon,index in zip(obj.data.polygons,indices):polygon.material_index=index
            obj.data.calc_loop_triangles();triangles=len(obj.data.loop_triangles)
            assert triangles<8000,(name,lod,triangles)
            destination=project/f'vehicle/truck/upgrade/bettergps/{name}';destination.mkdir(parents=True,exist_ok=True)
            assert pix.export(str(destination),'',host,[obj])
            if part in (396,397):
                vehicle_basis=Matrix(((0,-1,0,0),(0,0,1,0),(-1,0,0,0),(0,0,0,1))) if part==396 else Matrix(((0,1,0,0),(0,0,1,0),(1,0,0,0),(0,0,0,1)))
                obj.data.transform(vehicle_basis);obj.data.update();host.name=name+'_truck'+('_lod1' if lod else '')
                bpy.context.view_layer.update()
                assert pix.export(str(destination),'',host,[obj])
            if not lod:
                save_scs_globals_to_blend();bpy.ops.wm.save_as_mainfile(filepath=str(root/(name+'_scs.blend')))
            reports.append({'model':name,'lod':lod,'triangles':triangles,'materials':5 if part in (395,7032,7033) else 4,'export_objects':1})
    for file in project.rglob('*.pi[mt]'):file.write_text(file.read_text().rstrip()+'\n')
    (project/TEXTURE_PATH/'micro_normal.tobj').write_text('map 2d micro_normal.tga\naddr clamp_to_edge clamp_to_edge\nusage tsnormal\ncolor_space linear\n')
    (root/'geometry-report.json').write_text(json.dumps({'version':'0.13.0','models':reports,'texture_size':[2048,256],
        'shared_textures':True,'native_materials':['eut2.dif.spec.tsnmap','eut2.dif.spec','eut2.glass','eut2.dif.spec.add.env','eut2.dif'],
        'mounting_origins_and_optical_angles':'preserved','rear_cable':True,
        'fixings':{'each_side_flange':4,'rear_wall_plate':2,'front':'behind-grille backing bridge with two strap fixings'},
        'hardware_style':'round low head with recessed hex socket; rounded 32-side pivot washer',
        'cable_passages':'rear central drilled plate and side flange passages, rubber grommets',
        'normal_map_gradient_strength':.020,'sun_specular_optics':True,'housing_and_optics_environment_reflection':False,
        'optical_cavity':'separate eut2.dif material, diffuse .5, zero specular, no NM/environment',
        'front_flats':'constant section through to fixing, full 89 mm length; no necks',
        'front_mount_profiles':FRONT_PROFILES,'side_labels_removed':True,'box_housing_closed_optical_pocket':True,
        'front_bridge_bearing_feet':'Z=.0264-.09*X-.005*Y in approved local SCS metres',
        'front_bridge_rear_bar':'Z=.0435-.125*X-.005*Y; two bearing levels for thin and deep grilles',
        'IR_layout':'two staggered rectangular banks, 18 diodes',
        'uv':'padded swatches, local box projection; no unique complex unwrap','dimension_source':'existing approved reconstruction; not OEM drawings'},indent=2)+'\n')
    print('REFINEMENT_PASS',reports)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('output','addon'):parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args(sys.argv[sys.argv.index('--')+1:]);build(a.output,a.addon.resolve())
