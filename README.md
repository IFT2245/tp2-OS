````README:
# **TP2-OS**
## Ordonnanceur et Simulation de Système d’Exploitation

| **Mots clés**                              | **Description**                                                   |
|--------------------------------------------|-------------------------------------------------------------------|
| **Système d’exploitation**                 | Simulation d’un OS avec différents modes de concurrence           |
| **Synergie par pipeline**                  | Coordination optimisée de processus via un pipeline               |
| **Concurrence distribuée**                 | Gestion de tâches sur plusieurs machines                          |
| **Multi-thread**                           | Exécution parallèle au sein d’un même processus                   |
| **Multi-processus**                        | Exécution parallèle par plusieurs processus                       |
| **Interface**                              | Textuelle ou OpenGL (optionnelle)                                 |
| **Threads HPC overshadow**                 | Gestion de threads haute performance                              |
| **Conteneurs éphémères**                   | Création et utilisation temporaire de conteneurs                  |
| **Synergie de pipeline**                   | Optimisation et enchaînement des tâches dans un pipeline          |
| **Processus distribués**                   | Répartition des tâches sur plusieurs machines                     |
| **Démos multi-processus & multi-threads**  | Illustrations pratiques des différentes stratégies de concurrence |
| **Journaux (logs) de debug**               | Suivi et analyse des événements du système                        |
| **Ordonnancement ReadyQueue**              | FIFO, RR, BFS, Priority, SJF, etc.                                |
| **HPC Overshadow**                         | Intégration de logique HPC overshadow                             |
| **Conteneurs éphémères**                   | Création et gestion de conteneurs temporaires                     |
| **Approche de concurrence**                | Gestion via un seul Worker au mode basic                          |

---

## Fonctionnalités

| **Fonction**                    | **Description**                                                                          |
|---------------------------------|------------------------------------------------------------------------------------------|
| **HPC Overshadow**              | Lance plusieurs threads pour effectuer des calculs HPC ou un mode overshadow            |
| **Conteneur Éphémère**          | Crée un répertoire `/tmp/os_container_XXXXXX`, simulant un conteneur                    |
| **Pipeline**                    | Exécute `sleep` ou des calculs partiels HPC sur plusieurs étapes                         |
| **Distribué**                   | Utilise `fork()` pour créer des processus enfants si activé                             |
| **Multi-Processus**             | Démontre un `fork()` basique                                                             |
| **Multi-Threads**               | Si HPC overshadow n’est pas actif, lance un thread de base                              |
| **Logs de Debug**               | Affiche les détails de la concurrence (somme HPC, conteneur éphémère, étapes pipeline)  |
| **Ordonnancement ReadyQueue**   | FIFO, Round Robin, BFS, Priority, SJF                                                   |
| **5 Niveaux de Tests**          | BASIC, NORMAL, MODES, EDGE, HIDDEN (60% min pour débloquer le suivant)                 |

---

## Choix de l’Interface : Textuelle ou OpenGL

Lors du **menu interactif**, le programme propose :
1. **Mode Texte** : aucune dépendance graphique.
2. **Mode OpenGL** : si et **seulement si** la compilation a été faite avec l’option `-DUSE_OPENGL_UI` et les bibliothèques nécessaires.

> **Exemple** : Si vous avez compilé avec `-DUSE_OPENGL_UI`, le programme vous offrira l’option OpenGL dans le menu. Sinon, vous ne verrez que le mode texte.

---

## Méthodes de Compilation

```bash
sudo add-apt-repository universe
sudo apt update
sudo apt install freeglut3-dev
sudo apt install cmake xorg-dev libglu1-mesa-dev
```

### Compilation en Mode Texte (sans OpenGL)

```bash
gcc -o scheduler \
    src/scheduler.c src/os.c src/process.c src/ready.c src/worker.c src/safe_calls_library.c \
    test/basic-test.c test/normal-test.c test/modes-test.c test/edge-test.c test/hidden-test.c \
    -I./include -I./src -I./test -lpthread

./scheduler
```

### Compilation avec OpenGL (Interface Graphique)

| **Mode de Compilation**        | **Avec OpenGL, Interface Graphique**                  |
|--------------------------------|-------------------------------------------------------|
| **Bibliothèques requises**     | FreeGLUT ou GLUT classique, `libGL`, `libGLU`         |
| **Option de compilation**      | Inclure `-DUSE_OPENGL_UI` et lier `-lGL -lGLU -lglut`  |

