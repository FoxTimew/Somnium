<!--
Centralise les conventions du moteur et les invariants à vérifier par phase.
Le futur conventions.hpp en sera la traduction unique dans le code.
-->
# ADR-0002 — Conventions de données, maths et exécution

Date : 20 octobre 2026. Statut : **accepté**.

## Contexte

Les conventions de repère, de représentation, de possession et d'erreurs
traversent tous les modules. Une incohérence obligerait à convertir les assets,
revoir les shaders ou réécrire les interfaces.

Les exigences du cahier des charges sont reprises ci-dessous. Les valeurs de
détail sont acceptées ; leur implémentation fait partie de la phase 0. Le futur fichier
`engine/core/include/somnium/core/conventions.hpp` contiendra leurs constantes
et commentaires de référence ; les autres modules les utiliseront.

## Décision

### Repère, unités et matrices

| Sujet | Convention |
| --- | --- |
| Monde et caméra | Repère main droite, +Y en haut, caméra dirigée vers -Z |
| Distances et temps | Mètres et secondes ; horloge de simulation monotone |
| Rotations | Quaternions normalisés ; Euler seulement aux frontières UI/fichiers |
| Profondeur clip | Intervalle 0..1 ; near à 0, far à 1 |
| Projection | `glm::perspectiveRH_ZO` et `glm::orthoRH_ZO` |
| Stockage des matrices | GLM column-major ; aucune transposition à l'upload |
| Application aux vecteurs | Vecteurs colonnes ; clip = projection × vue × monde × position |
| Transform locale | Matrice T × R × S |
| Transform monde | Monde du parent × locale ; racine : monde = locale |
| C++ / HLSL | Layout de transfert explicite, matrices HLSL `column_major`, multiplication matrice-vecteur cohérente |

Les définitions `GLM_FORCE_DEPTH_ZERO_TO_ONE`, `GLM_FORCE_RADIANS` et
`GLM_FORCE_EXPLICIT_CTOR` sont définies depuis CMake pour toutes les unités
utilisant GLM, y compris gameplay et tests. Aucun fichier ne les redéfinit.
« Globalement » signifie ici une configuration commune transmise par les cibles,
pas une injection de flags incompatibles dans toutes les dépendances.

Les unités n'effectuent pas de conversion de repère à l'import glTF. Les angles
d'API internes sont en radians. Le schéma JSON/UI annoncera explicitement son
unité d'Euler ; aucune convention d'ordre Euler implicite ne sera ajoutée avant
l'ADR-0006.

Les layouts GPU ne reposent pas sur le padding implicite d'une struct C++ :
tailles, offsets et alignements sont contrôlés pour chaque bloc de transfert.
Les matrices mathématiques ne sont pas confondues avec la disposition d'un
`float3` dans un buffer uniforme.

### SDL GPU : en-tête épinglé vérifié au point de contrôle de phase 1

