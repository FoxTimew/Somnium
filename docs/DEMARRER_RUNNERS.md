<!-- Commandes d'exploitation de l'installation observée le 25 novembre 2026. -->
# Démarrer les runners Somnium

Les deux runners utilisent les comptes personnels Foxti (Windows) et foxti (WSL).
Ouvrir la session Windows normalement, sans élévation. La politique et les conditions
imposant un futur compte dédié sont dans [CI_RUNNER.md](CI_RUNNER.md).

## Démarrage courant

Les tâches **Somnium Runner Windows** et **Somnium Runner WSL** démarrent à
l'ouverture de session, avec privilèges limités et utilisateur connecté.
Pour les lancer manuellement depuis PowerShell **non administrateur** :

```powershell
Start-ScheduledTask -TaskName 'Somnium Runner Windows'
Start-ScheduledTask -TaskName 'Somnium Runner WSL'
Get-ScheduledTask -TaskName 'Somnium Runner*' | Select-Object TaskName, State
```

Windows lance C:\ActionsRunner\start.ps1, qui ajoute Git LFS épinglé au PATH
puis lance le listener installé dans C:\ActionsRunner. Il refuse l'élévation,
la session 0 et évite de lancer une seconde instance.
La tâche WSL lance C:\ActionsRunner\start_wsl.ps1 : elle garde Ubuntu-24.04
active avec un processus sleep ; le runner lui-même est le service systemd
actions.runner.FoxTimew-Somnium.FOXTIMEW-WSL.service, sous foxti.

## Vérifier les deux runners

```powershell
wsl -d Ubuntu-24.04 -u foxti -- systemctl is-active actions.runner.FoxTimew-Somnium.FOXTIMEW-WSL.service
wsl -d Ubuntu-24.04 -u foxti -- systemctl show actions.runner.FoxTimew-Somnium.FOXTIMEW-WSL.service -p User -p WorkingDirectory
```

Dans GitHub → Somnium → Settings → Actions → Runners : **FOXTIMEW-LAPTOP**
et **FOXTIMEW-WSL** doivent être Online. Busy signifie qu'un job est en cours ;
ne pas démarrer un second listener. Le service Linux s'exécute dans
/home/foxti/actions-runner ; ses caches sont dans /home/foxti/somnium-ci.
Sous Windows, caches et résultats sont dans C:\SomniumCI.

Si le service Linux est arrêté, dans un terminal Ubuntu :

```sh
sudo systemctl start actions.runner.FoxTimew-Somnium.FOXTIMEW-WSL.service
journalctl -u actions.runner.FoxTimew-Somnium.FOXTIMEW-WSL.service -n 40 --no-pager
```

Seul le démarrage administratif du service utilise sudo ; ses jobs restent sous
foxti. Ne pas relancer run.sh à côté du service. Avant une maintenance, attendre
que les jobs finissent. Arrêter le service avec systemctl stop si nécessaire ;
ne pas utiliser wsl --shutdown, qui interromprait aussi les autres distributions.

## Captures Windows, uniquement à la demande

Le job requis ouvre brièvement une fenêtre de test et vérifie D5/cube hors écran.
Il **n'injecte ni souris ni clavier**. Les captures du modèle, orbite, zoom, reset
et resize injectent de vrais événements dans le bureau : garder la session
déverrouillée et laisser souris/clavier libres pendant le job manuel.

Depuis un terminal disposant de GitHub CLI, sélectionner explicitement la branche :

```sh
gh workflow run ci.yml --repo FoxTimew/Somnium --ref phase/2-assets-renderer -f presentation=true
```

Le bouton Actions → CI self-hosted → Run workflow sera disponible lorsque la
version de `ci.yml` sur la branche par défaut déclarera workflow_dispatch. Choisir
alors la branche à prouver et cocher presentation. Ne pas fusionner dans main
uniquement pour faire apparaître ce bouton.

Le job windows-presentation attend le build Windows du même run, utilise ses
binaires debug puis conserve les captures dans
C:\SomniumCI\runs\<run-id>-<tentative>\presentation-direct3d12.
Les journaux et GITHUB_STEP_SUMMARY donnent le résultat et les chemins. Sans
l'option presentation, même un workflow_dispatch ne capture pas le bureau.
