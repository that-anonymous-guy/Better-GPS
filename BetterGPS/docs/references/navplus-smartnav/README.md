# Référence NavPlus HD / SmartNav 2.0 — DEA600

Référence conservée à la demande de l'utilisateur le 25 octobre 2026.
Avant toute modification de ce système, lire l'entrée correspondant à la
fonction et regarder les pages/figures du PDF. Les consignes et validations
de l'utilisateur priment sur les comportements génériques du fabricant.

## Sources préservées

- [Manuel opérateur DEA600, fourni sous le nom SmartNav 2019](smartnav_operators_manual_2019.pdf) :
  58 pages. Titre interne **Owner's Manual DEA600 Radio**, mention **MY 2016**,
  date de couverture **11/07/2019**, conservée sans interprétation du format.
  Ce manuel décrit la plateforme Delphi, avec notamment des illustrations
  Kenworth ; ce n'est pas une spécification propre à une configuration ATS.
- [Guide d'installation des caméras, Rev A](smartnav_camera_installation_guide_rev_a.pdf) :
  25 pages, fourni précédemment par l'utilisateur. Document complémentaire
  pour câblage, entrées vidéo et déclenchements automatiques.
- [Texte du manuel, indexable](smartnav_operators_manual_2019.txt) et
  [texte du guide caméra](smartnav_camera_installation_guide_rev_a.txt).
  Les extractions peuvent contenir des caractères mal reconnus et un ordre de
  lecture imparfait dans les doubles colonnes ; les PDF font foi.
- [Provenance et empreintes SHA-256](sources.json).

Les numéros ci-dessous sont les **pages PDF à partir de 1**. Dans le manuel
opérateur, ils correspondent aussi aux numéros imprimés vérifiés dans les
sections principales. Conserver cette convention pour les futures notes.

## Index rapide du manuel opérateur

| Fonction recherchée | Pages | Figures / points à consulter |
| --- | --- | --- |
| Matériel et fonctions générales | 4–5 | Écran résistif 7 pouces, radio séparée, quatre entrées analogiques, commandes physiques |
| Gestes tactiles | 6–7 | Figures 1–5 : toucher, quatre directions de swipe |
| Home, pages et six applications | 7–9 | Figures 6–10 : grille 2×3, widget audio à droite, indicateur de page, déplacement des icônes |
| Bandeaux et notifications | 8–11 | Tableau 1 ; figures 11–15 : états, écran horloge hors marche radio, volume/luminosité |
| Back, Home et Menu | 5, 8–9 | Retour à l'écran précédent, Home direct, menu contextuel ; Back sans effet sur Home |
| Audio, sources et bibliothèque | 11–23 | Figures 16–37 : média, radio, bibliothèque, recherche, présélections |
| Widget audio sur Home | 23–26 | Figures 38–46 : USB, Bluetooth, CD, AUX et bandes radio |
| Téléphone | 26–31 | Figures 47–58 : contacts, clavier, favoris, historique, appels et popup numérique |
| Messages | 31–32 | Figures 59–63 : conversations, notification, réponses prédéfinies, éditeur |
| Horloge et alarmes | 33–35 | Figures 64–69 : horloge, villes, chronomètre, timer, alarmes, Snooze/Dismiss |
| Caméras et sélection des vues | 35–36 | Figures 70–71 ; voir le conflit de vue par défaut ci-dessous |
| Gauges : pages, états, zoom et alertes | 36–38 | Figures 72–75 : grille, cadran normal et cadran rouge |
| Gauges : presets, priorités et placement | 38–41 | Figures 76–79 et tableaux p.39–40 |
| Réglages généraux et caméras | 41–42 | Figures 80–83 : grille de réglages, sliders d'automatisme caméra, unités et luminosité |
| Wi-Fi / Bluetooth | 42–47 | Figures 84–95 : listes, appairage et erreurs ; profils Bluetooth p.45 |
| Mise à jour / sons tactiles | 47 | Mise à jour navigation CoPilot, gauges et système ; figure 96 |
| Date, heure et fuseau | 47–48 | Figures 97–98 : formats 12/24 h, réglages automatiques/manuels |
| Gestion des applications | 48–49 | Figures 99–100 : liste, informations et actions |
| Informations système | 49–50 | Figure 101 : version Android, modèle, logiciel, adresses réseau |
| Langues et synthèse vocale | 50–51 | Figures 102–104 : anglais, espagnol, français, choix du moteur vocal |
| Réglages audio | 51–54 | Figures 105–111 : égalisation, balance/fader, volume selon vitesse, haut-parleurs |
| Dépannage et restrictions DDA | 55–56 | États de conduite / frein de parc, limites de saisie et de téléphone |
| Entretien, licences et réglementation | 56–58 | Hors logique du plugin ; ne pas confondre avec des instructions de travail |

## Home, navigation entre écrans et bandeaux

Le manuel montre jusqu'à **six applications par page**, plus le widget audio
à droite. La première page contient Gauges, Navigation, Audio, Phone,
Camera Views et Clock (p.7, fig.6). Les pages supplémentaires sont à droite ;
l'indicateur de page est sous le bandeau supérieur. Les icônes se réorganisent
par appui long puis glisser-déposer (p.8, fig.7).

- **Back / Return** remonte à l'écran précédent et ne fait rien sur Home.
- **Home** rejoint directement l'accueil.
- **Menu** dépend de l'application ouverte ; sur Home, il ouvre les réglages.
- Le bandeau peut montrer heure/date, température selon équipement OEM,
  Bluetooth, Wi-Fi, USB, mode muet, DDA, alarme et avertissement de gauge.
- Le panneau de notifications accepte un swipe vers le bas (p.8).
- La vue horloge en mode radio OFF (p.10, fig.11) n'établit pas que l'écran
  doit rester alimenté lorsque le contact du camion est complètement coupé.

La compatibilité navigation/CoPilot est mentionnée, mais ce manuel n'est pas
le manuel détaillé de l'application CoPilot : ne pas en déduire une règle
précise de guidage, de calcul d'itinéraire ou de projection de la carte.
Les latences et durées de swipe choisies pour BetterGPS ne sont pas des
valeurs fabricant établies par ce document.

## Gauges : ce que le manuel établit

### Première page et données

La liste p.39 confirme les six éléments de notre page principale :

1. Engine Oil Temperature.
2. Main Trans Oil Temperature.
3. Manifold Pressure / Boost.
4. DPF Status.
5. Fuel Economy.
6. Percent Torque.

Les données viennent des bus numériques du véhicule réel (p.36).
Le système peut gérer **30 gauges réparties sur cinq pages**. La dernière
page sélectionnée est mémorisée ; la page 1 revient notamment à la première
ouverture après un cycle IGN/BATT, ou si la page mémorisée est invalide/vide
(p.36–37).

### États, agrandissement et alertes

- État normal avec unités impériales ou métriques.
- État d'avertissement : **cadran rouge et agrandissement automatique**
  (p.37, figures 74–75).
- Une pression sur un cadran l'agrandit ; une pression sur la vue agrandie
  revient à la grille. Une pression pendant l'agrandissement l'annule (p.37).
- Si une seconde gauge entre en alerte, la première alerte agrandie reste
  affichée **au moins trois secondes** avant de laisser place à la seconde
  (p.37–38). Cela ne signifie pas un retour à une température normale : la
  gauge réduite reste rouge tant que son avertissement persiste.
- Les états d'erreur utilisent des décalages d'aiguille par rapport à la
  position dite **Null** : donnée absente −15°, sous-échelle −5°, sur-échelle
  +5° (p.37). Le texte ne définit pas une conversion universelle de Null en
  angle de notre texture ; ne pas inventer cette calibration.
- Ce manuel renvoie au manuel du véhicule pour la fonction de chaque gauge.
  Il ne donne pas nos seuils d'alerte, ni une calibration numérique fiable du
  MPG. Les figures restent des exemples visuels, pas un jeu de télémétrie ATS.

### Presets et autres gauges répertoriées

Deux presets, édition, chargement de mises à jour et réinitialisation usine
figurent dans le menu de Gauges (p.38, fig.76). Les modifications se valident
ou s'annulent ; leur enregistrement choisit un preset (fig.77–78).
La population dépend des données disponibles et des priorités. Une page sans
gauge est désactivée. Les données indisponibles peuvent conduire à masquer la
gauge au cycle de contact suivant (p.39).

Le tableau de priorité du preset 1 (p.40) répertorie aussi :

| Élément | Priorité indiquée |
| --- | --- |
| Ammeter | 1 |
| Drive Axle #1 (front) Oil Temperature | 2 |
| Drive Axle #2 (rear) Oil Temperature | 3 |
| Drive Axle #3 (center) Oil Temperature | 4 |
| Air Suspension Pressure 1 | 5 |
| Air Filter Restriction Pressure | 6 |
| Fuel Filter Restriction / Fuel Pressure Gauge | 7 |
| Pusher Axle #1 Pressure | 8 |
| PTO Hour Meter | 9 |
| Percent Horsepower, Idle Time, Idle Fuel, Fuel Usage Rate | NA dans ce preset |
| Tractor Brake Application | N/A, dans le cluster |

Ces numéros sont des **priorités**, pas des numéros de page.
Une entrée NA n'établit pas l'absence du capteur dans tous les véhicules.
Le tableau du preset 2 p.40 est plus court ; ne pas le compléter par supposition.

Le déplacement d'une gauge se fait par glisser-déposer avec accrochage aux
cases. Les autres gauges se décalent ; atteindre un bord peut changer de page
sans dépasser les pages actives (p.40). La réinitialisation utilise une
confirmation OK/Cancel (p.40–41, fig.79).

### Différences volontaires actuelles de BetterGPS

Ces demandes restent prioritaires jusqu'à une nouvelle instruction utilisateur :

- Six gauges seulement ; DPF fixe CLEAN ; température transmission représentée
  par la température de différentiel ; couple dérivé de la source boost.
- Oil : 100–300 °F ; trans : 150–300 °F ; boost : 0–80 ; couple : 0–100 %,
  avec butées demandées. Ne pas les remplacer automatiquement par les
  déflexions d'erreur du manuel.
- Moyenne MPG affichée à une décimale ; la photo à 7.1 MPG venait d'un autre
  camion et n'est pas un maximum de calibration.
- Alertes demandées au-dessus de 250 °F huile / 280 °F trans, ouverture unique
  par franchissement, acquittement et réarmement au refroidissement.
- Return remonte à l'action précédente. Ne pas remplacer ce choix par une
  fermeture au moindre clic sur le grand cadran sans demande.

Les 30 gauges et les capteurs réels cités ici ne prouvent **aucune nouvelle
valeur branchable dans ATS**. Il faut vérifier séparément SDK, données natives
et version du jeu avant de promettre une mesure.

## Caméras : sources contradictoires et règle actuelle

Le manuel opérateur p.35 affirme **Reverse à l'ouverture**, alors que sa
figure 71 montre Forward sélectionné. Le guide d'installation Rev A indique
**Forward par défaut** aux pages 13 et 25. La cause de cet écart n'est pas
établie : ne pas présenter une hypothèse de firmware comme un fait.

Le manuel confirme quatre vues, une à la fois, une sélection autour du dessin
du camion, un indicateur de sélection et un mode plein écran (p.35–36).
Il décrit recul / clignotants pour les vues automatiques et indique que Forward
est manuel. Les réglages séparent les trois automatismes (p.42, fig.82).
La p.36 mentionne une image Forward inversée gauche/droite ; vérifier la
convention du flux si de vraies vues sont ajoutées, sans retourner une image
virtuelle arbitrairement.

Le guide d'installation précise, lui, que les automatismes ne s'appliquent
que lorsque l'application Camera est déjà affichée (p.24), pas en GPS.
Il ne fixe pas la priorité de deux déclencheurs simultanés ni le retour exact
après leur extinction. Aucun des deux PDF ne fournit quatre coordonnées 3D
de montage pour le T680.

**Choix utilisateur en vigueur :** Forward par défaut si des flux sont ajoutés.
La page Camera reste actuellement non branchée par choix utilisateur.
L’[audit du 25 octobre](../../truck-camera-audit-20261025.md) confirme toutefois
les options DigitalVision du T680 NG : ne pas généraliser une absence de
caméras physiques à cette famille. L’interface actuelle conserve sa
page opaque RGB(10,10,10), son sélecteur à gauche réduit de 30 %, et le popup
fourni « No camera available. » à chaque ouverture, fermé uniquement par OK.
Ne pas ajouter de flux, changer la vue par défaut ou supprimer ce popup en
appliquant silencieusement le manuel.

### Index complémentaire du guide d'installation

| Sujet | Pages du guide caméra |
| --- | --- |
| Fournisseurs et références Pana-Pacific / Velvac | 1–7 |
| Adaptateur PP207020, quatre entrées et câblage | 8–12 |
| Video 1 Reverse, 2 Forward, 3 Right, 4 Left ; manuel/auto | 13 |
| Accès Camera depuis Home, activation des entrées | 14–18 |
| Position du sélecteur et exemples de vues | 19–20 |
| Compatibilité : T680, gauche/droite/recul = YES | 21 |
| Activation réelle par ESA / routage CECU | 22–24 |
| Limite : automatismes uniquement dans Camera | 24 |
| J1939 : PGN 65089 ; SPN 2367 gauche, 2369 droite, 2391 recul ; Forward par défaut | 25 |

## Autres fonctions utiles à une reproduction

- **Widget audio** (p.23–26) : média avec piste/artiste/pochette et
  précédent/lecture-pause/suivant ; AUX sans ces commandes ; radio avec bande,
  fréquence et précédent/suivant. Une pochette générique remplace une image
  absente. Audio ne force pas la sortie d'un menu de réglages au changement
  de piste (p.42).
- **Audio** (p.11–23) : cinq sources média et quatre radio, sélection unique,
  sources média indisponibles grisées ; appui long de deux secondes sur
  suivant/précédent pour l'avance/retour rapide (p.13). La marche arrière peut
  couper l'audio, avec exception d'un appel Bluetooth (p.11).
- **Téléphone** (p.26–31) : clavier, contacts, favoris, récents, recherche ;
  synchronisation avec état de chargement ou d'échec ; quatre entrées visibles
  par liste et huit favoris. Appel entrant, appel en cours, mute, transfert,
  second appel et clavier numérique fournissent des références de layout.
- **Messages** (p.31–32) : quatre messages visibles, dix conversations avec
  dix messages récents chacune, lecture vocale, réponses prédéfinies et éditeur.
- **Clock** (p.33–35) : horloge, villes, chronomètre, compte à rebours et
  alarmes ; popup de réglage puis Snooze/Dismiss, icône d'alarme dans le bandeau.
- **Réglages** (p.41–54) : grilles, listes, sliders et confirmations ; unités
  impériales/métriques, pas de luminosité de 10 % ou 25 %, formats 12/24 h et
  trois formats de date. Langues anglais/espagnol/français.
- **DDA** (p.10,32,56) : fonctions restreintes en conduite. La définition
  générale p.56 est frein de parc desserré **ou vitesse ≥5 km/h** ; mode normal
  si frein serré **et vitesse <5 km/h**. La p.32 dit « more than 5 km/h » pour
  l'éditeur de messages : noter cette différence au seuil, ne pas l'effacer.
  Ce mécanisme réel n'est pas une demande d'ajouter des blocages au mod.

## Mode d'emploi de cette référence

1. Choisir la section de l'index adaptée à la tâche et ouvrir les pages du PDF.
2. Relever le comportement et les figures, en conservant le numéro de page.
3. Distinguer document constructeur, choix utilisateur et contraintes ATS.
4. En cas de contradiction, la noter et conserver le dernier choix utilisateur.
5. Mettre à jour cette analyse si une nouvelle source ou décision change une
   conclusion ; garder les PDF originaux intacts.

La sauvegarde et cette analyse n'ont modifié ni DLL, ni texture, ni réglage du
plugin. Elles ne constituent pas l'implémentation des fonctions supplémentaires.
