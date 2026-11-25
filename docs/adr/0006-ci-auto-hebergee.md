<!-- Infrastructure et compromis de sécurité du poste personnel ; exploitation dans CI_RUNNER.md. -->
# ADR-0006 — CI auto-hébergée

Date : 5 novembre 2026, amendé le 25 novembre 2026. Statut : **accepté**.

## Contexte

Le poste personnel héberge Windows x64/Radeon RX 6600M et Ubuntu 24.04/WSL2.
Le propriétaire est le seul contributeur ayant le droit de pousser. Utiliser ses
comptes existants simplifie l'exploitation de cette machine personnelle.

## Décision

Windows fonctionne sous **Foxti**, tâche planifiée **non élevée en session
interactive** ; Linux utilise un service systemd **User=foxti**, jamais root.
Aucun compte dédié à ce stade. Tout code poussé s'exécute avec les droits du
propriétaire et peut accéder à ses fichiers ; **WSL peut aussi accéder aux disques
Windows montés sous /mnt**. WSL ne constitue donc pas une isolation du poste.

Garde-fous conservés : exécution non élevée, PR de forks refusées, jamais
pull_request_target, token contents: read, credentials checkout non persistés,
aucun secret injecté dans la CI. Ces règles ne protègent pas les données déjà
accessibles au compte personnel. Un **compte dédié devient obligatoire avant**
l'arrivée d'un second contributeur avec droit d'écriture, l'ajout de secrets à
la CI, ou l'ouverture d'un dépôt public acceptant des PR. Il faudra alors revoir
aussi les ACL et l'accès aux disques Windows depuis WSL.

Windows/D3D12/Vulkan matériel et Linux/lavapipe sont requis : quatre presets,
format, architecture, test de fenêtre, D5/cube ; tidy 18 et ASan/UBSan sur Linux.
Les captures Windows injectant souris/clavier sont **exclusivement dans un job
manuel workflow_dispatch**, hors job requis. Linux capture dans Xvfb isolé.
macOS reste best effort, manuel : build macos-latest, build/Metal macos-15, cible
13.3. **WARP n'est plus prouvé** ; les anciens résultats restent historiques.

Scripts locaux/CI communs, un runner par OS, vérifications courtes avant longues,
timeouts et annulation des runs dépassés par branche. Versions/SHA épinglés ;
caches CPM/shaders/sccache sur disque par hash, preuves par run, rétention 14 jours,
résumé dans le journal et GITHUB_STEP_SUMMARY. Aucun cache/artefact GitHub ni
transfert de chaîne entre jobs. WSL construit sur ext4 ; le test shader utilise
CMake/Ninja natifs et garde son timeout de 120 s.

## Alternatives

Compte dédié immédiat : meilleure séparation des fichiers, mais gestion de comptes
supplémentaire sur un poste utilisé seul. Runner Windows en service : pas de preuve
de présentation. CI hébergée requise : coûts et indisponibilité précédemment subis.

## Conséquences

La disponibilité dépend du poste, de la session Windows et de WSL. Les captures
manuelles demandent un bureau libre ; le job requis n'injecte aucun input global.
Le risque d'accès aux données personnelles est accepté dans le périmètre ci-dessus.
Installation : [CI_RUNNER.md](../CI_RUNNER.md) ; démarrage : [guide](../DEMARRER_RUNNERS.md).
Les futurs ADR scène/input prennent les numéros 0007/0008.

Preuves : CI des deux runners et capture D3D12 par job manuel vertes le 25 novembre sur 36f1824 (run 45622048422) ; D5/cube Radeon et lavapipe, fenêtre, tidy et ASan/UBSan validés ; aucun blocage du contrôle d'application Windows ; pièces dans le bilan de phase 2.
