# T680 NG : contrôle des variantes de pièces et des formes de caméra

Contrôle du stock SCS installé, le 25 octobre 2026, pour la famille
`kenworth.t680_2022`. Les mods sont exclus. Cette passe remplace, pour cette
famille, la sélection représentative du premier examen 3D.

## Résultat

Les seules caméras vidéo identifiées sont les trois lentilles des rétroviseurs
DigitalVision. Elles se trouvent dans des boîtiers ; « caméra ronde » et
« caméra en bloc » ne correspondent pas ici à deux familles de pièces vidéo
indépendantes proposées par SCS.

Aucune caméra autonome supplémentaire, en boule/dôme ou dans un petit boîtier
rectangulaire, n'a été identifiée sur la calandre, le pare-chocs avant, le
châssis arrière ou le dos de cabine dans les variantes examinées.

| Zone / pièces | Variantes contrôlées | Résultat |
| --- | --- | --- |
| Rétroviseurs principaux | 8 PMD : chrome/peinture, caméra/classique, vue extérieure/intérieure | Lentilles uniquement sur `camera_chrome` et `camera_paint`. Les versions `_int` représentent le même équipement plus détaillé, pas des caméras supplémentaires. |
| Rétroviseurs de capot | 20 PMD `fl_mirror` / `fr_mirror` | Surfaces de rétroviseurs optiques, aucune lentille vidéo identifiée. |
| Calandre | Chrome et Onyx | Aucune caméra identifiée. |
| Capot | Court et long | Aucune caméra identifiée. |
| Pare-chocs avant | 8 versions, peinture/plastique/chrome, aérodynamique ou non | Les optiques rondes utilisent la zone **antibrouillard** de l'atlas, et pas celle de la caméra. |
| Cabine | Day Cab, couchette 52 pouces, couchette 76 pouces, plus pièces animées de vitrage/essuie-glaces | Aucune lentille vidéo extérieure supplémentaire identifiée. |
| Dos de cabine | `day_standard`, `day_standard2`, `standard`, `standard_aircon` | Aucune caméra identifiée ; les grands éléments rectangulaires ne présentent pas de lentille vidéo reconnue. |
| Châssis | Les 8 variantes 4x2 / 6x4 | Aucune caméra identifiée sur le train arrière ou le châssis. |
| Éléments arrière | 2 pare-chocs arrière, garde-boue, marches, supports et 4 headache racks | Aucune caméra de recul ou de couplage identifiée. |
| Intérieur | `day`, `s52mb`, `s52ms`, `s76h`, `s76hb`, `s76hs` | Pas de caméra vidéo supplémentaire certifiée. Le petit boîtier au pare-brise ne présente pas d'objectif clairement identifiable dans cet examen. |

### Éléments ronds trompeurs

La texture `/vehicle/truck/kenworth_t680_2022/cabin_details_difspec.tobj`
contient **deux optiques différentes** : une petite lentille photographique
ronde à gauche et un antibrouillard à droite. Sa simple présence sur une
cabine ou un pare-chocs ne prouve donc pas la présence d'une caméra.

L'analyse des UV retrouve la petite lentille uniquement sur quatre PMG :

- `accessory/mirror/camera_chrome.pmg` : 44 triangles dans la zone de lentille ;
- `accessory/mirror/camera_paint.pmg` : 44 triangles ;
- `accessory/mirror/camera_chrome_int.pmg` : 114 triangles ;
- `accessory/mirror/camera_paint_int.pmg` : 114 triangles.

Les huit pare-chocs avant et les trois cabines utilisent aussi cet atlas,
mais leurs triangles ne couvrent pas la petite lentille de caméra. Les
pare-chocs prennent l'optique d'antibrouillard ; les cabines utilisent d'autres
zones de l'atlas. C'est la distinction qui manquait à une recherche fondée
uniquement sur les noms des matériaux.

Le disque ressemblant à une lentille dans `interior/lights_c.tobj` appartient
aux luminaires intérieurs. Sa zone ronde inférieure est utilisée dans les
trois variantes 76 pouces, sur un petit élément de la zone couchette ; elle
n'est pas une caméra de recul. `interior/sensor_color.tobj` est une texture
de trois voyants colorés, référencée par l'animation de formation, pas une
texture de lentille vidéo.

