# Kenworth T680 2022 — NavPlus HD / CoPilot V9

## Identification et format

L'affectation automatique exige un écran intégré effectivement observé :

- véhicule `kenworth.t680_2022` ;
- UI `/ui/dashboard/kenworth_t680_2022_gps.sii` ;
- matériau `/vehicle/truck/share/gps.tobj` ;
- paire GPS native avec un emplacement de fenêtre distinct.

Le cluster `/ui/dashboard/kenworth_t680_2022.sii` / `dashboard.tobj`, le
T680 classique et les accessoires de pare-brise sont exclus. Un choix enregistré
« Original » ou un profil existant n'est jamais remplacé automatiquement.

La surface physique mesure environ 154,20 × 93,20 mm (1,65452:1). Le tampon
natif est **1024 × 512**, ce qui n'est pas son ratio physique. Les assets fournis
sont **1324 × 800**, rendus dans un canevas de travail de **900 × 544**.
Le rectangle mesuré dans le tampon natif est `(1,0,1022,511)` ; le même calcul
sert à l'UI générée et à l'inversion des coordonnées tactiles.

## Démarrage et pages

Pour le splash `copilot/t680`, chaque appareil possède sa propre horloge :

1. 500 ms en RGB(10,10,10), opaque.
2. Les 98 images du MP4 mis à jour à 30 fps, une seule lecture (3,267 s).
3. 250 ms en RGB(10,10,10), opaque.
4. 500 ms de `navplushd_bottom_5by3` sur fond opaque RGB(10,10,10).
5. Home, puis Nav ou Notrip selon le système existant de premier démarrage.

Total avant Home : **4,517 s**. Le délai Home configurable existant reste
indépendant (2 s par défaut, uniquement après une initialisation confirmée du
véhicule). L'horloge du splash ne démarre qu'après le premier envoi GPU valide,
se fige en pause et repart à l'allumage suivant. Aucun bouton n'agit pendant
cette séquence. Les assets absents ou invalides maintiennent un écran opaque.

Le MP4 est converti hors jeu en BGA2 BC1 : 98 entrées chronologiques, 73 images
uniques, 900 × 544 pixels utiles. Aucun thread vidéo supplémentaire ni lecture
MP4 n'est créé dans ATS. `import-manifest.json` conserve les empreintes sources.

## Présentation et tactile

Les quinze emplacements habituels sont indépendamment déplaçables et
redimensionnables : barre haute, Home, zoom, manœuvre/distance, route/écussons,
footer Nav, footer Notrip, horloge/jour, distance restante, ETA, vitesse réelle,
commandes basses et limitation. L'ETA ne contient pas AM/PM ; le jour n'est
préfixé que pour une arrivée un autre jour. Aucun rappel de manœuvre n'est
dessiné au-dessus du footer.

Les coordonnées des boutons proviennent des images sources, puis suivent le
placement du module et le zoom global :

- Home : carte Navigation `x=400..610 /1324`, `y=153..390 /800`.
- Nav/Notrip : maison `x=586..738 /1324`, dans le module des commandes basses
  (source `y=710..800`).

Les interactions nécessitent le mode Ctrl+F9, une surface unique reconnue,
des générations de véhicule/alimentation/page cohérentes et un splash terminé.
Elles réutilisent la capture native, les preuves de copie et le picker CPU/depth
existant ; le replay des shaders natifs reste désactivé.

## Thème

Le style appareil **CoPilot V9** possède deux palettes natives distinctes,
`native_themes/copilot_light` et `copilot_dark` : crème/blanc/ocre en journée,
anthracite/gris/orange la nuit, itinéraire **vert dans les deux modes** selon
la demande. Les captures utilisateur servent de référence visuelle ; les
couleurs sont une adaptation aux catégories cartographiques disponibles dans
ATS, pas une reproduction du moteur ou des données cartographiques CoPilot.
Les icônes natives sont conservées ; le camion reste sélectionnable séparément.

Le manuel V9 et l'aide officielle documentent les palettes jour/nuit et
l'itinéraire vert :

