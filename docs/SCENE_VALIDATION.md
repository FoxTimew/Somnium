<!-- Décrit l'oracle indépendant et la reproduction des trois poses de scène.
Les images GPU sont des résultats, jamais des sources de la référence. -->
# Preuve de scène — phase 3

`somnium_scene_offscreen` charge `engine/runtime/tests/hierarchy.json` par les
codecs réels et demande `fixtures/hierarchy.glb` au contexte d’assets. Le fichier déclare le
mesh enfant avant son pivot : sa référence parent doit passer par la seconde
passe. Cette fixture dédiée conserve les sept primitives, matériaux et instances glTF
du générateur analytique ; sa géométrie et sa palette sont dans `fixture_geometry.json`.

Le pivot tourne de 0 à 90° autour de Y. Son enfant est décentré, tourné de 90°
autour de Z, avec une échelle non uniforme. La caméra racine regarde vers -Z.
Runtime extrait les poses alpha 0, 0,5 et 1 ; renderer les dessine dans une
cible RGBA8 UNORM/D32 sans fenêtre, puis gpu relit les pixels après soumission.
La pose médiane suit l’arc du pivot et conserve la rotation propre de l’enfant.

L’oracle Python intersecte les rayons avec les plans du cube en double précision,
avec une rotation fermée `Ry(alpha × 90°) × Rz(90°)` indépendante des matrices
du moteur. Il applique culling arrière, profondeur, couleurs de texture sRGB,
facteur linéaire, Lambert et encodage sRGB. La petite primitive dessinée en dernier
est occultée dans les deux premières poses ; son bord peut devenir visible dans
la dernière, ce que l’oracle calcule aussi. Aucun rendu GPU n’alimente les références.

La palette de cette fixture utilise seulement les extrêmes sRGB **0/255** et
un facteur matériau unitaire, pour éviter que les approximations de décodage
sRGB des devices déplacent les sondes d’un niveau au voisinage des seuils
UNORM. Son ambiant est nul ; les facteurs Lambert sont 0, 1 ou √(1/2).
Les couleurs et les sondes restent dérivées de la formule, jamais d’une capture.
Le sandbox reste à 0,15 ; la preuve colorimétrique du cube de phase 2 conserve
ses couleurs intermédiaires, sa référence et sa provenance inchangées.

`reference_scene.json` versionne angles, trois alphas, seuils et positions/couleurs
des sondes. Les centres sont exacts ; leur voisinage analytique de 7×7 pixels
appartient à la même face. L’image entière conserve **2/255 et 0,1 %**. Les trois
PNG de `engine/runtime/tests/reference/` sont communs à tous les backends et sous
LFS. `provenance.json` lie les entrées, shaders, fixture et images par SHA-256.
Les trois poses Radeon correspondent sans pixel hors tolérance.

```sh
python tools/gpu/scene_reference.py --verify
python tools/gpu/scene_reference.py --confirm-regeneration
python tools/gpu/run.py --test scene --executable build/debug/bin/somnium_scene_offscreen --backend vulkan --output .cache/scene-vulkan
```

Sur Windows ajouter `.exe` ; sélectionner `direct3d12` ou `vulkan`. Linux utilise
Xvfb/lavapipe avec les couches Vulkan épinglées. Les commandes usuelles
`tools/ci/{windows.ps1,linux.sh,macos.sh} gpu` exécutent D5, cube, scène puis le
probe de composition ; une comparaison ou une erreur native fait échouer le job.
Une indisponibilité Metal reste explicitement « non validé » dans le workflow
manuel macOS, jamais une preuve réussie. macos-latest construit ; macos-15 rend.

Captures `previous.png`, `actual.png`, `current.png`, différences, JSON de résultat
et journal de validation restent sous le répertoire local du run. Le runner macOS
éphémère publie également ces trois petits PNG en base64 avec SHA-256 dans son
journal (`SOMNIUM_SCENE_IMAGE`) ; leur copie dans la PR est vérifiée par ce hash,
sans action de transfert d’artefacts. Le résultat de CI et la relance macOS sont
consignés dans le bilan du point de contrôle.