## Couverture et preuves

- 1 522 définitions/références relevées pour cette famille.
- 626 chemins de modèles référencés, dont 623 PMD/PMG extraits et lisibles.
- 251 modèles sous la racine propre au T680 NG et 372 références partagées,
  dont les 4 headache racks propres à sa géométrie.
- Les 623 empreintes de PMG ont été vérifiées ; tous sont au format PMG v21.
- 180 modèles de pièces extérieures ont été rendus sous deux angles et examinés.
  Le modèle de vitrage seul `cabin/win_open.pmd`, également lu et vérifié,
  ne produit pas d'image après masquage du vitrage.
- Les 6 variantes d'intérieur détaillé ont été comparées.
- Les 125 textures de base propres au T680 présentes dans ces matériaux ont
  été examinées sur planches, avec consultation directe des atlas d'optiques.

Trois références optionnelles de décoration partagée sont absentes de
l'installation : `toyac/heads_ext.pmd`, `toyac/smelling_sack_ext.pmd` et
`toyac/wooden_classic_ext.pmd`. Elles sont conservées comme erreurs d'extraction,
pas comptées parmi les modèles inspectés. Aucun échec ne touche les pièces
extérieures de camion ou les modèles de rétroviseurs caméra.

[Manifest, empreintes et résultats UV](references/truck-cameras/t680-variants-20261025/verification.json).
Les modèles de pièces contiennent toutes leurs pièces de maillage, y compris
les alternatives de visibilité présentes dans le même PMG. L'inspection
cumule ces pièces ; elle ne reproduit pas la combinaison exacte choisie par
le joueur. Elle ne démontre ni une installation réelle sur son camion,
ni un flux vidéo disponible pour BetterGPS.

### Planches des variantes extérieures

Les rendus sont issus des PMG et textures SCS, avec éclairage/matériaux
approximatifs. Deux vues par pièce ; les PMG/PNG individuels sont conservés
sous `build/camera-audit-20261025/t680-all`. Le vitrage et les maillages de
shadow-only sont masqués dans ces rendus, mais leurs sources restent lues.

- [1 — Pare-chocs, calandres, rétroviseurs, dos de cabine, cabines et châssis](references/truck-cameras/t680-variants-20261025/parts-1.jpg)
- [2 — Châssis, balises, déflecteurs et échappements](references/truck-cameras/t680-variants-20261025/parts-2.jpg)
- [3 — Phares, filtres et rétroviseurs de capot](references/truck-cameras/t680-variants-20261025/parts-3.jpg)
- [4 — Capots, marches, garde-boue et panneaux](references/truck-cameras/t680-variants-20261025/parts-4.jpg)
- [5 — Jupes et variantes de barres latérales](references/truck-cameras/t680-variants-20261025/parts-5.jpg)
- [6 — Réservoirs, pare-soleil, raccords et headache racks](references/truck-cameras/t680-variants-20261025/parts-6.jpg)
- [Comparaison des six intérieurs](references/truck-cameras/t680-variants-20261025/interior-variants.jpg)

## Reproduction

Après le relevé `--camera-inventory` dans le dossier `stock` :

```powershell
AssetInspector.exe MAPBRIDGE_PUBLISH ATS_ROOT ATS_DOCUMENTS build/camera-audit-20261025/t680-all --mesh-audit --all-truck=kenworth.t680_2022
python BetterGPS/Overlay/scripts/audit_t680_camera_variants.py --data build/camera-audit-20261025/t680-all --output build/camera-audit-20261025/t680-all/verification.json
```

La recherche UV est une preuve positive d'utilisation de **cette** lentille,
pas un détecteur universel de caméra. La conclusion sur les autres formes
repose aussi sur les pièces, leurs définitions, les rendus et leurs textures.
Les boîtiers sans face optique visible ne reçoivent pas une attribution ADAS,
radar ou caméra sur leur seule silhouette. Les références du monde réel ne
sont pas utilisées comme preuve de présence dans le jeu.

Aucun changement au plugin, à sa page Camera ou à l'installation ATS.
