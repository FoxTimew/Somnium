<!-- Proposition issue du spike Windows/Linux ; le jeu de production reste statique. -->
# ADR-0011 — Module de gameplay rechargeable

Date : 27 novembre 2026. Statut : **proposé — validation requise**.

## Contexte

Le [spike](../HOT_RELOAD_SPIKE.md) prouve conservation et rollback avec World,
EnTT 4.0.0, codecs, Gameplay et rejeu existants. Le jeu réel suppose encore start au tick 0.

## Décision

- **Découpage** : seule la configuration de développement `iteration` construit
  les modules moteur dans une image partagée et game en module. Log et registres
  n'ont qu'un propriétaire ; Tracy, si activé, est un client partagé unique.
  Autres presets : statiques, mêmes sources/contrat Gameplay.
  La dérogation au statique est limitée à cette configuration.
- **ABI locale** : entrée C versionnée, taille de table et empreinte du build moteur,
  compilateur, bibliothèque standard, options ABI, dépendances et headers vérifiées
  avant factory. Sans exceptions/RTTI ; CRT partagé `/MD[d]` sous Windows,
  même libstdc++ sous Linux. Factory/destruction exécutées dans le module d'origine.
  Ni ABI tierce stable ni protection contre un crash natif.
- **Types** : hash/noms EnTT identiques pour les types partagés, jamais les
  `type_index` séquentiels. Types privés du jeu dans un namespace de génération
  distinct ; noms/versions de codecs persistants stables. Manifeste possédé par
  runtime pour codecs, stockages et abonnements. Le retrait
  détruit les fonctions ; `registry.reset(id)` retire le pool, pas seulement ses éléments.
- **Transaction au pas** : stop → instantané par codecs → destruction du Gameplay
  et retrait des enregistrements/pools → chargement d'une copie candidate → codecs
  réenregistrés → World candidat restauré/validé → start au tick courant → publication.
  L'ancien World conserve ses demandes d'assets jusqu'au succès puis est détruit.
  Aucun emprunt/vtable/callback ancien ne reste
  actif côté moteur. Échec de chargement, restauration ou start : candidat détruit,
  ancien module reconstruit depuis le même instantané ; erreur affichée et simulation
  poursuivie sans rejouer les événements. L'input reste vivant pendant l'échange.
- **État** : seul World persiste. Start reconstruit les caches et reçoit un contexte
  contenant mapping et tick courant, commun aux deux versions.
  L'orbite de reset de Gameplay devient une donnée World ;
  les progrès de portes, positions et historiques d'interpolation restent conservés.
- **Images** : platform charge des copies à noms uniques avec symboles de debug.
  Pas de déchargement physique avant sortie, y compris les candidats entrés :
  Tracy peut conserver des adresses de SourceLocationData. Maximum 64 images puis
  rechargement refusé avec demande de redémarrage ; seul le gestionnaire garde les
  handles inertes. Publication externe atomique, jamais de compilation dans le moteur.

## Alternatives

Lier le moteur statique à chaque DLL : état dupliqué. Type de même nom/disposition
réutilisé : pools et symboles ambigus. Décharger réellement : pointeurs Tracy non
maîtrisés. Copier les octets d'un composant : changement de disposition impossible.

## Conséquences

ABI incompatible = redémarrage. Octets retenus affichés, images bornées en nombre.
Services absents de shipping ; macOS best effort non bloquant.

Preuves : jeu réel et assets, empreinte ABI complète, retrait des abonnements/groupes/contextes, reprise au tick courant du contrat révisé, Tracy connecté et exclusion shipping après intégration.
