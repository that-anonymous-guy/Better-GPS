"""Validate converted SCS camera finishes, NM data and runtime dependencies."""
import argparse,io,json,re,statistics,struct
from pathlib import Path
from PIL import Image


def run(mod,authoring):
    models=[('pp804395','pp804395_lod0'),('pp804396','pp804396_truck'),('pp804397','pp804397_truck'),('pp807032','pp807032_lod0')]
    if (mod/'vehicle/truck/upgrade/bettergps/pp807032_low/pp807032_low_lod0.pmd').exists():models.append(('pp807032_low','pp807032_low_lod0'))
    expected=['eut2.dif.spec.tsnmap.rfx','eut2.dif.spec.rfx','eut2.glass.rfx','eut2.dif.spec.add.env.rfx','eut2.dif.rfx']
    shared=None
    for part,name in models:
        data=(mod/f'vehicle/truck/upgrade/bettergps/{part}/{name}.pmd').read_bytes()
        refs=[p.decode() for p in re.findall(rb'/automat/[^\0]+\.mat',data)]
        box=part in ('pp804395','pp807032','pp807032_low')
        assert len(refs)==(5 if box else 4),(part,'material count',len(refs))
        common=[p for p in refs if '"eut2.dif.rfx"' not in (mod/p.lstrip('/')).read_text()]
        if shared is None:shared=common
        assert set(common)==set(shared),(part,'exterior materials must be shared')
        for index,path in enumerate(refs):
            text=(mod/path.lstrip('/')).read_text()
            effect=re.search(r'effect\s*:\s*"([^"]+)"',text).group(1)
            assert effect in expected,(part,index,effect)
            assert re.search(r'additional_ambient\s*:\s*0(?:\.0+)?\b',text)
            assert '.lum' not in effect and 'emission' not in text
            if effect in expected[:2]:assert 'texture_reflection' not in text and 'env_factor' not in text
            if effect==expected[0]:assert 'texture_nmap' in text and 'micro_normal.tobj' in text
            if effect==expected[4]:
                assert re.search(r'specular\s*:\s*\{\s*0\.0+\s*,\s*0\.0+\s*,\s*0\.0+\s*\}',text)
                assert 'texture_nmap' not in text and 'texture_reflection' not in text
                assert 'diffuse : { 0.214041' in text
            if effect==expected[2]:assert 'shininess : 95.000000' in text and 'fresnel : { 0.200000 , 0.900000 }' in text
            for reference in re.findall(r'source\s*:\s*"([^"]+)"',text):
                assert (mod/reference.lstrip('/')).is_file() or reference in ('/material/environment/vehicle_reflection.tobj','/material/environment/fuzzy_reflection/fuzzy_reflection.tobj'),reference
    textures=mod/'vehicle/truck/upgrade/bettergps/camera_finish'
    for name in ('palette','micro_normal'):
        data=(textures/(name+'.dds')).read_bytes()
        assert data[:4]==b'DDS '
        height,width=struct.unpack_from('<II',data,12)
        assert (width,height)==(2048,256),(name,width,height)
        assert struct.unpack_from('<I',data,28)[0]>1,(name,'missing mipmaps')
    source=authoring/'project/vehicle/truck/upgrade/bettergps/camera_finish'
    tobj=(source/'micro_normal.tobj').read_text()
    assert 'usage tsnormal' in tobj and 'color_space linear' in tobj
    normal=Image.open(source/'micro_normal.png').convert('RGBA')
    red=list(normal.crop((0,0,256,256)).tobytes()[0::4])
    deviation=statistics.pstdev(red)
    assert 1<deviation<8,('NM must have fine, non-flat surface relief',deviation)
    encoded=bytearray((textures/'palette.dds').read_bytes())
    # Pillow supports BC3_UNORM but not its sRGB tag. Decode the exact same
    # compressed pixels through UNORM for byte-level swatch inspection only.
    if encoded[84:88]==b'DX10' and struct.unpack_from('<I',encoded,128)[0]==78:struct.pack_into('<I',encoded,128,77)
    palette=Image.open(io.BytesIO(encoded)).convert('RGBA')
    averages=[]
    for i in range(8):
        crop=palette.crop((i*256+32,32,(i+1)*256-32,224))
        pixels=crop.tobytes()
        averages.append([round(statistics.mean(pixels[c::4]),2) for c in range(3)])
    assert max(averages[0])<35 and max(averages[2])<80 and max(averages[5])<20
    version=json.loads((authoring/'geometry-report.json').read_text())['version']
    result={'status':'PASS','version':version,'native_effects':expected,'exterior_material_count':4,'box_material_count':5,
            'cavity_zero_specular':True,'cavity_normal_map':False,'cavity_environment_reflection':False,
            'housing_and_optics_environment_reflection':False,'additional_ambient':0,'emissive_materials':0,
            'normal_map_dimensions':[2048,256],'normal_red_stddev':deviation,'mipmaps':True,
            'diffuse_swatches_rgb':averages,'runtime_texture_bytes':sum(p.stat().st_size for p in textures.iterdir() if p.is_file()),
            'preview_note':'Principled studio preview is not a faithful ATS renderer; final appearance requires in-game verification'}
    (authoring/'material-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print('MATERIALS_PASS: calibrated native shaders; dark non-specular cavity, linear NM, stock-style glass Fresnel and fuzzy metal reflection.')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('mod','authoring'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();run(a.mod.resolve(),a.authoring.resolve())
