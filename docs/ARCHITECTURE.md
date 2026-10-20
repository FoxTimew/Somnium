<!--
Décrit les frontières, les possessions et les échanges prévus pour Somnium.
Document validé ; distingue les contrats du MVP des capacités déjà implémentées.
-->
# Architecture de Somnium

Statut : **accepté**, 20 octobre 2026, avec les arbitrages D1 à D5 ci-dessous.

La phase 0 est autorisée. La phase 1, la fusion finale et le tag `phase-0`
restent soumis à une validation distincte. Voir les
[ADR-0001](adr/0001-stack-dependances-licences.md) et
[ADR-0002](adr/0002-conventions.md).

## 1. Objectif et périmètre

Le consommateur de référence est un jeu C++ : personnage à la troisième personne,
sol et pièces, collisions AABB dans `game/`, portes articulées autour d'un pivot,
action `Interact`, clavier/souris et manette. Le MVP final charge une scène JSON
et affiche sa hiérarchie dans ImGui. Chaque capacité du moteur doit servir ce jeu.

La phase 0 fournit uniquement les fondations, l'outillage et une fenêtre vide.
Elle n'initialise aucun device GPU. Windows, Linux et macOS sont compilés et
testés ; aucune cible, API ni toolchain console n'est introduite.

Les conventions mathématiques, de mémoire, d'erreurs et de style sont décrites
dans l'ADR-0002. Leur traduction en constantes aura une seule autorité :
`engine/core/include/somnium/core/conventions.hpp`.

## 2. Arbitrages acceptés

| Référence | Décision |
| --- | --- |
| D1 | Toute dépendance sans release est épinglée par SHA complet, jamais par branche ; cette règle inclut stb et shadercross. |
| D2 | Les backends officiels sont compilés dans `somnium_platform_imgui` et `somnium_gpu_imgui`, cibles séparées activées uniquement par `SOMNIUM_ENABLE_DEBUG_UI=ON` et liées par `debug_ui`. Les sources de platform/gpu ne contiennent aucun include ImGui ni condition UI. SDL_Event reste dans platform (collecte et intégration privée) et input (traduction). Aucun type SDL public. |
| D3 | Le MVP charge le MSL produit hors ligne par shadercross. Le travail du driver est autorisé ; aucun compilateur HLSL dans le moteur. Le `.metallib` est reporté au cuiseur de phase 4 ; l'ADR-0003 le consignera comme prérequis de shipping macOS. |
| D4 | Pas de zones de durée GPU Tracy. Zones CPU, hooks mémoire, compteurs de draw calls/soumissions/uploads et marqueurs SDL visibles dans RenderDoc/PIX/Xcode. Critère de phase 2 amendé. |
| D5 | Test de rendu hors écran, relecture et comparaison à une référence avec tolérance. CI : Vulkan/lavapipe Linux, D3D12/WARP Windows si sélection supportée (à vérifier dans SDL épinglé), Metal macOS si device disponible. Sans device : job explicitement « non validé » et preuve locale avec capture et journal de validation joints à la PR. |

Les contrats de lecture dans `core`, réalisé par `platform` et injecté par
`runtime`, et de gameplay dans `runtime`, réalisé par `game` et assemblé dans
`sandbox`, sont acceptés. Les valeurs initiales de boucle sont acceptées.

`SOMNIUM_ENABLE_DEBUG_UI` est ON dans debug, release et profiling. Le preset
`shipping` le met à OFF. La licence doit être permissive sans copyleft pour le
code incorporé au moteur et à ses outils ; compilateurs et SDK de plateforme
sont hors de cette règle, y compris DXC utilisé comme outil.

## 3. Modules et visibilité CMake

Chaque module moteur ayant des sources réelles aura une cible statique `somnium_<module>` et un alias
`somnium::<module>`. Ses en-têtes publics résident dans
`engine/<module>/include/somnium/<module>/`, ses implémentations dans `src/`,
ses tests doctest dans `tests/`.

