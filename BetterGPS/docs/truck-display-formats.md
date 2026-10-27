# Écrans ATS : infotainment/GPS, cluster et afficheurs auxiliaires

Audit corrigé du 20 octobre 2026, ATS 1.61.3.18s, archives officielles installées, mods exclus. Les 19 familles et 83 modèles intérieurs du relevé ont été recontrôlés. Aucun DLL, réglage ni asset installé n’est modifié par cet audit.

## Correction du relevé précédent

**Ne pas utiliser le ratio UV comme format physique d’un nouvel écran.** Le relevé précédent donnait les dimensions exposées dans le buffer, pas les proportions du panneau 3D. Sur le T680 nouvelle génération, le GPS avait bien été trouvé à droite du cluster, mais le ratio interne 2:1 avait été présenté de manière trompeuse comme son format.

| Écran vérifié | Maillage | Position latérale X | Dimensions physiques du modèle | Format physique | Buffer natif déclaré |
| --- | --- | ---: | ---: | ---: | ---: |
| Kenworth T680 nouvelle génération (ID 2022) — infotainment/GPS | pièce 38 | +0.403 m | 154.20 × 93.20 mm | 1.655:1 | 1024 × 512 |
| Kenworth T680 nouvelle génération (ID 2022) — cluster | pièce 39 | -0.000 m | 361.70 × 128.99 mm | 2.804:1 | 1024 × 512 |
| Freightliner Cascadia 2019 — numérique — infotainment/GPS | pièce 49 | +0.410 m | 244.22 × 91.81 mm | 2.660:1 | 1024 × 512 |
| Freightliner Cascadia 2019 — numérique — cluster | pièce 45 | +0.003 m | 304.76 × 114.29 mm | 2.667:1 | 1024 × 512 |
| Freightliner Cascadia 2024 — numérique — infotainment/GPS | pièce 60 | +0.410 m | 244.22 × 91.81 mm | 2.660:1 | 1024 × 512 |
| Freightliner Cascadia 2024 — numérique — cluster | pièce 56 | +0.003 m | 304.76 × 114.29 mm | 2.667:1 | 1024 × 512 |

Les positions sont dans le repère du modèle intérieur (X≈0 pour les instruments devant le conducteur, X positif pour le centre du dashboard). Il ne s’agit pas de coordonnées écran de clic.

- **T680 nouvelle génération** : GPS pièce 38, `/vehicle/truck/share/gps.tobj`, `/ui/dashboard/kenworth_t680_2022_gps.sii` (UI 1280×720, fond `infotainment.mat`). Cluster pièce 39, `/vehicle/truck/share/dashboard.tobj`, `/ui/dashboard/kenworth_t680_2022.sii` (UI 1440×540). Modèle : `/vehicle/truck/kenworth_t680_2022/interior/model/day.pmd`.
- **Cascadia 2019 numérique** : GPS pièce 49, UI `freightliner_cascadia_icc5_gps.sii` ; cluster pièce 45, UI `freightliner_cascadia_icc5.sii`. Modèle `freightliner_cascadia2019/interior/model/day_d.pmd`.
- **Cascadia 2024 numérique** : GPS pièce 60, UI `freightliner_cascadia_2024_gps.sii` ; cluster pièce 56, UI `freightliner_cascadia_2024.sii`. Modèle `freightliner_cascadia2024/interior/model/day_d.pmd`.
- Les deux Cascadia numériques ont réellement un GPS très large (≈2,660:1), proche du cluster (≈2,667:1). Leur buffer commun de format 1024×512 ne prouve ni un écran identique ni un format physique 2:1.

## GPS / infotainment : table corrigée

Les millimètres viennent du maillage du jeu, pas d’une fiche constructeur. Le buffer est `gps_drawable_size` (256×256 lorsque non précisé). La dernière colonne conserve la convention UV du relevé initial, pour comparaison. **Pour préparer le format visuel, lire la colonne physique ; la conversion UV reste nécessaire au placement.**

