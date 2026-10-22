<!-- Pièces du point de contrôle renderer ; aucune capture GPU ne sert d'oracle. -->
# Phase 2 — cube hors écran, point de contrôle

Ce jalon est **validé**. La suite input/caméra/sandbox et ses limites de validation
figurent dans le [bilan de fin de phase préparé localement](FIN_DE_PHASE.md).

La [CI de preuve](https://github.com/FoxTimew/Somnium/actions/runs/39739082282)
est entièrement verte au commit `073413d1466096dc8ce49528b79013c386dd9b78`.
Les captures CI ci-dessous sont ses artefacts originaux, sans retouche. Le rapport
de chaque backend conserve SHA du binaire, manifests, scène, fixture et image.
Ces pièces archivent le premier passage du cube normal. La CI de la
[PR de phase](https://github.com/FoxTimew/Somnium/pull/4) teste aussi la variante
miroir X ajoutée ensuite : ses artefacts `cube-<backend>` contiennent les deux
images et comparaisons du SHA courant. La référence PNG commune reste identique.

| Backend | Device et origine | Pixels différents / écart maximal | Centres exacts | Validation | Pièces |
| --- | --- | --- | --- | --- | --- |
| Vulkan | llvmpipe LLVM 20.1.2, Mesa 25.2.8 ; CI ubuntu-24.04 | 0 / 0 | 5/5 | Khronos + synchronisation | [PNG](ci-vulkan/cube.png), [rapport](ci-vulkan/result.json), [journal](ci-vulkan/validation.txt) |
| D3D12 | Microsoft Basic Render Driver, WARP ; CI Windows | 0 / 0 | 5/5 | Couche debug D3D12 | [PNG](ci-direct3d12/cube.png), [rapport](ci-direct3d12/result.json), [journal](ci-direct3d12/validation.txt) |
| Metal | Apple Paravirtual device ; CI macos-15 | 0 / 0 | 5/5 | API et GPU shader validation | [PNG](ci-metal/cube.png), [rapport](ci-metal/result.json), [journal](ci-metal/validation.txt) |
| Vulkan local | Docker Linux depuis le poste Windows ; llvmpipe LLVM 20.1.2 / Mesa 25.2.8 | 0 / 0 | 5/5 × 2 | Khronos + synchronisation | [PNG](local-linux/cube.png), [miroir](local-linux/mirrored.png), [rapport](local-linux/result.json), [journal](local-linux/validation.txt) |

![Cube Vulkan CI](ci-vulkan/cube.png)
![Cube D3D12 CI](ci-direct3d12/cube.png)
![Cube Metal CI](ci-metal/cube.png)

Les trois PNG sont identiques à la référence analytique, SHA-256
`dedd7662f51f2b6baeee2fa3f41c1d1e3a2cedfd6eb7d0c1a3b657a40be22af5`.
Ils prouvent sept draws indexés par frame, hiérarchie glTF aplatie, textures sRGB,
facteur/éclairage linéaires, encodage shader unique, caméra RH_ZO et profondeur.
Le [protocole](../../CUBE_VALIDATION.md) détaille oracle, sondes, destruction
pendant l'enregistrement et régénération explicite. La scène D5 de phase 1 et
sa provenance sont inchangées ; D5 reste vert sur les trois backends.

La CI couvre aussi debug/release/shipping/profiling sur trois OS, clang-tidy 18,
ASan/UBSan et l'isolation shipping (UI absente, mode COOKED sans racine du dépôt).
Les étapes précédentes ont leurs CI vertes :
[extensions GPU](https://github.com/FoxTimew/Somnium/actions/runs/39726628021),
[assets](https://github.com/FoxTimew/Somnium/actions/runs/39730251136).
Le probe de composition a confirmé SDR_LINEAR sur les trois surfaces ; le
renderer désactive son encodage shader pour une cible sRGB. Le sandbox de fin de
phase l'utilise désormais ; ses preuves sont séparées dans le bilan ci-dessus.

**Limite locale Windows.** Le binaire CI de l'étape assets a été bloqué avant
exécution : `[WinError 4551] Une stratégie de contrôle d’application a bloqué ce fichier`.
Aucune modification de politique ni contournement ; les preuves Windows ci-dessus
proviennent du runner WARP. Cela ne remet pas en cause le test core release local
déjà validé. Le binaire GPU de l'étape précédente s'était exécuté sur Vulkan et
D3D12 Radeon RX 6600M locaux. Metal est explicitement une capture CI acceptée
comme substitut local ; il consomme le binaire macos-latest ciblant macOS 13.3.
La dépendance au runner macos-15 et son horizon de retrait restent documentés
dans [PHASE_1_CI.md](../../PHASE_1_CI.md).

**Historique.** Ce point de contrôle a précédé l'implémentation input, caméra
orbitale et sandbox. Les captures ci-dessus restent les pièces de ce jalon,
et ne constituent pas une preuve du nouveau sandbox. Aucun merge ni tag de phase 2.
