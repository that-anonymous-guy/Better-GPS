# BetterGPS live v1

Mise à jour du 11 octobre 2026 : les snapshots enrichis v2 peuvent porter `route.navigationRemainingAtOriginM`, la distance restante ATS au premier nœud du snapshot. Le moteur publié dans `publish-navigation` l'utilise uniquement si la télémétrie concorde avec l'arête où le camion est situé. Une origine non finie, négative ou plus courte que le corridor est rejetée. Les anciens snapshots restent lisibles. Les consommateurs utilisant l'ancien décodeur strict doivent être mis à jour pour accepter ce champ optionnel ; le protocole natif de la DLL de capture ne change pas. Les preuves `SIGN_GROUP` peuvent également porter un numéro de sortie optionnel `exit`, distinct de `reachableExits` qui contient les branches PPD.

Les actions et le guidage peuvent porter `signTargets` (`road`, `cardinal`, `destination`, `confidence`, `sourceId`, `kind` optionnel). `kind: StreetName` désigne une rue ; son absence conserve `RouteReference`. Ce sont des destinations de manœuvre provisoires MEDIUM, distinctes du nom de la chaussée courante. `sourceId` peut désigner un groupe de panneau, `network-forward:` pour une route rejointe par un raccordement sans choix, ou `route-leg-forward:` pour une route connue dans la même étape du trajet natif, avant la prochaine action. Une cible future ne remplit pas les noms des portions intermédiaires.

Le service `--live` reçoit un **trajet compilé et validé**, puis les poses successives du camion. Le même moteur traite les fichiers de rejeu et les messages live. La nouvelle DLL BetterGPSProbe live 0.2.0 émet les captures brutes sur un canal distinct ; `--capture serve` les prépare automatiquement et alimente ce moteur. Voir [NATIVE_CAPTURE.md](NATIVE_CAPTURE.md) et [INSTALLATION_LIVE.md](../INSTALLATION_LIVE.md). Le guidage apparaît dans la console ; aucun HUD n'est injecté dans le jeu.

## Démarrer

Depuis le dossier contenant ce document, après publication :

```powershell
# Démonstration : les positions sont synthétiques, le corridor et les preuves viennent d’une capture réelle.
dotnet publish/BetterGPSMapBridge.dll --live simulate `
  --directory ../validation/semantic-fix `
  --stem route_002_20261009_180432 `
  --output ../validation/live/route002.synthetic.jsonl

dotnet publish/BetterGPSMapBridge.dll --live replay `
  --input ../validation/live/route002.synthetic.jsonl `
  --output ../validation/live/route002.guidance.jsonl

# Serveur dans un terminal, émetteur dans un deuxième :
dotnet publish/BetterGPSMapBridge.dll --live serve `
  --pipe BetterGPS-v1 --output ../validation/live/pipe.guidance.jsonl

dotnet publish/BetterGPSMapBridge.dll --live send `
  --pipe BetterGPS-v1 --input ../validation/live/scenarios.jsonl --pace
