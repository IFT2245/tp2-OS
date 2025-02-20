# Advanced Scheduler & OS Simulation

This project simulates an **Operating System** environment with concurrency modes, scheduling policies, HPC overshadow, ephemeral container logic, pipeline synergy, distributed concurrency, multi-thread, multi-process, debug logs, and a game-like test progression across five suites: **BASIC**, **NORMAL**, **MODES**, **EDGE**, and **HIDDEN**.

---

## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [Compilation Approaches](#compilation-approaches)
   - [Without OpenGL](#without-opengl)
   - [With OpenGL](#with-opengl)
   - [CMake Build Procedure](#cmake-build-procedure)
4. [Usage](#usage)
5. [Directory Structure](#directory-structure)
6. [Game Progression](#game-progression)
7. [References & Theory](#references--theory)

---

## Overview

You can interact with multiple concurrency features through a textual or optional **OpenGL** UI:

- HPC overshadow threads
- Ephemeral container creation
- Pipeline synergy
- Distributed processes
- Multi-Process & Multi-Thread demos
- Debug logs
- ReadyQueue scheduling (FIFO, RR, BFS, Priority, SJF)
- Single Worker concurrency approach
- 5-level test progression: **BASIC → NORMAL → MODES → EDGE → HIDDEN**, each requiring ≥ 60% success to unlock the next.  

---

## Features

| **Feature**           | **Description**                                                               |
|-----------------------|-------------------------------------------------------------------------------|
| HPC Overshadow        | Spawn multiple threads for HPC partial sums or overshadow concurrency         |
| Container Ephemeral   | Create a `/tmp/os_container_XXXXXX` dir, simulating container usage           |
| Pipeline              | Sleep or partial HPC in multiple pipeline stages                              |
| Distributed           | Fork child processes if flagged                                               |
| Multi-Process         | Demonstrates a simple `fork()` usage                                          |
| Multi-Thread          | If HPC overshadow not active, spawns a single thread                          |
| Debug Logs            | Print concurrency details (HPC sums, ephemeral creation, pipeline steps)      |
| ReadyQueue Scheduling | FIFO, Round Robin, BFS, Priority, etc.                                        |
| Game Difficulty       | 0=none,1=easy,2=story,3=challenge,5=survival. Set flags for each mode.        |
| 5-Level Tests         | BASIC, NORMAL, MODES, EDGE, HIDDEN. ≥60% success needed to unlock next.       |

---

## Compilation Approaches

### Without OpenGL

If you **do not** have the necessary OpenGL/GLUT libraries installed or **do not** want the OpenGL UI, simply **exclude** `-DUSE_OPENGL_UI` from your build. This yields only the **text-based** UI.

**One-liner `gcc` command** (text-mode only):
```bash
gcc -o scheduler \
    src/scheduler.c src/os.c src/process.c src/ready.c src/worker.c src/safe_calls_library.c \
    test/basic-test.c test/normal-test.c test/modes-test.c test/edge-test.c test/hidden-test.c \
    -I./include -I./src -I./test -lpthread
```
Afterwards, run:
```bash
./scheduler
```
You’ll have a textual interface only, skipping any `#ifdef USE_OPENGL_UI` sections.

---

### With OpenGL

If you **do** want the **OpenGL** UI for an advanced graphical display, you need to install the appropriate dev packages (FreeGLUT or classical GLUT, plus `libGL`, `libGLU`). For example on Ubuntu-like distros:

```bash
sudo apt-get update
sudo apt-get install freeglut3-dev mesa-common-dev
```
Then compile **with** `-DUSE_OPENGL_UI`, plus linking `-lGL -lGLU -lglut`:
```bash
gcc -o scheduler \
    -DUSE_OPENGL_UI \
    src/scheduler.c src/os.c src/process.c src/ready.c src/worker.c src/safe_calls_library.c \
    test/basic-test.c test/normal-test.c test/modes-test.c test/edge-test.c test/hidden-test.c \
    -I./include -I./src -I./test \
    -lpthread -lGL -lGLU -lglut
```
Now, once you run `./scheduler`, you can see an option (like `[7]`) to start an OpenGL-based UI, which renders some HPC info, pipeline, ephemeral container usage, etc.

> **Note:** If you **cannot** install these packages (lacking sudo privileges), skip the `-DUSE_OPENGL_UI` approach and do a text-based UI only.

---

### CMake Build Procedure

**Recommended** for maintainability. Suppose your top-level `CMakeLists.txt` includes:

```cmake
cmake_minimum_required(VERSION 3.20)
project(TP2 VERSION 24 DESCRIPTION "Real-Time Process Scheduler" LANGUAGES C)

set(CMAKE_C_STANDARD 17)

option(USE_OPENGL_UI "Enable OpenGL-based UI" OFF)

if(USE_OPENGL_UI)
    find_package(OpenGL REQUIRED)
    find_package(GLUT REQUIRED)
    add_compile_definitions(USE_OPENGL_UI)
endif()

add_executable(scheduler
    src/scheduler.c
    src/os.c
    src/process.c
    src/ready.c
    src/worker.c
    src/safe_calls_library.c

    # tests in same build if you want
    test/basic-test.c
    test/normal-test.c
    test/modes-test.c
    test/edge-test.c
    test/hidden-test.c
)

target_include_directories(scheduler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/test
)

target_link_libraries(scheduler PRIVATE pthread)

if(USE_OPENGL_UI)
    target_link_libraries(scheduler PRIVATE OpenGL::GL GLUT::GLUT)
    # add GLU if needed
    # find_package(OpenGL REQUIRED GLU)
    # target_link_libraries(scheduler PRIVATE OpenGL::GLU)
endif()
```

**Steps**:

1. **Create Build Directory**:
   ```bash
   mkdir build
   cd build
   ```
2. **Configure**:
   - **Without** OpenGL:
     ```bash
     cmake -DUSE_OPENGL_UI=OFF -DCMAKE_BUILD_TYPE=Release ..
     ```
   - **With** OpenGL:
     ```bash
     cmake -DUSE_OPENGL_UI=ON -DCMAKE_BUILD_TYPE=Debug ..
     ```
3. **Build**:
   ```bash
   cmake --build . --target scheduler
   ```
4. **Run**:
   ```bash
   ./scheduler
   ```

No special linking or define flags are needed manually once you have `USE_OPENGL_UI=ON` in your CMake config, as it sets `-DUSE_OPENGL_UI` at compile time and links the needed libraries.

---

## Usage

After launching `./scheduler`, you’ll see a menu:

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

- **Test Progression**:
   - Must pass **BASIC** with ≥ 60% success to unlock **NORMAL**, then pass that to unlock **MODES**, etc., up to **EDGE**, finally **HIDDEN**.
   - Once all 5 pass, you get a final “**GAME OVER**” scoreboard out of 500 total synergy points.

- **OS-based concurrency UI**:
   - Toggle HPC overshadow, ephemeral container, pipeline, distributed, HPC threads, pipeline stages, etc.
   - Then `[R]` to run concurrency or `[0]` to exit.

- **ReadyQueue**:
   - Choose scheduling policy (FIFO, RR, BFS, Priority, etc.), HPC/Container flags, pipeline stages, distributed count, etc.
   - Enqueue processes, run them, see HPC overshadow or container ephemeral usage.

- **Single Worker**:
   - HPC overshadow in a single concurrency approach (threads sub-sum partial HPC), ephemeral container creation, pipeline synergy in one “worker run.”

- **Scoreboard**:
   - Shows partial success for each suite (0..4) plus locked/unlocked status.

- **Re-run**:
   - Attempt an old suite again to try for better success (the aggregator keeps the best of either run).

---

## Directory Structure

A typical layout:

```
.
├── CMakeLists.txt
├── build/                 # Where cmake build artifacts go
├── src/
│   ├── scheduler.c
│   ├── os.c, os.h
│   ├── process.c, process.h
│   ├── ready.c, ready.h
│   ├── worker.c, worker.h
│   ├── safe_calls_library.c, safe_calls_library.h
│   ...
├── test/
│   ├── basic-test.c, basic-test.h
│   ├── normal-test.c, normal-test.h
│   ├── modes-test.c,  modes-test.h
│   ├── edge-test.c,   edge-test.h
│   ├── hidden-test.c, hidden-test.h
│   ...
├── include/
│   └── ...
└── README.md              # This file
```

---

## Game Progression

Each aggregator prints something like `Success rate: X%`, or returns 0 => pass => 100% or 1 => fail => 0%. The code:

1. **BASIC** → must pass with ≥60% to unlock
2. **NORMAL** → pass with ≥60% to unlock
3. **MODES** → pass with ≥60% to unlock
4. **EDGE** → pass with ≥60% to unlock
5. **HIDDEN** → pass with ≥60% => final “GAME OVER” scoreboard.

**Re-running** an old suite can improve your partial success for that suite. Once `g_current_level==5`, you’ve completed them all, synergy is aggregated out of 500 points.

---

## References & Theory

- **Operating Systems**:
   - [OSTEP (Operating Systems: Three Easy Pieces)](https://pages.cs.wisc.edu/~remzi/OSTEP/)
   - [Linux Syscalls for fork/wait](https://man7.org/linux/man-pages/man2/fork.2.html)
- **Concurrency**:
   - [POSIX threads tutorial](https://computing.llnl.gov/tutorials/pthreads/)
   - HPC overshadow sums
- **OpenGL**:
   - [FreeGLUT vs classical GLUT differences](http://freeglut.sourceforge.net/docs.html)
   - [OpenGL basic references](https://www.khronos.org/opengl/wiki/)
- **Scheduling**:
   - FIFO, RR, BFS, Priority, SJF references from OS theory

Feel free to adapt or expand features (like advanced ephemeral container illusions with `chroot` or Linux namespaces, more pipeline synergy, deeper HPC overshadow, etc.).

**Enjoy** exploring concurrency, scheduling, and a fun test-based **game**!
