<!-- Preuves du projet autonome ; ne pas confondre ces résultats avec l'intégration des lots. -->
# Phase 4 — Spike du module de gameplay

Le premier commit de `phase/4-iteration`, `a95e2f1`, contient uniquement le nouvel
AGENTS.md ; sa [CI Windows/Linux est verte](https://github.com/FoxTimew/Somnium/actions/runs/45999924364).
Le spike est sur `feat/hot-reload-spike`, proposé dans la
[PR #7 vers la branche de phase](https://github.com/FoxTimew/Somnium/pull/7).
Il n'est pas raccordé au sandbox. `engine/`, `game/`, leurs CMake, les références
analytiques et leurs provenances sont inchangés depuis main.

## Expérience

Un CMake autonome sous `tools/hot_reload_spike/` compile **les sources existantes**
de core, scene, input et du rejeu dans `spike_engine`, une bibliothèque partagée.
Quatre modules sont construits : v1, v2, start refusé et ABI refusée. L'hôte utilise
SDL_LoadObject/SDL_LoadFunction de SDL 3.4.18 pour charger une copie à nom unique.
Ni fenêtre, ni GPU, ni injection dans le système d'exploitation.

Les versions viennent directement de `cmake/Dependencies.cmake` : EnTT v4.0.0,
GLM 1.0.3, nlohmann/json v3.12.0, tl::expected v1.3.1, spdlog v1.17.0 et Tracy
v0.14.1. CMake 3.31.6, Ninja 1.11.1.3, Python 3.12.12 et les compilateurs sont
ceux de `tools/ci/toolchain.json` (MSVC tools 14.51.36231, clang 18.1.3 Linux).
Le moteur et les modules de l'expérience n'activent ni RTTI ni exceptions.

Le composant contient une chaîne possédée, distance et compteur de ticks ; v2
ajoute 64 octets avant les champs. Chaque génération a un namespace C++ distinct,
mais le codec conserve le nom `game.SpikeState` et sa version 1. Une modification
de disposition C++ ne requiert donc pas de migration d'octets ; changer le schéma
persistant exigerait en revanche une migration explicite ou un refus.

Le test :

1. Exécute v1 pendant 30 ticks depuis un World réel, une identité `player` et un
   composant moteur Name ; vitesse 1 m/s, position attendue 0,5 m.
2. Stoppe, sérialise par les vrais codecs, détruit Gameplay et les codecs, retire
   son stockage par `registry.reset(id)`, charge v2, restaure un World candidat,
   puis start. Sérialisation canonique identique, types/layout distincts.
3. Continue le **même Input et le même rejeu** jusqu'au tick 60 ; vitesse 2 m/s,
   position 1,5 m. La nouvelle logique ne réinitialise pas le compteur du composant.
4. Teste un start qui déplace son candidat de 999 m avant de refuser, une ABI
   invalide, puis un fichier qui n'est pas une bibliothèque. Chaque échec reconstruit
   v2 depuis le même instantané et reprend au tick courant, état identique.
5. Vérifie la libération de W au tick 60 et sa nouvelle pression au tick 65, termine
   à 70 ticks, effectue dix échanges supplémentaires et remplace une publication
   dont la copie est toujours chargée. La sortie du producteur reste accessible.

Le filtre Log défini par l'hôte doit être vu dans chaque module ; avec profiling,
les adresses de `tracy::GetProfiler()` doivent être identiques. Les images restent
mappées, mais les objets, pools et codecs actifs de l'ancien jeu sont retirés.
Le World est conservé **logiquement**, pas à la même adresse ; aucune valeur EnTT
ni adresse empruntée n'est une identité persistante.

## Résultats initiaux

Sur `61e2c221def59e20d4fedc1f56b8682b72947263`, la
[CI de PR](https://github.com/FoxTimew/Somnium/actions/runs/46002991162) exécute le
spike avant le pipeline moteur complet. L'exécution push précédente a été annulée
par concurrency lors de l'ouverture de la PR ; ce n'est pas une erreur de configuration.

| Runner / variante | Configure | Build | Test | Résultat du contrat |
| --- | ---: | ---: | ---: | --- |
| Windows / plain | 15,578 s | 34,235 s | 0,719 s | Validé, layout 48 → 112 octets |
| Windows / profiling | 15,875 s | 53,563 s | 1,031 s | Validé, un seul client Tracy |
| Linux / plain | 27,277 s | 34,073 s | 0,087 s | Validé, layout 40 → 104 octets |
| Linux / profiling | 28,160 s | 71,059 s | 0,308 s | Validé, un seul client Tracy |
| Linux / ASan+UBSan | 29,360 s | 78,902 s | 0,194 s | Validé, LSan actif, aucun diagnostic |

Temps observés avec caches CPM/sccache déjà alimentés, pas des mesures de machine
vierge. Chaque contrat termine avec 70 ticks, trois chemins de rollback, dix cycles
et 15 images retenues. Windows local a également passé plain et profiling avant push.

## Preuve auditée

Le commit `38c8436bc52c47b3f32a2bdddef39d2ebf8b4e0a` retire les tables de fonctions
inutiles de l'hôte, ajoute l'audit de rotation et soumet les 13 unités du spike
à clang-tidy 18. Sa [CI Windows/Linux](https://github.com/FoxTimew/Somnium/actions/runs/46006326742)
est entièrement verte ; [journaux et provenance](validation/phase-4/spike/audited/provenance.json).
Le commit de fusion synthétique exécuté sur les deux runners est
`60e9fb66361318dbe0252af6021876e50c325009`.

Les deux tests passent en plain/profiling sur les deux OS et sous ASan/UBSan/LSan
Linux. L'analyse du spike prend 131,303 s, sans diagnostic ; l'analyse du moteur
inchangé passe également. Debug/release/shipping/profiling, preuves D5/cube/scène,
captures moteur et sanitizers restent verts. Les références et provenances GPU
n'ont pas changé. Le second test est une caractérisation de l'erreur Euler,
**pas** une preuve de restauration exacte pour des rotations arbitraires.

## Ce qui est décidé et ce qui reste à démontrer

Le découpage partagé est viable sur les deux compilateurs, sans dupliquer Log ou
Tracy. Les indices séquentiels EnTT ne constituent pas une ABI : les en-têtes lus
montrent que `type_info::operator==` compare le hash, que le hash utilise le nom
du type avec ces compilateurs, et que `registry.reset(id)` retire réellement le
pool. Aucun groupe, contexte ni abonnement n'est créé dans ce spike.

La génération C++ distincte et la visibilité cachée des symboles privés évitent
de mélanger deux dispositions sous la même identité. Le CMake expérimental utilise
l'export automatique Windows pour les sources existantes ; l'intégration devra
définir les exports moteur intentionnels et l'empreinte ABI complète avant factory.
Le protocole C++ suppose le même compilateur, CRT, options et dépendances : ce
n'est pas une ABI tierce durable.

Tracy utilise des SourceLocationData statiques dans ses macros. Décharger l'image
risquerait de rendre ces chaînes invalides ; les images sont donc conservées jusqu'à
la sortie, limite 64. La preuve confirme l'identité du client et l'exécution des
zones, **pas** une capture avec collecteur connecté. Un processus natif qui plante
dans un candidat ne peut pas être récupéré par ce protocole.

Les rotations du scénario sont identités, aucun modèle n'est demandé. L'exactitude
arbitraire des transforms, l'équilibrage d'assets, les abonnements, le vrai gameplay,
les symboles sous débogueur et la publication surveillée appartiennent aux lots
après validation. Les nouveaux comportements ne sont pas présentés comme déjà livrés.

## Rotations

Audit local du code inchangé `engine/scene/src/rotation.cpp`, MSVC et GLM épinglés :
`q = normalize(quat(.31F, .47F, .63F, .59F))`, puis
`e1 = eulerDegrees(q)`, `q2 = rotationDegrees(e1)`, `e2 = eulerDegrees(q2)`.
Comparaison des bits float : quaternion non identique ; la composante Y d'Euler
passe de `3238929087` à `3238929089`. Les deux autres composantes restent identiques.
Ce constat interdit d'utiliser sans adaptation le format Euler v1 comme instantané
exact pour tous les World. Il ne remet pas en cause les preuves GPU existantes.
L'audit ajouté à la CI confirme `quaternion_bit_exact=0` et
`canonical_euler_stable=0` sur Windows et Linux, y compris sous sanitizers.

L'ADR-0009 propose une représentation persistante v2 en quaternion, avec lecture
v1 et présentation Euler dans l'éditeur. **Ce changement de frontière fichier
attend un accord explicite** ; ADR-0002, conventions.hpp et les fixtures ne sont pas
modifiés. Une autre décision possible est de conserver les fichiers v1 Euler et
d'accepter une tolérance de rechargement disque, tout en gardant les instantanés
mémoire exacts ; cela assouplirait le critère d'identité demandé pour la phase 4.

## Reproduction et stockage

```powershell
./tools/ci/windows.ps1 hot-reload-spike --jobs 2
```

```sh
bash tools/ci/linux.sh hot-reload-spike --jobs 2
```

Même CLI en local et en CI, aucun accès GitHub requis pour l'opération. Les
workflows n'ajoutent aucun shell de build : le pipeline appelle cette opération.
Cache source et compilateur selon les hash déjà définis ; sorties et copies sous
`<racine CI>/runs/<run>/hot-reload-spike/<variante>/`, journaux et durées dans
`logs/`, `run.json` et `SUMMARY.md`. La rétention existante de 14 jours s'applique.
Les preuves textuelles utiles à la revue sont archivées sous
`docs/validation/phase-4/spike/`, aucun binaire ou blob compilé n'entre dans Git.
