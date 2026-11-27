<!-- Plan accepté ; lots 1 et 2 intégrés, lots 3 et 4 après le jalon intermédiaire. -->
# Phase 4 — Plan de tests et conditions de passage

Le périmètre est celui d'[AGENTS.md](../AGENTS.md), §6. Les
[ADR-0009](adr/0009-instantanes-edition-enregistrement.md),
[ADR-0010](adr/0010-rechargement-shaders.md) et
[ADR-0011](adr/0011-module-gameplay.md) sont acceptés avec les précisions du
27 novembre. Le [spike](HOT_RELOAD_SPIKE.md) reste autonome sous tools.

## Acquis à préserver à chaque push

Windows/Radeon D3D12 et Vulkan, Linux/lavapipe : D5, cube et scène avec les mêmes
références, seuils, sondes et provenances ; zéro différence reste attendu sur les
références actuelles. Rejeu et tests CPU de gameplay inchangés. Les captures restent
produites par le moteur, sans entrée système ni lecture du bureau. Format 18,
clang-tidy 18, warnings en erreurs, frontières CMake/scanner, debug/release/shipping/
profiling et ASan/UBSan Linux restent requis. macOS manuel reste non bloquant.

Les tests des lots 1 et 2 sont intégrés. Ceux des lots 3 et 4 restent le contrat
de leurs étapes ultérieures, après validation du jalon intermédiaire.

## Lot 1 — Instantanés et modes

| Preuve | Conditions et oracle |
| --- | --- |
| Exactitude | World avec hiérarchie, références de jeu, réglages, nextId et rotations arbitraires/proches des pôles. Capture, N pas synthétiques, restauration : égalité des octets canoniques et des TRS précédent/courant ; pas seulement égalité visuelle. |
| Versions | Scène v1 Euler migrée explicitement selon ADR-0002 ; v2 quaternion, signe et floats conservés sans renormalisation si abs(dot(q,q)-1) ≤ 1e-5 en double. Tester la frontière, le rejet au-delà et les non-finis. Version/composant inconnu rejeté. Chargement fichier synchronise l'historique ; restauration mémoire le conserve. |
| Frères | World impose l'ordre lexicographique des PersistentId, racines et frères. Créations/reparentages dans des ordres différents donnent les mêmes liens/parcours ; snapshot/restauration conserve exactement cet ordre. |
| Transaction | Asset absent/Failed, ID manquant, codec de jeu en erreur et échelle de caméra interdite : World actif, sélection par ID et dirty inchangés. Le candidat libère toutes ses demandes. |
| Durées de vie | 100 cycles Jouer/Arrêter ; compteurs de requêtes/libérations équilibrés, pas de ressource restante après destruction. ASan/LSan/UBSan et test de callbacks retirés. |
| Mode | Zéro fixedUpdate en Édition, start/stop appelés une seule fois par transition ; démarrage par défaut en Jeu reproduit le comportement actuel. Échec de start restitue l'édition et ne publie aucun état partiel. |
| Inputs/temps | Pas de rattrapage du temps passé en Édition, pas de transition accumulée rejouée au retour. Le tick des services reste monotone ; rechargement en Jeu conserve l'objet Input et la file d'événements. Tester changements de mode entre deux événements d'un même pas. |

L'instantané ne contient aucun pointeur, valeur EnTT ou handle d'asset. La vue
persistante et les données temporelles sont produites par les mêmes codecs dans
une seule capture ; l'enregistrement ne refait pas un parcours concurrent de World.
L'état hors World du jeu actuel est audité : caches reconstructibles autorisés,
orbite de reset déplacée dans un composant, tick initial fourni par le contexte
de démarrage. Des références empruntées ne survivent jamais à une transaction.
Toute référence d'entité durable hors World est un PersistentId. Instantané et
sérialisation canonique v2 sont testés dans tous les presets, shipping compris ;
les modes et outils seulement dans iteration. Lots 1 à 3 : jeu et moteur statiques.

## Lot 2 — Édition et enregistrement

- Scénario CPU et capture moteur : créer une entité, assigner le modèle original,
  déplacer, renommer, reparenter, ajouter/éditer/retirer un composant de jeu, supprimer
  un sous-arbre. Vérifier IDs, ordre, références et résultat de la validation.
