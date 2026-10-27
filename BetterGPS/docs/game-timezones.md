# Heure locale ATS

BetterGPS formatait directement `game.time + multiplayer.time.offset`.
Ce temps est celui de l'économie ; le réglage des fuseaux ajoute une conversion
spatiale pour les horloges visibles, sans déplacer les événements économiques.

La source commune `current_game_minutes()` utilise maintenant le même calcul
que le formateur natif de date locale :

```
mode 0 : game.time + multiplayer.time.offset
mode 1 : game.time + multiplayer.time.offset + time_zone - default_time_zone
mode 2 : même heure locale que le mode 1
```

0 correspond à Disabled, 1 à Only time, 2 à Full info. Le mode 2 permet au jeu
d'ajouter le libellé de zone ; cette correction conserve les formats graphiques
choisis pour les horloges BetterGPS. Elle n'ajoute pas de suffixe aux textures.

La documentation SCS décrit les offsets en minutes et le fuseau de référence
de l'économie : [guide ATS 1.29, Time zones data](https://modding.scssoft.com/wiki/Games/ATS/Modding_guides/1.29#Time_zones_data).
Les changements d'heure locale lors de l'activation sont également expliqués
par [Max, développeur SCS](https://forum.scssoft.com/viewtopic.php?t=247523).

## Lecture native, sans modification du jeu

Sur ATS Windows x64 1.61.3.18, les signatures du formateur local et du sélecteur
Convoy sont vérifiées avant d'autoriser les lectures :

- Le singleton de jeu déjà observé par l'initialisation du joueur contient
  `time_zone` à +0x138 et `default_time_zone` à +0x170.
- Le mode du profil vient de la variable nommée `g_time_zones`, avec suivi des
  nœuds d'override. Sa résolution partage le parcours des variables vocales
  existant : aucun parcours supplémentaire de l'exécutable au démarrage.
- Dans le contexte Convoy natif (état 8/9, configuration type 5), le mot des
  paramètres serveur à +0x25e remplace le choix du profil, comme dans le jeu.
- Les lectures passent par la vérification de mémoire et SEH existante. Les
  signatures inconnues, pointeurs absents, modes hors plage et valeurs absurdes
  utilisent le temps SDK sans correction plutôt qu'un offset périmé.

Le contrôle a lieu au plus toutes les 200 ms dans le callback de frame existant,
sans thread, accès disque périodique, hook ou appel supplémentaire au moteur.
Un instantané atomique commun évite que les différents renderers utilisent des
offsets distincts. Les changements invalident les textures d'heure/ETA.

## Périmètre

Horloges des téléphones/lockscreens, Sony, NavPlus/SmartNav, CoPilot/Clock,
TomTom/Mack/Volvo/ICC5, Zonar, Sygic et Garmin ; ETA et jours/semaine associés.
L'ETA reste exprimée dans le fuseau courant du camion. Les durées restantes,
statistiques, calcul solaire et événements économiques ne sont pas décalés.
Le calcul multijoueur est appliqué une seule fois. Les offsets non entiers en
heures sont acceptés, sans table géographique propre au plugin.

Au changement de source ou de réglage, le log `[GameClock]` indique :
`native`, `mode`, `local-zone`, `economy-zone`, `offset`, `sdk`, `convoy`, `clock`.
C'est la trace à confronter à l'horloge du camion après un lancement réel.

## Validation

`OverlayPreviewTest <dossier neuf> --timezones` vérifie les signatures du binaire
installé, les trois modes, les nœuds de profil et paramètres Convoy, plusieurs
familles d'affichage, les ETA, minuit et changement de semaine, les offsets
négatifs/non horaires, le multijoueur et la disparition/invalidation des sources.
Les fixtures de chaque famille restent les tests de régression graphique.
