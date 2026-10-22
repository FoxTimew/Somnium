<!-- Bilan local de la phase 2 ; les preuves du SHA final restent distinctes du jalon validé. -->
# Phase 2 — bilan local et validation finale en attente

État au 22 octobre 2026 : implémentation terminée localement, **phase non clôturée**.
GitHub Actions refuse de démarrer les nouveaux jobs pour une raison de facturation.
Le propriétaire indique une reprise dans dix jours. Aucun push supplémentaire,
merge dans main ni tag de phase 2 n'est effectué pendant ce blocage.

## Livraison

- Platform collecte/traduit SDL en événements Somnium bruts horodatés au prochain
  pas fixe. Input dépend seulement de core : actions, transitions conservées,
  axes normalisés, zone morte, focus et déconnexion ; tests CPU et scanner négatif.
- Gameplay reçoit les inputs, demande `models/orbit-cube.glb` par GameContext et
  fournit `renderer::Camera`. Game possède l'orbite bornée ; runtime charge via
  AssetManager et produit la DrawList avec ModelRendering. Le triangle et ses
  ressources ont disparu de runtime/sandbox ; aucune logique de caméra n'y réside.
- Présentation SDR_LINEAR, profondeur recréée au resize, deux frames en vol,
  acquisition bloquante et soumission après acquisition. Overlay passif.
- Ambiant de frame : zéro conserve le shader du cube et sa provenance ; 0,15 dans
  le sandbox via une variante PS b2. L'oracle, les sondes et la référence sont inchangés.
- Modèle original CC0 sous LFS, licence dans `assets/LICENSES.md`. Racines ordonnées
  en développement ; aucune copie de sources dans le build. Shipping utilise une
  racine unique, vérifiée par un paquet de test temporaire extérieur au build.
- README, architecture et ADR-0005 accepté reflètent ces contrats. Scene/ECS,
  mapping JSON et interpolation restent en phase 3 ; cuiseur/KTX2/BC/mipmaps et
  prérequis `.metallib` macOS restent en phase 4.

## Commits et publication

| Étape | Commit | État |
| --- | --- | --- |
| GPU / assets / renderer-cube | `e5e765a` / `f6d71cf` / `3edbfce` | Publiés, CI verte, jalon validé |
| Input neutre | `adeeae5` | Publié ; CI PR verte, workflow push bloqué à son démarrage GPU |
| Contrat Gameplay et caméra | `98ce7bf` | Local, non poussé |
| Sandbox glTF et captures | `9dc0733` | Local, non poussé |
| Documentation et pièces | Commit de ce bilan | Local, non poussé |

