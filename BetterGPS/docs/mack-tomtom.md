# Mack Anthem / Pinnacle — TomTom

Les deux GPS utilisent la même surface physique (154,68 × 93,59 mm), avec
un rendu BetterGPS 900 × 544. La région UV visible du GPS est mesurée sur
les meshes : (0,0057354 ; 0,2254548) à (0,9962873 ; 0,7708116).
Le cluster est une surface distincte ; ses petits LCD auxiliaires restent natifs.

## Démarrage et pages

- Cluster : texture 3 s, puis fondu alpha 0,5 s vers les instruments vivants.
- GPS : `mack_home_background` fixe pendant 2 secondes, sans lecture vidéo.
- Puis accueil progressif : top à +0,5 s, menus à +0,75 s,
  cases à +1,25 s. Aucun fondu au noir intermédiaire.
- Les horloges commencent au premier rendu affiché ; pause et extinction ne
  consomment pas les séquences. Les protections avant upload sont RGB(10,10,10).
- Après la séquence, les règles Home existantes s'appliquent (2 s par défaut,
  réglables). Le tactile est limité au mode menu CTRL+F9 et exclu du splash.
- Navigation : troisième case de l'accueil ou onglet Navigation. Retour Home :
  flèche Return en haut à gauche, en Nav comme en Notrip. Les cibles suivent
  les déplacements/échelles des modules et le zoom global du display.

## Carte et modules

Notrip conserve seulement les deux barres de gauche et la boussole animée.
Nav ajoute les instructions et la vitesse/rue, centrées et de largeur variable,
et le panneau trajet collé à droite. Les écussons utilisent les variantes
monochromes. La limite reste opaque ; le reste du module vitesse est à 50 %.
La carte TomTom jour/nuit utilise l'itinéraire #01EAF9 et un joueur cyan clair.
Les couleurs des modules ne changent pas la nuit.

Le trajet affiche l'ETA au format 24 h, la durée « 8h 5m » ou « 5 min », la distance
et le joueur fixe. Les chiffres sont agrandis et gras en Segoe UI ; la durée
en minutes seules est plus grande. Les annotations internes ne sont pas affichées.
Dans les dix derniers miles, le drapeau entre progressivement,
se rapproche du joueur et raccourcit la colonne avant ; la colonne arrière
reste fixe. Les deux colonnes sont à 33 %, toutes deux avec dégradé supérieur.
Les calculs utilisent les données de navigation existantes, sans nouveau thread.

## Assets et validation

Import : `tools/import_mack.py` et `tools/import_mack_cluster.py`.
Les DDS BC7 dont les dimensions ne sont pas multiples de quatre sont converties
hors ligne en RGBA32 : dimensions, pixels décodés et alpha sont préservés.
Les tests vérifient le fond fixe pixel pour pixel jusqu'à 1999 ms, puis les
étapes Home. Les anciens assets vidéo sont conservés, mais ne sont plus lus
ni requis pour le démarrage infotainment. Le cluster reste inchangé.

Tests : `OverlayPreviewTest <dossier> --mack` et `--mack-cluster`.
Ils couvrent acquisition GPS/cluster séparée, UV, timing, rendu jour/nuit,
ETA, progression du dernier mile, tactile et sauvegarde ; le test cluster
emploie de vrais contextes D3D11 WARP immédiats et différés.
Ces tests ne remplacent pas la confirmation visuelle dans ATS.
