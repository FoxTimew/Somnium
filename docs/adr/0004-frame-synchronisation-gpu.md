<!--
Propose le modèle de frame et de possession à partir de SDL 3.4.18 épinglé.
Les détails reproductibles du test restent dans GPU_VALIDATION.md.
-->
# ADR-0004 — Frame, ressources et synchronisation GPU

Date : 21 octobre 2026. Statut : **accepté**, avec les ajustements du point de contrôle.

## Contexte

Présenter sans attente artificielle, préserver les ressources en vol et prouver
les conventions. Autorité :
[`SDL_gpu.h` épinglé](https://github.com/libsdl-org/SDL/blob/829a65d769d935c4852f8159e964312c0957260a/include/SDL3/SDL_gpu.h)
et D2/D5 ; protocole détaillé dans [GPU_VALIDATION.md](../GPU_VALIDATION.md).

## Décision

- **Device.** Demander SPIR-V | DXIL | MSL ; driver `vulkan` Linux,
  `direct3d12` Windows, `metal` macOS, sans repli silencieux. Debug ON en
  debug/asan-ubsan, OFF en release/profiling/shipping. Choisir l'artefact selon les
  formats supportés. **WARP non sélectionnable dans l'API GPU épinglée** : aucun
  sélecteur dans `SDL_gpu.h` ; adaptateur 0 choisi selon préférence d'énergie.
  Aucun filtre logiciel dans le backend épinglé : le probe Windows décide si
  Microsoft Basic Render Driver/WARP permet néanmoins de créer le device.
- **Swapchain.** Réclamer la fenêtre, vérifier SDR/VSYNC et conserver deux frames
  en vol (SDL : 1–3). Dimensions en pixels à l'acquisition ; SDL reconstruit après
  resize, `gpu` recrée ses cibles et revalide le format. Texture NULL, notamment
  en minimisation : aucun rendu, attente d'événements par platform.
- **Frame.** Thread de fenêtre : command buffer, puis
  `SDL_WaitAndAcquireGPUSwapchainTexture`, bloquant pour borner frames/allocations ;
  uploads/UI, passes, soumission/présentation. Sans texture acquise, annulation
  possible ; après acquisition, terminer et soumettre même sans dessin, car SDL
  interdit l'annulation. Supprimer l'attente artificielle d'Engine.
- **Possession.** Les handles générationnels appartiennent au device ; aucune
  ressource native publique. À l'étape b, chaque appel enregistre et soumet avant
  de rendre la main : aucun emprunt CPU externe. Invalider un handle détruit ; différer
  l'appel `SDL_ReleaseGPU*` demandé durant une frame jusqu'à sa soumission ou son
  annulation pour préserver les emprunts CPU. SDL diffère ensuite la destruction
  native jusqu'à sûreté GPU. Uploads avec `cycle=true` ; fence avant relecture
  CPU d'un download, puis libération de la fence. Arrêter UI/ressources avant device.
- **Preuve D5.** Device sans fenêtre ni swapchain, texture RGBA8 UNORM 256×256,
  sans MSAA ni blending, download vers transfer buffer puis fence et mapping.
  PNG LFS unique `engine/gpu/tests/reference/gpu_conventions.png` : 2/255 par canal,
  au plus 0,1 % de pixels au-delà ; régions témoins NDC/UV/profondeur/culling de
  l'ADR-0002 avec centres **exactement** égaux. Géométrie alignée sur pixels,
  seuils/sondes/géométrie en texte versionné à côté du test. Référence par backend
  seulement sur justification écrite en PR. Régénération explicite hors CI. Joindre
  images obtenue/référence/différence, métriques et journal de validation.
- **D2.** `debug_ui` initialise le contexte, puis les façades
  `somnium_platform_imgui` et `somnium_gpu_imgui`, seulement si l'option UI est ON.
  Leurs en-têtes privés de coopération donnent accès aux objets natifs des modules.
  Événements traités dans platform/adaptateur ; `NewFrame`, construction puis
  `ImGui::Render` pilotés par `debug_ui`. La façade GPU prépare les données avant
  la passe, puis enregistre l'UI dedans. Aucun include direct platform/gpu dans
  debug_ui ; aucun include ImGui/condition UI dans les sources de platform/gpu.

## Alternatives

Acquisition non bloquante/sleep manuel : pression mémoire et cadence arbitraire.
Wait-idle par frame : parallélisme perdu. WARP natif : frontière SDL contournée.

## Conséquences

D5 passe sur Vulkan/lavapipe, D3D12/WARP et Metal/macOS 15, avec couches actives.
Présentation et overlay restent à observer séparément. Sans device/couche CI :
« non validé », capture et journal local joints à la PR selon le [plan CI](../PHASE_1_CI.md).

Preuves : cycles de présentation/resize/minimisation ; intégration ImGui et captures locales.
