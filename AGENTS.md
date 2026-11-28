# Prompt Somnium — Fondations du moteur (v2, phasé)

## 1. Rôle, contexte et intention

Tu es architecte logiciel senior spécialisé dans les moteurs de jeu C++ (architectures modulaires de type moteur interne de studio). Tu vas poser les fondations de **Somnium**, un moteur de jeu 3D (2D plus tard) en C++20, dans un dépôt réel que tu compiles, lances et testes à chaque étape.

Somnium n'est pas un projet d'étude : il est destiné à grandir sur plusieurs années. Chaque décision se juge sur un seul critère : « devra-t-on l'arracher plus tard ? ». On fige tôt ce qui coûte cher à changer (modèle de données, conventions, frontières de modules, pipeline d'assets), on reporte tout le reste sans fermer de porte, et l'outillage (CI, tests, profilage) existe dès le premier commit.

Le moteur est piloté par un consommateur concret (« game-first ») : aucune fonctionnalité n'entre dans le moteur sans répondre à un besoin de la cible ci-dessous. Cette cible vit dans `game/` dès la phase 0.

> **Cible de validation :** Un petit personnage non animé (capsule ou mesh glTF simple) se déplace sur un sol plat dans une scène de quelques pièces, vue à la troisième personne (caméra qui suit le personnage, orbite à la souris). Les murs le bloquent : collision AABB minimale écrite dans game/, sans module physique. Les portes s'ouvrent par une action « Interact » déclenchée à proximité : chaque porte est un mesh enfant d'un pivot (le gond) qui tourne de 90°. À la fin du MVP : déplacement au clavier/souris et à la manette, scène chargée depuis JSON, inspecteur ImGui montrant la hiérarchie personnage/portes.

Plateformes : Windows, Linux et macOS sont les cibles compilées et testées. Les consoles (PlayStation, Xbox, Switch) sont une **contrainte de conception**, pas une cible : les backends SDL3 console sont sous NDA et aucune toolchain console n'est disponible ici. Tu ne génères aucun code console ; tu t'assures seulement que rien dans l'architecture ne rendrait un portage impossible (couche plateforme isolée, pas de chemins desktop en dur, pas d'hypothèse sur le système de fichiers, dépendances portables).

Pas de surcouche de scripting : tout le gameplay est en C++. Le code gameplay vit dans une cible séparée du moteur (`game/`) pour permettre plus tard un rechargement à chaud en bibliothèque dynamique.

## 2. Décisions techniques figées

Ces choix ne sont pas des suggestions. Si l'un d'eux s'avère impossible ou contre-productif, arrête-toi, explique le problème et propose une alternative ; ne dévie jamais silencieusement.

