# PP807032 — caméra avant du T680 NG

Une seule PP807032 standard est désormais proposée sur tous les pare-chocs,
avec le filtre `suitable_for: bumper`. Les anciens filtres par identifiant
et le choix Low Bumper sont retirés. Le léger clipping des plats sur les
grilles basses est accepté. La caméra avant est incompatible avec les deux
bannières Oversize Load ; elle reste compatible avec la bull bar.
Les descriptions historiques ci-dessous ne décrivent pas ce catalogue courant.

**v0.13.2 :** les modèles actuels sont dans
[camera-refinement](../camera-refinement/README.md) : `pp807032_editable.blend`
pour les pare-chocs standard (plats constants 10,5 × 2,5 mm), et
`pp807032_low_editable.blend` pour les pare-chocs bas (5,5 × 1,5 mm).
Deux options indépendantes du catalogue : **Standard Bumper** et **Low Bumper**,
250 $ chacune, filtrées sur les identifiants exacts des pare-chocs compatibles. Les positions
courantes sont enregistrées dans `placement/mounts.json`.

Les sources décrites ci-dessous sont celles de la reconstruction antérieure.

Projet propre : [pp807032_editable.blend](pp807032_editable.blend), avec les pièces
séparées, matériaux SCS et UV limités aux couleurs d'une palette 128 × 16.
[pp807032_scs.blend](pp807032_scs.blend) contient les deux LOD joints pour export.
Le croquis Blender original de l'utilisateur est conservé dans `user-sketch.blend`.

La tête conserve l'angle mesuré sur la modification utilisateur : 5,563963° vers
le bas. Le boîtier, le pare-soleil, les optiques et les LED suivent maintenant
une même rotation rigide, sans déformer les pièces. Aucun changement aux modèles
arrière PP804395 ni aux caméras latérales PP804396/PP804397 livrés dans le jeu.

Deux plats droits de 89,017 mm partent des pivots vers l'arrière. Leurs lames
de 1,5 × 3 mm et leur entraxe de 62 mm permettent le passage dans les trous de
la grille stock. Ces dimensions sont adaptées au montage et au croquis, sans
prétendre être des cotes constructeur. Le corps conserve l'échelle 62,5 % de
la reconstruction PP804395 : environ 73,1 mm de largeur hors tout.

La version 0.9.0 du dossier de mod commun `../pp804395/scs-light/mod/` ajoute
une option indépendante **Pana-Pacific PP807032 - Front Camera**, 250 $, niveau 0,
sur les huit pare-chocs d'origine du T680 NG. Elle se trouve à gauche du camion
(côté conducteur), à droite de la plaque lorsqu'on regarde le camion de face.
Le point du configurateur regarde vers l'avant et la caméra n'est jamais ajoutée
par défaut. Les accessoires de pare-chocs SCS existants restent inchangés.

La caméra utilise le système SCS des boules de volant et des éclairages de
barres : un `slot_0` vide dans le pare-chocs et une unité `accessory_hookup_data`.
Le registre additionnel `def/vehicle/addon_hookup_storage.bettergps.sii` inclut
`addon_hookups/bettergps_pp807032.sui`. L'unité `pp807032.bgps.addon_hookup`
utilise `model:` et `suitable_for[]: "bumper"`, le type de pièce parente.
Le registre SCS d'origine n'est pas remplacé.

Dans les accessoires extérieurs du configurateur, choisir le pare-chocs puis
le point sur la grille et **Pana-Pacific PP807032 - Front Camera**, 250 $, niveau 0.
Le slot est vide par défaut. Le choix est enregistré dans les tableaux
`slot_name`/`slot_hookup` du pare-chocs et suit la position/rotation de celui-ci.
Il n'y a ni choix de longueur de capot ni filtre `low_*`/`std_*`.

Les anciens points avant de châssis et définitions d'accessoire direct sont
retirés. Les points, définitions et modèles arrière/latéraux restent identiques
à la 0.8.2 confirmée fonctionnelle par l'utilisateur. Le modèle avant, son
inclinaison et ses textures sont également conservés.

Les références stock réellement inspectées sont listées dans
`hookup-reference.json` : `steering_w/standard.pmg`, `sw_knob.sui`,
`cabflares/day_slots1.pmg` et `light.sui`. Leurs slots sont également vides,
avec un hookup nul ; le modèle parent ne contient pas la pièce enfant.

Les poses propres à chaque pare-chocs et trois vues par variante sont dans
`placement/`. Le corps est à moins de 0,3 mm de la grille au point le plus proche.
Les plats passent derrière la grille sans croiser ses parties opaques. Sur les
pare-chocs bas, le contrôle tient compte des trous de la texture alpha stock ;
sur les standards, il inclut l'insert d'admission séparé, avec son décalage réel.
Les volumes destinés seulement aux ombres sont exclus du contrôle physique.

544 triangles au premier LOD, 116 au second ; un seul matériau `eut2.dif.spec`.
Seul le premier LOD est livré comme accessoire enfant. Les sources PIM/PIT et
les ressources converties sont conservées pour que le projet ne dépende pas du
cache de travail. Le reçu commun et le contrôle indépendant TruckLib vérifient
le registre SII et son include, ainsi que les huit PMG de pare-chocs : un slot
vide ajouté, géométrie/UV/normales/index
préservés octet pour octet et ordre natif des sections conservé. Les 64 combinaisons
des huit châssis avec les huit pare-chocs conservent les placements vérifiés.

La reconstruction utilise `scripts/build_pp807032_scs.py` dans Blender 3.6 avec
`--addon` et `--output`. Après conversion officielle SCS, les scripts
`render_pp807032_preview.py` (`--cache`, `--output`) et
`render_pp807032_mounts.py` (`--stock`, `--cache`, `--output`) utilisent Blender
4.4. Le package commun est assemblé par `package_pp804395_scs.py`.

La validation en jeu reste à faire par l'utilisateur. Les aperçus de placement
utilisent des matériaux de contexte simplifiés, pas le rendu final d'ATS.

La structure des slots est documentée par SCS dans
[vehicle_addon_accessory](https://modding.scssoft.com/wiki/Documentation/Engine/Units/vehicle_addon_accessory).

Référence pour les options : [accessory_addon_data](https://modding.scssoft.com/wiki/Documentation/Engine/Units/accessory_addon_data)
et [accessory_data](https://modding.scssoft.com/wiki/Documentation/Engine/Units/accessory_data).
Les types d'accessoires correspondent aux locators du modèle du camion :
[définitions des camions](https://modding.scssoft.com/wiki/Documentation/Engine/Game_data/Player_trucks_definitions).
Le développeur SCS 50keda explique la correspondance par préfixe dans
[ce message](https://forum.scssoft.com/viewtopic.php?start=1370&t=176424).
