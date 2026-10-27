# Écrans 3D interactifs : recherche et application à BetterGPS

Audit du 19 octobre 2026. Recherche documentaire et lecture de sources, pas une
implémentation ni une certification du tactile ingame. Aucun DLL, asset, réglage
de périphérique, hook ou fichier installé n'est modifié par cet audit.

## Conclusion

La référence la plus instructive pour notre problème précis est désormais
**FlightGear**, devant Doom 3 : son code montre la conversion d'une intersection
sur un triangle en coordonnées UV, sans imposer une boîte propre à chaque bouton.
Le CCF de BeamNG reste une excellente référence de comportement automobile.
X-Plane fournit le meilleur exemple de contrat public entre une surface, un
périphérique et ses événements.

Leur principe ne suffit pas à rendre BetterGPS fonctionnel : ces projets ont
accès à la géométrie, aux transformations et/ou à un service natif de sélection.
Dans ATS, il faut encore obtenir et valider ces données à l'instant du rendu.
Un moteur utilisant Direct3D 11 n'est pas pour autant compatible avec Prism3D.

Recommandation : éprouver une sélection géométrique générique sur la surface
complète, conserver les actions et les layouts existants, et ne pas remettre en
service le replay graphique ayant crashé avant une validation de son interopérabilité.
Cela reste une orientation de prototype, pas une solution déjà démontrée dans ATS.

## Qualité des preuves et périmètre

- Code lu : CCF fourni localement, scripts BeamNG concernés, Doom 3, partie
  sélection/UV de FlightGear, API caméra SPF, couches tactiles BetterGPS.
- Contrats documentés : avionique X-Plane, interfaces 3D Unreal, terminaux SOMA,
  instruments MSFS, cockpit Bus Simulator 21, scripts OMSI.
- Fonctionnalité confirmée sans accès établi à son algorithme interne : moniteurs
  Stormworks, MFD Star Citizen, vue rapprochée des écrans Train Sim World.
- Les autres titres et noms de mods du texte fourni ne sont pas considérés comme
  des preuves d'implémentation. Les entrées sans source technique exploitable
  n'ont pas été artificiellement transformées en explications de moteur.
  En particulier, ce rapport ne certifie ni toutes les versions citées de TSW,
  ni le système interne de Prey, Stationeers, Carrier Command 2 ou SimRail.

La documentation d'un composant Unreal ne prouve pas qu'un jeu particulier
l'utilise. Une annonce de fonction prouve son existence, pas son algorithme.

## 1. FlightGear : la chaîne générique la plus pertinente

