<!--
Consigne les résultats du spike et les détails de la proposition ADR-0003.
Sépare les commandes déjà exécutées des interfaces à implémenter après accord.
-->
# Shaders — spike et annexe de l'ADR-0003

## État et sources

L'étape a implémente `cmake/Shaders.cmake`, le manifeste et les tests
d'incrémentalité, en plus du bootstrap isolé. Les commandes
de bootstrap ci-dessous fonctionnent indépendamment de CMake moteur. La fonction
`somnium_add_shaders`, le manifeste et l'en-tête HLSL commun sont **acceptés** et
leur réalisation est l'étape a, suivie de la preuve GPU hors écran à l'étape b.
Aucune création de device ni preuve de rendu n'appartient à ce spike.

Le [verrou machine](../tools/shader_toolchain/lock.json) contient les URLs,
révisions et SHA-256 utilisés ; aucun checkout de branche :

| Composant | Version | Commit complet |
| --- | --- | --- |
| SDL | release-3.4.18 | `829a65d769d935c4852f8159e964312c0957260a` |
| SDL_shadercross | sans release | `1ff05bec573988a98ef9e0260b4da44f512b8367` |
| SPIRV-Cross | vulkan-sdk-1.4.363.0 | `f11ba9f0b21ba8fc15153d50a2a1ae31ab1cf8f7` |
| DXC | v1.9.2609 | `01b62ad47db7dee3dd33f90e7339cf9e860b3f93` |

Les archives Windows/Linux et leurs empreintes sont celles de la
[release Microsoft](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2609),
vérifiées avec les digests de l'API GitHub. Cette release n'a pas d'archive macOS.
Le build source utilise les sous-modules aux gitlinks de ce commit, les paramètres
amont `PredefinedParams.cmake` et les cibles `dxc`, `dxcompiler`, `dxildll`.
Le CLI utilise SDL et SPIRV-Cross statiques, DXC partagé ; le dossier installé
contient aussi les bibliothèques nécessaires. Il ne dépend pas d'un DXC du PATH.

## Comparaison des trois voies

| Voie | Preuve obtenue | Coût/limite | Décision |
| --- | --- | --- | --- |
| Release DXC officielle | CLI compilé et exécuté sur Windows et Linux ; vertex/fragment vers SPIR-V, DXIL, MSL et réflexion | Archives 32 287 592 / 13 297 763 octets ; aucun binaire macOS | Retenue Windows/Linux x64 |
| DXC du Vulkan SDK 1.4.363.0 | Essai CI archivé, téléchargement HTTP 403 ; endpoint automatisé documenté également refusé localement | Taille annoncée ≈349 Mo ; version interne, bibliothèques et compilation non mesurées | Écartée, pas une preuve de compatibilité DXIL |
| Sources DXC v1.9.2609 | Construction macOS et essais du CLI dans le job dédié | Build LLVM/DXC à froid, cache obligatoire pour les itérations | Retenue macOS ; pas de build source Windows/Linux mesuré |

