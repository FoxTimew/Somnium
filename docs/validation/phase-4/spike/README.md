# Preuves du spike de phase 4

Les journaux sont textuels, normalisés en LF, sans espaces de fin de ligne ni
lignes vides finales. Chaque `provenance.json` contient
leurs empreintes archivées et celles des fichiers originaux, le SHA de tête de PR,
le lien CI et le SHA réellement exécuté (commit de fusion synthétique de GitHub).
`run.json` conserve les commandes nommées, durées, codes de sortie et état propre
du checkout ; `SUMMARY.md` garde le résultat de tout le pipeline moteur.

- [Première preuve](initial/provenance.json) : tête `61e2c22`,
  [CI 46002991162](https://github.com/FoxTimew/Somnium/actions/runs/46002991162),
  moteur inchangé, rechargement plain/profiling et ASan/UBSan Linux.
- [Preuve auditée](audited/provenance.json) : tête `38c8436`,
  [CI 46006326742](https://github.com/FoxTimew/Somnium/actions/runs/46006326742),
  deux tests par configuration, analyse clang-tidy 18 dédiée et pipeline moteur
  complet verts. L'audit Euler constate l'absence d'exactitude binaire sur les deux OS.

Les binaires, copies de modules et rapports de compilation complets restent sous
`C:/SomniumCI/runs/` et `/home/foxti/somnium-ci/runs/` selon la rétention de 14 jours.
Aucun artefact GitHub ou fichier de shader compilé n'est utilisé pour cette archive.
Le [compte rendu](../../../HOT_RELOAD_SPIKE.md) précise les limites de chaque preuve.
