"""Offline PMG v21 visual audit in Blender; approximate shaders, no game writes.

Run after AssetInspector --camera-inventory then --mesh-audit. This is an
inspection of selected model parts, not an assembled/variant-correct truck.
Glass and shadow-only geometry are excluded to expose hardware. Bone animation,
SCS shaders, variant visibility and actual player accessory selection are not
reproduced; therefore these images cannot prove absence on a truck family.
"""
import argparse
import json
import re
import struct
import sys
from pathlib import Path

if '--decode-textures' not in sys.argv:
    import bpy
    from mathutils import Vector


def scs_to_blender(point):
    return point[0], -point[2], point[1]


def material(path, audit, extracted, cache):
    if path in cache:
        return cache[path]
    result = bpy.data.materials.new(path)
    result.use_nodes = True
    nodes = result.node_tree.nodes
    nodes.clear()
    output = nodes.new('ShaderNodeOutputMaterial')
    shader = nodes.new('ShaderNodeBsdfPrincipled')
    result.node_tree.links.new(shader.outputs['BSDF'], output.inputs[0])
    shader.inputs['Roughness'].default_value = .7
    shader.inputs['Specular IOR Level'].default_value = .05
    text = audit['materials'].get(path, {}).get('text', '')
    color = re.search(r'diffuse\s*:\s*\{([^}]+)', text)
    rgb = tuple(float(v) for v in color[1].split(',')) if color else (.5,) * 3
    shader.inputs['Base Color'].default_value = (*rgb, 1)
    texture = re.search(r'texture\s*:\s*"texture_base"\s*\{.*?source\s*:\s*"([^"]+)"', text, re.S)
    if texture:
        image = extracted / (texture[1].lstrip('/') + '.dds.png')
        if image.exists():
            node = nodes.new('ShaderNodeTexImage')
            node.image = bpy.data.images.load(str(image), check_existing=True)
            result.node_tree.links.new(node.outputs['Color'], shader.inputs['Base Color'])
    cache[path] = result
    return result


def load_mesh(row, audit, extracted, cache):
    data = (extracted / row['geometry'].lstrip('/')).read_bytes()
    if data[:4] != b'\x15gmP':
        raise ValueError('Only PMG v21 is supported')
    def u32(offset):
        return struct.unpack_from('<I', data, offset)[0]
    for piece in range(u32(4)):
        at = u32(84) + piece * 100
        count, stride, position, uv = u32(at + 4), u32(at + 60), u32(at + 64), u32(at + 72)
        path = row['materials'][u32(at + 16)]
        text = audit['materials'][path]['text'].lower()
        if 'shadowonly' in text or ('glass' in text and 'frit' not in text):
            continue
        points = [scs_to_blender(struct.unpack_from('<3f', data, position + v * stride)) for v in range(count)]
        indices = struct.unpack_from('<' + 'H' * u32(at), data, u32(at + 96))
        faces = list(zip(indices[::3], indices[1::3], indices[2::3]))
        mesh = bpy.data.meshes.new(str(piece))
        mesh.from_pydata(points, [], faces)
        mesh.materials.append(material(path, audit, extracted, cache))
        mesh.update()
        obj = bpy.data.objects.new(str(piece), mesh)
        bpy.context.collection.objects.link(obj)
        layer = mesh.uv_layers.new()
        for loop in mesh.loops:
            x, y = struct.unpack_from('<2f', data, uv + loop.vertex_index * stride) if uv != 0xffffffff else (0, 0)
            layer.data[loop.index].uv = x, 1-y


def decode_textures(root):
    # Run with ordinary Python + Pillow, before starting Blender.
    from io import BytesIO
    from PIL import Image
    failures = []
    for source in root.rglob('*.dds'):
        output = Path(str(source) + '.png')
        if output.exists():
            continue
        try:
            data = bytearray(source.read_bytes())
            if data[84:88] == b'DX10':
                value = struct.unpack_from('<I', data, 128)[0]
                # Pillow lacks the sRGB aliases; block data stays identical.
                linear = {29: 28, 72: 71, 75: 74, 78: 77, 99: 98}.get(value, value)
                struct.pack_into('<I', data, 128, linear)
            Image.open(BytesIO(data)).convert('RGBA').save(output)
        except Exception as error:
            failures.append((str(source), str(error)))
    print('Unconverted textures:', failures)


def run(args):
    root = args.data.resolve()
    audit = json.loads((root / 'mesh-audit.json').read_text(encoding='utf-8-sig'))
    extracted = root / 'extracted'
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    cache = {}
    for row in audit['models']:
        if args.truck and args.truck != row['truck']:
            continue
        interior = '/interior/' in row['path'] and '/external/' not in row['path']
        if args.view in ('inside', 'front') and not interior:
            continue
        if args.view == 'hardware' and not (row['path'].endswith('/mirror/camera_chrome.pmd') or row['path'].endswith('/mirror/camera_only.pmd') or row['path'].endswith('/cam_chrome_dc.pmd')):
            continue
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete(use_global=False)
        load_mesh(row, audit, extracted, cache)
        scene = bpy.context.scene
        scene.render.engine = 'CYCLES'
        scene.cycles.samples = 8
        scene.cycles.use_denoising = True
        scene.world.color = (.5,) * 3
        scene.view_settings.view_transform = 'Standard'
        scene.view_settings.look = 'None'
        scene.render.resolution_x, scene.render.resolution_y = 1000, 700
        scene.render.resolution_percentage = 100
        camera_data = bpy.data.cameras.new('audit')
        camera = bpy.data.objects.new('audit', camera_data)
        bpy.context.collection.objects.link(camera)
        scene.camera = camera
        camera_data.clip_start = .01
        if args.view == 'inside':
            location, target, lens = (.52, -.15, -.05), (.52, .16, -1.1), 25
        elif args.view == 'front':
            location, target, lens = (.52, .15, -2), (.52, .15, -.85), 40
        elif row['truck'] == 'kenworth.t680_2022':
            location, target, lens = (-2, -.4, .8), (-1.3, .02, .05), 65
        else:
            target = (-1.4, 1, .52) if row['truck'].startswith('freightliner') else (-1.4, .62, .45)
            location, lens = (target[0]-.35, target[1]-.15, target[2]+.6), 55
        camera.location = scs_to_blender(location)
        camera.rotation_euler = (Vector(scs_to_blender(target))-camera.location).to_track_quat('-Z', 'Y').to_euler()
        camera_data.lens = lens
        light = bpy.data.lights.new('fill', 'AREA')
        light.energy, light.size = 100, 2
        obj = bpy.data.objects.new('fill', light)
        bpy.context.collection.objects.link(obj)
        obj.location, obj.rotation_euler = camera.location, camera.rotation_euler
        scene.render.filepath = str(output / (row['truck']+'_'+Path(row['path']).stem+'_'+args.view+'.png'))
        bpy.ops.render.render(write_still=True)
        print('AUDIT_RENDER', scene.render.filepath, flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True, help='AssetInspector mesh output directory')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--truck')
    parser.add_argument('--view', choices=['inside', 'front', 'hardware'], default='inside')
    parser.add_argument('--decode-textures', action='store_true')
    arguments = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else sys.argv[1:])
    if arguments.decode_textures:
        decode_textures(arguments.data / 'extracted')
    else:
        run(arguments)
