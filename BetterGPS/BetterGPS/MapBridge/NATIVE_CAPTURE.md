# Capture native et préparation automatique

La DLL compilée se trouve dans `../../build/BetterGPS/Probe/Release/BetterGPSProbe.dll`. La source native est dans `../Probe/src`. Le package final est généré par `../Packaging/Package-BetterGPS.ps1`.

## Flux

```text
ATS 1.61 x64
  SDK SCS : world.placement, truck.speed, navigation.distance, pause
  mémoire : route_task, tableaux physique/logique, UID, distances de navigation
        ↓ copies appartenant à la DLL ; aucun pointeur transmis
  worker C++ → canal local BetterGPSProbe-v1
        ↓ RawRouteSnapshot + TelemetryFrame
  MapBridge : carte/mods chargés une fois → corridor → PPD → branches → panneaux
        ↓ RouteSnapshot compilé + VehicleState + SourceState
  LiveGuidanceEngine → console + guidance.jsonl
```

Le SDK fournit la position réelle, la vitesse signée et la pause. Le trajet complet provient des signatures/layouts mémoire BetterGPSProbe/ETS2LA pour ATS 1.61. La DLL refuse les autres versions et les signatures absentes. La version SDK 1.00/1.01 est distincte du numéro de version du jeu.

La capture GPS est échantillonnée toutes les 250 ms par défaut, après les callbacks de télémétrie de la frame. Les en-têtes des tableaux sont relus après la copie ; les UID, métriques et l'accord des séquences physique/logique sont vérifiés. Deux observations successives de la même topologie sont nécessaires. Une incohérence retire le guidage. Cette vérification ne transforme pas les lectures mémoire du jeu en transaction atomique.

La pose est transmise au maximum à 10 Hz, en coordonnées SCS : X est/ouest, Y altitude, Z nord/sud. Le cap SDK est en tours normalisés : 0 nord, 0,25 ouest, 0,5 sud, 0,75 est. Le moteur reçoit `normalize360(-90 - 360 * heading)`. Une marche arrière inférieure à -0,1 m/s suspend la progression.

Le thread du jeu ne réalise aucune écriture sur le canal ni d'export CSV live. Il publie les copies à un worker avec prise de verrou non bloquante. Le worker gère les reconnexions, les écritures overlapped, une attente maximale de deux secondes par écriture et leur annulation à l'arrêt. Les journaux d'initialisation et d'arrêt restent disponibles. Le worker garde le dernier état de contrôle et la dernière pose ; il ne constitue pas un historique de toutes les frames du jeu.

## Contrat brut v1

Canal nommé Windows `BetterGPSProbe-v1`, réservé au compte Windows courant côté serveur. Une longueur de quatre octets little-endian précède chaque objet JSON UTF-8, limité à 8 MiB. Les UID sont des chaînes hexadécimales de 16 caractères ; les pointeurs restent dans la DLL.

L'enveloppe contient `version`, `type`, `sessionId`, `sequence`, `timestampMs`, `generation`. Les compteurs et le temps sont monotones par connexion. Une nouvelle connexion commence par `Hello` et une nouvelle session. Le temps est relatif à la connexion DLL, fondé sur `GetTickCount64`. Les poses conservent leur heure de capture ; elles ne sont pas redatées lors de l'installation du plan.

| Type | Contenu |
|---|---|
| `Hello` | identification du producteur |
| `CaptureStatus` | attente, recalcul ou indisponibilité ; suspension immédiate |
| `RawRouteSnapshot` | empreinte de topologie, `physical[]` et `logical[]` |
| `TelemetryFrame` | `position`, `scsHeading`, `signedSpeedMps`, `paused`, `poseValid`, `speedValid`, `navigationRemainingM` |
| `RouteCleared` | absence de trajet confirmée par deux observations |

Chaque item physique porte `nodeUid`, `distanceFromPreviousM`, `totalDistanceTillEndM`, `timeFromPreviousS`, `totalTimeTillEndS`. Chaque item logique porte les mêmes métriques, ainsi que `type` (0 nœud, 1 prefab), `nodeId`, `itemUid`. La liste des UID logiques de type 0 doit correspondre exactement à la liste physique.

