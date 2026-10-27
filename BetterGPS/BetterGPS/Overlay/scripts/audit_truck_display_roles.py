"""Read-only role/physical-format audit of the extracted ATS screen inventory.

Unlike texture/UV aspect, physical aspect is measured in 3D, per connected
screen surface. No game/DLL/profile writes. Uses only the Python standard library.
"""
from pathlib import Path
import argparse, csv, json, math, re, struct
from inventory_truck_screens import NAMES

def dot(a,b): return sum(x*y for x,y in zip(a,b))
def sub(a,b): return [x-y for x,y in zip(a,b)]
def cross(a,b): return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def norm(a): return math.sqrt(dot(a,a))
def unit(a):
    length=norm(a)
    if length<1e-12: raise ValueError('Degenerate geometry')
    return [v/length for v in a]
def u32(b,p): return struct.unpack_from('<I',b,p)[0]

def affine_axes(points,uv):
    n=len(points);mp=[sum(p[a] for p in points)/n for a in range(3)]
    mu=[sum(p[a] for p in uv)/n for a in range(2)]
    u=[p[0]-mu[0] for p in uv];v=[p[1]-mu[1] for p in uv]
    uu,vv,uvv=dot(u,u),dot(v,v),dot(u,v);den=uu*vv-uvv*uvv
    if abs(den)<1e-14: raise ValueError('Degenerate UV mapping')
    pu=[sum(u[i]*(points[i][a]-mp[a]) for i in range(n)) for a in range(3)]
    pv=[sum(v[i]*(points[i][a]-mp[a]) for i in range(n)) for a in range(3)]
    du=[(pu[a]*vv-pv[a]*uvv)/den for a in range(3)]
    dv=[(pv[a]*uu-pu[a]*uvv)/den for a in range(3)]
    error=max(norm([mp[a]+u[i]*du[a]+v[i]*dv[a]-points[i][a] for a in range(3)]) for i in range(n))
    return du,dv,error

def hull(points):
    points=sorted(set(points))
    def turn(a,b,c):return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    low=[];high=[]
    for point in points:
        while len(low)>1 and turn(low[-2],low[-1],point)<=1e-14:low.pop()
        low.append(point)
    for point in reversed(points):
        while len(high)>1 and turn(high[-2],high[-1],point)<=1e-14:high.pop()
        high.append(point)
    return low[:-1]+high[:-1]

def physical_region(points,uv,triangles):
    # UV fitting orients width versus height; the rectangle itself uses XYZ.
    du,dv,error=affine_axes(points,uv)
    normals=[cross(sub(points[b],points[a]),sub(points[c],points[a])) for a,b,c in triangles]
    normal=unit(max(normals,key=norm));axis=unit(sub(du,[dot(du,normal)*x for x in normal]))
    vertical=unit(cross(normal,axis));origin=points[0]
    projected=[(dot(sub(p,origin),axis),dot(sub(p,origin),vertical)) for p in points]
    outline=hull(projected);best=None
    for a,b in zip(outline,outline[1:]+outline[:1]):
        edge=unit(sub(b,a));perp=[-edge[1],edge[0]]
        spans=[max(dot(p,d) for p in outline)-min(dot(p,d) for p in outline) for d in (edge,perp)]
        if abs(edge[0])<abs(perp[0]):spans.reverse()
        area=spans[0]*spans[1]
        if best is None or area<best[0]:best=(area,*spans)
    _,width,height=best
    return dict(widthMm=width*1000,heightMm=height*1000,physicalAspect=width/height,
      planeErrorMm=max(abs(dot(sub(p,origin),normal)) for p in points)*1000,
      uvFitErrorMm=error*1000,center=[sum(p[a] for p in points)/len(points) for a in range(3)],
      uDirection=unit(du),vDirection=unit(dv))