LunarG décrit sa copie DXC comme destinée à Vulkan et sans support DirectX dans
sa [documentation DXC](https://vulkan.lunarg.com/doc/view/latest/windows/DXC.html).
Ce document courant ne prouve pas le contenu de l'archive 1.4.363.0 inaccessible.
Le probe conservé `--provider vulkan-sdk` reste exécutable manuellement, vérifie
le SHA-256 puis mesure SPIR-V/DXIL et les bibliothèques ; toute acquisition
impossible échoue. Il n'est plus une voie requise après le choix des fournisseurs.
L'[essai rejeté](https://github.com/FoxTimew/Somnium/actions/runs/39574749906/job/119137373766)
reste rouge et conservé, sans `continue-on-error` ni réécriture du résultat.

## Mesures reproductibles

Le premier [run du spike](https://github.com/FoxTimew/Somnium/actions/runs/39574749906)
correspond à `23dcff5d62138a7c1fdbd31d528a9327ace9ed7a`.
Durées murales de `build.py`, incluant téléchargement, configure, build,
installation et essais ; hors provisionnement du runner et restauration du cache.
Trois compilations parallèles, édition de liens DXC limitée à une.

| Runner / fournisseur | Total à froid | Configure CLI | Build CLI | Build DXC |
| --- | ---: | ---: | ---: | ---: |
| Windows / officiel | 111,016 s | 21,343 s | 82,782 s | binaire officiel |
| Linux / officiel | 75,818 s | 24,061 s | 47,958 s | binaire officiel |
| macOS arm64 / sources | 816,474 s | 29,521 s | 33,080 s | 702,267 s |

Les trois jobs CLI ont réussi. Le run global initial reste rouge uniquement à
cause du probe du fournisseur SDK rejeté. DXC officiel annonce 1.9.2609.5 ;
le build source annonce 1.9.0.1 avec suffixe `01b62ad4`, car les métadonnées du
packaging Microsoft ne sont pas injectées. Le commit complet vérifié, pas cette
chaîne marketing, identifie la version source utilisée.

Chaque répertoire de preuve `shader-toolchain` du run contient `result.json`, les
logs par commande, les versions, les sorties des deux shaders, leurs JSON et
l'inventaire des couches du runner. Les essais vérifient signatures SPIR-V/DXIL,
déclarations MSL, quatre compteurs de réflexion, échec sur HLSL invalide et échec
explicite au configure si le CLI manque. `--help` seul ne compte pas comme preuve.

Le cache auto-hébergé conserve sources, téléchargements, build de la chaîne et
installation sous `<racine-CI>/cache/shaders/<hash-recette-verrou-outils>`.
`verified-lock.json` version 2 couvre le verrou et la recette normalisée LF ;
il n'est écrit qu'après réussite. Une installation réutilisée repasse **tous**
les essais. Une ancienne marque sans recette provoque une reconstruction,
notamment pour ajouter le CLI de réflexion. La politique actuelle de stockage
et de nettoyage est décrite dans [CI_RUNNER.md](CI_RUNNER.md).

La [relance des fournisseurs retenus](https://github.com/FoxTimew/Somnium/actions/runs/39577126359)
est verte sur les trois OS : cache restauré et essais rejoués en **1,141 s sur
Windows**, **1,096 s sur macOS** (script seul). Linux a changé d'empreinte de
runner : cache manqué, reconstruction complète et essais réussis en **89,497 s**.
Ces observations ne sont pas une promesse de cache hit sur chaque machine.

## Utilisation locale et configuration bloquante

Prérequis : Git, Python 3.12, compilateur C/C++ ; MSVC dans un terminal développeur
sous Windows, GCC/Clang sous Linux, Xcode/AppleClang sous macOS. CMake 3.31.6 et
Ninja 1.11.1.3 sont ceux de CI :

```sh
python -m pip install cmake==3.31.6 ninja==1.11.1.3
# Windows / Linux x64
python tools/shader_toolchain/build.py --provider official --jobs 3
# macOS, architecture native
python tools/shader_toolchain/build.py --provider source --jobs 3
```

Installation sous `.cache/shader-toolchain/install/<provider>/bin` et `lib`.
Le projet isolé refuse les sources/DXC absents avec `FATAL_ERROR`. Le mini-projet
`verify` exige le chemin absolu du CLI et l'exécute au configure ; le workflow
teste aussi un chemin inexistant et exige le diagnostic attendu. Le configure moteur ne lance jamais `build.py` : il lit la variable CMake
`SOMNIUM_SHADERCROSS_EXECUTABLE`, par défaut le chemin installé du provider
`source` sur macOS, `official` ailleurs, et cherche `dxc` à côté du CLI. Il vérifie
**CLI et DXC pour les depfiles**, avec erreur fatale et commande de bootstrap ; une
option de build d'outils n'autorisera pas l'absence des produits nécessaires.

Constat local Windows du 21 octobre : le CLI a été compilé mais son lancement
est refusé avec « Une stratégie de contrôle d’application a bloqué ce fichier. »
L'exécution du même outil est validée en CI Windows ; aucune politique locale
n'a été modifiée. Ce cas concerne le nouvel outil, pas `ctest --preset release`,
validé localement **6/6** ; l'ancienne limitation du test core est retirée.

## Commandes de génération proposées

Exemple vertex, mêmes options que les essais déjà exécutés (remplacer `vertex`
par `fragment`, `vsMain` par `psMain` pour le pixel shader). Les chemins du CLI viennent du bootstrap :

```sh
shadercross triangle.vert.hlsl -s HLSL -t vertex -e vsMain -I shaders -d SPIRV -o triangle.vsMain.vert.spv
shadercross triangle.vert.hlsl -s HLSL -t vertex -e vsMain -I shaders -d DXIL -o triangle.vsMain.vert.dxil
shadercross triangle.vert.hlsl -s HLSL -t vertex -e vsMain -I shaders -d MSL --msl-version 2.3.0 -o triangle.vsMain.vert.msl
shadercross triangle.vsMain.vert.spv -s SPIRV -t vertex -e vsMain -d JSON -o triangle.vsMain.vert.reflection.json
dxc triangle.vert.hlsl -T vs_6_0 -E vsMain -I shaders -M -MF triangle.vsMain.vert.d -Fo triangle.vsMain.vert.spv
```

Le dernier mécanisme a été vérifié localement sur la fixture : le depfile contient
la source et `probe_common.hlsli`. Pour fragment, profil `ps_6_0`. Le CLI épinglé
accepte un seul `-I` : une racine d'includes, chemins relatifs sous celle-ci ;
defines `-Dnom=valeur` identiques pour chaque commande et le depfile.
DXC 1.9.2609 écrit ces chemins sans échappement
([source épinglée](https://github.com/microsoft/DirectXShaderCompiler/blob/01b62ad47db7dee3dd33f90e7339cf9e860b3f93/tools/clang/tools/dxcompiler/dxcompilerobj.cpp#L882)).
Le générateur conserve la liste DXC, normalise les séparateurs et échappe espaces,
`#`, `$` et `:` pour le depfile CMake/Ninja. Le test avec chemins contenant des
espaces vérifie qu'un build sans changement ne régénère aucun produit.
Pas de `--cull` ; pas de `--debug` automatiquement associé au preset debug, car
l'aide du CLI avertit que ces shaders nécessitent un debugger graphique attaché.
Le mode debug du device reste indépendant. MSL **2.3.0** est passé explicitement.
`CMAKE_OSX_DEPLOYMENT_TARGET=13.3` est le minimum moteur accepté pour conserver
`std::format` avec la libc++ Apple : son `to_chars` flottant exige macOS 13.3
([disponibilité Apple](https://developer.apple.com/xcode/cpp/)). Le configure moteur
rejette une cible inférieure et indique la correction du cache. MSL 2.3 correspond à macOS 11
([Apple](https://developer.apple.com/documentation/metal/mtllanguageversion/version2_3)).
Le backend SDL épinglé exige au minimum macOS 10.14 et Mac2 par défaut
(`METAL_PrepareDriver` et `METAL_CreateDevice` dans SDL_gpu_metal.m). Une cible
plus récente conserve ce sous-ensemble MSL suffisant et compatible, sans dépendre
du défaut du CLI. Le module shader et ses fixtures autonomes, qui ne lient pas le
moteur, gardent le plancher de langage macOS 11 ; le manifeste du moteur consigne 13.3.

Interface implémentée : `TARGET` consommateur, `NAME`, `SOURCE`, `ENTRYPOINT`,
`STAGE`, `INCLUDE_DIRECTORY`, `DEFINES`. Entrées imposées : `vsMain` et `psMain` ;
`main` est interdit et `MSL_ENTRYPOINT` est supprimé. Rejeter les collisions
de nom, étapes non supportées, ressources/bindings invalides et options ambiguës.
Sorties `${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/...`, jamais dans les
sources ; `DEPFILE` pour includes transitifs et `DEPENDS` pour source, outils,
verrou et recette. Vérifier ajout/retrait/modification d'include, absence de
rebuild sans changement, changement d'options et suppression d'un produit.

Un manifeste JSON version 1 rassemble fichiers relatifs, hash des produits,
étage, points d'entrée par format et compteurs. SPIR-V/DXIL conservent l'entrée
HLSL. La génération exige que la fonction MSL porte exactement le même nom
`vsMain`/`psMain`, ou échoue. La réflexion CLI ne fournit pas ce nom ; aucune
seconde valeur d'entrée ni remappage implicite n'est accepté. Valider également uniformes/resources non
utilisés et variantes pour prévenir une divergence des compilations par format.

Copie incrémentale dans `<dossier-exécutable>/assets/shaders` pour le build local,
installation au même emplacement relatif avec les trois formats et le manifeste.
Le runtime utilise la racine de données de platform, jamais le répertoire courant.
Il adapte les données validées vers une description GPU sans JSON ni SDL publics.
`SDL_GetGPUShaderFormats` est intersecté avec les artefacts disponibles ; format
obligatoire absent ou compteurs incohérents : échec explicite avant le dessin.

## Bindings de l'en-tête HLSL commun

Autorité : [`SDL_CreateGPUShader`](https://github.com/libsdl-org/SDL/blob/829a65d769d935c4852f8159e964312c0957260a/include/SDL3/SDL_gpu.h#L2700).
Pour chaque étage, S = textures échantillonnées, T = storage textures en lecture,
B = storage buffers en lecture. Indices denses à partir de zéro, sans tableaux
non bornés ni bindings éliminés. Le compute et les UAV ne font pas partie du MVP.

| Ressource | Vertex HLSL | Fragment HLSL | SPIR-V | MSL produit |
| --- | --- | --- | --- | --- |
| Textures échantillonnées | `t0..t(S-1), space0` | `t0..t(S-1), space2` | set 0 / 2, début | `texture(0..S-1)` |
| Samplers associés | `s0..s(S-1), space0` | `s0..s(S-1), space2` | associés aux textures correspondantes | `sampler(0..S-1)` |
| Storage textures lecture | `tS..t(S+T-1), space0` | mêmes t, `space2` | après textures échantillonnées | `texture(S..S+T-1)` |
| Storage buffers lecture | `t(S+T)..t(S+T+B-1), space0` | mêmes t, `space2` | après storage textures | buffers après uniformes |
| Uniform buffers | `b0.., space1` | `b0.., space3` | set 1 / 3 | `buffer(0..)` |

Les macros de `assets/shaders/conventions.hlsli` expriment les espaces par étage et les offsets
des catégories ; les shaders déclarent les ressources, pas une seconde copie
des compteurs C++. Utiliser `column_major` et `mul(matrice, vecteur)`, conformément
à l'ADR-0002. Entrées vertex utilisateur `TEXCOORD0`, `TEXCOORD1`, etc. ; valeurs
système `SV_*`. En MSL, laisser shadercross/`stage_in` gérer les vertex buffers,
réservés à partir de `buffer(14)` par SDL.

Le JSON de réflexion fournit `samplers`, `storage_textures`, `storage_buffers`,
`uniform_buffers`, mappés aux quatre champs `num_*` de `SDL_GPUShaderCreateInfo`.
Il fournit aussi les entrées/sorties ; les compteurs sont vérifiés au build puis
bornés/validés au chargement. Les bindings manuels restent contrôlés par le
compilateur et les tests ; la réflexion ne justifie aucun placement arbitraire.

Le test CTest `somnium_shader_build_tests` utilise le vrai CLI et couvre les
chemins avec espaces, no-op, includes transitifs/ajoutés/retirés, defines, produit
supprimé, installation et erreurs de source/entrée/outil. Le configure ne lance
aucun bootstrap. Le pipeline auto-hébergé appelle le bootstrap avant les builds
et réutilise son installation sur disque, indexée par verrou et recette ; aucun
cache ni artefact GitHub ne transmet les outils entre jobs.

## Rechargement en iteration

Le bootstrap construit également `spirv-cross --revision` / `--reflect`, depuis
la même source épinglée. Chaque étage conserve les compteurs shadercross et une
réflexion structurelle normalisée (`.layout.json`) : types développés, offsets,
strides, tailles, bindings et interfaces. Noms de debug et identifiants internes
SPIR-V sont exclus ; ordre des membres conservé. Les outils de réflexion restent
hors du moteur.

`somnium_add_shader_program(TARGET somnium_renderer NAME model STAGES
vertex=modelVertex lambert=modelFragment ambient=modelAmbient)` publie les trois
étages ensemble. `<bin>/assets/shaders/model.generations/<SHA256>/` contient leurs
18 fichiers immuables. `model.program.json`, remplacé atomiquement en dernier,
référence tous les formats, réflexions et empreintes. Une génération incomplète
ne remplace jamais la précédente. Les anciennes générations restent présentes
pendant la session ; leur suppression appartient au nettoyage du build.

```sh
python tools/shaders/watch.py --build build/iteration --config Debug
build/iteration/bin/sandbox --watch-shaders on
```

Le watcher externe suit les depfiles et les racines d’include, regroupe les
écritures pendant 250 ms, puis invoque la cible CMake du programme. Ne pas lancer
deux builds Ninja simultanés dans le même répertoire. Une erreur conserve le
manifeste et publie un diagnostic borné dans `model.status.json`. `--once` exécute
exactement une génération et rend son code de succès/échec.
Sous Windows, un lecteur peut bloquer brièvement le renommage : le producteur
réessaie pendant deux secondes au plus, sans jamais tronquer le manifeste actif.

Le service platform lit au plus quatre fois par seconde chaque publication.
À la frontière de pas, runtime demande au renderer de vérifier schéma, chemins,
tailles, SHA-256 et signature avant toute allocation GPU. Les octets vérifiés
sont conservés en mémoire pendant la création pour éviter une seconde lecture
mutable. Les quatre pipelines (Lambert/ambiant, winding normal/miroir) sont
échangés ensemble ; leur format est celui du renderer. En cas d’échec du dernier
candidat, tous les anciens pipelines restent actifs. Layout incompatible :
« redémarrage requis ». Aucune surveillance sans l’option, ni dans shipping.

`tools/ci/{windows.ps1,linux.sh} shader-reload-proof --backend <backend> --run-id
<run>` utilise le binaire iteration du run. La preuve garde le processus ouvert
pendant les publications : blanc → rouge → artefact invalide → layout différent
à compteurs égaux → blanc. Quarante images exactes couvrent les quatre pipelines
et les formats UNORM/sRGB. Géométrie, couleurs et sondes sont dans
`engine/renderer/tests/reload_scene.json` ; l’oracle calcule un rectangle sur
frontières de pixels. Les deux références dédiées et leur provenance sont
versionnées sous `engine/renderer/tests/reference/shader_reload_*` (PNG sous LFS).
Régénération explicite : `python tools/shaders/reload_reference.py --regenerate`,
interdite en CI ; CTest emploie `--verify`. Images, générations et journaux restent
dans le run CI.
