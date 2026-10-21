<!-- Décision normative ; commandes, bindings et mesures détaillés dans SHADERS.md. -->
# ADR-0003 — Chaîne de shaders hors ligne

Date : 21 octobre 2026. Statut : **accepté**.

## Contexte

SDL GPU utilise SPIR-V, DXIL ou MSL selon le device. D3 interdit la compilation
HLSL à l'exécution et tout compilateur dans le moteur. Le spike isolé a validé
les trois CLI ; [SHADERS.md](../SHADERS.md) conserve les mesures et commandes.

## Décision

- **Outils.** DXC **v1.9.2609** : archives officielles vérifiées par SHA-256 sous
  Windows/Linux, sources sous macOS. SDL_shadercross
  **1ff05bec573988a98ef9e0260b4da44f512b8367**, SPIRV-Cross
  **vulkan-sdk-1.4.363.0** et SDL **3.4.18** sont verrouillés par SHA complets dans
  [lock.json](../../tools/shader_toolchain/lock.json). Même bootstrap explicite
  en local/CI ; cache par OS, architecture, image, compilateur et recette,
  avec essais réexécutés après restauration. Le configure moteur ne lance jamais
  `build.py` : `SOMNIUM_SHADERCROSS_EXECUTABLE` pointe par défaut vers l'installation
  du bootstrap ; CLI ou DXC voisin absent/inexécutable donne `FATAL_ERROR` et
  commande corrective. Les jobs moteur dépendent du job outils via `needs`.
- **Génération.** `somnium_add_shaders()` reçoit cible, nom logique, source HLSL,
  entrée, étage, includes et defines. Entrées **vsMain/psMain**, `main` interdit ;
  aucun paramètre `MSL_ENTRYPOINT`, présence du même nom vérifiée dans le MSL.
  Sorties sous le build : `<nom>.<entrée>.<vert|frag>.{spv,dxil,msl,json}`.
  Depfile DXC `-M -MF` avec mêmes includes/defines, dépendances transitives et
  échappement Ninja. Copie et installation dans `<binaire>/assets/shaders`.
- **Bindings et métadonnées.** L'en-tête HLSL commun porte les espaces/registres
  SDL par étage, décrits dans [le tableau](../SHADERS.md). Indices denses ; pas
  d'élimination des bindings. La réflexion JSON shadercross du SPIR-V fournit
  les quatre compteurs de `SDL_GPUShaderCreateInfo` ; le manifeste ajoute étage,
  formats et entrées. Cette source unique évite les compteurs manuels divergents.
- **Chargement.** `gpu` valide manifeste, tailles et compteurs via `ByteReader`
  injecté. Sélection parmi les formats réellement supportés : DXIL/D3D12,
  SPIR-V/Vulkan, MSL/Metal. Aucun repli HLSL ni outil livré avec le moteur.
  `CMAKE_OSX_DEPLOYMENT_TARGET=13.3` minimum pour `std::format` avec libc++ Apple ;
  cible inférieure rejetée. MSL **2.3.0** reste explicite : sous-ensemble suffisant,
  compatible macOS 11+, au-delà du minimum SDL Metal (macOS 10.14, GPU Mac2). Le MVP
  charge le MSL tel quel ; compilation driver autorisée. **.metallib reporté
  au cuiseur de phase 4, prérequis de shipping macOS.**
- **Vérification historique.** `SOMNIUM_VERIFY_SHADERCROSS_LIBRARY=OFF` par défaut ;
  le CLI isolé et ses tests constituent la preuve obligatoire.

## Alternatives

DXC Vulkan SDK écarté : téléchargement refusé pendant le spike et support DirectX
non garanti. Compiler DXC partout augmente le coût sans gain sur les archives
vérifiées. Compteurs manuels et compilation HLSL runtime sont écartés.

## Conséquences

CLI à froid : 111 s Windows, 76 s Linux, 816 s macOS ; environ 1 s avec cache
Windows/macOS. Incrémentalité, installation et diagnostics sont testés sur les
trois OS ; [D5](../GPU_VALIDATION.md) confirme le chargement et le rendu des trois formats.
Le blocage du CLI local Windows par la stratégie d'application reste consigné dans l'annexe.

Preuves : production .metallib au cuiseur.
