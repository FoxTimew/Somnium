<!--
Précise le contrat accepté de scène et les invariants à couvrir par les tests.
Les preuves d'exécution sont consignées au point de contrôle et au bilan de phase 3.
-->
# Scènes et composants persistants

Le contrat est fixé par [ADR-0007](adr/0007-scene-entites-serialisation.md).
La scène est un World possédant directement son registre EnTT ; renderer ne lit
jamais ce registre. Les composants du jeu se déclarent et se sérialisent dans game.

## Identités et changements structurels

`PersistentId` est une chaîne opaque unique dans World. `Name` est une étiquette
modifiable, éventuellement dupliquée. Le jeu recherche une entité par `World::find`.
Les IDs créés à l'exécution ont la forme `runtime:0000000000000001` (16 chiffres
hexadécimaux). `nextId`, entier 64 bits positif sauvegardé dans le fichier, avance
sans revenir en arrière ; les IDs importés dans cet espace font également avancer
ce compteur. Une collision est sautée, l'épuisement est une erreur récupérable.
Les autres IDs importés peuvent être lisibles, par exemple `door-west-pivot`.

Création, destruction de sous-arbre et reparentage prennent effet immédiatement.
Ils passent par World et sont interdits pendant un parcours gardé : assertion en
debug, erreur InvalidState en release. Les vues EnTT directes utilisent aussi la
garde de World. L'index d'identités et les liens de hiérarchie ne sont jamais
modifiés directement dans le registre ; les autres composants restent accessibles
par les API EnTT. Un reparentage conserve les TRS locaux, rejette les cycles avant
mutation et synchronise l'historique du sous-arbre. Sa pose monde peut donc changer.

Transform contient deux TRS locaux (précédent/courant) et une matrice monde dérivée.
`beginStep` copie le courant, le jeu écrit le nouveau courant, puis `propagate`
recompose parent avant enfant. Une téléportation synchronise les deux TRS du
sous-arbre pour supprimer le trajet interpolé. Les échelles sont finies et non
nulles ; un changement de signe exige une téléportation. Camera, DirectionalLight
et tous leurs ancêtres ont une échelle exactement (1,1,1) ; chargement, reparentage
et changement de TRS vérifient cette contrainte. La caméra suiveuse est une racine.

## Fichier JSON v2 et migration v1

Le document contient `version`, `nextId`, `settings`, `activeCamera`, `entities`.
`settings` porte `ambient` (scalaire linéaire) et `clearColor` (RGBA linéaire).
`activeCamera` est un ID ou null ; une scène sans caméra n'est pas rendable.
Chaque entité contient `id` et un objet `components`. Chaque composant est un
enregistrement `version/data`, identifié par un nom stable qualifié :
`somnium.Name`, `somnium.Transform`, `somnium.Hierarchy`, `somnium.MeshRenderer`,
`somnium.Camera`, `somnium.DirectionalLight`, ou un nom du jeu comme `game.Door`.

Les parents et les références du jeu sont des PersistentId ; les modèles sont
des chemins logiques. Ni identifiants EnTT, ni handles, ni matrices monde, ni
historique temporel ne sont enregistrés. Transform v2 écrit `rotation: [x,y,z,w]`
sans renormalisation ni changement de signe ; la longueur au carré, calculée en
double, doit différer de 1 d’au plus `core::kQuaternionUnitTolerance = 1e-5`.
Transform v1 migre explicitement ses Euler degrés `Rz × Ry × Rx`. Camera v2
écrit `verticalFovRadians` pour éviter la perte d’un aller-retour degrés/radians.
L’interface d’édition garde une vue Euler en degrés. Entités, clés et frères sont
ordonnés par identité ; UTF-8, indentation de deux espaces et LF final.

## Codecs et transaction

La table de codecs associe un nom et une version à la présence du composant et
à ses fonctions serialize/deserialize et à une migration de version explicite.
Le jeu enregistre ses codecs avant le
chargement. Les fonctions renvoient Result ; le parseur n'utilise pas d'exceptions.
Composant/version inconnus, identités dupliquées, données invalides et références
absentes échouent avec le chemin du champ et l'ID de l'entité concernés.

Le chargement construit un World temporaire : identités et composants d'abord,
puis références différées, validation et propagation. Le contexte des codecs game
expose le même mécanisme de seconde passe que les codecs moteur ; un composant de
porte enregistre une référence à son pivot sans résoudre les IDs lui-même ni
capturer une adresse de composant qui pourrait être déplacée pendant le chargement.
La scène active n'est remplacée qu'après succès complet.

