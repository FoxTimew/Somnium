<!--
Décrit le moteur implémenté en phase 2 et les commandes de vérification reproductibles.
Les résultats de rendu et les limites de l'environnement restent explicitement identifiés.
-->
# Somnium

Moteur de jeu 3D C++20 pour Windows, Linux et macOS. `main` charge un modèle glTF
texturé et le présente en VSYNC avec profondeur, caméra orbitale et overlay ImGui.
Les phases 0 à 2 sont validées, fusionnées et taguées ; `main` est la branche
par défaut. `phase/3-scene` prépare le jalon documentaire scène et inputs.

État au 25 novembre : phase 2 clôturée, tag `phase-2` sur `bf0976a`.
Windows/Radeon et Linux/WSL2 sont requis ; macOS est manuel
best effort et WARP n'est plus couvert. Installation et commandes dans
[CI_RUNNER.md](docs/CI_RUNNER.md). Captures, journaux, CI et limites dans le
[bilan de phase 2](docs/validation/phase-2/FIN_DE_PHASE.md).
Le lancement macOS sur `main` a échoué au test de rétention, avant compilation :
aucune nouvelle preuve Metal à cette clôture. Le défaut de rétention est corrigé
en phase 3, avec régression reproduite puis couverte sous Windows/Linux.
Les ADR de phase 3 sont **acceptés** ; [World et les codecs JSON](docs/SCENE.md)
ont leurs tests CPU. Extraction et preuve GPU précèdent le prochain point de
contrôle ; input configurable et le gameplay de la cible viennent ensuite.

- [Architecture](docs/ARCHITECTURE.md) et [conventions](docs/adr/0002-conventions.md)
- [Dépendances et licences](docs/adr/0001-stack-dependances-licences.md)
- [Chaîne de shaders](docs/adr/0003-chaine-shaders.md), [frame GPU](docs/adr/0004-frame-synchronisation-gpu.md)
- [Plan et résultats CI](docs/PHASE_1_CI.md), [protocole D5](docs/GPU_VALIDATION.md)
- [Pipeline d’assets](docs/adr/0005-pipeline-assets.md), [contrôles](docs/INPUT.md) et [preuves de phase 2](docs/validation/phase-2/README.md)
- Contrats de phase 3 : [scène et sérialisation](docs/adr/0007-scene-entites-serialisation.md), [inputs configurables](docs/adr/0008-inputs-configurables.md)

## Prérequis et bootstrap

Git avec Git LFS (`git lfs install`), CMake ≥ 3.28, Ninja et **Python ≥ 3.12,
requis dès le configure**. Compilateur C++20 avec `std::format` : MSVC récent et
Windows SDK depuis un terminal développeur, LLVM-MinGW UCRT, GCC 13/Clang récent
sous Linux, ou Xcode récent sous macOS. Ajouter les outils au `PATH`.
Les paquets de développement SDL/X11/Wayland/audio/udev nécessaires sous Linux
figurent dans [ci.yml](.github/workflows/ci.yml) ; `xvfb` permet les tests sans écran.

Construire d'abord les outils de shaders :

```sh
python -m pip install cmake==3.31.6 ninja==1.11.1.3
python tools/shader_toolchain/build.py --provider official --jobs 3
```

Windows/Linux x64 utilisent les archives officielles DXC **v1.9.2609**, vérifiées
par SHA-256. Sous macOS arm64, remplacer `official` par `source`. SDL_shadercross,
SPIRV-Cross et DXC sont épinglés dans le [verrou](tools/shader_toolchain/lock.json).
Le bootstrap construit, installe et teste le CLI sous
`.cache/shader-toolchain/install/<provider>/bin`. Le configure moteur **ne le
lance jamais** : `SOMNIUM_SHADERCROSS_EXECUTABLE` désigne ce CLI, avec DXC voisin
pour les depfiles. Un outil manquant ou inexécutable provoque une erreur fatale
avec la commande de bootstrap. Les détails et le blocage local Windows du CLI
par la stratégie de contrôle d'application sont consignés dans [SHADERS.md](docs/SHADERS.md).

CPM acquiert les bibliothèques aux versions exactes de `cmake/Dependencies.cmake`.
`CPM_SOURCE_CACHE` vaut `.cache/cpm` par défaut ; le premier configure nécessite
Internet. Les caches et le bootstrap CPM sont vérifiés, jamais versionnés.

