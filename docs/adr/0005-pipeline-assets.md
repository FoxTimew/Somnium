<!-- Décision de phase 2 validée ; preuve intermédiaire avant input/sandbox. -->
# ADR-0005 — Assets sources, préparation GPU et rendu de modèle

Date : 22 octobre 2026. Statut : **accepté**.

## Contexte

Remplacer le triangle par un modèle [glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
et une caméra orbitale, sans scène/ECS ni cuiseur, dans les frontières des ADR-0001 à 0004.

## Décision

- **Sources.** Lecture directe au MVP. Racines `platform` ordonnées :
  `<build>/bin/assets` (shaders), puis `<dépôt>/assets` en développement ;
  aucune copie des sources ni dépendance au répertoire courant. Shipping :
  seule racine cuite `<exécutable>/assets`, produite par le cuiseur de phase 4.
  Mode choisi par preset, indépendamment de l'UI. `core::ByteReader` injecté par
  `runtime` ; chemins logiques normalisés, bornés aux racines ; repli uniquement si absent.
- **AssetManager.** Données CPU immuables, handles typés index/génération ; identité
  `(type, chemin logique)` ; primitive identifiée par `(index mesh, index primitive)`,
  stable pour un fichier donné : **une primitive = un draw = un matériau**. Demande puis
  consultation : Unloaded → Loading → Ready/Failed ; synchrone aujourd'hui,
  sans présumer Ready au retour. Demande irrecevable : `Result` ; échec : Failed
  et diagnostic. Loader par type, dépendances prêtes avant Ready. Déchargement explicite
  à frontière sûre, génération invalidée ; aucun job/éviction.
- **glTF.** cgltf épinglé : `.gltf`/`.glb`, buffers/images externes ou embarqués ;
  triangles indexés uint16/uint32, POSITION/NORMAL/TEXCOORD_0, baseColor facteur ×
  texture opaque. Hiérarchie aplatie en instances mesh + matrices monde, sans
  conversion de repère. Failed pour donnée requise absente/invalide (POSITION),
  non-triangles, extension requise non supportée ou accesseur sparse. NORMAL absent :
  normale géométrique ; UV0 absent : zéro. Skins/animations/caméras/lumières exclus.
  Options non prises en charge (MASK/BLEND, doubleSided, COLOR_n, UV1+, autres textures)
  ignorées, un avertissement Log par modèle et repli opaque/culling arrière.
- **Textures.** stb_image : RGBA8 sans flip vertical ; baseColor sRGB, facteur/alpha
  linéaires, éclairage Lambert en linéaire. Sortie : `SDR_LINEAR` si les trois probes
  CI le supportent, sinon encodage shader ; jamais les deux. SDL_gpu.h 3.4.18 impose
  d'interroger la fenêtre. Hors écran : RGBA8 UNORM, encodage shader et référence
  identiques : `12.92*x` si `x <= 0.0031308`, sinon `1.055*x^(1/2.4)-0.055`.
  Aucun mipmap ; sampler min/mag nearest/linear, U/V repeat/clamp, défaut linear/repeat.
  Mip réduit à LOD 0, mirrored-repeat replié sur repeat avec avertissement ; texture
  absente blanche 1×1. KTX2/BC reportés au cuiseur.
- **Rendu.** `runtime` adapte les données CPU de `assets` aux vues neutres de
  `renderer`, propriétaire des ressources GPU, sans lien renderer → assets.
  Upload avant rendu. DrawList produite par `runtime` depuis modèle/caméra : matrices
  et handles renderer ; aucun contenu de rendu dans runtime, triangle supprimé,
  `scene` seulement en phase 3.
- **GPU.** Vertex position/normale/UV0 float3/float3/float2 : 32 octets, offsets 0/12/24.
  Buffers d'indices uint16/uint32, upload/binding, `Draw` indexé (plage/baseVertex),
  bindings texture/sampler multiples, samplers décrits et RGBA8 sRGB échantillonné.
  Push SDL des uniformes : frame VS b0 vue/projection, PS b0 direction lumineuse ;
  draw VS b1 monde/normale inverse-transposée, PS b1 facteur baseColor. Espaces ADR-0003,
  std140 contrôlé, column-major sans transposition ; durée de vie ADR-0004.
- **Caméra/input.** `perspectiveRH_ZO`, aspect de l'extent de swapchain ; FOV vertical
  π/3 rad, near 0,1 m/far 100 m initiaux, explicites ; extent nul sans rendu.
  Input actions/axes d'orbite, zoom/déplacement ; mapping en code, JSON en phase 3.
- **Test cube.** D5 étendu : glTF texturé à couleurs plates par face, lumière alignée
  sur un axe du cube (Lambert fermé), caméra connue, faces attendues, UV/profondeur.
  Référence et sondes analytiques, éloignées des arêtes ; fixtures `.glb`/`.bin`
  sous `engine/<module>/tests/` en LFS.
  PNG LFS commun, sondes/paramètres/provenance versionnés ; centres exacts,
  seuils 2/255 et 0,1 %, régénération explicite. CI Vulkan/lavapipe, D3D12 et Metal
  avec validation, images/journaux ; indisponibilité « non validé » selon D5.
  Tests CPU : indices 16/32, hiérarchie, chemins/dépendances, erreurs et générations.

## Alternatives

Cuiseur immédiat : coût prématuré. GPU dans assets ou renderer couplé aux assets :
frontières rompues. Retour direct de ressource : incompatible avec l'évolution asynchrone.

## Conséquences

Phase 4 remplace loaders/conditionnement sans changer identité/états. Ordre : GPU →
assets/tests CPU → renderer/cube hors écran **avec point de contrôle** →
input/caméra/sandbox → documentation et bilan.

Preuves : racines ordonnées et isolation shipping ; fixtures CPU/handles ; uniformes et bindings multiples ; cube D5 sur trois backends ; caméra orbitale.
