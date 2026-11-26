<!-- Décision normative ; protocole et résultats détaillés dans GPU_VALIDATION.md. -->
# ADR-0004 — Frame, ressources et synchronisation GPU

Date : 21 octobre 2026. Statut : **accepté**.

## Contexte

Présenter en VSYNC, préserver les ressources en vol et prouver les conventions.
Autorité : [SDL_gpu.h épinglé](https://github.com/libsdl-org/SDL/blob/829a65d769d935c4852f8159e964312c0957260a/include/SDL3/SDL_gpu.h),
D2 et D5 ; détails dans [GPU_VALIDATION.md](../GPU_VALIDATION.md).

## Décision

- **Device.** Demander SPIR-V | DXIL | MSL ; Vulkan/Linux, D3D12/Windows,
  Metal/macOS, sans repli silencieux. Debug ON en debug/asan-ubsan, OFF ailleurs.
  Aucun sélecteur WARP public ; le backend épinglé ne filtre pas les adaptateurs
  logiciels : le probe CI décide. La capacité `depthClamp` reste active pour
  l'ImGui officiel ; les pipelines Somnium activent le clipping de profondeur.
- **Swapchain.** Fenêtre réclamée, SDR/VSYNC vérifiés, **deux frames en vol**.
  Dimensions réelles en pixels à l'acquisition, reconstruction SDL au resize.
  Texture NULL : soumettre sans dessin, attendre les événements via platform,
  réinitialiser l'horloge à la reprise. Aucun délai artificiel dans Engine.
- **Frame.** Thread de fenêtre : `beginFrame`, `acquireSwapchain` avec
  `SDL_WaitAndAcquireGPUSwapchainTexture` bloquant, préparation UI, `recordPass`,
  `submitFrame`. L'attente borne les frames/allocations. `finishFrame` annule
  seulement sans texture acquise ; **après acquisition, toujours soumettre**,
  y compris sur fermeture/erreur. `renderOffscreen` compose la même API sans
  swapchain. Uploads moteur avant la frame ; uploads ImGui avant les passes.
  Capture explicite : cible possédée de même format/extent, scène puis UI,
  copie vers swapchain avant soumission, relecture avec fence après soumission.
- **Ressources.** Handles générationnels détenus par le device. Destruction
  pendant l'enregistrement : invalidation immédiate, `SDL_ReleaseGPU*` différé
  jusqu'à soumission/annulation ; SDL diffère ensuite la libération native jusqu'à
  sûreté GPU. Uploads cyclés ; fence avant mapping de relecture. UI détruite
  avant device/fenêtre. `PipelineDescription` décrit formats couleur/profondeur,
  vertex layout, culling et profondeur : RGBA8/D32 pour D5, format swapchain sans
  profondeur pour le triangle. NDC/UV et CCW/BACK/LESS/clear 1 confirmés par D5
  dans l'ADR-0002.
- **D5.** Device sans fenêtre, RGBA8 UNORM 256², sans MSAA/blending ; transfer
  buffer, fence, mapping. PNG LFS unique sous `engine/gpu/tests/reference/`,
  géométrie/seuils/sondes en texte voisin : 2/255 par canal, ≤0,1 % de pixels
  différents, centres témoins **exactement** égaux. Arêtes alignées aux pixels.
  Régénération manuelle explicite hors CI ; image produite, référence/diff,
  métriques et journal joints. Référence par backend uniquement sur justification PR.
- **D2.** `debug_ui` possède le contexte et pilote les façades séparées
  `somnium_platform_imgui`/`somnium_gpu_imgui` : initialisation, NewFrame,
  construction, ImGui::Render, préparation, passe UI. Coopération native privée,
  événements confinés à platform/adaptateur. Overlay FPS/temps/compteurs.
  Aucun include ImGui/condition UI dans les sources de base platform/gpu ;
  aucun include direct platform/gpu dans debug_ui. Shipping exclut toutes les cibles UI.

## Alternatives

Acquisition non bloquante/sleep : cadence arbitraire et pression mémoire.
Wait-idle par frame : parallélisme perdu. WARP natif : frontière SDL contournée.

## Conséquences

Les tests de handles vérifient l'invalidation et la libération différée. D5 passe
sur lavapipe, WARP et Metal/macOS 15 avec validation. Présentation et captures
restent une preuve séparée ; absence de device/couche : « non validé » explicite.

Preuves : aucune restante pour cette décision ; capture Metal CI acceptée comme substitut local le 21 octobre 2026.
