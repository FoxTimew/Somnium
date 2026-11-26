<!-- Bilan de phase 3 ; les liens de runs et empreintes identifient les preuves exécutées.
La phase reste sur sa branche en attente de validation, sans fusion ni tag. -->
# Phase 3 — scène, inputs et boucle complète

État : phase livrée, CI requise et macOS manuel verts ; validation de fin de phase
attendue. Branche `phase/3-scene`,
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
  pivot, caméra racine de suivi avec orbite/zoom bornés. Déplacement dans son repère
  horizontal, orbite/reset pris en compte dans le même pas. Mapping clavier/souris/manette.
- **debug_ui** : hiérarchie, sélection par ID, édition des TRS au prochain pas via
  World, actions/axes/transitions en lecture seule. Vérification d’absence en shipping.
- **gpu/outils** : capture composée scène + UI dans une texture au format/extent
  de la swapchain, copie pour présentation normale puis relecture RGBA8 par fence
  et transfer buffer ; PNG, métadonnées, témoins UI et scénario versionné communs
  aux trois OS. Aucune lecture du bureau ou injection système. Rétention temporaire
  normalisée, documentation et contrats alignés sur le code.

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
  Le yaw tourne `MoveX/MoveZ` avant le balayage AABB ; pitch et zoom ne changent
  ni vitesse ni hauteur. La caméra est replacée après résolution du déplacement.
- Le rejeu neutralise le gameplay natif et l’interaction UI, conserve le cycle de
  fenêtre et fige un nombre exact de pas. Une frame stabilise l’UI, la suivante
  capture scène + UI au même pas. Windows redevient requis : recouvrements, focus
  et décorations ne déterminent plus les pixels. Une swapchain utilisable reste
  nécessaire, donc session interactive Windows ; Linux utilise Xvfb.
- L’inspecteur groupe les éditions par ID, les consomme une fois au prochain pas
  par teleport et affiche les refus de World ; il ne modifie pas une scène en parcours.
- Après les captures Windows réussies sur bureau dégagé, Metal a reproduit
  `Capture dimensions differ` au run 45789402385. Le repli autorisé remplace donc
  entièrement les captures du bureau. Les anciens outils Windows/X11/macOS sont
  supprimés ; aucune image masquée n’est publiée, aucune image n’est redimensionnée
  pour masquer l’erreur. Sur Metal, SDL conserve le drawable framebufferOnly : la
  copie finale utilise un blit SDL 1:1 nearest ; Vulkan/D3D12 utilisent la copie
  texture native. Le fichier relit uniquement la cible possédée, jamais la swapchain.

## 3. Écarts et limites

