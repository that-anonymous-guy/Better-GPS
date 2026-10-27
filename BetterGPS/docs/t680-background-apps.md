# DEA600 — applications résidentes

CoPilot est mémorisé par écran et par génération d'initialisation du camion.
La première ouverture, automatique après le démarrage ou depuis Home, conserve
la latence de 500 ms, le swipe de 320 ms et les trois secondes de logo.
Une ouverture interrompue avant sa fin ne marque pas l'application comme prête.

Quand la carte est disponible, CoPilot reste résident en passant par Home,
Clock, Gauges ou Camera. Les ouvertures suivantes conservent la latence et le
swipe, sans logo ni attente de trois secondes. Une coupure de contact, une
extinction d'écran ou une pause ne suppriment pas cet état. Une nouvelle
initialisation du véhicule le remet à zéro. Aucun fichier d'état ni thread
supplémentaire n'est créé.

Le bandeau affiche `navplushd_icon_copilot.dds` seulement après la première
ouverture terminée. Son premier emplacement source est x=19, y=18, 47×46 sur
le bandeau 1324×92, mesuré sur l'ancien artwork. Les emplacements suivants
progressent de 55 pixels, soit 8 pixels d'espacement, en suivant la position
et l'échelle du module supérieur. Le masque et la liste des icônes sont prêts
à recevoir ultérieurement les états de chronomètres, timers et alarmes.

Les versions du propriétaire de `navplushd_top_5by3.dds` et
`copilot_v9_logo.dds` sont conservées octet pour octet. L'installation de cette
fonction ne remplace pas ces fichiers dans le dossier du plugin.

Référence : manuel DEA600 p.8–9, notifications d'applications tierces ; la
logique de résidence et les durées ci-dessus suivent la demande du propriétaire.

Validation : `OverlayPreviewTest <dossier neuf> --copilot-background-apps`.