## Compiler et exécuter

Depuis la racine du dépôt, après le bootstrap :

```sh
cmake --preset debug -DSOMNIUM_WARNINGS_AS_ERRORS=ON
cmake --build --preset debug --parallel
ctest --preset debug
build/debug/bin/sandbox --frames 120
```

Sous Windows, exécuter `sandbox.exe`. Avec LLVM-MinGW, ajouter au premier
configure `-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`.
Sous Linux sans écran : `xvfb-run -a ctest --preset debug` ; les tests purement
CPU se sélectionnent avec `ctest --preset debug -LE window`.

Le backend par défaut est D3D12 sous Windows, Vulkan sous Linux, Metal sous
macOS. `sandbox --backend vulkan` permet une sélection explicite ; aucune
substitution silencieuse. Sans `--frames`, la croix ferme la fenêtre.
Le mode borné ferme après acquisition d'une frame et vérifie sa soumission.
Redimensionnement et minimisation utilisent la taille réelle de la swapchain ;
sans texture, le moteur attend les événements. La présentation VSYNC cadence
la boucle, avec deux frames en vol et aucun délai artificiel.

Le sandbox affiche le cube original CC0 sous `assets/models/orbit-cube.glb`
([licence](assets/LICENSES.md)). Bouton droit + souris ou stick droit : orbite ;
molette ou gâchettes : zoom ; R ou bouton sud de manette : reset. Pitch borné à
±80°, distance 4–30 m. L’overlay est passif et ne capte pas les commandes.

Les chemins sont **logiques et indépendants du répertoire courant**. En mode
DEVELOPMENT (debug/release/profiling), platform cherche `<exécutable>/assets`
d’abord — les shaders compilés du build — puis `<dépôt>/assets`. Aucun asset
source n’est copié dans le build. Shipping utilise COOKED : **une seule racine**
`<exécutable>/assets`, sans repli vers le dépôt. Un fichier absent ou invalide
provoque un diagnostic, jamais un modèle factice.

`cmake --install build/debug --prefix <dossier>` installe binaire et shaders ;
il ne conditionne pas les modèles. Avant le cuiseur de phase 4, constituer
explicitement un package de démonstration hors du build avec le GLB sous
`bin/assets/models/orbit-cube.glb`. Le test shipping vérifie d’abord l’échec sans
modèle, puis ce paquet temporaire depuis un répertoire courant sans rapport.
Ce staging de test ne remplace pas le cuiseur, ni le `.metallib` requis pour shipping macOS.

La chaîne actuelle est glTF/GLB + PNG/JPEG → loaders CPU cgltf/stb_image →
AssetManager typé (Unloaded/Loading/Ready/Failed) → préparation renderer par runtime
→ DrawList indexée. Les sources restent l’autorité ; KTX2/BC, mipmaps, formats cuits,
jobs et hot-reload sont reportés. La lecture boucle sur les retours partiels,
limite chaque fichier à 256 MiB et interdit les chemins natifs ou sortant des racines.

| Preset | Build | UI | Tracy | Validation GPU / sanitizers |
| --- | --- | --- | --- | --- |
| `debug` | Debug | ON | OFF | Debug GPU |
| `release` | Release | ON | OFF | OFF |
| `profiling` | RelWithDebInfo | ON | CPU/mémoire | OFF |
| `shipping` | Release | OFF | OFF | OFF |
| `asan-ubsan` | Debug, Linux/macOS | ON | OFF | Debug GPU, ASan + UBSan |

Chaque preset possède les commandes configure/build/ctest correspondantes.
L'overlay affiche FPS, temps moyen de frame et compteurs de dessins, soumissions
et uploads. `debug_ui` pilote les deux intégrations officielles séparées
`somnium_platform_imgui` et `somnium_gpu_imgui`. Le preset shipping ne construit
ni ces cibles, ni `somnium_imgui`, ni `somnium_debug_ui` ; la CI inspecte le graphe
et les unités compilées. Les modules encore sans sources restent sans cible.

## Shaders et preuve de rendu

