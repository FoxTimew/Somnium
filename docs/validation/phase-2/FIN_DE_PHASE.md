<!--
Bilan et pièces vérifiables de la phase 2 validée par le propriétaire.
Le lancement macOS après fusion est consigné séparément des preuves requises.
-->
# Phase 2 — assets, renderer et caméra orbitale

État au 25 novembre 2026 : **phase 2 validée par le propriétaire et clôturée**.
Après intégration de la [PR #5](https://github.com/FoxTimew/Somnium/pull/5), rebase
sur `main` et CI verte, la [PR de phase #4](https://github.com/FoxTimew/Somnium/pull/4)
est fusionnée sans fast-forward. Commit de fusion et tag annoté `phase-2` :
`bf0976aded532668946ad71b92847ff8ca7b3a2c`. La branche `phase/3-scene` part de ce
commit ; `main` est devenue la branche par défaut avec l'accord du propriétaire.

## Livraison

- **gpu** : indices uint16/32, draws indexés, bindings multiples, samplers décrits,
  textures sRGB et uniformes. Handles invalidés immédiatement et libération SDL
  différée pendant une frame ; pipelines paramétrés, profondeur et resize.
- **assets** : cgltf/stb_image, modèles et textures CPU, AssetManager avec identité
  logique, handles à génération et états ; hiérarchie glTF aplatie, une primitive
  par draw/matériau. Sources lues directement, fixtures et références sous LFS.
- **renderer** : ressources GPU depuis les données CPU, DrawList, caméra RH_ZO,
  lumière Lambert et ambiant. Présentation SDR_LINEAR ; cible hors écran UNORM
  avec encodage shader sRGB et oracle analytique commun.
- **platform/input** : flux neutre horodaté par pas ; input sans SDL, actions et
  axes normalisés, transitions, focus et déconnexion. Tests purement CPU.
- **runtime/game/sandbox** : Gameplay reçoit les inputs, demande le modèle via
  GameContext et fournit la caméra ; orbite/zoom bornés dans game. Runtime charge
  et produit la DrawList, sans contenu ni logique de caméra. Modèle original CC0,
  ambiant 0,15, overlay, VSYNC et profondeur adaptée au resize. Triangle supprimé.
- **outils/docs** : scripts local/CI identiques, caches sur disque, runners sous
  comptes personnels non élevés ; [guide de démarrage](../../DEMARRER_RUNNERS.md).
  Windows requis : fenêtres et rendu hors écran ; injection souris/clavier
  exclusivement dans le job manuel de captures. Linux capture sous Xvfb.

## Commits et validation

GPU `e5e765a`, assets `f6d71cf`, renderer `3edbfce`, input `adeeae5`, caméra
`98ce7bf`, sandbox `9dc0733`, documentation `806961c`. Infrastructure : `522c453`,
`27f4751`, correction sccache `7d5122d`, documentation `8c2f774`, puis fusion #5.
Les trois ajustements d'installation préexistants de `27f4751` sont conservés
avec l'accord explicite du propriétaire.

La [CI de #5](https://github.com/FoxTimew/Somnium/actions/runs/45618525005) est
entièrement verte sur `8c2f774`. Après fusion, le premier job manuel a refusé la
capture faute de focus ; le correctif de script `36f1824` conserve le code moteur.
Le [workflow manuel corrigé](https://github.com/FoxTimew/Somnium/actions/runs/45622048422)
est entièrement vert sur `36f18248b7ab05b3cec598a68c5b8ac4d33d6133` : les deux
pipelines et les captures D3D12 passent. [Métadonnées du run](self-hosted/workflow.json),
[résumé Windows](self-hosted/windows/SUMMARY.md), [résumé Linux](self-hosted/linux/SUMMARY.md).
Le commit documentaire de clôture `c4ede9c` passe la
[CI finale avant rebase](https://github.com/FoxTimew/Somnium/actions/runs/45624431353).
Les identifiants ci-dessus désignent l'historique avant rebase ; celui-ci conserve
exactement l'arbre de `c4ede9c` (`git diff` vide), à la tête publiée
`0d50ab75e77e94ae09418fc502b1ee202a2f1ade`.
La [CI après rebase](https://github.com/FoxTimew/Somnium/actions/runs/45627692899)
est verte sur Windows et Linux avant fusion ; la
[CI de main](https://github.com/FoxTimew/Somnium/actions/runs/45629637846)
est également verte sur les deux runners au commit de fusion.
Le tag `phase-2` est publié sur ce commit.

| Vérification requise | Windows / Radeon | Linux / WSL2 |
| --- | --- | --- |
| Debug, release, profiling | 18/18 par preset | 18/18 par preset |
| Shipping | 17/17 ; graphe sans UI | 17/17 ; graphe sans UI |
| Test de fenêtre | Inclus dans chaque preset | Inclus sous Xvfb |
| Format, architecture, contrat CI, cache d'en-têtes | Verts | Verts |
| clang-tidy 18 | — | Vert |
| ASan/UBSan avec détection des fuites | — | 17 tests + 1 fenêtre, verts |
| Contrôle d'application | Aucun blocage observé | Sans objet |

Les archives de chaque OS contiennent SUMMARY.md, run.json, les journaux
sélectionnés et un inventaire SHA-256. Sous Windows, run.json décrit la dernière
opération (captures) ; le résumé et le journal GitHub conservent aussi le pipeline.
Les PNG gardent leurs octets originaux ; seuls les fins de ligne des rapports
textuels sont normalisées en LF, conformément au dépôt, avant calcul de l'inventaire.
Les binaires restent dans les répertoires de run pendant 14 jours ; ils ne sont
pas versionnés. Caches CPM, shaders et sccache persistants, aucun artefact/cache GitHub.

## Preuves GPU et présentation

Les résultats ci-dessous viennent du workflow manuel vert après fusion.
Rapports, images et validation : [Windows](self-hosted/windows/) et [Linux](self-hosted/linux/).

| Exécution | D5 | Cube normal et miroir | Validation |
| --- | --- | --- | --- |
| Windows, D3D12 Radeon RX 6600M | 0 pixel différent ; 10/10 sondes exactes | 0 pixel différent ; 5/5 sondes par image | Debug D3D12 active |
| Windows, Vulkan Radeon RX 6600M | Identique | Identique | Khronos + synchronisation |
| WSL2, Vulkan/lavapipe | Identique | Identique | Khronos + synchronisation |

SDR_LINEAR est confirmé sur ces trois exécutions. Les PNG du cube conservent
le SHA-256 `dedd7662f51f2b6baeee2fa3f41c1d1e3a2cedfd6eb7d0c1a3b657a40be22af5`.
Référence, sondes, fixture et provenance restent inchangées ; ambiant zéro dans
le cube, variante à 0,15 uniquement dans le sandbox.

| Présentation | Modèle et overlay | Orbite | Zoom |
| --- | --- | --- | --- |
| D3D12, session Windows, job manuel | [Image](self-hosted/windows/presentation-direct3d12/model-overlay.png) | [Image](self-hosted/windows/presentation-direct3d12/orbit.png) | [Image](self-hosted/windows/presentation-direct3d12/zoom.png) |
| Vulkan, WSL2/Xvfb | [Image](self-hosted/linux/presentation-vulkan/model-overlay.png) | [Image](self-hosted/linux/presentation-vulkan/orbit.png) | [Image](self-hosted/linux/presentation-vulkan/zoom.png) |

Rapports, reset et resize : [Windows](self-hosted/windows/presentation-direct3d12/)
et [Linux](self-hosted/linux/presentation-vulkan/). Images originales sans retouche,
modèle/binaire identifiés par SHA. Reset exact hors overlay ; resize 960×600.
Sur chaque backend : 38 878 pixels de la région contrôlée changent après orbite,
94 035 après zoom ; zéro après reset par rapport à l'état initial.

## Incidents résolus et limites

Le [premier job manuel](https://github.com/FoxTimew/Somnium/actions/runs/45620375402)
a échoué avant injection : « Sandbox is not foreground (Win32 0) » ([rapport](self-hosted/failed-focus/result.json)).
Depuis la tâche planifiée, SetForegroundWindow n'avait pas activé la fenêtre.
Le script vérifie maintenant la fenêtre sous le point d'activation avant un clic
client réel, puis contrôle le focus. Trois tests sans fenêtre vérifient le chemin
normal, le repli et le refus de cliquer dans une fenêtre couvrante. Aucun changement
de politique Windows ; les captures restent exclusivement manuelles.
Le passage vert a obtenu le focus par SetForegroundWindow ; le repli est couvert
par les tests, sans prétendre qu'il a été emprunté pendant cette capture.

Le timeout shader initial de 120 s concernait l'environnement Docker avec lanceur
CMake Python et fichiers sur montage NTFS. Une relance avec CMake natif et dossier
Linux /tmp passait en 32 s ; la part de chaque facteur n'a pas été isolée séparément.
La correction utilise CMake/Ninja natifs, Python explicite et un checkout/cache WSL
sur ext4. Le contrat complet est désormais obligatoire dans tous les presets et
ASan ; **la limite de 120 s n'a pas été relevée et aucun test n'est exclu**.
Mesures du run archivé : **7,45–10,42 s sous Windows**, **3,20–4,11 s sous Linux**,
ASan compris. Les logs CTest complets sont archivés avec chaque OS.
Les [journaux initiaux](local-checks/) restent disponibles, sans transformer le
premier passage en succès.

Le cache MSVC ancien pouvait restituer des diagnostics français altérés par
sccache, faisant perdre les dépendances d'en-têtes à Ninja. Ressources anglaises,
VSLANG=1033 dans la clé du cache et vérification après chaque build : 43 objets
Somnium avec dépendances, 37 en shipping. L'ancien blocage WinError 4551 ne se
reproduit pas avec les outils et binaires MSVC actuels ; aucune politique de
contrôle d'application n'a été assouplie.

WARP n'est plus couvert. macOS reste manuel best effort ; le lancement sur main
décrit ci-dessous n'a pas atteint le moteur ni Metal. Les [preuves du jalon](README.md)
restent historiques. Metal dépend de macos-15, cible 13.3 ; horizon de retrait et
condition de migration dans [PHASE_1_CI.md](../../PHASE_1_CI.md). Les comptes
personnels ont accès aux fichiers utilisateur, dont les disques Windows depuis
WSL ; limites et conditions de compte dédié dans l'[ADR-0006](../../adr/0006-ci-auto-hebergee.md).

## Lancement macOS sur main — non bloquant

Un seul lancement manuel, demandé pendant que le dépôt est public :
[run 45630158935](https://github.com/FoxTimew/Somnium/actions/runs/45630158935),
sur `bf0976aded532668946ad71b92847ff8ca7b3a2c`, **échec**.
Le provisionnement, les versions, le format et le scanner d'architecture passent.
Les deux jobs s'arrêtent ensuite au contrat CI, avant la chaîne de shaders,
la compilation moteur et le probe GPU. **Metal : non validé** sur ce passage ;
il ne démontre ni une absence de device ni un défaut de rendu.

| Job | Environnement observé | Résultat |
| --- | --- | --- |
| macos-latest | macOS 26.7.2 arm64, Xcode 26.6 | Échec du test de rétention, 1/7 tests en échec |
| macos-15 | macOS 15.8.2 arm64, Xcode 16.4 | Même échec ; aucun test Metal exécuté |

Diagnostic exact : `test_retention_preserves_current_unowned_recent_and_cache`,
`tools/ci/test_ci.py:82`, `AssertionError: False is not true` ; puis
`RuntimeError: ci-contract failed (1)`. Métadonnées et extraits de journaux :
[résultat](macos-manual-2026-11-25/result.json),
[macos-latest](macos-manual-2026-11-25/macos-latest.txt),
[macos-15](macos-manual-2026-11-25/macos-15.txt).

Cause probable à la lecture de `clean_runs` : comparaison du chemin résolu au
chemin `current` non résolu du test, sensible aux alias des répertoires temporaires
macOS. Cette hypothèse n'a pas été reproduite séparément. Avant un prochain passage,
normaliser les deux chemins et couvrir ce cas ; aucune correction ni relance dans
ce jalon non bloquant. Aucune nouvelle image Metal n'est disponible.

## Phase suivante — jalon documentaire en attente

[ADR-0007](../../adr/0007-scene-entites-serialisation.md) : scène/EnTT, hiérarchie
et JSON versionné extensible par game, extraction et interpolation.
[ADR-0008](../../adr/0008-inputs-configurables.md) : mapping générique, IDs
compilés, transitions et interaction UI. Les deux restent proposés ; leur
validation précède tout code de scène et toute modification de input.
Le cuiseur, KTX2/BC/mipmaps, chargement asynchrone et .metallib shipping macOS
restent hors phase 2. L'implémentation de phase 3 n'est pas commencée.
