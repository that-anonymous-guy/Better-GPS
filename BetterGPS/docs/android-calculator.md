# Calculator Android dans le NavPlus HD

Adaptation de Calculator AOSP `android-4.2.2_r1`, fourni dans le dossier
Android-reference/Calculator. Les fichiers Logic.java, CalculatorEditable.java,
EventListener.java et le layout paysage sont conservés dans
`docs/references/android-calculator/`, avec la licence Apache 2.0.

La disposition suit la capture NavPlus fournie : panneau scientifique à gauche,
pavé numérique au centre, opérateurs/parenthèses et égal à droite, champ bleu
et touche d'effacement en haut. Le symbole d'effacement vient directement
des ressources Android ; l'import RGBA est sans perte. Les touches et textes
sont rendus à la résolution du display, avec les bandeaux NavPlus existants.

Le module Calculator remplace la destination Phone de la tuile Home inférieure
gauche. Ouverture après 500 ms puis swipe 320 ms ; Return/Home et le raccourci
reviennent via le swipe de fermeture habituel après 250 ms. Le calcul reste
en mémoire pendant les retours Home et coupures de contact dans cette session.
Les alertes de gauges gardent Calculator dans l'historique et les alertes Clock
restent prioritaires. Aucun itinéraire, zoom ou pan n'est modifié.

Fonctions : +, −, ×, ÷, parenthèses, puissances, factorielle, racine, sin/cos/tan
(en radians comme Android), ln, log base 10, e et π. Priorités, puissances
associées à droite, multiplication implicite et parenthèses finales omises
sont prises en charge. Un nouveau chiffre après résultat repart de zéro ; un
opérateur poursuit le calcul. Les décimales répétées et opérateurs successifs
suivent CalculatorEditable ; les opérateurs finaux sont ignorés à l'évaluation.
Effacement : un caractère pendant la saisie, tout le résultat/erreur ensuite.
Un second égal rappelle l'expression, comme le mode Clear de Logic.java.

Le moteur Java Arity contenu dans le JAR n'est pas exécuté dans ATS. Son rôle
pour ces fonctions est repris par un évaluateur natif borné (256 caractères,
64 niveaux), sans JVM, thread, scan mémoire ni coût au chargement du jeu.
Affichage à 12 chiffres significatifs ; NaN/domaine invalide = Error,
infini = ∞. Les touches de calcul n'envoient aucune touche clavier au jeu.

Validation : suite `calculator` (calculs et tactile réellement mis en file),
régressions des pages NavPlus et profil d'entrée. L'import ciblé ne modifie
aucune texture Home/Clock ou autre ressource personnalisée.