`somnium_add_shaders()` produit SPIR-V, DXIL, MSL **2.3** et manifeste réfléchi,
avec depfiles sur les includes HLSL transitifs. Entrées `vsMain`/`psMain`, jamais
`main`. Cible moteur macOS **13.3** minimum pour `std::format` avec la libc++ Apple ;
un ancien cache doit être reconfiguré avec `-DCMAKE_OSX_DEPLOYMENT_TARGET=13.3`.
MSL reste explicitement **2.3.0**, compatible avec cette cible. Aucun compilateur HLSL n'est lié
ou livré au moteur ; le driver compile le MSL. Le `.metallib` reste un prérequis
de shipping macOS à réaliser au cuiseur de phase 4.
`SOMNIUM_VERIFY_SHADERCROSS_LIBRARY=OFF` désactive l'ancienne archive de vérification.

D5 crée un device sans fenêtre, rend une scène RGBA8/D32, relit après une fence
et compare la référence PNG unique sous LFS. Géométrie, seuils et dix sondes
exactes sont dans `engine/gpu/tests/reference_scene.json`.

```sh
python tools/gpu/run.py --executable build/debug/bin/somnium_gpu_offscreen --backend vulkan --output .cache/d5-local
```

Windows : `.exe` et `--backend direct3d12` ; macOS : `--backend metal` ; Linux
sans écran : préfixer par `xvfb-run -a`. Le [passage D5 validé de l'étape b](https://github.com/FoxTimew/Somnium/actions/runs/39593890450)
obtient **zéro pixel différent et dix centres exacts** sur lavapipe/Linux,
Microsoft Basic Render Driver/Windows et Metal/macOS 15, avec couches actives.
La référence et sa provenance sont inchangées pendant les étapes c–e.
Le rendu Metal dépend du runner **macos-15** à cause d'un défaut de validation
shader paravirtualisée sur macOS 26 ; horizon de retrait et procédure de migration
dans le [plan CI](docs/PHASE_1_CI.md). D5 hors écran et captures de présentation
sont des preuves distinctes. Sans device/couche disponible, le résultat publié
est explicitement « non validé ».

## Qualité, logs et profilage

Le [test cube de phase 2](docs/CUBE_VALIDATION.md) rend le même glTF normal puis
miroir X sur les trois backends. Sa référence analytique est distincte de D5 ;
l’ambiant y reste nul, référence et provenance inchangées. Le sandbox ajoute un
ambiant de 0,15 dans une variante shader et utilise SDR_LINEAR : un seul encodage
sRGB à la sortie. Les tests input/caméra sont entièrement CPU.

La CI requise configure, construit et teste **debug, release, shipping et profiling
sur Windows et Linux**, puis vérifie D5/cube et le test de fenêtre ; clang-tidy **18**
et ASan/UBSan tournent sous Linux. macOS est un workflow manuel best effort.
Les captures Windows injectant souris/clavier sont réservées au job manuel
workflow_dispatch ; [démarrer les runners et les captures](docs/DEMARRER_RUNNERS.md).
Logs, binaires et images restent sur le disque du runner par run, pendant 14 jours ;
le résumé GitHub indique leurs chemins. Les actions restantes sont épinglées par SHA.

```sh
python -m pip install clang-format==18.1.8
python tools/format.py --check
python tools/check_architecture.py .
python tools/run_clang_tidy.py --build build/debug --tool clang-tidy-18
```

`Log::setLevel` règle le seuil global (Info par défaut).
`setCategoryLevel("gpu", Level::Debug)` remplace ce seuil pour la catégorie exacte ;
`clearCategoryLevel` restaure l'héritage, `Off` désactive les messages concernés.
`Log::write("gpu", Level::Info, "Uploads : {}", count)` utilise `std::format`
après filtrage. L'interface publique ne dépend pas de spdlog.

Le preset profiling active Tracy **0.14.1** : connecter le profiler de même
version à `127.0.0.1`. Zones CPU, hooks mémoire Engine/Platform, compteurs GPU et
groupes de debug SDL sont disponibles ; aucune zone de durée GPU Tracy ni
marqueur PIX spécifique n'est requis.

Sous Linux, après installation de Mesa/lavapipe :

```sh
cmake --preset asan-ubsan -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DSDL_WAYLAND=OFF -DSDL_X11_SHARED=OFF
cmake --build --preset asan-ubsan --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 python tools/run_sanitizers.py
```

Ce passage utilise X11/Xvfb, lie Xlib et conserve lavapipe chargé jusqu'au contrôle
LSan pour que les racines de caches natifs restent visibles. La détection des
fuites reste active, sans suppression. Wayland n'est pas couvert par ce job.
