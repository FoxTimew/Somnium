<!-- Contrat de phase 2 : événements neutres et mapping CPU à pas fixe. -->
# Entrées à pas fixe

`platform::pollEvents(nextStep)` collecte SDL sur le thread de fenêtre, appelle
l'adaptateur privé ImGui et expose une vue de `core::InputEvent` valable jusqu'au
prochain poll. `step` est l'indice du prochain pas de simulation, fourni par runtime,
pas une estimation depuis l'horloge du périphérique. Touches physiques Somnium,
boutons, déplacements relatifs en pixels, molette en unités de défilement et axes
manette signés bruts restent séparés. Le sens de molette inversé du système est normalisé.

`input::Input::enqueue` conserve les événements en ordre ; `advance(step)` les
consomme une seule fois, même si plusieurs frames précèdent le prochain pas.
Les événements futurs attendent. Un lot invalide est rejeté entièrement.
Un appui et un relâchement dans le même pas donnent `pressed=true`,
`released=true`, `held=false`. Les transitions et impulsions sont effacées au pas
suivant ; les boutons et sticks restent maintenus. Une perte de focus libère les
actions et annule les axes ; une déconnexion ne libère que la manette concernée.

| Action/axe en code | Sources | Normalisation |
| --- | --- | --- |
| `Orbit` | Bouton droit de souris | pressed / released / held |
| `Reset` | Touche physique R, bouton sud de manette | OU des sources maintenues |
| `MouseYaw`, `MousePitch` | Mouvement pendant Orbit | 100 pixels = 1, borné à ±1 par pas |
| `WheelZoom` | Molette verticale | 5 unités = 1, borné à ±1 par pas |
| `StickYaw`, `StickPitch` | Stick droit | [-32768,32767] → [-1,1], zone morte 15 %, reste remis à l'échelle |
| `TriggerZoom` | Gâchette droite moins gauche | [0,32767] → [0,1] par gâchette |

Quatre manettes simultanées ; platform journalise une connexion excédentaire.
Leurs axes sont additionnés puis bornés. La file réserve 128 événements et peut
grandir lors d'une rafale ; les états et les quatre slots n'allouent pas au pas fixe.
Le mapping JSON configurable est reporté à la phase 3. `input` ne dépend que de
`core` ; aucun include SDL ni type natif, vérifié par le scanner et ses tests négatifs.

La caméra orbitale est dans `game::OrbitCamera`. `Gameplay::fixedUpdate` emprunte
un `input::State` portant le même indice de pas ; `Gameplay::camera()` fournit une
`renderer::Camera` par valeur. `GameContext::requestModel` enregistre un chemin
logique sans I/O. Runtime réalise ensuite le chargement et le rendu.

Souris : 0,5 radian pour 100 pixels ; stick : 1,5 radian/seconde. Le yaw est ramené
modulo 2π, le pitch borné à ±80°, la distance à [4,30] m. La molette multiplie
la distance par `exp(-unités/5)` ; les gâchettes appliquent ce zoom en continu avec
le temps du pas. R/bouton sud réinitialise yaw 0,65 rad, pitch 0,45 rad, distance
10 m. La cible est l'origine, up = +Y, FOV vertical π/3, near 0,1 et far 100 m.
La pose est reconstruite en coordonnées sphériques ; elle n'atteint jamais les
pôles singuliers de `lookAtRH`. L'aspect reste calculé par renderer depuis l'extent.
