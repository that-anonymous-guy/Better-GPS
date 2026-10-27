# BetterGPS — développement quotidien

Point d'entrée : `python BetterGPS/Dev/workflow.py`, exécuté depuis le dépôt.
Les commandes lancent leurs outils sans shell intermédiaire et conservent les
sorties détaillées dans un dossier de run unique. Aucun outil ne ferme ATS.

## Démarrer une tâche

```powershell
python BetterGPS/Dev/workflow.py prepare
python BetterGPS/Dev/workflow.py test --list
```

Le préflight montre le commit, les modifications en cours, les modifications
préexistantes à préserver, Python, CMake, la racine ATS, les PID/chemins actuels
d'ATS, le dossier d'assets sources et les profils de test. CMake vient du PATH
ou de Visual Studio via vswhere. Un exemple de configuration machine est fourni
dans `BetterGPS/Dev/environment.example.json` : le copier vers
`.bettergps-dev.json` seulement pour remplacer une détection incorrecte.
Ce fichier local n'est pas versionné. Le répertoire ne change pas de modèle
Codex, de niveau de raisonnement ou de paramètres globaux.

Les cinq modifications antérieures (MPG, ses deux métadonnées et deux documents)
ainsi que `exports/` ont été classées dans `.bettergps-dev-pending.json`, sans
modifier leurs contenus ni les committer. Un changement ultérieur de leur hash
est signalé par le préflight. Le fichier peut être recréé explicitement avec
`pending --file CHEMIN --file AUTRE_CHEMIN` ; ne pas y masquer les changements
de la tâche courante.

## Choisir le travail nécessaire

| Type | Pendant les ajustements | Finalisation |
| --- | --- | --- |
| Texture seule | Inspecter/importer, vérifier pixels et aperçu | Déployer seulement les assets concernés |
| Position/typo/rendu | Compiler Preview, tester les suites du display | Compiler Overlay puis déployer la DLL |
| Tactile/entrée/composition | Profil input et suites du device | Overlay + tests concernés, installation ciblée |
| Temps/power/startup partagés | Profil clock/startup/global adapté | Vérifications natives et test en jeu si nécessaire |
| Livraison complète | Tous les composants nécessaires | Packaging ZIP et installateur de release |

```powershell
# Aperçu : ne construit pas la DLL ni TouchPhoneTest
python BetterGPS/Dev/workflow.py test --suite t680-clock

# Après les ajustements validés
python BetterGPS/Dev/workflow.py build --target overlay

# Entrées : build Preview + TouchPhone en une invocation MSBuild
python BetterGPS/Dev/workflow.py test --profile input

# Lorsque les exécutables viennent d'être construits et sont toujours à jour
python BetterGPS/Dev/workflow.py test --suite timezones --no-build
```

Cibles disponibles : overlay, preview, touch, checks (deux tests), all (DLL et
deux tests). Les cibles agrégées CMake `BetterGPSChecks` et `BetterGPSDev`
permettent à MSBuild de planifier les projets indépendants dans une invocation.
`--jobs` vaut 4 par défaut. Les suites graphiques indépendantes tournent en
parallèle ; les suites qui manipulent l'entrée/focus tournent ensuite une à une.

Profils maintenus dans `BetterGPS/Dev/profiles.json` : layout, t680, clock,
input, startup, global. Le catalogue de suites est extrait du dispatch réel de
`overlay_preview.cpp`, pour refuser un nom inexistant avant de lancer le test.
Chaque sortie reçoit un dossier neuf, sans réutiliser des images d'un ancien test.

Le reçu de build contient le hash des sources et de l'exécutable. Les tests
sans build et l'installation de DLL refusent les binaires périmés ou modifiés.
Un changement de source pendant la compilation invalide le reçu.

## Assets

La racine d'authoring détectée est d'abord `assets/` à côté de
`BetterGPS-main` (organisation Desktop/BetterGPS), puis les anciens emplacements
Desktop/newgpsoverlay et Downloads/newgpsoverlay. Les assets de compilation sont dans
`BetterGPS/Overlay/BetterGPS_assets`. Les assets installés appartiennent au
dossier BetterGPS d'ATS ; ils ne sont pas une source à écraser en bloc.

```powershell
# Inspection seule ; chemin relatif à la racine d'authoring détectée
python BetterGPS/Dev/workflow.py asset --source copilot/t680/icons/navplushd_icon_copilot.dds

# Préparer un DDS RGBA8 compatible, un PNG et un reçu, sans modifier le dépôt
python BetterGPS/Dev/workflow.py asset --source IMAGE --destination material/copilot/t680/IMAGE.dds
# Ajouter --apply pour écrire ce fichier précis, après sauvegarde de l'ancien.
```

