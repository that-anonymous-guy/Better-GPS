"""Trace the known T680 camera-lens atlas region through all exported variants.

Consumes AssetInspector --mesh-audit --all-truck=kenworth.t680_2022 output.
This is a positive lens-usage check, not a generic camera detector. Other
materials and shapes still require visual inspection.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

TEXTURE = '/vehicle/truck/kenworth_t680_2022/cabin_details_difspec.tobj'
REGION = (0, 0, .25, .52)  # native UVs; round photographic lens, top-left atlas


def scan(root):
    audit = json.loads((root / 'mesh-audit.json').read_text(encoding='utf-8-sig'))
    target_materials = set()
    for name, material in audit['materials'].items():
        match = re.search(r'texture\s*:\s*"texture_base"\s*\{.*?source\s*:\s*"([^"]+)"', material['text'], re.S)
        if match and match[1] == TEXTURE:
            target_materials.add(name)
    hits, atlas_users = [], []
    versions = {}
    for model in audit['models']:
        data = (root / 'extracted' / model['geometry'].lstrip('/')).read_bytes()
        assert hashlib.sha256(data).hexdigest().upper() == model['meshSha256']
        versions[data[:4].hex()] = versions.get(data[:4].hex(), 0) + 1
        if not target_materials.intersection(model['materials']):
            continue
        assert data[:4] == b'\x15gmP', model['geometry']
        atlas_users.append(model['path'])
        def u32(offset):
            return struct.unpack_from('<I', data, offset)[0]
        for piece in range(u32(4)):
            at = u32(84) + piece * 100
            if model['materials'][u32(at+16)] not in target_materials:
                continue
            count, stride, position, uv = u32(at+4), u32(at+60), u32(at+64), u32(at+72)
            if uv == 0xffffffff:
                continue
            coords = [struct.unpack_from('<2f', data, uv+i*stride) for i in range(count)]
            indices = struct.unpack_from('<'+'H'*u32(at), data, u32(at+96))
            chosen = []
            for tri in zip(indices[::3], indices[1::3], indices[2::3]):
                points = [coords[k] for k in tri]
                x, y = (sum(p[a] for p in points)/3 for a in range(2))
                area = abs((points[1][0]-points[0][0])*(points[2][1]-points[0][1])-(points[1][1]-points[0][1])*(points[2][0]-points[0][0]))
                if REGION[0] <= x <= REGION[2] and REGION[1] <= y <= REGION[3] and area > 1e-10:
                    chosen.append(tri)
            if chosen:
                used = sorted(set(i for tri in chosen for i in tri))
                points = [struct.unpack_from('<3f', data, position+i*stride) for i in used]
                hits.append(dict(model=model['path'], piece=piece, triangles=len(chosen), vertices=len(used),
                    bounds=[(min(p[a] for p in points), max(p[a] for p in points)) for a in range(3)]))
    core = [m for m in audit['models'] if '/vehicle/truck/kenworth_t680_2022/' in m['path']]
    result = dict(scope=audit['scope'], exportedModels=len(audit['models']), ownTruckModels=len(core),
        geometryVersions=versions, allGeometryHashesVerified=True, sourceErrors=audit['errors'],
        texture=TEXTURE, uvRegion=REGION, atlasUsers=atlas_users, lensHits=hits)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = scan(args.data)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(f"Verified {result['exportedModels']} PMG hashes; {len(result['lensHits'])} pieces use the camera-lens region.")
