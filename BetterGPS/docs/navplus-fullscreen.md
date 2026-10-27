# Plein écran NavPlus HD

Le mécanisme partagé concerne uniquement Clock > Night et les photos Gallery,
en Fit et 2x. Les autres applications et la grille Gallery gardent leurs
bandeaux habituels.

- Le contenu utilise toute la surface 900 × 544 dès l'entrée dans ce mode.
  Fit conserve le ratio de la photo, donc peut garder de petites marges
  naturelles ; 2x et ses limites de pan utilisent ce même grand viewport.
- Les contrôles sont affichés à l'entrée. Après 2 500 ms sans activité, le
  groupe supérieur glisse vers le haut et le groupe inférieur vers le bas,
  en 280 ms avec une courbe continue. Les commandes propres à Gallery
  accompagnent les barres NavPlus ; elles ne réservent pas de place vide.
- L'activité réinitialise le délai et inverse proprement une animation en
  cours. Les menus et alertes maintiennent les commandes visibles.
- Un premier clic pendant que les barres sont cachées/en mouvement les
  rappelle seulement. Il n'active jamais une hitbox Home/Return invisible.
  Le raccourci Home garde sa fonction ; les gestes de zoom/pan restent actifs.
- Sortir du mode, changer de page, de camion ou de génération d'alimentation
  remet l'état à zéro. Le mode ne se propage pas aux autres applications.

## Mouvement de souris

Le curseur seul (ou le mode d'interaction du menu) fournit sa position client.
Un mouvement déclenche une requête passive de 300 ms, uniquement si un mode
plein écran existe. Aucune requête permanente n'est émise pour une souris
immobile. Les coordonnées passent par TouchCpuPicker et ses preuves natives
habituelles : texture réellement copiée, projection de la surface, scissor,
profondeur, véhicule/page/power courants et contrôle non masqué par le menu.

Une preuve valide rappelle les barres sans entrer dans la file de gestes.
Les requêtes passives sont exclues du backend de replay GPU, même dans les
tests isolés. Elles ne cliquent pas, ne changent pas de caméra et n'ouvrent
aucune application. Une position hors écran, masquée, expirée ou appartenant
à une ancienne page/alimentation n'est pas acceptée. Quitter le mode curseur
annule les requêtes ; les mouvements de caméra ordinaires ne sont pas utilisés.

La préparation existante calcule le délai et les positions des bandes ; aucun
thread, scan disque ou timer Windows supplémentaire. L'image n'est recalculée
pendant l'animation que lorsque sa position visible change.

## Vérification

Suite `immersive` : délai, sorties opposées, inversion au mouvement, reset de
contexte, rendu intégral Fit/2x/Night, absence de barres et d'icônes résiduelles,
requêtes de survol sans clic, refus de preuves invalides, premier clic de réveil.
Les suites Gallery, DeskClock, CoPilot et le profil input couvrent les pages,
les retours et la géométrie tactile existante.
