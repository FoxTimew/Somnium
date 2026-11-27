# Spike de rechargement du gameplay

Expérience autonome de phase 4, jamais incluse par le CMake du moteur. Les sources
existantes de core, scene, input et du rejeu sont compilées sans modification dans
une bibliothèque partagée. Les dépendances sont lues dans les déclarations CPM
du dépôt ; aucun changement de version ni copie de World.

```powershell
./tools/ci/windows.ps1 hot-reload-spike --jobs 2
```

```sh
bash tools/ci/linux.sh hot-reload-spike --jobs 2
```

Les mêmes commandes s'utilisent sans GitHub. Résultats sous
`<racine CI>/runs/<run>/hot-reload-spike/`, journaux et durées sous `logs/` et
`SUMMARY.md`. Windows/Linux testent les configurations plain et profiling ;
Linux ajoute ASan/UBSan. Le pipeline requis appelle cette opération avant les
builds moteur. Aucun test n'ouvre de fenêtre ou n'injecte d'entrée système.

Le contrat vérifie les vrais codecs et World, un composant à chaîne possédée dont
la disposition change, des types C++ distincts par génération, le rejeu neutre
continu, la conservation de la sérialisation, une constante de vitesse modifiée,
trois échecs récupérables (start, ABI, fichier invalide), dix répétitions et la
possibilité de remplacer une publication dont la copie est chargée.
Le filtre Log et l'adresse du client Tracy doivent être communs aux images.

Les images restent mappées jusqu'à la sortie, limite de 64 chargements : les zones
Tracy peuvent conserver des adresses de chaînes statiques. Avant chaque échange,
l'objet Gameplay, tous les codecs et le stockage EnTT du jeu sont détruits. Aucun
groupe, contexte ou signal du jeu n'est enregistré dans cette expérience.
La restauration reconstruit transactionnellement World ; les identités logiques
sont préservées, pas les adresses ni les valeurs EnTT. Les références du jeu sont
reconstruites dans start. Les sources moteur et game restent inchangées.

Ce spike ne constitue pas le chargeur de production : inspection ABI complète,
publication surveillée, leases de stockages/codecs, captures connectées à Tracy,
assets de scène et intégration sandbox restent à prouver après validation des ADR.
