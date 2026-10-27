# Freightliner Cascadia 2019 / 2024 — ICC5 numérique

Le Western Star 57X (`westernstar.57x`, UI `westernstar_57x_gps.sii`) réutilise
exactement ce runtime et les mêmes géométries ICC5. Seuls trois médias changent :
`westernstar_icc5_cluster_intro.bga`, `westernstar_icc5_home.dds` (le Home fourni)
et `icc5_splash_westernstar.dds` à la place de Runsmart. Tous les autres éléments,
placements, palettes, durées, transitions et contrôles tactiles restent partagés.
L'intro cluster conserve 300 images lossless / 30 fps, 10 secondes et un cut
direct. Seuls les en-têtes sont lus au démarrage ; le worker ouvre la vidéo de
la marque active. Un changement de camion invalide la série d'images précédente
avant le premier bind GPU. Les deux UI stock du 57X sont conservées, avec les
mêmes masques anti-flash additifs que les Cascadia.

L'identité exige le véhicule `freightliner.cascadia2019` ou
`freightliner.cascadia2024`, l'alias `/vehicle/truck/share/gps.tobj` et l'UI
`freightliner_cascadia_icc5_gps.sii` ou `freightliner_cascadia_2024_gps.sii`.
Les anciens GPS `/ui/gps.sii`, les clusters et les versions à compteurs
analogiques ne sont pas acquis comme ICC5.

Le canvas infotainment est 1596 × 600, comme les textures fournies. Le mesh
GPS expose tout le buffer 1024 × 512 sur 244,22 × 91,81 mm (ratio 2,660).
Cette différence de ratio buffer/écran est compensée par le mapping natif,
pas par un letterboxing 2:1. Le cluster distinct mesure 304,76 × 114,29 mm.
Les deux UI de cluster sont 1440 × 540, entièrement couvertes par l'intro.

Le canvas logique n'est pas la texture marqueur intermédiaire : celle-ci garde
ses dimensions fixes par slot. Avant upload, tout canvas trop grand est réduit
uniformément pour tenir sur ses deux axes, et le crop SII utilise cette taille
stockée. Aucun UV ne dépasse 1, aucune copie de ligne ne déborde. Les canvases
qui tiennent déjà conservent leur disposition d'octets exacte. Le cadrage de
carte TomTom utilise le ratio large, même avec le format horizontal sélectionné.
La température est centrée puis décalée de 12 px vers la gauche et 10 px vers
le haut pour suivre la zone utile du module incliné. L'heure Home est centrée
entre les icônes et le coin droit, plutôt qu'alignée sur son bord droit.

## Démarrage

Le dernier `freightliner_icc5_cluster_intro.mp4` fourni est en 2134 × 800,
300 images à 30 fps, soit 10 secondes. Conversion de qualité Lanczos vers
1280 × 480 (au-dessus de la densité native), padding technique de 8 pixels
hors image et compression BGRA lossless BGC3/XPRESS-HUFF, sans BC1. Chaque
image convertie est décompressée et comparée avant publication. À 10 s,
cut direct vers les instruments vivants, sans fondu.

L'initialisation du plugin ne lit que l'en-tête du film. Son chargement et
son décodage se font sur le worker existant, uniquement quand un ICC5 est
alimenté ; aucun thread supplémentaire. L'écran reste opaque avant le
premier upload et l'horloge ne part qu'au premier rendu effectif.

Infotainment, temps depuis sa première image affichée :

- 0–500 ms : noir vers `icc5_splash_background`.
- 750 ms : ajout définitif de `icc5_overlay` et de la température.
- 1250–2250 ms : apparition Run Smart ; maintien jusqu'à 4250 ms ;
  disparition jusqu'à 5250 ms.
- Attente d'une seconde, puis 6250–7250 ms : apparition Detroit ; maintien
  jusqu'à 9250 ms ; disparition jusqu'à 10250 ms.
- Attente d'une seconde, puis 11250 ms : arrivée sur `icc5_home` avec overlay
  et heure 24 h.

Les logos restent centrés : Run Smart mesure 320 px de haut (−20 %),
Detroit 200 px (−50 %), en conservant leurs proportions.
Pause, changement d'acteur/session et nouveau contact suivent les règles
existantes. Les protections natives et générées sont noires opaques dès
la création SRV et au premier bind, sur contextes immédiats et différés.
Original/désactivé restitue le contenu natif antérieur, conservé intact.

## Navigation et interaction

Navigation est le module central du Home. La maison en haut à gauche
ramène à Home en Nav et Notrip. Les autres icônes ne déclenchent pas Home.
Les cibles suivent leurs modules, leurs déplacements et le zoom global ;
elles sont désactivées pendant le splash et hors mode tactile Ctrl+F9.

Le style 4 et la palette TomTom des Mack restent partagés : boussole,
instructions, vitesse/rue, écussons monochromes, ETA/durée/distance et
animation d'arrivée sur les dix derniers miles. Le layout ICC5 porte le
tag indépendant 9 ; aucune migration Garmin ne lui est appliquée.

`trip_revisisted` occupe le panneau droit : ETA en haut, distance et durée
sur la ligne suivante, avec une durée agrandie et le petit drapeau gris sur
cette même ligne, non sur l'axe de trajet. La distance est décalée à droite.
La colonne et le joueur finissent au-dessus de la zone basse ; l'heure courante
en bas reste inchangée. Instructions et vitesse sont agrandies de 30 % en Nav,
en conservant leur centre et l'ancrage bas de la vitesse. Ces ajustements visuels
ne réinitialisent aucun placement ou zoom personnalisé enregistré.
Les deux colonnes à 33 % gardent leur dégradé supérieur ; le drapeau et la
colonne avant se rapprochent du joueur avec la distance restante.
L'overlay et les couleurs des modules restent identiques jour/nuit.

La température ambiante native est lue par l'accès existant vérifié pour
ATS 1.61, formatée en °F, avec `--°F` si indisponible. Pas de valeur inventée.
Les bornes physiques tactiles sont générées depuis toutes les variantes
ICC5 auditées : elles excluent le cluster et ne filtrent pas les téléphones.

## Vérification

Importeur : `tools/import_cascadia_icc5.py`. Tests :
`OverlayPreviewTest <sortie> --icc5`, `--startup-guards`, `TouchPhoneTest`.
Ils couvrent les deux véhicules, le rendu et les timings, les UIs natives,
la séparation cluster/GPS, les copies réelles D3D11, la persistance et le
tactile. Le test de publication passe par le packing du worker, son véritable
upload, le crop lu dans la SII générée et le rendu GPU : huit bandes verticales,
y compris le tiers droit, sur ICC5/CoPilot/Sygic et les slots 0/31. Le centrage
est vérifié sur « 96°F », « --°F » et « -10°F ». Les captures de test ne
remplacent pas la confirmation dans ATS.
