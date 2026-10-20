<!--
Reserves the source-asset root for the game validation scene.
Phase 0 has no rendered assets; binary sources will use Git LFS.
-->
# Assets sources

Les scènes, HLSL, modèles glTF et images du jeu seront ajoutés dans les phases
qui les consomment. Aucun asset de démonstration ni shader n'est requis en phase 0.

Les identifiants de ressources sont relatifs à une racine injectée ; le
répertoire de travail du processus n'est pas une racine implicite. Les sorties
de compilation iront sous `build/`, jamais dans ce répertoire.
