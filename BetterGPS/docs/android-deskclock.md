# Android DeskClock dans NavPlus HD

Portage fonctionnel de DeskClock AOSP `android-4.2.2_r1`, commit
`0ed03e8e14cf2edcda9ee3ecc604d4e048c8ccf3`, sur le display du T680 NG.
L'application remplace la première page Clock réalisée précédemment.

## Écrans et commandes

- Les trois onglets Android sont Timer, Clock, Stopwatch ; les icônes et les
  swipes horizontaux changent d'onglet avec une animation de 220 ms.
- Le fond standard reste `navplushd_gradient_background.dds` sur les pages
  de l'application. Le mode Night conserve son fond noir dédié.
- Clock affiche l'heure ATS en 12/24 h et le jour/semaine du jeu. Toucher l'heure
  bascule le cadran analogique/digital. Aucun calendrier réel n'est inventé.
- Le bouton alarme ouvre la liste : ajout/suppression, activation, saisie
  numérique de l'heure, AM/PM, répétition par jour, label et son/silencieux.
- Par défaut : Los Angeles, Albuquerque, Dallas, New York, dans cet ordre. Les
  choix existants sont conservés, y compris une liste explicitement vidée. Les
  quatre villes tiennent ensemble dans le panneau de l'horloge.
- Les 280 villes du catalogue Android sont sélectionnables, avec recherche et
  pagination. Leur heure est calculée depuis la référence UTC native d'ATS.
  Les règles saisonnières viennent de Windows à la date réelle du PC : ATS
  n'expose pas de date calendaire mondiale utilisable pour cette application.
- Les minuteurs acceptent une durée HHMMSS, et fonctionnent simultanément :
  démarrage, pause/reprise, +1 min, réinitialisation, label et suppression.
  Comme TimerObj, la durée d'origine reste disponible après un +1 min.
- Le chronomètre propose démarrage/arrêt, tours et remise à zéro. Share exporte
  `logs/clock/laps.csv`, adaptation locale du partage Android.
- Settings propose 12/24 h, analogique/digital, temps réel/jeu pour les
  compteurs, sons, volume indépendant, durée du snooze et extinction du son.
  Night mode est un écran sombre tactile dont on peut sortir avec Return.
- Return ferme d'abord le clavier/éditeur/la liste ouverte, puis revient au
  Home NavPlus. Home et son raccourci gardent leur fonction habituelle.

Les alarmes suivent toujours l'heure locale ATS, avec ses réglages de fuseaux
et l'offset multijoueur. Par défaut les minuteurs et le chronomètre utilisent
le temps réel, y compris en arrière-plan/pause du jeu. Le réglage Game time
utilise les minutes brutes du SDK et ne compte pas les retours en arrière
temporels. Les changements de fuseau modifient les horloges et alarmes, jamais
la durée des compteurs. Si la référence UTC native est indisponible, les villes
affichent une heure inconnue plutôt qu’un décalage supposé.

## Arrière-plan et persistance

Le worker existant fait avancer les compteurs et surveille les alarmes sans
ajouter de thread. Un popup Snooze/Dismiss ou +1 min/Dismiss apparaît aussi sur
Home, la carte et les autres applications NavPlus. Les expirations simultanées
restent en file ; fermer une alerte ne supprime pas les suivantes. Les icônes
minuteur, chronomètre et alarme apparaissent dans le bandeau supérieur.

Les sons sont ceux d'Android, convertis en PCM WAV. Un gain fixe de 30 %
(-70 % sur le signal) précède la sortie, indépendamment du réglage conservé.
Le volume est appliqué aux
échantillons propres à l'application, sans modifier le volume Windows ou ATS.
Le volume interne des notifications vaut 50 % par défaut. La sortie suit aussi
le master d'ATS et son mute, lus directement dans les cvars natives
`s_master_volume` / `s_master_mute` du profil actif toutes les 200 ms. Une
modification en jeu est donc prise en compte sans attendre l'écriture du CFG.
La lecture réutilise l'unique scan de constructeurs existant, sans nouveau scan
au démarrage. Si les cvars deviennent indisponibles, le son du plugin est coupé.
Le gain final est : signal × 0,30 × volume Clock / 100 × master ATS.
Le son est suspendu lorsque le display est éteint ou le jeu hors gameplay.
Référence SCS : https://modding.scssoft.com/wiki/Documentation/Engine/Configuration_variables

`plugins/BetterGPS/BetterGPSClock.ini` conserve réglages, alarmes, villes,
compteurs et tours. Les écritures sont atomiques et limitées ; les compteurs
réels reprennent le temps écoulé depuis la dernière sauvegarde après relance.
Ce fichier utilisateur n'est jamais fourni comme configuration de livraison.
Les installations ciblées le capturent pour récupération sans l'écraser.

## Provenance et intégration

Sources fournies :
`C:/Users/Crym2/Desktop/BetterGPS/assets/misc/android_resources/Android-reference/DeskClock`.
Les ressources importées sont sous `material/copilot/t680/deskclock/`, avec
`import.json` (empreintes, dimensions et provenance) et `NOTICE-AOSP.txt`.
L'importeur vérifie l'égalité exacte des pixels des PNG et DDS RGBA8.
Les sources de référence et leur licence Apache 2.0 sont conservées dans
`docs/references/android-deskclock/`. Les interfaces graphiques sont adaptées
aux 900 × 544 pixels logiques et aux bandeaux NavPlus ; la DLL n'a pas besoin
d'un émulateur, d'une JVM ni d'un Android externe. Les widgets Android, services
système, écran de verrouillage du système et intents de partage sont remplacés
par les mécanismes natifs de BetterGPS lorsqu'ils concernent l'application.

`DeskClockModel.h` contient les états et la persistance ;
`DeskClockPresentation.h` partage les rectangles entre rendu et hit-testing ;
`DeskClockRuntime.h` raccorde le worker, les sons et l'heure ATS.

## Vérification

`python BetterGPS/Dev/workflow.py test --suite deskclock` teste les états avec
une horloge injectée, les expirations, pause/reprise, tours/export, récurrences,
snooze, persistance/offline, temps du jeu, villes, rendus et vrais clics tactiles.
Les profils Clock/T680/Global incluent aussi cette suite.

## Plein écran Night mode

Night mode étend son fond noir à toute la surface du display. Les bandeaux
NavPlus se retirent vers le haut et le bas après 2,5 secondes sans activité,
en 280 ms. Un mouvement du curseur sur le display les rappelle avec la même
animation. Les menus et alertes gardent les commandes visibles. Voir
`navplus-fullscreen.md` pour la détection passive, distincte des clics.
