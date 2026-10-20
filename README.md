<!--
Introduces Somnium and the validated foundation milestone.
Build instructions are expanded alongside the phase 0 implementation.
-->
# Somnium

Moteur de jeu 3D C++20, développé par phases sur Windows, Linux et macOS.
La phase 0 est en cours : outillage, fondations et fenêtre vide, sans device GPU.

- [Architecture](docs/ARCHITECTURE.md)
- [Stack et dépendances](docs/adr/0001-stack-dependances-licences.md)
- [Conventions](docs/adr/0002-conventions.md)

Le jalon documentaire est accepté. Le socle `main` contient les documents,
ce README et les règles Git/LFS ; le moteur est développé sur
`phase/0-fondations`. La fusion de phase et le tag `phase-0` attendent une
validation distincte.

Prérequis prévus : Git avec Git LFS, CMake ≥ 3.28, Ninja et un compilateur C++20.
Les dépendances sont acquises par CPM avec versions exactes et cache
`CPM_SOURCE_CACHE`. Les instructions exécutables de build seront ajoutées
avec les cibles de phase 0.
