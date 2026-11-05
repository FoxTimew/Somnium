<!-- Exploitation locale des runners ; aucun secret ni token d'enregistrement dans ce document. -->
# Runners Somnium

Windows et Linux/WSL2 sont requis. Le workflow macOS manuel reste best effort.
La politique est fixée par l'[ADR-0006](adr/0006-ci-auto-hebergee.md).

## Installation Windows

Le runner FOXTIMEW-LAPTOP est enregistré avec `self-hosted`, `Windows`, `X64`.
Mode retenu : **session interactive**, pas service. Il doit utiliser un compte
local standard dédié (par exemple `somnium-ci`), sans droits d'administration,
sans identifiants de développement personnels et sans accès aux dossiers privés.

Les versions sont épinglées dans [toolchain.json](../tools/ci/toolchain.json) :
Python 3.12.12, CMake 3.31.6, Ninja 1.11.1.3, MSVC 14.51.36231 (VS 18.11.1),
Windows SDK 10.0.26100.0, LLVM/clang-format/clang-tidy 18.1.8,
Git LFS 3.7.0, Vulkan SDK 1.4.363.0, sccache 0.18.0.
MSVC inclut les ressources anglaises (`en-US`, `VSLANG=1033`) : les diagnostics
français non UTF-8 sont altérés par sccache 0.18 en sortie redirigée, ce qui casse
le suivi d'en-têtes Ninja ([défaut amont](https://github.com/mozilla/sccache/issues/2864)).
`doctor` refuse cette configuration incomplète et les sessions élevées.

Depuis le dépôt, dans un PowerShell **administrateur**, une fois :

```powershell
& ./tools/ci/provision_windows.ps1 -Root C:/SomniumCI -Machine
```

Cette passe installe les outils absents et Graphics Tools pour la couche debug
D3D12. Elle réutilise MSVC si la version exacte est déjà présente. Les téléchargements
sont vérifiés par SHA-256 ; aucune modification du contrôle d'application.
Le compte runner peut répéter la passe portable sans `-Machine` ; l'exécution CI
n'appelle jamais la passe administrative. Un redémarrage demandé par l'installateur
doit être fait avant de lancer les jobs.

Pour réenregistrer le runner sous le compte dédié : arrêter son listener actuel,
puis suivre Settings → Actions → Runners → New self-hosted runner → Windows x64.
Décompresser et vérifier le package indiqué par GitHub dans `C:\ActionsRunner`,
exécuter `config.cmd` comme compte standard (URL du dépôt, nom FOXTIMEW-LAPTOP,
labels par défaut `self-hosted,Windows,X64`, work `_work`), sans installer de service.
Saisir le token d'enregistrement éphémère à l'invite. Ne pas conserver de token
personnel ; retirer l'ancienne inscription si GitHub demande de remplacer le nom.

Créer une tâche planifiée pour ce compte : déclencheur à son ouverture de session,
« Exécuter uniquement si l'utilisateur est connecté », privilèges normaux,
copier `tools/ci/start_windows_runner.ps1` dans `C:\ActionsRunner\start.ps1`,
action `powershell.exe -NoProfile -File C:\ActionsRunner\start.ps1`, dossier
`C:\ActionsRunner`. Ce lanceur expose Git LFS épinglé avant l'étape checkout.
Ne pas lancer un second listener si l'instance actuelle est déjà active. Conserver
la session ouverte et déverrouillée pendant les preuves de présentation.
La commande `windows.ps1 doctor` contrôle la session ; session 0 échoue en mode
interactive. L'option locale `--window-mode service` marque fenêtres/captures
« non validé » ; elle n'est pas utilisée par le workflow requis.

## Installation Linux dans WSL2

Dans PowerShell administrateur, si Ubuntu n'existe pas :

```powershell
wsl --install -d Ubuntu-24.04
```

Initialiser la distribution puis créer un compte Linux `somnium-ci` standard,
sans sudo permanent. Utiliser un compte d'installation distinct pour la passe
administrative. Cloner le dépôt **dans `/home/somnium-ci/Somnium`**, jamais sous
`/mnt/c`. Le checkout initial privé peut être transféré par bundle Git et objets
LFS pour éviter de conserver un PAT dans WSL.

```sh
# Passe de paquets système, depuis un compte administrateur Ubuntu :
sudo bash tools/ci/provision_wsl.sh --root /home/somnium-ci/somnium-ci
# Passe portable depuis le compte runner standard :
bash tools/ci/provision_wsl.sh --root /home/somnium-ci/somnium-ci
```

Le script impose Ubuntu 24.04, Clang/clang-tidy 18.1.3 (paquets exacts du verrou),
Python 3.12.12, CMake/Ninja/format/LFS/sccache épinglés et le SDK Vulkan vérifié.
Mesa fournit lavapipe ; les versions des paquets de plateforme restent consignées
dans les journaux. Xvfb fournit un écran pour les tests, sans dépendance à WSLg.

Dans GitHub : Settings → Actions → Runners → New self-hosted runner → Linux x64.
Exécuter les commandes de téléchargement et vérification données par GitHub dans
`/home/somnium-ci/actions-runner`, puis `./config.sh` pour
`https://github.com/FoxTimew/Somnium`, nom `FOXTIMEW-WSL`, labels par défaut
`self-hosted,Linux,X64`, work `_work`. Saisir le token éphémère dans l'invite,
jamais dans un script, fichier versionné ou journal. Lancer `./run.sh` comme
compte standard après `export PATH="$HOME/somnium-ci/tools/bin:$PATH"` : checkout
a besoin de Git LFS avant les scripts de build. Un service systemd avec
`User=somnium-ci` et ce même PATH convient pour Linux ;
Windows peut démarrer WSL à l'ouverture de session. Vérifier les deux états Online.

