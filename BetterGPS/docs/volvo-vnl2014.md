# Volvo VNL 2014 — Dynafleet / Garmin 600

L'acquisition est limitée au véhicule `volvo.vnl`, à l'alias GPS natif et aux
UI `/ui/gmaps_volvovnl_gps.sii` ou `/ui/gps.sii`. Le cluster `volvo_vnl.sii`
reste un producteur distinct. Les VNL 2018/2025 ne sont pas concernés.

## Démarrage

Le cluster lit les 180 images à 30 fps de `vnl2014_cluster.mp4` (6 s), puis
conserve la dernière image pendant un fondu alpha de 0,5 s vers les instruments
natifs vivants. Le buffer natif et ses autres valeurs ne sont pas remplacés.
L'overlay couvre les 600 lignes du haut de l'atlas 800 × 800, soit les
coordonnées UI SCS y=200..800 (origine en bas), sans toucher aux lignes restantes.
La vidéo est stockée en BGC3 sans perte, sans décodeur vidéo ou thread nouveau.

Le GPS affiche un fond opaque RGB(245,245,245) pendant 3 s comptées depuis
le premier upload visible, puis l'accueil `volvydynafleet_home`. Avant contact,
le masque est RGB(10,10,10). Les protections transparentes sur les UI d'origine
évitent une image native préalable et respectent le choix Original/désactivé.
Pause, reprise et nouvelle mise de contact réutilisent les horloges existantes.
Le masque opaque du cluster est attaché dès la création de sa vue GPU, avant
la première liaison. Une texture streamée non encore identifiée est vérifiée
dès cette première liaison immédiate, sans laisser passer une image native.

L'étape Home et sa durée restent réglables ; le raccourci de changement de page
fonctionne. Il n'y a **aucun tactile**, ni sur Home ni sur Notrip/Nav : le GPS
n'est pas tagué pour le picking et les gestes éventuels sont rejetés.

## Rendu et format

- Canvas GPS : 900 × 457, issu des assets 1181 × 600, ratio proche de 1,970:1.
- Région GPS visible : UV (0,0017121 ; 0,2469933) à (0,9982884 ; 0,7530067).
- Cluster : vidéo 1124 × 700, ratio proche de 1,605:1, quatre colonnes de
  garde supplémentaires exclues par le crop du marqueur.
- Notrip : seul l'asset `garmin600_notrip`, au-dessus de la carte Garmin native.
- Nav : `garmin600_nav`, instructions blanches textuelles sans flèches ni
  écussons ; Arrival/ETA à gauche et rappel manœuvre/distance à droite.
- ETA : heure du jeu + durée du trajet, format 12 h sans zéro initial,
  avec A/P puis M empilés verticalement. Chiffres ETA et distance en gras.
- Nuit : ajout de `garmin600_nav_dark_overlay`, textes du bas inversés.
  Les autres éléments de l'habillage restent identiques.
- Palette cartographique : Garmin existante, aucune nouvelle palette.

Importeur : `tools/import_vnl2014.py`. Les DDS BC7 de largeur 1181 sont
converties en RGBA32 en conservant tous les pixels/alpha décodés. Les 180
images vidéo sont vérifiées par décompression après l'encodage.

Test ciblé : `OverlayPreviewTest <sortie> --vnl2014`. Il couvre les identités,
UV, pages, absence de tactile, formats, images jour/nuit et vrais contextes
D3D11 WARP immédiats/différés pour le blanc RGB245 et le fondu du cluster.
La validation visuelle finale dans ATS reste nécessaire.
