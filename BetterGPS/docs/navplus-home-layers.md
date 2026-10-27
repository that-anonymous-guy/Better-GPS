# Couches Home NavPlus HD

Le Home conserve le canevas logique 1324 × 800. Ses couches sont indépendantes :

- `navplushd_gradient_background.dds` : fond complet, inchangé.
- `navplushd_homescreen_toplayer.dds` : contenu de page rogné, 1244 × 586,
  enregistré à x=73, y=126. Cette position correspond à l'ancien contenu dans
  la couche 1324 × 641 placée à y=71 (rognage x=73, y=55). Aucun étirement ni
  déplacement des cartes ou de leurs hitboxes.
- `navplushd_page_full.dds` et `navplushd_page_empty.dds` : 34 × 34, indicateurs
  séparés à y=84. Pas de 44 px, groupe centré horizontalement. Le nombre vient
  du catalogue réel `copilotHomePages` ; seul l'indicateur actif est plein.
  Une seule page existe actuellement, donc un seul indicateur plein.

Le rendu et la garde anti-flash utilisent les mêmes définitions de pages. Le
Home peut donc démarrer avec la nouvelle couche sans l'ancien homescreen fixe.
Les transitions réutilisent la même composition que le Home stable.

Import ciblé sans toucher au fond ni aux métadonnées communes en attente :

```
python BetterGPS/Overlay/scripts/import_copilot_home.py SOURCE PREVIEW --foreground-only --no-shared-manifest
```

L'import vérifie les dimensions et l'égalité des pixels RGBA, puis émet un DDS
RGBA8 compatible et un reçu `home-layers-import.json`.

## Menu Gauges

Le nouveau `navplushd_gauges_background.dds` est un titre RGBA de 169 × 48,
placé à x=8, y=81 dans le même canevas 1324 × 800. Le fond RGB(10,10,10),
les six cadrans, leurs aiguilles et leurs hitboxes conservent leurs positions.
Les anciens fonds complets 1324 × 800 restent interprétés comme tels.

Les indicateurs utilisent les mêmes textures et coordonnées que Home, après
les cadrans et aiguilles. Leur nombre vient du catalogue des cadrans, avec six
emplacements par page : actuellement une page, donc un point plein centré.
Les vues individuelles agrandies et les alertes ne portent pas ces indicateurs.

Import ciblé du titre seul, sans réimporter les cadrans personnalisés :

```
python BetterGPS/Overlay/scripts/import_copilot_gauges.py SOURCE PREVIEW --background-only --no-shared-manifest
```

Le reçu indépendant est `gauges-background-import.json`.