| Camion / variante | Surface physique du modèle (mm) | Format physique | Buffer natif | Zone UV exposée (px) |
| --- | ---: | ---: | ---: | ---: |
fait
| Kenworth T680 classique | ≈ 154.71 × 86.59 | ≈ 1.787:1 | 256 × 256 | 255.30 × 155.37 |
| Kenworth T680 nouvelle génération (ID 2022) | ≈ 154.20 × 93.20 | ≈ 1.655:1 | 1024 × 512 | 1021.57 × 510.65 |
| Kenworth W900 | ≈ 159.14 × 106.81 | ≈ 1.490:1 | 256 × 256 | 252.69 × 169.60 |
| Peterbilt 389 | ≈ 155.12 × 90.25 | ≈ 1.719:1 † | 256 × 256 | 254.35 × 148.83 |
| Peterbilt 579 | ≈ 112.19 × 79.47 | ≈ 1.412:1 † | 256 × 256 | 252.73 × 172.09 |
| International 9900i | Aucun maillage GPS intégré trouvé dans les trois intérieurs | — | — | — |
| Mack Anthem | ≈ 154.68 × 93.59 | ≈ 1.653:1 | 256 × 256 | 253.58 × 139.61 |
| Mack Pinnacle | ≈ 154.68 × 93.59 | ≈ 1.653:1 | 256 × 256 | 253.58 × 139.61 |
| Volvo VNL ancien | ≈ 151.27 × 76.81 | ≈ 1.970:1 | 256 × 256 | 255.12 × 129.54 |
| Volvo VNL 2018 | ≈ 153.40 × 92.44 | ≈ 1.659:1 | 512 × 256 | 507.28 × 253.32 |
| Volvo VNR Electric | ≈ 153.40 × 92.44 | ≈ 1.659:1 | 512 × 256 | 507.28 × 253.32 |
| Volvo VNL 2025 | ≈ 204.25 × 113.56 | ≈ 1.799:1 | 1024 × 512 | 1013.76 × 506.88 |
| Freightliner Cascadia 2019 — numérique | ≈ 244.22 × 91.81 | ≈ 2.660:1 | 1024 × 512 | 1024.00 × 512.00 |
| Freightliner Cascadia 2024 — numérique | ≈ 244.22 × 91.81 | ≈ 2.660:1 | 1024 × 512 | 1024.00 × 512.00 |
| Freightliner Cascadia 2019 — analogique | ≈ 195.31 × 126.05 | ≈ 1.549:1 | 256 × 256 | 254.46 × 178.80 |
| Freightliner Cascadia 2024 — analogique | ≈ 195.31 × 126.05 | ≈ 1.549:1 | 256 × 256 | 254.46 × 178.80 |
| Western Star 49X | ≈ 195.31 × 126.05 | ≈ 1.549:1 | 256 × 256 | 254.46 × 178.80 |
| Western Star 57X | ≈ 244.22 × 91.81 | ≈ 2.660:1 | 1024 × 512 | 1024.00 × 512.00 |
| Western Star 5700XE | ≈ 151.47 × 88.45 | ≈ 1.713:1 | 512 × 512 | 509.42 × 297.46 |
| International LoneStar | ≈ 146.51 × 79.59 | ≈ 1.841:1 | 256 × 256 | 255.03 × 139.40 |
| International LT | ≈ 146.51 × 79.59 | ≈ 1.841:1 | 256 × 256 | 255.03 × 139.40 |


a faire


† Les surfaces Peterbilt 389/579 ne sont pas parfaitement planes ; le ratio donné est celui du rectangle projeté d’encombrement. Il reste approximatif et ne remplace pas un ajustement visuel sur ces modèles.

## Cluster / instruments devant le conducteur

Les surfaces déconnectées sont mesurées séparément, même si elles utilisent la même texture et le même buffer. Un atlas de plusieurs afficheurs n’est pas un grand écran unique. Les petites surfaces pilotées par le dashboard mais situées ailleurs sont rangées dans la section suivante.

| Camion / variante | Surface physique du modèle (mm) | Format physique | Buffer dashboard | Zone UV de cette surface (px) |
| --- | ---: | ---: | ---: | ---: |

