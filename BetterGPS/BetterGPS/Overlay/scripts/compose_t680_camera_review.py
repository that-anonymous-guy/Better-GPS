"""Assemble annotated camera calibration boards from measured 3D renders."""
import argparse,json
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont,ImageOps

def run(root,out):
 out.mkdir(parents=True,exist_ok=True)
 fontroot=Path('C:/Windows/Fonts')
 def font(n,bold=False):return ImageFont.truetype(str(fontroot/('segoeuib.ttf' if bold else 'segoeui.ttf')),n)
 bg='#111922';white='#e6edf3';muted='#a5b2c0'
 colors={'forward':'#63d6ff','left':'#72efb0','right':'#ffc06b','reverse':'#e6a0ff'}
 def text(draw,xy,value,size=25,fill=white,bold=False):draw.text(xy,value,font=font(size,bold),fill=fill)
 board=Image.new('RGB',(1800,1690),bg);d=ImageDraw.Draw(board)
 text(d,(36,22),'T680 NG · validation des quatre vues caméra',43,bold=True)
 text(d,(36,82),'Géométrie SCS + caméras du mod fourni. Matériaux gris de présentation.',24,muted)
 def panel(name,box):
  im=Image.open(root/(name+'.png')).convert('RGB');board.paste(ImageOps.fit(im,(box[2],box[3])),(box[0],box[1]))
 panel('overview-top',(32,140,1060,678))
 # The field-of-view rays are projections, not the whole vertical image frustum.
 text(d,(50,154),'Vue du dessus · axes et champ horizontal',27,bold=True)
 text(d,(405,185),'AVANT · 100°',27,colors['forward'],True)
 text(d,(65,370),'GAUCHE',27,colors['left'],True);text(d,(65,406),'100° · 35° vers l’extérieur',22,colors['left'])
 text(d,(746,370),'DROITE',27,colors['right'],True);text(d,(725,406),'100° · 35° vers l’extérieur',22,colors['right'])
 text(d,(386,740),'ARRIÈRE · 100°',27,colors['reverse'],True)
 panel('forward-side',(1114,140,654,290));text(d,(1127,154),'AVANT · inclinaison 5° vers le bas',24,colors['forward'],True)
 panel('reverse-side',(1114,452,654,366));text(d,(1127,466),'ARRIÈRE · inclinaison 45° vers le bas',24,colors['reverse'],True)
 data=json.loads((root/'review-poses.json').read_text());rows={p['view']:p for p in data['poses'] if p['cabin']=='s76h'}
 titles={'forward':'Avant · PP807032 · pare-chocs','left':'Gauche · PP804397 · sous le rétroviseur','right':'Droite · PP804396 · sous le rétroviseur','reverse':'Arrière · PP804395 · dos de cabine'}
 for i,v in enumerate(['forward','left','right','reverse']):
  x=32+(i%2)*884;y=848+(i//2)*377
  panel(v+'-mount',(x,y,852,290));text(d,(x+12,y+10),titles[v],26,colors[v],True)
  p=rows[v]['opticalCenterChassis'];value='Centre optique · X {:+.3f} / Y {:.3f} / Z {:+.3f} m'.format(*p)
  text(d,(x+8,y+300),value,22)
  text(d,(x+8,y+331),'Latérales : 35° depuis l’arrière, sans inclinaison verticale.' if v in ['left','right'] else ('Champ horizontal 100° · vers le bas 5°' if v=='forward' else 'Champ horizontal 100° · vers le bas 45°'),21,muted)
 text(d,(36,1620),'Repère du châssis : X droite, Y haut, Z arrière. Exemple principal : cabine 76″ / châssis 224″.',23,muted)
 text(d,(36,1654),'Validation de placement uniquement · le flux vidéo natif reste à brancher et à vérifier dans ATS.',22,muted)
 board.save(out/'t680-quatre-cameras.png',optimize=True)
 variants=Image.new('RGB',(1800,650),bg);v=ImageDraw.Draw(variants)
 text(v,(32,18),'Caméra arrière · les trois variantes de cabine',39,bold=True)
 for i,(cab,label) in enumerate([('dc','Day Cab'),('s52m','Sleeper 52″'),('s76h','Sleeper 76″')]):
  x=24+596*i;im=Image.open(root/(cab+'-rear.png')).convert('RGB');variants.paste(ImageOps.fit(im,(568,420)),(x,100));text(v,(x+12,108),label,28,colors['reverse'],True)
  p=next(p['opticalCenterChassis'] for p in data['poses'] if p['cabin']==cab and p['view']=='reverse')
  text(v,(x+12,532),'Centre optique Y {:.3f} / Z {:+.3f} m'.format(p[1],p[2]),22)
 text(v,(32,599),'Le point de vue suivra la caméra réellement montée, avec une visée à 45° vers le bas pour les trois cabines.',25,muted)
 variants.save(out/'t680-arriere-variantes.png',optimize=True)
 print('Boards written',out)

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--renders',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();run(a.renders,a.output)
