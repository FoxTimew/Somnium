<!-- Archive les pièces du point de contrôle, sans déclarer la phase terminée.
Les images sont les captures des jobs, vérifiées contre leurs empreintes publiées. -->
# Phase 3 — preuves du point de contrôle scène

Code : `02340d9a7c1f876365fcb0f59640a077840c0713`, branche `phase/3-scene`,
[PR #6](https://github.com/FoxTimew/Somnium/pull/6). Aucun tag de phase 3 ni fusion
dans main. Le point de contrôle attend la validation avant input configurable,
gameplay cible et inspecteur.

[CI requise verte](https://github.com/FoxTimew/Somnium/actions/runs/45682666750),
25 novembre 2026. GitHub a testé la fusion provisoire de PR
`b68d5aca593cd77bb067b6e9a1b25df0bbf39397` (head `02340d9`, base `bf0976a`),
arbre propre. Les `run.json` et rapports conservent cette provenance.

| Exécution | Trois poses de scène | Sondes exactes | Validation native |
| --- | --- | --- | --- |
| [Windows D3D12 / Radeon RX 6600M](windows/scene-direct3d12/result.json) | 0 pixel hors tolérance | toutes | active |
| [Windows Vulkan / Radeon RX 6600M](windows/scene-vulkan/result.json) | 0 pixel hors tolérance | toutes | Vulkan + synchronisation |
| [Linux WSL2 / lavapipe](linux/scene-vulkan/result.json) | 0 pixel hors tolérance | toutes | Vulkan + synchronisation |
| [macos-15 / Metal Apple Paravirtual](macos/scene-metal/result.json) | 0 pixel hors tolérance | toutes | API et shader |

Chaque dossier contient les poses précédente, interpolée et courante, le rapport,
le device, le journal natif, les entrées JSON et la provenance de référence.
Les fins de ligne des journaux sont normalisées en LF ; leur texte est conservé.
Les SHA-256 des PNG archivés ont été comparés aux rapports avant copie. Les
**douze images sont identiques à l’oracle** (erreur maximale par canal : zéro).
D5 et cube sont également verts dans cette exécution ; leurs références n’ont
pas changé. WARP reste hors couverture.

| Contrôle | Windows | Linux |
| --- | --- | --- |
| debug / release / profiling | 23/23 chacun | 23/23 chacun |
| shipping, package et absence ImGui | 22/22 | 22/22 |
| clang-tidy 18 | — | vert |
| ASan/UBSan, fuites actives sans suppression | — | 22 tests CPU/outils + 1 fenêtre, verts |

Journaux : [Windows](windows/logs), [Linux](linux/logs).
Les captures de présentation Windows n’ont pas été déclenchées : aucune injection
de souris/clavier pour ce point de contrôle. Linux a exercé le sandbox sous Xvfb.
Le contrôle GPU de scène est distinct du test de fenêtre.

Les commits précédents sont également verts :
[rétention `7720a75`](https://github.com/FoxTimew/Somnium/actions/runs/45672432952),
[World/codecs `b08c7a8`](https://github.com/FoxTimew/Somnium/actions/runs/45678319476).
La rétention couvre un vrai alias temporaire Unix et les chemins normalisés.

## Captures de scène

| Backend | Alpha 0 | Alpha 0,5 | Alpha 1 |
| --- | --- | --- | --- |
| D3D12 / Radeon | ![précédent](windows/scene-direct3d12/previous.png) | ![interpolé](windows/scene-direct3d12/actual.png) | ![courant](windows/scene-direct3d12/current.png) |
| Vulkan / Radeon | ![précédent](windows/scene-vulkan/previous.png) | ![interpolé](windows/scene-vulkan/actual.png) | ![courant](windows/scene-vulkan/current.png) |
| Vulkan / lavapipe | ![précédent](linux/scene-vulkan/previous.png) | ![interpolé](linux/scene-vulkan/actual.png) | ![courant](linux/scene-vulkan/current.png) |
| Metal / CI macos-15 | ![précédent](macos/scene-metal/previous.png) | ![interpolé](macos/scene-metal/actual.png) | ![courant](macos/scene-metal/current.png) |

Protocole et régénération explicite : [SCENE_VALIDATION.md](../../SCENE_VALIDATION.md).
Les écarts d’arrondi sRGB constatés pendant la préparation ont conduit à une
fixture de hiérarchie distincte à couleurs 0/255 ; aucun assouplissement des sondes,
des seuils ou de la validation, et aucun remplacement des références par une capture.

## Relance macOS

[Workflow manuel](https://github.com/FoxTimew/Somnium/actions/runs/45682684508)
lancé sur `02340d9` : macos-latest construit ; macos-15 construit et probe Metal.
Le job **macos-15 échoue après la preuve GPU**, dans la capture de présentation :
`Orbit/zoom/reset must produce three camera poses and restore the initial pose`.
Une seule pose de caméra est observée ; les commandes d’injection retournent zéro.
La cause de cette absence de mouvement n’est pas établie (le helper signale aussi
la dépréciation d’`activateIgnoringOtherApps`). Ce résultat est non bloquant ; il
ne valide pas l’orbite/zoom/reset Metal. Resize/minimisation/restauration ont été
observés, mais le scénario de présentation complet reste en échec.

En revanche : [rétention 9/9](macos/retention.log), [debug 23/23](macos/test-debug.log),
D5, cube, scène et composition SDR_LINEAR **réussis**. La scène a été rendue avec
les validations Metal API **et** shader actives. Les trois PNG ci-dessus proviennent
bien du runner CI, récupérés dans le journal et vérifiés par SHA-256. Les presets
suivants du job macos-15 ne sont pas exécutés après l’échec de présentation ;
macos-latest construit indépendamment et n’apporte aucune preuve Metal.
[Diagnostic de présentation](macos/presentation-failure.log).
