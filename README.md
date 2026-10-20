<!--
Describes the foundation build, tooling and platform verification.
Later shader and rendering work remains scoped to its approved phase.
-->
# Somnium

Moteur de jeu 3D C++20, développé par phases sur Windows, Linux et macOS.
La phase 0 fournit l'outillage, les fondations et une fenêtre vide, sans device GPU.

- [Architecture](docs/ARCHITECTURE.md)
- [Stack et dépendances](docs/adr/0001-stack-dependances-licences.md)
- [Conventions](docs/adr/0002-conventions.md)

Le jalon documentaire est accepté. Le socle `main` contient les documents,
ce README et les règles Git/LFS ; le moteur est développé sur
`phase/0-fondations`. La fusion de phase et le tag `phase-0` attendent une
validation distincte.

## Prérequis

- Git avec Git LFS (`git lfs install`), CMake ≥ 3.28, Ninja, Python ≥ 3.10.
- Windows : MSVC avec Windows SDK depuis un terminal développeur, ou
  LLVM-MinGW UCRT. Ce dernier doit avoir son répertoire `bin` dans `PATH`.
- Linux : GCC/Clang et les paquets de développement SDL de la CI (X11/Wayland,
  xkbcommon, EGL/OpenGL, ALSA/PulseAudio et udev) ; `xvfb` pour tester sans écran.
- macOS : Xcode Command Line Tools, CMake et Ninja.
- Formatage : `python -m pip install clang-format==18.1.8`.

Les bibliothèques sont acquises par CPM avec les versions exactes de
`cmake/Dependencies.cmake`. `CPM_SOURCE_CACHE` peut être défini dans
l'environnement ou passé à CMake ; il vaut `.cache/cpm` par défaut. Le premier
configure nécessite Internet. Les fichiers en cache ne sont pas versionnés.
Le bootstrap CPM est vérifié par SHA-256.

## Compiler, exécuter et tester

Depuis la racine du dépôt, dans un terminal avec le compilateur dans `PATH` :

```sh
cmake --preset debug -DSOMNIUM_WARNINGS_AS_ERRORS=ON
cmake --build --preset debug --parallel
ctest --preset debug
```

Sous LLVM-MinGW, sélectionner les compilateurs au premier configure :

```sh
cmake --preset debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DSOMNIUM_WARNINGS_AS_ERRORS=ON
```

Exécution : `build/debug/bin/sandbox` (`sandbox.exe` sous Windows). La fenêtre est
redimensionnable ; la croix de fermeture quitte proprement. Le mode
`sandbox --frames 120` termine en envoyant l'événement de fermeture normal.
La sortie indique frames, pas fixes, coefficient d'interpolation et temps
abandonné. Aucun device GPU ni rendu n'est créé en phase 0.

Sur Linux sans écran : `xvfb-run -a ctest --preset debug`.
Pour les tests purement CPU : `ctest --preset debug -LE window`.
La lecture des fichiers est plafonnée à 256 MiB au MVP et retourne une erreur
explicite au-delà. Les identifiants relatifs n'acceptent pas de remontée `..`.

| Preset | Build | UI | Tracy | Sanitizers |
| --- | --- | --- | --- | --- |
| `debug` | Debug | ON | OFF | OFF |
| `release` | Release | ON | OFF | OFF |
| `profiling` | RelWithDebInfo | ON | CPU/mémoire | OFF |
| `shipping` | Release | OFF | OFF | OFF |
| `asan-ubsan` | Debug, Linux/macOS | ON | OFF | ASan + UBSan |

Chaque preset a les commandes `cmake --preset <nom>`,
`cmake --build --preset <nom>` et `ctest --preset <nom>`.
L'UI est une option de build : les backends officiels sont compilés dans
`somnium_platform_imgui` et `somnium_gpu_imgui`, sans initialisation à cette phase.
Les modules sans source n'ont pas de cible factice.

## Profilage et mémoire

Le preset profiling active Tracy 0.14.1. Connecter le profiler ou l'outil
`capture` de **la même version** à `127.0.0.1` pendant l'exécution. La découverte
réseau et le profilage système sont désactivés. Les zones couvrent la frame,
les pas fixes, le gameplay et la collecte d'événements. Les hooks mémoire
instrumentent les objets Engine et Platform ; ils ne prétendent pas couvrir
les allocations de toutes les dépendances. Hors profiling, les macros
n'évaluent pas leurs arguments. Pas de mesure de durée GPU au MVP.

Sous Linux :

```sh
cmake --preset asan-ubsan -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DSDL_WAYLAND=OFF -DSDL_X11_SHARED=OFF
cmake --build --preset asan-ubsan --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 xvfb-run -a ctest --preset asan-ubsan
```

Le test Linux utilise X11 sous Xvfb. Dans ce build sanitizer, SDL lie X11
normalement au lieu de le charger par `dlopen` : son déchargement ferait
disparaître les racines des caches globaux Xlib avant le passage de LSan.
La détection de fuites reste active, sans suppression. Wayland n'est pas couvert
par cette exécution ; les autres configurations locales peuvent l'activer.

## Outils de shaders — préparation de la phase 1

Phase 0 compile les sources de la bibliothèque SDL_shadercross avec le support
DXC activé et SPIRV-Cross `vulkan-sdk-1.4.363.0`. L'archive reste hors du moteur ;
le CLI lié à DXC et la génération HLSL par `somnium_add_shaders()` appartiennent
à la phase 1. Aucun binaire de shader n'est produit en phase 0.

Pour préparer un CLI complet, installer DXC
[`v1.9.2609`](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2609)
et ses bibliothèques `dxcompiler`/`dxil` (DLL et import libraries sous Windows,
bibliothèques partagées sous Linux). Pour macOS, une construction de DXC depuis
ses sources est nécessaire ; sa configuration sera vérifiée en phase 1.
Installer SPIRV-Cross au tag ci-dessus, avec son interface C et ses packages
CMake, puis SDL3 3.4.18. Configurer le dépôt SDL_shadercross au SHA
`1ff05bec573988a98ef9e0260b4da44f512b8367` avec les options amont
`SDLSHADERCROSS_CLI=ON`, `SDLSHADERCROSS_DXC=ON`,
`SDLSHADERCROSS_VENDORED=OFF`, `SDLSHADERCROSS_SPIRVCROSS_SHARED=OFF`
et les préfixes des packages installés. Son module
`FindDirectXShaderCompiler.cmake` décrit les chemins des SDK attendus.
Les instructions d'installation et la validation du CLI complet seront
finalisées avec la chaîne de shaders ; elles ne sont pas une preuve actuelle
de compilation HLSL sur les trois OS.

Au MVP, le MSL généré hors ligne est chargé par SDL et compilé par le driver.
Le `.metallib` est reporté au cuiseur de phase 4, prérequis de shipping macOS.

## Qualité et CI

```sh
python tools/format.py --check
python tools/check_architecture.py .
```

[GitHub Actions](https://github.com/FoxTimew/Somnium/actions) compile et teste
Windows/Linux/macOS en debug et shipping, avec un job Linux ASan/UBSan.
Les dépendances directes entre modules et leur visibilité sont contrôlées
au configure ; CTest vérifie également les frontières d'includes.
Les logs CTest sont conservés comme artefacts. Les tests de dépendances vérifient
notamment JSON sans exceptions, cgltf/stb sur entrée invalide, EnTT direct et
projection GLM. Les tests de modules vérifient erreurs, handles, lecture et
accumulateur. La preuve de rendu hors écran sera introduite en phase 1.