La table décrit les dépendances **directes autorisées**. `PUBLIC` signifie que
des types de la dépendance appartiennent au contrat public ; `PRIVATE` réserve
les détails à l'implémentation. Les bibliothèques tierces sont précisées dans
l'ADR-0001.

| Module | Responsabilité | Somnium PUBLIC | Somnium PRIVATE |
| --- | --- | --- | --- |
| `core` | Logs, erreurs, handles, temps monotone, conventions, instrumentation, contrat de lecture de données | aucune | aucune |
| `platform` | Cycle de vie SDL, fenêtre, événements système, lecture et résolution des chemins | `core` | aucune |
| `gpu` | Enveloppe SDL GPU, ressources, commandes, frame et présentation | `core`, `platform` | aucune |
| `renderer` | Meshes et matériaux GPU, caméra de rendu, DrawList, passe forward | `core`, `gpu` | aucune |
| `assets` | Ressources CPU immuables, loaders, handles et états | `core` | aucune |
| `scene` | World/EnTT, composants, transforms, hiérarchie et JSON | `core`, `assets` | aucune |
| `input` | Traduction des événements en actions et axes | `core` | `platform` |
| `debug_ui` | Contexte ImGui, statistiques, inspecteur et édition | `core` | `platform_imgui`, `gpu_imgui`, `scene`, `input` |
| `platform_imgui` | Backend officiel SDL3 et coopération privée | aucune | `platform` |
| `gpu_imgui` | Backend officiel SDL GPU et coopération privée | aucune | `gpu` |
| `runtime` | Engine, orchestration, simulation, extraction et correspondance assets/rendu | `core`, `scene`, `input` | `platform`, `gpu`, `renderer`, `assets`, `debug_ui` si activé |

`game/` produit une bibliothèque statique `somnium_game`, consommant `runtime`.
`sandbox/` contient l'exécutable et assemble le gameplay et `Engine`. C'est la
lecture retenue de « game/ et sandbox/ sont les seuls exécutables » : aucun
`main` dans le moteur ; un éventuel autre lanceur restera dans ces répertoires.
Le jeu est compilé dès la phase 0. Sa frontière dynamique n'est pas implémentée.

Le graphe moteur, lu dans le sens « dépend de », est :

```mermaid
flowchart TD
    platform --> core
    gpu --> platform
    gpu --> core
    renderer --> gpu
    renderer --> core
    assets --> core
    scene --> assets
    scene --> core
    input --> platform
    input --> core
    debug_ui --> platform_imgui
    debug_ui --> gpu_imgui
    platform_imgui --> platform
    gpu_imgui --> gpu
    debug_ui --> scene
    debug_ui --> input
    debug_ui --> core
    runtime --> platform
    runtime --> gpu
    runtime --> renderer
    runtime --> assets
    runtime --> scene
    runtime --> input
    runtime --> debug_ui
    runtime --> core
    game --> runtime
    sandbox --> game
    sandbox --> runtime
```

Il n'existe notamment aucun arc `renderer -> scene`, `scene -> renderer`,
`scene -> gpu`, `assets -> platform` ou `runtime -> game`. `core` ne devient
pas un registre global de services.

Les includes ne sont pas ajoutés globalement à CMake. Les dépendances tierces
sont déclarées `SYSTEM`. `cmake/Architecture.cmake` rejette les liens directs
interdits et les dépendances privées exposées publiquement lors du configure.
Le contrôle d'includes `tools/check_architecture.py` vérifie en CI les frontières
natives et les dépendances entre en-têtes Somnium. Les en-têtes privés de
coopération ne sont pas installés. `SDL.h`, qui peut inclure d'autres
sous-systèmes, n'est pas utilisé comme raccourci pour contourner ces frontières.

## 4. Possession et durée de vie

