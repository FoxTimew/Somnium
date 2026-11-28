<!-- Usage des outils d’édition et de rechargement de shaders.
     Les preuves partagent les commandes de l’UI et la publication hors ligne. -->
# Modifier et enregistrer une scène

Configurer/compiler/tester avec le preset `iteration`, puis lancer
`build/iteration/bin/sandbox --mode edit` (ajouter `.exe` sous Windows).
Le moteur et le jeu restent statiques. Les presets sans iteration démarrent en Jeu et
n’incluent aucun outil d’édition ; shipping conserve les capacités de snapshot/v2
de scene, sans interface, mode d’édition ni écriture de sources.

Pour recharger les shaders, ajouter `--watch-shaders on` et lancer en parallèle
`python tools/shaders/watch.py --build build/iteration --config Debug`.
Voir le [protocole de publication et ses preuves](SHADERS.md#rechargement-en-iteration).

## Édition

La caméra d’outil est indépendante de la scène : bouton droit et souris pour
regarder, W/A/S/D pour se déplacer, Q/E verticalement, molette pour avancer/reculer.
Elle n’est jamais enregistrée. Les inputs captés par ImGui ne la commandent pas.
Sans caméra active dans la scène, elle démarre à (0, 3, 6) m : une scène dont
la caméra a été supprimée reste éditable et peut être réparée.

Dans la hiérarchie, sélectionner une entité par son identité persistante.
**Create entity** crée une racine avec un ID généré ; **Delete subtree** supprime
toute sa branche. **Parent ID** puis **Reparent** change son parent ; laisser
l’ID vide pour une racine. Un cycle ou une référence pendante est refusé.

Choisir **Component**, modifier les champs, puis **Apply validated edit**.
Renommer via `somnium.Name`. **Add** propose les codecs absents ; les défauts
ne dispensent pas de compléter les références, par exemple le chemin
`models/courtyard-wall.glb` de MeshRenderer. **Remove component** retire un
composant facultatif ; Name, Transform et Hierarchy restent requis.
Le jeu fournit ses codecs et des indications de champs, jamais du code ImGui.
Les quaternions ont une vue Euler en degrés ; aucune conversion n’est appliquée
sans changement. **Raw JSON** permet aussi d’éditer objets, tableaux et valeurs nulles.
**Scene settings** expose l’ambiant, le fond et l’ID de la caméra active.

Une erreur de validation conserve le World actif et l’indicateur de modifications.
Les règles de gameplay plus larges restent vérifiées par `start` : par exemple,
les colliders de la cour doivent être des racines. Un Play refusé laisse l’édition
intacte. Les modifications de brouillon doivent être appliquées avant Save.

## Jouer, arrêter, enregistrer

Sans `--mode edit`, le sandbox démarre en Jeu. **Play** prend un instantané,
restaure un candidat puis appelle start au tick courant. **Stop** arrête le jeu
et restaure exactement l’état d’édition, y compris les historiques, les IDs et
les demandes d’assets. Aucun fixedUpdate de jeu n’est appelé en Édition.

**Save** est refusé en Jeu. En Édition, la scène chargée est enregistrée en v2
canonique : clés/entités triées, UTF-8/LF, floats et quaternions à aller-retour exact.
Deux Save successifs ne changent ni les octets ni le fichier s’il est déjà identique.
**Reload** valide un candidat ; cocher l’abandon des modifications pour recharger
une scène dirty. Un fichier invalide ou un asset Failed ne remplace pas World.

**Undo/Redo** restaurent les états complets, y compris les historiques des codecs.
La pile commune contient au plus 16 états et 16 Mio ; les états les plus éloignés
du curseur sont évincés. Une édition nouvelle coupe la branche Redo ; une erreur
de validation ou de restauration ne change ni la pile ni le World actif.
Save conserve la pile : revenir à sa valeur canonique éteint l’indicateur dirty.
Reload réussi vide la pile ; Play/Stop la conserve, mais Undo/Redo sont refusés
en Jeu. Une opération dont un instantané dépasse seul 16 Mio est refusée avec
un diagnostic ; elle ne sacrifie pas silencieusement l’historique.

La racine source par défaut est `assets/` du dépôt. `--source-root /chemin/absolu`
permet une autre racine existante ; `--scene scenes/exemple.json` choisit la scène.
La lecture essaie les shaders compilés à côté du binaire, cette racine, puis les
sources du dépôt. Seule la racine explicitement choisie reçoit les écritures.
Aucun asset source n’est copié dans le build ; aucun Save As n’est implicite.

Platform refuse les chemins sortants et symlinks d’évasion, vérifie les octets
observés, écrit un temporaire voisin exclusif, le synchronise, revérifie la source
puis le remplace atomiquement. Une modification externe impose Reload. Ce contrôle
optimiste ne constitue pas un verrou entre éditeurs concurrents. Sur POSIX,
un échec de synchronisation du répertoire après renommage est signalé comme
« publié, durabilité incertaine » ; il ne prétend pas que le fichier est inchangé.

## Preuves et coût

`python tools/iteration/prove.py --executable /chemin/sandbox --backend vulkan
--output /chemin/preuves` exécute les commandes versionnées et les événements neutres.
Le scénario crée, assigne un modèle, déplace, reparente, édite un composant de jeu,
retire un composant/sous-arbre, teste des refus puis Save/Play/Stop. Un second
processus recharge le fichier publié. Les images scène + UI viennent du moteur,
sans lecture du bureau ni injection système. La zone de scène après Stop et
redémarrage doit être strictement identique à celle de l’édition enregistrée.
Les fichiers JSON sont préparés uniquement dans le répertoire de preuve.

`tools/ci/windows.ps1 iteration-proof --backend direct3d12 --run-id <run>` et
`bash tools/ci/linux.sh iteration-proof --backend vulkan --run-id <run>` utilisent
le binaire iteration construit dans ce run. Les jobs requis les appellent sur
D3D12/Vulkan Radeon et Vulkan lavapipe. Les preuves D5/cube/scène restent inchangées.

CTest lance `somnium_iteration_benchmark <assets-absolu> <rapport.json>` :
20 chauffes, 200 captures et restaurations, p50/p95/max et mémoire de 16 états.
La capture Debug de 5–7 ms et optimisée de 0,3 ms est payée par édition validée,
jamais par frame ; le seuil de 1 ms a été retiré. La variante
`iteration-profiling` mesure aussi ce coût avec optimisation et Tracy, dans
des jobs manuels exigés aux points de contrôle.
