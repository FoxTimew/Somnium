<!-- Décision acceptée de phase 2 ; détails dans INPUT et CUBE_VALIDATION. -->
# ADR-0005 — Assets sources et rendu de modèle

Date : 22 octobre 2026. Statut : **accepté**.

## Contexte

Afficher un glTF texturé avec caméra orbitale, sans scène/ECS ni cuiseur. Préserver
les frontières CPU/GPU et les preuves D5 communes aux trois backends.

## Décision

- **Sources et identité.** Lecture directe au MVP : platform cherche
  `<exécutable>/assets` (shaders compilés), puis `<dépôt>/assets` en développement.
  Shipping : seule première racine, conditionnée explicitement avant le cuiseur
  de phase 4. Aucune copie automatique des assets sources dans le build ; aucun
  recours au répertoire courant. `core::ByteReader` injecté par runtime ; chemin
  logique canonique borné aux racines, repli uniquement sur NotFound.
- **AssetManager.** Handles typés index/génération ; identité `(type, chemin)` ;
  états Unloaded/Loading/Ready/Failed. Demande puis consultation, synchrone
  aujourd’hui derrière un contrat compatible asynchrone. Loader par type,
  données CPU immuables ; échec conservé, déchargement explicite invalidant le
  handle, aucun job/éviction. Primitive identifiée par `(mesh, primitive)`, stable
  pour un fichier : **une primitive = un draw = un matériau**.
- **glTF 2.0.** cgltf épinglé ; glTF/GLB, buffers/images externes ou embarqués,
  triangles indexés uint16/32, POSITION/NORMAL/UV0, baseColor facteur × texture.
  Hiérarchie aplatie en matrices monde sans conversion de repère. Failed pour
  donnée requise absente/invalide, topologie hors périmètre, extension requise
  non supportée ou sparse. NORMAL absent : normales géométriques ; UV0 absent :
  zéro. Skins/animations/caméras/lumières exclus. Options non supportées
  (MASK/BLEND, doubleSided, COLOR_n, UV1+, autres textures) ignorées avec un seul
  avertissement par modèle ; repli opaque/BACK.
- **Textures et lumière.** stb_image RGBA8 sans flip ni mipmaps ; min/mag
  nearest/linear, U/V repeat/clamp, défaut linear/repeat ; mirrored-repeat devient
  repeat avec avertissement, texture absente blanche. BaseColor échantillonné
  en sRGB, facteur × (Lambert + ambiant) en linéaire. Présentation **SDR_LINEAR**,
  support vérifié sur les trois backends ; absence : erreur explicite. Hors écran
  UNORM : encodage shader IEC sRGB, même formule dans l’oracle. Ambiant 0 dans le
  cube, shader/référence/provenance inchangés ; variante ambiante à 0,15 dans le
  sandbox. KTX2/BC et mipmaps au cuiseur.
- **Rendu.** Vertex position/normale/UV0 : 32 octets, offsets 0/12/24. GPU fournit
  index buffers, draws indexés, bindings multiples, samplers décrits, textures
  sRGB et push d’uniformes. VS b0 vue/projection, b1 monde/inverse-transposée ;
  PS b0 lumière/encodage, b1 facteur, b2 ambiant dans sa variante. Espaces ADR-0003,
  column-major/std140. BACK/LESS/clear 1 ; CCW, ou CW pour déterminant monde négatif.
  Renderer possède les ressources GPU ; runtime adapte les données CPU et produit
  la DrawList, sans contenu de rendu propre. Triangle supprimé ; scene en phase 3.
- **Gameplay/input.** Platform traduit SDL en événements neutres bruts horodatés
  par pas dans core ; input, sans SDL, mappe actions/axes en code ([détails](../INPUT.md)).
  Gameplay reçoit l’état dans fixedUpdate, demande le modèle via GameContext et
  fournit `renderer::Camera` par valeur. Orbite dans game, pitch/zoom bornés ;
  `perspectiveRH_ZO`, aspect de l’extent, FOV vertical π/3, near 0,1/far 100.
  Mapping JSON en phase 3 ; modèle original CC0 sous assets/models en LFS.
- **Preuve cube.** [Protocole](../CUBE_VALIDATION.md) : glTF à couleurs plates,
  Lambert fermé, sondes hors arêtes, référence analytique unique, miroir X et
  témoin de profondeur. Fixtures GLB/BIN et PNG sous `engine/<module>/tests/`,
  LFS ; centres exacts, seuils 2/255 et 0,1 %. Régénération explicite ; tests CPU
  des loaders/handles, rendu avec validation Vulkan, D3D12 et Metal selon D5.

## Alternatives

Cuiseur immédiat : coût prématuré. GPU dans assets ou lien renderer → assets :
frontières rompues. Caméra dans runtime : couplage au gameplay.

## Conséquences

Phase 4 change loaders/conditionnement sans changer identité/états. Ordre réalisé :
GPU → assets/tests CPU → renderer/cube (jalon validé) → input/caméra/sandbox.

Preuves : CI Windows/Linux, D5/cube exacts Radeon/lavapipe et captures D3D12/Vulkan validés sur 36f1824 (run 45622048422, bilan de phase 2) ; Metal reste une preuve historique du jalon, best effort manuel depuis ADR-0006 ; WARP non couvert.
