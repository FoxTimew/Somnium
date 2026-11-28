<!-- Décision de rechargement ; la compilation reste exclusivement hors ligne. -->
# ADR-0010 — Rechargement des shaders

Date : 27 novembre 2026. Statut : **accepté**.

## Contexte

Remplacer un shader sans compilateur dans le moteur, publication partielle ou
layout incompatible doit préserver les garanties de l'ADR-0004.

## Décision

- **Producteur externe** : un outil sous tools surveille HLSL et includes issus
  des depfiles, regroupe les changements puis relance la génération CMake existante.
  Versions épinglées ; configure ne bootstrappe rien.
  Aucune dépendance de compilation/réflexion n'est liée au moteur.
- **Publication par programme** : génération immuable contenant tous les étages,
  formats, réflexions et SHA-256 ; un manifeste de programme est remplacé en dernier,
  atomiquement sur le même volume. Les anciennes générations restent lisibles
  pendant la session. Échec de compilation : ancien manifeste inchangé et statut
  diagnostic atomique séparé pour debug_ui. Aucun artefact commité.
- **Contrat de layout** : les compteurs shadercross sont conservés, complétés hors
  ligne par `spirv-cross --reflect` de la version déjà épinglée. Le bootstrap
  construit et vérifie ce CLI. Signature structurelle : étages,
  interfaces vertex/fragment, sets/bindings, catégories/nombres de ressources,
  tailles, offsets, types et strides des uniformes. Une modification de cette
  signature produit le diagnostic dédié « redémarrage requis », distinct d'un
  artefact invalide. Les seuls compteurs ne valident pas le layout.
- **Consommateur** : service neutre platform de détection, limité à 4 Hz, actif
  seulement dans `iteration`, sur option `--watch-shaders on`.
  Il surveille manifeste/statut, pas les sources.
  Contrôle du schéma, des chemins logiques et des empreintes avant chargement.
  Un changement est mis en attente ; runtime le traite à la frontière de pas,
  et publie les ressources avant l'enregistrement de la prochaine frame.
- **Transaction GPU** : renderer garde le catalogue des programmes et descriptions
  de pipelines ; gpu reste l'enveloppe SDL. Shaders et pipelines dépendants
  sont recréés en candidats, y compris variantes de culling/format. Échec quelconque
  = candidats libérés, anciennes ressources actives et erreur contextualisée.
  Succès = échange du programme complet ; libération des anciens handles selon
  l'ADR-0004. Aucun paquet en cours d'enregistrement n'est modifié.
- **Diagnostic** : une erreur par génération, catégorie Log et panneau en lecture
  seule ; publication valide = erreur effacée. ADR-0002/0003 inchangés : vsMain/psMain,
  MSL 2.3, macOS 13.3 minimum ; metallib reporté.
- **Exclusion** : surveillance, rechargement et diagnostic d'itération ne sont
  compilés que pour `iteration`, liée statiquement jusqu'au lot 3 inclus ;
  shipping sans ces sources/cibles/options, vérifié par check_shipping.

## Alternatives

Compiler dans le moteur : interdit. Écraser les blobs avant leur manifeste :
générations mélangées. Remplacer un seul pipeline : matériaux incohérents.
Tolérer un changement d'uniforme de même taille : risque de corruption silencieuse.

## Conséquences

Manifeste et CLI de réflexion étendent la chaîne hors ligne. L'échange peut
temporairement doubler les ressources du programme.
Pas d'I/O, de parsing ni d'allocation de surveillance quand les outils sont inactifs.

Preuves : CI Windows/Radeon et Linux/lavapipe du lot 3 ; rechargement Metal en best effort.
