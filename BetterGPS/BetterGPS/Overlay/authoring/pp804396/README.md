# Pana-Pacific PP804396 — caméra latérale droite

**v0.12.0 :** les sources affinées droite **et gauche**, avec leurs matériaux
texturés SCS, sont dans [camera-refinement](../camera-refinement/README.md).
Utiliser ses `pp804396_editable.blend` et `pp804397_editable.blend` ; les fichiers
Blender/GLB ci-dessous décrivent la reconstruction antérieure.

Reconstruction 3D à partir des photos fournies. La coque, le cerclage extérieur
et la semelle restent fixes ; la boule optique avec sa lentille et ses diodes
est orientée vers le haut de **57,337061°**, selon le projet Blender modifié
par l'utilisateur. Tous les modules IR/POD reprennent la profondeur de ses
modules 1, conservés dans `user-adjustments.json`.

Depuis la 0.8.2, les points utilisent `bgps_right` et `bgps_left` : ils ne
commencent plus par le type arrière `bgps_cam`, qu'ATS pouvait leur associer
par préfixe. Rééquiper les deux latérales sur ces nouveaux points après la mise
à jour. Les anciens fichiers de définition et leurs prix restent disponibles
pour la lecture des sauvegardes ; les modèles, poses et textures sont conservés.

## Fichiers à utiliser

- `pp804396_reconstruction.blend` : projet Blender 3.6, pièces séparées,
  matériaux simples et palette embarquée. Compatible avec les Blender récents.
- `pp804396_reconstruction.glb` : export portable avec matériaux et palette.
- `pp804396_preview.png`, `pp804396_front.png`, `pp804396_side.png` : aperçus hors jeu.
- `palette.png` : seulement 128 × 16 pixels, huit couleurs uniformes.
- `scs/pp804396_scs.blend` : variante SCS avec un seul matériau `eut2.dif.spec`.
- `scs/project/` : sources PIM/PIT, palette TGA et TOBJ pour Conversion Tools.
- `scs/converted/` : PMD/PMG, DDS, TOBJ et matériau convertis pour préparer
  l'intégration ATS 1.61. Les variantes `*_truck` utilisent un repère adapté
  aux points du configurateur ; PP804397 est la version gauche symétrique.
  Le mod combiné décompressé est préparé dans `../pp804395/scs-light/mod/`.

Dans Blender, sélectionner `PP804396_OPTICAL_BALL` pour régler l'inclinaison.
Sa rotation X de −57,337061° oriente la lentille vers le haut. Modifier cette seule
rotation laisse la coque et la semelle en place. Le fichier SCS contient la
pose aplatie dans le maillage ; réexporter depuis les sources pour la modifier.

## UV et matériaux

Chaque pièce échantillonne une seule case de palette : aucun dépliage complexe,
aucun atlas 4K ni normal map. De gauche à droite : coque, caoutchouc, verre,
métal des bagues, diodes IR, pupille, petite étiquette, joints. Le canal alpha
de la TGA est utilisé comme information spéculaire SCS, et non comme découpe
transparente. La version SCS utilise une seule pièce et un seul matériau par LOD.

Le modèle proche compte 4 407 triangles ; le modèle distant 1 249 triangles.
Le modèle Blender conserve les pièces séparées pour faciliter les retouches.

## Échelle et limites des références

Unités : mètres. La semelle repose sur Z = 0 ; l'avant de la caméra est −Y.
La base de reconstruction est de **84 × 106 mm**. La taille actuelle est
réduite de 35 % : semelle **54,6 × 68,9 mm**, racine Blender à l'échelle 0,65.
Ces dimensions sont **estimées d'après
les photos**, car aucun plan coté constructeur n'a été trouvé. Le nombre de
six petits modules IR est une interprétation visuelle, pas une spécification
fabricant. Les détails du kit électrique ne sont pas modélisés.

`pp804396_dimensions.json` sépare les caractéristiques confirmées et les
estimations. `validation.json` vérifie les modèles convertis, leurs dépendances,
la petite texture, la hiérarchie GLB et la direction réelle de la pupille.

`placement/` montre les poses sur les trois cabines T680 NG : PP804396 à
droite et PP804397 à gauche, chacune optionnelle à 250 $, sous le rétroviseur
et orientée vers l'arrière. Les caméras ont été remontées et avancées après
le premier aperçu. Les points de sélection sont visibles du côté concerné.

La fiche [Pana-Pacific](https://www.panapacific.com/products/pro-view-right-side-view-color-cmos-camera-kit-for-kenworth-navplus-system-or-peterbilt-smartnav-system-and-for-kenworth-navplus-hd-system-or-peterbilt-smartnav-2nd-generation-system)
identifie la PP804396 comme une caméra CMOS latérale droite, à coque étanche
et peignable. La [fiche Peterbilt fournie](https://peterbiltparts.com/products/pana-pacific-camera-kit-pp804396)
et les photos de l'utilisateur servent aussi de références.

Scripts de génération : `create_pp804396_camera.py` et
`package_pp804396_sources.py` dans `BetterGPS/Overlay/scripts/`.