```bash
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

| **Mode de Compilation** | **Commande CMake**                                               | **Remarque**                                                                              |
|-------------------------|------------------------------------------------------------------|-------------------------------------------------------------------------------------------|
| **Sans OpenGL**         | `cmake -DUSE_OPENGL_UI=OFF -DCMAKE_BUILD_TYPE=Release ..`        | Mode texte uniquement                                                                     |
| **Avec OpenGL**         | `cmake -DUSE_OPENGL_UI=ON -DCMAKE_BUILD_TYPE=Debug ..`           | Aucune liaison manuelle nécessaire pour OpenGL/GLUT, CMake s’occupe de tout               |

```bash
cmake --build . --target scheduler
./scheduler
```

## Niveaux de Test et Objectifs

| **Niveau** | **Environnement**                      | **Objectif**                                                                                                                                                          |
|------------|-----------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **BASIC**  | Un seul Worker                          | Tests plus simples sur chaque algorithme (ex. `cfs`, `fifo`, `rr`) pour vérifier la fonctionnalité de base.                                                            |
| **NORMAL** | Un seul Worker, utilisation de threads  | Couvre des scénarios de concurrence plus avancés, exécutés sur tous les algorithmes (ex. `cfs-srtf`, `hrrn`, etc.).                                                    |
| **MODES**  | Introduction de “hpc-overshadow”        | Ajoute un mode supplémentaire **hpc-overshadow** en plus des algorithmes classiques. Peut tester des cas multi-conteneurs ou multi-processus.                          |
| **EDGE**   | Cas limites                             | Valide la robustesse de tous les modes, y compris overshadow, sur des scénarios extrêmes (charge élevée, temps très court ou très long, etc.).                         |
| **HIDDEN** | Tests cachés                            | Même champ que EDGE, mais scénarios non dévoilés pour confirmer la solidité et la réactivité du système. Une fois ce niveau débloqué, un **test théorique secret** est également accessible. Il porte majoritairement sur l’utilisation de HPC overshadow, la containerisation et le choix optimal des modes selon des cas d’usage précis. |

> **Note** : Le passage à chaque niveau requiert un minimum de 60% de réussite au niveau précédent.

---

## Détails sur les Algorithmes d’Ordonnancement

| **Algorithme**    | **Nom Complet**                          | **Préemptif ?** | **Caractéristiques Principales**                                                                                                              | **Niveau d’Introduction** |
|-------------------|------------------------------------------|-----------------|-----------------------------------------------------------------------------------------------------------------------------------------------|---------------------------|
| **cfs**           | Completely Fair Scheduler                | Oui (en général)| Basé sur la répartition “équitable” du temps CPU. Chaque tâche dispose d’une part de processeur proportionnelle à sa priorité.                | BASIC                     |
| **cfs-srtf**      | CFS + Shortest Remaining Time First      | Oui             | Combine la logique de partage équitable avec une reprise plus fréquente de la tâche ayant le moins de temps restant.                          | NORMAL                    |
| **fifo**          | First In, First Out                      | Non (bloquant)  | La première tâche arrivée est exécutée en totalité, sans interruption.                                                                        | BASIC                     |
| **hrrn**          | Highest Response Ratio Next              | Non (classique) | Sélectionne la tâche dont (Temps d’attente + Durée) / Durée est maximal.                                                                      | NORMAL                    |
| **hrrn-rt**       | Variante HRRN (réactive temps réel)      | Oui (souvent)   | Ajoute une préemption ou un ajustement des priorités pour répondre à des contraintes “temps réel”.                                            | NORMAL                    |
| **rr**            | Round Robin                              | Oui             | Chaque tâche obtient une tranche de CPU (“quantum”). À la fin du quantum, la tâche est replacée en file d’attente si elle n’est pas terminée. | BASIC                     |
| **sjf**           | Shortest Job First                       | Non             | Priorise la tâche la plus courte (durée totale). Non préemptif.                                                                               | NORMAL                    |
| **strf**          | Shortest Time Remaining First            | Oui             | Variante préemptive du SJF. Exécute la tâche avec le temps restant minimal.                                                                   | NORMAL                    |
| **hpc-overshadow**| HPC Overshadow (mode haute performance)  | Oui             | Intègre des threads HPC. Se substitue aux autres modes ou les complète, introduit au niveau MODES pour des scénarios complexes.               | MODES                     |

---

## Scoreboard, Choix du Niveau et Déroulement des Tests

Le **programme interactif** vous permettra de :
- **Sélectionner le Niveau** (BASIC, NORMAL, MODES, EDGE, HIDDEN)  
  ou **lancer automatiquement** tous les niveaux débloqués.
- **Choisir l’Interface** (texte ou OpenGL, si la compilation l’autorise).

### Scoreboard
- **À la fin de l’exécution**, un **scoreboard** récapitule :
    - Le **score par niveau** (tests passés / total), ainsi que le pourcentage obtenu.
    - Les algorithmes d’ordonnancement utilisés et leur statut (succès ou échec).
    - Le **score global** décidant du déblocage éventuel du niveau suivant (≥ 60%).

| **Niveau** | **Nb Tests** | **Réussites** | **Échecs** | **Score (%)** | **Débloqué ?** |
|------------|-------------:|--------------:|-----------:|--------------:|:-------------:|
| BASIC      | 10           | 8            | 2          | 80            | ✅            |
| NORMAL     | 12           | 9            | 3          | 75            | ✅            |
| MODES      | 15           | 7            | 8          | 46            | ❌            |
| EDGE       | -            | -            | -          | -             | ❌            |
| HIDDEN     | -            | -            | -          | -             | ❌            |

Une fois HIDDEN débloqué, le **test théorique secret** s’ajoute : il évalue principalement
1. **HPC Overshadow**
2. **Containerisation**
3. **Sélection optimale des modes** (cas d’usage spécifiques)
4. **Automatisation de la sélection optimale des modes**

## Aperçu de la Structure des Fichiers

| **Fichier**                  | **Rôle**                                                                                                                      |
|------------------------------|------------------------------------------------------------------------------------------------------------------------------|
| **`main.c`**                | Point d’entrée minimal, appelle une fonction globale (`run_levels()`) ou un gestionnaire interactif (choix du niveau, interface, etc.). |
| **`runner.c` / `runner.h`** | Gère la logique itérant sur chaque niveau, détermine les algorithmes, agrège les résultats pour le scoreboard, etc.          |
| **`os.[ch]`, `scheduler.[ch]`, `worker.[ch]`, `ready.[ch]`, `process.[ch]`** | Simulation interne de l’OS (processus, files d’attente, scheduling).            |
| **`basic-test.c`**          | Scénarios de test pour le niveau BASIC.                                                                                      |
| **`normal-test.c`**         | Scénarios de test pour le niveau NORMAL.                                                                                     |
| **`modes-test.c`**          | Scénarios de test pour le niveau MODES (incluant hpc-overshadow).                                                            |
| **`edge-test.c`**           | Tests poussés (EDGE).                                                                                                        |
| **`hidden-test.c`**         | Tests cachés (HIDDEN).                                                                                                       |

---

## Schéma Global : Niveaux, Modes et Tests

| **Niveau**    | **Modes d’Ordonnancement**                            | **Exemples de Tests**                                                         | **Rapport**                |
|---------------|-------------------------------------------------------|-------------------------------------------------------------------------------|----------------------------|
| **BASIC**     | `cfs`, `fifo`, `rr` (et autres simples)               | Cas simples (peu de processus), un seul Worker                                | Succès/Échec par algorithme|
| **NORMAL**    | Ajout de `cfs-srtf`, `hrrn`, `hrrn-rt`, `sjf`, `strf` | Scénarios multi-threads, priorités, temps d’attente, etc.                     | Succès/Échec + code erreur |
| **MODES**     | Tous les précédents + `hpc-overshadow`                | Tests avec conteneurs, multi-processus, HPC overshadow                        | Statuts par algorithme     |
| **EDGE**      | Tous les algos (y compris overshadow)                 | Stress tests, edge cases (temps très court/long, charge extrême, etc.)        | Succès/Échec + logs        |
| **HIDDEN**    | Identique à EDGE + Processus distribués               | Scénarios cachés pour évaluer la robustesse finale + **test théorique secret** | Rapports non dévoilés  |

- **Progression** :
    - En **choisissant le niveau** (menu ou ligne de commande), seuls les tests du niveau indiqué sont lancés.
    - En **lancement séquentiel**, le programme part du plus bas niveau non encore validé et enchaîne jusqu’à l’éventuel échec (<60%).
    - Une fois HIDDEN débloqué, un test théorique secret s’ajoute automatiquement pour compléter l’évaluation globale.

- **Scoreboard** :
    - Le tableau final illustre les performances par niveau, le score global et le score du test final si HIDDEN est validé````