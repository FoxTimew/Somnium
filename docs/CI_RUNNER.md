<!-- Exploitation locale des runners ; aucun secret ni token d'enregistrement dans ce document. -->
# Runners Somnium

Windows et Linux/WSL2 sont requis. Le workflow macOS manuel reste best effort.
La politique est fixée par l'[ADR-0006](adr/0006-ci-auto-hebergee.md), amendé le
26 novembre 2026. Exploitation quotidienne : [Démarrer les runners Somnium](DEMARRER_RUNNERS.md).

## Installation Windows

Le runner FOXTIMEW-LAPTOP est enregistré avec `self-hosted`, `Windows`, `X64`.
Mode réel : **tâche planifiée non élevée en session interactive**, sous le compte
personnel **Foxti**, sans compte dédié. La tâche « Somnium Runner Windows » utilise
un jeton utilisateur non élevé ; les jobs ne s'exécutent jamais en administrateur.
Ce choix privilégie la praticité sur la machine personnelle d'un seul contributeur.

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

Pour réenregistrer le runner sous le compte personnel Foxti : arrêter son listener actuel,
puis suivre Settings → Actions → Runners → New self-hosted runner → Windows x64.
Décompresser et vérifier le package indiqué par GitHub dans `C:\ActionsRunner`,
exécuter `config.cmd` sans élévation (URL du dépôt, nom FOXTIMEW-LAPTOP,
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

Installation réelle : Ubuntu-24.04, compte personnel **foxti**, service systemd
**actions.runner.FoxTimew-Somnium.FOXTIMEW-WSL.service**, avec User=foxti et
WorkingDirectory=/home/foxti/actions-runner. Les jobs ne tournent pas sous root.
Le checkout Actions est sur ext4 dans /home/foxti/actions-runner/_work/Somnium/Somnium,
jamais sous /mnt/c. sudo est réservé à l'installation/administration du service.

```sh
# Passe administrative de paquets système :
sudo bash tools/ci/provision_wsl.sh --root /home/foxti/somnium-ci
# Passe portable depuis foxti, sans sudo :
bash tools/ci/provision_wsl.sh --root /home/foxti/somnium-ci
```

Le script impose Ubuntu 24.04, Clang/clang-tidy 18.1.3 (paquets exacts du verrou),
Python 3.12.12, CMake/Ninja/format/LFS/sccache épinglés et le SDK Vulkan vérifié.
Mesa fournit lavapipe ; les versions des paquets de plateforme restent consignées
dans les journaux. Xvfb fournit un écran pour les tests, sans dépendance à WSLg.

Dans GitHub : Settings → Actions → Runners → New self-hosted runner → Linux x64.
Exécuter les commandes de téléchargement et vérification données par GitHub dans
`/home/foxti/actions-runner`, puis `./config.sh` pour
`https://github.com/FoxTimew/Somnium`, nom `FOXTIMEW-WSL`, labels par défaut
`self-hosted,Linux,X64`, work `_work`. Saisir le token éphémère dans l'invite,
jamais dans un script, fichier versionné ou journal. Lancer `./run.sh` comme
foxti après `export PATH="$HOME/somnium-ci/tools/bin:$PATH"` : checkout
a besoin de Git LFS avant les scripts de build. Un service systemd avec
`User=foxti` et ce même PATH convient pour Linux ;
la tâche Windows « Somnium Runner WSL » maintient Ubuntu active via
C:/ActionsRunner/start_wsl.ps1 à l'ouverture de session. Vérifier les deux états Online.

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
`pipeline` ordonne version/format/architecture → shaders → debug/tests/GPU hors
écran/captures moteur → tidy Linux → release/shipping/profiling → sanitizers
Linux. Captures D3D12/Vulkan **requises** sous Windows interactif, Vulkan sous
Xvfb Linux : aucun pixel n'est lu sur le bureau. Un échec interrompt la suite.
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
Le hash sccache Windows inclut VSLANG=1033 : les sorties françaises de l'ancien
cache ne sont pas réutilisées. Après chaque build Windows, la CI refuse tout
objet du projet dont Ninja n'a enregistré aucune dépendance d'en-tête.

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
Le réglage a été aligné sur `all_external_contributors` le 25 novembre, puis relu
par l'API. Cette approbation préalable ne remplace pas le refus des PR de forks
dans les jobs. Appliquer l'obligation de compte dédié ci-dessous avant toute
ouverture aux contributions externes ; la visibilité temporaire du dépôt ne
change pas le mode d'exploitation convenu avec le propriétaire.
Le token par défaut est vérifié en lecture, sans approbation de PR par Actions.
Chaque workflow garde aussi l'égalité du dépôt source d'une PR avec le dépôt
cible ; aucun `pull_request_target`. Checkout ne conserve pas son credential.
Aucun secret n'est injecté dans les jobs. Les credentials d'enregistrement du runner
restent locaux ; aucun PAT de développement n'est ajouté aux workflows. Restreindre
les ACL des caches et du runner à Foxti, SYSTEM et administrateurs ; sous Linux,
limiter l'accès aux credentials à foxti. Le provisionnement ne change pas les réglages GitHub.

Le risque résiduel est accepté sur cette machine personnelle : **tout code poussé
s'exécute avec les droits utilisateur de Foxti/foxti**, y compris l'accès aux fichiers
personnels et, depuis WSL, aux **disques Windows montés sous /mnt**. Le refus des
forks, l'absence de secrets CI et le token en lecture ne cloisonnent pas les données
déjà accessibles à ces comptes. Les caches persistants partagent ce niveau de confiance.

Un **compte dédié devient obligatoire avant** l'un des événements suivants :

- second contributeur disposant d'un droit d'écriture ;
- ajout de secrets à la CI ;
- dépôt public acceptant des PR.

Ce changement impose aussi de revoir les ACL, les montages Windows dans WSL et
l'approbation des contributeurs externes. Aucun de ces déclencheurs n'est supposé
couvert par le simple passage à un jeton non élevé.

Windows exige Radeon RX 6600M pour D3D12 et Vulkan, avec couches actives ; **WARP
n'est plus couvert**. Linux impose lavapipe. Les captures Windows sont produites
par le moteur à l'extent de la swapchain, avec rejeu neutre de platform. Elles
font partie du pipeline requis, utilisent le build debug du même run et ne
dépendent ni du focus ni des fenêtres superposées ; voir le [guide](DEMARRER_RUNNERS.md).
Aucun assouplissement de politique
de contrôle d'application. Un blocage doit être rapporté avec le message exact.
macOS est manuel, ses deux jobs indépendants utilisent des disques éphémères.
Les petits PNG de scène et les frames composées produites, même si la comparaison échoue, sont
aussi conservés dans le journal avec empreinte SHA-256 et données encodées, puis
archivés explicitement dans le dépôt sous LFS. Un arrêt avant production des images
ne fournit qu'un journal d'échec. Aucun service d’artefacts GitHub. Les anciens résultats Metal
restent historiques et ne prouvent pas un nouveau commit.

Sources : [runner GitHub](https://docs.github.com/en/actions/how-tos/manage-runners/self-hosted-runners/configure-the-application),
[approbations des forks](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/enabling-features-for-your-repository/managing-github-actions-settings-for-a-repository),
[installation Vulkan](https://vulkan.lunarg.com/doc/view/latest/windows/getting_started.html),
[bootstrapper VS fixe](https://learn.microsoft.com/en-us/visualstudio/releases/2026/release-history).

### Rejeu de présentation (phase 3)

Le rejeu neutre remplace toute injection système. `--capture-output` demande au
moteur une cible intermédiaire de même format et taille que la swapchain : scène,
UI, copie GPU vers swapchain, soumission, relecture par transfer buffer/fence.
Le script commun `tools/presentation/capture.py` produit les PNG et vérifie les
pas, marqueurs gameplay, témoins UI et différences entre poses. Aucun accès aux
pixels du bureau, aux titres de fenêtres ou au focus. Les captures redeviennent
requises sous Windows ; des fenêtres apparaissent toujours, mais les recouvrements
et décorations ne contaminent plus les preuves. La session doit rester ouverte
avec une swapchain utilisable. Metal emploie le même chemin dans son workflow
manuel non bloquant. Voir [INPUT.md](INPUT.md).
