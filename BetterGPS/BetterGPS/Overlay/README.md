# BetterGPS Overlay

Ce composant gère tous les écrans BetterGPS. Le rendu commun est partagé, mais les comportements propres aux téléphones et aux écrans embarqués restent séparés dans leurs modules respectifs (`Phone*` et `Device*`).

- `BetterGPS_assets/material/ui` contient les ressources communes et téléphone.
- `BetterGPS_assets/material/ford` contient les ressources d'infotainment Ford.
- `BetterGPS_assets/material/garmin/nuvi465t` contient les modules et splashs Garmin.
- `BetterGPS_assets/material/garmin/player_truck.dds` contient l'icône camion.
- `BetterGPS_assets/native_themes/garminmap_light` et `garminmap_dark` contiennent les palettes Garmin.
- `layouts/vertical` contient les surfaces téléphone.
- `layouts/horizontal` contient les écrans embarqués.
- `config/display-settings` contient les placements livrés par écran.

Les fichiers générés sont produits dans le dossier de build et ne doivent pas être copiés dans les sources.

## Garmin Nuvi 465T

Dans Add device, le GPS portable Cabin Accessories (`gps.dlc_toys.sii`) reçoit
le preset Garmin horizontal. Le sélecteur Style permet aussi d'appliquer Garmin
à un autre écran. Device format propose Horizontal ou 3:2 pour ce preset.
Les variantes nav/no-trip et light/dark suivent l'état de navigation et Map theme.
Chaque module, ses textes attachés, la caméra et le zoom restent configurables.

Player icon propose Theme default, Original et Truck. Theme default utilise le
camion pour Garmin et conserve les icônes Google/Waze pour les autres styles.
Les remplacements GPU concernent uniquement la carte privée de l'écran choisi.
Les placements sont enregistrés dans des fichiers distincts par device, style
et format ; les autres préférences sont dans BetterGPSDevices.ini.

Le splash Garmin est sélectionnable dans Splash image et couvre l'écran pendant
3 secondes après l'allumage. Une couverture noire protège la première image
tant que les pixels du nouvel allumage ne sont pas prêts.

Le GPS DLC produit une texture 512×256, mais son interface mesure 300×168 :
le preset compense ce mapping pour conserver les proportions physiques.
Les sources Nuvi mesurent 1200×672 (horizontal) et 1200×800 (3:2) ; leur rendu
CPU utilise respectivement 900×504 et 900×600 avec un facteur uniforme.
Les footers fournis ont des noms light/dark inversés ; la sélection suit leur
apparence. Les DDS BC7 non alignés sur des blocs de 4 pixels sont importés en
RGBA non compressé, sans modifier leurs pixels ni leurs dimensions.
