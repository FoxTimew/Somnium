<!-- Mapping et rejeu neutres : contrat public de phase 3. -->
# Entrées configurables à pas fixe

`assets/input/courtyard.json` illustre le schéma **version 1** : tableaux `actions`
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

`assets/replays/courtyard.json`, schéma version 1, contient `events` ordonnés par
`step` (entier depuis zéro), puis `kind` : key/mouseButton/gamepadButton avec
control/down ; mouseMotion/mouseWheel avec x/y ; gamepadAxis avec control/value ;
gamepadConnected/gamepadDisconnected ; focusLost. Les événements manette ont
un `device` entier neutre. Champs inconnus, contrôles inconnus, valeurs non finies,
axes hors [-32768,32767] et pas désordonnés sont rejetés avant installation.

`sandbox --replay replays/courtyard.json` installe `platform::InputReplay` : les
événements physiques de gameplay sont écartés, les événements de fenêtre restent
actifs. Runtime demande les événements à platform avant chaque pas, y compris
pendant un rattrapage. `--capture-step N` exige un rejeu, fige la simulation après
N pas, présente la pose courante pendant trois secondes puis ferme normalement.
Avec `--capture-output prefix`, le moteur compose scène et UI dans une texture
au format et à l’extent exacts de la swapchain acquise, la copie vers celle-ci,
soumet puis relit cette texture par transfer buffer/fence. Le premier frame figé
stabilise l’UI ; le suivant fournit les pixels au même pas. Le sandbox écrit
`prefix.rgba` (RGBA8 sRGB) et `prefix.json`, puis ferme normalement. La relecture
BGRA est réordonnée en RGBA sans nouvel encodage colorimétrique.
La copie utilise `SDL_CopyGPUTextureToTexture` sur Vulkan/D3D12 et un blit SDL
1:1 nearest sur Metal : le drawable `framebufferOnly` conservé par SDL 3.4.18
interdit une copie Metal par blit-encoder. Le blit dessiné utilise les mêmes
formats sRGB source/destination ([restriction Apple](https://developer.apple.com/documentation/metal/mtltexture/isframebufferonly)).

`tools/presentation/capture.py` utilise cette option sur les trois OS, convertit
en PNG et vérifie les métadonnées, le pas exact et des témoins de texte UI.
Aucune lecture du bureau, injection système ou modification du focus/pointeur.
Les captures Windows redeviennent **requises** : recouvrements et décorations
n’influencent plus les pixels. La session interactive et une swapchain utilisable
restent nécessaires ; Linux utilise Xvfb et macOS reste manuel best effort.
Ces captures de frames composées ne remplacent pas les références analytiques.

## Mapping de la cible et routage UI

`Orbit`, `Reset`, `Interact` appartiennent à game ; axes `MoveX/MoveZ`,
`MouseYaw/MousePitch/WheelZoom`, `StickYaw/StickPitch/TriggerZoom`.
Les touches sont physiques : W/A/S/D correspondent aux positions Z/Q/S/D sur
AZERTY. Le stick gauche déplace ; E/bouton sud interagit, R/bouton nord réinitialise.
`MoveX/MoveZ` décrivent droite et avant/arrière dans le repère horizontal de la
caméra de suivi. Game actualise son yaw, tourne ces axes, résout les collisions,
puis replace la caméra autour du personnage ; aucun calcul de caméra dans runtime.

Runtime construit l’UI après le premier poll et applique sa demande de capture
avant de mettre en file les événements de cette frame. Le second poll, après
acquisition, est différé jusqu’à la décision UI suivante. Les événements déjà
en file gardent leur ordre. Les éditions TRS passent par une autre file, consommée
après `World::beginStep`, sans modifier World pendant son affichage.

Le scénario `tools/presentation/courtyard.json` choisit les pas de capture et la
région de comparaison hors panneaux : déplacement, mur, porte ouverte, inspecteur.
Il contrôle aussi les marqueurs du gameplay (personnage bloqué et état des deux
portes). Ces comparaisons de présentation ne remplacent pas les oracles D5/cube/scène.
