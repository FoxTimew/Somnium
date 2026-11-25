<!-- Bilan de phase 3 ; les liens de runs et empreintes identifient les preuves exécutées.
La phase reste sur sa branche en attente de validation, sans fusion ni tag. -->
# Phase 3 — scène, inputs et boucle complète

État : code implémenté ; clôture en attente des captures Windows à reprendre sur
un bureau dégagé. Branche `phase/3-scene`,
[PR #6](https://github.com/FoxTimew/Somnium/pull/6). Aucun tag `phase-3`, aucune fusion
vers `main`. Les preuves antérieures restent dans le [point de contrôle](README.md).

## 1. Livraison

- **scene** : World/EnTT direct, identité persistante et recherche, hiérarchie
  intrusive, cycles rejetés, opérations structurelles gardées, propagation,
  historique et téléportation ; JSON versionné, codecs extensibles par game,
  références en seconde passe et transaction des demandes d’assets.
- **runtime/renderer** : extraction des TRS locaux interpolés avant recomposition,
  caméra/lumière de scène, DrawList indépendante de World, ressources GPU préparées
  par handle CPU. Aucun contenu ou comportement de caméra dans runtime.
- **platform/input** : clavier complet, mapping JSON compilé en IDs, actions/axes
  génériques, transitions ordonnées, capture UI et rejeu neutre horodaté par pas.
  Aucun SDL dans input, aucun nom résolu pendant le pas fixe.
- **game/sandbox** : scène originale CC0, personnage à 3 m/s, murs AABB avec
  glissement et balayage sans traversée à grande vitesse, deux portes Interact sur
  pivot, caméra racine de suivi avec orbite/zoom bornés. Mapping clavier/souris/manette.
- **debug_ui** : hiérarchie, sélection par ID, édition des TRS au prochain pas via
  World, actions/axes/transitions en lecture seule. Vérification d’absence en shipping.
- **outils/docs** : captures par rejeu commun, aucune injection système ; rétention
  temporaire normalisée, documentation et contrats alignés sur le code.

## 2. Décisions et raisons

- L’interface publique des codecs expose délibérément `nlohmann::json`. Les codecs
  du jeu sont des sérialiseurs et utilisent les lecteurs vérifiés et le résolveur
  commun ; un format binaire peut repartir de la même valeur (ADR-0001/0007).
- Une porte garde son volume fermé pendant toute l’ouverture, puis désactive son
  collider à 90°. Le battant ouvert est décoratif ; une AABB ne suit pas sa rotation.
  Balayage X puis Z pour le personnage ; sortie horizontale minimale si l’inspecteur
  le téléporte dans un volume. Aucun module physique dans le moteur.
- La caméra appartient au jeu, est racine et passe par l’historique de World.
  Pitch 5–75°, distance 4–30 m ; pas de traitement particulier dans l’extraction.
- Le rejeu neutralise le gameplay natif et l’interaction UI, conserve le cycle de
  fenêtre, puis fige un nombre exact de pas pour lire les pixels présentés.
  Windows reste manuel : les fenêtres visibles peuvent gêner ou être masquées dans
  le compte personnel, même sans injection. Test de fenêtre et preuves GPU restent requis.
- L’inspecteur groupe les éditions par ID, les consomme une fois au prochain pas
  par teleport et affiche les refus de World ; il ne modifie pas une scène en parcours.
- La revue visuelle a rejeté une première série Windows masquée par une fenêtre
  flottante malgré le premier plan. Ces images ne sont pas publiées. Le contrôle
  vérifie maintenant les fenêtres au-dessus, avant et après lecture des pixels,
  avec régression CPU sur ce cas ; aucune injection ou reprise de focus.

## 3. Écarts et limites

WARP reste hors couverture ; Windows valide la Radeon. Linux valide X11/Xvfb et
lavapipe, pas Wayland. Les captures sont des fenêtres réellement présentées,
distinctes des références analytiques GPU. Metal demeure best effort sur macos-15,
cible moteur 13.3 ; macos-latest construit sans prétendre prouver Metal.
La validation shader Metal dépend encore de macos-15 (horizon annoncé automne 2027,
pas de jour arm64 confirmé ; [plan CI](../../PHASE_1_CI.md)).

Pas de collision caméra, gravité, fermeture de porte, animation de personnage,
sauvegarde depuis l’inspecteur, hot reload de mapping ou contexte d’input.
Les liaisons manette sont vérifiées par événements/états synthétiques, sans essai
manuel d’une manette physique dans cette validation.
Ces limites sont explicites ; les composants et codecs restent extensibles par game.
Les modèles sont originaux CC0, [licences](../../../assets/LICENSES.md).
D5, cube et scène gardent leurs références, sondes et provenance inchangées.

## 4. Validation et pièces

| Étape | Commit | Résultat requis |
| --- | --- | --- |
| Rétention | `7720a75` | [vert](https://github.com/FoxTimew/Somnium/actions/runs/45672432952) |
| World/codecs | `b08c7a8` | [vert](https://github.com/FoxTimew/Somnium/actions/runs/45678319476) |
| Extraction/scène hors écran | `02340d9` | [vert](https://github.com/FoxTimew/Somnium/actions/runs/45682666750) |
| Input configurable/rejeu | `8020c78` | [vert](https://github.com/FoxTimew/Somnium/actions/runs/45696957925) |
| Gameplay | `05a469b` | [vert](https://github.com/FoxTimew/Somnium/actions/runs/45701699366) |
| Inspecteur | `f54d0e5` | [vert](https://github.com/FoxTimew/Somnium/actions/runs/45704526812) |
| Refus des captures masquées | `6853c41` | [vert](https://github.com/FoxTimew/Somnium/actions/runs/45709436402) |

Validation locale finale : Windows/Linux debug 26/26 ; Windows shipping 24/24,
graphe sans UI. Gameplay : huit cas CPU, 904 assertions, sans fenêtre/GPU, sur
fixture JSON et inputs synthétiques ; inspecteur : trois cas CPU, 26 assertions.
Clang-tidy 18 sans diagnostic projet sur les sources modifiées. Le rejeu Linux
montre déplacement, arrêt au mur, ouverture puis passage, et inspecteur.
La CI teste debug/release/profiling (26/26 chacun), shipping (24/24 et graphe sans
inspecteur), fenêtre, architecture et contrats de capture sur Windows/Linux ;
clang-tidy 18 et ASan/UBSan sur Linux. Les rapports identifient le SHA effectivement
exécuté, y compris le commit de vérification généré par GitHub pour une PR.

| Backend / machine | D5, cube, scène | Présentation finale |
| --- | --- | --- |
| D3D12 / Radeon RX 6600M Windows | Exacts, sondes exactes, validation active | À reprendre ; première série masquée, non publiée |
| Vulkan / Radeon RX 6600M Windows | Exacts, sondes exactes, validation active | À reprendre ; première série masquée, non publiée |
| Vulkan / lavapipe WSL2 | Exacts, sondes exactes, validation active | Rejeu validé sous Xvfb |
| Metal / macos-15 | Exacts, sondes exactes, validations API + shader actives | Échec de comparaison des dimensions, ci-dessous |

« Exacts » signifie zéro pixel différent et erreur maximale par canal nulle ;
pour la scène, les trois poses précédent/interpolé/courant sont vérifiées.
Les [pièces Windows hors écran](final/windows/) et les [pièces Linux](final/linux/)
comprennent rapports, empreintes, images et journaux. La validation de présentation
Windows reste ouverte : un job vert avant le contrôle d’occlusion ne remplace pas
la revue visuelle, et ses images ont été rejetées. La reprise attend un bureau dégagé.

Présentation Vulkan sous Xvfb, captures du runner Linux (pas des captures Radeon) :

| Déplacement | Mur bloquant |
| --- | --- |
| ![Déplacement Linux](final/linux/presentation-vulkan/movement.png) | ![Mur Linux](final/linux/presentation-vulkan/wall.png) |
| Porte ouverte | Inspecteur et passage |
| ![Porte Linux](final/linux/presentation-vulkan/door-open.png) | ![Inspecteur Linux](final/linux/presentation-vulkan/inspector.png) |

Relance macOS avec rejeu : [workflow manuel](https://github.com/FoxTimew/Somnium/actions/runs/45704526805),
commit `f54d0e5`, **échec global non bloquant** :

- **macos-latest : validé**, debug/release/profiling 26/26, shipping 24/24 et graphe
  sans UI. [Journaux](final/macos-latest/), compilation sans preuve Metal complète.
- **macos-15 : GPU validé**, debug 26/26, D5/cube/scène exacts, validations API et
  shader actives, composition SDR_LINEAR validée. [Rapports et trois PNG de scène](final/macos/).
- **Présentation Metal : échec**, message exact `Capture dimensions differ`.
  Les cinq poses ont atteint leur pas demandé, terminé normalement et satisfait
  les contrôles de validation native et les marqueurs du gameplay. La comparaison
  inter-images échoue ensuite sur leurs dimensions ; leur cause précise reste
  non établie. [Rapport](final/macos/presentation-result.json),
  [journal](final/macos/presentation-failure.log). Les PNG de présentation ne sont
  pas récupérés : la publication dans le journal était après l’étape en échec.
  Ils ne sont donc pas joints ni déclarés validés. Les presets suivants de ce job
  ne tournent pas ; ils sont couverts indépendamment par macos-latest.

L’échec d’injection du point de contrôle est remplacé par un rejeu effectivement
consommé, sans injection système. Le contrôle de comparaison reste strict ; aucune
image redimensionnée ou référence remplacée pour rendre le job vert. Une relance
optionnelle devra d’abord journaliser les dimensions et conserver les images même
en échec afin d’établir la cause, puis stabiliser la surface capturée.

## 5. Questions pour la phase 4

Le prochain jalon devra fixer le format et les versions des assets cuits, leurs
dépendances/invalidation et l’assemblage de la racine shipping ; préciser la
production `.metallib` pour macOS et la stratégie KTX2/BC/mipmaps. Les contextes
d’input, le hot reload et l’éditeur de scène restent des reports distincts.
Aucun code de phase 4 n’est commencé.