def components(data,piece,tile):
    at=u32(data,84)+100*piece;stride,pos,_,uv=struct.unpack_from('<4I',data,at+60)
    count=u32(data,at+4)
    xyz=[struct.unpack_from('<3f',data,pos+i*stride) for i in range(count)]
    tex=[struct.unpack_from('<2f',data,uv+i*stride) for i in range(count)]
    indices=struct.unpack_from('<'+'H'*u32(data,at),data,u32(data,at+96))
    triangles=[]
    for n in range(0,len(indices)-2,3):
        tri=indices[n:n+3]
        if tuple(math.floor(sum(tex[v][a] for v in tri)/3+1e-7) for a in range(2))==tuple(tile):triangles.append(tri)
    # Weld coincident positions, not merely index IDs (native seams duplicate vertices).
    key=lambda i:tuple(round(v,5) for v in xyz[i])
    owners={};parent=list(range(len(triangles)))
    def find(i):
        while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
        return i
    for i,tri in enumerate(triangles):
        for v in tri:
            k=key(v)
            if k in owners:parent[find(i)]=find(owners[k])
            else:owners[k]=i
    groups={}
    for i,tri in enumerate(triangles):groups.setdefault(find(i),[]).append(tri)
    for j,group in enumerate(groups.values()):
        used=sorted(set(v for tri in group for v in tri));mapping={v:i for i,v in enumerate(used)}
        p=[xyz[v] for v in used];t=[tex[v] for v in used]
        try:result=physical_region(p,t,[tuple(mapping[v] for v in tri) for tri in group])
        except ValueError:continue
        result.update(component=j,vertices=len(used),triangles=len(group),
          uv=[min(v[0] for v in t),min(v[1] for v in t),max(v[0] for v in t),max(v[1] for v in t)])
        yield result