Dans [renderer.cxx](https://github.com/FlightGear/flightgear/blob/next/src/Viewer/renderer.cxx#L550),
`FGRenderer::pick` demande les intersections correspondant à la position dans la
fenêtre. Pour les destinataires demandant des UV, `uvFromIntersection` retrouve
les trois indices du triangle et ses poids d'intersection, puis combine les UV
des trois sommets. Le résultat et le callback restent associés au même impact.
Les données absentes produisent une valeur d'échec, pas un clic valide par défaut.

Ce que nous pouvons reprendre conceptuellement :

- Séparer identification de surface et résolution des boutons.
- Utiliser les vrais triangles et UV, plutôt que mesurer un rectangle englobant
  dans la capture d'écran.
- Transporter l'identité et les coordonnées ensemble.
- Rendre explicite l'absence de résultat.

La formule, avec des poids barycentriques obtenus sur le triangle 3D, est :

`UV_impact = poidsA * UV_A + poidsB * UV_B + poidsC * UV_C`.

Attention : des poids calculés simplement dans l'image projetée nécessitent une
correction de perspective. Pour un écran plan avec UV affines, une homographie
est une autre possibilité, à condition d'avoir les vrais coins projetés à chaque
frame. Un rectangle fixe ne remplace aucune de ces méthodes.

Limite : FlightGear a accès à sa scène OpenSceneGraph. BetterGPS n'a pas encore
un accès validé équivalent à la scène finale de Prism3D.

## 2. Doom 3 : surface interactive et interface sont deux couches

Dans [GuiTrace](https://github.com/id-Software/DOOM-3/blob/master/neo/renderer/RenderWorld.cpp#L901),
le rayon est transformé dans le repère de l'objet ; les surfaces portant une GUI
sont testées ; l'impact est converti dans les axes de texture. Le code du
[joueur](https://github.com/id-Software/DOOM-3/blob/master/neo/game/Player.cpp#L4200)
utilise ensuite ces coordonnées pour déplacer le curseur de l'interface choisie.

Leçon : une coordonnée locale exploitable doit être produite avant de déclencher
une action. La fonction `GuiTrace` précise elle-même qu'elle ne traite pas
l'occlusion par les surfaces non-GUI. Ce détail interdit de la recopier comme
une solution complète contre les clics à travers les objets.

Le code est publié sous GPL avec conditions associées ; ce rapport n'en intègre
aucun fragment dans BetterGPS.

## 3. BeamNG / CCF : des boutons 3D configurés par page

Le fichier local
[ccfScreenInteract.lua](/C:/Users/Crym2/Downloads/thw_ccf2/lua/ge/extensions/zeit/ccfScreenInteract.lua:76)
obtient `getCameraMouseRay()`, transforme les boîtes avec la position et
l'orientation du véhicule, puis appelle `intersectsRay_OBB`. Chaque boîte
porte un identifiant et un type : clic, déplacement, glissement, etc.

Le
[gestionnaire de pages](/C:/Users/Crym2/Downloads/thw_ccf2/vehicles/ccf/lua/controller/gauges/screenManager.lua:36)
charge les zones propres à la page active. Dans le Home LHD fourni, le trigger
116 correspond à `navi`, avec centre `(0.561, 0.145, 0.6453)` et dimensions
`(0.035, 0.005, 0.035)` dans le repère du véhicule. Le mode `drawBoxes` rend les
zones inspectables.

À reprendre : instrumentation visible, zones actives conditionnées par la page,
association explicite entre cible et action. À ne pas reprendre tel quel : des
coordonnées 3D de chaque bouton propres au CCF. Cela casserait notre objectif
Add Display et l'édition indépendante des layouts.

Les scripts signalent une restriction de réutilisation ; ils sont analysés ici,
pas copiés ni exécutés.

## 4. X-Plane : un écran dessiné n'est pas un écran sélectionnable

L'[API avionique](https://developer.x-plane.com/sdk/XPLMDisplay/#AVIONICS_API)
distingue écran, contour et callbacks souris. Pour recevoir les événements dans
le cockpit 3D, il faut aussi un `ATTR_manip_device`. La
[spécification OBJ8](https://developer.x-plane.com/article/obj8-file-format-specification/#ATTR_manip_device)
exige que ce manipulateur ait la même forme et le même mapping UV que l'écran.
Ce mécanisme est documenté à partir de X-Plane 12.1.0 ; ne pas l'attribuer
indifféremment à toutes les versions de X-Plane de la liste.

À reprendre : un descripteur de périphérique séparant résolution logique,
surface visible, mapping et événements down/drag/up. Les coordonnées X-Plane
croissent vers le haut : notre convention top-left doit être explicitée et
convertie, jamais supposée commune à tous les moteurs.

## 5. SOMA : un contrat géométrique volontairement contraint

La [documentation HPL3](https://wiki.frictionalgames.com/page/HPL3/Gui#In-Scene_GUI_Rendering)
décrit une surface plane à quatre sommets, avec UV de 0 à 1 et origine en haut
à gauche, reliée à un terminal par son sous-maillage et une taille logique.
L'[API terminal](https://wiki.frictionalgames.com/page/HPL3/SOMA/Scripting/Scripting_Api)
distingue également visibilité de la GUI et autorisation d'interaction.

À reprendre : un contrat de surface explicite et des états indépendants
visible/interactif. Ne pas imposer ses contraintes à tous les modèles ATS :
certains ont plus de quatre sommets ou des coordonnées répétées.

## 6. Unreal et simulateurs de bus : distinguer boutons et panneaux UI

Le [Widget Interaction Component](https://dev.epicgames.com/documentation/en-us/unreal-engine/widget-interaction-component?application_version=4.27)
teste un rayon contre une interface 3D et simule des événements de pointeur.
Il distingue utilisateur, pointeur, focus, source d'interaction et affichage de
diagnostic. Une source personnalisée peut fournir son propre résultat d'impact.

La [documentation Bus Simulator 21](https://stillalive.atlassian.net/wiki/spaces/BSM/pages/461471866)
décrit des colliders sur les commandes du dashboard, leur association aux bones
et aux actions, ainsi que le diagnostic des boutons non sélectionnables.
Cela ne documente pas l'algorithme de tous ses écrans tactiles.

Dans [OMSI](https://wiki.omnibussimulator.de/omsiwikineu/index.php?title=Scripting_System),
un maillage déclaré avec `[mouseevent]` appelle les triggers de pression,
maintien/glissement et relâchement. Là encore, le moteur fait la sélection.

À reprendre : un même destinataire pendant toute la durée du geste, la priorité
du menu sur les appareils, l'annulation lors d'un changement de contexte et une
visualisation des cibles avant d'activer des actions.

## 7. MSFS, Stormworks et références de comportement

MSFS utilise notamment des instruments HTML/JS/CSS liés au moteur par Coherent.
La [documentation Microsoft](https://docs.flightsimulator.com/msfs2024/html/6_Programming_APIs/JavaScript/JavaScript.htm)
et le code EFB 787 lu dans l'installation locale montrent la séparation entre
enregistrement d'un instrument interactif et gestion des clics dans son UI.
Changer BetterGPS en HTML n'apporterait pas à lui seul la sélection 3D d'ATS.

L'[annonce officielle Stormworks](https://store.steampowered.com/news/posts/?appids=573090&enddate=1558108345)
explique que les moniteurs émettent les données de contact vers les scripts.
Un [exemple de contrôleur publié par son auteur](https://github.com/TearingMoon/StormWorks-Microcontrollers/blob/main/Whiteboard%20Controller/MyMicrocontroller.lua)
sépare les coordonnées, l'état pressé et le dessin. C'est une référence pour le
contrat d'entrée, pas une publication de la sélection 3D interne du moteur.

La [documentation Star Citizen](https://support.robertsspaceindustries.com/hc/en-us/articles/360020925254-How-to-Land-Your-Ship)
confirme l'usage d'un mode d'interaction pour les MFD. Celle de
[Train Sim World](https://support.dovetailgames.com/hc/en-us/articles/28572764655122-Train-Sim-World-Settings-Explained)
confirme une option de vue rapprochée des écrans. Ces références peuvent guider
l'ergonomie, mais n'établissent pas leurs algorithmes de picking.

## 8. Ce que nous avons réellement côté ATS

### Géométrie connue, mais pas encore projetée en direct

Nos inspections existantes contiennent déjà des PMG, sommets et UV. Le
[W900](/C:/Users/Crym2/Downloads/BetterGPS-main/docs/kenworth-w900-screen.md:9)
a une pièce GPS à 16 sommets et des UV autour de 3 à 4. Le
[T680](/C:/Users/Crym2/Downloads/BetterGPS-main/docs/kenworth-t680-screen.md:15)
a une pièce à quatre sommets et une différence entre ratio physique et ratio UV.

Cela prouve que certains modèles sont inspectables hors ligne, pas que leur
placement vivant, accessoires, variantes, animations et projection sont résolus.
La lecture devra respecter les matériaux réellement montés et la priorité des
mods. Un nom de texture ou une résolution commune ne suffit pas pour identifier
un appareil physique.

Le mapping final devra reproduire les UV et le mode d'adressage du matériau
(répétition, miroir ou clamp), ses transformations éventuelles, puis le cadrage
du contenu BetterGPS. Ni un clamp global à `[0,1]`, ni un modulo systématique
ne sont corrects pour tous les matériaux.

### Caméra : piste SPF plus précise qu'une simple promesse

Le [header public SPF](https://github.com/TrackAndTruckDevs/SPF-Framework/blob/main/include/SPF/SPF_API/SPF_Camera_API.h)
expose notamment position mondiale active, rotation de tête, siège et FOV final
horizontal/vertical de la caméra intérieure. Il ne fournit pas dans ce header
un service complet écran-vers-UV ni une matrice finale générique de picking.
Les unités doivent être vérifiées fonction par fonction ; les commentaires de
rotation ne sont pas tous uniformes.

Cette piste mérite un prototype en lecture seule, mais ajouter SPF n'est pas
automatiquement nécessaire ni suffisant. La pose après animations, head tracking,
inclinaison cabine et zoom doit coïncider avec celle qui produit l'image. Les
canaux de télémétrie livrés dans notre repo donnent des offsets tête/cabine,
pas ce contrat complet de projection.

### Autres plugins Prism3D

[PrismTextureStreamer](https://github.com/Baldywaldy09/PrismTextureStreamer)
documente l'injection d'image dans les textures de cabine, pas le picking.
[PrismMedia](https://github.com/foodmaher/PrismMedia/blob/main/PER-DISPLAY-ROUTING.md)
documente une association à un draw précis et ses limites multi-écrans. Cette
technique peut informer la recherche de la géométrie, mais ne démontre ni des UV
de clic corrects ni un système multi-appareils tactile prêt à importer.

### Notre code et la régression actuelle

Les hitboxes/actions existent dans `TouchActions.h`. Le backend de
`TouchPicking.h` réexécute des draws avec une sortie ID/UV. Les captures
préservées avaient des impacts reconnus à UV nuls ; le correctif expérimental
a ensuite crashé dans la chaîne proxy Snowymoon/ReShade. Voir
[le diagnostic conservé](/C:/Users/Crym2/Downloads/BetterGPS-main/docs/touchscreen.md:3).

Un test GPU isolé ne reproduit pas nécessairement cette chaîne. Ce constat ne
prouve pas que le picking GPU est une mauvaise technique en général ; il
interdit de déclarer notre implémentation compatible en l'état.

## 9. Architecture proposée, sans modifier les layouts ni Add Display

Une surface physique est associée à un appareil logique indépendant. Les
boutons restent dans le repère du layout, pas dans le modèle de chaque voiture.

`curseur fenêtre -> surface visible -> UV matériau -> contenu affiché -> layout -> action`

Le backend géométrique et le backend graphique doivent produire le même contrat :

- résultat valide/invalide avec motif précis ;
- identité de l'appareil ET de la surface touchée ;
- frame/instant d'échantillonnage et générations véhicule/appareil/page ;
- coordonnées locales avant puis après conversion vers le layout ;
- statut de visibilité/occlusion et source de la preuve.

Après la sélection : inverser exactement les transformations du rendu (cadrage,
orientation, fit, zoom global, placement de module). Les hitboxes doivent venir
des mêmes descriptions de placement que l'affichage ; le retour Crown fixe est
une exception existante à corriger ultérieurement, pas une raison de changer le
style ou la détection des appareils.

Au début d'un geste, capturer un seul appareil. Conserver cette cible jusqu'au
relâchement, y compris lorsque le pointeur quitte la surface. Définir explicitement
si le déplacement hors surface prolonge le plan capturé ou annule le geste ; ne
jamais transférer le swipe à un autre appareil. Le menu, la perte de focus, le
changement de véhicule, l'extinction et les splashes annulent/interdisent l'action.

## 10. Ordre de travail et critères d'arrêt

1. **Projection diagnostique, aucune action.** Choisir un appareil connu, dessiner
   ses contours projetés et le point touché. Vérifier des points répartis sur la
   surface avec caméra, siège et zoom variables. Si les contours dérivent, arrêter
   ici : retoucher les boutons ne peut pas corriger la projection.
2. **Établir les données source.** Tester une pose/projection en lecture seule via
   accès moteur/caméra ; si elles sont insuffisantes, étudier la géométrie et les
   constantes du draw natif identifié. Lire un VB ne révèle pas automatiquement
   sa disposition, son skinning ni ses matrices. Les cas non résolus restent
   explicitement non interactifs. Ne pas introduire une dépendance sans décision.
3. **Surface vers layout.** Afficher simultanément UV bruts, UV adressés et
   coordonnées logiques. Tester les coins, limites UV répétées, letterbox, zoom,
   paysage/portrait/3:2 et modules déplacés.
4. **Une action téléphone, puis les gestes.** Réutiliser les destinations explicites
   Home/Map et la sélection de thème par appareil. Ne pas simuler le raccourci
   clavier qui bascule dans les deux sens.
5. **Plusieurs appareils et occlusion.** Deux appareils affichant la même texture,
   écran partiellement masqué, clic dans le cadre, retour de menu et changement
   de camion. Aucune identité choisie seulement d'après les dimensions.
6. **Validation de la chaîne réelle.** ATS avec Snowymoon/ReShade présents. Tests
   automatisés, puis observation ingame. Aucun replay ayant crashé réactivé comme
   prétendu correctif sans régression proxy pertinente.

Instrumentation minimale : appareil survolé, page, coordonnées, cible visée,
down/move/up/cancel et motif de rejet. Une capture doit permettre de distinguer
« rien reçu », « mauvaise surface », « mauvaise projection », « hors bouton » et
« état interdit ».

Critères proposés, pas résultats obtenus : erreur de reprojection visuelle visée
à deux pixels fenêtre sur des points répartis ; aucun clic hors écran activant un
appareil ; événements uniques sans répétition par frame ; aucun geste transféré
entre appareils ; aucune attente GPU bloquante ni régression de sortie du jeu.
Mesurer p95/p99 du temps de frame contre une référence sans interaction, avec
un budget défini, plutôt que promettre « zéro impact FPS » sans mesure.

## Décision à retenir

La recherche ne justifie ni une réécriture des thèmes ni des hitboxes par modèle
de voiture. Elle justifie un **prototype de picking géométrique générique**,
avec FlightGear comme référence algorithmique et X-Plane comme référence de
contrat, avant toute nouvelle livraison tactile. Si la pose/projection réelle
n'est pas récupérable par la voie retenue, documenter cette limite puis choisir
un autre backend ; ne pas masquer l'absence de preuve par des coordonnées devinées.
