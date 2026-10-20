<!--
Defines contributor-facing rules that complement ADR-0002.
Commands use project-scoped tooling and leave upstream dependency code unchanged.
-->
# Style et contribution

C++20, sans modules de langage. Le code moteur est compilé sans exceptions ni
RTTI. Les erreurs récupérables retournent `somnium::Result<T>` ; assertions et
vérifications servent aux invariants de programme. Ne jamais ignorer un résultat
faillible ni placer un effet de bord nécessaire dans `SOMNIUM_ASSERT`.

Types `PascalCase`, fonctions/variables `camelCase`, membres privés `m_`,
constantes `kPascalCase`, macros `SOMNIUM_*`, fichiers `snake_case`.
Les composants ECS sont des structs plates dans `somnium`, les services dans
`somnium::<module>`. Les en-têtes utilisent `#pragma once` et chaque fichier
commence par un commentaire de rôle de deux à cinq lignes. Les fichiers JSON
restent valides et utilisent les champs descriptifs de leur schéma.

Les en-têtes publics résident dans `engine/<module>/include/somnium/<module>`.
Ils n'exposent aucun type SDL. La façade `Log` est le seul accès à spdlog.
Pas d'inclusion relative pour contourner le graphe, ni d'include global CMake.
Le contrôle `tools/check_architecture.py` vérifie ces frontières.

Valeurs et `std::unique_ptr` pour la possession, références et vues pour les
emprunts. Une durée de vie doit être explicite. Aucun pointeur durable vers
un asset, aucune possession partagée sans ADR, aucune globale mutable moteur
hors logs. Les handles sont validés par le propriétaire du stockage.

Les conteneurs chauds réutilisent leur capacité. Les allocations au chargement
sont permises et documentées ; aucun chargement disque ni log par objet/frame
dans la boucle stabilisée. Les exceptions internes aux dépendances ne doivent
pas traverser une frontière moteur.

Formatage commun : **clang-format 18.1.8**.

```sh
python -m pip install clang-format==18.1.8
python tools/format.py
python tools/format.py --check
python tools/check_architecture.py .
```

`.clang-tidy` fournit les contrôles de revue. Le fichier de compilation est
généré dans `build/<preset>/compile_commands.json`. Les caches de dépendances ne
sont jamais reformatés. Les warnings du projet deviennent des erreurs en CI.

Un test doit exercer un contrat ou un cas de panne réel. Chaque module actif
a sa cible doctest dans `tests/`. Les tests temporels injectent des durées ;
les tests de fenêtre sont bornés et étiquetés `window`.

Branche par phase, commits conventionnels et poussées cohérentes suivies de
lecture CI. `main`, la fusion `--no-ff` et le tag de phase suivent les
autorisations explicites du propriétaire. Aucune phase suivante n'est implicite.
