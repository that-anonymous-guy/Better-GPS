# Caméras des camions ATS — audit du 25 octobre 2026

Les 19 familles officielles installées ont été parcourues hors ligne, mods
exclus : **26 643 fichiers de définition lus et 11 209 vérifications de références
PMD** (des modèles partagés apparaissent dans plusieurs familles). Les archives
ne sont pas modifiées. Le plugin et sa page Camera ne sont pas modifiés.

**Résultat confirmé dans le stock : T680 NG, Cascadia 2024 et VNL 2025 disposent
d’accessoires caméra/rétroviseurs numériques et d’une interface vidéo déclarée.**
Ce résultat est conditionnel à l’équipement monté, pas à tous les exemplaires.
La présence d’un écran infotainment ou d’une touche Camera ne suffit pas.

## Complément : examen physique des modèles 3D

Le premier relevé ci-dessous concernait surtout les déclarations et références.
La vérification des maillages et textures est maintenant documentée séparément :
[examen 3D des 19 familles](truck-camera-mesh-audit-20261025.md). Elle confirme
les trois surfaces de lentille du T680 NG avec DigitalVision, ainsi que les
modules physiques MirrorEye/CMS. Elle ne certifie pas tous les boîtiers ADAS
ni l’absence de caméra sur toutes les variantes des autres familles.

## Tableau complet

La colonne jeu décrit les systèmes vidéo déclarés dans les assets ; la colonne
réelle indique les références constructeur. Une caméra ADAS réelle ne prouve
ni un flux vidéo dans ATS, ni son branchement sur l’écran central.