Le contexte d'assets demande les modèles par chemin logique et contrôle leur état.
Failed invalide la transaction avec le diagnostic du loader. World conserve les
demandes comptées ; destruction et échec les relâchent. Deux Worlds peuvent demander
le même modèle sans qu'abandonner l'un invalide l'autre. Le manager refuse un unload
forcé tant qu'une demande comptée existe et fournit le chemin inverse du handle
pour la sérialisation. Le manager et son lecteur vivent plus longtemps que World.

## Extraction et couverture

Tests CPU : génération/recherche/rechargement d'IDs, garde des trois opérations
structurelles, liens/sous-arbres/cycles, propagation/historique/téléportation,
échelles de caméra/lumière, aller-retour des codecs moteur et d'un codec défini
hors du moteur, référence vers une entité déclarée plus loin, diagnostic des
références absentes, rollback et libération des assets partagés.

Runtime prépare les ressources GPU par handle de Model et réutilise des buffers
pmr réservés pour l’extraction. À alpha, il interpole les TRS locaux (slerp sur
le chemin court), recompose les parents, puis multiplie chaque instance glTF.
Caméra active et lumière utilisent les mêmes poses. Les tests CPU vérifient
l’arc du pivot (et non une corde interpolée en monde), l’instance glTF, la caméra
parente et locale, le signe équivalent q/-q, les limites, la téléportation et
l’absence de croissance des buffers réservés.

Le sandbox charge `scenes/courtyard.json` avec les codecs `game.AabbCollider`,
`game.Character`, `game.Door`, `game.FollowCamera`. L’en-tête public des codecs
expose délibérément `nlohmann::json` ; game utilise `requireFields`, `readString`,
`readNumber`, `readVector3` et le résolveur commun. Un futur format binaire peut
partir de la même valeur. Le round-trip d’une porte en mouvement conserve sa
progression, référence au pivot et rotation fermée. Door v2 et FollowCamera v2
conservent l’état d’initialisation ; FollowCamera écrit les radians, l’orbite de
réinitialisation et l’orbite courante. Les versions 1 migrent explicitement.

`snapshot` capture la chaîne canonique et les données temporelles des codecs.
`restore` reconstruit transactionnellement World, redemande les modèles et restaure
l’historique sans l’écraser avec `beginStep`. Les callbacks temporels sont appariés ;
Transform les utilise pour son TRS précédent. Ces capacités sont testées dans
tous les presets, shipping compris. Les modes runtime et la caméra debug_ui sont
compilés seulement avec `SOMNIUM_ENABLE_ITERATION`. Le jeu conserve ses références
durables par PersistentId et reçoit `{mapping, tick}` au démarrage.

L’inspecteur parcourt World en lecture seule et sélectionne par ID persistant.
Dans iteration, chaque codec fournit facultativement des défauts JSON et des
indications de présentation de champs ; aucun callback ImGui n’entre dans les codecs.
Le brouillon générique est appliqué par une commande runtime au prochain pas.
Création/suppression/reparentage passent par World ; les modifications de composants
repassent par les mêmes lecteurs et le résolveur de références que le chargement.
Un World candidat absorbe les erreurs : asset absent, référence pendante, cycle,
échelle ou composant invalide ne changent pas la scène active ni dirty.

Les historiques des composants inchangés sont conservés. Modifier un TRS ou un
parent synchronise la branche concernée, sans effacer l’historique des autres
entités. Name, Transform et Hierarchy restent requis ; leur retrait est refusé.
Les nombres/quaternions non modifiés ne sont jamais réécrits par une vue Euler.
La migration Euler v1 et cette vue utilisent les mêmes conversions dans scene.
Save, refusé en Jeu, publie la chaîne canonique via platform ; Reload construit
également un candidat. Voir le [guide d’itération](ITERATION.md).

La [preuve analytique](SCENE_VALIDATION.md) reste le JSON pivot/enfant, trois poses,
référence et provenance inchangées. Les tests CPU de game chargent la fixture de
courtyard sans MeshRenderer ni GPU et vérifient vitesse, glissement, non-traversée,
portée/temps d’ouverture, passage, caméra et codecs. Voir le [bilan final](validation/phase-3/FIN_DE_PHASE.md).