already done
| Kenworth T680 nouvelle génération (ID 2022) — pièce 39, surface 0 | ≈ 361.70 × 128.99 | ≈ 2.804:1 | 1024 × 512 | 1011.80 × 508.54 |
| Mack Anthem — pièce 12, surface 0 | ≈ 106.37 × 64.50 | ≈ 1.649:1 | 256 × 256 | 254.93 × 144.74 |
| Mack Pinnacle — pièce 11, surface 0 | ≈ 106.37 × 64.50 | ≈ 1.649:1 | 256 × 256 | 254.93 × 144.74 |
| Volvo VNL ancien — pièce 82, surface 0 | ≈ 122.89 × 76.56 | ≈ 1.605:1 | 256 × 256 | 255.08 × 192.09 |
| Volvo VNL 2018 — pièce 20, surface 0 | ≈ 109.39 × 66.26 | ≈ 1.651:1 | 512 × 256 | 509.39 × 254.31 |
| Volvo VNR Electric — pièce 13, surface 0 | ≈ 109.39 × 66.26 | ≈ 1.651:1 | 512 × 256 | 503.76 × 253.61 |
| Volvo VNL 2025 — pièce 106, surface 0 | ≈ 292.81 × 109.80 | ≈ 2.667:1 | 1024 × 512 | 1013.76 × 506.88 |
| Freightliner Cascadia 2019 — numérique — pièce 45, surface 0 | ≈ 304.76 × 114.29 | ≈ 2.667:1 | 1024 × 512 | 1024.00 × 512.00 |
| Freightliner Cascadia 2024 — numérique — pièce 56, surface 0 | ≈ 304.76 × 114.29 | ≈ 2.667:1 | 1024 × 512 | 1024.00 × 512.00 |
| Freightliner Cascadia 2019 — analogique — pièce 56, surface 0 | ≈ 75.60 × 100.80 | ≈ 0.750:1 | 256 × 256 | 191.91 × 255.88 |
| Freightliner Cascadia 2024 — analogique — pièce 73, surface 0 | ≈ 75.60 × 100.80 | ≈ 0.750:1 | 256 × 256 | 191.91 × 255.88 |
| Western Star 49X — pièce 59, surface 0 | ≈ 72.90 × 90.54 | ≈ 0.805:1 | 256 × 256 | 190.55 × 254.24 |
| Western Star 57X — pièce 52, surface 0 | ≈ 72.90 × 90.54 | ≈ 0.805:1 | 1024 × 512 | 762.18 × 508.48 |
| Western Star 57X — pièce 52, surface 0 | ≈ 304.76 × 114.29 | ≈ 2.667:1 | 1024 × 512 | 1024.00 × 512.00 |
| Western Star 5700XE — pièce 27, surface 0 | ≈ 103.63 × 29.21 | ≈ 3.548:1 | 512 × 128 | 512.00 × 127.87 |
| International 9900i — pièce 22, surface 1 | ≈ 39.94 × 14.93 | ≈ 2.675:1 | 256 × 64 | 137.07 × 62.10 |
| International LoneStar — pièce 18, surface 0 | ≈ 108.65 × 65.64 | ≈ 1.655:1 | 256 × 256 | 255.34 × 154.01 |
| International LT — pièce 18, surface 0 | ≈ 108.65 × 65.64 | ≈ 1.655:1 | 256 × 256 | 255.34 × 154.01 |

to do
| Peterbilt 389 — pièce 13, surface 0 | ≈ 36.83 × 16.62 | ≈ 2.216:1 | 256 × 256 | 108.04 × 47.88 |
| Peterbilt 389 — pièce 13, surface 1 | ≈ 36.45 × 16.17 | ≈ 2.254:1 | 256 × 256 | 107.74 × 47.68 |
| Peterbilt 389 — pièce 13, surface 2 | ≈ 93.80 × 41.52 | ≈ 2.259:1 | 256 × 256 | 240.03 × 117.39 |
| Peterbilt 579 — pièce 10, surface 0 | ≈ 99.39 × 77.44 | ≈ 1.283:1 | 256 × 256 | 253.11 × 253.11 |
| Kenworth T680 classique — pièce 10, surface 0 | ≈ 105.40 × 79.85 | ≈ 1.320:1 | 256 × 256 | 252.17 × 194.37 |
| Kenworth W900 — pièce 20, surface 0 | ≈ 31.46 × 13.07 | ≈ 2.407:1 | 256 × 128 | 88.88 × 34.28 |
| Kenworth W900 — pièce 20, surface 1 | ≈ 31.77 × 13.23 | ≈ 2.402:1 | 256 × 128 | 89.36 × 34.28 |
| Kenworth W900 — pièce 20, surface 2 | ≈ 89.07 × 38.17 | ≈ 2.334:1 | 256 × 128 | 256.00 × 92.22 |



## Autres afficheurs pilotés par le dashboard

