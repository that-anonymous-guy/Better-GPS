# ATS — diagnostic autonome des mods

Cet outil est séparé de BetterGPS. Il fonctionne sans ses DLL, ne s'installe pas dans les plugins et ne modifie ni mods ni profils. Windows 10/11 x64 et .NET Runtime 10 x64 sont nécessaires.

## Utilisation

1. Extraire toute l'archive dans un dossier normal, par exemple sur le Bureau. Garder tous les fichiers ensemble.
2. Lancer ATS et charger la sauvegarde à tester.
3. Double-cliquer sur `DIAGNOSTIC_MODS_ATS.cmd`, puis accepter la demande administrateur Windows. ETW nécessite cette élévation.
4. Retourner dans le jeu et conduire pendant trois minutes. **Ctrl+Maj+F8** ajoute un marqueur lorsque tu ressens une saccade. **Ctrl+C**, dans la fenêtre du diagnostic, termine plus tôt.
5. Le rapport HTML s'ouvre à la fin. Les données restent dans `captures/<date>/` : rapport HTML filtrable, rapport JSON et événements bruts compressés `capture.json.gz`.

Les demandes administrateur concernent l'application de diagnostic. Le jeu reste lancé normalement. L'outil possède sa propre session de trace, la ferme à la fin et ne supprime pas les sessions d'autres outils. Il n'y a aucun téléchargement ni envoi réseau pendant son utilisation.

Privilégier Ctrl+C pour terminer proprement. Après un arrêt brutal du processus, relancer le lanceur : il nettoie uniquement les anciennes sessions portant son identifiant et dont le processus propriétaire n'existe plus.

## Ce que le rapport identifie

- Cadence CPU des Present DXGI d'ATS : moyenne, 1 % low, P99, maximum et gros intervalles. Cela fonctionne indépendamment de BetterGPS, sur le rendu DXGI utilisé par ATS en DirectX 11.
- Archives et dossiers effectivement lus par ATS autour des 30 plus grands intervalles de plus de 33,33 ms. Le rapport sépare mods, jeu/DLC, BetterGPS et fichiers non attribués.
- Pour les archives ZIP, y compris beaucoup de fichiers `.scs`, l'offset d'une lecture permet de retrouver l'entrée d'archive concernée : texture `.dds`, modèle `.pmg`, définition, etc. Les plages couvrent données ou en-têtes d'entrée, sans décompresser tout le mod. Une lecture peut couvrir plusieurs entrées.
- Pour les mods en dossier : chemin physique exact. Pour HashFS SCS ou un format non indexable : conteneur seulement, avec la limite explicitement affichée.
- Marqueurs personnels, inventaire des montages trouvés dans `game.log.txt`, noms du manifeste si accessible, erreurs/avertissements du journal et ressources CPU/RAM d'ATS, BetterGPSNavigation et du collecteur.
- Par défaut : échantillons CPU avec identifiants de threads et piles regroupées par chaînes de modules. Les DLL BetterGPS chargées dans ATS sont comprises ; les symboles privés du jeu ne sont pas disponibles. Les cinq threads les plus occupés sont relevés toutes les 0,5 seconde.
- Charge globale NVIDIA, mémoire GPU utilisée/totale, fréquence, température et puissance via le nvidia-smi installé avec le pilote, à raison d’un relevé par seconde. Un seul processus de télémétrie caché est lancé puis arrêté. Aucune installation supplémentaire ; si les compteurs manquent, le rapport le dit.
- Fenêtres de deux secondes classées par FPS avec charge GPU, thread ATS le plus occupé, modules exécutés et indications de limite CPU/GPU possibles. Ces indications ne sont pas des verdicts par mod.

Les événements perdus, lectures sans nom et plafonds atteints sont signalés. Aucun FPS n'est inventé si aucun Present exploitable n'a été reçu. Les intervalles hors premier plan sont exclus ; les menus et chargements au premier plan restent inclus.

## Ce qu'il ne peut pas affirmer automatiquement

**Un fichier lu près d'une saccade est un suspect, pas une cause confirmée.** Une lecture fréquente peut coïncider par hasard avec les pics. La fenêtre de corrélation couvre l'intervalle d'image et 250 ms avant/après. Le classement porte sur les 30 plus gros pics, pas toutes les baisses de FPS.

Les lectures mesurées sont logiques : elles ne prouvent pas une saturation physique du SSD. Les accès mémoire mappés, ressources déjà chargées, textures en VRAM, shaders, géométrie, trafic/IA ou scripts peuvent coûter cher sans lecture visible à cet instant. L'outil ne mesure pas directement le temps GPU ni la latence d'affichage et ne peut pas automatiquement attribuer un coût de rendu à un mod précis.

Une erreur de journal ou une grosse texture ne prouve pas non plus un ralentissement. Les avertissements du journal concernent toute la session ; ils ne sont pas synchronisés automatiquement avec les pics.