`Engine` possède les sous-systèmes par valeur ou `std::unique_ptr`. Les
consommateurs reçoivent des références et des vues de durée de vie explicite.
Aucun service locator, singleton moteur ni `std::shared_ptr` n'est introduit.
La façade `Log` reste la seule exception d'état global mutable propre à Somnium.

Le lanceur possède l'instance de gameplay et la conserve pendant l'exécution
d'`Engine`. `runtime` définit le contrat de démarrage, pas fixe et arrêt ; `game`
le réalise. L'appel passe par ce contrat, jamais par un include de `game`.
Les références de scène et d'inputs sont empruntées pour l'appel. L'état moteur,
la registry et les ressources restent possédés par `Engine`.

En phase 0, `Engine` possède uniquement ce qu'il utilise : plateforme, fenêtre,
lecteur de ressources, horloge et état de boucle. Tous les modules ont leurs
répertoires et leur
CMakeLists ; aucune cible sans sources réelles ni fonction factice n’est créée.

À partir des phases suivantes, la création respecte les dépendances. À l'arrêt :
le gameplay cesse d'être appelé, les travaux GPU sont achevés, l'UI et ses
adaptateurs sont libérés, puis les ressources renderer, le device, la fenêtre
et SDL. Une initialisation partielle est nettoyée par RAII. Un destructeur ne
lève pas d'exception ; une fermeture récupérable qui doit être rapportée possède
une opération explicite retournant `Result`.

La simulation s'exécute sur un thread au MVP. Les contraintes documentées de SDL
sur le thread de fenêtre et d'événements sont respectées. Cela n'autorise pas
d'accès implicites depuis n'importe quel thread : les objets ont un propriétaire
et les emprunts ne sont pas partagés simultanément. Les dépendances peuvent
avoir leur propre état interne ou leurs threads, notamment Tracy ; elles ne
constituent pas une promesse de sûreté concurrente du moteur.

## 5. Contrats de données

Ces descriptions fixent la sémantique, sans promettre des signatures C++ déjà
implémentées.

| Contrat | Contenu et invariants | Propriétaire / emprunteur |
| --- | --- | --- |
| `Result<T>` | Alias de `tl::expected<T, Error>`, y compris `Result<void>` ; erreur typée et diagnostic possédé | retourné par valeur |
| `Handle<Tag>` | Index 32 bits et génération 32 bits ; types distincts par domaine, valeur nulle explicite | valeur copiée, validation par le propriétaire du stockage |
| Lecture de données | Identifiant logique et résultat de lecture d'octets possédés ; aucune notion de chemin OS dans les loaders | contrat minimal dans `core`, implémentation `platform`, injection par `runtime` |
| Asset CPU | Mesh, matériau ou image immuable après `Ready` ; références internes par handles | `AssetManager` ; emprunts bornés au traitement |
| `Transform` | Position, quaternion et échelle locaux ; matrice monde dérivée ; historique nécessaire à l'interpolation | composant plat dans le namespace `somnium`, stocké dans World |
| Hiérarchie | Parent et liens enfants utilisant les identités EnTT ; arbre acyclique, ordre de propagation défini | `scene` |
| Caméra extraite | Matrices vue/projection, position et paramètres utiles au rendu ; aucune référence à une entité | valeur dans la DrawList |
| Draw packet | Matrice monde interpolée, handle renderer de mesh, handle renderer de matériau, clé de tri | stockage réutilisable détenu par `runtime` |
| `DrawList` | Caméra et séquence contiguë de packets ; aucune registry, entité, ressource CPU empruntée ni pointeur SDL | `runtime` produit, `renderer` lit |
| État d'input | Actions tenues et transitions, axes, deltas et informations de périphérique utiles | `input` produit, gameplay consulte |
| Frame GPU | Ressources temporaires et commandes d'une frame ; fin par soumission ou abandon contrôlé | `gpu`, emprunts limités à la frame |

### Handles et assets

