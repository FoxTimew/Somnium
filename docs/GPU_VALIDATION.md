<!--
Décrit le test hors écran de l'ADR-0004 et les contrôles de présentation à venir.
Distingue la lecture des en-têtes, le rendu hors écran et la présentation réelle.
-->
# Protocole de validation GPU — contrat accepté de phase 1

L'étape b implémente le protocole hors écran dans `somnium_gpu_offscreen` et
`tools/gpu/run.py`. Les résultats observés sont consignés séparément dans le
[plan CI](PHASE_1_CI.md) ; l'existence du test ne vaut pas validation d'un backend.

## Autorité et conventions

La version lue dans le cache CPM est SDL `release-3.4.18`, commit
`829a65d769d935c4852f8159e964312c0957260a`.
[En-tête](https://github.com/libsdl-org/SDL/blob/829a65d769d935c4852f8159e964312c0957260a/include/SDL3/SDL_gpu.h)
: Coordinate System, `SDL_GPUFrontFace`, `SDL_GPUCullMode`, transfert de texture,
création de device et acquisition/soumission des command buffers.

Les NDC vont de (-1,-1) en bas à gauche à (1,1) en haut à droite ; Z va de 0
(near) à 1. Viewport et UV commencent en haut à gauche avec +Y vers le bas.
SDL convertit les différences de backend : aucune inversion Vulkan manuelle.
Le monde Somnium reste main droite et sa projection `RH_ZO` ; cela ne change
pas les conventions des coordonnées projetées de SDL.

Choix accepté et confirmé par D5 sur les trois backends : `SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE` et
`SDL_GPU_CULLMODE_BACK`, profondeur LESS avec clear à 1. Ces valeurs sont lues
dans l'en-tête et leur signification confirmée par les tables des backends ;
leur application à notre géométrie passe le test ci-dessous, sans aucun écart
dans le [run de preuve](https://github.com/FoxTimew/Somnium/actions/runs/39592617094).
Un échec conduit à diagnostiquer la projection, les indices et le viewport,
pas à introduire une inversion locale propre à un backend.

## Test hors écran obligatoire

1. Initialiser les services SDL nécessaires, créer un device avec driver explicite
   et validation activée, sans créer ni réclamer de fenêtre. Journaliser version SDL,
   OS, device/driver, formats supportés et état de la couche de validation.
2. Charger les artefacts produits hors ligne et leur manifeste. Créer une cible
   `R8G8B8A8_UNORM` de 256×256 et une profondeur `D32_FLOAT`, un échantillon,
   viewport complet, blending désactivé. Refuser explicitement un format requis
   indisponible ; ce n'est pas une image de référence différente.
3. Dessiner une composition asymétrique déterministe : triangle CCW coloré ;
   triangle de winding opposé qui doit être culled ; texture 2×2 à quatre couleurs
   distinctes échantillonnée en nearest ; géométrie superposée à Z=0,25 et Z=0,75.
   Des zones repères en haut/bas et aux quatre coins distinguent UV et NDC.
   Un triangle supplémentaire à Z négatif doit être entièrement coupé : il
   distingue le near plane 0 d'un intervalle -1..1, indépendamment de l'orientation.
4. Terminer et soumettre la passe de rendu. Une soumission de copie ordonnée après elle
   `SDL_DownloadFromGPUTexture` copie vers un transfer buffer DOWNLOAD. Offset 0,
   `pixels_per_row=256`, `rows_per_layer=256` : pitch 1024 octets, aligné sur 256,
   et offset aligné sur 512 conformément à l'avertissement D3D12 de l'en-tête.
5. Soumettre avec `SDL_SubmitGPUCommandBufferAndAcquireFence`, attendre cette
   fence avant `SDL_MapGPUTransferBuffer(..., false)`, copier les octets CPU,
   unmapper et libérer la fence. Borner l'exécution par le timeout Python de 120 s ; un
   timeout/échec de device ne devient jamais un succès de comparaison.
6. Comparer les quatre canaux entiers aux octets décodés du PNG de référence.
   Un pixel est différent si au moins un canal a un écart > 2. Succès si la
   fraction de pixels différents est ≤ 0,001, avec mêmes dimensions et mêmes
   canaux. Aucune conversion sRGB, correction colorimétrique ni masque de bord
   implicite. Les couleurs des centres des régions témoins doivent également
   correspondre exactement, sans aucune tolérance par canal, pour que la tolérance globale ne masque pas un repère absent.
7. Libérer toutes les ressources, puis le device ; aucune erreur des couches de
   validation n'est admise, même si la comparaison d'image passe.

La référence commune et indépendante du backend est suivie par LFS sous
`engine/gpu/tests/reference/gpu_conventions.png` (la règle PNG existe déjà).
Les seuils, données géométriques/UV et positions des sondes sont dans
`engine/gpu/tests/reference_scene.json`, jamais en dur dans le code. Les arêtes
horizontales et verticales sont alignées sur des frontières
de pixels autant que possible. Une référence par backend exige une justification
écrite dans la PR ; la référence commune est la règle. Le cube glTF de phase 2 complète cette preuve ; il ne la remplace pas.

## Régénération et preuves

Après bootstrap et build debug, depuis la racine du dépôt :

```sh
python tools/gpu/run.py --executable build/debug/bin/somnium_gpu_offscreen --backend vulkan --output .cache/d5-local
python tools/gpu/run.py --accept-capture .cache/d5-local --confirm-reference-update
```

Sous Windows, ajouter `.exe` et choisir `direct3d12` ; sous macOS, choisir `metal`.
Linux sans écran : préfixer la première commande par `xvfb-run -a` et sélectionner
l'ICD lavapipe avec `VK_DRIVER_FILES`, comme dans le workflow. Aucun outil HLSL
n'est exécuté pendant ce test. Les trois artefacts/manifeste sont vérifiés par SHA-256
avant lancement ; FileReader fournit leurs octets au device.

La référence initiale décrit analytiquement la scène attendue (règle de rasterisation
top-left, centres de pixels, nearest et profondeur LESS) ; elle n'est pas présentée
comme une capture GPU. Le fichier `reference/provenance.json` identifie son origine.
Une régénération ultérieure exige une capture réelle dont les centres exacts,
le hash de scène et l'activation de validation sont vérifiés.

La deuxième commande exige les deux options et refuse toute variable `CI` active. Le changement
PNG est présenté dans la PR, avec backend/device, versions, hash des shaders et
journal propre ; il est revu avant d'être accepté. Un fichier de référence absent
fait échouer le test normal. Il n'est jamais généré au premier lancement en CI.

À chaque échec : sauvegarder `actual.png`, `expected.png`, `diff.png`, les valeurs
d'écart maximal/fraction différente, les artefacts de shaders et le journal complet.
Le workflow publie ce dossier avec `if: always()` et échoue sur rendu incorrect.
Les seuils ne sont pas augmentés automatiquement pour rendre le job vert.

Les uploads RGBA8 sont compacts sur Metal : le
[`METAL_UploadToTexture` épinglé](https://github.com/libsdl-org/SDL/blob/829a65d769d935c4852f8159e964312c0957260a/src/gpu/metal/SDL_gpu_metal.m#L1800)
calcule le pas depuis la largeur de destination, sans utiliser `pixels_per_row`.
Les autres backends gardent les lignes alignées sur 256 octets. Ce choix privé
de transfert ne change ni géométrie, ni UV, ni référence. Les clear hints D3D12
reprennent les valeurs de la scène pour éviter les avertissements de clear lent.

Si Metal échoue, `tools/gpu/diagnose_metal.py` exécute un petit programme SDK
indépendant de SDL/shadercross/Somnium avec puis sans instrumentation shader,
et refait D5 avec seulement la validation API. Ces captures sont des diagnostics :
elles ne remplacent pas D5, ne changent pas la référence et ne neutralisent pas
l'échec du job principal. Un rendu sans validation shader reste **non validé**.

Constat sur macOS 26 / Xcode 26.6, `Apple Paravirtual device` :
[`native-full.log` et les captures](https://github.com/FoxTimew/Somnium/actions/runs/39591476758/artifacts/12341888108)
montrent `Texture usage flags mismatch` malgré `MTLTextureUsageShaderRead` déclaré
et relu égal à 1. Le programme SDK retourne quatre texels nuls avec instrumentation,
les quatre couleurs exactes sans elle. D5 suit la même distinction : 6 400 pixels
différents (zone texturée entière) avec instrumentation, zéro avec validation API
seule après correction du pas d'upload. Cette reproduction isole le défaut hors
du moteur ; elle ne fournit pas une exemption automatique au contrat de validation.

Le même binaire arm64 et les mêmes shaders passent ensuite D5 sous macOS 15.7.9,
Xcode 16.4 (16F6), image `macos-15-arm64/20260907.0337`, avec **API et GPU shader
validation actives**, zéro pixel différent et les dix centres exacts :
[capture et journal Metal](https://github.com/FoxTimew/Somnium/actions/runs/39592617094/artifacts/12342870817).
Seul le job de rendu reste fixé à `macos-15`. À la clôture de phase 1, builds et
chaîne d'outils reviennent sur `macos-latest` ; le job Metal consomme leur binaire
arm64 ciblant macOS 13.3 et leurs shaders, sans recompilation. Aucune validation
désactivée ni référence adaptée au backend.

Sur Windows, les avertissements de clear optimisé ont été supprimés. L'absence
de `WinPixEventRuntime.dll` reste signalée : les marqueurs SDL D3D12 ne sont pas
disponibles dans ces captures. La couche debug et la comparaison D5 sont actives ;
les groupes de debug SDL suffisent au contrat de phase et aucun marqueur PIX spécifique n’est requis.

## Présentation, UI et durée de vie

Les backends ImGui officiels de la version CPM sont lus avant implémentation.
Séquence pilotée par `debug_ui` : créer le contexte, appeler la façade platform
(`ImGui_ImplSDL3_InitForSDLGPU`), puis la façade GPU (`ImGui_ImplSDLGPU3_Init` avec
device, format de swapchain, SDR/VSYNC et sample count). Par frame : la plateforme
fournit les événements à son adaptateur `ImGui_ImplSDL3_ProcessEvent`, les deux
façades exécutent leur `NewFrame`, puis `ImGui::NewFrame`, construction de l'UI
et `ImGui::Render`. Une fois le command buffer et la cible acquis, la façade GPU
appelle `ImGui_ImplSDLGPU3_PrepareDrawData` **avant** toute passe de rendu, puis
`ImGui_ImplSDLGPU3_RenderDrawData` dans la passe prévue. Arrêt : attendre les
travaux nécessaires, arrêter GPU ImGui, platform ImGui, puis détruire le contexte,
avant les objets GPU/fenêtre dont ils dépendent.

Ces fonctions natives restent dans `somnium_platform_imgui` et
`somnium_gpu_imgui`. Ces cibles accèdent aux en-têtes privés de coopération
platform/gpu pour l’UI ; aucune dépendance UI ne remonte dans les modules de base. Leurs façades accessibles à `debug_ui` n'exposent aucun type SDL et
portent les namespaces d'includes d'intégration autorisés par l'architecture.
Le `SDL_Event` ne passe jamais par debug_ui ni gpu. Option UI OFF : aucune cible
d'intégration et aucun backend compilé ; sources platform/gpu sans include ImGui
ni condition liée à cette option.

Le test hors écran est complété par une exécution sandbox : triangle et overlay,
redimensionnements, passage minimisé/restauré, fermeture pendant une frame,
release shipping sans cibles ImGui. L'acquisition bloquante borne à deux frames
en vol ; une texture NULL ne déclenche aucun rendu. La simulation réinitialise
son horloge au retour de suspension ; l'attente en minimisation porte sur les
événements, pas sur une fréquence artificielle. L’adaptateur GPU recale le
FramebufferScale des draw data ImGui sur la taille de la texture effectivement
acquise : un resize arrivé entre acquisition et préparation UI ne produit ainsi
jamais un viewport plus grand que la cible courante.

Les tests de handles couvrent destruction, génération, double libération,
invalidation et emprunts limités à la frame. `renderOffscreen` compose désormais
`beginFrame`/`recordPass`/`submitFrame` sans swapchain. `PipelineDescription` déclare
formats couleur/profondeur, vertex layout, culling et profondeur ; D5 et le
triangle l’utilisent avec des descriptions différentes. Les handles sont propres à leur device
et toutes ses opérations restent sur le thread créateur. Une destruction demandée pendant
l'enregistrement invalide le handle pour les nouvelles résolutions ; les objets
déjà empruntés survivent jusqu'à soumission/annulation. SDL assure ensuite la
libération native différée. Le test de relecture prouve séparément l'attente de
fence ; les transfers d'upload utilisent le cycling pour éviter l'écrasement.

Sans device ou couche en CI, publier un résultat **non validé** identifié par
backend, sans enregistrer un test CTest réussi ou skipped. La clôture de phase
exige alors captures et journal de validation locaux joints
à la PR pour le backend concerné. Le [plan CI](PHASE_1_CI.md) distingue ces cas.

Le test de ressources CPU réutilise un slot invalidé pendant une frame et vérifie
que son ancien objet natif n'est libéré qu'après la fin de l'enregistrement.
D5 détruit cinq handles pendant une passe enregistrée, vérifie immédiatement
leur invalidité et le compteur de libérations en attente, soumet puis vérifie
la vidange ; une autre frame couvre l'annulation. Sa scène, sa référence et
`provenance.json` sont inchangés par ces tests de durée de vie.

La dépendance de validation au runner macos-15, l’horizon de retrait et la
migration exigée sont consignés dans le [plan CI](PHASE_1_CI.md).
