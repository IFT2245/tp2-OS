# **TP2-OS**
## Ordonnanceur & Simulation de Système d’Exploitation

| Mots clefs                                | Description                                                        |
|-------------------------------------------|--------------------------------------------------------------------|
| **Système d’exploitation**                | Simulation d'un OS avec divers modes de concurrence                |
| **Synergie par pipeline**                 | Coordination optimisée des processus                               |
| **Concurrence distribuée**                | Gestion de tâches sur plusieurs machines                           |
| **Multi-thread**                          | Exécution parallèle dans un même processus                         |
| **Multi-processus**                       | Exécution parallèle avec plusieurs processus                       |
| **Interface**                             | Textuelle ou OpenGL (optionnelle)                                  |
| **Threads HPC overshadow**                | Gestion de threads haute performance                               |
| **Conteneurs éphémères**                  | Création et gestion temporaire de conteneurs                       |
| **Synergie de pipeline**                  | Optimisation et enchaînement des tâches                            |
| **Processus distribués**                  | Répartition des tâches sur plusieurs machines                      |
| **Démos multi-processus & multi-threads** | Illustrations des différentes stratégies de concurrence            |
| **Journaux (logs) de debug**              | Suivi et analyse des événements du système                         |
| **Ordonnancement ReadyQueue**             | FIFO, RR, BFS, Priority, SJF                                       |
| **HPC Overshadow**                        | Intégration de logique HPC overshadow                              |
| **Conteneurs éphémères**                  | Création et gestion de conteneurs temporaires                      |
| **Approche de concurrence**               | Gestion via un seul Worker                                         |
| **Progression en 5 niveaux de test**      | BASIC → NORMAL → MODES → EDGE → HIDDEN (≥ 60% requis pour avancer) |

## Fonctionnalités

| **Fonction**              | **Description**                                                                        |
|---------------------------|----------------------------------------------------------------------------------------|
| HPC Overshadow            | Lance plusieurs threads pour des sommes partielles HPC ou concurrence overshadow       |
| Conteneur Éphémère        | Crée un répertoire `/tmp/os_container_XXXXXX`, simulant l’utilisation d’un conteneur   |
| Pipeline                  | Exécute sleep ou HPC partielle sur plusieurs étapes de pipeline                        |
| Distribué                 | `fork()` de processus enfants si activé                                                |
| Multi-Processus           | Démonstration simple d’un `fork()`                                                     |
| Multi-Threads             | Si HPC overshadow n’est pas actif, lance un thread de base                             |
| Logs de Debug             | Affiche les détails de concurrence (somme HPC, conteneur éphémère, étapes de pipeline) |
| Ordonnancement ReadyQueue | FIFO, Round Robin, BFS, Priority, SJF                                                  |
| Difficulté de Jeu         | 0=none,1=easy,2=story,3=challenge,5=survival. Les flags déterminent le mode...         |
| 5 Niveaux de Tests        | BASIC, NORMAL, MODES, EDGE, HIDDEN. 60% min requis pour débloquer le suivant.          |

## Méthodes de Compilation

### Sans OpenGL

```bash
gcc -o scheduler \
    src/scheduler.c src/os.c src/process.c src/ready.c src/worker.c src/safe_calls_library.c \
    test/basic-test.c test/normal-test.c test/modes-test.c test/edge-test.c test/hidden-test.c \
    -I./include -I./src -I./test -lpthread

./scheduler
```

### Compilation avec **OpenGL**

| Mode de Compilation        | **Avec OpenGL, Interface Graphique**                  |
|----------------------------|-------------------------------------------------------|
| **Bibliothèques requises** | FreeGLUT ou GLUT classique, `libGL`, `libGLU`         |
| **Option de compilation**  | Inclure `-DUSE_OPENGL_UI` et lier `-lGL -lGLU -lglut` |

```bash
sudo apt-get update
sudo apt-get install freeglut3-dev mesa-common-dev
gcc -o scheduler \
    -DUSE_OPENGL_UI \
    src/scheduler.c src/os.c src/process.c src/ready.c src/worker.c src/safe_calls_library.c \
    test/basic-test.c test/normal-test.c test/modes-test.c test/edge-test.c test/hidden-test.c \
    -I./include -I./src -I./test \
    -lpthread -lGL -lGLU -lglut
    
./scheduler
```

### Procédure de Build avec CMake

```bash
mkdir build
cd build
```
| Mode de Compilation     | Commande CMake                                                      | Remarque                                                                   |
|-------------------------|---------------------------------------------------------------------|----------------------------------------------------------------------------| 
| **Sans OpenGL**         | ```bash cmake -DUSE_OPENGL_UI=OFF -DCMAKE_BUILD_TYPE=Release .. ``` | Mode texte uniquement                                                      |
| **Avec OpenGL**         | ```bash cmake -DUSE_OPENGL_UI=ON -DCMAKE_BUILD_TYPE=Debug .. ```    | Pas besoin de lier manuellement OpenGL/GLUT, CMake le gère automatiquement |


```bash
cmake --build . --target scheduler
./scheduler
```

## Utilisation

Après `./scheduler`, vous verrez un menu :

```
================= SCHEDULER MENU =================
Test progression level: 0/5 => ...
[1] Run NEXT Test in progression
[2] OS-based concurrency UI
[3] ReadyQueue concurrency UI
[4] Single Worker concurrency
[5] Show partial scoreboard
[6] Re-run a previously passed test suite
[7] Direct OpenGL-based OS UI (only if compiled with USE_OPENGL_UI)
[0] Exit
Your choice:
```

| Fonctionnalité       | Description                                                                                                  |
|----------------------|--------------------------------------------------------------------------------------------------------------|
| **Test Progression** | Débloquez les niveaux de jeu avec des scores de (≥ 60% par niveau). Score final sur 500 pts après validation |
| **UI OS**            | Activez/désactivez HPC, conteneur, pipeline, etc. `[R]` pour lancer, `[0]` pour quitter                      |
| **ReadyQueue**       | Choisissez une politique, enfilez et exécutez des processus, observez HPC/pipeline                           |
| **Single Worker**    | Concurrence avec un seul bloc : threads HPC, conteneur, pipeline, logs                                       |
| **Scoreboard**       | Affiche la réussite (0..4) et statut (verrouillé/débloqué)                                                   |
| **Re-run**           | Rejouez une suite pour améliorer votre score (meilleure retenue), `g_current_level==5`, tout est complété    |


## Structure du Répertoire (exemple)

```
.
├── CMakeLists.txt
├── build/               # Dossier de build cmake
├── src/
│   ├── scheduler.c, scheduler.h
│   ├── os.c, os.h
│   ├── process.c, process.h
│   ├── ready.c, ready.h
│   ├── worker.c, worker.h
│   ├── safe_calls_library.c, safe_calls_library.h
│   ...
├── test/
│   ├── basic-test.c,   basic-test.h
│   ├── normal-test.c,  normal-test.h
│   ├── modes-test.c,   modes-test.h
│   ├── edge-test.c,    edge-test.h
│   ├── hidden-test.c,  hidden-test.h
├── include/
│   └── ...
└── README.md
```
