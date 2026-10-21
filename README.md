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

Le jalon documentaire et la phase 0 sont validés ; la fusion `--no-ff` dans
`main` et le tag `phase-0` sont réalisés. La suite est préparée sur
`phase/1-gpu-shaders`, avec un spike isolé `feat/shader-toolchain-spike`.
Les [ADR-0003](docs/adr/0003-chaine-shaders.md),
[ADR-0004](docs/adr/0004-frame-synchronisation-gpu.md) et le
[plan CI](docs/PHASE_1_CI.md) sont acceptés avec les ajustements du 21 octobre. L'implémentation s'arrête
après la preuve GPU hors écran (étape b) pour validation intermédiaire.

## Prérequis

- Git avec Git LFS (`git lfs install`), CMake ≥ 3.28, Ninja, Python ≥ 3.12.
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

Exécuter d'abord le bootstrap de shaders décrit ci-dessous. Puis, depuis la racine
du dépôt, dans un terminal avec le compilateur dans `PATH` :

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
L'intégration ImGui au moteur appartient à la phase 1 : initialisation,
façades de coopération et liaison par `debug_ui`. En phase 0, l'option UI
active seulement la compilation des sources amont, dont les deux backends
officiels dans `somnium_platform_imgui` et `somnium_gpu_imgui`.
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

## Outils de shaders — phase 1, étape a

La phase 1 produit SPIR-V, DXIL, MSL 2.3 et un manifeste depuis HLSL au build,
avec depfiles DXC. Entrées imposées : `vsMain` / `psMain`, jamais `main`.
La cible macOS est 11.0 minimum. Les produits sont copiés et installés sous
`bin/assets/shaders`. L'ancienne archive de vérification est optionnelle :
`SOMNIUM_VERIFY_SHADERCROSS_LIBRARY=OFF` par défaut.

Le spike prouve maintenant le CLI complet en CI sur les trois OS : DXC
`v1.9.2609` officiel sous Windows/Linux x64, sources épinglées sous macOS arm64.
Avec Python 3.12, Git et le compilateur C/C++ installé :

```sh
python -m pip install cmake==3.31.6 ninja==1.11.1.3
python tools/shader_toolchain/build.py --provider official --jobs 3
```

Sous macOS, remplacer `official` par `source`. Sous Windows, utiliser un terminal
développeur MSVC. Le bootstrap acquiert SDL, SPIRV-Cross, shadercross et DXC au
[verrou](tools/shader_toolchain/lock.json), vérifie les archives, construit et
teste l'outil. Installation sous `.cache/shader-toolchain/install/<provider>`.
Un outil absent/inexécutable échoue explicitement ; aucun test n'est ignoré.
Les commandes, mesures, cache, comparaison avec le Vulkan SDK et le blocage
local du nouvel exécutable CLI sont consignés dans [SHADERS.md](docs/SHADERS.md).
Exécuter le bootstrap **avant** le configure moteur. Celui-ci ne le lance jamais :
`SOMNIUM_SHADERCROSS_EXECUTABLE` désigne le CLI installé et DXC doit être voisin.
Une erreur fatale indique la commande à exécuter si un outil manque. Les jobs
moteur CI attendent la chaîne d'outils via `needs`. Aucun rendu à l'écran à l'étape a.

Au MVP, le MSL généré hors ligne est chargé par SDL et compilé par le driver.
Le `.metallib` est reporté au cuiseur de phase 4, prérequis de shipping macOS.

## GPU hors écran — phase 1, étape b

`somnium_gpu` possède le device et ses ressources derrière des handles typés et
générationnels. La session `Platform::createHeadless()` initialise les services
vidéo sans fenêtre. Le test D5 charge les shaders du build, rend dans RGBA8/D32,
effectue une relecture avec fence puis compare une référence PNG unique sous LFS.
Géométrie, seuils et sondes exactes : `engine/gpu/tests/reference_scene.json`.

```sh
python tools/gpu/run.py --executable build/debug/bin/somnium_gpu_offscreen --backend vulkan --output .cache/d5-local
```

Windows : `.exe` et `--backend direct3d12` ; macOS : `--backend metal`.
Linux sans écran : `xvfb-run -a` devant la commande. Le
[protocole D5](docs/GPU_VALIDATION.md) décrit la sélection de lavapipe,
la régénération explicite et les captures. D5 est un job GPU distinct des tests
CTest CPU/fenêtre, avec résultat visible « non validé » si device/couches manquent.
La [preuve D5](https://github.com/FoxTimew/Somnium/actions/runs/39592617094) passe
sur lavapipe/Linux, Microsoft Basic Render Driver/Windows et Metal/macOS 15,
avec dix centres exacts et zéro pixel différent sur chaque backend. Le job de
rendu Metal utilise macOS 15 à cause d'un défaut de validation shader du device
paravirtualisé macOS 26, reproduit hors du moteur et documenté dans le protocole.
La sandbox conserve sa fenêtre vide jusqu'à l'étape c, soumise au prochain accord.

## Qualité et CI

```sh
python tools/format.py --check
python tools/check_architecture.py .
```

[GitHub Actions](https://github.com/FoxTimew/Somnium/actions) compile et teste
Windows/Linux/macOS en debug, shipping et profiling, avec un job Linux ASan/UBSan.
Les dépendances directes entre modules et leur visibilité sont contrôlées
au configure ; CTest vérifie également les frontières d'includes.
Les logs CTest sont conservés comme artefacts. Les tests de dépendances vérifient
notamment JSON sans exceptions, cgltf/stb sur entrée invalide, EnTT direct et
projection GLM. Les tests de modules vérifient erreurs, handles, lecture et
accumulateur. Les jobs D5 publient leurs captures, métriques et journaux natifs.
