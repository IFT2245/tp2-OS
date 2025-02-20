Below is an **updated** set of **CMakeLists.txt** files for a **two-level** CMake project:

1. **Top-level `CMakeLists.txt`** in the project root directory.
2. **`src/CMakeLists.txt`** where you compile `scheduler` and link optional OpenGL libraries.

This structure:

- **Prevents** in-source builds.
- **Defines** an option `USE_OPENGL_UI` (default ON or OFF as you prefer).
- **Finds** OpenGL and GLUT if `USE_OPENGL_UI=ON`.
- **Builds** an executable **`scheduler`** that includes your OS sources, Worker, ReadyQueue, `safe_calls_library`, plus your test sources (`basic-test`, etc.).
- **Links** pthreads and, optionally, OpenGL + GLUT.
- **Does not** list `.h` files in `add_executable` (typical best practice to list only `.c` or `.cpp`).

---

## **Project Layout**

```
ProjectRoot/
├─ CMakeLists.txt               # Top-level
├─ cmake/
│   └─ dependencies.cmake       # Optional or can be empty
├─ src/
│   ├─ CMakeLists.txt           # Where we define the target "scheduler"
│   ├─ os.c, os.h
│   ├─ process.c, process.h
│   ├─ ready.c, ready.h
│   ├─ worker.c, worker.h
│   ├─ safe_calls_library.c, safe_calls_library.h
│   ├─ scheduler.c, scheduler.h
├─ test/
│   ├─ basic-test.c, basic-test.h
│   ├─ normal-test.c, normal-test.h
│   ├─ edge-test.c, edge-test.h
│   ├─ hidden-test.c, hidden-test.h
│   ├─ modes-test.c, modes-test.h
└─ ...
```

### **1) Top-Level `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.20)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR
        "In-source builds are not allowed. Please create a separate build directory, e.g.:\n"
        "  mkdir build && cd build && cmake ..")
endif()

project(TP2
    VERSION 24
    DESCRIPTION "Real-Time Process Scheduler"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)

# If you have extra dependencies logic:
include(cmake/dependencies.cmake)  # This can remain empty or contain custom checks.

# Let user enable or disable the OpenGL-based UI
option(USE_OPENGL_UI "Enable OpenGL-based UI code" ON)

# Add subdirectory "src" where the main logic is
add_subdirectory(src)
```

- **Note**: By default, we set `USE_OPENGL_UI=ON`. You can change to OFF if you like.

### **2) `src/CMakeLists.txt`**

```cmake
# Inside src/ directory

if(USE_OPENGL_UI)
    # If user wants OpenGL UI, find the libraries
    find_package(OpenGL REQUIRED)
    find_package(GLUT REQUIRED)
    # Add -DUSE_OPENGL_UI to compiler flags
    add_compile_definitions(USE_OPENGL_UI)
endif()

# Always find Threads for pthread
find_package(Threads REQUIRED)

add_executable(scheduler
    # Core OS sources
    os.c
    process.c
    ready.c
    worker.c
    safe_calls_library.c
    scheduler.c

    # The five test sources
    ../test/basic-test.c
    ../test/normal-test.c
    ../test/edge-test.c
    ../test/hidden-test.c
    ../test/modes-test.c

    # (We do NOT list .h headers here. It's recommended to omit them from add_executable.)
)

# Tweak include dirs if needed
target_include_directories(scheduler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}       # for local .h
    ${CMAKE_CURRENT_SOURCE_DIR}/..    # maybe to see "test" or "include"
)

# Link pthread
target_link_libraries(scheduler PRIVATE Threads::Threads)

# If using the OpenGL-based UI, link to OpenGL + GLUT
if(USE_OPENGL_UI)
    target_link_libraries(scheduler PRIVATE OpenGL::GL GLUT::GLUT)
    # If your code also needs -lGLU, do:
    # find_package(OpenGL REQUIRED GLU)
    # target_link_libraries(scheduler PRIVATE OpenGL::GLU)