La [CI PR input](https://github.com/FoxTimew/Somnium/actions/runs/39755226013) est
entièrement verte. Dans le [workflow push](https://github.com/FoxTimew/Somnium/actions/runs/39755216717),
desktop, outils, analyse et sanitizers sont verts ; les jobs GPU n'ont exécuté
aucune étape. Annotation GitHub : « The job was not started because recent account
payments have failed or your spending limit needs to be increased. Please check
the 'Billing & plans' section in your settings ». Ce n'est pas un échec de rendu.

## Vérifications locales

Linux Ubuntu 24.04 sous Docker depuis le poste Windows ; Clang/clang-tidy 18,
Mesa/lavapipe, Xvfb et couches Khronos + synchronisation.

| Vérification | Résultat |
| --- | --- |
| Debug, tests CPU/architecture | 16/16 sélectionnés ; test d'incrémentalité shader exclu de cette sélection |
| Fenêtre debug, fermeture après acquisition | 1/1 |
| Shipping, tests et paquet temporaire | 16/16 sélectionnés, incrémentalité shader exclue |
| Graphe shipping | Aucune cible/source UI ; COOKED, aucune racine du dépôt |
| clang-tidy 18, format, scanner et actionlint | Verts |
| ASan/UBSan | 16 tests CPU/architecture + 1 fenêtre passent, détection des fuites active ; délai du test shader détaillé ci-dessous |
| Cube normal + miroir, ambiant nul | 0 pixel différent, écart maximal 0, 5/5 sondes exactes par image |
| D5 conservé | 0 pixel différent, écart maximal 0, 10/10 sondes exactes |
| Présentation Vulkan | Modèle, orbite, zoom, reset et resize validés ; aucune erreur de validation |

Pièces locales : [commandes et environnement](local-checks/COMMANDS.md), [CPU debug](local-checks/tests-sandbox.txt),
[fenêtre](local-checks/window-sandbox.txt), [shipping](local-checks/tests-shipping.txt),
[graphe shipping](local-checks/shipping-graph.txt), [clang-tidy](local-checks/tidy-sandbox.txt),
[cube normal/miroir](final-local-cube/result.json) avec [validation](final-local-cube/validation.txt),
[D5](final-local-d5/result.json) avec [validation](final-local-d5/validation.txt).
La référence du cube et tous les fichiers sources de sa provenance sont inchangés
par rapport au jalon `3edbfce` ; `cube_reference.py --verify` passe.

Le premier [passage ASan/UBSan](local-checks/tests-asan.txt) arrête le script sur
le timeout CTest de 120 s du test shader ; les 16 autres tests de cette sélection
passent. Le [test fenêtre](local-checks/tests-asan-window.txt), lancé séparément avec
le même lavapipe préchargé et les mêmes options ASan/UBSan, passe aussi. Aucun
signalement mémoire ni suppression de fuite. Le même [contrat shader](local-checks/tests-shader-local.txt) passe ensuite en
**32 s**, avec le point d’entrée natif CMake 3.31.6 et un dossier de travail Linux
`/tmp`, au lieu du lanceur Python et du montage NTFS. Cette relance diagnostique
isole la lenteur locale ; elle ne transforme pas le premier CTest en succès.
La limite CI de 120 s et son test obligatoire sont conservés.

La nouvelle chaîne n'a **pas** de validation finale Windows/macOS : les captures
D3D12/Metal du [jalon](README.md) prouvent ce jalon seulement. L'exécution native
Windows d'un précédent binaire a rencontré WinError 4551 (journal archivé au jalon) ;
aucune modification de politique n'a été faite et le test core release déjà validé
n'est pas remis en cause. La preuve Metal CI acceptée comme substitut local reste
à obtenir pour ce sandbox ; son script est préparé mais non exécuté ici.

## Captures

Captures Vulkan réelles sous Xvfb/lavapipe du commit **9dc0733**, code propre :
[rapport avec SHA binaire/modèle](sandbox-vulkan/result.json) et
[journal de validation](sandbox-vulkan/validation.txt). Elles sont conservées
localement sous LFS ; leur publication dans la PR attend la reprise de la CI.

| Modèle et overlay | Orbite | Zoom |
| --- | --- | --- |
| ![Modèle](sandbox-vulkan/model-overlay.png) | ![Orbite](sandbox-vulkan/orbit.png) | ![Zoom](sandbox-vulkan/zoom.png) |

[Reset](sandbox-vulkan/reset.png) · [Redimensionnement](sandbox-vulkan/resize.png).
38 878 pixels de la région contrôlée changent après l’orbite, 94 035 après le zoom,
et zéro après le reset par rapport à l’état initial.
L'orbite/zoom sont produits par de vrais événements X11 envoyés à la fenêtre ;
le reset doit restaurer exactement les pixels du modèle, hors région de l'overlay.
Le resize vérifie une capture 960×600 et la validation de la nouvelle profondeur.

Reproduction : `tools/presentation/capture_linux.py` sous Xvfb, avec xdotool,
ImageMagick et les variables de validation Vulkan du workflow. Le script macOS
utilise Quartz pour orbite/zoom/reset puis vérifie resize/minimisation/restauration.
Les [outils de capture](../../../tools/presentation/) gardent les originaux sans retouche.

## Reprise et sortie de phase

1. Rétablir GitHub Actions puis relancer les jobs en échec de `39755216717`.
2. Pousser **uniquement le commit caméra** vers la branche de phase, attendre les
   deux CI vertes ; pousser ensuite le commit sandbox, puis attendre sa CI verte.
3. Vérifier D5/cube sur les trois backends et joindre les nouvelles captures
   Vulkan/Metal à la PR, avec journaux. Une capture Metal indisponible reste
   explicitement non validée ; deux backends de présentation sont requis.
4. Actualiser ce bilan/ADR-0005 avec les preuves finales, puis publier le commit
   documentaire après la CI de l'étape précédente. Demander la validation de fin
   de phase ; fusion/tag ne sont pas autorisés par le seul jalon intermédiaire.

Seul `gpu-macos` dépend de `macos-15`, avec binaire macos-latest ciblant macOS 13.3.
L'horizon de retrait annoncé est l'automne 2027, sans jour arm64 confirmé dans la
source documentée ; [détails et condition de migration](../../PHASE_1_CI.md).
