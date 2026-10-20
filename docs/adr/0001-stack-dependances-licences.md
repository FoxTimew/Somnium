<!--
Consigne la stack imposée, les versions candidates vérifiées et leurs limites.
Les arbitrages signalés restent soumis à validation avant le code moteur.
-->
# ADR-0001 — Stack, dépendances et licences

Date : 20 octobre 2026. Statut : **proposé, non accepté**.

## Contexte

Somnium est un moteur de jeu C++20 destiné à évoluer plusieurs années.
Le MVP doit fonctionner sur Windows, Linux et macOS avec une chaîne de build
reproductible et des frontières compatibles avec un futur portage console.
Le cahier des charges impose les technologies ; cet ADR enregistre leur emploi,
leurs versions candidates et les incompatibilités identifiées avant le code.

Les sources distantes ont été consultées pour cette revue. Les API de SDL,
shadercross, EnTT et cgltf devront ensuite être relues dans le cache CPM de la
révision effectivement épinglée avant d'écrire les appels. Les liens ci-dessous
ne constituent pas une preuve de compilation ni un audit exhaustif de licences.

## Décision proposée

### Langage et construction

- C++20 sans modules de langage ; C pour les dépendances qui le nécessitent.
- Cibles Somnium statiques, sans exceptions et sans RTTI dans le moteur.
  Options GCC/Clang `-fno-exceptions -fno-rtti` ; côté MSVC, `/GR-`,
  désactivation effective de l'unwinding C++ et `_HAS_EXCEPTIONS=0`, vérifiées
  dans les commandes de compilation.
- `JSON_NOEXCEPTION` et `SPDLOG_NO_EXCEPTIONS` définis de manière cohérente
  dans les unités qui utilisent ces dépendances. Les erreurs d'entrée JSON
  sont détectées avant les conversions susceptibles d'abandonner le processus.
- CMake ≥ 3.28, presets versionnés et CPM.cmake. Pas de `find_package` silencieux
  substituant une autre version aux dépendances gérées par CPM.
- `CPM_SOURCE_CACHE` configurable, hors des fichiers versionnés. Aucun dossier
  personnel ni chemin OS n'est fixé dans le dépôt.
- Options et includes portés par les cibles. Includes tiers `SYSTEM`, sans
  imposer les warnings Somnium aux sources tierces. Pour Somnium :
  `-Wall -Wextra -Wpedantic`, `/W4`, et `-Werror` ou `/WX` en CI.
- Les tests et outils hors ligne peuvent utiliser exceptions et RTTI. Ils ne
  sont pas autorisés à faire traverser une exception dans le moteur.

Les presets prévus sont `debug`, `release`, `profiling` (RelWithDebInfo, Tracy)
et `asan-ubsan` (Linux/macOS). Shipping exclut `debug_ui` et ses adaptateurs ;
le preset release sera le contrôle de cette exclusion.

### Inventaire initial

Les tags sont des candidats exacts relevés sur les releases officielles.
Leur compatibilité collective reste à démontrer après approbation ; toute
modification ultérieure sera justifiée. SDL 3.4.18 est la dernière release
stable 3.x indiquée par l'amont à la date de cette revue.

