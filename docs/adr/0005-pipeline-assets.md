<!-- Proposition de phase 2 ; aucun code assets/renderer avant validation. -->
# ADR-0005 — Assets sources, préparation GPU et rendu de modèle

Date : 21 octobre 2026. Statut : **proposé — en attente de validation**.

## Contexte

Remplacer le triangle par un modèle [glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
et une caméra orbitale, sans scène/ECS ni cuiseur, dans les frontières des ADR-0001 à 0004.

## Décision

- **Sources.** Lecture directe au MVP. Racines ordonnées dans `platform` :
  `<build>/bin/assets` (shaders compilés), puis `<dépôt>/assets` en développement ;
  aucune copie des sources dans le build, aucune dépendance au répertoire courant.
  Shipping : seule racine cuite `<exécutable>/assets`, sans repli dépôt ;
  sa production et la distribution cuite relèvent du cuiseur de phase 4.
  Le preset choisit explicitement le mode de données, indépendamment de l'UI.
  `core::ByteReader` injecté par `runtime` ; chemins logiques relatifs normalisés,
  sans sortie des racines ; seul « absent » autorise le repli.
- **AssetManager.** Données CPU immuables, handles typés index/génération ; identité
  `(type, chemin logique)`, sous-ressources par modèle/index stable. Demande puis
  consultation d'état/données/erreur : Unloaded → Loading → Ready/Failed ; synchrone
  aujourd'hui, sans présumer Ready au retour. Demande irrecevable : `Result` ;
  chargement échoué : Failed et diagnostic. Loader par type, dépendances prêtes avant
  Ready. Déchargement explicite à frontière sûre, génération invalidée ; aucun job/éviction.
- **glTF.** cgltf épinglé : `.gltf`/`.glb`, buffers/images externes ou embarqués ;
  triangles indexés uint16/uint32, POSITION/NORMAL/TEXCOORD_0, baseColor facteur ×
  texture opaque. Hiérarchie aplatie en instances mesh + matrices monde, sans
  conversion de repère. Skins, animations, caméras, lumières et extensions exclus ;
  données requises absentes ou usage hors périmètre : Failed explicite.
- **Textures.** stb_image : RGBA8 sans flip vertical ; baseColor sRGB, facteur/alpha
  linéaires, éclairage directionnel Lambert en linéaire, encodage SDR unique.
  Aucun mipmap ; sampler min/mag nearest/linear, U/V repeat/clamp, défaut linear/repeat.
  Filtres mip ramenés au filtre spatial LOD 0, mirrored-repeat rejeté ; texture
  absente blanche 1×1. KTX2/BC reportés au cuiseur.
- **Rendu.** `runtime` adapte les données CPU de `assets` aux vues neutres de
  `renderer`, propriétaire des ressources GPU, sans lien renderer → assets.
  Upload avant rendu. DrawList produite par `runtime` depuis modèle/caméra : matrices
  et handles renderer, sans contenu de rendu codé en dur ; triangle supprimé,
  `scene` seulement en phase 3.
- **GPU.** Vertex position/normale/UV0 float3/float3/float2 : 32 octets, offsets 0/12/24.
  Ajouter buffers d'indices uint16/uint32, upload/binding, `Draw` indexé (plage/baseVertex),
  bindings texture/sampler multiples, samplers décrits et RGBA8 sRGB échantillonné.
  Push SDL des uniformes : frame VS b0 vue/projection, PS b0 direction lumineuse ;
  draw VS b1 monde/normale inverse-transposée, PS b1 facteur baseColor. Espaces ADR-0003,
  std140 contrôlé, column-major sans transposition ; durée de vie ADR-0004.
- **Caméra/input.** `perspectiveRH_ZO`, aspect de l'extent de swapchain ; FOV vertical
  π/3 rad, near 0,1 m/far 100 m initiaux, paramètres explicites ; extent nul sans rendu. Actions/axes d'orbite,
  zoom/déplacement et mapping en code en phase 2 ; mapping JSON configurable en phase 3.
- **Test cube.** D5 étendu : glTF texturé, caméra connue, faces attendues, UV/profondeur.
  Référence PNG LFS commune, sondes/paramètres/provenance versionnés ; centres exacts,
  seuils 2/255 et 0,1 %, régénération explicite. CI Vulkan/lavapipe, D3D12 et Metal
  avec validation, images/journaux ; indisponibilité « non validé » selon D5.
  Fixtures glTF CPU : indices 16/32 bits, hiérarchie, chemins/dépendances, erreurs et générations.

## Alternatives

Cuiseur immédiat : coût prématuré. GPU dans assets ou renderer couplé aux assets :
frontières rompues. Retour direct de ressource : incompatible avec l'évolution asynchrone.

## Conséquences

Phase 4 remplace loaders/conditionnement sans changer identité/états. Ordre : GPU →
assets/tests CPU → renderer/cube hors écran **avec point de contrôle** →
input/caméra/sandbox → documentation et bilan.

Preuves : racines ordonnées et isolation shipping ; fixtures CPU/handles ; uniformes et bindings multiples ; cube D5 sur trois backends ; caméra orbitale.
