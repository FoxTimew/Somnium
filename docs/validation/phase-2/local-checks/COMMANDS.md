# Vérifications locales du sandbox

Source : `9dc073342c7255ff87c6ffe7df3fe6b803c64441` ; code des captures propre.
Linux Ubuntu 24.04 / Docker sur Windows, Clang et clang-tidy 18.1.3, CMake 3.31.6,
Python 3.12, lavapipe LLVM 20.1.2/Mesa 25.2.8, Xvfb.
`/src` = dépôt monté en lecture seule ; `/work` = cache local hors Git.

Debug/shipping : `cmake --build /work/build/<preset> --parallel 3`.
Contrôles debug : `ctest --test-dir /work/build/debug --output-on-failure -LE window -E somnium_shader_build_tests`,
puis `xvfb-run -a ctest --test-dir /work/build/debug --output-on-failure -L window`.
Shipping : `python tools/check_shipping.py /work/build/shipping`, puis
`xvfb-run -a ctest --test-dir /work/build/shipping --output-on-failure -E somnium_shader_build_tests`.
Les configurations utilisent Clang 18, `SDL_WAYLAND=OFF`, warnings-as-errors,
et le CLI épinglé `/work/install/official/bin/shadercross`.

Analyse : `python tools/run_clang_tidy.py --build /work/build/debug --tool clang-tidy-18`.
`tidy-present.txt` contient le second contrôle ciblé après l’ajout du message de
première présentation. Format clang-format 18, scanner d’architecture et actionlint passent.

Sanitizers :
```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
python tools/run_sanitizers.py --build-dir /work/build/asan-ubsan
```
Ce premier passage conserve son échec (timeout shader 120 s) dans `tests-asan.txt`.
Les 16 autres tests passent. Test fenêtre complémentaire : mêmes variables ASan/UBSan,
`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json`,
`LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libvulkan_lvp.so`,
`xvfb-run -a ctest --test-dir /work/build/asan-ubsan --output-on-failure --no-tests=error -L window`.
La détection de fuites est active ; aucune suppression.

Relance du contrat shader, CMake natif dans le PATH :
```sh
python tools/shaders/test_build.py --root /src \
  --work /tmp/somnium-final-shader-contract-native \
  --compiler /work/install/official/bin/shadercross
```
Elle passe en 32 s. Le timeout du dépôt et le job CI restent inchangés.

D5/cube : `tools/gpu/run.py`, Vulkan avec Khronos et validation de synchronisation ;
rapports, SHA binaires/manifests et journaux dans `../final-local-d5` et `../final-local-cube`.
La présentation utilise le même environnement de validation et le vrai flux X11 :
```sh
xvfb-run -a -s '-screen 0 1600x1000x24' python tools/presentation/capture_linux.py \
  --executable /work/build/debug/bin/sandbox --output /tmp/somnium-presentation-vulkan-final
```
Sortie copiée sans retouche vers `../sandbox-vulkan` ; `/tmp` évite les accès
concurrents au journal via le montage NTFS. Aucun asset source n’est copié dans le build.
Ces preuves locales ne remplacent pas les tests finaux Windows/macOS.