WARP reste hors couverture ; Windows valide la Radeon. Linux valide X11/Xvfb et
lavapipe, pas Wayland. Les captures sont des frames composées par le moteur et
présentées normalement, distinctes des références analytiques GPU ; elles ne
prouvent pas la composition du bureau par l’OS. Metal demeure best effort sur macos-15,
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
| Contrats et preuves disponibles | `9e18f45` | [vert](https://github.com/FoxTimew/Somnium/actions/runs/45711948936) |
| Déplacement relatif à la caméra | `38bb0c3` | [vert](https://github.com/FoxTimew/Somnium/actions/runs/45789351744) |
| Capture moteur et garde clang-tidy | `dd376fa` → `a7c8ed2` | [vert](https://github.com/FoxTimew/Somnium/actions/runs/45795328234) |

Validation locale finale : Windows/Linux debug 26/26 ; Windows shipping 24/24,
graphe sans UI. Gameplay : neuf cas CPU, 2 639 assertions, sans fenêtre/GPU, sur
fixture JSON et inputs synthétiques ; inspecteur : trois cas CPU, 26 assertions.
Déplacement testé à 0°, ±90°, 180° et au yaw initial ; avant/droite, diagonale,
pitch/zoom, orbite/reset dans le même pas, mur/glissement et grande vitesse avec
caméra tournée. Le rejeu réduit la translation latérale avant Interact pour
retrouver l’ouverture depuis cette nouvelle trajectoire et contrôle le passage.
Les tests GPU vérifient aussi RGBA/BGRA, UNORM/sRGB, relecture avec pitch aligné,
refus des mauvaises dimensions/formats, copie vers swapchain et retrait immédiat
du handle sélectionné avec libération différée. Les tests CPU du transport refusent
métadonnées incohérentes, mauvais pas, pixels incomplets et absence de témoins UI.
Clang-tidy 18 sans diagnostic projet sur les sources modifiées. Le rejeu Linux
montre déplacement, arrêt au mur, ouverture puis passage, et inspecteur.
La CI teste debug/release/profiling (26/26 chacun), shipping (24/24 et graphe sans
inspecteur), fenêtre, architecture et contrats de capture sur Windows/Linux ;
clang-tidy 18 et ASan/UBSan sur Linux (25 tests sans fenêtre puis le test fenêtré,
détection des fuites active). Les rapports identifient le SHA effectivement
exécuté, y compris le commit de vérification généré par GitHub pour une PR.

| Backend / machine | D5, cube, scène | Présentation finale |
| --- | --- | --- |
| D3D12 / Radeon RX 6600M Windows | Exacts, sondes exactes, validation active | Cinq captures moteur 1280×720 validées en CI requise |
| Vulkan / Radeon RX 6600M Windows | Exacts, sondes exactes, validation active | Cinq captures moteur 1280×720 validées en CI requise |
| Vulkan / lavapipe WSL2 | Exacts, sondes exactes, validation active | Cinq captures moteur validées sous Xvfb en CI requise |
| Metal / macos-15 | Exacts, sondes exactes, validations API + shader actives | Cinq captures moteur 1024×653 validées, workflow manuel |

« Exacts » signifie zéro pixel différent et erreur maximale par canal nulle ;
pour la scène, les trois poses précédent/interpolé/courant sont vérifiées.
Les [pièces Windows hors écran](final/windows/) et les [pièces Linux](final/linux/)
comprennent rapports, empreintes, images et journaux. La validation de présentation
Windows a été reprise le 26 novembre sur bureau dégagé, puis régénérée après
le changement de déplacement. Les pièces finales ci-dessous utilisent la nouvelle
capture moteur, avec témoins UI, marqueurs de pas/position/portes et validation native.
Les anciennes images masquées restent rejetées, sans publication.
Le premier lancement du commit de capture a rencontré le verrou d’une analyse
locale Linux encore active ; cette concurrence a été supprimée. Le correctif
`a7c8ed2` rend explicite la garde de l’optionnel pour clang-tidy 18, sans changement
de comportement. La CI complète de ce commit est verte.

| Pose | D3D12 / Radeon | Vulkan / Radeon |
| --- | --- | --- |
| Déplacement | ![Déplacement D3D12](final/windows/presentation-direct3d12/movement.png) | ![Déplacement Vulkan](final/windows/presentation-vulkan/movement.png) |
| Mur | ![Mur D3D12](final/windows/presentation-direct3d12/wall.png) | ![Mur Vulkan](final/windows/presentation-vulkan/wall.png) |
| Porte ouverte | ![Porte D3D12](final/windows/presentation-direct3d12/door-open.png) | ![Porte Vulkan](final/windows/presentation-vulkan/door-open.png) |
| Inspecteur et passage | ![Inspecteur D3D12](final/windows/presentation-direct3d12/inspector.png) | ![Inspecteur Vulkan](final/windows/presentation-vulkan/inspector.png) |

Présentation Vulkan sous Xvfb, captures du runner Linux (pas des captures Radeon) :

| Déplacement | Mur bloquant |
| --- | --- |
| ![Déplacement Linux](final/linux/presentation-vulkan/movement.png) | ![Mur Linux](final/linux/presentation-vulkan/wall.png) |
| Porte ouverte | Inspecteur et passage |
| ![Porte Linux](final/linux/presentation-vulkan/door-open.png) | ![Inspecteur Linux](final/linux/presentation-vulkan/inspector.png) |

Relance macOS avec capture moteur : [workflow manuel](https://github.com/FoxTimew/Somnium/actions/runs/45794898319),
commit `dd376fa`, **vert, non bloquant** :

- **macos-latest : validé**, debug/release/profiling 26/26, shipping 24/24 et graphe
  sans UI. [Journaux](final/macos-latest/), compilation sans preuve Metal complète.
- **macos-15 : validé**, debug/release/profiling 26/26, shipping 24/24, D5/cube/scène
  exacts, validations API et shader actives, SDR_LINEAR et copie/relecture validés.
  [Rapports, journaux et trois PNG de scène](final/macos/).
- **Capture Metal : validée**, cinq poses à 1024×653, dimensions de la swapchain
  effectivement acquise sur le runner. Pas, marqueurs gameplay et témoins UI
  vérifiés ; les images ne sont ni recadrées ni redimensionnées. Elles sont
  décodées depuis le journal du job puis vérifiées par SHA-256. [Rapport](final/macos/presentation-metal/result.json).

Ces images sont des preuves **CI macOS**, acceptées comme substitut local ;
elles ne sont pas présentées comme captures d’un Mac personnel. Le commit macOS
précède seulement la garde explicite pour clang-tidy, sans différence de comportement.
Les échecs historiques de lecture du bureau (dont [45789402385](https://github.com/FoxTimew/Somnium/actions/runs/45789402385)
sur `38bb0c3`) sont remplacés par ce chemin moteur commun ; aucun nouvel essai
de capture du bureau n’a été effectué après cette récidive.

| Déplacement Metal / CI | Mur Metal / CI |
| --- | --- |
| ![Déplacement Metal](final/macos/presentation-metal/movement.png) | ![Mur Metal](final/macos/presentation-metal/wall.png) |
| Porte ouverte | Inspecteur et passage |
| ![Porte Metal](final/macos/presentation-metal/door-open.png) | ![Inspecteur Metal](final/macos/presentation-metal/inspector.png) |

## 5. Questions pour la phase 4

Le prochain jalon devra fixer le format et les versions des assets cuits, leurs
dépendances/invalidation et l’assemblage de la racine shipping ; préciser la
production `.metallib` pour macOS et la stratégie KTX2/BC/mipmaps. Les contextes
d’input, le hot reload et l’éditeur de scène restent des reports distincts.
Aucun code de phase 4 n’est commencé.