Les importeurs propres à chaque système restent prioritaires pour les tailles
imposées et leurs receipts. Le convertisseur générique vérifie l'égalité des
pixels RGBA décodés et la limite de 4096 pixels du décodeur CPU. Il ne touche
pas automatiquement aux manifests communs : une modification d'asset ne doit
pas publier les autres changements en attente.

## Installer un changement

```powershell
# Plan lisible, sans écriture dans ATS
python BetterGPS/Dev/workflow.py deploy --file BetterGPSOverlay.dll

# Installation des seuls octets différents, avec sauvegarde et contrôle
python BetterGPS/Dev/workflow.py deploy --file BetterGPSOverlay.dll --apply

# Asset explicitement demandé, y compris remplacement d'une version personnalisée
python BetterGPS/Dev/workflow.py deploy --file BetterGPS_assets/material/copilot/t680/IMAGE.dds --replace-custom --apply
```

Un asset modifié manuellement est conservé par défaut. Le remplacement explicite
reste limité à la liste `--file`. Les configurations de racine et préférences
utilisateur ne sont pas des cibles de cette commande. Les manifests de package
sont fusionnés uniquement pour les chemins installés, sans remplacer les autres
entrées. Pour les manifests d'import partagés, ajouter par exemple
`--merge-manifest BetterGPS_assets/material/copilot/t680/import-manifest.json` :
seuls les hashes des assets et références de receipts sélectionnés sont fusionnés.

Avant toute écriture, le processus ATS est revérifié par PID et chemin. Le plan
est invalidé si la source ou le fichier installé a changé. Chaque fichier est
remplacé atomiquement, vérifié et sauvegardé dans `backups/dev-ID`. Une erreur
de copie restaure les fichiers déjà touchés. Les deux configurations principales
sont aussi capturées pour une récupération après migration au prochain lancement.
L'installation sans différence ne fait ni copie, ni backup, ni ZIP.

```powershell
python BetterGPS/Dev/workflow.py rollback --backup backups/dev-ID
# Ajouter --apply pour restaurer ; les changements utilisateur plus récents
# et les installations suivantes sont détectés et ne sont pas écrasés.
```

La première installation et les changements au loader, au Probe ou aux runtimes
.NET passent par la livraison complète ; le chemin rapide couvre la DLL Overlay,
les assets, layouts explicites et documents. Cette limite évite une installation
incomplète de composants interdépendants.

## Mesures et rétention

Chaque run possède `run.json` : statut, durée de chaque phase, suites et durées,
logs et backup éventuel. Les sorties sont sous `build/dev-runs/ID` et les reçus
de compilation sous `build/dev-state`. Aucun ancien ZIP de 428 Mio n'est copié
par le workflow de développement.

```powershell
python BetterGPS/Dev/workflow.py cleanup --days 14
# --apply ne supprime que les runs terminés, identifiés comme créés par cet
# outil et sans point de réanalyse. Les builds/dépendances/audits historiques
# extérieurs à dev-runs et les backups installés sont laissés disponibles.
python -m unittest discover -s BetterGPS/Dev/tests -v
```

Mesure initiale : build agrégé sans modification 1,392 s sur la machine locale,
contre 1,5 s mesurée lors de l'audit pour trois invocations. Ce n'est pas une
mesure de recompilation complète. Les gains principaux immédiats viennent des
cibles évitées et de l'absence de ZIP complet, pas de cet écart de 0,1 s.

## Carte technique

- `BetterGPSOverlay.cpp` : hooks/rendu principal ; `Overlay/*.h` : sous-systèmes.
- `DevicePages.h` : pages, historique et applications résidentes.
- `DeviceRenderer.h` : préparation et cache des images de displays.
- `SettingsMenu.h`, `CursorOnlyInput.h`, `TouchInput.h` : entrées et tactile.
- `GameTimezone.h`, `VoiceNavigation.h` : lectures natives gardées et variables.
- `Overlay/scripts/` : importeurs d'assets ; `Overlay/tests/` : fixtures.
- `docs/references/navplus-smartnav/` : sources DEA600, index obligatoire.
- `Packaging/` : construction complète, runtimes, ZIP et installateur de release.

Les trois cibles incluent encore le gros code Overlay avec des définitions de
test distinctes. Le fractionnement en unités C++ et l'essai de PCH restent des
changements structurels à mesurer isolément : ils ne sont pas nécessaires au
nouveau workflow et ne sont pas présentés comme déjà réalisés.
