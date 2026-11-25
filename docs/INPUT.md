<!-- Mapping et rejeu neutres : contrat public de phase 3. -->
# Entrées configurables à pas fixe

`assets/input/orbit.json` illustre le schéma **version 1** : tableaux `actions`
et `axes`, noms uniques entre les deux. Une action contient `name`, `bindings` ;
un axe ajoute `mode` (`rate` ou `delta`). Chaque binding contient `source` :
`key`, `mouseButton`, `gamepadButton`, `gamepadAxis`, `mouseX`, `mouseY`,
`wheelX`, `wheelY`. Les quatre premiers exigent `control` (nom Somnium sensible
à la casse, inventaire dans `core/keys.hpp` et `core/input_names.hpp`). Touches
physiques : A..Z, Digit0..9, F1..F24, modificateurs et clavier étendu complet.
Aucun code SDL dans le fichier ni dans input.

Options des axes : `scale` fini (défaut 1), `invert` booléen (défaut false),
`deadzone` uniquement pour un axe manette, dans [0,1[ (défaut 0). Une paire
clavier est deux bindings, l’un avec scale -1, l’autre +1. Pour une manette,
`slot` vaut `any` (défaut) ou 0..3, attribué dans l’ordre des connexions aux slots
libres. Actions : OU de leurs boutons. Axes rate : somme bornée à [-1,1],
sticks normalisés asymétriquement par 32768/32767, gâchettes dans [0,1], zone
morte axiale remise à l’échelle. Axes delta : somme sans borne ±1, consommée
une fois, sans multiplication par dt. Seuls les deltas acceptent `held`, nom
résolu d’une action conditionnante, évaluée dans l’ordre des événements.

Une source physique a une seule destination, même entre action et axe ; un
doublon est refusé, et `any` entre en conflit avec tout slot du même contrôle.
Champ/contrôle/version inconnus, mauvais type ou conflit donnent une erreur
contextualisée. `Configuration::parse/load` construit une nouvelle valeur ou
échoue sans toucher à la configuration active. Le jeu résout `ActionId` et
`AxisId` dans `start(World, Configuration)` ; aucune recherche par nom au pas.
Les contextes d’input sont reportés.

`platform::pollEvents(nextStep)` collecte les événements bruts puis input les
conserve. `advance` exige des pas consécutifs depuis zéro. Les événements futurs
attendent ; chaque transition apparaît dans le journal ordonné `transitions`,
en plus du résumé pressed/released/held. Deux taps dans un pas donnent quatre
transitions. Les états empruntés restent valides jusqu’au prochain advance.
Un retard supprimé de l’accumulateur ne saute jamais un indice de simulation.
Perte de focus : relâchements ; déconnexion : seule la manette concernée.
Les impulsions déjà reçues avant une perte de focus restent livrées.

`setCapture({keyboard,mouse})` insère une frontière ordonnée au prochain pas.
L’entrée en capture libère une fois les actions concernées. Les appuis capturés
et deltas sont ignorés pour le jeu ; les relâchements sont toujours traités.
La sortie exige un nouvel appui, aucun bouton maintenu ne se réactive seul.
La file et le journal réservent 128 entrées, grandissent si nécessaire ; les
états et les quatre slots sont dimensionnés avant simulation.

## Rejeu et captures

`assets/replays/orbit.json`, schéma version 1, contient `events` ordonnés par
`step` (entier depuis zéro), puis `kind` : key/mouseButton/gamepadButton avec
control/down ; mouseMotion/mouseWheel avec x/y ; gamepadAxis avec control/value ;
gamepadConnected/gamepadDisconnected ; focusLost. Les événements manette ont
un `device` entier neutre. Champs inconnus, contrôles inconnus, valeurs non finies,
axes hors [-32768,32767] et pas désordonnés sont rejetés avant installation.

`sandbox --replay replays/orbit.json` installe `platform::InputReplay` : les
événements physiques de gameplay sont écartés, les événements de fenêtre restent
actifs. Runtime demande les événements à platform avant chaque pas, y compris
pendant un rattrapage. `--capture-step N` exige un rejeu, fige la simulation après
N pas, présente la pose courante pendant trois secondes puis ferme normalement.
Les scripts `tools/presentation/` lisent la fenêtre après `CAPTURE_READY` ; ils
n’injectent aucun événement système et ne déplacent ni focus ni pointeur.
Les captures Windows restent **manuelles** : les fenêtres visibles et leur
lecture dans la session personnelle peuvent gêner le travail ou être masquées.
Les preuves GPU hors écran et le test de fenêtre restent requis. Les scénarios
Linux/macOS utilisent exactement le même rejeu ; leurs pixels restent des
captures de présentation, distinctes des références analytiques hors écran.
