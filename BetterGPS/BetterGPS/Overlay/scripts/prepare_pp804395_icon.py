"""Fit the supplied camera DDS to the stock 256x64 accessory icon layout."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct

from PIL import Image


def prepare(source,reference_tobj,output):
    original=Image.open(source).convert('RGBA')
    assert original.width==original.height,'Expected the supplied square camera icon'
    # Stock icons occupy the left 64x64 tile; the rest of the 256x64 row is
    # transparent. Preserve the supplied artwork and its alpha, with no crop.
    canvas=Image.new('RGBA',(256,64))
    canvas.paste(original.resize((64,64),Image.Resampling.LANCZOS),(0,0))
    archive=output/'icon';archive.mkdir(parents=True,exist_ok=True)
    shutil.copy2(source,archive/'source-camera.dds')
    canvas.save(archive/'pp804395.png')
    target=output/'project/material/ui/accessory/bettergps'
    target.mkdir(parents=True,exist_ok=True)
    dds=target/'pp804395.dds';canvas.save(dds,format='DDS')
    assert Image.open(dds).convert('RGBA').tobytes()==canvas.tobytes()
    # Keep the stock native UI sampler flags (clamp, no mip filter). The only
    # variable field in this one-image TOBJ is its referenced path length.
    template=reference_tobj.read_bytes()
    assert len(template)==48+struct.unpack_from('<I',template,40)[0]
    header=bytearray(template[:48])
    path=b'/material/ui/accessory/bettergps/pp804395.dds'
    struct.pack_into('<I',header,40,len(path))
    (target/'pp804395.tobj').write_bytes(header+path)
    (target/'pp804395.mat').write_text('''effect : "ui.rfx" {
    texture : "texture" {
        source : "pp804395.tobj"
        u_address : clamp
        v_address : clamp
        mip_filter : none
    }
}
''',encoding='utf-8')
    report={'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'source_size':list(original.size),'output_size':list(canvas.size),
            'artwork_tile':[0,0,64,64],'format':'RGBA8 DDS, native UI TOBJ, ui.rfx',
            'dds_roundtrip_pixels_identical':True,'source_preserved':True}
    (archive/'import-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('ICON_PASS',dds,'256x64, original alpha preserved')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','reference-tobj','output'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();prepare(args.source,args.reference_tobj,args.output)
