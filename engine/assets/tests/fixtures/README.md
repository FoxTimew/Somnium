Les fixtures sont générées explicitement par `python tools/assets/generate_fixtures.py --confirm-regeneration`.
`model.gltf` référence `mesh.bin` et `texture.png` ; `model.glb` embarque les mêmes données,
et `embedded.gltf` utilise des URI base64. Les fichiers binaires sont sous Git LFS.

Le mesh 0 contient deux primitives partageant un matériau (indices uint16 puis uint32).
Le mesh 1 contient une primitive sans matériau, avec le repli blanc. Les positions,
normales et UV sont entrelacés sur 32 octets. Le PNG 2×2 est rouge/vert puis bleu/blanc.
La hiérarchie applique une rotation de 90° autour de Z, une translation et une échelle :
la translation monde attendue du mesh 0 est `(1, 6, 3)`, sans conversion de repère.