- Composant défini uniquement dans un consommateur de test : defaults et codec
  enregistrés suffisent à l'éditeur générique. Aucun branchement ImGui sur son nom
  ou son type. Quaternion présenté en Euler via une métadonnée de champ générique ;
  aucune conversion si l'utilisateur n'applique pas de modification.
- Une édition invalide ne change aucun composant ni l'état dirty. Les structures
  requises restent protégées ; supprimer une cible référencée doit être rejeté ou
  faire partie d'une transaction qui corrige explicitement les références.
- Caméra d'outil indépendante du World, jamais sérialisée ; ses commandes et la
  scène restent manipulables quand la simulation est arrêtée. Son mapping n'est
  jamais fusionné à celui du jeu et ne crée pas un système de contextes générique.
- Deux sauvegardes sans modification produisent exactement les mêmes octets.
  Redémarrer le processus, recharger la scène enregistrée, comparer sa valeur
  canonique ; tester négatifs, petites/grandes valeurs finies, quaternions et chaînes.
- Écriture atomique platform : échec avant renommage, disque non accessible,
  destination modifiée depuis la lecture, chemin sortant de la racine, arrêt avant
  publication. Le précédent fichier reste complet ; le temporaire n'est jamais lu
  comme scène. Tester Windows et Linux avec injection d'échec au point de publication.
- Enregistrer en Jeu est refusé. Recharger une édition dirty ne détruit rien sans
  abandon explicite. Échec de sauvegarde conserve dirty ; succès met à jour la base
  de comparaison seulement après publication.
- Mesurer 200 captures sur la cour après chauffe : p50/p95/max, taille JSON et
  mémoire retenue. Si p95 ≤ 1 ms et 16 états tiennent dans 16 Mio, implémenter une
  pile Annuler/Rétablir bornée ; sinon consigner son report au bilan intermédiaire.
  Mesure locale iteration Debug : p95 7,20 ms, 16 états/552 432 octets ; annulation
  reportée. Si activée ultérieurement : nouvel edit invalide la branche redo ; échec de restauration ne
  dépile pas l'état et ne change pas les demandes d'assets.

Le réenregistrement des seuls `assets/scenes/*.json` est isolé dans un commit dédié,
après les tests de migration. Les fixtures des preuves analytiques restent en v1
pour préserver leurs empreintes et exercer la compatibilité de lecture.

**Arrêt obligatoire après le lot 2** : CI verte et captures moteur montrant
édition, résultat enregistré/rechargé et retour à l'édition après Jeu ; validation
utilisateur avant shaders et module de production.

## Lot 3 — Shaders

| Preuve | Conditions et oracle |
| --- | --- |
| Publication | Lire pendant l'écriture de la génération suivante : ancien ou nouveau programme complet, jamais mélange. Manifestes tronqués, hash erroné, fichier manquant et chemin interdit rejetés. |
| Includes | Modifier un include transitif déclenche la génération existante ; aucun changement = aucun build, aucune publication. Erreur HLSL conserve l'ancien manifeste et publie un diagnostic borné. |
| Réflexion | Compteurs égaux mais type/offset/stride d'un uniforme modifié, vertex input incompatible ou sortie fragment incompatible : diagnostic « redémarrage requis », distinct d'un artefact invalide, avant création/échange. Signature issue du SPIR-V et comparée au contrat du renderer ; CLI de réflexion hors ligne épinglé. |
| Rendu | Variante de fragment à couleur constante analytique A puis B, capture hors écran après échange ; ensuite artefact invalide, image strictement B ; nouvelle publication valide A rétablie. Référence dédiée, jamais une nouvelle référence D5/cube/scène. |
| Ressources | Plusieurs pipelines dépendants, dont culling miroir et format de cible différent ; échec du dernier candidat laisse tous les anciens actifs. Handles invalidés/libérations différées selon ADR-0004, soumission toujours respectée. |
| Coût | Outils inactifs : aucun accès disque, hash, parsing ou allocation dans update/extraction/record. Actifs : au plus quatre sondages par seconde et diagnostic dédupliqué par génération. |

D3D12 et Vulkan Radeon, Vulkan lavapipe requis. Metal reste manuel best effort.
La publication garde les formats SPIR-V/DXIL/MSL ensemble ; MSL/metallib ne change pas.

## Lot 4 — Gameplay

Étendre la preuve du spike au chargeur réel et au jeu de la cour :

