<!-- Décision de phase 4 ; les capacités de scene restent disponibles en shipping.
     Les outils seuls dépendent de la configuration de développement. -->
# ADR-0009 — Instantanés, modes, édition et enregistrement

Date : 27 novembre 2026. Statut : **accepté**.

## Contexte

Jouer/Arrêter, rechargement et sauvegarde partagent un instantané par codecs.
L'aller-retour Euler n'est pas exact ([mesure](../HOT_RELOAD_SPIKE.md#rotations)).

## Décision

- **Scene, tous presets** : `WorldSnapshot` et sérialisation canonique v2 sont
  compilés/testés aussi en shipping. Capture JSON par codecs : IDs, compteur,
  réglages, chemins d'assets, TRS et historique précédent/courant transitoire.
  Restauration dans un World candidat ; modèles redemandés, références résolues,
  validation puis échange. L'ancien garde ses demandes jusqu'au succès.
- **Exactitude** : scène v2, Transform v2, quaternion `[x,y,z,w]` fini, avec
  `|dot(q,q) − 1| ≤ 10⁻⁵` calculé en double ; rejet au-delà, aucune renormalisation,
  signe conservé. Migration explicite des scènes v1 Euler ; vue d'édition en
  degrés/Rz·Ry·Rx. ADR-0002 amendé. Racines et frères sont canoniques par
  PersistentId dans World, indépendamment de l'insertion ; instantané et test
  préservent exactement cet ordre. Entités/clefs triées, UTF-8/LF, indentation fixe,
  nombres à aller-retour exact. Historique exclu du disque, synchronisé à sa lecture.
- **Modes, iteration seulement** : runtime applique les commandes aux frontières
  de pas ; Jeu par défaut. Jouer capture l'édition puis start ; Arrêter appelle
  stop puis restaure exactement. Aucun fixedUpdate du jeu en Édition. Horloge
  des services monotone, start reçoit le tick courant. Caméra d'outil possédée
  par debug_ui hors World, sans type renderer ; runtime adapte sa pose/projection.
  Mapping distinct, routage exclusif, sans contextes d'input génériques.
- **Édition** : commandes par PersistentId, création/suppression de sous-arbre,
  nom/parent/composants via World et transactions de codecs. Défauts/métadonnées
  de champs en données, éditeur JSON générique sans callback ImGui par composant.
  Invariants structurels protégés ; références/cycles/échelles/assets invalides
  rejetés avec contexte.
- **Enregistrer** refusé en Jeu. En Édition, écriture platform dans un temporaire
  voisin, synchronisation/remplacement atomique dans la racine source autorisée,
  avec contrôle de modification externe. Dirty compare la valeur canonique au
  dernier enregistrement réussi ; Recharger exige abandon explicite si dirty.
  Réenregistrement de `assets/scenes/` dans un commit dédié ; fixtures analytiques
  et provenances inchangées.
- **Annuler/Rétablir** : pile commune de 16 états/16 Mio ; coût par édition validée,
  jamais par frame (capture Debug 5–7 ms, optimisée 0,3 ms ; 16 états ≈ 0,55 Mio).
  Nouvel edit invalide redo ; restauration candidate avant tout échange de curseur,
  échec sans effet sur World/demandes/pile. Save garde la pile et actualise la base
  dirty ; Reload réussi la vide. Éviction des états les plus éloignés du curseur ;
  instantané individuel > 16 Mio : opération refusée, jamais d'historique perdu.
  Modes, commandes, écriture source, surveillance et
  chargeur exclus du shipping. Lots 1 à 3 avec jeu/moteur statiques ; partagé
  seulement au lot 4. Profilage d'iteration manuel, exigé aux jalons.

## Alternatives

Copie mémoire EnTT : couplage aux dispositions. Commandes inverses : second
mécanisme. Euler seul : perte constatée. Sauver le Jeu : écrasement de l'édition.

## Conséquences

Codecs versionnés et données transitoires, lecteurs vérifiés. Tout état durable
du jeu appartient à World ; références extérieures par PersistentId. Reconstruction
aux actions explicites, aucun travail d'édition inactif.

Preuves : restauration lors du rechargement du module réel (lot 4) ; édition macOS hors couverture requise.
