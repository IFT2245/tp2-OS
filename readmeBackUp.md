# **TP2-OS**
## Ordonnanceur & Simulation de Système d’Exploitation

| Fonctionnalité                  | Description                                         |
|---------------------------------|-----------------------------------------------------|
| **Système d’exploitation**      | Simulation d'un OS avec divers modes de concurrence |
| **Politiques d’ordonnancement** | Gestion de l'exécution des tâches                   |
| **HPC Overshadow**              | Intégration de logique HPC overshadow               |
| **Conteneurs éphémères**        | Création et gestion de conteneurs temporaires       |
| **Synergie par pipeline**       | Coordination optimisée des processus                |
| **Concurrence distribuée**      | Gestion de tâches sur plusieurs machines            |
| **Multi-thread**                | Exécution parallèle dans un même processus          |
| **Multi-processus**             | Exécution parallèle avec plusieurs processus        |
| **Suites de tests**             | BASIC, NORMAL, MODES, EDGE, HIDDEN ---> GAME OVER   |

## Aperçu

| Fonctionnalité                            | Description                                                        |
|-------------------------------------------|--------------------------------------------------------------------|
| **Interface**                             | Textuelle ou OpenGL (optionnelle)                                  |
| **Threads HPC overshadow**                | Gestion de threads haute performance                               |
| **Conteneurs éphémères**                  | Création et gestion temporaire de conteneurs                       |
| **Synergie de pipeline**                  | Optimisation et enchaînement des tâches                            |
| **Processus distribués**                  | Répartition des tâches sur plusieurs machines                      |
| **Démos multi-processus & multi-threads** | Illustrations des différentes stratégies de concurrence            |
| **Journaux (logs) de debug**              | Suivi et analyse des événements du système                         |
| **Ordonnancement ReadyQueue**             | FIFO, RR, BFS, Priority, SJF                                       |
| **Approche de concurrence**               | Gestion via un seul Worker                                         |
| **Progression en 5 niveaux de test**      | BASIC → NORMAL → MODES → EDGE → HIDDEN (≥ 60% requis pour avancer) |

## Fonctionnalités

| **Fonction**           | **Description**                                                                        |
|------------------------|----------------------------------------------------------------------------------------|
| HPC Overshadow         | Lance plusieurs threads pour des sommes partielles HPC ou concurrence overshadow      |
| Conteneur Éphémère     | Crée un répertoire `/tmp/os_container_XXXXXX`, simulant l’utilisation d’un conteneur   |
| Pipeline               | Exécute sleep ou HPC partielle sur plusieurs étapes de pipeline                        |
| Distribué             | `fork()` de processus enfants si activé                                                |
| Multi-Processus        | Démonstration simple d’un `fork()`                                                    |
| Multi-Threads          | Si HPC overshadow n’est pas actif, lance un thread de base                             |
| Logs de Debug          | Affiche les détails de concurrence (somme HPC, conteneur éphémère, étapes de pipeline) |
| Ordonnancement ReadyQueue | FIFO, Round Robin, BFS, Priority, etc.                                             |
| Difficulté de Jeu      | 0=none,1=easy,2=story,3=challenge,5=survival. Les flags déterminent le mode.          |
| 5 Niveaux de Tests     | BASIC, NORMAL, MODES, EDGE, HIDDEN. 60% min requis pour débloquer le suivant.         |

| Mode de Compilation                                                                          | Description                                   |
|----------------------------------------------------------------------------------------------|-----------------------------------------------|
| **Sans OpenGL**                                                                              | Utilisation sans l’interface graphique OpenGL |
| **Bibliothèques requises**                                                                   | OpenGL/GLUT non nécessaires                   |
| **Interface obtenue**                                                                        | Interface textuelle uniquement                |
| **Option de compilation**                                                                    | Ne pas inclure `-DUSE_OPENGL_UI`              |

```bash                                                                                      
gcc -o scheduler \                                                                           
src/scheduler.c src/os.c src/process.c src/ready.c src/worker.c src/safe_calls_library.c \   
test/basic-test.c test/normal-test.c test/modes-test.c test/edge-test.c test/hidden-test.c \ 
-I./include -I./src -I./test -lpthread
```                                                       
