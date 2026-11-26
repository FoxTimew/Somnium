<!-- Décisions d’input générique ; détails et exemples dans INPUT.md. -->
# ADR-0008 — Inputs configurables

Date : 25 novembre 2026. Statut : **accepté**.

## Contexte

Les noms d’actions appartiennent au jeu. Configuration et capture UI doivent préserver
les transitions pendant les frames sans pas et les rattrapages. Les preuves de
présentation ne doivent pas injecter d’entrées dans la session de travail.

## Décision

- **Configuration** JSON v1 `version/actions/axes` sous `assets/input/`, noms uniques,
  plusieurs liaisons par destination. Compilation transactionnelle via le lecteur
  fourni par runtime ; `ActionId/AxisId` typés résolus une fois dans start, aucun nom
  recherché dans la boucle. Configuration immutable, IDs stables pour sa durée de vie ;
  aucune enum du sandbox dans input.
- **Sources** : touches physiques, boutons souris/manette pour les actions (OU).
  Axes `rate` : touches ±, sticks/gâchettes, persistants et sommés dans [-1,1]. Axes
  `delta` : souris/molette, cumulés une fois sans écrêtage ±1 ni facteur dt. Gain fini,
  inversion, zone morte axiale 0 ≤ d < 1 sur manette ; normalisation asymétrique des
  sticks, gâchettes [0,1], remise à l’échelle hors zone morte. Quatre slots de manette,
  filtre `any` ou 0..3 ; excédent signalé. Seuls les deltas peuvent être conditionnés
  par une action maintenue, évaluée dans l’ordre des événements (souris pendant Orbit).
- **Clavier** : `core::Key` 16 bits couvre les touches nommées non réservées du
  SDL_scancode.h 3.4.18 épinglé, clavier étendu compris. Noms Somnium stables,
  traduction explicite par platform ; Unknown/réservées non configurables. Texte/IME
  séparé. Input dépend uniquement de core : aucun SDL, seulement InputEvent neutre.
- **Erreurs/conflits** : version/champ/source/nom inconnus, mauvais type, valeurs
  invalides, doublons ou condition absente donnent un diagnostic contextualisé.
  Une source physique a une seule destination ; chevauchements `any`/slot détectés.
  Lire une action comme condition n’est pas une liaison supplémentaire. Aucun arbitrage
  implicite ; contextes permettant le partage d’une source explicitement reportés.
- **Temps** : platform estampille le prochain pas. File ordonnée ; advance consécutif
  depuis zéro, futurs conservés, transitions consommées une fois. Journal logique
  ordonné et résumés pressed/released/held : double tap conservé. Impulsions effacées
  au pas suivant, held/rates conservés ; abandon de temps sans saut d’indice. Répétition
  clavier ignorée, focus perdu/déconnexion libèrent une fois les sources concernées.
- **UI** : panneau noms/IDs, pas, transitions et axes en lecture seule. Runtime prend
  la demande neutre de capture après construction de l’UI CPU, avant d’enfiler les
  nouvelles entrées ; second poll différé à la décision suivante. Entrée en capture :
  libération unique ; appuis/deltas capturés exclus, relâchements traités ; nouvelle
  pression exigée au retour. Aucun ImGui dans input ni les sources de base platform/gpu.
- **Rejeu** : JSON v1 d’InputEvent horodatés par pas, chargé par option du sandbox
  et injecté à la frontière de platform avant chaque pas. Gameplay natif écarté,
  cycle de fenêtre conservé, inspecteur non interactif. Capture après N pas, alpha=1,
  frame scène + UI dans une cible au format/extent acquis, copie vers swapchain,
  soumission puis relecture GPU et fermeture. Scripts communs sans lecture du bureau,
  injection système ni changement de focus. Captures Windows interactif/Linux Xvfb
  requises car indépendantes des recouvrements ; Metal reste manuel best effort.

## Alternatives

Enum moteur ou noms par pas : couplage/coût évitables. Mélanger rate/delta : cadence
incorrecte. Booléens seuls : transitions perdues. Injection OS : preuve dépendante du focus.

## Conséquences

Déplacement/orbite/Interact résident dans game. Capacités réservées, croissance
exceptionnelle journalisée sans perte. Tests CPU de compilation, conflits, normalisation,
transitions, capture et rejeu ; clavier exhaustif. Pas de hot reload ni éditeur de mapping.
UI absente de shipping. Formats et scénario dans [INPUT.md](../INPUT.md).

Preuves : input/rejeu/gameplay/inspecteur verts en CI Windows/Linux ; cinq poses composées validées sur D3D12/Vulkan Radeon, lavapipe et Metal CI, avec pas exacts et sans injection système (bilan de phase 3).
