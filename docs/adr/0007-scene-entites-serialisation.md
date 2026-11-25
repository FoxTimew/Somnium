<!--
Décisions proposées pour la scène de phase 3 et ses preuves.
Aucune implémentation avant validation de ce jalon documentaire.
-->
# ADR-0007 — Scène, entités et sérialisation

Date : 25 novembre 2026. Statut : **proposé**.

## Contexte

Les scènes `assets/scenes/*.json` portent personnage, sol, murs, caméra et portes.
EnTT v4.0.0 a été vérifié dans les en-têtes du cache CPM.

## Décision

- **World.** `scene::World` possède un `entt::registry` directement accessible,
  sans façade ECS. Structs plates dans `somnium` : `PersistentId`, `Name`,
  `Transform` (TRS locaux précédent/courant, monde dérivé), `Hierarchy`,
  `MeshRenderer` (handle typé de Model), `Camera` (FOV vertical, near/far),
  `DirectionalLight` (direction locale vers la source). World porte
  `SceneSettings` (ambiant/fond RGBA linéaires), la caméra active et une lumière.
  Camera/light et leurs ancêtres gardent une échelle unitaire.
- **Hiérarchie.** Parent/premier enfant/frères précédent-suivant intrusifs :
  aucune liste allouée par entité. Création, destruction de sous-arbre et reparentage
  passent par World ; registre direct pour les autres composants. Parents invalides
  et cycles sont rejetés avant mutation.
  Un parcours parent avant enfant, reconstruit aux changements structurels,
  propage `mondeParent × T × R × S` ; capacités réservées au chargement.
- **Persistance.** ID textuel opaque, unique dans la scène, stable indépendamment
  du nom ; nouveaux IDs jamais dérivés d'EnTT. JSON v1 :
  `version/settings/activeCamera/entities`, entité `id/components`,
  composant `version/data`. Parents/caméra par ID, assets par chemin logique ;
  aucun ID EnTT, handle, monde ou historique sérialisé. Chargement transactionnel :
  création des identités/composants, puis résolution/validation des références.
- **Extension jeu.** Table de noms stables, versions et fonctions
  `serialize/deserialize` par composant (erreurs via Result), recevant
  World/entité et contexte d'assets. Game enregistre ses codecs, dont `game.AabbCollider`,
  avant chargement, sans modifier le moteur. Runtime fournit chemin ↔ handle
  (lookup inverse à ajouter à AssetManager). Composant/version inconnus,
  ID dupliqué, référence absente ou données invalides : erreur contextualisée,
  scène active inchangée. Sauvegarde ordonnée par ID.
- **Temps et rotations.** Euler fichier/UI en **degrés**, rotations extrinsèques
  X puis Y puis Z : `R = Rz × Ry × Rx` ; quaternions normalisés en mémoire.
  Round-trip équivalent en rotation, pas nécessairement en triplet Euler.
  Avant chaque pas, courant → précédent ; gameplay modifie le courant, puis
  propagation. `teleport` synchronise les deux états du sous-arbre ; création
  et reparentage aussi. TRS finis, échelles non nulles ; changement de signe
  d'échelle par téléportation.
- **Extraction/modèles.** Runtime interpole les TRS locaux à l'alpha du pas fixe
  (lerp, slerp au plus court), puis recompose la hiérarchie. Caméra interpolée pareil :
  vue inverse de pose, perspective RH_ZO, aspect de la cible. DrawList =
  monde interpolé × instances glTF aplaties ; cache renderer par handle d'asset.
  **Une entité par modèle** : porte = pivot de scène + mesh enfant ; ouvrant
  et bâti sont des modèles séparés, sans animation des nœuds internes glTF.
- **Gameplay/UI.** start/stop empruntent World ; fixedUpdate reçoit World, inputs,
  dt et indice de pas. Suppression de
  `requestModel/lightDirection/ambient/clearColor/camera()` et de tous les types
  renderer visibles du jeu. Sandbox fournit les chemins scène/input. Inspecteur :
  éditions appliquées aux frontières de pas via hiérarchie/téléportation.

## Alternatives

Entités par nœud glTF : refonte d'import inutile ici. Handles persistés : identité
fragile. Interpolation monde : pivots incorrects. Composants ignorés : collisions perdues.

## Conséquences

Runtime → renderer devient privé ; collision AABB et ouverture Interact à proximité
(90°) restent dans game. Tests CPU : aller-retour/codecs game, cycles, propagation,
extraction/interpolation/caméra/téléportation. Preuve : scène JSON avec pivot,
enfant décentré tourné et occultation, poses initiale/intermédiaire/finale ; oracle
analytique indépendant, références/sondes/provenance sous tests et PNG LFS.
Centres exacts, 2/255 et 0,1 %, régénération explicite/validation GPU comme D5/cube.

Preuves : World, codecs extensibles, hiérarchie, interpolation et preuve de scène restent à implémenter et valider sur Windows/Radeon et Linux/lavapipe requis ; Metal best effort.
