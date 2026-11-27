<!-- Preuves locales produites par le moteur au jalon lots 1 et 2.
     Les résultats des jobs requis et de profilage sont conservés par tools/ci. -->
# Édition, enregistrement et restauration

Exécution locale Windows du 27 novembre 2026 sur Radeon RX 6600M,
commit `5afc84b3144b3d1ea7d916f6d1df37031e322bdc`, arbre propre.
Les rapports contiennent les SHA du binaire, du scénario et de chaque image.
Les images composent scène et UI dans le moteur : aucune lecture du bureau,
aucune injection clavier/souris. Les journaux confirment les couches de validation.

| Backend | Initial → édition | Édition → Arrêter | Édition → nouveau processus |
| --- | ---: | ---: | ---: |
| D3D12 Radeon | 10 839 pixels | 0 pixel | 0 pixel |
| Vulkan Radeon | 10 839 pixels | 0 pixel | 0 pixel |

Comparaisons exactes sur la région de scène définie dans
[`scenario.json`](../../../../tools/iteration/scenario.json), hors texte de diagnostic
variable. Des sondes distinctes exigent la présence de l’inspecteur et de la barre
Iteration. Le scénario crée un pivot et un enfant, assigne un modèle, déplace,
reparente, renomme et édite `game.AabbCollider`, puis supprime un sous-arbre.
Les valeurs invalides, cycles et modèles absents sont refusés sans remplacer World.

Deux Save successifs produisent les mêmes octets. Après Jeu puis Arrêter, la
sérialisation canonique redevient strictement celle enregistrée ; un autre processus
la charge et la sauvegarde sans changement. Chaque dossier contient aussi
`edited-scene.json`, le fichier effectivement publié dans la racine de preuve.
Les assets sources ne sont pas copiés dans le build. Les références D5/cube/scène
et leurs provenances restent inchangées.

| Capture | D3D12 | Vulkan |
| --- | --- | --- |
| Avant édition | [image](direct3d12/initial.png) | [image](vulkan/initial.png) |
| Édité et enregistré | [image](direct3d12/edited-saved.png) | [image](vulkan/edited-saved.png) |
| Arrêt après simulation | [image](direct3d12/stopped-restored.png) | [image](vulkan/stopped-restored.png) |
| Rechargé après redémarrage | [image](direct3d12/reloaded.png) | [image](vulkan/reloaded.png) |
| Rapport | [JSON](direct3d12/result.json) | [JSON](vulkan/result.json) |

Reproduction : `python tools/iteration/prove.py --executable <sandbox-iteration>
--backend <direct3d12|vulkan> --output <répertoire-de-preuve>`.
Le même script est requis en CI, avec lavapipe sous Linux. Les résumés par run
rapportent les durées des opérations et la mesure d’instantané ; les jobs manuels
`iteration_profiling=true` valident aussi la variante optimisée avec Tracy.

La [mesure locale Debug](snapshot-windows-debug.json), 200 captures après
20 chauffes, donne p50 4,45 ms, p95 7,20 ms, maximum 10,98 ms ; 16 instantanés
retiennent 552 432 octets. Le seuil accepté de 1 ms n’est pas atteint dans le
preset de développement : annulation reportée, sans bouton fictif.
Metal reste hors couverture requise de cette phase.