Un handle ne possède pas sa ressource. Le propriétaire teste l'index, l'occupation
et la génération avant de fournir un emprunt. Une destruction incrémente la
génération avant réutilisation du slot. Le retour à une ancienne génération est
interdit : un slot arrivé à saturation est retiré de la liste réutilisable.
Deux gestionnaires du même type ne s'échangent pas leurs handles.

Une demande d'asset retourne un handle et expose l'état `Unloaded`, `Loading`,
`Ready` ou `Failed`. Une demande irrecevable peut échouer par `Result` avant de
créer un handle ; une erreur de chargement d'une demande acceptée est conservée
avec l'état `Failed`. Au MVP, le chargement synchrone peut atteindre `Ready` ou
`Failed` avant le retour. Les appelants consultent néanmoins l'état.

Les erreurs et les données ne sont pas empruntées au-delà des garanties du
gestionnaire. L'éviction automatique, les jobs de chargement et le hot-reload
ne sont pas implémentés. La destruction contrôlée intervient à une frontière
où aucune extraction ni commande ne référence la ressource.

### Lecture et résolution des chemins

La dépendance `assets -> platform` serait contraire au graphe imposé.
Le contrat de lecture minimal est donc défini dans `core` ; `platform` le
réalise et `runtime` le fournit aux loaders. Les callbacks de lecture de cgltf
passent par ce contrat, y compris pour les buffers et images externes.

`platform` est seul responsable des racines de ressources, du dossier utilisateur
et des chemins physiques. Le moteur ne suppose pas que le répertoire courant
est celui du dépôt, ni qu'une ressource sera toujours un fichier de bureau.
Les données JSON et les shaders peuvent également être fournis comme octets.
Le futur format cuit remplacera la source et les loaders, sans changer les
handles ou les états ; aucun cuiseur ni système de montage n'est écrit au MVP.

### Frontière assets / rendu

`scene` conserve des handles d'assets CPU dans `MeshRenderer`. `renderer` possède
ses ressources de mesh et matériau, ainsi que leurs handles typés distincts.
`runtime` maintient la correspondance entre les deux domaines et adapte les
données CPU aux vues de création acceptées par `renderer`.

L'upload a lieu au chargement ou dans une étape de préparation explicitement
séparée, jamais implicitement au milieu du parcours chaud d'extraction. Un asset
non prêt est omis avec un diagnostic limité ; un shader obligatoire absent fait
échouer le démarrage graphique. Aucun chargement disque n'est déclenché par un
draw packet. Les matériaux de rendu regroupent données et handle de pipeline.

### Scène, persistance et interpolation

`World` possède directement une registry EnTT ; il ne masque pas EnTT derrière
une abstraction d'ECS. Les composants restent des structs plates dans
`somnium`. Les opérations de hiérarchie vérifient les cycles et les parents
invalides ; la destruction d'un parent doit appliquer une politique explicite,
précisée avec l'ADR-0006 avant l'implémentation de scène.

Le JSON porte `"version": 1` et des versions de composants. Les références
persistées utilisent des identités de scène et des identifiants logiques
d'assets, jamais les bits des handles runtime ni les valeurs brutes d'EnTT.
Les détails du schéma et la politique de migration seront soumis avec
l'ADR-0006 en phase 3.

Pour l'interpolation, les positions et échelles locales sont interpolées
linéairement, les quaternions par interpolation sphérique normalisée. Les
matrices monde sont ensuite reconstruites dans l'ordre parent-enfant. Mélanger
directement les coefficients de matrices monde ne convient pas aux pivots de
portes. Un nouvel objet, une téléportation ou un changement de parent réinitialise
son historique ; un objet détruit est exclu de l'extraction.

## 6. Événements et intégration ImGui

`platform` collecte les événements SDL sur le thread requis et traite le cycle
de vie de la fenêtre. Sa coopération privée permet à `input` de traduire les
événements et à l’intégration de plateforme ImGui de les recevoir. Aucun
`SDL_Event` ne traverse une interface publique, `runtime`, `scene`, `renderer`,
`gpu` ou `game`.

