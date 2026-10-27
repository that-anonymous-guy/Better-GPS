"""Prepare the camera's UVs, paint layers and verified Blender painting project.

Run uv/finish/verify inside Blender, templates with repository Python (Pillow).
The original reconstruction is read only. Outputs belong to texture-kit/.
"""
from pathlib import Path
import argparse
import colorsys
import hashlib
import json
import math
import sys

SIZE = 4096
UV_NAME = 'PP804395_ATLAS'
TILES = {
    'camera_housing': (0, 0, 2048, 2048, '01 Boitier'),
    'sun_shield_top': (2048, 0, 2048, 1024, '02 Casquette'),
    'mount_base': (2048, 1024, 2048, 1024, '03 Base support'),
    'front_face_inset': (0, 2048, 1536, 1024, '04 Facade'),
    'pivot_plate_left': (1536, 2048, 768, 768, '05 Pivot gauche'),
    'pivot_plate_right': (2304, 2048, 768, 768, '06 Pivot droit'),
    'lens_bezel': (3072, 2048, 512, 512, '07 Bague objectif'),
    'camera_lens': (3584, 2048, 512, 512, '08 Verre objectif'),
    'weatherproof_connector': (3072, 2560, 512, 512, '09 Connecteur'),
    'sun_shield_left': (0, 3072, 1024, 512, '10 Bord gauche'),
    'sun_shield_right': (1024, 3072, 1024, 512, '11 Bord droit'),
    'bracket_left': (2048, 2816, 512, 768, '12 Montant gauche'),
    'bracket_right': (2560, 2816, 512, 768, '13 Montant droit'),
}
for i, name in enumerate(('mounting_slot', 'mounting_slot.001', 'mounting_slot.002',
                          'pivot_screw_left', 'pivot_screw_right')):
    TILES[name] = (512 * i, 3584, 512, 512, f'{14+i:02} {name}')
for i, name in enumerate(f'ir_led_{side}_{n}' for side in (-1, 1) for n in range(1, 10)):
    TILES[name] = (3072 + 204 * (i % 5), 3072 + 256 * (i // 5), 204, 256,
                   f'{19+i:02} LED {i+1:02}')


def dump(path, obj):
    path.write_text(json.dumps(obj, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def uv_stage(source, output):
    import bpy
    import bmesh
    bpy.ops.wm.open_mainfile(filepath=str(source))
    bpy.context.preferences.filepaths.save_version = 0
    meshes = sorted((o for o in bpy.context.scene.objects if o.type == 'MESH'), key=lambda o: o.name)
    assert set(o.name for o in meshes) == set(TILES), 'Unexpected source parts; update the tile plan.'
    report = {'size': SIZE, 'uv_layer': UV_NAME, 'parts': []}
    for obj in meshes:
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        # Freeze bevels before UV authoring: templates match the final surfaces.
        for mod in list(obj.modifiers):
            bpy.ops.object.modifier_apply(modifier=mod.name)
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        # Clamped bevels on the very thin slot insets can leave coincident
        # vertices. Clean those geometric degeneracies before unwrapping.
        bm=bmesh.new();bm.from_mesh(obj.data)
        bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-8)
        bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=1e-8)
        bm.to_mesh(obj.data);bm.free();obj.data.update()
        while obj.data.uv_layers:
            obj.data.uv_layers.remove(obj.data.uv_layers[0])
        obj.data.uv_layers.new(name=UV_NAME)
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.035,
                                 area_weight=0, correct_aspect=True, scale_to_bounds=False)
        bpy.ops.object.mode_set(mode='OBJECT')
        x, y, w, h, label = TILES[obj.name]
        # Preserve UV aspect ratio when placing a part into its labeled tile.
        coords = obj.data.uv_layers.active.data
        lo = [min(v.uv[j] for v in coords) for j in (0, 1)]
        hi = [max(v.uv[j] for v in coords) for j in (0, 1)]
        border = min(32, min(w, h) * 0.075)
        scale = min((w-2*border)/(hi[0]-lo[0]), (h-2*border)/(hi[1]-lo[1]))
        used = [(hi[j]-lo[j])*scale for j in (0, 1)]
        for v in coords:
            v.uv = ((x+(w-used[0])/2+(v.uv.x-lo[0])*scale)/SIZE,
                    (SIZE-y-h+(h-used[1])/2+(v.uv.y-lo[1])*scale)/SIZE)
        obj.data.calc_loop_triangles()
        triangles = [[[float(coords[i].uv.x), float(coords[i].uv.y)] for i in tri.loops]
                     for tri in obj.data.loop_triangles]
        faces = [[[float(coords[i].uv.x), float(coords[i].uv.y)] for i in poly.loop_indices]
                 for poly in obj.data.polygons]
        mat = obj.data.materials[0]
        bsdf = mat.node_tree.nodes.get('Principled BSDF')
        report['parts'].append({'name': obj.name, 'label': label, 'tile_px_top_left': [x,y,w,h],
                                'material': mat.name, 'color_linear': list(mat.diffuse_color),
                                'roughness': bsdf.inputs['Roughness'].default_value,
                                'metallic': bsdf.inputs['Metallic'].default_value,
                                'triangles': triangles, 'faces': faces})
    output.mkdir(parents=True, exist_ok=True)
    dump(output/'uv_geometry.json', report)
    bpy.ops.wm.save_as_mainfile(filepath=str(output/'pp804395_texture_ready.blend'))
    print(f'UV PREPARED: {len(meshes)} objects with applied bevels, one atlas, unique part tiles.')