endif()
```

---

## **Building**

1. **Create Build Directory**:
   ```bash
   cd /path/to/ProjectRoot
   mkdir build
   cd build
   ```
2. **Configure**:
    - **Without** OpenGL UI:
      ```bash
      cmake -DUSE_OPENGL_UI=OFF -DCMAKE_BUILD_TYPE=Release ..
      ```
    - **With** OpenGL UI (if you installed dev packages):
      ```bash
      cmake -DUSE_OPENGL_UI=ON -DCMAKE_BUILD_TYPE=Debug ..
      ```
3. **Compile**:
   ```bash
   cmake --build . --target scheduler
   ```
4. **Run**:
   ```bash
   ./scheduler
   ```
   You’ll see a menu with concurrency options, test progression, HPC overshadow toggles, ephemeral containers, etc.

### **Single-Line GCC Approach (Optional)**

If you **cannot** or **do not** want to use CMake, you can run a one-liner:

- **Without** OpenGL:
  ```bash
  gcc -o scheduler \
      src/scheduler.c src/os.c src/process.c src/ready.c src/worker.c src/safe_calls_library.c \
      test/basic-test.c test/normal-test.c test/modes-test.c test/edge-test.c test/hidden-test.c \
      -I./src -I./test -I./include \
      -lpthread
  ```

- **With** OpenGL** (assuming you have `libGL`, `libGLU`, `libglut`):
  ```bash
  gcc -o scheduler \
      -DUSE_OPENGL_UI \
      src/scheduler.c src/os.c src/process.c src/ready.c src/worker.c src/safe_calls_library.c \
      test/basic-test.c test/normal-test.c test/modes-test.c test/edge-test.c test/hidden-test.c \
      -I./src -I./test -I./include \
      -lpthread -lGL -lGLU -lglut
  ```

---

## **Usage**

Once compiled, run `./scheduler`. You’ll get a text-based menu:

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
```

**Choose**:

- **[1]**: Triggers the next test aggregator in the chain (BASIC → NORMAL → MODES → EDGE → HIDDEN). Each needs ≥60% success to unlock the next.
- **[2]**: OS-based concurrency UI: toggles HPC overshadow, ephemeral containers, pipeline, distribution, HPC threads, pipeline stages, etc. Then `[R]` to run concurrency.
- **[3]**: ReadyQueue concurrency UI: pick scheduling policy (FIFO, RR, BFS, Priority, SJF), HPC overshadow, pipeline, container flags, enqueue processes, run them.
- **[4]**: Single Worker concurrency approach in one shot. HPC overshadow, ephemeral container, pipeline synergy, distributed concurrency, debug logs.
- **[5]**: Show scoreboard for the 5 test suites, partial success in each.
- **[6]**: Re-run an old suite if you want a better success rate.
- **[7]** (only if `USE_OPENGL_UI`=ON): Launch a simple **OpenGL** UI that displays HPC threads, ephemeral container path, HPC result, pipeline stages, etc.

---

## **Directory Structure**

```
ProjectRoot/
├── CMakeLists.txt        # Top-level
├── cmake/
│   └── dependencies.cmake (optional)
├── build/                # Out-of-source build folder
├── src/
│   ├── CMakeLists.txt    # Builds "scheduler" target
│   ├── os.c, os.h
│   ├── process.c, process.h
│   ├── ready.c, ready.h
│   ├── worker.c, worker.h
│   ├── safe_calls_library.c, safe_calls_library.h
│   ├── scheduler.c, scheduler.h
├── test/
│   ├── basic-test.c, basic-test.h
│   ├── normal-test.c, normal-test.h
│   ├── modes-test.c, modes-test.h
│   ├── edge-test.c, edge-test.h
│   ├── hidden-test.c, hidden-test.h
└── ...
```

---

## **Game Progression**

- **Level 0**: BASIC tests (≥60% => unlock next)
- **Level 1**: NORMAL tests
- **Level 2**: MODES tests
- **Level 3**: EDGE tests
- **Level 4**: HIDDEN tests
- **Level 5**: All done => final scoreboard “GAME OVER” out of 500 synergy points.

You can re-run an old suite to improve that partial success. The aggregator only moves forward automatically if you pass the next suite in sequence.

---

## **References & Theory**

- [POSIX Threads Programming](https://computing.llnl.gov/tutorials/pthreads/)
- [Linux Syscalls: fork(), wait()](https://man7.org/linux/man-pages/man2/fork.2.html)
- [OpenGL / GLUT basics](https://www.khronos.org/opengl/wiki/)
- [FreeGLUT vs classic GLUT difference](http://freeglut.sourceforge.net/docs.html)

Happy concurrency explorations and game-like test progression!