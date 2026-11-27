<!-- Proposition de phase 4 ; aucune fonctionnalité d'édition n'est encore intégrée. -->
# ADR-0009 — Instantanés, modes, édition et enregistrement

Date : 27 novembre 2026. Statut : **proposé — validation requise**.

## Contexte

Jouer/Arrêter, rechargement et sauvegarde partagent un instantané par codecs.
L'aller-retour quaternion/Euler actuel n'est pas exact ([mesure](../HOT_RELOAD_SPIKE.md#rotations)).

## Décision

- **Une transaction** : `WorldSnapshot` capture une valeur JSON par les codecs,
  IDs, compteur d'identités, réglages, références logiques et TRS. Les codecs
  capturent aussi l'historique précédent/courant pour la restauration mémoire ;
  cet historique transitoire est exclu du fichier. Restauration dans un World
  candidat, modèles redemandés, références résolues et validation avant échange.
  L'ancien World garde ses demandes jusqu'au succès ; échec = état intact.
- **Exactitude proposée** : schéma scène v2 et codec Transform v2 stockant le
  quaternion `[x,y,z,w]`, fini/unitaire, sans renormalisation au chargement.
  Lecture v1 Euler conservée par migration explicite ; Euler degrés/Rz·Ry·Rx
  reste la vue d'édition. **Cet amendement de l'ADR-0002 est soumis à validation** ;
  les conventions actuelles restent inchangées. Entités triées par ID,
  clés ordonnées, UTF-8/LF, indentation fixe et décimaux à aller-retour exact.
- **Modes** : runtime applique les commandes aux frontières de pas ; Jeu par
  défaut. Jouer capture l'édition puis appelle start ; Arrêter appelle stop puis
  restaure exactement l'instantané. En Édition, aucun fixedUpdate du jeu.
  Horloge des services maintenue ; démarrage/rechargement reçoit le tick courant.
  La caméra d'outil appartient à debug_ui, hors World, sans type renderer ;
  runtime adapte seulement sa pose/projection à l'extraction. Mapping outil
  distinct, routage exclusif par mode, sans introduire les contextes d'input.
- **Édition** : file de commandes par ID ; création, suppression du sous-arbre,
  nom, parent et composants passent par World et une transaction de codecs.
  Défauts et métadonnées de champs sont des données ; éditeur JSON générique,
  aucun callback ImGui par composant. Invariants structurels non supprimables,
  références/cycles/échelles/assets invalides rejetés avec leur contexte.
- **Enregistrer** est refusé en Jeu. En Édition, platform écrit un temporaire
  voisin, synchronise puis remplace atomiquement dans la racine source autorisée,
  avec contrôle de modification externe. Dirty compare la valeur canonique au
  dernier enregistrement réussi ; Recharger exige un abandon explicite si dirty.
  `assets/scenes/` sera réenregistré dans un commit dédié ; fixtures analytiques
  et provenances restent inchangées. Chargement fichier : historique synchronisé.
- **Annulation** : décision conditionnée à la mesure du lot 2 : pile de 16
  instantanés/16 Mio si capture p95 ≤ 1 ms sur la cour ; sinon report documenté.
  Les outils sont limités à la configuration `iteration`, jamais shipping.

## Alternatives

Copie mémoire EnTT : adresses et disposition couplées. Commandes inverses : second
mécanisme. Euler seul : perte constatée. Sauver le Jeu : risque d'écraser l'édition.

## Conséquences

Les codecs gagnent défauts, versions et données transitoires ; les lectures restent
vérifiées. Aucun état gameplay durable hors World. Coût de reconstruction accepté
aux actions explicites, aucun travail d'édition dans la boucle inactive.

Preuves : rotations arbitraires et historique exacts, migrations v1/v2, assets équilibrés, cycles Jouer/Arrêter, édition/sauvegarde/rechargement après redémarrage et mesure de l'annulation.