| Camion | Résultat dans les fichiers stock | Référence réelle / limite |
| --- | --- | --- |
| Kenworth T680 NG (ID 2022) | DigitalVision en option : appareils extérieurs sur les ensembles de rétroviseurs, moniteurs sur les montants A. | Caméras arrière latérales ; le constructeur décrit aussi une vue plongeante côté passager. Caméra ADAS au pare-brise selon équipement ; celle-ci n’est pas une entrée NavPlus prouvée. [S1](https://blog.scssoft.com/2023/12/kenworth-t680-100-years-anniversary.html) [S2](https://kenworth.anthology-digital.com/trucks/t680/) [S3](https://static.nhtsa.gov/odi/tsbs/2019/MC-10163233-9999.pdf) |
| Freightliner Cascadia 2024 / 5e génération | MirrorEye en option, ensembles extérieurs de rétroviseurs et moniteurs intérieurs. | Caméra multipurpose Detroit Assurance au pare-brise ; un dispositif orienté conducteur peut aussi équiper le véhicule réel. [S4](https://blog.scssoft.com/2024/10/ats-introducing-fifth-generation.html) [S5](https://dtnacontent-dtna.prd.freightliner.com/content/dam/public/dtna-servicelit/dtna/pdfs/en_us/freightliner/drivers-manuals/New%20Cascadia%20Driver%27s%20Manual_.pdf) |
| Volvo VNL 2025 | CMS en option : modules latéraux hauts sur la cabine, moniteurs aux montants A. | Volvo documente aussi une caméra de couplage à l’arrière de cabine et plusieurs entrées infotainment réelles. Leurs flux/implantations ne sont pas confirmés dans le stock audité. [S6](https://www.volvotrucks.us/news-and-stories/press-releases/2024/march/safety-drives-everything-in-the-all-new-volvo-vnl/) [S16](https://blog.scssoft.com/2026/05/ats-volvo-vnl-2025-volvo-vnr-electric.html) |
| Freightliner Cascadia 2019 | Aucun CMS déclaré identifié. | Caméra Detroit Assurance 5.0 contre le pare-brise sur le véhicule réel ; pas de flux infotainment identifié dans ce relevé. [S5](https://dtnacontent-dtna.prd.freightliner.com/content/dam/public/dtna-servicelit/dtna/pdfs/en_us/freightliner/drivers-manuals/New%20Cascadia%20Driver%27s%20Manual_.pdf) |
| Western Star 49X | Aucun CMS déclaré identifié. | Caméra MPC2 Detroit Assurance contre le pare-brise, selon équipement réel. [S11](https://dtnacontent-dtna.prd.freightliner.com/content/dam/public/dtna-servicelit/dtna/pdfs/en_us/western-star/drivers-manuals/47X%2C%2049X%20Drivers%20Manual.pdf) |
| Western Star 57X | Aucun CMS déclaré identifié. | Detroit Assurance 5.0 caméra + radar documenté d’origine sur le véhicule réel. Ne pas importer les rétros caméra du Cascadia au seul motif du dashboard commun. [S10](https://northamerica.daimlertruck.com/news-stories/2022/western-star-introduces-all-new-57x) |
| Western Star 5700XE | Aucun CMS déclaré identifié. | Fiche constructeur : Bendix AutoVue avec avertissement de voie. Caméra ADAS avant ; pas une caméra de recul. [S12](https://westernstar.blob.core.windows.net/assets/brochures/5700XETechSheet.pdf) |
| Mack Anthem du jeu | Aucun CMS déclaré identifié. | Wingman Fusion : caméra au pare-brise sur le modèle réel. Les Digital Mirror des nouveaux Anthem/Pioneer ne prouvent pas leur présence sur cet Anthem ATS. [S8](https://www.macktrucks.ca/mack-news/2019/mack-now-offers-bendix-wingman-fusion-with-enhanced-features-on-mack-anthem-and-pinnacle-models) [S17](https://www.stoneridge.com/commercial-truck/) |
| Mack Pinnacle | Aucun CMS déclaré identifié. | Wingman Fusion : caméra au pare-brise selon équipement réel. [S8](https://www.macktrucks.ca/mack-news/2019/mack-now-offers-bendix-wingman-fusion-with-enhanced-features-on-mack-anthem-and-pinnacle-models) |
| International LT | Aucun CMS déclaré identifié. | Wingman Fusion avant, caméra + radar ; référence constructeur de 2019. [S9](https://news.international.com/BendixWingmanFusionStandard) |
| International LoneStar | Aucun CMS déclaré identifié. | Wingman Fusion documenté sur la gamme à partir des équipements cités en 2019. Ne valide pas chaque finition/millésime stock. [S9](https://news.international.com/BendixWingmanFusionStandard) |
| Volvo VNL 2018 | Aucun CMS déclaré identifié. | Volvo Active Driver Assist au pare-brise ; documentation de la génération 2017 mentionnant aussi une possibilité de caméra extérieure avec l’infotainment. Installation stock non confirmée. [S15](https://www.volvogroup.com/content/dam/volvo-group/markets/master/news-and-media/volvo-group-magazine/news-and-media-vgm-4-2017.pdf) |
| Volvo VNR Electric | Aucun CMS déclaré identifié. | Volvo décrit VADA caméra + radar sur le VNR Electric. Ce n’est pas le CMS du nouveau VNL. [S7](https://www.volvotrucks.us/trucks/vnr-electric/) |
| Kenworth T680 classique (dit 2014) | Aucun CMS déclaré identifié. | T680 compatible caméras NavPlus/DEA600 en option ; AutoVue documenté sur la famille T680, sans preuve d’équipement pour chaque millésime ATS. [S3](https://static.nhtsa.gov/odi/tsbs/2019/MC-10163233-9999.pdf) [S14](references/navplus-smartnav/smartnav_camera_installation_guide_rev_a.pdf) |
| Kenworth W900 | Aucun CMS déclaré identifié. | Guide DEA600 : compatibilité des entrées caméras gauche/droite/recul. Le tableau de compatibilité n’est pas la preuve d’un kit physiquement installé. [S14](references/navplus-smartnav/smartnav_camera_installation_guide_rev_a.pdf) |
| Peterbilt 389 | Aucun CMS déclaré identifié. | Peterbilt confirme les provisions caméra SmartNav ; kit, installation et génération à vérifier sur le camion réel. [S13](https://www.peterbilt.com/trucks/specialized/389) [S14](references/navplus-smartnav/smartnav_camera_installation_guide_rev_a.pdf) |
| Peterbilt 579 ancien du jeu | Aucun CMS déclaré identifié. | Compatibilité SmartNav ; AutoVue documenté sur la famille. Les Digital Vision du New 579 de 2021 ne sont pas transposables à l’ancien 579 ATS. [S3](https://static.nhtsa.gov/odi/tsbs/2019/MC-10163233-9999.pdf) [S14](references/navplus-smartnav/smartnav_camera_installation_guide_rev_a.pdf) [S17](https://www.stoneridge.com/commercial-truck/) |
| Volvo VNL ancien (dit 2014) | Aucun CMS déclaré identifié. | Pas de référence d’époque suffisamment précise retenue pour certifier une caméra sur cette configuration. Ne pas extrapoler le VADA/CMS récent à ce véhicule.  |
| International 9900i | Aucun CMS déclaré identifié. | Aucune installation caméra d’origine certifiée par les références retenues pour cette génération. Cela n’exclut pas un kit de flotte ajouté après fabrication.  |

## T680 nouvelle génération : correction importante

L’affirmation antérieure « aucune caméra sur le T680 » était trop générale.
L’ID SCS `2022` désigne ici la famille Next Gen, dont les options ont évolué ;
il ne date pas à lui seul l’apparition de chaque équipement réel.
Les fichiers stock comportent bien ces options :

```
/def/vehicle/truck/kenworth.t680_2022/accessory/mirror/camera_chrome.sii
/def/vehicle/truck/kenworth.t680_2022/accessory/mirror/camera_paint.sii
/def/vehicle/truck/kenworth.t680_2022/accessory/int_display/digital_mirror.sii
```

Les deux premiers référencent des modèles extérieurs et intérieurs dédiés
sous `/vehicle/truck/kenworth_t680_2022/accessory/mirror/`. Ils imposent le
module de moniteurs par `overrides[]`. Il faut donc vérifier l’accessoire
réellement monté avant de conclure sur le camion du joueur.

Sur le véhicule réel DigitalVision, Kenworth décrit une vue vers l’arrière
à gauche et des vues arrière/plongeante à droite, avec moniteurs sur les
montants A. Cela situe les caméras dans les ensembles de rétroviseurs, pas
sur le nez. Le nombre de vues dessinées par SCS ne prouve pas le nombre
exact de lentilles physiques. La caméra ADAS de pare-brise est encore un
autre équipement ; le manuel FLC-20 donne des implantations sur les familles
T680/579 sans certifier la configuration exacte du millésime stock.

Le choix utilisateur « No camera available » du NavPlus reste en vigueur.
Il représente actuellement une page non branchée ; il ne doit plus être
utilisé comme preuve d’absence de toute caméra sur la famille de camions.

## Les interfaces vidéo réellement trouvées

| Famille | Module | UI native | Texture de moniteurs |
| --- | --- | --- | --- |
| T680 NG | `accessory/int_display/digital_mirror.sii` | `/ui/dashboard/kenworth_t680_2022_cam_mirrors.sii` | `/vehicle/truck/kenworth_t680_2022/accessory/mirror/cam_mirrors_ui.tobj` |
| Cascadia 2024 | `accessory/int_display/digital_mirror.sii` | `/ui/dashboard/freightliner_cascadia_2024_cam_mirrors.sii` | `/vehicle/truck/freightliner_cascadia2024/accessory/int_display/cam_mirrors_ui.tobj` |
| VNL 2025 | `accessory/int_display/digital_mirror.sii` | `/ui/dashboard/volvo_vnl2025_cam_mirrors.sii` | `/vehicle/truck/volvo_vnl2025/accessory/int_display/cam_mirrors_ui.tobj` |

Les trois UI dessinent des matériaux `close_mirror_reflection`,
`far_mirror_reflection`, `close_s_mirror_reflection` et
`far_s_mirror_reflection` sous `/material/environment/`. Le Cascadia référence
aussi `side_mirror_reflection`. Ce sont des images natives de rétroviseurs,
pas la preuve d’une caméra de recul au pare-chocs ou sur la remorque.

Les fichiers `interior/mirrors.sui` et leurs `mirror_camera_presets` existent
également pour des rétroviseurs optiques : ils décrivent la simulation des
reflets et ne sont pas comptés comme preuve de caméra physique.

## Ce qui reste non certifié

- Les petites lentilles/boîtiers ADAS éventuellement intégrés dans les maillages
  et textures génériques des 19 camions n’ont pas tous été inspectés visuellement.
  Le relevé ne prétend donc pas prouver l’absence physique sur les 16 autres.
- Aucune caméra avant de nez ni caméra arrière de recul dédiée n’est confirmée
  par cet audit stock. Les options réelles NavPlus/SmartNav/VNL ne les installent
  pas automatiquement dans le jeu. Les options arrière Volvo derrière la cabine
  sont documentées par Volvo, mais pas raccordées par les trois UI trouvées.
- Une caméra ADAS n’est pas une entrée vidéo destinée à afficher une image sur
  le GPS. Aucun flux natif ADAS exploitable n’a été identifié ici.
- Les nouveaux Peterbilt 579 et Mack Anthem/Pioneer vendus aujourd’hui ne sont
  pas les modèles historiques du jeu : ne pas transférer leurs options modernes.
- 55 références optionnelles absentes (11 chemins uniques : décorations,
  mobilier, deux intérieurs de calandre Mack) sont conservées dans le relevé.
  Aucun de ces échecs ne concerne les fichiers caméra/rétroviseurs vérifiés.

## Reproduction et preuves

L’outil existant AssetInspector prend désormais `--camera-inventory`, qui
force `IgnoreMods=true`. Il parcourt les répertoires HashFS réels et conserve
les définitions pertinentes, PMD/PMG des accessoires caméra, UI et TOBJ sous
`build/camera-audit-20261025/stock/extracted`. Le manifest `sources.json`
conserve la provenance/les empreintes des ressources lues. Le JSON complet et
les avertissements restent dans ce dossier de travail ignoré ; la synthèse
pérenne est [audit-20261025.json](references/truck-cameras/audit-20261025.json).

```
dotnet build BetterGPS/Overlay/tools/AssetInspector/AssetInspector.csproj -c Release -o build/camera-audit-20261025/tool --no-restore
build/camera-audit-20261025/tool/AssetInspector.exe MAPBRIDGE_PUBLISH ATS_ROOT ATS_DOCUMENTS build/camera-audit-20261025/stock --camera-inventory
```

Trois assertions portent sur le périmètre (19 familles), les trois modules
vidéo natifs attendus et l’absence d’échec d’extraction lié aux caméras.
Les sources web ci-dessous ont été consultées le 25 octobre 2026 ; leurs
années et générations sont indiquées pour éviter les extrapolations.

- [S1 — SCS — T680 Anniversary, 19 décembre 2023](https://blog.scssoft.com/2023/12/kenworth-t680-100-years-anniversary.html)
- [S2 — Kenworth — T680 / DigitalVision](https://kenworth.anthology-digital.com/trucks/t680/)
- [S3 — Bendix — AutoVue FLC-20, document constructeur déposé NHTSA](https://static.nhtsa.gov/odi/tsbs/2019/MC-10163233-9999.pdf)
- [S4 — SCS — Cascadia cinquième génération, 15 octobre 2024](https://blog.scssoft.com/2024/10/ats-introducing-fifth-generation.html)
- [S5 — Freightliner — New Cascadia Driver’s Manual / Detroit Assurance 5.0](https://dtnacontent-dtna.prd.freightliner.com/content/dam/public/dtna-servicelit/dtna/pdfs/en_us/freightliner/drivers-manuals/New%20Cascadia%20Driver%27s%20Manual_.pdf)
- [S6 — Volvo — sécurité du nouveau VNL, mars 2024](https://www.volvotrucks.us/news-and-stories/press-releases/2024/march/safety-drives-everything-in-the-all-new-volvo-vnl/)
- [S7 — Volvo — VNR Electric / sécurité](https://www.volvotrucks.us/trucks/vnr-electric/)
- [S8 — Mack — Wingman Fusion sur Anthem et Pinnacle, octobre 2019](https://www.macktrucks.ca/mack-news/2019/mack-now-offers-bendix-wingman-fusion-with-enhanced-features-on-mack-anthem-and-pinnacle-models)
- [S9 — International — Wingman Fusion sur LoneStar et LT, février 2019](https://news.international.com/BendixWingmanFusionStandard)
- [S10 — DTNA — lancement Western Star 57X](https://northamerica.daimlertruck.com/news-stories/2022/western-star-introduces-all-new-57x)
- [S11 — Western Star — manuel 47X/49X, caméra MPC2](https://dtnacontent-dtna.prd.freightliner.com/content/dam/public/dtna-servicelit/dtna/pdfs/en_us/western-star/drivers-manuals/47X%2C%2049X%20Drivers%20Manual.pdf)
- [S12 — Western Star — fiche constructeur 5700XE, 2018](https://westernstar.blob.core.windows.net/assets/brochures/5700XETechSheet.pdf)
- [S13 — Peterbilt — Model 389 / SmartNav camera provisions](https://www.peterbilt.com/trucks/specialized/389)
- [S14 — Guide SmartNav 2.0 Rev A fourni, tableau de compatibilité p.21](references/navplus-smartnav/smartnav_camera_installation_guide_rev_a.pdf)
- [S15 — Volvo Group Magazine 4/2017, VNL/VNR p.63](https://www.volvogroup.com/content/dam/volvo-group/markets/master/news-and-media/volvo-group-magazine/news-and-media-vgm-4-2017.pdf)
- [S16 — SCS — VNL 2025 / VNR Electric, mai 2026](https://blog.scssoft.com/2026/05/ats-volvo-vnl-2025-volvo-vnr-electric.html)
- [S17 — Stoneridge — programmes OEM, générations à distinguer](https://www.stoneridge.com/commercial-truck/)