1. Enregistrer le module, démarrer et rejouer jusqu'à une porte partiellement ouverte.
   Capturer position, progression, caméra, identités, historique et demandes d'assets.
2. Recompiler v2 avec une vitesse différente et une disposition de composant modifiée.
   Charger une copie avec symboles ; assurer que la sortie du linker reste remplaçable.
3. Au tick courant, comparer l'instantané avant/après échange puis vérifier le nouveau
   déplacement et la continuation de la porte. Aucun événement perdu ni rejoué.
4. Module absent, binaire invalide, symbole/ABI/empreinte refusés, codec incompatible,
   asset Failed, puis start qui modifie son candidat avant d'échouer : ancien jeu
   restauré à l'identique, diagnostic visible, progression suivante correcte.
5. Tester retrait des codecs, pools, groupes/contextes/signaux et tout abonnement
   extérieur déclaré. Détruire le World retiré avant de relâcher ses enregistrements
   de support ; aucune adresse de composant ni vtable n'est réutilisée. Vérifier que
   seules les images inertes et les métadonnées Tracy autorisées restent retenues.
6. Répéter les cycles et atteindre la limite de 64 images : refus récupérable,
   ancien jeu actif, compteur mémoire affiché. ASan/LSan/UBSan sur hôte, image moteur
   et modules ; profilage avec collecteur Tracy connecté et événements après échange.

Tous les scénarios logiques sont sans fenêtre, avec rejeu JSON neutre, sur Windows
et Linux. Une preuve sandbox complète ensuite le changement de constante sans
redémarrage. Les presets statiques et `iteration` utilisent les mêmes sources et
le même contrat Gameplay révisé ; un changement ABI moteur impose un redémarrage.
La garde CI contrôle les exports explicites et les objets/archives du module :
aucune unité moteur liée dans game. Les en-têtes publics Somnium ne définissent
aucun état mutable inline/statique ; les données exportées passent par accesseurs
dans l'image moteur. Tests négatifs avec objet moteur et état mutable injectés.

## CI et shipping

Les workflows restent des enveloppes : `tools/ci/windows.ps1 pipeline --jobs 2
--window-mode interactive` et `bash tools/ci/linux.sh pipeline --jobs 2`. Le spike
dispose déjà de l'opération `hot-reload-spike`, identique en local et CI. Cache CPM
et sccache existants, preuves dans un répertoire par run, aucune dépendance à
actions/cache ou au service d'artefacts. Budget global conservé : 120 min Windows,
150 min Linux comme limites d'arrêt ; objectif mesuré de **moins de 60 min par
push et par runner à cache chaud**. Profilage d'iteration dans des jobs manuels,
exigés aux points de contrôle. Le bilan intermédiaire consigne la durée totale
de chaque job requis et manuel, SHA, caches et commandes. Les timings du spike
restent dans son compte rendu ; aucune mesure n'est annoncée comme cache froid.

`check_shipping` est déjà étendu et testé contre des injections de :

- cibles UI/inspecteur, `somnium_iteration*`, `somnium_editor*`, `somnium_file_watch*`,
  `somnium_shader_reload*`, `somnium_module_loader*`, `somnium_game_module*`,
  `somnium_engine_shared` et `spike_*` ;
- sources d'itération, y compris chemins Windows normalisés, et définitions actives
  ou simplement présentes des options ITERATION/SHADER_RELOAD/GAMEPLAY_RELOAD ;
- options CMake activées, racine de développement ou mode autre que COOKED.

L'absence des nouvelles options est admise tant que les lots ne sont pas intégrés.
Les options d'outils seront OFF dans tous les presets existants, ON dans `iteration`,
avec variante de profilage manuelle. Snapshot/v2 ne dépendent pas de ces options.
La liaison partagée est introduite seulement au lot 4. Le configure refusera
iteration sans debug_ui ou avec COOKED. Les cibles/services porteront les noms
contrôlés ; à leur introduction, compléter le contrôle par inspection des dépendances
du binaire shipping et de son installation : aucun module de jeu ni image moteur
partagée ni outil. Le symbole SDL_LoadObject seul n'est pas un critère, SDL pouvant
l'utiliser pour ses propres pilotes. Les six tests négatifs/positifs du scanner
sont exécutés avant le build, puis son contrôle réel après configure shipping.
