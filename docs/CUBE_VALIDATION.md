<!-- Protocole reproductible du point de contrôle intermédiaire de phase 2. -->
# Cube glTF hors écran

`somnium_renderer_offscreen` exerce le loader glTF, l'AssetManager, l'adaptateur
privé `runtime::ModelRendering` et le renderer de production. Le renderer ne
connaît aucun type d'asset. Les sept primitives deviennent sept draws : six faces
du cube et un petit plan magenta placé derrière, dessiné en dernier. Les matrices
monde viennent de la hiérarchie glTF ; les indices alternent uint16/uint32.

```sh
python tools/gpu/cube_reference.py --verify
python tools/gpu/run.py --test cube --executable build/debug/bin/somnium_renderer_offscreen --backend vulkan --output .cache/cube-local
```

Même variantes `.exe`/direct3d12, metal et `xvfb-run -a` que [D5](GPU_VALIDATION.md).
Les couches de validation et l'état explicite « non validé » suivent D5. Les
artefacts CI `cube-<backend>` contiennent image, différence, référence, shaders,
provenance, résultat JSON et journal natif, y compris en cas d'échec.

La scène texte `engine/renderer/tests/reference_scene.json` déclare géométrie,
atlas, caméra, lumière, seuils et sondes. La fixture LFS
`engine/assets/tests/fixtures/cube.glb` contient l'atlas PNG ; aucun asset source
n'est copié dans le build. La référence unique LFS est
`engine/renderer/tests/reference/cube.png`. Sa construction analytique intersecte
les rayons des centres de pixels avec les plans du cube, indépendamment du code
C++ et du GPU. Les arêtes projetées obliques ne peuvent pas toutes coïncider avec
la grille ; les sondes sont entourées d'au moins quatre pixels de la même face.

Lumière +Z : Lambert vaut exactement 1 sur la face avant et 0 sur les deux autres
faces visibles. L'atlas porte une couleur plate par face ; la face éclairée
échantillonne la cellule inférieure droite, de sorte qu'un UV nul ou un flip U/V
change sa couleur. Décodage sRGB de la texture, facteur linéaire (0,4 ; 0,75 ; 1),
Lambert puis encodage sRGB donnent exactement (83 ; 176 ; 64) au centre après
quantification. Ces valeurs sont choisies loin des demi-entiers : le facteur rouge
0,5 donnait 92,37, trop proche de l'arrondi 92/93 avec la précision matérielle sRGB.
Le choix 0,4 donne 83,03 ; ni la formule analytique ni l'exigence exacte ne changent.
Le test utilise RGBA8 UNORM et encode dans le fragment shader. Pour la future
présentation de phase 2, SDR_LINEAR a été acquis/soumis avec succès sur les trois
backends : la cible sRGB encode, le shader reste linéaire. Une composition non
supportée provoquera une erreur explicite, sans double encodage ni repli caché.

Centres : correspondance exacte obligatoire. Image entière : 2/255 par canal,
au plus 0,1 % de pixels au-delà du seuil. Aucune référence par backend ni capture
GPU promue en référence. Pour modifier intentionnellement scène ou shaders :
`python tools/gpu/cube_reference.py --confirm-regeneration`, puis revue des PNG,
fixture et empreintes `provenance.json`. `--verify` recalcule tout en CTest.

Le test couvre aussi la rétention texture/matériau, les générations et doubles
destructions, le rejet d'une matrice singulière et la destruction du modèle
pendant l'enregistrement : handles invalidés immédiatement, ressources libérées
après soumission. `Renderer::record` réutilise une capacité réservée au chargement.
La caméra vérifie RH_ZO, aspect de l'extent, FOV vertical et near/far en tests CPU.

Une seconde frame applique l'échelle miroir X déclarée dans la scène. Le
[contrat glTF d'instanciation](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#instantiation)
impose d'inverser le winding pour un déterminant monde négatif : le renderer
sélectionne une variante CW, conserve BACK et ne convertit pas le repère des assets.
L'oracle calcule également le cube réfléchi et vérifie qu'il produit la même image
analytique (couleur avant uniforme, autres faces visibles à Lambert nul).
`mirrored.png` et sa comparaison indépendante appliquent les mêmes cinq centres
exacts et les mêmes seuils. Sept draws par frame, quatorze dans cette exécution.

Ce jalon s'arrête au cube hors écran. Le sandbox conserve provisoirement le
triangle de phase 1 ; son remplacement, input et la caméra orbitale attendent
la validation intermédiaire.

Les [captures et journaux](validation/phase-2/README.md) prouvent le cube sur
lavapipe, WARP et Metal : zéro écart, cinq centres exacts, validation native active.
