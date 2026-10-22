<!--
Décrit le moteur implémenté en phase 1 et les commandes de vérification reproductibles.
Les résultats de rendu et les limites de l'environnement restent explicitement identifiés.
-->
# Somnium

Moteur de jeu 3D C++20 pour Windows, Linux et macOS. La branche
`phase/1-gpu-shaders` ajoute les shaders hors ligne, le device et les ressources
GPU, un triangle présenté en VSYNC et un overlay ImGui. La fusion de phase 1 et
son tag restent soumis à la validation de fin de phase.

- [Architecture](docs/ARCHITECTURE.md) et [conventions](docs/adr/0002-conventions.md)
- [Dépendances et licences](docs/adr/0001-stack-dependances-licences.md)
- [Chaîne de shaders](docs/adr/0003-chaine-shaders.md), [frame GPU](docs/adr/0004-frame-synchronisation-gpu.md)
- [Plan et résultats CI](docs/PHASE_1_CI.md), [protocole D5](docs/GPU_VALIDATION.md)
- [Captures de présentation et journaux](docs/validation/phase-1/README.md) : Windows local, Metal CI identifié

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

Les shaders sont toujours lus via `platform` sous **`<exécutable>/assets`**,
indépendamment du répertoire courant. `cmake --install build/debug --prefix <dossier>`
installe le binaire avec ses artefacts. La lecture boucle sur les retours partiels
SDL et échoue explicitement en cas d'erreur, d'absence de progression ou de fichier
supérieur à 256 MiB. Les chemins logiques refusent `..` et les chemins natifs.

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

Le [point de contrôle cube de phase 2](docs/CUBE_VALIDATION.md) ajoute les loaders
glTF/stb_image, des handles d'assets et un renderer indexé avec uniforms et textures
sRGB. Sa référence analytique hors écran est distincte de D5. Le sandbox conserve
le triangle jusqu'à la validation de ce jalon, avant input et caméra orbitale.

La CI configure, construit et teste **debug, release, shipping et profiling sur
les trois OS**, puis exécute D5, clang-tidy **18** et ASan/UBSan Linux. Les logs,
images de rendu et rapports d'analyse sont des artefacts. Les actions GitHub
sont épinglées par SHA complet.

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
