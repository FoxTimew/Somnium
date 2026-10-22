<!-- Provenance des assets source livrés avec le sandbox. -->
# Licences des assets

`models/orbit-cube.glb` : création originale du projet Somnium, 2026, dédiée
au domaine public sous [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
Géométrie, atlas de six couleurs et matériaux produits analytiquement par
`tools/gpu/cube_reference.py`, sans image ni modèle tiers. Il s'agit du cube de
fixture approuvé, avec son petit témoin de profondeur entièrement intérieur.
Le GLB contient les buffers et le PNG ; aucun téléchargement n'est requis.

Le modèle source est versionné sous LFS et lu directement depuis cette racine
en développement. Il n'est pas copié dans le build par CMake. Le conditionnement
automatique et les formats cuits relèvent de la phase 4.
