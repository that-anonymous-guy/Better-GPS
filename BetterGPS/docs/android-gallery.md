# Gallery Android dans NavPlus HD

Adaptation de Gallery2 AOSP `android-4.2.2_r1`, fournie dans
`Android-reference/Gallery`. Les références AlbumPage, PhotoPage, PhotoView
et SlideshowPage sont conservées dans `docs/references/android-gallery/`.
L'icône vient des ressources originales, importée en DDS RGBA sans perte.

## Navigation

- La tuile Gallery, en haut à droite du Home, ouvre l'application avec le
  délai et le swipe NavPlus habituels (500 + 320 ms).
- Grille Android sur fond sombre, adaptée au paysage : cinq colonnes,
  deux lignes visibles ; swipe vertical pour parcourir les lignes.
- Une miniature ouvre la photo avec un agrandissement de 200 ms. L'image
  conserve son ratio dans le corps du display, entre les bandeaux NavPlus.
- Swipe gauche/droite ou flèches : photo précédente/suivante. Les extrémités
  restent bornées ; seul le diaporama revient à la première photo.
- Les photos Fit/2x utilisent toute la surface 900 × 544, en conservant leur
  ratio. Les barres NavPlus et les commandes photo se retirent après 2,5 s
  sans activité et reviennent au mouvement du curseur sur le display.
  Double toucher ou Fit/2x bascule le zoom. Maintenir-glisser déplace l'image agrandie, avec
  blocage réel aux bords (sans zone vide ni déplacement de la carte GPS).
- Le menu de l'application et les trois points NavPlus proposent Diaporama,
  Détails en vue photo et Actualiser. Le diaporama avance toutes les trois
  secondes, durée de SlideshowPage, et s'arrête en quittant Gallery.
- Return ferme d'abord menu/détails, puis quitte la photo pour la grille,
  puis la grille pour Home. Home et son raccourci reviennent directement
  à Home via l'animation habituelle. Une alerte de gauge conserve Gallery
  et sa photo dans l'historique ; les alertes Clock restent prioritaires.

## Captures Steam locales

SteamPath et ActiveUser viennent du registre Steam, lus à l'ouverture et
à l'actualisation. La source est :

```
STEAM/userdata/ACTIVE_USER/760/remote/270880/screenshots
```

270880 est ATS. Sans compte actif publié, les dossiers numériques userdata
sont consultés ; sans SteamPath, le dossier Steam standard est essayé.
Les images JPEG, PNG et BMP sont classées par modification décroissante.
Le sous-dossier thumbnails n'est jamais compté comme un deuxième album.
Un changement de compte Steam est pris en compte à l'actualisation.

La collection est relue toutes les cinq secondes uniquement pendant que
Gallery est visible ; Actualiser déclenche une relecture immédiate. Les
images nouvelles/modifiées sont reconnues par chemin, date et taille.
Les erreurs de dossier/image donnent un état vide/illisible récupérable.
L'interface ne lance pas la caméra et ne modifie/supprime aucune capture.
La galerie interne d'ATS n'est pas interceptée par cette adaptation.

## Coût et arrêt

Aucun accès disque, scan Steam ni thread Gallery au chargement de la DLL.
À la première ouverture, un worker de priorité basse traite le catalogue
et les décodages WIC, hors du worker de composition/tactile. Il dort quand
il n'a plus de travail. Les callbacks de rendu et de souris ne lisent pas
les fichiers. L'arrêt est explicite après l'arrêt du worker de composition.

Miniatures Steam préexistantes utilisées si disponibles, sinon réduction
à 256 px. La photo courante et ses voisines sont décodées à 2048 px maximum,
avec un cache borné à trois photos détaillées et vingt miniatures/échecs.
La photo courante et la zone visible sont protégées contre l'éviction.
Le catalogue est borné à 20 000 entrées et exclut les fichiers vides ou
supérieurs à 512 Mio. Les photos restent dans le dossier Steam, sans copie
ni conversion permanente de la bibliothèque.

## Validation

Suites `gallery` et `gallery-live` : catalogue et décodage différés,
miniatures, images corrompues, ratio, zoom/pan, scroll, diaporama,
actualisation, gestes réels TouchInput, clics mis en file, protections de
pages, alertes et navigation. `gallery-live` lit la collection Steam locale
et enregistre ses rendus uniquement dans le dossier de run ignoré par Git.
Le profil input couvre également les autres devices et les hooks tactiles.

Le mécanisme commun et les preuves de survol sont décrits dans
`navplus-fullscreen.md`.
