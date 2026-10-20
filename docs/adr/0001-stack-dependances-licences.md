<!--
Consigne la stack et les arbitrages validés pour le moteur.
Les détails d'intégration restent derrière les frontières de modules.
-->
# ADR-0001 — Stack, dépendances et licences

Date : 20 octobre 2026. Statut : **accepté**.

## Contexte

Somnium utilise une stack C++20 portable et reproductible, avec SDL GPU comme
seule API graphique. Les incompatibilités de la revue initiale ont été arbitrées
explicitement par le propriétaire du projet.

## Décision

C++20 sans modules de langage, exceptions ni RTTI dans les cibles moteur ;
CMake ≥ 3.28, bibliothèques statiques, CPM et `CPM_SOURCE_CACHE`. Warnings
`-Wall -Wextra -Wpedantic` ou `/W4`, traités en erreurs en CI. Les includes tiers
sont `SYSTEM`. Les tests et outils hors ligne peuvent utiliser exceptions/RTTI.
Les tests de phase 0 utilisent toutefois la même configuration sans exceptions
ni RTTI que les modules liés, notamment pour éviter une incompatibilité UBSan/vptr.
`JSON_NOEXCEPTION` et `SPDLOG_NO_EXCEPTIONS` sont définis de façon cohérente.

Les versions retenues sont les suivantes ; les appels SDL, shadercross, EnTT
(notamment cette nouvelle version majeure) et cgltf sont écrits uniquement après
lecture de leurs en-têtes dans le cache CPM.

