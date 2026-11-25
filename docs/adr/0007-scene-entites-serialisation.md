<!--
Contrats de scène validés avant leur implémentation.
Le format détaillé et les cas de vérification sont décrits dans SCENE.md.
-->
# ADR-0007 — Scène, entités et sérialisation

Date : 25 novembre 2026. Statut : **accepté**.

## Contexte

Les scènes `assets/scenes/*.json` portent personnage, sol, murs, caméra et portes.
EnTT v4.0.0 est utilisé depuis les en-têtes du cache CPM.

## Décision

- **World.** `scene::World` possède un `entt::registry` directement accessible.
  Structs plates dans `somnium` : PersistentId, Name, Transform (TRS locaux
  précédent/courant, monde dérivé), Hierarchy, MeshRenderer (handle de Model),
  Camera (FOV vertical, near/far), DirectionalLight (direction locale vers la source).
  SceneSettings porte ambiant scalaire et fond RGBA linéaire ; caméra active par
  entité, au plus une lumière. Caméras/lumières et ancêtres : échelle unitaire,
  vérifiée au chargement, reparentage et changement de TRS, avec erreur contextualisée.
  La caméra de suivi du jeu est une racine.
- **Identités.** ID textuel stable, indépendant de Name et d'EnTT. World maintient
  l'index ID → entité et `find(id)` pour le jeu. Création à l'exécution :
  `runtime:<compteur hexadécimal 64 bits>`, compteur monotone sauvegardé dans
  `nextId`, collisions sautées, épuisement signalé ; jamais de réutilisation
  après destruction. Les IDs importés sont contrôlés pour unicité.
- **Hiérarchie/temps.** Parent/premier enfant/frères précédent-suivant intrusifs :
  aucune liste allouée par entité. Création, destruction de sous-arbre et
  reparentage via World sont **immédiats**, interdits pendant un parcours gardé
  (assertion debug, erreur en release). Les vues EnTT manuelles utilisent la même
  garde. Cycles/parents invalides rejetés avant mutation ; parcours parent avant
  enfant reconstruit aux changements structurels. Avant chaque pas : courant →
  précédent ; après gameplay : propagation `mondeParent × T × R × S`.
  `teleport`, création et reparentage synchronisent les historiques du sous-arbre.
- **JSON/codecs.** Schéma v1 : version/nextId/settings/activeCamera/entities ;
  entité id/components, composant version/data. Parents/caméra par ID, assets
  par chemin logique ; aucun identifiant EnTT, handle ou monde sérialisé.
  Table extensible de noms/versions et fonctions serialize/deserialize avec Result ;
  game enregistre ses composants sans modifier le moteur. Contexte commun :
  résolution différée d'IDs en **seconde passe**, accessible aussi aux codecs game
  (porte → pivot). Composant/version inconnus, référence absente ou donnée invalide :
  erreur contextualisée ; scène active inchangée. Sauvegarde ordonnée par ID.
- **Assets/transaction.** Le contexte demande les modèles et contrôle Ready ;
  Failed annule le chargement. World possède les demandes comptées et les libère
  au déchargement ou à l'échec ; les demandes d'une autre scène restent valides.
  Runtime fournit le contexte et le chemin inverse des handles.
- **Rotations/extraction.** Euler fichier/UI en degrés, extrinsèques X puis Y
  puis Z : `Rz × Ry × Rx` ; quaternions normalisés en mémoire. TRS finis,
  échelles non nulles ; inversion de signe par téléportation. Runtime interpole
  les TRS locaux à l'alpha fixe (lerp/slerp court), puis recompose la hiérarchie,
  caméra comprise (vue inverse, RH_ZO, aspect cible). DrawList = monde interpolé
  × instances glTF aplaties ; cache renderer par handle. **Une entité par modèle** :
  porte = pivot de scène + mesh enfant, ouvrant/bâti séparés.
- **Gameplay/UI.** start/stop empruntent World ; fixedUpdate reçoit World, inputs,
  dt et indice de pas. requestModel/lightDirection/ambient/clearColor/camera()
  disparaissent du contrat ; aucun type renderer visible du jeu. Sandbox fournit
  les chemins. Inspecteur : éditions en frontière de pas, via World/téléportation.

## Alternatives

Nœuds glTF exposés : refonte d'import inutile ici. Handles persistés : identité
fragile. Interpolation monde : pivots incorrects. Composants ignorés : collisions perdues.

## Conséquences

Runtime → renderer devient privé ; AABB et portes Interact à 90° restent dans game.
Tests CPU : identités, garde structurelle, codecs game/références, transaction/assets,
échelles, aller-retour, cycles, propagation et interpolation/extraction/caméra.
Preuve JSON : pivot/enfant décentré tourné, occultation, trois poses ; oracle
analytique, références/sondes/provenance versionnées, PNG LFS. Centres exacts,
2/255 et 0,1 %, régénération explicite et validation GPU comme D5/cube.

Preuves : CPU et trois poses GPU validés en CI D3D12/Vulkan Radeon, lavapipe et Metal (images exactes) ; présentation Metal orbite/zoom/reset en échec, inspecteur et gameplay cible après validation du point de contrôle.
