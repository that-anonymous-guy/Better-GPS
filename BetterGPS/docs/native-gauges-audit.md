# Valeurs des jauges natives ATS — audit du 24 octobre 2026

Audit des fichiers stock installés et du code d'`amtrucks.exe` 1.61.3.18.
L'absence de canal dans le SDK ne permet pas de conclure à l'absence de valeur
dans le jeu. Le tableau de bord natif constitue une autre source exploitable.
Cet audit initial ne modifiait ni la DLL ni l'installation du plugin. La page
Gauges du T680 implémente maintenant les lecteurs SDK huile/consommation et les
lecteurs natifs différentiel/turbo décrits ci-dessous ; voir
`kenworth-t680-2022-copilot.md`. Leur comparaison en roulant reste à faire.

## Sources et vérifications

- Extraction stock fraîche par `AssetInspector --screen-inventory`, avec
  `IgnoreMods=true`, dans `build/gauge-audit-20261024/stock/extracted`.
- Les 29 définitions d'écran extraites sont identiques à celles du précédent
  inventaire du 20 octobre. Les 13 avertissements d'extraction concernent des
  includes de siège VNL2018/VNR absents et n'affectent pas les écrans audités.
- Analyse des widgets dans `widgets.json` et `summary.txt`, de leurs IDs,
  valeurs, types et animations d'intérieur.
- Vérification des pistes PMA v5 contre le squelette du **modèle animé**,
  distinct du modèle statique. Résultats : `animation-bones.txt`.
- Désassemblage ciblé du code natif, conservé dans
  `dashboard-update-full.asm` et `setters.asm`.
- Douze assertions sur les assets et instructions machine :
  `build/gauge-audit-20261024/verification.json`.
- SHA-256 de l'exécutable :
  `c60e8ee02cc97b3cc6ef8c6298bb5c3c23c4ffd330b8185b646fb1b608a3c7b4`.

## Valeurs accessibles

| Affichage | Source identifiée | Remarque |
|---|---|---|
| Oil temp, oil PSI, coolant, batterie V | SDK et widgets natifs 1010 / 1760 / 1090 / 1320 | Températures et pressions convertibles dans les unités voulues. |
| Primary / Secondary Air, T680 2022 | Widgets 1120 / 1370 | Le fichier dessine deux barres à partir du même widget 1120 et un seul signal de pression ; pas deux capteurs distincts démontrés. |
| Fuel quantity, fuel range, DEF | SDK ; widgets 1060 / 1080 / 1140 / 1510 / 1520 | Quantité, autonomie et niveaux. |
| Fuel economy moyenne | SDK ; widgets 1170 / 1360 | Valeur moyenne ; ne pas confondre avec l'instantané. |
| Fuel economy instantanée / fuel flow | Widget 1160 et source native de consommation | Le formateur 1160 change entre économie en mouvement et débit à faible vitesse. Un affichage de débit permanent doit lire la source avant formatage. |
| Boost | `turbo_anim` et widget 1130 | Source native localisée, absente des canaux de notre SDK. |
| Torque affiché sur T680 2022 | `_nameless._.torque_bar`, ID **1130** | SCS utilise ici le signal turbo avec une plage 0–0,45. On peut reproduire cette barre ; ce n'est pas une preuve d'un couple physique instantané en %. |
| Front / Rear drive axle temp, T680 2022 | `diff_temp_anim` | Le même fichier anime les bones 56 `g_fdifftmp` et 59 `g_rdifftmp` : les deux aiguilles partagent une source. |
| Brake App | `brake_pressure_anim` | Pression appliquée aux freins ; distincte de la pression du réservoir. Chemin d'animation confirmé, lecteur runtime à raccorder. |
| Suspension load | `suspension_load_anim` ; widget 1560 | Signal natif localisé. La valeur est une force ; le dessin en PSI nécessite de reproduire l'échelle du camion. |
| Trailer supply | `trailer_air_press_anim`, confirmé sur 389 et 5700XE | Chemin d'animation confirmé ; lecteur runtime à rechercher/raccorder. |
| Cruise speed, gear selected, speed | SDK ; widgets 1100 / 1040 / 1300 / 1380 / 1020 | Valeurs dynamiques. |
| Trip distance / fuel / time / average speed | Widgets 1210 / 1500 / 1390 / 1400 | Possibilité de lire les compteurs natifs, plutôt que d'inventer un second trip. |
| Engine running time | Widget 1280 | Affichage natif disponible ; persistance et remise à zéro à vérifier avant de le présenter comme heures moteur cumulées à vie. |

## Éléments présents à l'écran mais fixes ou réutilisés

Ces conclusions concernent les fichiers **stock inspectés**, pas tous les mods
ni l'impossibilité absolue de trouver d'autres données dans la simulation.