| Dépendance | Référence candidate | Licence amont à conserver | Usage |
| --- | --- | --- | --- |
| [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake/releases/tag/v0.43.2) | `v0.43.2` | MIT | Acquisition et cache CMake |
| [SDL3](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.18) | `release-3.4.18` | zlib | Plateforme, inputs, SDL GPU |
| [GLM](https://github.com/g-truc/glm/releases/tag/1.0.3) | `1.0.3` | Option MIT | Types et calculs mathématiques |
| [EnTT](https://github.com/skypjack/entt/releases/tag/v4.0.0) | `v4.0.0` | MIT | Registry et identités d'entités directement utilisées |
| [cgltf](https://github.com/jkuhlmann/cgltf/releases/tag/v1.15) | `v1.15` | MIT | glTF 2.0 |
| [stb](https://github.com/nothings/stb/tree/a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d) | `a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d`, soumis à D1 | Option MIT | `stb_image`, PNG/JPEG |
| [nlohmann/json](https://github.com/nlohmann/json/releases/tag/v3.12.0) | `v3.12.0` | MIT | Scènes et mapping versionnés |
| [spdlog](https://github.com/gabime/spdlog/releases/tag/v1.17.0) | `v1.17.0` | MIT ; notices du fmt embarqué à conserver | Implémentation privée de `Log` |
| [Tracy](https://github.com/wolfpld/tracy/releases/tag/v0.14.1) | `v0.14.1` | BSD-3-Clause ; notices des sources embarquées à auditer | CPU et mémoire ; GPU soumis à D4 |
| [Dear ImGui](https://github.com/ocornut/imgui/releases/tag/v1.93.0) | `v1.93.0` | MIT | UI de debug et backends officiels |
| [tl::expected](https://github.com/TartanLlama/expected/releases/tag/v1.3.1) | `v1.3.1` | CC0-1.0, dédicace au domaine public | `Result<T>` |
| [doctest](https://github.com/doctest/doctest/releases/tag/v2.5.3) | `v2.5.3` | MIT | Tests par module via CTest |
| [SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross/tree/1ff05bec573988a98ef9e0260b4da44f512b8367) | `1ff05bec573988a98ef9e0260b4da44f512b8367` | zlib | CLI de build, jamais lié au moteur pour compiler à l'exécution |

Les SHA ont été relevés via `git ls-remote` ; aucune branche flottante ne sera
utilisée dans CPM. Shadercross n'a pas de release publiée, cas expressément
autorisé par le cahier des charges. `stb` nécessite une autorisation distincte.

Les bibliothèques header-only sont validées par des unités de compilation qui
exercent leurs contrats, sans prétendre qu'elles produisent toutes une archive.
Les implémentations de cgltf et stb sont compilées une seule fois dans la cible
tierce appropriée. Les modules encore inactifs ne reçoivent pas de fonctions
factices uniquement pour satisfaire le linker.

### Chaîne de shaders et dépendances transitives

Les sources sont HLSL Shader Model 6. La génération SPIR-V, DXIL et MSL est
effectuée au build par le CLI shadercross et des commandes CMake incrémentales.
Les artefacts sont placés sous le répertoire de build et ne sont pas commités.
Le moteur ne distribue ni n'appelle le compilateur shadercross.

Le CLI nécessite DXC et SPIRV-Cross. La release DXC
[`v1.9.2609`](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2609)
est identifiée comme candidate. Son
[fichier de licences](https://github.com/microsoft/DirectXShaderCompiler/blob/v1.9.2609/LICENSE.TXT)
regroupe plusieurs composants ; l'inventaire doit couvrir les licences réellement
embarquées, pas seulement le label du dépôt.

SPIRV-Cross est sous Apache-2.0. Son tag exact et les éventuelles dépendances
transitives de construction devront être arrêtés avec l'installation vérifiée
du CLI. Le verrouillage reste donc **incomplet pour les outils transitifs** à
ce jalon documentaire. Aucune sélection de la branche principale ni du contenu
flottant d'un SDK n'est autorisée en attendant.

Le [CMake de shadercross à la révision candidate](https://github.com/libsdl-org/SDL_shadercross/blob/1ff05bec573988a98ef9e0260b4da44f512b8367/CMakeLists.txt)
permet de sélectionner DXC, la construction du CLI et les dépendances vendored.
Ses sous-modules incluent également des composants hors besoin MVP : ils ne
seront pas récupérés ou activés aveuglément. DXBC n'est pas requis ; aucune
dépendance vkd3d ne sera ajoutée pour ce chemin.

Les licences admises pour les dépendances du projet sont zlib, MIT, BSD,
Apache-2.0 et domaine public. Aucune dépendance GPL/LGPL n'est ajoutée,
y compris aux outils construits pour le projet. Les notices transitives et les
options Tracy de backtrace/profilage système font partie de cette vérification.
La présence d'une dépendance installée sur la machine n'autorise pas sa liaison.

Les compilateurs et SDK de plateforme, notamment les outils Metal proposés en
D3, ont leurs propres conditions. Leur périmètre doit être distingué des
bibliothèques incorporées au moteur ; si la liste de licences est censée interdire
aussi les SDK propriétaires de plateforme, le choix Windows/macOS doit être
réarbitré avant installation.

### Frontières des bibliothèques

- GLM est autorisé dans les contrats mathématiques, EnTT dans `scene` et les
  consommateurs de la registry, `tl::expected` derrière l'alias public `Result`.
- Les includes spdlog sont confinés à l'implémentation de `Log`. La façade
  n'expose ni logger spdlog ni type fmt ; elle garde catégories et niveaux.
- cgltf et stb sont privés à la production des données CPU. Aucun type de ces
  parseurs ne figure dans les composants ou les draw packets.
- nlohmann/json est privé aux sérialiseurs et loaders de configuration.
- SDL GPU est privé à `gpu`. La coopération de plateforme et d'inputs est
  explicitée en D2 et dans l'architecture.
- Tracy n'est visible dans le code Somnium qu'au travers de macros
  `SOMNIUM_PROFILE_*`. Hors profiling, elles n'évaluent pas leurs arguments.
- ImGui est réservé aux builds non shipping. Les allocateurs et états internes
  des dépendances ne justifient pas de créer des singletons moteur.

## Incompatibilités à arbitrer

### D1 — Épinglage de stb

Constat : `git ls-remote --tags` ne renvoie aucun tag pour le dépôt officiel
[stb](https://github.com/nothings/stb/tags). Un numéro inscrit dans `stb_image.h`
ne constitue pas un tag Git de release.

**Recommandation :** étendre à stb l'exception de SHA complet déjà prévue pour
shadercross. Alternative : choisir une autre bibliothèque à releases, ce qui
changerait la décision imposant stb. Aucun miroir à tag inventé n'est proposé.

### D2 — Backends ImGui et encapsulation SDL

Constat : le backend officiel
[`imgui_impl_sdlgpu3.h`](https://github.com/ocornut/imgui/blob/v1.93.0/backends/imgui_impl_sdlgpu3.h)
inclut `SDL_gpu.h` et reçoit des objets SDL GPU. Le backend
[`imgui_impl_sdl3.h`](https://github.com/ocornut/imgui/blob/v1.93.0/backends/imgui_impl_sdl3.h)
reçoit un `SDL_Event`. La phrase « rien d'autre que input ne voit SDL_Event »
entre aussi en conflit avec la collecte imposée dans `platform`.

**Recommandation :** intégrations privées dans `gpu` et `platform`, UI dans
`debug_ui`, types SDL absents des interfaces publiques. Les backends officiels
restent inchangés. Alternative : autoriser les types SDL dans `debug_ui`, ce qui
élargirait davantage la frontière GPU ; alternative écartée par défaut.

### D3 — MSL et compilation hors ligne

Constat : dans le
[backend Metal de SDL 3.4.18](https://github.com/libsdl-org/SDL/blob/release-3.4.18/src/gpu/metal/SDL_gpu_metal.m),
le chargement MSL passe par `newLibraryWithSource`; METALLIB passe par
`newLibraryWithData`. MSL n'est donc pas un binaire déjà compilé.

**Recommandation :** garder les trois sorties shadercross, et ajouter sur macOS
la compilation MSL vers `.metallib` par les outils Metal avant lancement.
L'ADR-0003 détaillera les artefacts et commandes après lecture des outils
épinglés. Alternative : autoriser explicitement la compilation MSL par le driver
au chargement, ce qui relâcherait l'exigence actuelle.

### D4 — Mesures de temps GPU Tracy

Constat : l'[API publique GPU de SDL 3.4.18](https://github.com/libsdl-org/SDL/blob/release-3.4.18/include/SDL3/SDL_gpu.h)
ne fournit pas d'API de timestamps GPU. Les
[adaptateurs GPU livrés par Tracy 0.14.1](https://github.com/wolfpld/tracy/tree/v0.14.1/public/tracy)
sont liés aux API graphiques natives ; aucun adaptateur SDL GPU n'y est fourni.

**Recommandation :** report explicite des zones de durée GPU ; conserver zones
CPU, hooks mémoire, marqueurs GPU SDL et compteurs de soumissions/draw calls.
Un chronomètre CPU entourant une soumission n'est pas une durée GPU.
Alternative : autoriser des appels natifs ou une modification de SDL, ce qui
contredirait l'isolation et la stack figées. Aucune de ces alternatives n'est
implémentée ni considérée comme acceptée.

### D5 — Preuve de rendu en CI

La compilation d'un backend et les tests CPU ne prouvent pas qu'un triangle a
été rendu. L'accès effectif à un device et à une session adaptée doit être établi
pour chaque runner avant de revendiquer Vulkan, D3D12 ou Metal validé.

**Recommandation :** réserver les validations graphiques de phase 1 à des runners
équipés ; fournir captures et logs de validation GPU comme artefacts. À défaut,
demander une modification explicite du critère de sortie. Ce point n'empêche
pas la matrice sans GPU de phase 0.

## Alternatives écartées par le cahier des charges

- C++20 modules, moteur partagé dès le MVP et scripting : coût sans besoin du jeu.
- OpenGL ou appels Vulkan/D3D12/Metal directs : autre frontière graphique.
- SDL_Renderer : ne répond pas au choix SDL GPU.
- RHI générique : abstraction supplémentaire non requise.
- Wrapper ECS universel : EnTT doit rester directement utilisable.
- OBJ/Assimp, physique tierce et autres formats d'assets : hors périmètre.
- Compilation HLSL au runtime, dépendances sur branches et binaires de shaders
  versionnés : incompatibles avec la reproductibilité demandée.

## Conséquences

La structure sépare les données CPU, leur interprétation en scène et les
ressources de rendu. Les builds de production peuvent exclure l'UI et le
profilage sans changer les composants.

Les bibliothèques standard peuvent encore échouer par manque de mémoire :
désactiver les exceptions n'en fait pas des opérations récupérables. La
proposition de traitement fatal est précisée dans l'ADR-0002.

La stack ne sera considérée verrouillée et utilisable qu'après résolution de D1
à D4, audit des composants effectivement construits, puis compilation et tests
des versions retenues. D5 doit être réglé avant la sortie de phase 1.

Le jalon actuel ne revendique aucune compatibilité binaire, aucune CI verte,
aucune capture Tracy et aucune validation GPU. L'acceptation de cet ADR ne
remplace pas ces preuves.