L'empreinte native ignore les variations de distances pendant la conduite, et couvre les UID, l'ordre, les types et les node IDs. Les générations de capture sont distinctes des révisions de plans compilés.

## Préparation

Une seule tâche de compilation possède les caches de carte/PPD. Le récepteur continue à lire la télémétrie pendant cette tâche. Un changement de branche suspend le guidage et remplace le trajet en attente ; le résultat d'une ancienne branche est écarté.

Si le trajet reçu est exactement un suffixe ordonné d'un corridor installé, celui-ci reste en place : stations et actions franchies sont conservées. La même règle permet de terminer une préparation déjà en cours pendant que le camion avance. Un résultat valide peut être réutilisé si le prochain trajet est son suffixe. Les comparaisons portent aussi sur les items logiques et leurs node IDs.

La préparation vérifie les UID dans la carte chargée, la couverture du corridor, les chemins PPD sélectionnés et les cinq garde-fous sémantiques. Chaque champ de panneau doit conserver sa propre preuve `HIGH` sur la bonne branche. Un échec produit `ROUTE_REJECTED` sans instruction. Après une nouvelle installation, une **nouvelle pose de la génération courante** est nécessaire. Aucune pose mémorisée avant la préparation n'est réinjectée comme une pose fraîche.

Les stations du corridor gardent leur pleine précision. Le calcul des limites d'arêtes utilise le même ordre d'addition pour éviter des recouvrements de quelques ulps auparavant masqués par l'arrondi des CSV.

## Exécution et rejeu

```powershell
dotnet publish/BetterGPSMapBridge.dll --capture serve `
  --game-root "H:\SteamLibrary\steamapps\common\American Truck Simulator" `
  --documents "$env:USERPROFILE\Documents\American Truck Simulator" `
  --output ../validation/ingame-live
```

Chaque lancement live crée un sous-dossier daté et écrit `capture.jsonl`, `compiled.jsonl`, `guidance.jsonl`, `asset_sources.txt`. `Ctrl+C` ferme le récepteur proprement ; une déconnexion retire immédiatement l'instruction et la prochaine connexion commence une nouvelle session.

```powershell
# Rejouer les entrées préparées et leurs suspensions, sans charger la carte :
dotnet publish/BetterGPSMapBridge.dll --live replay `
  --input "CHEMIN_SESSION/compiled.jsonl" --output ../validation/replayed-guidance.jsonl

# Repréparer les captures brutes avec les archives de carte :
dotnet publish/BetterGPSMapBridge.dll --capture replay `
  --game-root "H:\SteamLibrary\steamapps\common\American Truck Simulator" `
  --documents "$env:USERPROFILE\Documents\American Truck Simulator" `
  --input "CHEMIN_SESSION/capture.jsonl" --output ../validation/raw-replay
```

`--capture replay` attend chaque préparation et ne reproduit pas sa durée réelle ni les émissions périodiques sans nouvelle trame. `compiled.jsonl` enregistre les décisions effectivement transmises au moteur pendant le live. Le protocole compilé v1 accepte désormais `SourceState`, avec `status` et `detail`, pour rejouer les suspensions (recalcul, absence de télémétrie, pause, marche arrière, rejet ou déconnexion). Cet état reste sans instruction jusqu'à une pose acceptée ou un changement explicite de source/plan. Utiliser le binaire publié ici pour lire cette extension.

`--capture fixture` crée un jeu d'essai à partir des anciens CSV et des poses synthétiques ; `--capture send --pace` l'émet sur le canal brut. Ces commandes servent aux tests et ne fournissent pas des données de camion réelles. `--once` sur `serve` arrête le récepteur après la première déconnexion, utile pour la validation.

## Construction native

```powershell
cmake -S ../Probe -B ../Probe/build -A x64
cmake --build ../Probe/build --config Release
```

La DLL utilise C++17 et le runtime MSVC statique. Les headers SDK SCS et leurs notices sont inclus sous `third_party`. La source et les notices ETS2LA/SCS accompagnent le binaire. Les essais automatisés valident le transport et la préparation, mais le chargement SDK et les lectures mémoire de cette nouvelle DLL doivent encore être vérifiés dans le jeu.