| Dépendance | Référence retenue | Licence amont à conserver | Usage |
| --- | --- | --- | --- |
| [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake/releases/tag/v0.43.2) | `v0.43.2` | MIT | Acquisition et cache CMake |
| [SDL3](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.18) | `release-3.4.18` | zlib | Plateforme, inputs, SDL GPU |
| [GLM](https://github.com/g-truc/glm/releases/tag/1.0.3) | `1.0.3` | Option MIT | Types et calculs mathématiques |
| [EnTT](https://github.com/skypjack/entt/releases/tag/v4.0.0) | `v4.0.0` | MIT | Registry et identités d'entités directement utilisées |
| [cgltf](https://github.com/jkuhlmann/cgltf/releases/tag/v1.15) | `v1.15` | MIT | glTF 2.0 |
| [stb](https://github.com/nothings/stb/tree/a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d) | `a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d`, D1 accepté | Option MIT | `stb_image`, PNG/JPEG |
| [nlohmann/json](https://github.com/nlohmann/json/releases/tag/v3.12.0) | `v3.12.0` | MIT | Scènes et mapping versionnés |
| [spdlog](https://github.com/gabime/spdlog/releases/tag/v1.17.0) | `v1.17.0` | MIT ; notices du fmt embarqué à conserver | Implémentation privée de `Log` |
| [Tracy](https://github.com/wolfpld/tracy/releases/tag/v0.14.1) | `v0.14.1` | BSD-3-Clause ; client avec LZ4/queue BSD-2-Clause et rpmalloc domaine public | CPU et mémoire ; marqueurs GPU selon D4 |
| [Dear ImGui](https://github.com/ocornut/imgui/releases/tag/v1.93.0) | `v1.93.0` | MIT | UI de debug et backends officiels |
| [tl::expected](https://github.com/TartanLlama/expected/releases/tag/v1.3.1) | `v1.3.1` | CC0-1.0, dédicace au domaine public | `Result<T>` |
| [doctest](https://github.com/doctest/doctest/releases/tag/v2.5.3) | `v2.5.3` | MIT | Tests par module via CTest |
| [SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross/tree/1ff05bec573988a98ef9e0260b4da44f512b8367) | `1ff05bec573988a98ef9e0260b4da44f512b8367` | zlib | CLI de build, jamais lié au moteur pour compiler à l'exécution |
| [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross/tree/vulkan-sdk-1.4.363.0) | `vulkan-sdk-1.4.363.0` | Apache-2.0 ; en-tête SPIR-V MIT | Dépendance de shadercross, hors moteur |

Une dépendance sans release est épinglée par SHA complet, jamais par branche
(D1). Les bibliothèques header-only sont vérifiées par compilation de leurs
contrats ; cgltf/stb ont une seule unité d’implémentation. Les modules sans
sources réelles ont un répertoire et un CMakeLists, sans cible ni fonction factice.

D2 : les backends officiels sont compilés dans `somnium_platform_imgui` et
`somnium_gpu_imgui`, cibles d’intégration séparées, liées par `debug_ui`, ayant
accès aux en-têtes privés de coopération et construites seulement avec
`SOMNIUM_ENABLE_DEBUG_UI=ON`. Aucun include ImGui ou condition UI dans les
sources de `platform`/`gpu`. `SDL_Event` reste dans platform et input ; aucun
type SDL dans les interfaces publiques.

D3 : les HLSL Shader Model 6 sont transformés au build par le CLI shadercross
en SPIR-V, DXIL et MSL. Le moteur charge le MSL ; la compilation effectuée par
le driver est autorisée. Aucun compilateur HLSL dans le moteur. Le `.metallib`
est reporté au cuiseur de phase 4 et sera inscrit dans l’ADR-0003 comme prérequis
de shipping macOS. Aucun shader compilé n’est commité. DXC et SPIRV-Cross sont
les dépendances du CLI ; leurs versions et notices sont verrouillées au build.
Aucun chemin DXBC/vkd3d n’est ajouté.

D4 : pas de zones de durée GPU Tracy. Sont retenus les zones CPU, hooks mémoire,
compteurs de draw calls/soumissions/uploads et marqueurs SDL pour
RenderDoc/PIX/Xcode. Le critère de sortie de phase 2 est amendé ; une durée CPU
de soumission n’est jamais présentée comme une durée GPU.

D5 : un test hors écran rend dans une texture, relit les pixels et compare à
une image de référence avec tolérance. CI : lavapipe/Vulkan sous Linux ;
WARP/D3D12 sous Windows si SDL épinglé permet de choisir cet adaptateur ; Metal
sous macOS si un device existe. Sinon, le job indique « non validé » et une
capture locale avec journal de validation est jointe à la PR de phase 1.

Les presets debug, release et profiling activent l’UI par défaut ; shipping
met `SOMNIUM_ENABLE_DEBUG_UI=OFF`. Profiling utilise RelWithDebInfo et Tracy.
Le preset asan-ubsan cible Linux/macOS.

La règle de licence couvre le code lié ou incorporé à Somnium et ses outils :
toute licence permissive sans copyleft, dont zlib, MIT, BSD, Apache-2.0, NCSA,
BSL-1.0 et CC0/domaine public. Les compilateurs et SDK de plateforme (MSVC,
Windows SDK, Xcode, outils Metal et DXC utilisé comme outil) sont hors périmètre.
Les notices et licences transitives réellement incorporées sont conservées.
Pour les choix multiples, Somnium retient MIT pour GLM/stb et BSD pour HIDAPI
embarqué dans SDL. Les licences restent disponibles avec les sources épinglées
du cache CPM ; une distribution binaire devra les joindre.

## Alternatives

Écartés : SDL_Renderer, appels natifs Vulkan/D3D12/Metal, RHI générique,
wrapper ECS, scripting, Assimp/OBJ et compilation HLSL au runtime. La compilation
MSL vers metallib est différée ; les durées GPU ne sont pas simulées par des
mesures CPU. Les dépendances sans tag ne sont pas remplacées par un miroir à
tag artificiel.

## Conséquences

La scène conserve des handles d’assets CPU et le renderer consomme une DrawList
indépendante de World. Les intégrations UI peuvent être retirées sans modifier
platform/gpu. Le contrat de lecture est défini dans core, réalisé par platform,
injecté par runtime ; le contrat gameplay suit runtime → game → sandbox.
Le socle main contient les documents acceptés, README, règles Git/LFS ; la
phase 0 est développée depuis ce socle sans fusion ni tag avant validation.

Preuves : CLI shadercross lié à DXC et compilation HLSL sur les trois OS, sélection WARP, comparaison hors écran et captures GPU à établir en phase 1.
