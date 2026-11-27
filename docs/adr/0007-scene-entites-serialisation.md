<!-- Contrat de scène accepté ; schéma et exemples détaillés dans SCENE.md. -->
# ADR-0007 — Scène, entités et sérialisation

Date : 25 novembre 2026. Statut : **accepté**.

## Contexte

La scène JSON doit porter personnage, murs et portes articulées, avec composants
définis par game, interpolation et inspection. EnTT v4.0.0 est utilisé depuis ses en-têtes épinglés.

## Décision

- **World** possède directement `entt::registry`. Structs plates dans `somnium` :
  PersistentId, Name, Transform (TRS locaux précédent/courant, monde dérivé),
  Hierarchy, MeshRenderer (handle CPU Model), Camera, DirectionalLight ; réglages
  de scène ambiant/fond et caméra active. Camera porte FOV vertical et near/far.
  Caméras/lumières et ancêtres ont une échelle exactement unitaire, contrôlée au
  chargement, reparentage et édition, avec erreur contextualisée. Caméra de suivi racine.
- **Identité** : ID textuel stable, jamais EnTT ni Name. Index World ID → entité,
  `find(id)` public. Création runtime : `runtime:<compteur hexadécimal 64 bits>`,
  monotone persisté dans `nextId`, collisions sautées, épuisement signalé ; aucune
  réutilisation après destruction. IDs importés uniques.
- **Hiérarchie** intrusive parent/premier-dernier enfant/frères, sans liste allouée
  par entité. Création, destruction de sous-arbre et reparentage immédiats via World,
  interdits pendant un parcours gardé (assertion debug, erreur release), y compris
  les vues EnTT manuelles. Cycles/parents invalides rejetés avant mutation ; ordre
  parent avant enfant reconstruit aux changements structurels.
- **Persistance** : JSON v1 `version/nextId/settings/activeCamera/entities`, composants
  `version/data`. Références par ID, assets par chemin logique ; aucun handle,
  EnTT ou monde dérivé persisté. Table de codecs extensible par game, fonctions
  avec Result ; version/composant inconnu ou donnée invalide fait échouer le chargement.
  Par décision, les codecs exposent publiquement `nlohmann::json` : ce sont des
  sérialiseurs, un format binaire pouvant partir de la même valeur. Game utilise
  `readString/readNumber/readVector3/requireFields` et le même résolveur d’entités
  en seconde passe que le moteur (porte → pivot), sans adresse de composant capturée.
- **Transaction** : World temporaire, résolution, validation et propagation avant
  publication. Le contexte demande les modèles ; Failed annule tout. World relâche
  ses demandes comptées à l’échec/déchargement, sans invalider celles d’une autre scène.
  Sauvegarde ordonnée par ID ; schéma détaillé dans [SCENE.md](../SCENE.md).
- **Temps/extraction** : début de pas courant → précédent ; gameplay puis propagation
  `mondeParent × T × R × S`. Téléportation/création/reparentage synchronisent le
  sous-arbre. Euler fichier/UI en degrés, extrinsèques X/Y/Z (`Rz × Ry × Rx`),
  quaternions normalisés en mémoire ; TRS finis, échelle non nulle, changement de signe
  par téléportation. Runtime interpole les TRS locaux (lerp/slerp court), recompose,
  puis extrait DrawList, caméra et lumière ; vue inverse, RH_ZO, aspect acquis.
  Une entité par modèle glTF aplati : porte = pivot de scène + modèle enfant séparé.
- **Gameplay/UI** : start emprunte World et Configuration ; fixedUpdate reçoit World,
  inputs, dt et tick ; stop reçoit World. Aucun requestModel, réglage de rendu ou
  camera() dans le contrat, aucun type renderer visible du jeu. L’inspecteur lit
  World et groupe les demandes par ID ; runtime les applique par teleport au prochain
  pas. Actions/axes sont affichés en lecture seule ; UI absente de shipping.

## Alternatives

Nœuds glTF exposés : import inutilement remanié. Handles persistés : identité fragile.
Interpolation monde : pivots incorrects. Composants inconnus ignorés : collisions perdues.

## Conséquences

Runtime → renderer est privé ; AABB et portes restent dans game. Tests CPU de
persistance, transactions, structure, extraction/interpolation et gameplay. La scène
hors écran utilise trois poses, un oracle analytique commun et des sondes exactes
avec seuils D5 ; aucune référence dérivée d’une capture.

Preuves : CPU, gameplay et trois poses GPU validés ; frames composées scène + inspecteur validées sur D3D12/Vulkan Radeon, lavapipe et Metal CI, pièces et commits dans le bilan de phase 3.