Les cibles séparées `somnium_platform_imgui` et `somnium_gpu_imgui` compilent
respectivement les backends officiels `imgui_impl_sdl3` et `imgui_impl_sdlgpu3`.
Elles ont accès aux en-têtes privés de coopération de `platform` et `gpu` et
sont liées par `debug_ui`. Les sources de `platform` et `gpu` ne contiennent
aucun include ImGui ni `#if` lié à l’UI. Les cibles d’intégration n’existent
que si `SOMNIUM_ENABLE_DEBUG_UI=ON` ; leurs dépendances ne créent aucun cycle.

`debug_ui` possède le contexte, construit l’inspecteur et utilise les façades
d’intégration sans type SDL. Les pointeurs natifs demeurent dans la coopération
privée. Aucune initialisation d’UI ou de device GPU n’a lieu en phase 0.
À cette phase, seules les sources officielles sont compilées dans les deux
cibles d'intégration ; les façades de coopération et la cible `debug_ui` seront
créées avec leurs sources réelles en phase 1.

Les événements sont toujours traités pour les relâchements, pertes de focus et
connexions de manette. La capture UI filtre ensuite les actions de gameplay,
sans perdre les transitions nécessaires pour éviter une touche bloquée.
Le détail du mapping et du routage est fixé dans l'ADR-0007.

## 7. Boucle et flux d'une frame

La boucle appartient à `runtime` ; le gameplay fournit ses mises à jour fixes.
L'horloge monotone de `core` exprime les durées en secondes.

1. Mesurer le temps réel et collecter les événements via `platform`.
2. Mettre à jour les états d'input et la capture de l'UI lorsqu'elles existent.
3. Ajouter un delta borné à l'accumulateur. Exécuter de zéro à plusieurs pas
   fixes : conserver l'état précédent, appliquer gameplay et systèmes de scène,
   puis propager les transforms courants.
4. Calculer `alpha = reste / pasFixe`, compris entre zéro inclus et un exclu.
5. Préparer les ressources nouvelles hors du parcours chaud, puis extraire la
   caméra et les packets interpolés dans une DrawList réutilisée.
6. Construire l'UI et obtenir une frame GPU. Préparer ses uploads avant les passes
   de rendu, comme l'exige le backend officiel ImGui.
7. Enregistrer la passe renderer, puis l'overlay UI via `gpu`.
8. Soumettre et présenter via `gpu`, puis marquer la frame CPU pour Tracy.

En phase 0, seules la collecte, l'horloge, la boucle bornée et les notifications
de gameplay utiles existent. Aucune DrawList ni opération GPU factice n'est
appelée. La cadence externe est régulée par une attente de plateforme pour
éviter une boucle active sans rendu.

Valeurs initiales acceptées : pas fixe `1/60 s`, delta entrant plafonné à
`250 ms`, maximum de huit pas par itération. Après ce maximum, les pas entiers
excédentaires sont abandonnés et comptés, en conservant le reste fractionnaire.
Cette perte de temps de simulation est visible dans les diagnostics. Aucun
pas variable n'est substitué au pas fixe. Une reprise après suspension
réinitialise la mesure du temps plutôt que de rattraper toute la suspension.

Une action pressée entre deux pas fixes est mémorisée jusqu'au prochain pas et
consommée une seule fois. Un état tenu reste disponible pour chaque pas ;
les deltas souris ne sont pas répétés à chaque pas de rattrapage. Une frame sans
simulation ne perd pas les transitions. Les politiques détaillées seront testées
avec les inputs en phase 3.

Les conteneurs chauds sont réservés avant la scène, réutilisés et, lorsque
pertinent, de type `std::pmr`. Une croissance déclenche une étape documentée de
montée en charge. La DrawList est figée pendant sa consommation ; le stockage
reste valide jusqu'à la fin de l'enregistrement au MVP. Un futur thread de rendu
devra transférer ou doubler ce stockage et différer les libérations de ressources,
sans changer le contenu des packets.

