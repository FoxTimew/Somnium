<!--
Propose la chaîne hors ligne à partir du spike isolé et des sources épinglées.
Les commandes, mesures et bindings détaillés sont dans SHADERS.md.
-->
# ADR-0003 — Chaîne de shaders hors ligne

Date : 21 octobre 2026. Statut : **accepté**, avec les ajustements du point de contrôle.

## Contexte

SDL GPU exige des formats et bindings différents selon le backend. D3 interdit
le compilateur HLSL dans le moteur. Le spike `feat/shader-toolchain-spike`
construit le CLI réel sans modifier `gpu`, `debug_ui` ou `cmake/Shaders.cmake`.
Les preuves et commandes reproductibles sont dans [SHADERS.md](../SHADERS.md).

## Décision

- **Acquisition.** Même recette en local et CI : archives officielles DXC
  `v1.9.2609` vérifiées par SHA-256 sous Windows/Linux ; sources de cette version
  sous macOS, faute d'archive officielle. SDL_shadercross
  `1ff05bec573988a98ef9e0260b4da44f512b8367`, SPIRV-Cross
  `vulkan-sdk-1.4.363.0` et SDL 3.4.18 sont résolus en SHA complets dans le
  [verrou](../../tools/shader_toolchain/lock.json). Cache des outils installés par
  OS, architecture, image/compilateur et recette ; tous les essais restent actifs
  après restauration. Le configure moteur ne lance jamais le bootstrap : variable
  `SOMNIUM_SHADERCROSS_EXECUTABLE`, chemin installé par défaut, erreur fatale avec
  commande de bootstrap si CLI ou DXC des depfiles manque. CI : outils requis
  avant moteur via `needs`, avec transfert de l'installation vérifiée.
- **Génération.** `somnium_add_shaders()` recevra cible, nom logique, `.hlsl`,
  entrée `vsMain`/`psMain`, étage vertex/fragment, dossier d'includes et defines.
  `main` interdit, aucun `MSL_ENTRYPOINT` ; vérifier le même nom dans le MSL.
  Sorties sous le build par
  configuration : `<nom>.<entrée>.<vert|frag>.{spv,dxil,msl,json}`. Un depfile DXC
  `-M -MF`, utilisant les mêmes includes/defines, suivra les includes transitifs.
  Installer/copier les artefacts sous `assets/shaders` à côté de l'exécutable.
- **Contrat.** L'en-tête HLSL commun portera les espaces/registres SDL du tableau
  de l'annexe, indices denses sans trous. La réflexion JSON de shadercross sur
  le SPIR-V produit fournira les quatre compteurs de `SDL_GPUShaderCreateInfo` ;
  un manifeste versionné ajoutera étage, formats et entrées. Cela évite une
  duplication manuelle des compteurs. Ne pas activer l'élimination des bindings.
  Le JSON CLI ne fournit pas l'entrée MSL : la convention évite le renommage
  `main` vers `main0` ; un nom absent du MSL produit fait échouer le build.
- **Chargement.** Choisir dans les artefacts et formats réellement retournés par
  le device : DXIL/D3D12, SPIR-V/Vulkan, MSL/Metal. Lire via le contrat de données
  injecté, valider manifeste et tailles, puis créer le shader. Aucun repli HLSL.
  Cible macOS déclarée **11.0**, MSL **2.3.0** explicite, compatible avec cette
  cible et supérieur au minimum SDL Metal (macOS 10.14, GPU Mac2). Une cible
  inférieure à 11.0 est rejetée. Le driver compile le MSL ; `.metallib` reste en phase 4,
  **prérequis de shipping macOS**. Aucun DXC/shadercross livré ou lié au moteur.
- **Ancienne vérification.** Passer l'archive seule `somnium_shadercross` derrière
  `SOMNIUM_VERIFY_SHADERCROSS_LIBRARY=OFF` par défaut ; le CLI
  isolé et ses essais deviennent la preuve obligatoire de la chaîne complète.

## Alternatives

DXC du Vulkan SDK écarté : téléchargement refusé dans l'essai et support DirectX
non garanti par sa documentation. Compiler DXC partout augmenterait le coût à
froid sans gain sur les archives officielles vérifiées. Compteurs écrits à la
main et compilation HLSL à l'exécution sont écartés.

## Conséquences

Le spike valide les trois CLI : 111 s Windows, 76 s Linux, 816 s macOS à froid ;
environ 1 s avec cache Windows/macOS. Le [plan CI](../PHASE_1_CI.md) réserve donc
un budget distinct au build source macOS. L'incrémentalité et le chargement GPU
restent à démontrer aux étapes a/b ; arrêt après b avant swapchain/UI.

Preuves : intégration CMake/depfiles et installation ; validation du manifeste et des entrées par format ; chargement/rendu des trois formats ; production .metallib au cuiseur.