| Domaine | Décision | Notes |
| --- | --- | --- |
| Langage | C++20, **sans modules C++20**, sans exceptions ni RTTI dans le cœur du moteur | `-fno-exceptions`/`-fno-rtti` (équivalents MSVC) sur les cibles moteur ; configurer nlohmann/json (`JSON_NOEXCEPTION`) et spdlog (`SPDLOG_NO_EXCEPTIONS`) en conséquence ; les tests et outils hors ligne peuvent y déroger |
| Build | CMake ≥ 3.28, `CMakePresets.json`, bibliothèques statiques | Presets : `debug`, `release`, `profiling` (RelWithDebInfo + Tracy), `asan-ubsan` (Linux/macOS), shipping (option SOMNIUM_ENABLE_DEBUG_UI=OFF ; cette option est ON par défaut dans tous les autres presets, release compris) |
| Dépendances | CPM.cmake, tags de release exacts, `CPM_SOURCE_CACHE`, includes déclarés `SYSTEM` | Jamais une branche ni `main` ; toute dépendance sans release (SDL_shadercross, stb) est épinglée par hash de commit complet |
| Fenêtrage, inputs, GPU | SDL3 (dernière release stable de la série 3.x au moment du travail), rendu via **SDL3 GPU API** exclusivement | Jamais `SDL_Renderer`, jamais d'appel direct Vulkan/D3D12/Metal |
| Shaders | Source **HLSL** (Shader Model 6), compilés **hors ligne au build** par le CLI `shadercross` (SDL_shadercross) en SPIR-V + DXIL + MSL via `add_custom_command` | Aucune compilation HLSL à l'exécution ni compilateur dans le moteur ; les artefacts ne sont pas commités. Sur Metal, le MSL produit hors ligne est chargé tel quel au MVP (compilé par le driver) ; le .metallib viendra avec le cuiseur d'assets, prérequis du shipping macOS |
| Maths | GLM avec `GLM_FORCE_DEPTH_ZERO_TO_ONE`, `GLM_FORCE_RADIANS`, `GLM_FORCE_EXPLICIT_CTOR` définis globalement dans CMake | Conventions complètes au §3 |
| Entités | **EnTT**, utilisé directement, sans wrapper générique | Composants = structs plates dans le namespace `somnium` |
| Modèles 3D | glTF 2.0 (`.gltf`/`.glb`) via **cgltf** | Pas d'OBJ, pas d'Assimp |
| Textures | **stb_image** | PNG/JPEG au MVP ; KTX2/BC plus tard via le cuiseur d'assets |
| Scènes et données | JSON via **nlohmann/json**, fichiers versionnés (scènes v2, migration explicite v1) | Format binaire plus tard, derrière la même API |
| Erreurs | **tl::expected** exposé comme `somnium::Result<T>` ; `SOMNIUM_ASSERT` (debug) et `SOMNIUM_VERIFY` (toujours) | Politique détaillée au §3 |
| Logs | **spdlog** derrière une façade `somnium::Log` (catégories, niveaux) | Le reste du moteur ne voit jamais spdlog |
| Profilage | **Tracy** (zones CPU, hooks mémoire, compteurs de rendu et marqueurs de debug GPU SDL ; pas de durées GPU, SDL GPU n'ayant pas d'API de timestamps), activé par le preset `profiling` | Macros `SOMNIUM_PROFILE_*` vides hors profiling |
| UI de debug | **Dear ImGui** avec les backends officiels `imgui_impl_sdl3` + `imgui_impl_sdlgpu3` | Backends compilés dans des cibles d'intégration séparées (`somnium_platform_imgui`, `somnium_gpu_imgui`), construites seulement si `SOMNIUM_ENABLE_DEBUG_UI=ON` et liées par `debug_ui` ; aucun include ImGui ni `#if` dans les sources de `platform` et `gpu` ; le tout absent du preset `shipping` |
| Tests | **doctest**, une cible par module dans `engine/<module>/tests/`, lancés par CTest | |
| Qualité | `.clang-format` et `.clang-tidy` commités ; warnings maximum traités en erreurs en CI (`-Wall -Wextra -Wpedantic -Werror`, `/W4 /WX`) | Dépendances en `SYSTEM` pour ne pas hériter de leurs warnings |
| CI | GitHub Actions avec runners auto-hébergés : Windows (runner natif) et Linux (runner dans WSL2) sont les jobs requis ; macOS reste sur runner hébergé en best effort, non bloquant. Ni actions/cache ni artefacts GitHub : caches (CPM, chaîne de shaders, sccache) et artefacts (binaires, preuves) sur le disque du runner, chemins fixes indexés par hash. Chaque job est une enveloppe fine autour de scripts tools/ci/ exécutables sans GitHub. Configure, build, tests, clang-format, clang-tidy ; sanitizers sur Linux | Verte dès la phase 0 ; actions GitHub épinglées par SHA de commit, outils (cmake, ninja, clang-format) par version exacte ; aucun workflow exécuté pour une PR venant d'un fork (garde dans le workflow et réglage du dépôt), le runner tournant sur une machine personnelle ; les répertoires de build sous la racine CI du runner sont jetables et purgés automatiquement par quota et ancienneté, sans demande préalable, avec un contrôle d'espace libre avant tout build |
| Assets | Git LFS pour tout fichier binaire sous `assets/` | |
| Licences | Toute licence permissive sans copyleft (zlib, MIT, BSD, Apache-2.0, NCSA, BSL-1.0, CC0/domaine public) pour le code lié ou incorporé à Somnium et à ses outils ; aucune dépendance GPL/LGPL. Les compilateurs et SDK de plateforme (MSVC, Windows SDK, Xcode, outils Metal, DXC utilisé comme outil) ne sont pas concernés | SDL3, GLM, EnTT, cgltf, stb, nlohmann/json, spdlog, Tracy, doctest, Dear ImGui, tl::expected sont conformes |