- [CoPilot Truck v9 User Guide](https://customer.omnitracs.com/training/docs/copilot/CoPilot_Truck_v9_UG.pdf)
- [CoPilot User Guide — Map Display / My Route](https://apphelp.copilotgps.com/copilot/en-us/)
- [API officielle jour/nuit depuis CoPilot 9.2](https://developer.trimblemaps.com/copilot-navigation/sdk-app/api-functions/settings/navigation-and-display/)

L'identifiant du style enregistré est `3`. Les combinaisons internes palette /
icône 0..8 restent inchangées ; CoPilot utilise 9..11. Le fichier de placements
est `device_<slot>_5by3_copilot.cfg`, séparé des profils Google, Waze et Garmin.

## Validation

`OverlayPreviewTest <dossier-neuf> --copilot` vérifie l'identification GPS-only,
le rectangle UV, tous les intervalles opaques de démarrage et les 98 frames,
la pause/réinitialisation de l'horloge, les textes dynamiques, le panneau nuit,
l'excès de vitesse, les palettes vertes, le tactile Home/Nav/Notrip, les modules
déplacés/zoomés et la persistance des profils indépendants.

`TouchPhoneTest <dossier-neuf>` couvre aussi CoPilot avec des copies natives
réelles D3D11 WARP, un tampon 1024 × 512, la projection 3D, les preuves de copie,
le picking CPU/depth, le hover et les actions. Ces tests hors jeu ne constituent
pas une certification visuelle dans une session ATS réelle.

## Correctif tactile et lisibilité — 21 octobre 2026

Les captures ont montré qu'une texture portant l'association du GPS pouvait
aussi être échantillonnée par le cluster. La preuve de copie valide ne prouve
pas à elle seule **quelle surface physique** l'échantillonne. Le relevé natif
contient exactement les dix sommets du cluster, pièce 39 (centre X ≈ 0 m,
largeur 362 mm), et non ceux du GPS, pièce 38 (centre X ≈ +0,403 m, 154 mm).

Pour ce T680 uniquement, l'acquisition sépare les cadences par appareil, forme
de draw et cible native : le premier draw du cluster ne bloque plus celui du GPS
pendant 66 ms. Les passes hors du viewport/résolution principal sont exclues
avant readback. Seuls les triangles du plan GPS mesuré dans les cinq cabines
stock sont retenus, avec une marge millimétrique explicite, pas un rectangle
écran deviné. Toutes les preuves de copie, génération et profondeur restent
nécessaires. Une géométrie inconnue est rejetée plutôt que d'autoriser le cluster.
Les budgets restent bornés et aucun replay natif n'est ajouté. Les téléphones
et anciens GPS conservent leur acquisition existante.

Le test GPU dessine maintenant le cluster (24 indices) avant le GPS (66 indices),
avec une même texture taggée, leurs vraies positions modèle et une projection de
test explicite. Il vérifie que seul le GPS fournit le hover et les contacts.

Les instructions/distances sont agrandies, le texte et les écussons décalés à
droite, le cadran horaire légèrement à droite et le jour centré dans l'espace
restant. `CoPilotLayout 2` migre les origines concernées en conservant les
décalages personnalisés et toutes les échelles ; la migration est idempotente.
Les écussons d'instruction sont des copies monochromes à fond blanc et chiffres
noirs. Les images colorées partagées avec Google/Waze restent intactes.
Le joueur CoPilot par défaut a sa propre matière rouge avec un disque blanc,
sans modifier les choix explicites Original ou Camion. Les tests contrôlent
également ces pixels, l'alpha et l'isolation du catalogue d'écussons partagé.

## Noir initial — correction du flash gris

Le noir d'attente RGB(10,10,10), utilisé avant le premier upload et lors d'une
nouvelle ignition, était déclaré en UNORM linéaire alors que l'overlay préparé
est sRGB. Le passage dans la cible sRGB native transformait ce premier noir en
RGB(56,56,56), puis l'upload revenait brusquement à RGB(10,10,10). La texture
d'attente CoPilot utilise désormais le même encodage sRGB. Le noir pur des
autres appareils et la durée complète de la séquence restent inchangés.

`OverlayPreviewTest <dossier-neuf> --copilot-startup-gpu` reproduit l'écart avec
le binding réel du marqueur, sans image CPU disponible, puis après son premier
upload. Le test effectue des draws D3D11 WARP immédiats et différés, relit les
pixels sur des cibles sRGB et linéaires, et contrôle aussi la nouvelle ignition,
la contention du worker, l'horloge du premier upload visible et le noir Garmin.
Avant correction : 56 contre 10 ; après correction : 10 contre 10 en sRGB.

## Intro du cluster (indépendante du GPS)

Le cluster physique 361,70 × 128,99 mm (2,804:1), UI 1440 × 540 et buffer
1024 × 512, reçoit une couche supplémentaire dans son UI d'origine. Les cinq
groupes et tous les instruments/pages de la version ATS 1.61 auditée sont
conservés : aucun cluster privé, remplacement de logique ni double export UI.
Cette UI est spécifique à `kenworth_t680_2022.sii`, pas au GPS `_gps.sii`.

Le fichier fourni `t680_clusterintro.mp4` fait 300 images à 30 fps. L'import
offline conserve les 270 premières images (9 s), puis remplace la dernière
seconde par un fondu alpha de l'image 269 vers les instruments natifs vivants.
La durée totale reste 10 s : aucune image noire ou capture figée du cluster
n'est intercalée. Le fichier source est conservé intact. L'asset BGC3 contient
256 images uniques BGRA8 sans perte, avec une chronologie de 270 entrées.

Le contact électrique confirmé arme une horloge indépendante. Elle démarre
au premier upload effectivement utilisé par le marqueur de cette UI, se fige
pendant les menus/pauses et ne redémarre pas au retour d'un menu. Une nouvelle
mise du contact ou initialisation du véhicule la réarme. Le showroom, un
véhicule différent, l'alimentation inconnue et BetterGPS globalement désactivé
ne lancent pas l'intro. Une vidéo absente/invalide laisse les instruments natifs.

L'import précompresse les images ; le worker CPU existant prépare seulement
l'image nécessaire. Aucun décodeur vidéo, thread additionnel, polling GPU,
replay natif ou fichier temporaire n'est créé dans ATS. L'alpha est appliqué par
le shader UI natif, également sur les contextes différés. Le rafraîchissement
forcé ne concerne que la fenêtre du cluster positivement identifiée et pendant
l'intro. Le marqueur redevient transparent à la fin. Avant le contact et pendant
l'attente d'initialisation, il reste opaque en RGB(10,10,10).

`OverlayPreviewTest <dossier-neuf> --t680-cluster` vérifie la chronologie,
l'identité du cluster, le contenu natif préservé, les 270 images et le rendu
GPU immédiat/différé du fondu sur des instruments simulés qui changent sous
l'intro. Il couvre aussi pause, contact, rallumage et isolation des sessions.
Les événements `[ClusterIntro]` indiquent asset prêt, première image visible,
début du fondu et retour intégral aux instruments. Validation hors jeu ; la
position finale et la lecture en cabine restent à contrôler dans ATS.

### Contrôle SII après le crash de chargement

Le journal ATS a rejeté le premier identifiant ajouté, dont le dernier composant
dépassait la limite native de 12 caractères. Le marqueur s'appelle désormais
`_nameless.bgcl_intro`. La déclaration et la référence enfant utilisent le même
nom. Le parseur tiers TruckLib acceptait le nom trop long : son succès ne
certifiait donc pas la compatibilité du chargeur ATS.

Le test GPU vérifie maintenant explicitement les tokens de toutes les unités,
avec un contrôle négatif sur l'ancien nom. L'importeur et le générateur de
paquet vérifient également longueur/alphabet, doublons et liens UI locaux avant
d'écrire ou distribuer le fichier. Règle officielle :
https://modding.scssoft.com/wiki/Documentation/Engine/Units

### Qualité et protection avant initialisation

Le premier import BC1 a introduit des artefacts rouge/blanc par blocs 4 × 4.
Ils sont reproductibles hors ATS : 1088 pixels de débordement blanc dans les
zones rouges de l'image 269. Ce n'était pas une limite du moteur SCS.
L'intro utilise désormais du BGRA8 compressé sans perte (XPRESS-HUFF Windows,
format BGC3 distinct du lecteur BC1 des Ford/GPS). Chaque image est vérifiée
après décompression lors de l'import ; le test C++ vérifie aussi les pixels
finaux contre la référence indépendante. Les 270 hashes sont conservés dans
le manifeste. Le noir neutre 9 issu du décodage H.264 est rétabli au RGB10
explicitement demandé. Le MP4 source n'est pas modifié.

Le masque opaque sRGB est préparé dès la création native du marqueur, même si
le gameplay n'est pas encore activé. Il protège aussi le chemin de binding en
cache, avant que l'éligibilité du véhicule ou le contact ne soit confirmé.
La durée de l'intro ne commence toujours qu'avec la première image réellement
uploadée. Le test GPU couvre cette course initiale, l'attente, le rallumage,
le rendu immédiat/différé et le RGB10 exact après rendu sur une cible sRGB.
Les autres appareils et le décodage des intros F150/Bronco restent inchangés.

### Modules CoPilot V9 — 24 octobre 2026

Le T680 nouvelle génération ne découpe plus ses anciens overlays Nav/Notrip.
Il compose `navplushd_top_5by3`, `copilot_v9_zoom` sur les deux pages,
`copilot_v9_belowfooter` en Nav ou le petit `copilot_v9_notrip` seul en Notrip.
Le Home utilise son nouveau fond sans bandeaux intégrés. Les textures top et
bottom (1324×92 et 1324×90) sont des couches indépendantes communes à Home,
Notrip et Nav, sans recadrage ni étirement plein écran. Chaque module garde son
placement et son échelle éditables. Le bouton Apps suit son module Nav/Notrip ;
le retour par la maison inférieure est conservé.

`CoPilotLayout 3` adapte les anciens profils en préservant leurs décalages et
échelles. Les fonctions Home, horloge, films cluster/GPS, durée, protections anti-flash,
palette et télémétrie sont inchangés. Les autres camions ne sont pas migrés.
L'import ciblé `scripts/import_copilot_modules.py` vérifie les pixels RGBA des
six DDS sans réencoder ni remplacer les vidéos.

### Zoom tactile par vues natives

En Nav et Notrip, seuls les deux carrés du module `copilot_v9_zoom` sont
cliquables : + revient au preset précédent, − passe au suivant. L'ordre est
3D proche, 3D éloignée, 2D proche, 2D éloignée. Les extrémités sont bloquées,
sans rebouclage ni zoom supplémentaire. Les zones suivent le déplacement,
l'échelle du module et le zoom global de l'interface.

La sélection est temporaire et propre à la carte du T680 nouvelle génération.
Les autres GPS et les réglages caméra enregistrés ne sont pas modifiés. Le
changement de vue natif du jeu reprend la main dès qu'un nouveau mode est
observé ; la destruction/recréation de la source efface aussi la sélection.
Le producteur natif reçoit directement le mode choisi, avec les offsets du
bon preset. Aucun appui clavier n'est simulé, aucun rendu de carte n'est rejoué
par le tactile et aucune carte n'est simplement agrandie comme une image.

### Exploration tactile de la carte

En Nav et Notrip, maintenir le clic sur la carte puis glisser déplace la
caméra native. Le point exploré reste ancré dans le monde pendant que le
camion avance. Le bouton « re-center », blanc à 50 % d'opacité et texte noir,
apparaît au-dessus d'Apps à gauche ; il rétablit immédiatement le suivi.
Il occupe le module éditable 13. `CoPilotLayout 4` place ce nouveau module
sans déplacer les modules déjà personnalisés.

Un geste commencé sur zoom, Apps, les bandeaux, les vitesses ou re-center
reste réservé au bouton. Un glissement commencé sur la carte ne déclenche
aucun bouton en passant dessus. Les échantillons gardent les validations de
profondeur, appareil, page, alimentation et génération du véhicule.

Le décentrement est temporaire, limité aux cartes CoPilot du T680 nouvelle
génération, et n'est jamais écrit dans les profils caméra. Home, extinction,
changement de véhicule ou de vue native le réinitialisent. Les presets zoom
tactiles restent disponibles. Le pan ne modifie ni la position du joueur ni
le centre natif empaqueté. L'orientation continue de suivre le cap natif.
Les déplacements sont limités à ±5000 unités sur chaque axe ; la disponibilité
de la géométrie reste celle du producteur natif ATS.

Les tests hors jeu couvrent le chemin natif, les gestes maintenus, Nav/Notrip,
les priorités des boutons, le retour au suivi et les changements de génération.
La sensibilité de glissement et les limites de géométrie sont à confirmer dans
ATS sur l'écran physique ; la projection utilise une focale normalisée.

### Application et Driving Menu

Le clic attend 500 ms sur Home, puis le glissement d'ouverture de CoPilot dure 320 ms (sens inversé après essai en jeu).
À la fin du swipe, `copilot_v9_logo` apparaît en cut sur fond opaque RGB(10,10,10) pendant 3 secondes, puis coupe vers la carte. Le logo conserve ses proportions dans une zone centrée de 450×272, soit la moitié des dimensions du canevas (rendu 330×272). Les
bandeaux Kenworth restent fixes. Le retour Home attend 250 ms sur l’application courante, puis fait glisser la carte réelle
et ses modules dans le sens inverse de l’ouverture pendant 320 ms, avec Home qui entre du côté opposé,
sans attente noire. La fermeture utilise une capture GPU du contenu déjà
rendu, avec restauration du contexte graphique et garde par appareil/session.

Apps, en Nav comme en Notrip, ouvre maintenant le Driving Menu fourni. Le
module 2 mesure 660×372 sur le canevas 900×544 ; il monte depuis le bas pendant
260 ms jusqu'au centre. Le fond est assombri à 50 %, en conservant les bandeaux
et leur horloge. `CoPilotLayout 5` ajoute ce module en conservant les autres
placements, le pan et les profils caméra.

- Les six grandes cases ouvrent la carte ATS par la commande clavier M.
- Clear Destination demande le reset natif du GPS, sur le thread du jeu.
  Il reste inactif sans trajet ou pendant une mission. Le contrôleur de navigation
  est autorisé en parcours libre : ses points sont effacés avant le GPS. Les call sites et le prologue
  sont vérifiés sur la version ATS déjà supportée ; aucune structure de route
  n'est modifiée directement et aucun effacement visuel artificiel n'est fait.
- Main Menu ferme l'application avec le glissement vers Home.
- La croix ferme seulement le Driving Menu en le faisant descendre.

Le menu intercepte les clics et le pan en arrière-plan. Les zones suivent son
placement, son échelle et le zoom global de l'interface. Les animations sont
interrompues en cas de changement d'appareil, d'alimentation ou de génération.
Les tests hors jeu ne certifient pas l'exécution d'une commande dans une partie
ATS réelle ; ouverture de la carte et effacement natif restent à vérifier en jeu.

La carte et ses modules utilisent maintenant la même composition GPU sur les
pages Nav/Notrip : le voile à 50 % couvre donc aussi le fond de carte. Les
pixels transparents des bandeaux reposent sur un fond RGB10 opaque pendant les
glissements. Le cache distingue la dernière image de sortie du menu de l'état
complètement fermé, même si les deux sont dans la même tranche d'animation.
La luminosité complète est ainsi republiée à la fermeture. La maison du bandeau
inférieur et Main Menu dans le dialogue restent tous deux actifs vers Home.

Le diagnostic en jeu a identifié le refus restant de Clear Destination : la
table de fonctions SCS est en lecture seule, alors que `map_memory` contrôle
volontairement des destinations modifiables pour les palettes. Le contrôle de
cette table utilise désormais les bornes de l'image exécutable, puis valide
que sa méthode est du code exécutable. Le test place la table dans la section
en lecture seule du binaire pour reproduire ce cas, au lieu d'une table sur
la pile. Les motifs de refus et le mode natif sont consignés par clic.

La liste officielle des commandes console SCS ne documente pas d'effacement
du trajet ; `route` y sert au débogage entre deux entreprises. Cette liste est
signalée ancienne, donc cela ne prouve pas l'absence de toute commande interne :
https://modding.scssoft.com/wiki/Documentation/Engine/Console/Commands
Le plugin continue d'appeler les fonctions natives utilisées par le bouton
Reset Navigation, sans passer par une commande console supposée.

La limite supérieure du contenu animé est à 51 px : le DDS du bandeau occupe
63 px mais ses derniers 12 px sont transparents et recouvrent les points de
pagination du Home. Le fond Home est déplacé avant de recomposer cette bordure,
afin de conserver les points entiers à l'ouverture comme à la fermeture. La
capture GPU de sortie commence aussi à 51 px. Le masque du fond de carte reste
à 63 px pour conserver la protection contre la carte visible dans le bandeau.

Les délais de clic et de chargement utilisent l'horloge de page existante,
sans attente bloquante ni thread supplémentaire. Ils restent propres au T680
CoPilot : les animations des téléphones et des International sont inchangées.
Le retour garde la capture réelle de l'application pendant les 250 ms, puis
anime la fermeture dans le sens validé ; il n'ajoute pas d'attente noire.
## Gauges application

The T680 2022 Home Gauges tile opens a separate page. It reuses the map app's
500ms click latency, 320ms horizontal slide and 3-second loading hold. During
that hold, `navplushd_gauges_background` replaces the CoPilot logo; the six
gauges appear together at the end. Shared top/bottom chrome and the live clock
remain fixed. Back, Home and the configured Home shortcut return with the
250ms latency and inverse slide. An active route never redirects this page to
Nav, and map zoom/pan/Driving Menu targets are disabled on Gauges.

The six supplied DDS faces use a three-column, two-row grid measured from the
user's layout. Their source-space hub is `(164.5,164)` within a 329x286 face.
The 163x75 needle rotates around its own hub at `(125.5,37)`, not its texture
midpoint. Premultiplied bilinear resampling prevents fringes while keeping that
hub stationary. Different minimum angles match the supplied left-stop photo:

| Gauge | Data / conversion | Limits | Sweep in image coordinates |
|---|---|---|---|
| Oil temperature | SDK `truck.oil.temperature`, Celsius to Fahrenheit | 100–300 °F | 158–382° |
| Transmission temperature | Guarded native differential temperature, Celsius to Fahrenheit | 150–300 °F | 158–382° |
| Boost | Native turbo signal mapped through the stock T680 0–1 animation scale | 0–80 on the dial | 180–360° |
| DPF | Deliberately fixed CLEAN | Fixed | 180° |
| Average MPG | SDK average L/km to US MPG | 0–11.760729… MPG | 180–360° |
| Torque | Native turbo signal normalized to the stock T680 Torque bar's 0–0.45 range | 0–100% | 160–380° |

Oil, transmission, boost, MPG and torque clamp at both mechanical stops. DPF
does not simulate soot or regeneration. Torque and boost deliberately retain
different stock T680 calibrations; Torque is not a physical engine torque
measurement. The transmission display deliberately uses differential data.

The MPG upper stop follows the stock T680's 5 km/l economy-bar limit, converted
to US gallons. This provides a useful scale for real T680 averages: Kenworth's
[Hoosier Air report](https://www.kenworth.com/media/ymkhwom5/hoosier-air-transport.pdf)
reports an 8 MPG fleet average and 9 MPG for its top driver. Those are operating
examples, not guaranteed fuel economy or hard physical limits.

`T680GaugeTelemetry.h` validates the installed executable's exact native read
instructions before accessing the owner vehicle's differential/turbo fields.
Only the matching T680 and confirmed actor epoch may publish native values.
Null, non-finite, out-of-range and stale sources fail closed. SDK no-value
callbacks clear readiness; truck configuration changes reset the data. The
existing preparation worker renders quantized quarter-degree needle changes;
there is no new thread, render-thread file I/O, native simulation write or
profile migration. DDS imports verify exact source RGBA pixels.

Validation: `OverlayPreviewTest --t680-gauges` covers SDK/native conversions,
mechanical stops, the real needle pivot/tips, screenshots, Home/Gauges/Nav
touch, route persistence, shortcut and timings. `--copilot-menu` also exercises
Gauges composition and closing without a native map on deferred and isolated
GPU contexts. Existing CoPilot, device pages and physical touch regressions
remain required. Offline fixtures do not certify values while driving ATS.

### Individual gauges and Return history

Each of the six overview faces is touchable and opens its own centred 466x405
view, preserving the original face aspect and needle hub. Detail uses a plain
RGB(10,10,10) background under the shared chrome. It reuses the standard click
delay, opening slide and loading hold. Return closes back to the six-gauge
overview with the standard 250ms delay and inverse slide; a second Return
returns to Home. The house and Home shortcut always return directly to Home.
The bounded per-device history is cleared on player generation changes and
explicit Home. It does not change other truck or phone navigation.

The user's corrected MPG DDS (325x284) replaces the original 329x286 face.
It is imported with exact source RGBA pixels and fitted to the existing gauge
bounds. Its green opacity gradient is displayed unchanged: no procedural
recolouring, hue sweep or dependency on the needle position is applied.

The lower MPG label shows the live average to one decimal in the free numeric
field beside its small AVG caption, matching the supplied photo. It appears in
both overview and detail; an unavailable initial average shows `--.-`. The
numeric value remains independent of needle clamping. The 0–11.760729… US MPG
scale is unchanged: the later 7.1 MPG reference photo was from another truck
and is explicitly not a calibration reference.

SDK MPG null, non-finite and zero packets retain the last positive average
until the next valid sample or truck configuration reset. A temporary native
differential/turbo read failure no longer suppresses independent SDK oil/MPG
values. This prevents a minimum-stop frame between successive averages without
adding a synthetic average or smoothing away real changes.

### Fast Gauges and temperature warnings

Gauges overview and detail now open in **820ms**: the existing 500ms click
latency plus 320ms swipe, without the former 3-second loading hold. Navigation
keeps its CoPilot logo hold. Return still uses 250ms latency plus 320ms swipe.

Oil above **250 °F** and transmission/differential above **280 °F**, with valid
data, automatically open the corresponding full-size gauge. Its entire face,
labels, frame, hub and needle use a red tint until it returns below or to the
threshold. Other gauges and shared chrome retain their colours. The mask is
part of the preparation key so a threshold crossing redraws even if the
quarter-degree needle angle has not changed.

Each crossing opens once per display/player generation. Acknowledging with
Return or Home is allowed while the temperature stays high; it does not
continually force the page open. Cooling rearms that warning. Pending warnings
wait for startup/other page animations or the settings editor to finish. If
both thresholds cross together, oil has priority; both faces are red in the
overview. No simulation values are written.

When a warning interrupts Nav/Notrip, the actual GPU map/UI snapshot slides
out, rather than showing Home in its place. Return restores that map. Warning
messages in the game log use `[T680Gauges] overheat` with the source Fahrenheit
value. GPU tests cover both directions on deferred and isolated contexts.

The Return hitbox is measured from the real bottom texture: the visible arrow
spans source x=390–468. Its target now covers x=370–490, including its right
edge, in Home-independent Nav/Notrip and gauge navigation. Tests click both
sides of the actual arrow on Nav and Notrip.

### Replacing artwork manually

With ATS closed, users can replace artwork in the installed
`BetterGPS_assets/material/copilot/t680` directory, preserving names, dimensions,
alpha and needle alignment. For non-multiples-of-four sizes such as the MPG
face's **325x284**, export **uncompressed 32-bit RGBA/BGRA DDS**. A BC7 325x284
replacement was rejected by D3D11 WARP with `E_INVALIDARG (0x80070057)`, while
BC7 328x284 and uncompressed BGRA 325x284 were accepted. A required image that
cannot decode keeps this display's startup readiness guard black. The import
scripts normally avoid this by converting supplied artwork to uncompressed DDS.
The repair preserves the user's latest installed image and verifies exact
decoded RGBA equality; it does not revert to an older design or resize it.
