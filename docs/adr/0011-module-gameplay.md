<!-- Décision issue du spike Windows/Linux ; le partagé arrive au lot 4 seulement.
     Les presets existants conservent leur liaison statique. -->
# ADR-0011 — Module de gameplay rechargeable

Date : 27 novembre 2026. Statut : **accepté**.

## Contexte

Le [spike](../HOT_RELOAD_SPIKE.md) prouve conservation et rollback avec World,
EnTT 4.0.0, codecs, Gameplay et rejeu existants.

## Décision

- **Découpage** : au lot 4 seulement, `iteration` construit une image moteur
  partagée et game en module. Lots 1 à 3 statiques ; autres presets toujours
  statiques, mêmes sources/contrat. Log/registres ont un propriétaire ; Tracy
  est un client partagé unique quand activé. SDL est aussi partagé dans iteration,
  pour que moteur et hôtes de tests utilisant ses hooks partagent la même instance.
- **Exports explicites** : macro `SOMNIUM_ENGINE_API`, dllexport/dllimport sous
  Windows, visibilité default sous Linux (cachée par défaut), vide en statique.
  Fonctions et classes d'API annotées ; aucun export automatique. Données mutables
  privées aux .cpp, accessibles par fonctions exportées, pas de variables exportées
  ni d'état mutable inline/statique dans les en-têtes publics du moteur.
  CI vérifie ces en-têtes et le graphe/les objets liés : aucun code objet moteur
  dans game, seulement l'import de l'image partagée. Cela rend la frontière révisable
  et évite les duplications invisibles du spike à exports automatiques.
- **ABI locale** : entrée C versionnée, taille/empreinte du build moteur,
  compilateur, bibliothèque standard, options ABI, dépendances/headers vérifiées
  avant factory. `ComponentCodec.editor` reste conditionnel : option iteration et
  taille/alignement du codec font partie de l'empreinte. Sans exceptions/RTTI ;
  CRT `/MD[d]` Windows, même libstdc++ Linux.
  Factory/destruction dans leur module d'origine. Ni ABI tierce ni isolation de crash.
- **Types/enregistrements** : hash/noms EnTT partagés, jamais les type_index
  séquentiels. Types privés dans un namespace de génération **non inline**, car
  Clang omet les namespaces inline dans le nom haché ; codecs persistants
  stables. Runtime possède le manifeste codecs/stockages/abonnements ; retrait des
  fonctions et pools via `registry.reset(id)`, pas seulement clear.
- **Transaction au pas** : stop → instantané → destruction du Gameplay/retrait
  des enregistrements → copie candidate chargée → codecs → World candidat restauré
  et validé → start au tick courant → publication. Ancien World garde ses demandes
  d'assets jusqu'au succès puis disparaît. Aucun emprunt/vtable/callback ancien
  actif. Échec : candidat détruit, ancien module reconstruit depuis l'instantané ;
  erreur affichée, input conservé, aucun événement rejoué.
  Les instantanés Play/Stop et Undo/Redo sont validés avec les nouveaux codecs.
  En Édition, start/stop valide un candidat puis l'instantané intact est restauré.
- **État/références** : World seul persiste. Hors World, toute entité est adressée
  par PersistentId, jamais par entt::entity durable. Start reconstruit l'état dérivé
  avec mapping/tick ; orbite de reset, portes, positions/historique restent dans World.
- **Images** : platform charge des copies uniques avec symboles de debug.
  Conservation jusqu'à sortie, y compris candidats entrés : Tracy peut retenir
  SourceLocationData. Limite 64 puis refus/redémarrage ; gestionnaire
  seul détenteur des handles inertes. Publication externe atomique, aucune compilation
  dans le moteur.

## Alternatives

Moteur statique dans chaque DLL : état dupliqué. Export automatique : surface
accidentelle, données Windows ambiguës. Déchargement physique : pointeurs Tracy.
Copie d'octets : migration de disposition impossible.

## Conséquences

ABI incompatible = redémarrage ; octets retenus affichés. Services exclus de
shipping ; profilage iteration manuel exigé aux jalons, macOS non bloquant.

Preuves : passage CI requis et Tracy manuel après intégration, contrôle final shipping installé.
