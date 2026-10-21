<!-- Pièces de validation conservées avec leurs origines et empreintes. -->
# Captures de phase 1

Les images de présentation sont des captures de fenêtre réelles, sans retouche.
Elles sont distinctes du test hors écran D5 et de sa référence unique.

## Windows local : D3D12 et Vulkan

Le binaire de l’étape d, commit `2356c60f8c828ab197c5c3630510778a6b7351aa`,
provient de l’[artefact CI Windows](https://github.com/FoxTimew/Somnium/actions/runs/39633123591/artifacts/12359488455).
Il est exécuté sur la Radeon RX 6600M locale, depuis la racine du dépôt :
les shaders sont bien trouvés à côté de l’exécutable, indépendamment du répertoire courant.
Les empreintes, versions et observations sont dans [provenance.json](windows/provenance.json).

| Backend | Triangle et overlay | Redimensionnement | Après minimisation et restauration | Journal |
| --- | --- | --- | --- | --- |
| D3D12 | [Capture](windows/direct3d12-overlay.jpg) | [1920 × 991](windows/direct3d12-resize.jpg) | [1280 × 720](windows/direct3d12-restored.jpg) | [Couche debug](windows/direct3d12-validation.txt) |
| Vulkan | [Capture](windows/vulkan-overlay.jpg) | [1920 × 991](windows/vulkan-resize.jpg) | [1280 × 720](windows/vulkan-restored.jpg) | [Khronos + synchronisation](windows/vulkan-validation.txt) |

La minimisation est confirmée par l’outil de fenêtre (« window is minimized »),
puis par le rendu restauré. Une fenêtre minimisée n’a pas d’image dessinable à
capturer. Le journal Vulkan montre aussi l’attente d’événements après texture NULL.
Les captures maximisées incluent la notification de cette tâche en bas à droite ;
les pixels n’ont pas été effacés. Les journaux de fermeture bornée
[D3D12](windows/direct3d12-close-during-frame.txt) et
[Vulkan](windows/vulkan-close-during-frame.txt) confirment la soumission après
acquisition, après douze frames rendues.

D5 local passe avec ce même artefact :
[D3D12](windows/direct3d12-d5-result.json) et [Vulkan](windows/vulkan-d5-result.json),
zéro pixel différent, dix centres exacts, couches actives. Les captures
[D3D12](windows/direct3d12-d5-actual.png) et [Vulkan](windows/vulkan-d5-actual.png)
ont le SHA-256 de la référence versionnée ; aucune référence spécifique par backend.

## Metal

Le poste de capture accessible est sous Windows. Aucune capture **locale** Metal
n’est présentée comme acquise. La présentation Metal est **validée en CI** sur
macOS 15, commit `2356c60f8c828ab197c5c3630510778a6b7351aa` :
[triangle/overlay](metal-ci/triangle-overlay.png), [resize](metal-ci/resize.png),
[minimisation](metal-ci/minimized-desktop.png), [restauration](metal-ci/restore.png),
[résultat et empreinte](metal-ci/result.json), [journal API/shader](metal-ci/validation.txt).
L’[artefact original](https://github.com/FoxTimew/Somnium/actions/runs/39633123591/artifacts/12361400786)
conserve également le helper exécuté. Les quatre images ont été inspectées ;
le journal confirme la soumission lors de la fermeture après acquisition.
Cette preuve porte explicitement la provenance **CI** ; son utilisation comme
substitut à la capture locale Metal est acceptée le 21 octobre 2026.