| Affichage | Preuve dans les fichiers installés |
|---|---|
| T680 2022 Fuel Restrict | `ui/dashboard/kenworth_t680_2022.sii`, lignes 2251 / 2273 : barre ID **0**, texte ID **0**, valeur littérale `0%|`. Aucune liaison native de restriction identifiée. |
| 49X DPF Soot | `ui/dashboard/westernstar_49x.sii`, lignes 1795 / 1931 : `Soot Level Good` ID **0** et barre `ui::text` ID **0**, dessin vert de largeur fixe **50px**. Ce widget ne fournit pas une mesure de suie. |
| LT Transmission Oil Level | `ui/dashboard/international_lt.sii`, ligne 1467 : `TRANSMISSION OIL<br>LEVEL OK`, ID **0**, texte littéral. |
| 57X Engine Oil Level | `ui/dashboard/westernstar_57x.sii`, ligne 1732 : `Oil level OK`, ID **0**, texte littéral. |
| W900 Diagnostic | `ui/dashboard/kenworth_w900.sii`, ligne 775 : `No Faults Found`, ID **0**. Le jeu a séparément de vrais avertissements de dégâts ; ce texte n'est pas un lecteur de codes défauts. |
| 389 Fuel Filter | `generic_anim` pointe sur `gen_anim.pma`, dont la piste anime le bone **33 `j_fuel_fltr`**. C'est une animation contact coupé / contact mis, pas une mesure variable d'encrassement. |
| Indicateurs identifiés visuellement comme Trans Temp | Aucun canal de température de boîte distinct dans les définitions inspectées. W900 utilise `diff_temp_anim` sur le bone **21 `g_difftmp`** ; 5700XE utilise une seule animation de différentiel pour 14 pistes de ses variantes d'aiguilles. Il faut distinguer la valeur affichée par ces aiguilles d'une température physique de transmission indépendante. |
| W900 Battery amps | Pas de source d'ampérage indépendante identifiée dans cet audit ; ne pas la déduire de la tension batterie. |

## Points de branchement concrets pour BetterGPS

`NavplusTelemetry.h` lit déjà la température extérieure formatée dans les
widgets natifs, après leur mise à jour via `managed_native_frame`. On peut
étendre ce mécanisme avec des contrôles de version, d'identité du véhicule,
de durée de vie et de type de widget. Le SDK reste préférable pour ses canaux
déjà disponibles.

Sur le binaire audité, le propriétaire de dashboard possède :

- un pointeur vers les données véhicule à **+0x118** ;
- un tableau de groupes de widgets à **+0x130**, pointeur **+0x138**,
  nombre **+0x140**, descripteurs de **0x38 octets** ;
- des widgets avec ID à **+0x14** ;
- pour `ui::text_common`, une chaîne de valeur à **+0x108** ;
- pour `ui_text_bar`, la valeur float avant normalisation à **+0x10C**.
  Le setter RVA **0x10199A0** la conserve à **0x10199DA** avant de calculer
  la largeur/hauteur du dessin. Ce n'est pas la chaîne d'un texte.

Sources vérifiées dans le désassemblage de mise à jour :

| Valeur | Lecture native observée | Instruction / utilisation |
|---|---|---|
| Turbo, groupe 13 / ID 1130 | données véhicule **+0x1280** | **0x541FFA**, puis setter de barre **0x542002** |
| Différentiel, groupe 55 / ID 1550 | données véhicule **+0x1160** | **0x543C55**, puis setter **0x543C8A** |
| Suspension, groupe 56 / ID 1560 | pointeur données véhicule **+0x15A8**, champ **+0x4B8** | **0x543CB3–0x543CC7**, avec facteur d'échelle natif |
| Consommation instantanée | données véhicule **+0x117C**, vitesse **+0x2F0** | **0x54208D–0x54217B** alimente le formateur 1160 |

Ces offsets sont des résultats d'analyse de **cette version**, pas une API
portable ni un lecteur déjà validé en roulant. Une implémentation doit vérifier
les instructions attendues avant de les utiliser et publier des snapshots
depuis le thread de mise à jour. Ne pas dépendre d'une page visible pour une
jauge qui doit fonctionner partout, ni écrire dans la simulation.

L'étape de validation en jeu est de comparer chaque snapshot à la jauge SCS
à l'arrêt, en accélération, au freinage et après changement de camion. Pour les
éléments statiques ci-dessus, aucun nouveau lecteur ne doit être présenté comme
un capteur réel sans avoir trouvé une autre source dynamique.

Référence officielle pour les IDs et les animations :
https://modding.scssoft.com/wiki/Documentation/Engine/Truck_Interior_Animations_and_IDs