Pour confirmer un suspect, comparer le **même passage**, la même sauvegarde et les mêmes réglages avec et sans le suspect, en conservant ses dépendances et celles de la carte. Ne pas retirer au hasard une dépendance de carte d'une sauvegarde. Garder le diagnostic actif dans les deux essais, puisqu'il a son propre coût.

## Commandes avancées

Depuis un terminal administrateur pour `capture` uniquement :

```powershell
.\ATSModDiagnostics.exe capture --seconds 180 --label reference
.\ATSModDiagnostics.exe capture --seconds 180 --label leger --light
.\ATSModDiagnostics.exe capture --documents 'C:\chemin\American Truck Simulator'
```

Sans élévation, pour relire ou comparer des captures :

```powershell
.\ATSModDiagnostics.exe analyze --capture 'captures\date\capture.json.gz' --output 'rapport-relu'
.\ATSModDiagnostics.exe compare --baseline 'captures\reference\capture.json.gz' --candidate 'captures\essai\capture.json.gz' --output 'comparaison.html'
.\ATSModDiagnostics.exe --self-test
```

`--output DOSSIER` choisit la destination ; `--pid NOMBRE` sélectionne un processus particulier. Le mode `--include-unfocused` est réservé aux tests contrôlés : il inclut les pauses dues à une perte de focus et peut fausser les statistiques habituelles.

## Coût et stockage

Le collecteur est en priorité basse ; son CPU et sa RAM sont enregistrés. Session ETW en mémoire avec 32 Mio de buffers, pas de gros ETL écrit pendant la conduite. Durée maximale : dix minutes. Bornes : 300 000 lectures, 100 000 Present, 750 000 échantillons CPU, 50 000 groupes de piles et 50 000 chemins distincts. Les limites atteintes sont signalées ; l'indexation ZIP après capture se limite aux 20 premiers mods lus, 200 000 entrées par archive. Les autres conteneurs restent identifiés.

Les événements sont compressés après la capture. L'historique par défaut conserve au maximum huit captures et vise 128 Mio, en protégeant la capture en cours. Seuls les dossiers marqués par l'outil, contenant exclusivement ses fichiers connus, peuvent être nettoyés. Un dossier avec des fichiers supplémentaires est conservé. Les sorties externes demandées avec `--output` ne sont pas nettoyées automatiquement.

## Sources techniques

- [Microsoft : événements FileIo et association des noms de fichiers](https://learn.microsoft.com/en-us/windows/win32/etw/fileio-readwrite).
- [Microsoft : bibliothèque TraceEvent](https://github.com/microsoft/perfview/blob/main/documentation/TraceEvent/TraceEventProgrammersGuide.md).
- [Intel PresentMon : identifiants du fournisseur DXGI](https://github.com/GameTechDev/PresentMon/blob/main/PresentData/ETW/Microsoft_Windows_DXGI.h). Cet outil utilise directement ETW et ne redistribue pas PresentMon.
- [SCS : emplacement du dossier utilisateur et du journal](https://modding.scssoft.com/wiki/Documentation/Engine/Game_user_path).

Les licences des bibliothèques redistribuées sont dans `licenses/`.

## Correctif beta-0.2

Le collecteur active aussi le canal Windows de correspondance entre identifiants de fichiers et noms. Les lectures anonymes sont conservées temporairement et résolues avec les noms reçus en fin de trace, y compris pour les fichiers déjà ouverts avant le diagnostic. Un identifiant associé à plusieurs noms reste non attribué. Le rapport affiche le taux de couverture, les récupérations tardives et le nombre de lectures restant anonymes. Une ancienne capture qui avait déjà écarté ces identifiants ne peut pas être complétée rétroactivement.

## Capture approfondie beta-0.3

Le même lanceur active désormais les mesures approfondies sans changer les mods. Le mode `--light` coupe les échantillons et piles CPU pour limiter le coût. Les mesures GPU et threads restent disponibles. Les deux modes ont une surcharge différente : ne pas interpréter une différence entre modes comme un gain dû aux mods. La collecte des piles est bornée et les modules sont recherchés avec un cache d’adresses. Les adresses sont attribuées à la liste de DLL présente au début de la capture ; un module chargé plus tard peut rester inconnu.

Les packs d’assets sont des données consommées par le même moteur : une pile `amtrucks.exe` ne distingue pas à elle seule la texture ou l’IA d’un mod. Une seule capture peut montrer une saturation ou un coût de DLL, mais ne garantit pas de désigner un unique pack graphique. Le GPU est global à toutes les applications, la mémoire utilisée n’est pas un compteur d’évictions et les chiffres ne sont pas des temps GPU par image. La première seconde de télémétrie peut contenir de l’activité antérieure à la capture.

Sources : [NVIDIA, nvidia-smi](https://docs.nvidia.com/deploy/nvidia-smi/index.html) ; [Microsoft, événements et piles kernel TraceEvent](https://github.com/microsoft/perfview/blob/main/src/TraceEvent/Parsers/KernelTraceEventParser.cs).