## 8. Validation et séquencement

| Phase | Capacités ajoutées | Preuves exigées |
| --- | --- | --- |
| Jalon documentaire | Architecture, stack et conventions | Accepté avec D1 à D5 |
| 0 | CMake/CPM, conventions codées, core, platform, boucle runtime, game et sandbox | Build sans warning, tests Result/handles/accumulateur, fenêtre locale et fermeture, matrice CI et sanitizers Linux verts |
| 1 | GPU, chaîne de shaders hors ligne, triangle, UI | Backends réellement exécutés, validation GPU, tests de ressources et shaders, ADR-0003/0004 |
| 2 | Assets CPU, rendu de mesh texturé, caméra via input | Modèle de référence, orientation et textures correctes, Tracy CPU/mémoire, compteurs et marqueurs GPU selon D4, ADR-0005 |
| 3 | World, hiérarchie, JSON, extraction, interpolation, mapping complet, gameplay de référence | Tests de hiérarchie/round-trip/extraction/interpolation, personnage bloqué par les murs, portes à pivot et inspecteur, ADR-0006/0007 |

Chaque module implémenté dispose de sa cible de tests doctest sous `tests/`.
Les tests n'instancient pas de GPU pour vérifier les contrats purement CPU.
Le test de fenêtre borné en CI ne remplace pas le contrôle visuel local ;
l'exécution ASan doit couvrir les chemins réellement exercés, sans prétendre
valider un backend non exécuté.

La CI couvre configure, build, CTest et clang-format sur les trois OS en debug
et shipping, plus ASan/UBSan sur Linux. La fenêtre Linux est testée sur X11
avec Xvfb ; Wayland n'est pas exercé par ces jobs. Les presets sont `debug`,
`release`, `profiling`, `shipping` sans UI et `asan-ubsan` sur Linux/macOS.
Le README décrit les commandes de build et les prérequis
shadercross/DXC/SPIRV-Cross. Le CLI de shaders lié à DXC et sa validation
appartiennent à la phase 1 ; phase 0 compile l'archive shadercross hors moteur.

Le socle initial de `main` contient les trois documents acceptés, un README,
`.gitignore` et `.gitattributes` avec Git LFS. Il conserve le commit documentaire
antérieur. `phase/0-fondations` est avancée sur ce socle avant toute implémentation.
Les étapes cohérentes sont commitées et poussées, puis leur CI consultée.
Le merge `--no-ff` dans `main` et le tag `phase-0` attendent la validation de fin
de phase. Aucune phase suivante ne commence implicitement.

## 9. Extensions réservées

| Extension hors MVP | Frontière préservée dès maintenant |
| --- | --- |
| Jobs et rendu sur un autre thread | Possession explicite, données extraites autonomes, aucune lecture de World par renderer |
| Arènes | Conteneurs `std::pmr` réutilisables ; aucune arène custom aujourd'hui |
| Render graph, ombres, PBR | Passe forward isolée, données de matériau et pipeline |
| Cuiseur, formats binaires, textures compressées, asynchrone | Lecture injectée, loaders par type, handles et états |
| Éditeur | Sérialisation par composant et inspecteur séparé |
| Hot-reload gameplay/shaders | Cible game distincte et chargement de shader centralisé ; aucune DLL ni surveillance de fichiers |
| Audio | Emplacement futur `engine/audio`, sous-système possédé par Engine |
| Tilemaps 2D | Emplacement futur `engine/tilemap`, données sans dépendance GPU |
| Réseau | Emplacement futur `engine/network`, câblage dans runtime |
| Consoles | Adaptateurs plateforme et GPU confinés, aucune supposition de fichiers desktop dans les loaders |

Les trois derniers emplacements sont réservés dans ce plan uniquement. Aucun
module vide audio/tilemap/network, système physique ni scripting n'est ajouté.
