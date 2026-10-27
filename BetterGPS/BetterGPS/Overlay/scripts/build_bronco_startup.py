"""Build Bronco assets using the existing Ford BC1/marker/UI pipeline."""
from pathlib import Path
from PIL import Image
import io, json, re, struct, sys

root = Path(__file__).resolve().parents[1]
assets = root / 'BetterGPS_assets'
source = Path(sys.argv[1])
template = (assets / 'material/ui/f150_startup/marker.tobj').read_bytes()

def sequence(folder, target, size, role, first=False):
    target = assets / 'material/ui' / target
    target.mkdir(parents=True, exist_ok=True)
    frames = sorted((source / folder).glob('*.png'))
    assert frames and len(frames) <= 300
    w, h = size
    unique, timeline, indices = [], [], {}
    for path in frames:
        image = Image.open(path).convert('RGBA')
        # Keep the established instrument texture geometry. The UI maps it
        # back to the Bronco's complete 1024x620 panel, without cropping.
        bg = Image.new('RGBA', image.size, (5,5,5,255))
        bg.alpha_composite(image)
        image = bg.convert('RGB').resize(size, Image.Resampling.LANCZOS)
        padded = Image.new('RGB', (w+4,h))
        padded.paste(image, (4,0))
        padded.paste(image.crop((0,0,1,h)).resize((4,h)), (0,0))
        data = io.BytesIO(); padded.save(data, format='DDS', pixel_format='DXT1')
        blocks = data.getvalue()[128:]
        index = indices.get(blocks)
        if index is None:
            index = len(unique); indices[blocks] = index; unique.append(blocks)
        timeline.append(index)
    # BGA2 keeps GPU-native BC1 blocks but stores repeated images once. The
    # timeline preserves the exact 30 fps cadence while repeated entries skip
    # redundant GPU uploads at runtime.
    with (target / 'animation.bga').open('wb') as out:
        out.write(struct.pack('<5I', 0x32534742, w+4, h, len(frames), len(unique)))
        out.write(struct.pack(f'<{len(timeline)}H', *timeline))
        for blocks in unique: out.write(blocks)
    marker = Image.new('RGBA', (w+4,h), (5,5,5,255))
    if first: marker.paste(Image.open(frames[0]).convert('RGBA').resize(size), (4,0))
    for i in range(4): marker.putpixel((i,0), (0x31+role+i,0x5b,0xa7,255))
    marker.save(target / 'marker.dds')
    name = ('/' + (target / 'marker.dds').relative_to(assets).as_posix()).encode()
    descriptor = bytearray(template[:48]); struct.pack_into('<I',descriptor,40,len(name)); descriptor.extend(name)
    (target / 'marker.tobj').write_bytes(descriptor)
    (target / 'marker.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "'+name.decode().replace('.dds','.tobj')+'" u_address : clamp v_address : clamp mip_filter : none } }\n')
    legacy = 16 + len(frames) * (w+4) * h // 2
    actual = (target / 'animation.bga').stat().st_size
    print(folder, len(frames), 'timeline frames,', len(unique), 'unique,', f'{actual/1048576:.2f} MiB ({100*actual/legacy:.1f}% of BGA1)')

sequence('welcome_centerstack','bronco_startup',(1280,800),10,True)
sequence('welcome_cluster','bronco_info_startup',(1440,540),11,True)

inventory=json.loads((root.parent/'validation/gps-screen-inventory/official-candidates.json').read_text())
native=next(c['text'] for c in inventory['candidates'] if c['path']=='/ui/dashboard/ford_bronco_gps_24.sii')
f150=(root/'layouts/horizontal/f150.sii').read_text()
def block(text,name):
    return re.search(r'ui::\w+ : '+re.escape(name)+r' \{[^}]+\}',text).group()
def children(node,names):
    return re.sub(r' my_children: \d+\n(?: my_children\[\d+\]: [^\n]+\n)*', ' my_children: '+str(len(names))+'\n'+''.join(f' my_children[{i}]: {v}\n' for i,v in enumerate(names)),node)
ui=native
# Preserve the stock navigation page as Nav2, with its own native page ID.
nav=block(ui,'_nameless._.display1')
nav2=nav.replace('_nameless._.display1','_nameless._.nav2').replace('id: 100','id: 200')
for index,name in enumerate(re.findall(r'my_children\[\d+\]: (\S+)',nav)):
    node=block(ui,name)
    # SCS unit tokens do not accept arbitrary suffixes after the stock
    # `_nameless._.*` names. Use compact, valid BetterGPS-owned tokens.
    copy=f'_nameless.bettergps.bn{index}'
    nav2+='\n\n'+node.replace(name,copy).replace('_nameless._.display1','_nameless._.nav2').replace('>Trips</font>','>Nav2</font>')
    nav2=nav2.replace(': '+name+'\n', ': '+copy+'\n')
for page in (2,3,4):
    old=block(ui,f'_nameless._.display{page}')
    ui=ui.replace(old,re.sub(r' id: \d+',f' id: {(page+1)*100}',old))
window=block(ui,'_nameless.wnd')
names=re.findall(r'my_children\[\d+\]: (\S+)',window)
names[0:0]=['_nameless.bettergps.samplebg','_nameless.bettergps.samplemap']
names.insert(names.index('_nameless._.display1')+1,'_nameless._.nav2')
names.append('_nameless.bettergps.f150startup')
ui=ui.replace(window,children(window,names))
page=block(ui,'_nameless._.display1')
names=re.findall(r'my_children\[\d+\]: (\S+)',page)+['_nameless.bettergps.nativemap','_nameless.bettergps.horizontal']
ui=ui.replace(page,children(page,names))
ui=ui[:ui.rfind('}')]+nav2+'\n'+ui[ui.rfind('}'):]
# Sampling markers must draw before the stock page backgrounds.
ui=re.sub(r' layer: (\d+)',lambda m:' layer: '+str(int(m[1])+2),ui)
# CarPlay occupies layer 8. Keep the Bronco's stock clock and ambient
# temperature visible above it, as they are on every other center-stack page.
for shared in ('_nameless._.clock','_nameless._.ambient_temp'):
    original=block(ui,shared)
    ui=ui.replace(original,re.sub(r' layer: \d+', ' layer: 9', original))
nodes='\n\n'.join(block(f150,'_nameless.bettergps.'+n) for n in ['samplebg','samplemap','nativemap','horizontal','f150startup'])
nodes=nodes.replace('f150_infotainment','bronco_infotainment').replace('f150_startup','bronco_startup')
ui=ui[:ui.rfind('}')]+nodes+'\n'+ui[ui.rfind('}'):]
(root/'layouts/horizontal/bronco.sii').write_text(ui)
(assets/'ui/dashboard/ford_bronco_gps_24.sii').write_text(ui)
cluster=(root.parent/'validation/bronco-dimensions/ui/dashboard/ford_bronco_24.sii').read_text()
window=block(cluster,'_nameless.wnd')
names=re.findall(r'my_children\[\d+\]: (\S+)',window)+['_nameless.bettergps.f150info']
cluster=cluster.replace(window,children(window,names))
node=block((assets/'ui/dashboard/ford_f150_23.sii').read_text(),'_nameless.bettergps.f150info').replace('f150_info_startup','bronco_info_startup').replace('coords_r: 1440','coords_r: 1024').replace('coords_t: 540','coords_t: 620')
cluster=cluster[:cluster.rfind('}')]+node+'\n'+cluster[cluster.rfind('}'):]
(assets/'ui/dashboard/ford_bronco_24.sii').write_text(cluster)