```

Le rejeu affiche les changements d’action/phase dans le terminal. Chaque résultat JSON contient la distance, les champs effectivement affichables et les preuves, y compris les preuves rejetées ou de confiance inférieure. `serve` écrit les résultats au fil des messages ; arrêter avec Ctrl+C. Le serveur accepte une nouvelle connexion après la déconnexion du producteur. Les named pipes sont locaux au compte Windows ; un sandbox peut en interdire l’accès.

Pour préparer seulement le trajet, remplacer `simulate` par `pack`. Ajouter ensuite de vrais messages `VehicleState`, avec les mêmes session et révision. **Ne pas présenter le fichier `synthetic` comme un enregistrement de conduite.**

## Contrat de transport

Named pipe Windows `BetterGPS-v1`, flux d’octets : chaque message est précédé d’une longueur `uint32` little-endian, suivie d’un document JSON UTF-8 de cette taille. Maximum 8 MiB par message. Aucun pointeur natif ni UID converti en nombre JSON. Les UID de carte sont des chaînes hexadécimales de 16 caractères. Une trame partielle ou trop grande est rejetée.

Le fichier de rejeu contient les mêmes documents, un par ligne, sans préfixe binaire. Maximum 128 MiB par fichier ; fractionner les grandes sessions. Rejeu sans attente ; `send --pace` respecte les écarts de temps du fichier, limités à 60 secondes par intervalle. Les propriétés inconnues et les paramètres obligatoires absents sont rejetés.

| Enveloppe | Signification |
|---|---|
| `version` | entier `1` |
| `type` | `Hello`, `RouteSnapshot`, `VehicleState`, `RouteCleared`, `Heartbeat`, `SourceState` |
| `sessionId` | identifiant du producteur, renouvelé à son redémarrage |
| `sequence` | entier 64 bits, strictement croissant dans la session |
| `timestampMs` | entier 64 bits, horloge monotone en millisecondes, jamais décroissante |
| `revision` | version du trajet compilé ; les poses doivent référencer la version installée |
| `route` | présent seulement pour `RouteSnapshot`, sinon null/absent |
| `vehicle` | présent seulement pour `VehicleState`, sinon null/absent |

Premier message obligatoire : `Hello`. `RouteSnapshot` et `RouteCleared` augmentent strictement la révision. `Hello` d’une nouvelle session remet à zéro les horloges, la progression et les événements terminés. Le producteur republie le trajet complet après reconnexion. Un recalcul à longueur identique est une nouvelle révision ; un simple préfixe parcouru ne doit pas être automatiquement interprété comme un nouvel itinéraire.

```json
{"version":1,"type":"Hello","sessionId":"example","sequence":0,"timestampMs":0,"revision":0}
```

Après installation d’un trajet de révision 1 :

```json
{"version":1,"type":"VehicleState","sessionId":"example","sequence":2,"timestampMs":100,"revision":1,"vehicle":{"position":{"x":50,"y":0,"z":0},"headingDeg":0,"speedMps":10,"paused":false,"navigationRemainingM":9000}}
```

Coordonnées SCS en mètres, Y vertical. `headingDeg` est **notre convention** `atan2(deltaZ, deltaX)` : 0 vers +X, 90 vers +Z. Le raccordement SCS doit convertir son orientation dans cette convention, pas recopier une rotation normalisée. La vitesse est positive en m/s. Les nombres non finis sont invalides. `navigationRemainingM` est optionnel et reste diagnostique dans cette version, jusqu’à calibration avec les stations des captures.

## Trajet compilé

`route` contient `routeId`, `edges` et `actions`. Les edges forment un corridor orienté, ordonné et issu du trajet sélectionné :

- `segmentIndex`, `itemUid`, `start` et `end` (points X/Y/Z) ;
- `worldStartM` / `worldEndM` : abscisses géométriques ;
- `navStartM` / `navEndM` : abscisses de navigation en mètres.

Les deux référentiels restent distincts. Les segments doivent être ordonnés, les métriques finies et non décroissantes, les arêtes non dégénérées. Maximum 100 000 edges et 10 000 actions, dans la limite de taille du message.

Chaque action contient `id` (unique, incluant l’occurrence), `eventIndex`, `segmentIndex`, `navStationM`, `instruction`, `branchId`, `confidence`, `proofs`. Sa station doit appartenir à son segment. Le compilateur importe uniquement les actions requises ; les traversées supprimées par le plan restent supprimées.

Les preuves contiennent `field`, `value`, `confidence`, `source`, `sourceId`, `branchId`, `detail`. Champs affichables : `action`, `road`, `cardinal`, `destination`, `exit`. Une action active exige une preuve HIGH issue de `PPD`, un identifiant source et la même branche. Les panneaux exigent une preuve HIGH `SIGN_BOARD` avec une référence au panneau/zone et la branche correspondante. Les détails importés conservent les raisons de validation, textes bruts et attributs typés. Des valeurs contradictoires sont masquées. Un cardinal exige aussi une identité de route validée sur le même panneau.

Le moteur contrôle la cohérence des preuves ; il ne remplace pas le validateur sémantique du compilateur. Le producteur du trajet compilé doit être de confiance. Étiqueter arbitrairement une valeur HIGH n’est pas une certification cryptographique.

Le compilateur `pack` vérifie la classification physique VALID et les cinq compteurs de garde à zéro avant de charger les CSV. Il exige les exports `route_corridor`, `driver_actions_v11`, `branch_board_matches_v11`, `board_semantics_v11` et `v11_regression` d’un même stem.

## Progression et sortie

Projection sur le corridor avec cap, altitude et fenêtre autour de la position précédente. Les seuils initiaux (25 m latéraux, 6 m verticaux, 75° de cap à vitesse supérieure à 1 m/s) restent à calibrer en jeu. Après perte de localisation, la frame suivante recherche de nouveau le corridor. Une discontinuité demande une nouvelle localisation. Aucun ordre ancien n’est conservé dans les états d’incertitude.

La distance affichée est `action.navStationM − position.navStationM`, bornée à zéro ; miles = mètres / 1609.344. Une décision n’est terminée que lorsque le camion passe à un segment ultérieur à celui de la décision. Cela évite de terminer une action simplement parce que la distance affichée atteint zéro. Le premier nœud d’une capture peut être devant le camion : cette version ne reconstruit pas la partie du trajet absente du corridor.

Phases `CRUISE`, `PREPARE`, `EXECUTE`, puis action suivante ou `COMPLETE`. La préparation dépend de la distance physique et de la vitesse. Les phases ne reculent pas pendant une même action, pour éviter le clignotement. `ThenInstruction` est présent si la prochaine décision prouvée est à moins de 300 mètres physiques ; `thenDistanceM` reste une distance de navigation entre les décisions.

États sans instruction : `WAITING_ROUTE`, `WAITING_POSITION`, `NO_ROUTE`, `PAUSED`, `OFF_ROUTE`, `RELOCALIZING`, `UNCERTAIN_ACTION`, `STALE`, `COMPLETE`. Plus de 2 secondes sans pose compatible expire le guidage, même si des heartbeats arrivent. Le serveur vérifie cette expiration toutes les 500 ms ; le rejeu la déclenche par les horodatages des heartbeats.

La sortie `LiveGuidance` expose `actionId`, `instruction`, `distanceM`, `distanceMi`, `road`, `cardinal`, `destination`, `exit`, `thenInstruction`, `thenDistanceM`, `proofs`, `navStationM`, `worldStationM`, `status`, `detail` et la révision. Les champs non prouvés restent null. La complétion concerne les décisions couvertes par le corridor, pas une preuve de livraison ou d’arrivée exacte à destination.

## Raccordement natif suivant

Le plugin doit capter les canaux SCS de pose, vitesse et pause, copier un état cohérent à `frame_end`, puis faire l’IPC sur un thread séparé. Ce thread ne doit lire aucun pointeur du jeu. Le service de compilation devra transformer les snapshots bruts en corridor/actions avec la carte chargée une fois, puis publier `RouteSnapshot` et relayer les poses sous la bonne révision. Cette couche de capture/compilation dynamique et le HUD ingame restent distincts du protocole et du moteur livrés ici.
