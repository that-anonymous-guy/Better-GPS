# PP804395 - version SCS légère pour le T680 NG

Une seule PP807032 standard est désormais proposée sur tous les pare-chocs,
avec le filtre `suitable_for: bumper`. Les anciens filtres par identifiant
et le choix Low Bumper sont retirés. Le léger clipping des plats sur les
grilles basses est accepté. La caméra avant est incompatible avec les deux
bannières Oversize Load ; elle reste compatible avec la bull bar.
Les descriptions historiques ci-dessous ne décrivent pas ce catalogue courant.

**v0.13.2 :** les quatre caméras affinées, leurs projets Blender actuels,
textures et aperçus sont dans [camera-refinement](../../camera-refinement/README.md).
Le `pp804395_scs.blend` de ce dossier est la source historique de la 0.9.0 ;
pour modifier la nouvelle arrière, utiliser `camera-refinement/pp804395_editable.blend`.

La source de la version 0.13.0 reste un dossier de mod décompressé `mod/`.
L'utilisateur a ensuite renommé et activé `BetterGPS_PP804395_T680NG.zip` dans
ATS : le correctif est appliqué dans ce ZIP, en conservant son nom actif.
La PP807032 propose deux montages à 250 $ : « Standard Bumper » pour les
pare-chocs standard, « Low Bumper » pour les pare-chocs bas. Chaque option est
restreinte aux quatre identifiants de sa famille de pare-chocs. Les plats gardent une
section constante de bout en bout. Les détails actuels sont dans le README partagé.

Quatre accessoires sont disponibles séparément, sans installation par défaut :
PP804395 arrière (150 $), PP804396 droite (250 $), PP804397 gauche (250 $),
PP807032 avant (250 $). Cette dernière utilise un `slot_0` vide sur les huit
pare-chocs avant d'origine et une définition `accessory_hookup_data` enregistrée
dans un catalogue additionnel. C'est le système SCS des boules de volant et
des éclairages de barres : la caméra est enfant du pare-chocs et suit sa pose.
Les anciens points avant de châssis et leurs filtres ont été retirés. Aucun
choix de longueur de capot ni installation par défaut n'est nécessaire.
Les trois points de cabine `bgps_cam`, `bgps_right` et `bgps_left`, leurs
définitions et leurs poses sont conservées ; les modèles sont affinés.
Le modèle propre et les poses sont dans `../../pp807032/`.

Les caméras latérales ont été réduites de 35 %, remontées d'environ 6 cm et
avancées d'environ 8 cm depuis leur première pose. Elles sont symétriques,
contre la cabine, sous le rétroviseur, avec le logement dirigé vers l'arrière.
Leurs points du configurateur regardent chacun vers l'extérieur de leur côté.
Les poses et aperçus sont dans le dossier voisin `../../pp804396/placement/`.
Le réglage de boule (57,337061°) et la profondeur des modules IR/POD 1 du
projet utilisateur sont appliqués aux deux modèles. L'écart de semelle mesuré
est inférieur à 0,15 mm, sans intersection avec les rétroviseurs stock.

Dans le configurateur, ouvrir les accessoires extérieurs, tourner autour du
camion et sélectionner le point au dos de la cabine. Choisir
« Pana-Pacific PP804395 - Rear Camera » (150 $, niveau 0). Le bouton de retrait
du configurateur permet de revenir à une cabine sans caméra.

## Cabines prises en charge

Les trois géométries extérieures SCS sont prises en charge : Day Cab (`dc`),
52 pouces (`s52m`) et 76 pouces (`s76h`). Les variantes d'intérieur s'appuient
sur ces trois cabines extérieures. Chaque géométrie a son propre placement,
mesuré sur la paroi arrière et contrôlé avec les équipements arrière stock.

| Cabine | X | Y (hauteur) | Z (longueur) |
| --- | --- | --- | --- |
| Day Cab | 0 | 2.574779 | -0.531909 |
| 52 pouces | 0 | 2.674786 | 0.524004 |
| 76 pouces | 0 | 2.674779 | 1.122934 |

Coordonnées locales du modèle de cabine SCS, en mètres. Les images sous
`placement/` sont des aperçus hors jeu des véritables maillages convertis,
avec les cabines stock en gris. Les vues `*-mount.png` montrent la nouvelle
platine plate et ses deux pattes droites vers les pivots latéraux, selon le
schéma fourni par l'utilisateur. Le support inférieur et l'adaptateur coudé
ont été retirés.

La version 0.7.0 conserve l'échelle de 62,5 % du modèle arrière initial (largeur totale
75 mm) et incline uniquement la tête caméra de 45° vers le bas, autour de
l'axe latéral passant par les pivots. Le boîtier, la casquette, les optiques,
les LED et le connecteur tournent ensemble. La platine, les pattes droites,
les pivots et leurs boulons restent fixes. Le point de sélection garde une
échelle de 1.

La hauteur de la Day Cab v0.5.0 approuvée par l'utilisateur est conservée.
Les deux couchettes sont remontées de 10 cm par rapport à la version 0.6.0 :
axe à 2,719779 m, contre 2,619779 m pour la Day Cab inchangée. Les huit châssis stock ont
le même point d'ancrage de cabine. La référence de hauteur est conservée
dans `placement-reference.json`.

La profondeur est recalculée pour maintenir la platine contre chaque paroi.
Un contrôle de 25 points de platine et de tous les sommets ne trouve aucune
pénétration ; l'écart maximal de la platine arrière est inférieur à 0,22 mm.
Les intersections avec les équipements arrière stock sont aussi contrôlées
et absentes. Les mesures figurent dans `mounts.json` et les aperçus sont hors jeu.