def templates_stage(output):
    from PIL import Image, ImageDraw, ImageFilter, ImageFont
    import io
    import zipfile
    from xml.etree.ElementTree import Element, SubElement, tostring

    report = json.loads((output/'uv_geometry.json').read_text(encoding='utf-8'))
    tex = output/'textures'
    guides = output/'templates'
    masks = guides/'masks'
    for folder in (tex, guides, masks):
        folder.mkdir(parents=True, exist_ok=True)
    wire = Image.new('RGBA', (SIZE,SIZE))
    wd = ImageDraw.Draw(wire)
    ids = Image.new('RGBA', (SIZE,SIZE))
    idd = ImageDraw.Draw(ids)
    base = Image.new('RGBA', (SIZE,SIZE), (24,26,28,255))
    rough = Image.new('L', (SIZE,SIZE), 128)
    metal = Image.new('L', (SIZE,SIZE), 0)
    labels = Image.new('RGBA', (SIZE,SIZE))
    ld = ImageDraw.Draw(labels)
    font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 25)
    mask_images = {}
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{SIZE}" height="{SIZE}" viewBox="0 0 {SIZE} {SIZE}">',
           '<g fill="none" stroke="#34e3d0" stroke-width="1">']
    palette = []
    for index, part in enumerate(report['parts']):
        color = tuple(round(c*255) for c in colorsys.hsv_to_rgb((index*0.61803398875)%1,0.60,0.95))+(255,)
        mask = Image.new('L', (SIZE,SIZE), 0)
        md = ImageDraw.Draw(mask)
        for face in part['faces']:
            pts = [(u*SIZE,(1-v)*SIZE) for u,v in face]
            md.polygon(pts,fill=255)
            idd.polygon(pts,fill=color)
            wd.line(pts+[pts[0]], fill=(52,227,208,230), width=1)
            svg.append('<polygon points="'+' '.join(f'{x:.3f},{y:.3f}' for x,y in pts)+'"/>')
        # 8px of bleed for painting; reserved tile borders keep parts separate.
        x,y,w,h = part['tile_px_top_left']
        bleed = Image.new('L',(SIZE,SIZE))
        bleed.paste(mask.crop((x,y,x+w,y+h)).filter(ImageFilter.MaxFilter(17)),(x,y))
        linear = part['color_linear'][:3]
        srgb = tuple(round(255*(12.92*c if c<=0.0031308 else 1.055*c**(1/2.4)-0.055)) for c in linear)+(255,)
        base.paste(srgb, mask=bleed)
        rough.paste(round(part['roughness']*255), mask=bleed)
        metal.paste(round(part['metallic']*255), mask=bleed)
        group = part['material']
        if group not in mask_images:
            mask_images[group] = Image.new('L', (SIZE,SIZE))
        from PIL import ImageChops
        mask_images[group] = ImageChops.lighter(mask_images[group],mask)
        x,y,w,h = part['tile_px_top_left']
        ld.rectangle((x+2,y+2,x+w-2,y+h-2),outline=color,width=3)
        ld.rectangle((x+6,y+6,x+w-6,y+40),fill=(12,18,25,225))
        ld.text((x+12,y+10),part['label'],font=font,fill=(255,255,255,255))
        palette.append({'part':part['name'],'label':part['label'],'id_rgb':list(color[:3]),'tile_px_top_left':[x,y,w,h]})
    svg.extend(['</g>','</svg>'])
    (guides/'pp804395_uv_wire.svg').write_text('\n'.join(svg),encoding='utf-8')
    wire.save(guides/'pp804395_uv_wire.png')
    ids.save(guides/'pp804395_part_ids.png')
    labels.save(guides/'pp804395_labels.png')
    Image.alpha_composite(Image.alpha_composite(ids,wire),labels).save(guides/'pp804395_uv_guide.png')
    base.save(tex/'pp804395_basecolor.png')
    rough.save(tex/'pp804395_roughness.png')
    metal.save(tex/'pp804395_metallic.png')
    for group, mask in mask_images.items():
        mask.save(masks/f'{group}.png')
    checker = Image.new('RGBA',(SIZE,SIZE))
    cd = ImageDraw.Draw(checker)
    for row in range(32):
        for col in range(32):
            x,y=col*128,row*128
            color=(235,235,235,255) if (row+col)%2 else (35,70,100,255)
            cd.rectangle((x,y,x+127,y+127),fill=color)
            cd.rectangle((x,y,x+127,y+127),outline=(225,105,45,255),width=2)
            cd.text((x+10,y+10),f'{col+1:02}/{row+1:02}',font=font,
                    fill=(10,10,10,255) if (row+col)%2 else (255,255,255,255))
    checker.save(guides/'pp804395_checker.png')
    # OpenRaster is an editable layer stack supported by Krita and GIMP.
    layers=[('REPERES - masquer avant export',labels,'hidden'),
            ('CONTOURS UV - masquer avant export',wire,'hidden'),
            ('ID PIECES - masquer avant export',ids,'hidden'),
            ('PEINTURE - vos details',Image.new('RGBA',(SIZE,SIZE)),'visible'),
            ('BASE COULEUR',base,'visible')]
    root=Element('image',{'w':str(SIZE),'h':str(SIZE),'name':'PP804395 peinture'})
    stack=SubElement(root,'stack')
    with zipfile.ZipFile(output/'pp804395_paint_template.ora','w',zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('mimetype','image/openraster',compress_type=zipfile.ZIP_STORED)
        for i,(name,img,visibility) in enumerate(layers):
            entry=f'data/layer{i:02}.png'
            buf=io.BytesIO(); img.save(buf,format='PNG')
            archive.writestr(entry,buf.getvalue())
            SubElement(stack,'layer',{'name':name,'src':entry,'opacity':'1.0','visibility':visibility,
                                     'composite-op':'svg:src-over','x':'0','y':'0'})
        archive.writestr('stack.xml',tostring(root,encoding='utf-8',xml_declaration=True))
        buf=io.BytesIO(); base.save(buf,format='PNG'); archive.writestr('mergedimage.png',buf.getvalue())
        thumb=base.copy(); thumb.thumbnail((256,256)); buf=io.BytesIO();thumb.save(buf,format='PNG')
        archive.writestr('Thumbnails/thumbnail.png',buf.getvalue())
    dump(guides/'part_palette.json',palette)
    print(f'TEMPLATES: {SIZE}px, wire, labels, {len(mask_images)} material masks, editable ORA and starter maps.')


def finish_stage(output):
    import bpy
    from mathutils import Vector
    bpy.ops.wm.open_mainfile(filepath=str(output/'pp804395_texture_ready.blend'))
    bpy.context.preferences.filepaths.save_version=0
    scene=bpy.context.scene
    meshes=[o for o in scene.objects if o.type=='MESH']
    def load(name, noncolor=False):
        image=bpy.data.images.load(str(output/name),check_existing=True)
        image.colorspace_settings.name='Non-Color' if noncolor else 'sRGB'
        return image
    base=load('textures/pp804395_basecolor.png')
    rough=load('textures/pp804395_roughness.png',True)
    metal=load('textures/pp804395_metallic.png',True)
    checker=load('templates/pp804395_checker.png')
    ao=bpy.data.images.new('PP804395_AO',SIZE,SIZE,alpha=False)
    ao.colorspace_settings.name='Non-Color'
    materials={o.data.materials[0] for o in meshes}
    for mat in materials:
        nodes=mat.node_tree.nodes; links=mat.node_tree.links
        bsdf=nodes.get('Principled BSDF')
        for name,img,input_name,x,y in (
                ('PAINT_BASECOLOR',base,'Base Color',-500,350),
                ('PAINT_ROUGHNESS',rough,'Roughness',-500,0),
                ('PAINT_METALLIC',metal,'Metallic',-500,-350)):
            node=nodes.new('ShaderNodeTexImage'); node.name=name;node.label=name
            node.image=img; node.location=(x,y)
            links.new(node.outputs['Color'],bsdf.inputs[input_name])
        target=nodes.new('ShaderNodeTexImage');target.name='AO_GUIDE';target.image=ao
        target.label='AO guide - optional, not multiplied into base color';target.location=(-800,-600)
        nodes.active=target
    bpy.ops.object.select_all(action='DESELECT')
    for obj in meshes: obj.select_set(True)
    bpy.context.view_layer.objects.active=meshes[0]
    scene.render.engine='CYCLES'
    scene.cycles.samples=16
    scene.render.bake.margin=8
    scene.render.bake.use_selected_to_active=False
    bpy.ops.object.bake(type='AO')
    ao.filepath_raw=str(output/'templates/pp804395_ao.png');ao.file_format='PNG';ao.save()
    scene.render.engine='BLENDER_EEVEE_NEXT'
    scene.render.resolution_x=1400;scene.render.resolution_y=1100
    for obj in scene.objects:
        if obj.type=='LIGHT': obj.data.energy=1.8 if obj.location.x>0 else 0.9
    camera=scene.camera
    for name,pos in [('front',(-0.22,-0.35,0.20)),('rear',(0.22,0.35,0.20))]:
        camera.location=pos
        camera.rotation_euler=(Vector((0,0,0.058))-camera.location).to_track_quat('-Z','Y').to_euler()
        for mat in materials: mat.node_tree.nodes['PAINT_BASECOLOR'].image=checker
        scene.render.filepath=str(output/f'preview_checker_{name}.png')
        bpy.ops.render.render(write_still=True)
    for mat in materials:
        mat.node_tree.nodes['PAINT_BASECOLOR'].image=base
        mat.node_tree.nodes.active=mat.node_tree.nodes['PAINT_BASECOLOR']
    camera.location=(-0.22,-0.35,0.20)
    camera.rotation_euler=(Vector((0,0,0.058))-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(output/'preview_basecolor.png');bpy.ops.render.render(write_still=True)
    # A clean selection lets the user enter edit mode and see all UVs immediately.
    for area in bpy.context.screen.areas:
        if area.type=='VIEW_3D':
            area.spaces.active.shading.type='MATERIAL'
            area.spaces.active.region_3d.view_distance=0.35
            area.spaces.active.region_3d.view_location=Vector((0,0,0.058))
    for img in (base,rough,metal,checker,ao): img.filepath=bpy.path.relpath(img.filepath,start=str(output))
    bpy.ops.wm.save_as_mainfile(filepath=str(output/'pp804395_texture_ready.blend'))
    bpy.ops.export_scene.gltf(filepath=str(output/'pp804395_texture_ready.glb'),export_format='GLB',
                             use_selection=True,export_cameras=False,export_lights=False)
    print('FINISHED: external editable images linked, AO baked, front/rear checker previews rendered, GLB exported.')


def verify_stage(output):
    import bpy
    from mathutils import Vector
    from mathutils.geometry import tessellate_polygon
    bpy.ops.wm.open_mainfile(filepath=str(output/'pp804395_texture_ready.blend'))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    tris=[]
    face_references=[]
    for obj in meshes:
        assert len(obj.data.uv_layers)==1 and obj.data.uv_layers.active.name==UV_NAME
        assert not obj.modifiers
        coords=obj.data.uv_layers.active.data
        if obj.name in ('camera_housing','sun_shield_top','mount_base','front_face_inset'):
            largest=max(p.area for p in obj.data.polygons)
            for polygon in obj.data.polygons:
                axis=max(range(3),key=lambda i:abs(polygon.normal[i]))
                if polygon.area<largest*0.25 or abs(polygon.normal[axis])<0.995:continue
                sign=1 if polygon.normal[axis]>0 else -1
                surface={(0,-1):'COTE -X',(0,1):'COTE +X',(1,-1):'AVANT -Y',
                         (1,1):'ARRIERE +Y',(2,-1):'DESSOUS -Z',(2,1):'DESSUS +Z'}[(axis,sign)]
                center=[sum(coords[i].uv[j] for i in polygon.loop_indices)/polygon.loop_total for j in (0,1)]
                face_references.append({'part':obj.name,'surface':surface,
                                        'center_px':[center[0]*SIZE,(1-center[1])*SIZE]})
        obj.data.calc_loop_triangles()
        for tri in obj.data.loop_triangles:
            pts=[tuple(coords[i].uv) for i in tri.loops]
            assert all(0<=u<=1 and 0<=v<=1 for u,v in pts)
            cross=lambda a,b,c:(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
            area=cross(*pts)
            assert abs(area)>1e-13, f'Degenerate UV on {obj.name}'
            if area<0: pts.reverse()
            tris.append((obj.name,pts))
    def overlap(a,b):
        poly=list(a)
        for e in range(3):
            p,q=b[e],b[(e+1)%3]
            def side(r): return (q[0]-p[0])*(r[1]-p[1])-(q[1]-p[1])*(r[0]-p[0])
            result=[]
            for i,start in enumerate(poly):
                end=poly[(i+1)%len(poly)];ds=side(start);de=side(end)
                if ds>=0: result.append(start)
                if (ds>0 and de<0) or (ds<0 and de>0):
                    t=ds/(ds-de);result.append((start[0]+t*(end[0]-start[0]),start[1]+t*(end[1]-start[1])))
            poly=result
            if len(poly)<3:return 0.0
        return abs(sum(poly[i][0]*poly[(i+1)%len(poly)][1]-poly[(i+1)%len(poly)][0]*poly[i][1] for i in range(len(poly))))/2
    buckets={};errors=[];tested=set()
    for index,(name,tri) in enumerate(tris):
        xmin,xmax=min(p[0] for p in tri),max(p[0] for p in tri)
        ymin,ymax=min(p[1] for p in tri),max(p[1] for p in tri)
        cells=[(x,y) for x in range(int(xmin*64),int(xmax*64)+1) for y in range(int(ymin*64),int(ymax*64)+1)]
        for cell in cells:
            for other in buckets.get(cell,[]):
                key=(other,index)
                if key in tested:continue
                tested.add(key)
                area=overlap(tri,tris[other][1])
                if area>1e-10:errors.append([name,tris[other][0],area])
            buckets.setdefault(cell,[]).append(index)
    assert not errors, f'UV overlaps: {errors[:8]}'
    assert len([o for o in meshes if o.name.startswith('ir_led_')])==18
    missing=[i.filepath for i in bpy.data.images if i.source=='FILE' and not Path(bpy.path.abspath(i.filepath)).is_file()]
    assert not missing,missing
    dump(output/'templates/face_references.json',face_references)
    dump(output/'validation.json',{'status':'PASS','objects':len(meshes),'triangles':len(tris),
         'uv_layer':UV_NAME,'atlas_size':SIZE,'overlapping_triangle_pairs':0,'degenerate_uv_triangles':0,
         'missing_image_files':missing,'source_preserved':True,
         'files_sha256':{str(p.relative_to(output)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in sorted(output.rglob('*')) if p.is_file() and p.name!='validation.json'}})
    print(f'PASS: {len(meshes)} objects, {len(tris)} triangles, no UV overlap/degeneracy, all external images resolve.')


def export_stage(output):
    """Retry only the portable export, without repeating UVs, bake or rendering."""
    import bpy
    bpy.ops.wm.open_mainfile(filepath=str(output/'pp804395_texture_ready.blend'))
    bpy.ops.object.select_all(action='DESELECT')
    for obj in bpy.context.scene.objects:
        if obj.type=='MESH':obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(output/'pp804395_texture_ready.glb'),export_format='GLB',
                             use_selection=True,export_cameras=False,export_lights=False)
    print('EXPORTED: portable GLB with UVs and embedded textures.')


def package_stage(output):
    """Inspect actual outputs and create a portable, CRC-checked delivery archive."""
    from PIL import Image, ImageDraw, ImageFont
    from xml.etree import ElementTree
    import io
    import struct
    import zipfile
    validation=json.loads((output/'validation.json').read_text(encoding='utf-8'))
    assert validation['status']=='PASS'
    # Additional aligned orientation overlay makes front/back paint placement
    # explicit without stamping any lettering into the user's base texture.
    overlay=Image.new('RGBA',(SIZE,SIZE));draw=ImageDraw.Draw(overlay)
    font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',32)
    for ref in json.loads((output/'templates/face_references.json').read_text(encoding='utf-8')):
        x,y=ref['center_px'];label=ref['surface']
        box=draw.textbbox((x,y),label,font=font,anchor='mm')
        draw.rectangle((box[0]-8,box[1]-5,box[2]+8,box[3]+5),fill=(10,14,20,220))
        draw.text((x,y),label,font=font,anchor='mm',fill=(255,255,255,255))
    overlay.save(output/'templates/pp804395_face_labels.png')
    with Image.open(output/'templates/pp804395_uv_guide.png') as guide:
        Image.alpha_composite(guide.convert('RGBA'),overlay).save(output/'templates/pp804395_uv_orientation.png')
    for folder in ('textures','templates'):
        for path in (output/folder).rglob('*.png'):
            with Image.open(path) as img:
                assert img.size==(SIZE,SIZE),path
                img.verify()
    with zipfile.ZipFile(output/'pp804395_paint_template.ora') as archive:
        assert archive.testzip() is None
        assert archive.read('mimetype')==b'image/openraster'
        tree=ElementTree.fromstring(archive.read('stack.xml'))
        assert tree.attrib['w']==str(SIZE) and tree.attrib['h']==str(SIZE)
        for layer in tree.findall('.//layer'):
            with Image.open(io.BytesIO(archive.read(layer.attrib['src']))) as img:
                assert img.size==(SIZE,SIZE)
        with Image.open(io.BytesIO(archive.read('mergedimage.png'))) as merged:
            with Image.open(output/'textures/pp804395_basecolor.png') as base:
                assert merged.convert('RGBA').tobytes()==base.convert('RGBA').tobytes()
    glb=(output/'pp804395_texture_ready.glb').read_bytes()
    magic,version,length=struct.unpack_from('<III',glb)
    assert magic==0x46546c67 and version==2 and length==len(glb)
    chunklen,chunktype=struct.unpack_from('<II',glb,12)
    assert chunktype==0x4e4f534a
    doc=json.loads(glb[20:20+chunklen])
    assert all('TEXCOORD_0' in p['attributes'] for m in doc['meshes'] for p in m['primitives'])
    # glTF packs roughness and metallic into two channels of one image.
    assert len(doc['images'])>=2
    assert all('bufferView' in image for image in doc['images'])
    for mat in doc['materials']:
        assert 'baseColorTexture' in mat['pbrMetallicRoughness']
        assert 'metallicRoughnessTexture' in mat['pbrMetallicRoughness']
    # Include all last-mile template outputs in the receipt before archiving.
    validation['files_sha256']={str(p.relative_to(output)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in sorted(output.rglob('*')) if p.is_file() and p.name!='validation.json'}
    dump(output/'validation.json',validation)
    archive_path=output.parent/'pp804395-texture-kit.zip'
    with zipfile.ZipFile(archive_path,'w',zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(output.rglob('*')):
            if path.is_file(): archive.write(path,'pp804395-texture-kit/'+path.relative_to(output).as_posix())
    with zipfile.ZipFile(archive_path) as archive:
        assert archive.testzip() is None
    print(f'PASS: PNG sizes, ORA layers/flattened image, GLB embedded UV textures; ZIP CRC verified: {archive_path}')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage',choices=('uv','templates','finish','export','verify','package'))
    parser.add_argument('--source',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else None)
    out=args.output.resolve()
    if args.stage=='uv':uv_stage(args.source.resolve(),out)
    elif args.stage=='templates':templates_stage(out)
    elif args.stage=='finish':finish_stage(out)
    elif args.stage=='export':export_stage(out)
    elif args.stage=='verify':verify_stage(out)
    else:package_stage(out)
