# T680 NG — Clock

La tuile Clock de Home ouvre la première page Clock avec la transition NavPlus
existante : latence de 0,5 s, swipe de 0,32 s, puis page immédiatement visible.
Return et Home reviennent à Home avec la latence de fermeture de 0,25 s.

Le fond est `navplushd_gradient_background.dds`, sans les tuiles de Home.
Les deux bandes fournies sont importées sans altérer leurs pixels :
`navplushd_clock_top_time.dds` et `navplushd_clock_bottom.dds`.
Les commandes Kenworth communes restent au-dessus du contenu de la page.

L'heure utilise `game.time` et le décalage multijoueur, au format 12 h sans zéro
initial. Les minutes ont deux chiffres. Le jour/semaine utilise la même base
Mon–Sun que les autres écrans du plugin, avec `WEEK 1` au premier lundi.
Une télémétrie absente affiche des tirets ; aucune date civile n'est inventée.

Roboto : heure ExtraBold (800), minutes Thin (100), AM/PM et jour/semaine
Medium (500). Le tracking vaut 0 pour l'heure et −50/1000 em pour les autres
libellés. Le rendu utilise les polices Roboto installées sous Windows, comme
les présentations Roboto déjà présentes dans le plugin.

Les icônes de minuterie, chronomètre et alarme ne déclenchent pas encore
d'autres pages. Seule la page d'arrivée est implémentée à cette étape.

Référence matérielle : [index SmartNav](references/navplus-smartnav/README.md),
manuel opérateur p.33–35. Le screenshot et les choix du propriétaire priment.

Validation : `OverlayPreviewTest <dossier neuf> --t680-clock` couvre les clics,
les latences, le rendu opaque pendant le swipe, les formats midi/minuit, le
changement de semaine, le temps multijoueur, l'isolation carte/menus et le retour
à Clock après une interruption par une gauge.