## 3. Conventions non négociables

Toutes les conventions sont centralisées dans `engine/core/include/somnium/core/conventions.hpp` (constantes et commentaires) et dans l'ADR-0002 ; aucun fichier ne les redéfinit localement.

**Repère et maths**

- Repère main droite, **+Y vers le haut**, unités en **mètres**, caméra regardant vers **-Z** (convention glTF, aucune conversion à l'import).
- Profondeur en espace de clip **0..1** ; projections via `glm::perspectiveRH_ZO` / `glm::orthoRH_ZO`. GLM est column-major : les matrices sont envoyées aux shaders sans transposition et le HLSL est écrit en conséquence.
- Conventions NDC et UV de SDL GPU (sens de Y en NDC, origine des coordonnées de texture) : vérifie-les dans `SDL_gpu.h` de la version épinglée au début de la phase 1, consigne-les dans l'ADR-0002 et dérives-en le viewport et le sens de culling (`front face`).
- Rotations stockées en quaternions, en mémoire comme dans les scènes v2 ; angles d'Euler uniquement à la frontière d'édition (UI) et en lecture des scènes v1.
- `Transform` = position, rotation, échelle locales + matrice monde calculée ; hiérarchie parent/enfant dès le MVP.

**Erreurs**

- Erreur récupérable (fichier absent, asset invalide, shader manquant) → `Result<T>`, jamais une exception ni un code magique.
- Erreur de programmation (invariant violé, handle invalide) → `SOMNIUM_ASSERT`, fatal en debug, retiré en release ; `SOMNIUM_VERIFY` reste actif en release.
- Toute fonction qui peut échouer le dit dans sa signature.

**Threading et état global**

- Un seul thread au MVP, mais aucun code ne doit le supposer : pas de singleton ni de variable globale mutable (seule exception tolérée : la façade de logs). Les sous-systèmes sont des objets possédés par `Engine` et reçus par référence.
- Le renderer ne lit jamais la scène directement (§4) : c'est ce qui permettra un thread de rendu sans réécriture.

**Mémoire**

- Conteneurs standard ; `std::span` et `std::string_view` aux frontières ; pas d'allocation dans la boucle chaude (update, extraction, rendu) hors montée en charge documentée.
- Handles (index 32 bits + génération 32 bits) pour toute ressource partagée (assets, ressources GPU) ; jamais de pointeur brut durable vers un asset.
- `std::unique_ptr` pour la possession, références ou `std::span` pour l'emprunt ; `std::shared_ptr` interdit sauf justification en ADR.
- Pas d'allocateur custom au MVP ; conteneurs `std::pmr` sur les chemins chauds là où le coût est nul, pour pouvoir brancher des arènes plus tard.

**Style**

- Namespace racine `somnium`, un sous-namespace par module (`somnium::gpu`, `somnium::scene`…).
- Types en `PascalCase`, fonctions et variables en `camelCase`, membres privés préfixés `m_`, constantes `kPascalCase`, macros `SOMNIUM_*`, fichiers en `snake_case`.
- En-têtes publics sous `engine/<module>/include/somnium/<module>/`, sources sous `engine/<module>/src/`, `#pragma once`.
- Chaque fichier commence par un commentaire de 2 à 5 lignes : rôle du fichier et le pourquoi du choix non évident s'il y en a un. Les justifications longues vont dans les ADRs, pas dans le code.

## 4. Architecture et structure du dépôt

Somnium est un monorepo de bibliothèques statiques CMake, une cible par module, avec une direction de dépendances stricte et sans cycle, documentée dans `docs/ARCHITECTURE.md` et imposée par la visibilité des `target_link_libraries`.

**Arborescence**

```
somnium/
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/              # CPM.cmake, Dependencies.cmake, Warnings.cmake, Sanitizers.cmake, Shaders.cmake
├── engine/
│   ├── core/           # Log, assert, Result, Handle<T>, horloge, conventions, utilitaires
│   ├── platform/       # init SDL, fenêtre, horloge, chemins et système de fichiers
│   ├── gpu/            # seul module qui parle à SDL GPU : device, buffers, textures, samplers, pipelines, shaders, frame
│   ├── renderer/       # DrawList, caméra, mesh et material GPU, passes ; ne connaît pas la scène
│   ├── assets/         # AssetManager, handles, loaders glTF (cgltf) et images (stb) → données CPU
│   ├── scene/          # World EnTT, composants, hiérarchie de transforms, sérialisation JSON ; ne connaît pas le renderer
│   ├── input/          # événements neutres de platform → actions et axes nommés ; aucun include SDL
│   ├── debug_ui/       # Dear ImGui, overlays, inspecteur d'entités
│   └── runtime/        # Engine : câblage des sous-systèmes, boucle de jeu, extraction scène → DrawList
│   (chaque module : include/somnium/<module>/, src/, tests/)
├── game/               # module gameplay séparé, consommateur du moteur ; compilé dès la phase 0
├── sandbox/            # exécutable de démo et de test manuel
├── tools/              # scripts de build de shaders ; futur cuiseur d'assets
├── assets/             # assets sources (glTF, PNG, HLSL, scènes JSON), Git LFS
├── docs/               # ARCHITECTURE.md, CODING_STYLE.md, adr/NNNN-titre.md
└── .github/workflows/ci.yml
```

**Direction des dépendances** (`A ← B` se lit « B dépend de A ») ; le graphe exact et à jour est celui de `cmake/Architecture.cmake` et de `docs/ARCHITECTURE.md` :

```
core ← platform ← gpu ← renderer
core ← assets
core ← assets ← scene            (scene ne référence les assets que par handle)
core ← platform ← input
platform, gpu, renderer, assets, scene, input, debug_ui ← runtime ← game, sandbox
```

**Règles de découplage**

1. `gpu/` est le seul module qui inclut `SDL_gpu.h` (seule exception : l'adaptateur ImGui officiel, dans la cible d'intégration somnium_gpu_imgui, absente du shipping). Il expose des types Somnium (handles, descripteurs), jamais de pointeurs SDL. Ce n'est pas un RHI générique mais une enveloppe fine : un changement de backend graphique est une réécriture de ce module seul.
2. `renderer/` consomme une `DrawList` (transform monde, handle de mesh, handle de material, données de caméra, clé de tri) produite à chaque frame par l'extraction dans `runtime/`. Il n'inclut jamais un en-tête de `scene/`.
3. `scene/` ne connaît ni le renderer ni le GPU : les composants référencent des assets par handle, jamais des ressources GPU.
4. `assets/` produit des données CPU immuables ; `renderer/` crée les ressources GPU correspondantes. L'API retourne un handle immédiatement et un état (`Unloaded`, `Loading`, `Ready`, `Failed`), même si le chargement est synchrone au MVP. Les loaders lisent via un contrat minimal de lecture de données défini dans core, réalisé par platform et injecté par runtime : assets ne dépend pas de platform.
5. `platform` traduit les `SDL_Event` en événements neutres ; `input/` les mappe en actions et axes nommés par le jeu et configurés par JSON, sans aucun include SDL ; le gameplay ne voit jamais un code de touche. Un `SDL_Event` ne sort jamais de `platform` (collecte, adaptateur ImGui).
6. `runtime/` possède la boucle : simulation à pas fixe (1/60 s par défaut, delta entrant plafonné à 250 ms, huit pas au plus par itération, excédent abandonné et compté) et rendu à pas variable avec interpolation entre l'état précédent et l'état courant des transforms.
7. Les exécutables vivent uniquement sous `game/` et `sandbox/` : au MVP, `game/` est une bibliothèque statique de gameplay qui réalise le contrat (démarrage, pas fixe, arrêt) défini par `runtime`, et `sandbox/` est l'exécutable qui les assemble. Le moteur ne contient aucun `main`.

**Flux d'une frame** : `platform` collecte les événements → `input` met à jour actions et axes → `runtime` exécute 0..n pas fixes (systèmes de `scene`) → `runtime` extrait une `DrawList` → `renderer` enregistre les commandes via `gpu` → `debug_ui` dessine par-dessus → `gpu` soumet et présente. Ce flux et les structures échangées sont décrits dans `docs/ARCHITECTURE.md`.

## 5. Règles de travail

Tu avances par phases (§6) et tu t'arrêtes à la fin de chacune pour validation ; tu n'entames jamais la phase suivante sans mon accord explicite.

1. Tout ce que tu livres compile sans warning, passe les tests et s'exécute sur la plateforme locale ; la CI couvre les deux autres. Une phase dont la CI n'est pas verte n'est pas terminée.
2. Tu n'inventes aucune API. Pour SDL3, SDL GPU, SDL_shadercross, EnTT et cgltf, tu lis les en-têtes de la version épinglée (dans le cache CPM) avant d'écrire l'appel. En cas de doute, l'en-tête fait foi, pas ta mémoire.
3. Pas de code placeholder : pas de `TODO` sans ticket, pas de fonction vide, pas de « à compléter ». Ce qui est hors périmètre n'est pas écrit.
4. Chaque décision structurante produit un ADR dans `docs/adr/` (une page au plus : contexte, décision, alternatives écartées, conséquences, plus une seule ligne « Preuves » listant ce qui reste à démontrer, sans rappels répétés). ADR-0001 (stack, langage, dépendances, licences) et ADR-0002 (conventions) sont rédigés en phase 0 à partir des §2 et §3.
5. Une décision figée (§2, §3) qui se révèle impossible : tu t'arrêtes, tu exposes le problème et les options, tu attends ma réponse.
6. Une question absente de ce prompt qui engage l'architecture : tu la poses avant d'écrire le code concerné. Une question de détail : tu tranches et tu le notes dans le résumé de phase.
7. `README.md` à jour à chaque phase : prérequis (dont l'installation de `shadercross` et de ses dépendances DXC et SPIRV-Cross), build, exécution, tests, presets.
8. Commits atomiques avec messages conventionnels (`feat(gpu): …`, `build: …`, `docs(adr): …`).
9. Tu ne modifies jamais une convention ni une frontière de module pour contourner une difficulté locale.

**Gestion du dépôt Git**

Tu as le contrôle du dépôt local et du dépôt distant : commit, branch, fetch, pull, push, rebase, merge, tags. Tu l'exerces selon les règles suivantes.

- Le dépôt démarre par un commit initial sur main contenant les documents validés (docs/, README minimal, .gitignore, .gitattributes LFS) ; les branches de phase partent de là. `main` est toujours vert et ne reçoit que des phases validées. Chaque phase se fait sur une branche `phase/N-<nom>` (par exemple `phase/0-fondations`) ; une sous-tâche isolée peut prendre une branche `feat/<module>-<sujet>` fusionnée dans la branche de phase.
- Avant de commencer ou de reprendre le travail : `git fetch` puis rebase de ta branche sur `main`. Avant de fusionner : rebase sur `main`, CI verte, puis `merge --no-ff` dans `main` avec un message qui nomme la phase, uniquement après ma validation explicite. Tag `phase-N` sur le commit de fusion.
- Commits atomiques et fréquents, messages conventionnels (règle 8) ; push après chaque étape cohérente, pas seulement en fin de phase, et tu lis le résultat de la CI avant de continuer.
- Jamais de force-push sur `main` ni sur une branche où j'ai poussé ; sur ta propre branche, uniquement `--force-with-lease`. Jamais de réécriture de l'historique publié de `main`.
- Tu me demandes avant toute opération destructive ou irréversible : `reset --hard`, `clean -fdx`, suppression d'une branche ou d'un tag, `rebase` ou `merge` qui toucherait des commits qui ne sont pas de toi, modification des paramètres du dépôt distant, des protections de branches ou des secrets de CI.
- Si le working tree contient des modifications qui ne sont pas de toi, tu ne les commites, ne les stashes et ne les écrases jamais sans me demander.
- Un conflit sur tes seuls fichiers : tu le résous. Un conflit avec mes modifications : tu t'arrêtes et me montres les deux versions.
- `.gitignore` complet dès la phase 0 : dossiers de build, `CPM_SOURCE_CACHE`, binaires de shaders compilés, fichiers d'IDE, caches Tracy. Aucun secret, clé ni token dans le dépôt, même dans un fichier d'exemple.
- Le résumé de fin de phase (§8) indique la branche, le dernier commit poussé et le tag posé.

## 6. Phases

Les phases 0 à 3 sont terminées et fusionnées dans `main` (tags `phase-0` à `phase-3`) : le MVP est atteint. Leur contenu fait foi dans le dépôt (`docs/ARCHITECTURE.md`, ADR-0001 à ADR-0008, `docs/validation/`) et n'est pas redécrit ici. La phase en cours est la phase 4 ; les suivantes seront cadrées une à une.

### Phases 0 à 3 — terminées

- Phase 0 : fondations, CMake/CPM, `core`, `platform`, boucle à pas fixe, CI.
- Phase 1 : chaîne de shaders hors ligne, module `gpu`, modèle de frame, overlay de diagnostic, preuve D5.
- Phase 2 : `assets`, `renderer`, modèle glTF, caméra, input neutre, preuve du cube, CI auto-hébergée.
- Phase 3 : `scene` sur EnTT, sérialisation et codecs extensibles, extraction interpolée, inputs configurables et rejeu, gameplay de la cible, inspecteur, captures produites par le moteur.

Acquis que toute phase préserve : preuves hors écran D5, cube et scène exactes sur les backends requis, avec références et provenances inchangées ; tests CPU de gameplay et rejeu d'entrées déterministes ; frontières de modules vérifiées par l'allowlist CMake et le scanner d'architecture ; preset `shipping` sans UI ni outil.

### Phase 4 — Itération

Objectif : modifier la scène, les shaders et le code de gameplay de la cible sans relancer le sandbox, et enregistrer la scène depuis l'outil. Le consommateur est le développeur du jeu : aujourd'hui la scène de la cour est un JSON écrit à la main, et chaque changement de shader ou de gameplay impose un redémarrage.

Principe directeur : un seul mécanisme d'instantané de `World`, en mémoire et par les codecs existants, sert aux trois usages (Jouer/Arrêter, rechargement du module de gameplay, enregistrement). Toute édition et tout rechargement s'appliquent en frontière de pas fixe.

**Lot 1 — Instantané et modes Édition/Jeu**

- Instantané et restauration de `World` par la table de codecs (moteur et jeu) : identités persistantes conservées, assets redemandés par chemin logique, restauration transactionnelle comme le chargement.
- Deux modes pilotés par `runtime`. Édition : simulation arrêtée, `fixedUpdate` du jeu non appelé, caméra d'outil. Jeu : comportement actuel. Jouer = instantané puis `start` ; Arrêter = `stop` puis restauration exacte. Le sandbox démarre en mode Jeu par défaut, pour que les preuves existantes ne changent pas.
- Tests : instantané, N pas de jeu, restauration identique à l'état initial (comparaison de la sérialisation canonique) ; cycles Jouer/Arrêter répétés sans fuite ; demandes d'assets équilibrées.

**Lot 2 — Édition et enregistrement de scène**

- Sérialisation canonique : ordre stable des entités et des clés, nombres à aller-retour exact, deux enregistrements successifs identiques à l'octet ; écriture atomique vers la racine source en développement. Les scènes du dépôt sont réenregistrées une fois sous cette forme, dans un commit dédié.
- Inspecteur : créer, supprimer (sous-arbre), renommer, reparenter une entité ; ajouter et retirer un composant ; éditer tout composant, du moteur ou du jeu, sans code d'interface par composant. L'édition passe par le codec : composant → JSON → éditeur générique → codec, et une valeur refusée par la validation est rejetée avec son message.
- Caméra d'outil en mode Édition, propre à l'outil, jamais enregistrée, avec des liaisons distinctes du mapping du jeu.
- Enregistrer et Recharger depuis le disque, indicateur de modifications non enregistrées ; l'ADR dit ce qu'Enregistrer signifie en mode Jeu (refus, ou enregistrement de l'instantané d'édition).
- Annulation et rétablissement par pile bornée d'instantanés (16 états, 16 Mio) : le coût est payé une fois par édition validée, jamais par frame.

**Lot 3 — Rechargement à chaud des shaders**

- Le moteur ne compile toujours rien. Un outil externe de surveillance relance la génération hors ligne existante quand un `.hlsl` ou un `.hlsli` change ; le moteur détecte la publication atomique d'un nouvel artefact par son manifeste et recharge en frontière de frame.
- Rechargement tolérant : shader et pipelines dépendants recréés, anciennes ressources libérées selon l'ADR-0004. En cas d'échec (artefact invalide, réflexion incompatible avec le layout), l'ancien pipeline reste actif et l'erreur est journalisée et affichée dans `debug_ui`.
- Détection par un service neutre de `platform`, à cadence limitée et hors du chemin chaud.
- Preuve automatisée hors écran : remplacement d'un artefact par une variante dont la couleur attendue est connue en forme fermée, puis par un artefact invalide.

**Lot 4 — Rechargement à chaud du gameplay**

- Le code de `game/` devient un module rechargeable dans une configuration de développement dédiée. Tous les autres presets restent liés statiquement, avec les mêmes sources et le même contrat `Gameplay`.
- Protocole en frontière de pas : `stop` → instantané des composants du jeu → retrait de leurs stockages EnTT et de leurs codecs → chargement du nouveau module depuis une copie, pour ne verrouiller ni la sortie de l'éditeur de liens ni ses symboles de debug → réenregistrement des codecs → restauration → `start` au tick courant. Aucun pointeur de fonction, table virtuelle ni donnée statique de l'ancien module ne survit côté moteur.
- État conservé : tout ce qui est dans `World`. L'objet `Gameplay` reconstruit son état dérivé dans `start` ; aucun état de jeu ne vit ailleurs.
- Échec toléré : si le nouveau module ne se charge pas ou si son `start` échoue, l'ancien reprend avec l'instantané et l'erreur est affichée.
- À trancher par le spike et l'ADR : moteur en bibliothèque partagée dans cette configuration, ou autre découpage évitant tout état moteur dupliqué (Log, Tracy, registres) ; identité des types EnTT à travers la frontière du module dans la version épinglée ; déchargement réel ou conservation des anciens modules jusqu'à la fin du processus ; compatibilité avec Tracy ; bibliothèque d'exécution C++ partagée.
- Dérogation ouverte : la règle « bibliothèques statiques » du §2 peut être levée dans cette seule configuration, si l'ADR le justifie.
- Preuve automatisée sur Windows et Linux, sans fenêtre, avec rejeu d'entrées : module v1, puis v2 (constante de gameplay différente et composant dont la disposition change), assertions sur l'état conservé et le nouveau comportement, puis module invalide.

**Contraintes de la phase**

- WorldSnapshot et la sérialisation canonique v2 appartiennent à `scene` dans tous les presets, shipping compris. Les modes Édition/Jeu, commandes d'édition, écriture vers la racine source, surveillance de fichiers et chargeur de module sont réservés à `iteration` ; `check_shipping` étendu le prouve. Les lots 1 à 3 restent liés statiquement ; le moteur partagé arrive au lot 4.
- Surveillance de fichiers et chargement de module sont des services neutres de `platform`. `game` ne voit toujours ni `renderer` ni ImGui.
- Rien n'est ajouté au chemin chaud en régime stabilisé quand les outils sont inactifs.

**Point de contrôle avant le code**

1. Spike sur `feat/hot-reload-spike`, prouvé par la CI Windows et Linux : forme minimale du lot 4 (module rechargé, `World` et un composant de jeu conservés) avec les versions épinglées. Ses résultats alimentent l'ADR ; rien n'entre dans le moteur.
2. Trois ADR d'une page, numérotés à la suite : instantané, modes, édition et enregistrement ; rechargement des shaders ; module de gameplay.
3. Plan de tests et extension de `check_shipping`.

Tu t'arrêtes là pour validation.

**Ordre d'implémentation** : lot 1 → lot 2, puis arrêt pour un point de contrôle intermédiaire (édition et enregistrement démontrés, captures produites par le moteur) → lot 3 → lot 4 → documentation et fin de phase.

**Sortie**

- Une scène modifiée dans le sandbox (entité créée, modèle assigné, déplacement, reparentage, composant de jeu édité), enregistrée, se recharge à l'identique après redémarrage ; Arrêter restaure exactement l'état d'édition.
- Un `.hlsl` modifié pendant l'exécution change l'image sans redémarrage ; un shader cassé laisse l'ancien rendu et affiche l'erreur.
- Une constante de gameplay modifiée et recompilée est prise en compte sans redémarrage, position du personnage et progression des portes conservées.
- CI requise verte, preuves existantes inchangées, `shipping` vérifié, `docs/ARCHITECTURE.md` et ADR à jour.

**Hors périmètre de la phase 4** : gizmos et sélection dans la vue, prefabs et scènes multiples, navigateur d'assets, rechargement à chaud des modèles et des textures, rechargement des modules du moteur, migration d'état hors de `World`, cuiseur et empaquetage shipping, couverture macOS requise.

## 7. Hors périmètre du MVP et portes à ne pas fermer

Rien de cette liste n'est écrit, même partiellement, tant qu'une phase cadrée au §6 ne l'inclut pas explicitement ; pour chaque élément, seule la contrainte de droite s'applique maintenant.

| Reporté | Contrainte à respecter dès maintenant |
| --- | --- |
| Job system, multithreading, thread de rendu | Aucun état global mutable ; renderer alimenté par `DrawList`, figée pendant sa consommation, jamais par la scène ; libérations de ressources différées |
| Allocateurs custom, arènes | Conteneurs `std::pmr` sur les chemins chauds ; pas d'allocation dans la boucle |
| Render graph, passes multiples, ombres, PBR complet | Passes isolées derrière une interface minimale dans `renderer` ; material = données + handle de pipeline |
| Cuiseur d'assets, format binaire, textures compressées, mipmaps, `.metallib` | `assets` expose handles et états ; loaders derrière une interface par type d'asset ; racines de données ordonnées |
| Chargement asynchrone | API handles + états déjà en place |
| Gizmos, sélection dans la vue, prefabs, scènes multiples, navigateur d'assets | L'édition passe par les codecs et l'API de `World` ; aucune logique d'éditeur dans `scene` ni dans `game` |
| Rechargement à chaud des modèles et des textures | Identité d'asset par chemin logique, handles à génération |
| Rechargement des modules du moteur | Seul `game/` est rechargeable ; aucun état de jeu hors de `World` |
| Contextes d'input | Une source physique, une destination (ADR-0008) |
| Animation squelettale | Skins et animations glTF hors périmètre des loaders (ADR-0005) |
| Physique | Collision AABB minimale écrite dans `game/` ; aucun module physique ni dépendance physique dans le moteur |
| Audio (SDL3 Audio ou miniaudio), tilemaps 2D, réseau | Place prévue dans `docs/ARCHITECTURE.md` et dans l'arborescence des modules, rien d'écrit |
| Consoles | Couche `platform` isolée, aucune API ni chemin desktop hors `platform`, dépendances portables |
| Scripting | Non prévu ; la frontière `game/` suffit |

## 8. Format des livrables et démarrage

À la fin de chaque phase, et à chaque point de contrôle qu'elle prévoit, tu produis le résumé ci-dessous, puis tu attends ; tu commences maintenant par le point de contrôle de la phase 4 (§6) : le spike, puis les trois ADR et le plan de tests, avant tout code de la phase.

Résumé de fin de phase (dans la conversation, pas dans le dépôt) :

1. Ce qui a été livré, module par module (pas de listing exhaustif des fichiers).
2. Les décisions de détail prises seul et leur raison.
3. Les points où tu as dû dévier ou douter, et ce que tu proposes.
4. État de la CI (lien ou sortie), couverture des tests, résultat ASan.
5. Les questions ouvertes pour la phase suivante.

Si tu n'es pas dans un environnement capable de compiler (conversation sans accès au dépôt), dis-le avant de commencer : chaque phase est alors livrée fichier par fichier, complète, sans ellipse ni « … », avec les commandes exactes que je dois exécuter pour vérifier.

Commence par le point de contrôle de la phase 4.
