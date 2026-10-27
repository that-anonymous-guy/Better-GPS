# Volvo VNL 2018 / VNR — TomTom Trucker

Véhicules exacts : `volvo.vnl2018`, `volvo.vnr_e`. L'acquisition GPS exige
`/vehicle/truck/share/gps.tobj` et respectivement les UI
`/ui/dashboard/volvo_vnl2018_gps.sii`, `/ui/dashboard/volvo_vnr_e_gps.sii`.
Le cluster reste séparé ; les choix Original/désactivé existants sont conservés.

## Démarrage

- Cluster : 241 images à 30 fps (8,034 s), puis cut vers les instruments natifs.
- GPS : `volvo2018_home_background` fixe pendant 2 secondes, sans lecture vidéo,
  puis accueil Volvo + barre gauche. Identique sur VNL 2018 et VNR.
- Top à +500 ms, menus à +750 ms, cases à +1250 ms après les 2 secondes.
- Aucun fondu ajouté. Le décompte commence au premier upload visible, se fige
  en pause et repart selon les règles de contact/session existantes.
- Les deux UI natives portent des marqueurs distincts ; masque opaque RGB10
  dès la création SRV et récupération au premier bind, comme le correctif VNL14.
- Pas d'export natif supplémentaire ni de nouveau thread/décodeur vidéo.

## Pages et interaction

Canvas commun Mack/Volvo : 900 × 544. Le GPS Volvo mesure 153,40 × 92,44 mm,
ratio 1,65945 ; ajustement à ses UV natifs (0,00460482 ; 0,00522709) à
(0,99539518 ; 0,99477291) dans le buffer 512 × 256, densifié à 1024 × 512.
Ces mêmes coordonnées servent au rendu et au tactile inverse.

Nav/Notrip, palette TomTom, boussole, instructions, vitesse, durée, ETA,
écussons monochromes et animation des dix derniers miles sont partagés avec
les Mack. Seule la barre gauche devient `volvo_backtohome_left`.

Accueil Volvo : fond et top à droite de la barre ; menus sous le top en
recadrant leur marge supérieure opaque ; cases sous les menus. La zone
Navigation couvre la troisième case gauche, pas les autres applications.
Retour carte→Home : flèche en haut à gauche, sur Nav comme sur Notrip.
Les gestes sont ignorés pendant l'intro et hors menu Ctrl+F9. Offsets/zoom
personnalisés sont inversés avant le hit-test, comme les Mack.

L'heure Home est l'heure du jeu, en 12 h avec AM/PM : centrée dans le top
Volvo ; à droite du top Mack. Elle apparaît avec le top, pas avant.

Importeur : `BetterGPS/Overlay/tools/import_volvo2018.py`. DDS décodées en
RGBA32 et BGC3 lossless vérifiés image par image. Assets sources renommés
`mack_volvo_infotainment`, mais chemin runtime Mack antérieur conservé.

Test ciblé : `OverlayPreviewTest <sortie> --volvo18` (deux véhicules,
identités/UV, pages, timing, horloge AM/PM, hitboxes et vrais binds/rendus GPU
immédiats/différés, première image et cut sans fade).