L’identité du producteur est certaine (`dashboard_path` / `dashboard.tobj`). La distinction spatiale cluster/auxiliaire est fondée sur la position des composants dans les modèles inspectés, pas sur le mot `dashboard` dans le nom. Cette section ne suppose pas que ces afficheurs sont des GPS ; leur fonction exacte radio/climatisation/horloge peut demander un contrôle visuel en jeu.

| Camion | Position X | Surface physique du modèle (mm) | Format physique |
| --- | ---: | ---: | ---: |
| Freightliner Cascadia 2019 — analogique — pièce 56, surface 1 | +0.618 m | 85.20 × 17.16 | 4.964:1 |
| Freightliner Cascadia 2024 — analogique — pièce 73, surface 1 | +0.618 m | 85.20 × 17.16 | 4.964:1 |
| International 9900i — pièce 22, surface 0 | +0.491 m | 82.77 × 12.72 | 6.505:1 |
| International LoneStar — pièce 18, surface 1 | +0.363 m | 86.27 × 18.90 | 4.564:1 |
| International LT — pièce 18, surface 1 | +0.363 m | 86.27 × 18.90 | 4.564:1 |
| Mack Anthem — pièce 12, surface 1 | +0.361 m | 17.35 × 12.60 | 1.377:1 |
| Mack Anthem — pièce 12, surface 2 | +0.389 m | 37.91 × 14.14 | 2.681:1 |
| Mack Anthem — pièce 12, surface 1 | +0.346 m | 17.35 × 12.60 | 1.377:1 |
| Mack Anthem — pièce 12, surface 3 | +1.220 m | 51.53 × 16.25 | 3.171:1 |
| Mack Pinnacle — pièce 11, surface 1 | +0.361 m | 17.35 × 12.60 | 1.377:1 |
| Mack Pinnacle — pièce 11, surface 2 | +0.389 m | 37.91 × 14.14 | 2.681:1 |
| Mack Pinnacle — pièce 11, surface 1 | +0.346 m | 17.35 × 12.60 | 1.377:1 |
| Mack Pinnacle — pièce 11, surface 3 | +1.220 m | 51.53 × 16.25 | 3.171:1 |
| Volvo VNL 2018 — pièce 20, surface 1 | +0.509 m | 18.27 × 11.72 | 1.558:1 |
| Volvo VNR Electric — pièce 13, surface 1 | +0.509 m | 18.27 × 11.72 | 1.558:1 |
| Western Star 49X — pièce 59, surface 1 | +0.615 m | 85.20 × 17.16 | 4.964:1 |
| Western Star 49X — pièce 56, surface 0 | +1.295 m | 58.17 × 18.35 | 3.171:1 |

## Méthode et limites

1. UI choisie dans la définition d’intérieur et ses includes : `gps_path` contre `dashboard_path`.
2. Matériau référencé par le PMD, alias exact `gps.tobj` contre `dashboard.tobj`, et contenu de l’UI (carte/infotainment contre instruments).
3. Pièce PMG, position XYZ, triangles et surfaces physiquement connectées (sommets de couture soudés à 0,01 mm).
4. Rectangle minimal de la surface projetée dans son plan ; les UV servent à orienter largeur/hauteur, pas à calculer les longueurs physiques. Les erreurs de planéité et d’ajustement UV sont conservées dans les données.
5. Contrôle des variantes de modèles ; les surfaces identiques sont regroupées dans la synthèse, les références restent dans les JSON/CSV.

L’ancienne moyenne des ratios par triangle surpondérait les petits triangles des coins arrondis : elle donnait par exemple 1,224 pour le T680 GPS. Elle ne doit pas être utilisée. L’ancien regroupement par tuile UV pouvait aussi réunir plusieurs petits LCD éloignés.

Périmètre : GPS et afficheurs pilotés par les dashboards des 19 camions officiels présents. Pas de mesure des rétroviseurs caméra, accessoires portables, ni véhicules moddés. Les erreurs `seat.sui` des Volvo dans l’extraction ne concernent pas les écrans. Ce contrôle hors ligne ne certifie pas une capture GPU en jeu.

Données : `build/truck-screen-role-audit-20261020/role-audit.csv`, `role-audit.json`, `role-audit-full.json`. Chaque surface conserve UI, définition, modèle, pièce, texture, buffer, exposition UV, dimensions, centre et qualité géométrique.

Reproduction :

`python BetterGPS/Overlay/scripts/audit_truck_display_roles.py build/truck-screen-inventory-20261020/stock build/truck-screen-inventory-20261020/report build/truck-screen-role-audit-20261020`
