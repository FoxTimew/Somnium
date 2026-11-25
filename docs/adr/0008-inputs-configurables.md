<!--
Décisions validées pour les inputs génériques et leurs garanties temporelles.
Leur implémentation suit le point de contrôle scène et extraction.
-->
# ADR-0008 — Inputs configurables

Date : 25 novembre 2026. Statut : **accepté**.

## Contexte

Les noms d'actions appartiennent au jeu. La configuration doit préserver les
transitions pendant les frames sans pas et les rattrapages de simulation.

## Décision

- **Configuration.** JSON v1 sous `assets/input/` : `version/actions/axes`,
  noms uniques, plusieurs liaisons par destination. Runtime fournit le lecteur ;
  compilation transactionnelle avant start en `ActionId`/`AxisId` typés.
  Le jeu résout ses IDs une fois ; configuration immutable, indices stables
  jusqu'à destruction. Aucun lookup par nom dans la boucle ni enum du sandbox
  dans input.
- **Sources/valeurs.** Actions : touches physiques, boutons souris/manette,
  OU des sources maintenues. Axes `rate` : paires de touches ±, sticks/gâchettes,
  persistants, sommés puis bornés à [-1,1]. Axes `delta` : souris/molette,
  cumulés une fois sans écrêtage ni facteur dt ; le jeu applique dt aux rates.
  Axes : gain fini et inversion ; zone morte axiale 0 ≤ d < 1 sur manette.
  Liaisons manette : filtre `any` ou slot 0..3. Sticks normalisés asymétriquement,
  gâchettes [0,1], reste hors zone morte remis à l'échelle avant gain.
  Condition facultative d'action maintenue pour les deltas, évaluée dans l'ordre
  des événements (souris pendant Orbit). Quatre manettes ; excédent signalé.
- **Clavier.** `core::Key` sur 16 bits couvre toutes les touches nommées
  non réservées du `SDL_scancode.h` 3.4.18 épinglé : F1–F24, pavé numérique,
  modificateurs gauche/droite, internationales et multimédia compris.
  Noms Somnium stables dans JSON, table explicite dans platform, aucun cast SDL.
  Unknown/réservées non configurables ; texte/IME séparé des touches physiques.
  Aucun SDL dans input, seulement les événements neutres de core.
- **Erreurs.** JSON/version/source/nom inconnus, doublons, types incompatibles,
  valeurs invalides ou action conditionnelle absente : erreur contextualisée,
  configuration inchangée. Réutiliser une source physique pour
  plusieurs destinations ou dupliquer une liaison constitue un conflit ;
  filtres `any`/slot chevauchants détectés. Lire une action comme condition n'est
  pas une liaison supplémentaire et reste limitée aux deltas pendant cette action.
  Aucune priorité implicite ; contextes/modes partageant une source reportés.
- **Transitions.** File ordonnée, estampillée au prochain pas par platform ;
  un advance par pas consécutif consomme une seule fois les événements éligibles,
  conserve les futurs et survit aux frames sans pas. Journal ordonné des
  transitions logiques, plus résumés pressed/released/held : double tap conservé.
  Au pas suivant, impulsions effacées, held/rates conservés ; aucun saut d'indice
  lorsque l'accumulateur abandonne du temps. Répétition clavier ignorée,
  focus perdu/déconnexion libèrent une fois les sources concernées.
- **UI.** debug_ui affiche noms, IDs, pas, transitions/actions et axes actifs
  en lecture seule, sans consommer d'événements. Runtime applique ses demandes
  neutres de capture : libération unique à l'entrée, presses/deltas capturés
  exclus du gameplay, relâchements toujours traités, nouvelle pression exigée
  au retour. Aucun ImGui dans input ou les sources de base platform/gpu.

- **Rejeu.** JSON v1 d’événements neutres horodatés par pas, livré par platform
  avant chaque pas fixe ; aucun événement de gameplay natif pendant le rejeu.
  Les captures figent un pas précis sans injection système. Windows reste manuel
  pour éviter les fenêtres visibles dans la session de travail. Détails : INPUT.md.

## Alternatives

Enum moteur ou noms résolus par pas : couplage/coût évitables. Axe mélangeant
souris et stick : unités dépendantes de la cadence. Booléens seuls :
plusieurs transitions deviennent indiscernables.

## Conséquences

Déplacement, orbite et Interact sont définis par le jeu. Capacités réservées ;
surcharge exceptionnelle extensible et journalisée, aucun événement jeté.
Ni hot reload ni éditeur de mappings. Tests CPU : compilation/conflits,
multi-liaisons, normalisation, doubles taps, frames sans pas/rattrapages,
rate/delta, focus/UI/déconnexions ; correspondance clavier exhaustive dans platform.
Inspecteur et affichage d'inputs absents de shipping.

Preuves : clavier, compilation/conflits, transitions, capture neutre et rejeu couverts par tests CPU locaux ; validation CI du rejeu et panneau UI à terminer.
