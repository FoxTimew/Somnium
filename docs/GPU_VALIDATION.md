<!--
Décrit les tests à implémenter après validation de l'ADR-0004.
Distingue la lecture des en-têtes, le rendu hors écran et la présentation réelle.
-->
# Protocole de validation GPU — proposition de phase 1

Ce document est un protocole, pas un compte rendu de rendu réussi. Aucun test
GPU n'est implémenté au point de contrôle documentaire. Les noms de commandes
ci-dessous sont des interfaces proposées à valider avec l'ADR-0004.

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

Choix proposé : `SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE` et
`SDL_GPU_CULLMODE_BACK`, profondeur LESS avec clear à 1. Ces valeurs sont lues
dans l'en-tête et leur signification confirmée par les tables des backends ;
leur application à notre géométrie doit encore passer le test ci-dessous.
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
   Les sommets restent dans le clip volume pour isoler le test d'orientation.
4. Terminer la passe de rendu. Dans une passe de copie du même command buffer,
   `SDL_DownloadFromGPUTexture` copie vers un transfer buffer DOWNLOAD. Offset 0,
   `pixels_per_row=256`, `rows_per_layer=256` : pitch 1024 octets, aligné sur 256,
   et offset aligné sur 512 conformément à l'avertissement D3D12 de l'en-tête.
5. Soumettre avec `SDL_SubmitGPUCommandBufferAndAcquireFence`, attendre cette
   fence avant `SDL_MapGPUTransferBuffer(..., false)`, copier les octets CPU,
   unmapper et libérer la fence. Borner l'exécution par le timeout CTest ; un
   timeout/échec de device ne devient jamais un succès de comparaison.
6. Comparer les quatre canaux entiers aux octets décodés du PNG de référence.
   Un pixel est différent si au moins un canal a un écart > 2. Succès si la
   fraction de pixels différents est ≤ 0,001, avec mêmes dimensions et mêmes
   canaux. Aucune conversion sRGB, correction colorimétrique ni masque de bord
   implicite. Les couleurs des centres des régions témoins doivent également
   correspondre, pour que la tolérance globale ne masque pas un repère absent.
7. Libérer toutes les ressources, puis le device ; aucune erreur des couches de
   validation n'est admise, même si la comparaison d'image passe.

La référence commune et indépendante du backend sera suivie par LFS sous
`assets/tests/reference/gpu_conventions.png` (la règle PNG existe déjà).
Les données géométriques/UV et positions des sondes restent du texte versionné
à côté du test. Le cube glTF de phase 2 complète cette preuve ; il ne la remplace pas.

## Régénération et preuves

Interface proposée après validation :

```sh
gpu_reference_test --backend vulkan --compare assets/tests/reference/gpu_conventions.png
gpu_reference_test --backend vulkan --write-reference assets/tests/reference/gpu_conventions.png --confirm-reference-update
```

La deuxième commande exige les deux options et refuse `CI=true`. Le changement
PNG est présenté dans la PR, avec backend/device, versions, hash des shaders et
journal propre ; il est revu avant d'être accepté. Un fichier de référence absent
fait échouer le test normal. Il n'est jamais généré au premier lancement en CI.

À chaque échec : sauvegarder `actual.png`, `expected.png`, `diff.png`, les valeurs
d'écart maximal/fraction différente, les artefacts de shaders et le journal complet.
Le workflow publie ce dossier avec `if: always()` et échoue sur rendu incorrect.
Les seuils ne sont pas augmentés automatiquement pour rendre le job vert.

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
`somnium_gpu_imgui`. Seules ces cibles accèdent aux en-têtes privés de coopération
platform/gpu. Leurs façades accessibles à `debug_ui` n'exposent aucun type SDL et
portent les namespaces d'includes d'intégration autorisés par l'architecture.
Le `SDL_Event` ne passe jamais par debug_ui ni gpu. Option UI OFF : aucune cible
d'intégration et aucun backend compilé ; sources platform/gpu sans include ImGui
ni condition liée à cette option.

Le test hors écran est complété par une exécution sandbox : triangle et overlay,
redimensionnements, passage minimisé/restauré, fermeture pendant une frame,
release shipping sans cibles ImGui. L'acquisition bloquante borne à deux frames
en vol ; une texture NULL ne déclenche aucun rendu. La simulation réinitialise
son horloge au retour de suspension ; l'attente en minimisation porte sur les
événements, pas sur une fréquence artificielle.

Les tests de handles couvrent destruction, génération, double libération,
invalidation et emprunts limités à la frame. Une destruction demandée pendant
l'enregistrement invalide le handle pour les nouvelles résolutions ; les objets
déjà empruntés survivent jusqu'à soumission/annulation. SDL assure ensuite la
libération native différée. Le test de relecture prouve séparément l'attente de
fence ; les transfers d'upload utilisent le cycling pour éviter l'écrasement.

Sans device ou couche en CI, publier un résultat **non validé** identifié par
backend, sans enregistrer un test CTest réussi ou skipped. La clôture de phase
exige alors capture RenderDoc/PIX/Xcode et journal de validation locaux joints
à la PR pour le backend concerné. Le [plan CI](PHASE_1_CI.md) distingue ces cas.