def run(stock,previous,out):
    source=json.loads((previous/'screens-full.json').read_text())['screens'];extracted=stock/'extracted'
    out.mkdir(parents=True,exist_ok=True);audited=[];seen=set()
    for row in source:
        if row['regionType']!='dynamic screen':continue
        key=(row['truck'],row['model'],row['piece'],tuple(row['tile']),row['ui'],tuple(row['render']))
        if key in seen:continue
        seen.add(key)
        ui=(extracted/row['ui'].lstrip('/')).read_text(encoding='utf-8-sig')
        gps=row['texture']=='/vehicle/truck/share/gps.tobj'
        evidence=dict(texture=row['texture'],ui=row['ui'],definition=row['definition'],
          mapContent=bool(re.search(r'bare_map|ui::map\b|infotainment',ui,re.I)),
          gaugeContent=bool(re.search(r'rpm|odometer|gear_|fuel',ui,re.I)))
        data=(extracted/Path(row['model']).with_suffix('.pmg').as_posix().lstrip('/')).read_bytes()
        for region in components(data,row['piece'],row['tile']):
            left,top,right,bottom=region['uv'];rw,rh=row['render']
            # The dashboard producer also drives radio/climate/sleeper LCDs.
            # Keep the producer identity separate from spatial role. On these
            # stock models X=0 is the driver's instrument stack; the actual
            # coordinates remain available to review this spatial inference.
            identity='infotainment/GPS' if gps else 'cluster' if abs(region['center'][0])<.2 else 'dashboard auxiliaire'
            audited.append(dict(truck=row['truck'],role=identity,producer='GPS' if gps else 'dashboard',model=row['model'],piece=row['piece'],tile=row['tile'],
               render=row['render'],layout=row['uiSize'],exposed=[(right-left)*rw,(bottom-top)*rh],
               evidence=evidence,**region))
    unique={}
    for row in audited:
        key=(row['truck'],row['role'],row['evidence']['ui'],tuple(round(v,1) for v in (row['widthMm'],row['heightMm'])),
             tuple(round(v,1) for v in row['center']),tuple(row['render']))
        if key not in unique:unique[key]=dict(row,models=[])
        if row['model'] not in unique[key]['models']:unique[key]['models'].append(row['model'])
    rows=list(unique.values())
    (out/'role-audit-full.json').write_text(json.dumps(audited,indent=2),encoding='utf-8')
    (out/'role-audit.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
    with (out/'role-audit.csv').open('w',newline='',encoding='utf-8-sig') as f:
        writer=csv.writer(f);writer.writerow(['Truck','Role','Mesh width mm','Mesh height mm','Physical aspect','Native render','UV exposure','Model','Piece','Component','Center X m','UI','Material texture','Plane error mm','UV fit error mm'])
        for r in rows:writer.writerow([r['truck'],r['role'],r['widthMm'],r['heightMm'],r['physicalAspect'],r['render'],r['exposed'],r['model'],r['piece'],r['component'],r['center'][0],r['evidence']['ui'],r['evidence']['texture'],r['planeErrorMm'],r['uvFitErrorMm']])
    for row in rows:
        print(f"{row['truck']:27} {row['role']:16} {row['widthMm']:7.2f} x {row['heightMm']:7.2f} mm {row['physicalAspect']:.5f}:1 X={row['center'][0]:+.3f} piece={row['piece']} component={row['component']} plane={row['planeErrorMm']:.3f}mm")
    print(f'{len(audited)} measured surfaces; {len(rows)} grouped records; {out}')
    (out/'truck-display-formats.md').write_text(markdown(rows),encoding='utf-8')
    return rows

def markdown(rows):
    def name(r):
        label=NAMES.get(r['truck'],r['truck'])
        if r['truck'].startswith('freightliner.'):
            label+=' — numérique' if r['render'][0]==1024 else ' — analogique'
        return label
    def numbers(r):
        physical=f"≈ {r['widthMm']:.2f} × {r['heightMm']:.2f}"
        aspect=f"≈ {r['physicalAspect']:.3f}:1"
        if r['planeErrorMm']>.5:aspect+=' †'
        return f"{physical} | {aspect} | {r['render'][0]} × {r['render'][1]} | {r['exposed'][0]:.2f} × {r['exposed'][1]:.2f}"
    lines=['# Écrans ATS : infotainment/GPS, cluster et afficheurs auxiliaires','',
      'Audit corrigé du 20 octobre 2026, ATS 1.61.3.18s, archives officielles installées, mods exclus. Les 19 familles et 83 modèles intérieurs du relevé ont été recontrôlés. Aucun DLL, réglage ni asset installé n’est modifié par cet audit.','',
      '## Correction du relevé précédent','',
      '**Ne pas utiliser le ratio UV comme format physique d’un nouvel écran.** Le relevé précédent donnait les dimensions exposées dans le buffer, pas les proportions du panneau 3D. Sur le T680 nouvelle génération, le GPS avait bien été trouvé à droite du cluster, mais le ratio interne 2:1 avait été présenté de manière trompeuse comme son format.','',
      '| Écran vérifié | Maillage | Position latérale X | Dimensions physiques du modèle | Format physique | Buffer natif déclaré |',
      '| --- | --- | ---: | ---: | ---: | ---: |']
    for truck,role in [('kenworth.t680_2022','infotainment/GPS'),('kenworth.t680_2022','cluster'),('freightliner.cascadia2019','infotainment/GPS'),('freightliner.cascadia2019','cluster'),('freightliner.cascadia2024','infotainment/GPS'),('freightliner.cascadia2024','cluster')]:
        r=next(r for r in rows if r['truck']==truck and r['role']==role and r['render'][0]==1024)
        lines.append(f"| {name(r)} — {role} | pièce {r['piece']} | {r['center'][0]:+.3f} m | {r['widthMm']:.2f} × {r['heightMm']:.2f} mm | {r['physicalAspect']:.3f}:1 | {r['render'][0]} × {r['render'][1]} |")
    lines+=['','Les positions sont dans le repère du modèle intérieur (X≈0 pour les instruments devant le conducteur, X positif pour le centre du dashboard). Il ne s’agit pas de coordonnées écran de clic.','',
      '- **T680 nouvelle génération** : GPS pièce 38, `/vehicle/truck/share/gps.tobj`, `/ui/dashboard/kenworth_t680_2022_gps.sii` (UI 1280×720, fond `infotainment.mat`). Cluster pièce 39, `/vehicle/truck/share/dashboard.tobj`, `/ui/dashboard/kenworth_t680_2022.sii` (UI 1440×540). Modèle : `/vehicle/truck/kenworth_t680_2022/interior/model/day.pmd`.',
      '- **Cascadia 2019 numérique** : GPS pièce 49, UI `freightliner_cascadia_icc5_gps.sii` ; cluster pièce 45, UI `freightliner_cascadia_icc5.sii`. Modèle `freightliner_cascadia2019/interior/model/day_d.pmd`.',
      '- **Cascadia 2024 numérique** : GPS pièce 60, UI `freightliner_cascadia_2024_gps.sii` ; cluster pièce 56, UI `freightliner_cascadia_2024.sii`. Modèle `freightliner_cascadia2024/interior/model/day_d.pmd`.',
      '- Les deux Cascadia numériques ont réellement un GPS très large (≈2,660:1), proche du cluster (≈2,667:1). Leur buffer commun de format 1024×512 ne prouve ni un écran identique ni un format physique 2:1.','',
      '## GPS / infotainment : table corrigée','',
      'Les millimètres viennent du maillage du jeu, pas d’une fiche constructeur. Le buffer est `gps_drawable_size` (256×256 lorsque non précisé). La dernière colonne conserve la convention UV du relevé initial, pour comparaison. **Pour préparer le format visuel, lire la colonne physique ; la conversion UV reste nécessaire au placement.**','',
      '| Camion / variante | Surface physique du modèle (mm) | Format physique | Buffer natif | Zone UV exposée (px) |',
      '| --- | ---: | ---: | ---: | ---: |']
    gps=[];seen=set()
    for r in rows:
        if r['role']!='infotainment/GPS':continue
        key=(r['truck'],r['evidence']['ui'],round(r['physicalAspect'],2),tuple(r['render']))
        if key in seen:continue
        seen.add(key);gps.append(r)
        lines.append(f"| {name(r)} | {numbers(r)} |")
    lines+=['| International 9900i | Aucun maillage GPS intégré trouvé dans les trois intérieurs | — | — | — |','',
      '† Les surfaces Peterbilt 389/579 ne sont pas parfaitement planes ; le ratio donné est celui du rectangle projeté d’encombrement. Il reste approximatif et ne remplace pas un ajustement visuel sur ces modèles.','',
      '## Cluster / instruments devant le conducteur','',
      'Les surfaces déconnectées sont mesurées séparément, même si elles utilisent la même texture et le même buffer. Un atlas de plusieurs afficheurs n’est pas un grand écran unique. Les petites surfaces pilotées par le dashboard mais situées ailleurs sont rangées dans la section suivante.','',
      '| Camion / variante | Surface physique du modèle (mm) | Format physique | Buffer dashboard | Zone UV de cette surface (px) |',
      '| --- | ---: | ---: | ---: | ---: |']
    for r in rows:
        if r['role']=='cluster':lines.append(f"| {name(r)} — pièce {r['piece']}, surface {r['component']} | {numbers(r)} |")
    lines+=['','## Autres afficheurs pilotés par le dashboard','',
      'L’identité du producteur est certaine (`dashboard_path` / `dashboard.tobj`). La distinction spatiale cluster/auxiliaire est fondée sur la position des composants dans les modèles inspectés, pas sur le mot `dashboard` dans le nom. Cette section ne suppose pas que ces afficheurs sont des GPS ; leur fonction exacte radio/climatisation/horloge peut demander un contrôle visuel en jeu.','',
      '| Camion | Position X | Surface physique du modèle (mm) | Format physique |',
      '| --- | ---: | ---: | ---: |']
    for r in rows:
        if r['role']=='dashboard auxiliaire':lines.append(f"| {name(r)} — pièce {r['piece']}, surface {r['component']} | {r['center'][0]:+.3f} m | {r['widthMm']:.2f} × {r['heightMm']:.2f} | {r['physicalAspect']:.3f}:1 |")
    lines+=['','## Méthode et limites','',
      '1. UI choisie dans la définition d’intérieur et ses includes : `gps_path` contre `dashboard_path`.',
      '2. Matériau référencé par le PMD, alias exact `gps.tobj` contre `dashboard.tobj`, et contenu de l’UI (carte/infotainment contre instruments).',
      '3. Pièce PMG, position XYZ, triangles et surfaces physiquement connectées (sommets de couture soudés à 0,01 mm).',
      '4. Rectangle minimal de la surface projetée dans son plan ; les UV servent à orienter largeur/hauteur, pas à calculer les longueurs physiques. Les erreurs de planéité et d’ajustement UV sont conservées dans les données.',
      '5. Contrôle des variantes de modèles ; les surfaces identiques sont regroupées dans la synthèse, les références restent dans les JSON/CSV.','',
      'L’ancienne moyenne des ratios par triangle surpondérait les petits triangles des coins arrondis : elle donnait par exemple 1,224 pour le T680 GPS. Elle ne doit pas être utilisée. L’ancien regroupement par tuile UV pouvait aussi réunir plusieurs petits LCD éloignés.','',
      'Périmètre : GPS et afficheurs pilotés par les dashboards des 19 camions officiels présents. Pas de mesure des rétroviseurs caméra, accessoires portables, ni véhicules moddés. Les erreurs `seat.sui` des Volvo dans l’extraction ne concernent pas les écrans. Ce contrôle hors ligne ne certifie pas une capture GPU en jeu.','',
      'Données : `build/truck-screen-role-audit-20261020/role-audit.csv`, `role-audit.json`, `role-audit-full.json`. Chaque surface conserve UI, définition, modèle, pièce, texture, buffer, exposition UV, dimensions, centre et qualité géométrique.','',
      'Reproduction :','',
      '`python BetterGPS/Overlay/scripts/audit_truck_display_roles.py build/truck-screen-inventory-20261020/stock build/truck-screen-inventory-20261020/report build/truck-screen-role-audit-20261020`','']
    return '\n'.join(lines)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('stock',type=Path);parser.add_argument('previous',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args();run(args.stock,args.previous,args.output)