## Commandes et stockage

Entrées identiques en local et CI : `tools/ci/windows.ps1` et `tools/ci/linux.sh`,
suivies d'une opération et des mêmes options. Exemples :

```powershell
& tools/ci/windows.ps1 doctor --run-id manuel
& tools/ci/windows.ps1 shader-toolchain --run-id manuel
& tools/ci/windows.ps1 configure --preset debug --run-id manuel
& tools/ci/windows.ps1 build --preset debug --run-id manuel
& tools/ci/windows.ps1 test --preset debug --run-id manuel
& tools/ci/windows.ps1 gpu --backend direct3d12 --run-id manuel
& tools/ci/windows.ps1 captures --backend direct3d12 --run-id manuel
```

Autres opérations : `format`, `compiler-cache`, `tidy`, `sanitizers`, `cleanup`, `pipeline`.
`pipeline` ordonne version/format/architecture → shaders → debug/tests/GPU/captures
→ tidy Linux → release/shipping/profiling → sanitizers Linux. Un échec interrompt la suite.
Garder le même `--run-id` pour reprendre un configure/build/test. Aucun accès
GitHub n'est requis hors acquisition des dépendances publiques au premier passage.

Racines par défaut : `C:\SomniumCI` et `$HOME/somnium-ci` sous Linux ; override
`--root`/`SOMNIUM_CI_ROOT`. `tools/` contient les installations ; `cache/cpm/<hash>`,
`cache/shaders/<hash>` et `cache/sccache/<hash>` sont persistants. Le hash shader
inclut le lock, son bootstrap et les versions hôtes. Le CLI installé est revalidé
à chaque run et n'est jamais transmis entre jobs. sccache est limité à 20 GiB/OS.
Les ports des serveurs sccache diffèrent entre Windows et WSL.
Le serveur est relancé sous verrou avant un build pour prendre le répertoire
du hash courant. Un probe compile un en-tête modifié deux fois pour vérifier
que Ninja recompile effectivement à travers sccache, y compris après retour au cache.

`runs/<run-id>/build/<preset>/bin` contient les binaires et shaders ; les captures,
rapports GPU, logs et `SUMMARY.md` sont voisins. Le journal GitHub et le résumé du
job donnent leurs chemins ; aucune sauvegarde GitHub des artefacts ou caches.
Prévoir au moins 100 GiB disponibles pour les deux OS. La rétention supprime les
runs de plus de **14 jours**, identifiés par leur marqueur et bornés à la racine
des runs ; elle ne suit aucun lien/jonction et ne supprime jamais les caches.
Les pièces de phase retenues sont copiées explicitement dans `docs/validation/`
(PNG sous LFS) avant expiration.

## Sécurité et couverture

Configurer Settings → Actions → General → approbation des workflows de fork pour
**tous les contributeurs externes**, permissions du token en lecture seule.
Sur ce dépôt **privé**, GitHub n'expose pas le réglage public « all outside
collaborators » : l'API dédiée répond 422. Vérification du 5 novembre : workflows
de PR de forks désactivés, envoi de tokens write et de secrets désactivés ; garder
cette interdiction. Si les workflows de forks privés sont un jour autorisés,
activer « Require approval for fork pull request workflows » avant toute exécution.
Le token par défaut est déjà en lecture, sans approbation de PR par Actions.
Chaque workflow garde aussi l'égalité du dépôt source d'une PR avec le dépôt
cible ; aucun `pull_request_target`. Checkout ne conserve pas son credential.
Ne pas injecter de secrets de déploiement ni réutiliser un compte personnel.
Restreindre les ACL de `C:\SomniumCI` et du dossier du runner au compte dédié,
SYSTEM et administrateurs ; sous Linux, permissions 0700 sur le home et les
credentials du runner. Le provisionnement ne change pas les réglages GitHub.

La garde de fork ne protège pas contre un collaborateur autorisé qui modifierait
le workflow ; ne donner l'écriture qu'aux personnes dont le code peut s'exécuter
sur ce poste. Les caches persistants ont le même niveau de confiance.

Windows exige Radeon RX 6600M pour D3D12 et Vulkan, avec couches actives ; **WARP
n'est plus couvert**. Linux impose lavapipe. Les captures Windows viennent de la
fenêtre réelle et d'événements souris/clavier ; aucun assouplissement de politique
de contrôle d'application. Un blocage doit être rapporté avec le message exact.
macOS est manuel, ses deux jobs indépendants conservent leurs résultats seulement
sur le runner éphémère et dans les résumés ; publier une pièce retenue nécessite
une copie explicite pendant le run. Les anciennes preuves Metal restent historiques.

Sources : [runner GitHub](https://docs.github.com/en/actions/how-tos/manage-runners/self-hosted-runners/configure-the-application),
[approbations des forks](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/enabling-features-for-your-repository/managing-github-actions-settings-for-a-repository),
[installation Vulkan](https://vulkan.lunarg.com/doc/view/latest/windows/getting_started.html),
[bootstrapper VS fixe](https://learn.microsoft.com/en-us/visualstudio/releases/2026/release-history).