L'en-tête
[`SDL_gpu.h` de `release-3.4.18`](https://github.com/libsdl-org/SDL/blob/829a65d769d935c4852f8159e964312c0957260a/include/SDL3/SDL_gpu.h)
a été relu dans le cache CPM le 21 octobre 2026, au commit
`829a65d769d935c4852f8159e964312c0957260a`. Il décrit :

- NDC : coin bas gauche (-1, -1), coin haut droit (1, 1), profondeur 0..1.
- Viewport : origine en haut à gauche, +Y vers le bas.
- UV : origine en haut à gauche, +Y vers le bas.
- Conversion des différences de backend par SDL, sans inversion Y à ajouter
  dans les shaders de l'application.

Le repère monde main droite reste distinct de ces coordonnées après projection.
Les deux ne doivent pas être confondus. Aucune inversion Y propre à Vulkan ni
conversion de Z supplémentaire ne sera ajoutée localement.

Pour un viewport d'origine (x0, y0), de largeur W et hauteur H, ces conventions
donnent x = x0 + (ndcX + 1) × W/2 et y = y0 + (1 - ndcY) × H/2. La dimension
provient des pixels de la cible de rendu, sans supposer qu'elle égale les
dimensions logiques de la fenêtre.

Les enums lus sont `SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE` / `CLOCKWISE` et
`SDL_GPU_CULLMODE_NONE` / `FRONT` / `BACK`. Choix accepté avec l'ADR-0004 :
front face COUNTER_CLOCKWISE, culling BACK, profondeur LESS et clear à 1. Les
backends épinglés traduisent directement ces choix en leurs conventions natives ;
le résultat est vérifié par le [test de référence](../GPU_VALIDATION.md)
hors écran : triangle CCW asymétrique, winding opposé rejeté, texture à quatre
couleurs, repères UV/NDC et recouvrement à deux profondeurs. La lecture de l'en-tête
est acquise. D5 confirme ces choix sur **Vulkan/lavapipe, D3D12/WARP et Metal**, avec
zéro pixel différent et les dix centres exacts dans le
[run de preuve](https://github.com/FoxTimew/Somnium/actions/runs/39592617094).
Metal est validé sous macOS 15 avec les couches API et shader actives ; le défaut
du runner macOS 26 est documenté dans le protocole. Le cube glTF complète cette
preuve en phase 2.

Les bindings SDL GPU seront documentés avec l'ADR-0003 et un en-tête HLSL commun.
Ils ne seront pas inventés depuis les conventions d'un backend natif.

### Transforms et hiérarchie

`Transform` contient position, quaternion et échelle locaux, ainsi qu'une
matrice monde dérivée. La hiérarchie parent/enfant est présente dès le MVP ;
les portes sont des meshes enfants de pivots.

La propagation suit un ordre parent-enfant et rejette les cycles. Le cache monde
n'est pas sérialisé comme source de vérité. Les modifications structurales sont
réalisées à des frontières contrôlées, sans invalider un parcours d'extraction.

L'état précédent et courant est conservé pour les transforms rendus.
L'interpolation se fait dans les TRS locaux, puis la hiérarchie est recomposée.
Une téléportation initialise les deux états à la même valeur. Les identités
d'entités et handles techniques ne sont pas des identifiants persistants JSON.

### Erreurs et assertions

`somnium::Result<T>` est un alias de `tl::expected<T, Error>`. `Error` transporte
un code typé et un diagnostic possédé ; pas de `string_view` vers un buffer SDL
ou un temporaire. Les catégories d'erreurs sont ajoutées au besoin réel, sans
catalogue spéculatif. Les fonctions faillibles signalent leur échec dans la
signature et les résultats sont marqués pour ne pas être ignorés.

| Situation | Traitement |
| --- | --- |
| Fichier absent, JSON invalide, asset non conforme, shader manquant | `Result<T>` ou état `Failed` documenté d'une demande d'asset acceptée |
| Invariant de programme, mauvais handle interne | `SOMNIUM_ASSERT`, fatal en debug |
| Invariant qui doit rester vérifié en production | `SOMNIUM_VERIFY`, fatal dans tous les builds |
| Manque de mémoire d'un conteneur standard | Politique acceptée : fatal, sans promesse de récupération |
| Erreur pendant la destruction | Aucun throw ; opération explicite préalable si récupération nécessaire, diagnostic de nettoyage sinon |

`SOMNIUM_ASSERT` est retiré en release et n'évalue pas son expression :
aucun effet de bord ne peut y être nécessaire au fonctionnement.
`SOMNIUM_VERIFY` évalue son expression exactement une fois dans tous les builds.
Les deux macros indiquent le lieu de l'échec. Aucune ne sert à traiter une
entrée utilisateur malformée.

Un handle périmé issu d'une entrée externe est validé par une API faillible
avant tout accès ; les assertions d'accès interne ne remplacent pas ce contrôle.
Le code n'appelle pas aveuglément `value()` sur un `Result` sans succès.
Avec `JSON_NOEXCEPTION`, une erreur n'est pas automatiquement transformée en
`Result` : parsing sans exception, contrôle des types, des bornes et de la
version précèdent toute conversion.

### Handles et possession

`Handle<Tag>` représente exactement un index 32 bits et une génération 32 bits,
sans dépendance à la taille d'un pointeur. Sentinelles retenues :
index maximal réservé à l'invalidité et génération zéro réservée ; première
génération utilisable égale à un. Des assertions de taille vérifieront le contrat.
Ce format est une représentation mémoire, pas un format de sauvegarde.

Une vérification de nullité ne prouve pas la validité d'un handle : seul le
stockage propriétaire peut confirmer occupation et génération. Libérer puis
réutiliser un slot invalide l'ancien handle. Un slot est retiré définitivement
avant le débordement de génération ; aucune ancienne identité ne ressuscite.
Les handles de domaines différents ne sont pas convertibles implicitement.

La possession exclusive utilise la valeur ou `std::unique_ptr`.
Les paramètres empruntés utilisent références, `std::span` et `std::string_view`
avec durée de vie précisée. Aucun pointeur brut durable vers un asset ni
`std::shared_ptr` sans un nouvel ADR.

Les emprunts courts n'interdisent pas les pointeurs natifs privés nécessaires
aux adaptateurs SDL ; ils sont encapsulés et libérés par leur propriétaire.
Le renderer ne conserve aucun pointeur vers une entité ou un composant.

### Mémoire et profilage

Les conteneurs standard constituent la base. Sur les chemins chauds, les
conteneurs `std::pmr` sont employés lorsque cela ne change pas le contrat ;
leur ressource mémoire est injectée et survit aux conteneurs. Aucune modification
globale de la ressource PMR par défaut, aucun allocateur custom ni arène au MVP.

Les capacités de simulation, extraction et rendu sont réservées au chargement
et réutilisées. Une croissance à l'arrivée d'une nouvelle scène ou ressource
est isolée et documentée ; « aucune allocation en boucle » s'applique au régime
stabilisé, sans cacher les allocations internes des dépendances ou du driver.
Les logs répétitifs ne doivent pas ajouter des allocations à chaque frame.

`SOMNIUM_PROFILE_*` encapsule zones CPU, marqueurs de frame et notifications
d'allocation/libération. Hors preset profiling, les macros sont vides et leurs
arguments ne sont pas évalués. Les hooks mémoire suivent des allocations
réelles et équilibrées ; ils ne supposent pas qu'une réservation de capacité
correspond systématiquement à une allocation.

Le périmètre initial des mesures mémoire doit être annoncé. Il ne couvre pas
automatiquement toutes les allocations de la STL, de SDL ou d'un driver.
L'instrumentation globale de `new/delete` n'est pas introduite implicitement.
Les durées GPU sont exclues selon D4 de l'ADR-0001 ; les durées CPU
d'enregistrement ne seront jamais étiquetées comme durées GPU.

### Temps, inputs et threading

Le temps monotone est mesuré indépendamment de l'heure civile. Le pas fixe
initial est `1/60 s`, avec un delta entrant limité à `250 ms` et huit
pas maximum par itération. Les pas excédentaires sont comptabilisés puis
abandonnés en conservant le reste fractionnaire. Les paramètres ont des noms
et une définition unique dans les conventions.

Les tests d'accumulateur couvrent zéro delta, plusieurs pas, une très longue
pause, la conservation du reste et l'intervalle d'alpha. Ils injectent le temps
et n'attendent pas une horloge réelle. Les entrées temporelles invalides sont
rejetées par le contrat de configuration ou signalées comme invariants.

Les actions et axes sont résolus depuis leurs noms à la configuration.
Le gameplay n'utilise aucun code de touche. Les transitions restent disponibles
jusqu'au pas fixe qui les consomme ; un rattrapage ne répète pas un appui unique.

Un seul thread de simulation au MVP n'autorise ni état global mutable ni
transmission de références à durée de vie implicite. `Engine` possède les
sous-systèmes et reçoit les dépendances extérieures explicitement.
L'état interne des dépendances tierces reste confiné à leurs adaptateurs ;
leurs contraintes de thread sont respectées, sans être propagées aux données
de scène ou aux draw packets.

### Style et organisation

- Namespace racine `somnium`, sous-namespace par module pour les services ;
  composants ECS plats directement dans `somnium`.
- Types `PascalCase`, fonctions et variables `camelCase`, membres privés `m_`,
  constantes `kPascalCase`, macros `SOMNIUM_*`, fichiers `snake_case`.
- En-têtes publics sous `engine/<module>/include/somnium/<module>/`,
  implémentations sous `src/`, tests sous `tests/`, `#pragma once`.
- Chaque fichier commence par un commentaire de deux à cinq lignes indiquant
  son rôle et, si nécessaire, le choix non évident. Les explications longues
  vont dans les ADR. Pour un format strict tel que JSON, qui n'accepte pas les
  commentaires, cette information utilisera un champ descriptif accepté par
  son schéma ou la documentation adjacente, sans rendre le fichier invalide.
- `.clang-format`, `.clang-tidy` et `CODING_STYLE.md` seront livrés après
  validation de ce jalon ; leur contenu devra respecter cette convention.

### Assets, dépôt et distribution

Les scènes et configurations JSON portent une version explicite. Les assets
sources sont séparés des produits de build. Git LFS suit les formats binaires
sous `assets/` ; HLSL, JSON et glTF textuel restent du texte ordinaire. Les
fichiers `.bin`, `.glb`, PNG et JPEG font notamment partie du suivi LFS.

Les dossiers de build, le cache CPM, les shaders générés, les fichiers d'IDE
et les captures/caches Tracy sont exclus par `.gitignore` dès l'implémentation
de phase 0. Aucun token, clé ni secret n'est fourni en exemple ou commité.

Les branches sont `phase/N-<nom>` ; `main` ne reçoit que les phases validées.
Pas de fusion ni tag `phase-N` avant validation explicite et preuves de CI.
Le dépôt initial vide et l'absence de base `main` sont consignés dans
l'architecture, sans fabriquer un historique pour masquer cette situation.

## Alternatives écartées

Des conversions locales à l'import, une inversion Y par backend dans les
shaders ou une transposition automatique masqueraient une incohérence globale.
Elles sont donc exclues. Les matrices monde interpolées coefficient par
coefficient et les handles sérialisés compromettraient respectivement les pivots
et la persistance.

Les exceptions pour les erreurs attendues, les sentinelles numériques non
typées, les singletons de sous-systèmes et la possession partagée généralisée
contredisent les conventions imposées.

## Conséquences et vérification

Les tests initiaux portent sur `Result`, l'invalidation/réutilisation des handles
et l'accumulateur borné. Les tests de transforms, de persistance et d'interpolation
arrivent avec leurs modules, sans fonctions de remplacement anticipées.

La vérification visuelle des conventions SDL est une obligation de phase 1,
suivie du modèle glTF de référence en phase 2. Aucune observation de cette
revue documentaire ne doit être présentée comme un test graphique réussi.

Les arbitrages D1 à D5 de l’ADR-0001 sont acceptés : SHA complet pour toute
dépendance sans release ; intégrations ImGui séparées de platform/gpu ; MSL
produit hors ligne puis chargé par SDL sans compilateur HLSL dans le moteur ;
Tracy CPU/mémoire, compteurs et marqueurs GPU sans durées GPU ; preuve de rendu
hors écran avec tolérance et preuve locale explicite si device CI indisponible.
Le preset release garde l’UI, shipping l’exclut. Les licences permissives sans
copyleft sont admises ; les compilateurs/SDK de plateforme sont hors de cette règle.
Le socle initial main et la branche de phase suivent la procédure Git autorisée.

Preuves : hiérarchie, persistance et interpolation aux phases concernées.
