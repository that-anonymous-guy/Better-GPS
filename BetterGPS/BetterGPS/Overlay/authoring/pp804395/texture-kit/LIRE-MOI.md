# Peindre la caméra PP804395

Ouvrir `pp804395_texture_ready.blend` avec Blender 4.4 ou plus récent.
Le modèle possède un dépliage UV dédié, `PP804395_ATLAS`, pour les 36 pièces.
Les biseaux sont appliqués dans cette copie pour que les contours correspondent
exactement aux surfaces à peindre. La reconstruction d'origine est conservée.

## Les fichiers à modifier

| Fichier | Usage | Espace couleur |
| --- | --- | --- |
| `textures/pp804395_basecolor.png` | Couleurs, rayures, inscriptions, verre, LED | sRGB |
| `textures/pp804395_roughness.png` | Rugosité : noir = lisse, blanc = mat | Non-Color / données |
| `textures/pp804395_metallic.png` | Métal : noir = non métallique, blanc = métal nu | Non-Color / données |

Les trois images sont déjà reliées aux matériaux Blender. Elles mesurent
4096 × 4096 pixels. Conserver ces dimensions, l'orientation et les noms pour
retrouver immédiatement les modifications sur le modèle. Le canal alpha de
la couleur est opaque ; il ne pilote pas la transparence du verre.

## Peinture avec des calques

Ouvrir `pp804395_paint_template.ora` dans Krita ou GIMP. Il contient une base
couleur et un calque transparent PEINTURE pour les détails. Les calques ID
PIECES, CONTOURS UV et REPERES sont des guides, initialement masqués.
Les rendre visibles pour travailler, puis les masquer avant d'exporter le
résultat vers `textures/pp804395_basecolor.png`. Conserver le fichier ORA
comme source à calques. Les repères et la grille ne doivent pas apparaître
dans la couleur finale.

Dans Photoshop ou un autre éditeur PNG, ouvrir la base couleur puis importer
les PNG transparents de `templates/` comme calques de repérage alignés au
coin supérieur gauche, à leur taille d'origine. Le SVG des contours peut
aussi être importé à 4096 × 4096. Aucun fichier PSD propriétaire n'est requis.

## Se repérer sur l'atlas

`templates/pp804395_uv_guide.png` montre les zones colorées, leurs noms et les
contours. La moitié supérieure réserve de grandes surfaces au boîtier, au
dessus de la casquette et à la base du support. La façade et les pivots sont
au milieu. Les petites pièces et les 18 LED ont leurs zones individuelles.
Gauche/droite désignent les noms des pièces du modèle, vus depuis sa face.

- `pp804395_uv_wire.png` : contours sur fond transparent.
- `pp804395_labels.png` : noms et limites des zones, sur fond transparent.
- `pp804395_uv_orientation.png` : guide avec les grandes faces repérées
  AVANT, ARRIERE, DESSUS et DESSOUS. `pp804395_face_labels.png` fournit ces
  indications en calque transparent aligné sur les UV.
- `pp804395_part_ids.png` : une couleur par pièce, pour les sélections.
- `masks/*.png` : masques noir/blanc par famille de matériau. Blanc = surface.
- `part_palette.json` : noms, couleurs ID et rectangles de repérage en pixels.
- `pp804395_ao.png` : occlusion ambiante calculée sur la géométrie, utilisable
  comme guide ou calque Multiplication léger selon l'effet souhaité.
- `pp804395_checker.png` : grille numérotée réservée au contrôle des UV.

Peindre jusqu'au bord des îlots et prolonger la couleur de quelques pixels
autour (8 pixels conseillés à cette résolution). Les images de départ ont
déjà ce débordement. Éviter les lignes UV dans la texture finale. Une peinture
sur les deux faces d'une pièce se fait sur deux îlots distincts : les UV ne
sont pas superposés. Les faces latérales fines et les biseaux sont également
dépliés ; zoomer dans le gabarit pour les détails.

## Revoir les textures dans Blender

1. Choisir l'espace de travail UV Editing ou Texture Paint. Le modèle est
   sélectionné ; Tab affiche le maillage et l'ensemble des UV.
2. Utiliser le mode d'affichage Material Preview pour voir la couleur.
3. Après une modification externe d'un PNG, utiliser Image > Reload dans
   l'éditeur d'image de Blender pour l'image correspondante. Rouvrir le
   fichier `.blend` recharge aussi les PNG externes.
4. Conserver le dossier `textures` à côté du `.blend`, car les chemins sont
   relatifs. Copier le dossier complet si le projet est déplacé.

Les rendus `preview_checker_front.png` et `preview_checker_rear.png` montrent
les UV sur les deux côtés ; `preview_basecolor.png` montre la base à peindre.
`pp804395_texture_ready.glb` contient la même géométrie, les UV et les images
de départ pour l'import dans d'autres logiciels. Il faudra réexporter le GLB
pour y inclure les images que vous aurez modifiées.

## Destination ATS 1.61

Ce kit prépare le travail de texture et les sources 3D. La conversion SCS,
les matériaux du jeu, le placement sur un camion et la vérification en jeu
restent une étape d'intégration. Les cartes Roughness/Metallic sont ici des
sources de travail PBR ; leur conversion dépendra du shader SCS choisi.
Les cotes de reconstruction du boîtier restent celles du modèle précédent.

`validation.json` enregistre le contrôle des UV, les références d'images et
les empreintes des fichiers livrés. Ne pas relancer les scripts de génération
sur vos textures peintes : leur rôle est de fabriquer le kit initial.
