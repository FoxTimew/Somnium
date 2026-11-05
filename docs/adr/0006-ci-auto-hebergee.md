<!-- Infrastructure approuvée par la demande du 5 novembre ; preuves à compléter avant fusion. -->
# ADR-0006 — CI auto-hébergée

Date : 5 novembre 2026. Statut : **accepté** (infrastructure, validation en cours).

## Contexte

Les limites du compte GitHub ont interrompu la phase 2. Le même poste héberge un
runner Windows x64 avec Radeon RX 6600M et un runner Ubuntu 24.04 sous WSL2.
Les opérations doivent aussi fonctionner hors GitHub.

## Décision

Windows/D3D12/Vulkan matériel et Linux/lavapipe sont **requis** : quatre presets,
format, architecture, D5/cube, présentation ; clang-tidy 18 et ASan/UBSan sur Linux.
Un job par OS exécute les scripts `tools/ci/`, du contrôle court au build long,
avec délais et annulation des runs dépassés par branche. Windows fonctionne
comme processus de session interactive, tâche planifiée à l'ouverture de session
d'un compte standard dédié ; fenêtres et captures restent obligatoires. Un mode
service explicitement choisi marque ces preuves **non validé**, à produire à la main.

macOS devient **best effort**, workflow manuel séparé, sans bloquer ces deux jobs :
build sur macos-latest ; build et Metal sur macos-15, cible 13.3. Chaque job construit
ses outils et binaires sur place, sans transfert d'artefact. **WARP n'est plus prouvé** ;
les preuves historiques sont conservées mais ne décrivent pas les nouveaux commits.

Versions et SHA dans `toolchain.json` et le verrou shader. CPM, shadercross et
sccache résident sur le disque du runner, indexés par hash. Chaque run conserve
binaires, captures, journaux et résumé sur disque, rétention **14 jours** ; aucun
`actions/cache`, upload ou download d'artefact. Le résumé publie les chemins et
résultats dans le journal et `GITHUB_STEP_SUMMARY`. Sous WSL, sources/builds/caches
sur ext4 ; CTest transmet CMake/Ninja natifs au test shader (timeout 120 s conservé).

Provisionnement administratif séparé de l'exécution non privilégiée. Dépôt privé,
contributeurs de confiance seulement, garde rejetant les PR de forks dans tous
les workflows, jamais `pull_request_target`, approbation requise pour contributeurs
externes, token `contents: read`, credentials checkout non persistés, aucun secret
de déploiement. Un runner persistant n'est pas une sandbox contre du code hostile.

## Alternatives

Runners hébergés requis : coût et blocage inchangés. Windows en service : pas de
preuve de présentation. Builds WSL sous `/mnt/c` : pénalité I/O et délais observés.
Transfert d'artefacts : inutile entre jobs locaux ; builds macOS indépendants.

## Conséquences

Disponibilité liée au poste allumé, à la session Windows ouverte et à WSL.
Disques, comptes, enregistrement et commandes dans [CI_RUNNER.md](../CI_RUNNER.md).
Les ADR scène/input ultérieurs prennent les numéros 0007/0008.

Preuves : local Windows, D5/cube et captures D3D12/Vulkan Radeon validés, 18/18 tests debug dont shader en 11,57 s sans blocage de contrôle d'application ; CI des deux runners et SHA final encore à valider avant fusion.
