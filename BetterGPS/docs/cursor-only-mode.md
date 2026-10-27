# Curseur sans menu

Le clic droit maintenu libère le curseur pour utiliser les écrans tactiles,
sans ouvrir BetterGPS, son bandeau ou les diagnostics. Les mouvements et
boutons souris sont retirés du flux reçu par la caméra du jeu via les mêmes
hooks DirectInput que le menu. Le clavier et les autres périphériques restent
disponibles. Les clics gauches utilisent la chaîne tactile existante.

Dans **Shortcuts**, **Cursor only shortcut** accepte une touche avec modificateurs,
le clic droit, milieu ou les boutons latéraux X1/X2. Le clic gauche reste réservé
aux interactions tactiles. **Cursor activation** propose **Hold** (défaut) et
**Toggle**. Le raccourci doit être distinct de Menu, Display toggle et Home/Map.

**Cursor double press**, activé par défaut, ajoute un verrouillage au mode
**Hold** : deux appuis rapides sur le raccourci configuré gardent le curseur
libre après relâchement. Un appui supplémentaire le désactive, même si cette
dernière pression est maintenue. Le délai suit le réglage de double-clic de
Windows, pour les boutons souris comme pour les touches avec modificateurs.
Un appui maintenu seul reste immédiat. Désactiver l'option restaure le Hold
simple ; le mode Toggle garde son fonctionnement habituel.

Relâcher en mode Hold restaure le confinement et la position antérieurs du
curseur. En mode Toggle, une seconde pression désactive le mode. Une perte de
focus ou l'ouverture du menu désactive le mode ; la fermeture du plugin libère
aussi la souris. Les données souris en attente sont vidées pour éviter un saut
de caméra à la reprise. Aucun autre thread ni hook supplémentaire n'est ajouté.

Les choix sont enregistrés dans `BetterGPSDisplayPower.ini` : `CursorKey`
(défaut 2, VK_RBUTTON), `CursorModifiers` (0), `CursorToggle` (0=Hold, 1=Toggle).
`CursorDoubleTap` vaut 1 par défaut (y compris dans les anciens fichiers sans
cette entrée) ; 0 désactive le double appui. La perte de focus, l'ouverture du
menu et le changement de raccourci annulent aussi le double appui en attente.
L'état actif reste temporaire, sans être restauré au prochain lancement.

Nouveaux défauts : **F9** Menu, **Shift+F9** Display toggle, **Ctrl+F9** Home/Map.
Les anciens défauts Menu Ctrl+F9 et Home/Map Ctrl+Shift+F9 sont migrés une fois.
Les autres choix personnalisés sont conservés. La version de préférence des
écrans passe à 2 ; les emplacements, textures et réglages graphiques sont inchangés.

Validation : `OverlayPreviewTest <dossier neuf> --cursor-only`, `--menu-mouse`,
`--menu-shortcut`, `--device-pages`, et `TouchPhoneTest <dossier neuf>`.