## Coût du modèle

- 556 triangles pour l'accessoire installé.
- Un seul matériau `eut2.dif.spec`.
- Une palette couleur/spéculaire de 128 × 16 pixels, DDS avec mipmaps.
- UV de palette très simples ; aucun atlas 4K, normal map ou réflexion
  supplémentaire n'est nécessaire pour cette version.

Le kit UV détaillé et ses textures de peinture restent séparés dans le dossier
voisin `texture-kit/`. Les anciennes dimensions de reconstruction restent des
estimations ; ce travail ne transforme pas ces cotes en mesures constructeur.

## Intégration

La version 0.7.0 utilise un `accessory_addon_data` SCS dans
`def/vehicle/truck/kenworth.t680_2022/accessory/bgps_cam/pp804395.sii`.
Un locator vide `bgps_cam` est ajouté au modèle de chacune des trois cabines,
avec son placement propre. Il utilise le même format de socket que les
accessoires stock. Aucun hookup permanent ni `defaults`/`overrides` ne monte
la caméra. Le paquet remplace uniquement les trois PMG, ajoute les fichiers
de caméra, son matériau, sa texture et sa définition d'accessoire.
Les tables de vertex, triangles, normales, couleurs, UV et les anciens
locators des cabines sont conservés. Les PMD, collisions et animations stock
restent fournis par ATS. Le volume de visibilité est étendu pour contenir
aussi la caméra.

Le point de sélection est orienté vers l'arrière (rotation de 180° autour de
l'axe vertical SCS). La géométrie locale de la caméra reçoit la rotation
inverse. L'inclinaison locale de la paroi est ensuite appliquée à la pose.
Le configurateur SCS filtre les
points selon leur orientation ; ce point est donc destiné à la vue arrière.

Les tables PMG suivent désormais l'ordre natif : parties, locators, pièces,
chaînes, vertex et indices. L'ajout fait seulement 44 octets par cabine et
reloge les pointeurs absolus de tous les attributs. La version 0.2.0 ajoutait
à tort des tables après les pools ; la relecture précédente acceptait cette
disposition sans contrôler l'ordre des sections. Le nouveau contrôle rejette
cette ancienne disposition et vérifie tous les flux, y compris les trois
canaux UV de la cabine 52 pouces.

L'icône fournie par l'utilisateur est conservée dans `icon/source-camera.dds`.
`icon/pp804395.png` montre l'adaptation au format stock 256 × 64 : dessin
redimensionné dans la case gauche de 64 × 64, sans déformation, et fond
transparent. Les DDS/TOBJ/MAT de `project/material/ui/accessory/bettergps/`
utilisent le shader UI et les paramètres de sampler stock.

La version 0.1.0 ajoutait à tort la caméra en permanence via un hookup de
modèle ; elle est remplacée par cet accessoire optionnel. Le modèle distant
de 128 triangles reste dans les sources Blender/PIM mais n'est pas livré
comme hookup de cabine.

Un autre mod remplaçant ces mêmes PMG de cabine
nécessite une fusion ; une simple priorité ne permet pas de conserver les
deux versions d'une cabine modifiée.

Ce modèle physique ne fournit pas d'image vidéo au NavPlus. La page Camera
de BetterGPS demeure une fonctionnalité distincte.

## Sources et contrôles

`pp804395_scs.blend` est le projet Blender 3.6 utilisant les matériaux SCS.
`project/` contient les PIM/PIT et la palette TGA à réexporter/convertir avec
SCS Blender Tools et Conversion Tools 2.21. Si ce dossier est déplacé,
sélectionner `project/` comme SCS Project Base Path avant de réexporter.

`geometry-report.json`, `package-validation.json` et
`independent-model-check.json` conservent les résultats de conversion, les
empreintes des sources et le contrôle indépendant de relecture des modèles.
Le rapport de validation native, lorsqu'il est disponible, distingue les
messages du jeu stock de ceux du mod.

Pour la version 0.7.0, le lecteur indépendant contrôle l'ordre natif des tables,
l'identité exacte des pools de vertex et d'indices, les sockets vides orientés
vers l'arrière, la largeur conservée, la hauteur commune des pivots, l'appui
de la platine, la définition optionnelle et le format d'icône. Il vérifie aussi
la direction de la pupille optique à 45° dans le PMG converti et les sommets
fixes de la platine et des pattes. Les rendus
de placement sont hors jeu. Le test manuel des corrections reste à faire par
l'utilisateur : vérifier que la cabine 52 pouces est visible, sélectionner
le point depuis l'arrière, vérifier l'icône, puis installer et retirer la
caméra sur les trois cabines.

Outils de construction dans `BetterGPS/Overlay/scripts/` :
`build_pp804395_scs.py`, `render_pp804395_mounts.py`,
`scs_camera_mounts.py`, `package_pp804395_scs.py`, `prepare_pp804395_icon.py`.

Les sources moteur sont documentées par SCS :
[Blender Tools](https://modding.scssoft.com/wiki/Documentation/Tools/SCS_Blender_Tools),
[Conversion Tools](https://modding.scssoft.com/wiki/Documentation/Tools/Conversion_Tools),
[locators](https://modding.scssoft.com/wiki/Documentation/Tools/SCS_Blender_Tools/Locators/Model_Locators).

L'ancienne archive v0.6.0 est conservée dans `archives/`. La livraison actuelle
est exclusivement le contenu décompressé du dossier `mod/`.
