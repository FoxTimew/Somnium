<!--
Plan de CI soumis avec les ADR-0003 et ADR-0004 avant toute implémentation GPU.
Les inventaires observés sont distingués des jobs de rendu encore à écrire.
-->
# Plan CI de phase 1 — point de contrôle du 21 octobre 2026

## Ce qui est déjà prouvé

Le [spike](SHADERS.md) compile et exécute shadercross lié à DXC sur les trois OS,
pour vertex et fragment vers SPIR-V/DXIL/MSL et réflexion. Il teste le diagnostic
de configuration sans outil et le rejet de HLSL invalide. Cela ne crée aucun
device GPU. **Aucun backend n'est encore validé par rendu dans ce checkpoint.**

La CI générale conserve debug/shipping/profiling sur les trois OS, formatage,
frontières d'architecture, six tests par preset et ASan/UBSan Linux. Elle est
[verte sur le premier commit du spike](https://github.com/FoxTimew/Somnium/actions/runs/39574749835).
Le test release local est validé 6/6. Le commit docs `842155c` retire sa limitation.

Toutes les actions des deux workflows sont maintenant épinglées par SHA complet :
checkout 7.0.1, setup-python 7.0.0, cache 6.1.0, upload-artifact 7.0.2,
msvc-dev-cmd 1.13.0. Leurs SHA sont l'autorité dans les YAML ; les commentaires
de version servent à la lecture. Les dépendances non releasées suivent la même
règle. Les images hébergées restent évolutives : version enregistrée et cache
invalidé quand elle change, sans prétendre épingler l'image système entière.

## Outils, versions, caches et budgets

Commun : Python 3.12, CMake **3.31.6**, Ninja **1.11.1.3**, clang-format
**18.1.8** ; DXC **v1.9.2609**, shadercross et SPIRV-Cross du verrou.
Les durées à froid ci-dessous concernent le script de chaîne de shaders ;
les budgets de phase sont des objectifs, pas des mesures de rendu.

| OS / image observée | Compilateur et acquisition DXC | Ajouts de phase 1 | Budget |
| --- | --- | --- | --- |
| Windows x64, `windows-2025-vs2026` 20261018.275.1 | MSVC 19.51.36260.0, toolset 14.51.36231, Windows SDK 10.0.26100.0 ; archive Microsoft avec SHA-256 | Contrôle Graphics Tools/debug layer ; tentative D3D12 sans contournement natif | CLI froid 111 s ; objectif <4 min avec setup, moteur/presets <20 min |
| Linux x64, Ubuntu 24.04 20261012.334.1 | GCC 13.3.0 ; archive Microsoft avec SHA-256 | `mesa-vulkan-drivers`, `libvulkan1`, `vulkan-tools`, `vulkan-validationlayers`, dépendances SDL/Xvfb existantes ; clang-tidy 18 | CLI froid 76 s ; objectif <3 min avec setup, moteur <15 min, sanitizers <10 min |
| macOS arm64, `macos-26-arm64` 20261013.0424.1 | Xcode 26.6 (17F113), AppleClang 21.0.0.21000101 ; sources DXC au SHA complet | Vérifier device Metal et activation effective de la validation | CLI froid 816 s ; objectif <20 min avec setup, moteur <15 min |

Le job CLI actuel a un timeout de 60 min, sans retry qui masque un échec.
Les budgets futurs sont 2 min pour le test de rendu et 5 min pour clang-tidy,
avec timeouts explicites. Les versions effectives MSVC/SDK/Xcode/Mesa/Vulkan
seront jointes aux résultats (`--version`, `dpkg-query`, inventaires plateforme).
Les paquets système Ubuntu sont ceux de 24.04, versions effectives consignées ;
ils ne sont pas confondus avec les compilateurs de shaders strictement verrouillés.

Deux caches distincts : CPM par OS et verrou de dépendances existant ; outils
installés du spike par OS/architecture/image/toolchain/hash de recette. Aucun
cache de sortie des tests ni d'image de référence. Les fichiers LFS sont tirés
par checkout avec `lfs: true` dans les futurs jobs GPU. Le cache d'outils ne
dispense jamais de compilation des fixtures, réflexion et essais négatifs.
Le manifeste de verrou dans le cache doit être identique à celui du dépôt.

La voie SDK rejetée garde son probe manuel et son échec HTTP 403 archivé ; elle
n'est pas une solution de repli. Le configure échoue si les outils sélectionnés
manquent, y compris après une restauration de cache incomplète.

## Matrice de rendu et couches de validation

| Backend | Observation du spike | Travail prévu après validation | Preuve exigée |
| --- | --- | --- | --- |
| Vulkan / Linux | Runner nu : aucun ICD ni layer JSON inventorié | Installer Mesa/lavapipe et validation layers ; pointer `VK_DRIVER_FILES` vers l'ICD lvp installé, vérifier `vulkaninfo --summary` et `VK_LAYER_KHRONOS_validation` ; device debug autorisant le logiciel | D5 obligatoire en CI, journal de validation propre ; capture locale RenderDoc pour présentation/overlay |
| D3D12 / Windows | Graphics Tools Installed, `System32/d3d12SDKLayers.dll` présent ; aucun device créé | Device SDL driver `direct3d12`, debug ON ; vérifier que la couche est effectivement chargée | Rendu CI seulement si device utilisable ; sinon **non validé**, test D5 local et capture PIX/RenderDoc + journal joints à la PR |
| Metal / macOS | Xcode 26.6 présent ; `system_profiler SPDisplaysDataType` vide, aucune bibliothèque de validation trouvée par l'inventaire initial | Interroger un vrai device SDL Metal ; vérifier outils/couches avec Xcode, puis activer API/shader validation | Si device et validation disponibles : D5 CI ; sinon **non validé**, D5 local et capture Xcode + journal joints à la PR |

**WARP : réponse négative pour la sélection.** Les propriétés publiques du
[`SDL_gpu.h` épinglé](https://github.com/libsdl-org/SDL/blob/829a65d769d935c4852f8159e964312c0957260a/include/SDL3/SDL_gpu.h#L2266)
n'offrent aucun choix WARP ou identifiant d'adaptateur. Le
[backend D3D12](https://github.com/libsdl-org/SDL/blob/829a65d769d935c4852f8159e964312c0957260a/src/gpu/d3d12/SDL_gpu_d3d12.c#L9112)
énumère l'adaptateur 0 selon préférence d'énergie, sans `EnumWarpAdapter`.
Le hint WARP de SDL Renderer D3D11 ne s'applique pas à SDL GPU. La présence de
WARP ou de la couche debug dans Windows ne prouve donc pas un rendu D3D12 CI.
Pas de fork SDL ni d'injection native d'adaptateur dans cette phase.

Vulkan : exiger l'activation réelle de la couche, pas seulement son installation.
D3D12 : journaliser la création debug et l'absence de messages de validation ;
Graphics Tools présent ne suffit pas. Metal : lancer avec `MTL_DEBUG_LAYER=1`,
`MTL_SHADER_VALIDATION=1`, `MTL_SHADER_VALIDATION_REPORT_TO_STDERR=1` avant création,
selon la [documentation Apple](https://developer.apple.com/documentation/xcode/validating-your-apps-metal-shader-usage).
Le backend SDL pose lui-même `MTL_DEBUG_LAYER=1` en debug sans écraser une valeur
existante ; le job doit donc contrôler l'environnement et la disponibilité
effective. L'inventaire macOS vide n'est ni une preuve d'absence définitive ni
une preuve de validation. Les presets release/profiling/shipping ont le mode
debug GPU OFF ; ils ne remplacent pas le passage debug avec couches.

## Résultats explicites et clôture

Les futurs jobs GPU auront un probe préalable journalisé et une synthèse toujours
exécutée. Trois résultats distincts : **validé** (image + couches propres),
**échec** (outil/rendu/validation incorrects), **non validé** (device ou couche
indisponible, raison et inventaire joints). Un code d'erreur de rendu n'est jamais
reclassé en indisponibilité. Pas de `continue-on-error`, de test skipped, ni de
succès CTest inventé pour une absence de device.

Pour un backend indisponible, le job/check porte explicitement « non validé »
dans son nom et sa synthèse, avec conclusion neutre pour la couverture non
effectuée. Les jobs CPU/outils restent requis et doivent réussir. La clôture
manuelle de phase vérifie alors les pièces locales attachées à la PR : même
commit, backend/device/version, résultat D5, référence/hash, capture graphique
et journal complet des couches. À défaut, la phase n'est pas validable même si
les autres jobs sont verts. Linux/lavapipe est prévu obligatoire : une erreur
d'installation d'ICD/layers est un échec du job, pas une exemption locale.

Le [protocole D5](GPU_VALIDATION.md) fixe RGBA8 256×256, écart de 2/255 par canal,
fraction maximale 0,001, références PNG LFS, régénération manuelle refusée en CI
et images obtenue/référence/diff jointes sur échec. Le rendu hors écran n'atteste
pas la swapchain : resize/minimisation/overlay restent vérifiés séparément.

## Compléments de qualité à implémenter dans la phase

- Ajouter **release** aux trois OS : shipping couvre Release sans UI, profiling
  couvre l'UI optimisée avec Tracy ; aucun ne prouve exactement Release + UI.
- Ajouter un job clang-tidy 18 sous Ubuntu 24.04, compile database du preset
  debug, filtrage des unités Somnium, `.clang-tidy` versionné ; warnings du projet
  bloquants, aucun traitement des caches/dépendances tierces.
- Préserver ASan/UBSan et les contrôles d'architecture, ajouter les tests utiles
  de `Log` (niveau/catégorie/format) et de lecture partielle `SDL_ReadIO`.
- Tester l'incrémentalité des includes HLSL, l'installation des shaders, les
  erreurs de manifeste et la séparation shipping/UI après validation des ADR.

Ces ajouts ne sont pas présentés comme déjà exécutés dans le spike. Les sources
GPU/UI et `cmake/Shaders.cmake` attendent la validation des ADR-0003/0004.
