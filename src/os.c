#include "os.h"
#include "safe_calls_library.h"

#ifdef USE_OPENGL_UI
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

/*==================================================
  Inline flag helpers
==================================================*/
static inline void os_set_flag(OSCore* c, os_flags_t f) { c->flags |= f; }
static inline void os_clear_flag(OSCore* c, os_flags_t f) { c->flags &= ~f; }
static inline bool os_has_flag(const OSCore* c, os_flags_t f) { return (c->flags & f) != 0; }

/*==================================================
  HPC concurrency
==================================================*/
typedef struct {
    long start;
    long end;
    long partial;
} HPCThreadArg;

static void* hpc_thread_func(void* arg) {
    HPCThreadArg* a = (HPCThreadArg*)arg;
    long sumv = 0;
    for (long i = a->start; i < a->end; i++) {
        sumv += i;
    }
    a->partial = sumv;
    return arg;
}

static void os_handle_hpc(OSCore* core) {
    if (!os_has_flag(core, OS_FLAG_MODE_HPC)) return;
    long N = 100;
    int threads = (core->hpc_threads < 1) ? 1 : core->hpc_threads;
    HPCThreadArg* arr = (HPCThreadArg*)calloc((size_t)threads, sizeof(HPCThreadArg));
    pthread_t* tids = (pthread_t*)calloc((size_t)threads, sizeof(pthread_t));
    if (!arr || !tids) {
        os_set_flag(core, OS_FLAG_ERROR);
        free(arr);
        free(tids);
        return;
    }
    long chunk = N / threads;
    long start = 1;
    for (int i = 0; i < threads; i++) {
        arr[i].start = start;
        if (i == threads - 1) arr[i].end = N + 1;
        else arr[i].end = start + chunk;
        start = arr[i].end;
        pthread_create(&tids[i], NULL, hpc_thread_func, &arr[i]);
    }
    long total = 0;
    for (int i = 0; i < threads; i++) {
        void* retp = NULL;
        pthread_join(tids[i], &retp);
        HPCThreadArg* rr = (HPCThreadArg*)retp;
        total += rr->partial;
    }
    core->hpc_result = total;
    free(arr);
    free(tids);
}

/*==================================================
  Container ephemeral creation
==================================================*/
static bool create_container_dir(char* dir) {
    if (!dir) return false;
    if (!strstr(dir, "XXXXXX")) {
        size_t len = strlen(dir);
        if (len + 6 < 256U) {
            strcat(dir, "XXXXXX");
        } else {
            snprintf(dir, 256U, "/tmp/os_container_XXXXXX");
        }
    }
    char* r = mkdtemp(dir);
    return (r != NULL);
}

static void os_handle_container(OSCore* core) {
    if (!os_has_flag(core, OS_FLAG_MODE_CONTAINER)) return;
    if (!create_container_dir(core->container_dir)) {
        os_set_flag(core, OS_FLAG_ERROR);
    }
}

/*==================================================
  Pipeline logic
==================================================*/
static void os_handle_pipeline(OSCore* core) {
    if (!os_has_flag(core, OS_FLAG_MODE_PIPELINE)) return;
    int st = (core->pipeline_stages < 1) ? 1 : core->pipeline_stages;
    for (int i = 0; i < st; i++) {
        sleep(1);
    }
}

/*==================================================
  Distributed processes
==================================================*/
static void os_handle_distributed(OSCore* core) {
    if (!os_has_flag(core, OS_FLAG_MODE_DISTRIBUTED)) return;
    int cnt = (core->distributed_count < 1) ? 1 : core->distributed_count;
    for (int i = 0; i < cnt; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            os_set_flag(core, OS_FLAG_ERROR);
            continue;
        } else if (pid == 0) {
            sleep(1);
            _exit(0);
        } else {
            waitpid(pid, NULL, 0);
        }
    }
}

/*==================================================
  Multi-process demonstration
==================================================*/
static void os_handle_multiprocess(OSCore* core) {
    if (!os_has_flag(core, OS_FLAG_MULTIPROCESS)) return;
    pid_t pid = fork();
    if (pid < 0) {
        os_set_flag(core, OS_FLAG_ERROR);
        return;
    } else if (pid == 0) {
        sleep(1);
        _exit(0);
    } else {
        waitpid(pid, NULL, 0);
    }
}

/*==================================================
  Multi-thread if HPC not overshadowing
==================================================*/
static void* basic_thread(void* arg) {
    (void)arg;
    sleep(1);
    return NULL;
}

static void os_handle_multithread(OSCore* core) {
    if (!os_has_flag(core, OS_FLAG_MULTITHREAD)) return;
    if (os_has_flag(core, OS_FLAG_MODE_HPC)) return;
    pthread_t tid;
    pthread_create(&tid, NULL, basic_thread, NULL);
    pthread_join(tid, NULL);
}

/*==================================================
  Game difficulty modes
==================================================*/
static void os_handle_game_modes(OSCore* core) {
    if (os_has_flag(core, OS_FLAG_MODE_GAME_EASY)) {
        core->game_difficulty = 1;
        core->game_score = 10;
    }
    if (os_has_flag(core, OS_FLAG_MODE_GAME_CHALLENGE)) {
        core->game_difficulty = 3;
        core->game_score = 30;
    }
    if (os_has_flag(core, OS_FLAG_MODE_GAME_SURVIVAL)) {
        core->game_difficulty = 5;
        core->game_score = 50;
    }
    if (os_has_flag(core, OS_FLAG_MODE_GAME_SANDBOX)) {
        core->game_difficulty = 0;
        core->game_score = 100;
    }
    if (os_has_flag(core, OS_FLAG_MODE_GAME_STORY)) {
        core->game_difficulty = 2;
        core->game_score = 20;
    }
}

/*==================================================
  Scheduling approach (stub)
==================================================*/
static void os_handle_scheduling(OSCore* core) {
    if (core->sched_quantum_ms <= 0) core->sched_quantum_ms = 100;
}

/*==================================================
  OS API
==================================================*/
OSCore* os_init(void) {
    OSCore* c = (OSCore*)calloc(1, sizeof(OSCore));
    if (!c) return NULL;
    os_set_flag(c, OS_FLAG_INITIALIZED);
    os_set_flag(c, OS_FLAG_COMPAT_BREAK);
    c->hpc_threads = 2;
    c->distributed_count = 2;
    snprintf(c->container_dir, sizeof(c->container_dir), "/tmp/os_container_XXXXXX");
    c->pipeline_stages = 3;
    c->hpc_result = 0;
    c->sched_quantum_ms = 100;
    c->game_score = 0;
    c->game_difficulty = 0;
    return c;
}

bool os_run(OSCore* c) {
    if (!c) return false;
    if (!os_has_flag(c, OS_FLAG_INITIALIZED)) return false;
    os_set_flag(c, OS_FLAG_RUNNING);

    os_handle_game_modes(c);
    os_handle_scheduling(c);
    os_handle_multithread(c);
    os_handle_multiprocess(c);
    os_handle_hpc(c);
    os_handle_container(c);
    os_handle_distributed(c);
    os_handle_pipeline(c);

    return !os_has_flag(c, OS_FLAG_ERROR);
}

bool os_shutdown(OSCore* c) {
    if (!c) return false;
    os_set_flag(c, OS_FLAG_SHUTDOWN);
    if (os_has_flag(c, OS_FLAG_MODE_CONTAINER)) {
        if (strstr(c->container_dir, "/tmp/os_container_")) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "rm -rf %s", c->container_dir);
            system(cmd);
        }
    }
    free(c);
    return true;
}

#ifdef USE_OPENGL_UI
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>

static OSCore* g_core_ref = NULL;
static int g_window_width = 640;
static int g_window_height = 480;

static void gl_render_text(float x, float y, const char* str) {
    glRasterPos2f(x, y);
    for (const char* c = str; *c; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
}

static void gl_display(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (g_core_ref) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "OS HPC threads: %d", g_core_ref->hpc_threads);
        gl_render_text(10.0f, (float)(g_window_height - 20), buffer);

        snprintf(buffer, sizeof(buffer), "Pipeline stages: %d", g_core_ref->pipeline_stages);
        gl_render_text(10.0f, (float)(g_window_height - 40), buffer);

        snprintf(buffer, sizeof(buffer), "Distributed count: %d", g_core_ref->distributed_count);
        gl_render_text(10.0f, (float)(g_window_height - 60), buffer);

        snprintf(buffer, sizeof(buffer), "Scheduling quantum ms: %d", g_core_ref->sched_quantum_ms);
        gl_render_text(10.0f, (float)(g_window_height - 80), buffer);

        snprintf(buffer, sizeof(buffer), "Container dir: %s", g_core_ref->container_dir);
        gl_render_text(10.0f, (float)(g_window_height - 100), buffer);

        snprintf(buffer, sizeof(buffer), "HPC result: %ld", g_core_ref->hpc_result);
        gl_render_text(10.0f, (float)(g_window_height - 120), buffer);

        snprintf(buffer, sizeof(buffer), "Game difficulty: %d, score: %d", g_core_ref->game_difficulty, g_core_ref->game_score);
        gl_render_text(10.0f, (float)(g_window_height - 140), buffer);
    }

    glutSwapBuffers();
}

static void gl_reshape(int w, int h) {
    g_window_width = w;
    g_window_height = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, (GLdouble)w, 0.0, (GLdouble)h);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void gl_keyboard(unsigned char key, int x, int y) {
    (void)x;
    (void)y;
    if (key == 27) {
        /* ESC: Some FreeGLUT versions have glutLeaveMainLoop().
           If it's not available, fallback to exit(0). */

#if defined(FREEGLUT) || defined(__FREEGLUT_H__)
        glutLeaveMainLoop();
#else
        /* If classical GLUT, no function => do a manual exit. */
        exit(0);
#endif
    }
    else if (key == 'r') {
        if (g_core_ref) {
            bool ok = os_run(g_core_ref);
            if (!ok) printf("os_run encountered an error.\n");
            else printf("os_run success => HPC result=%ld\n", g_core_ref->hpc_result);
        }
    }
}

static void gl_idle(void) {
    glutPostRedisplay();
}

static void opengl_ui_run(OSCore* core) {
    g_core_ref = core;
    int argc = 1;
    char* argv[1] = {(char*)"OpenGL-OS"};
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(g_window_width, g_window_height);
    glutInitWindowPosition(100,100);
    glutCreateWindow("OS System - Advanced UI");
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    glutDisplayFunc(gl_display);
    glutReshapeFunc(gl_reshape);
    glutKeyboardFunc(gl_keyboard);
    glutIdleFunc(gl_idle);
    glutMainLoop();
}
#endif

void os_ui_interact(OSCore* core) {
    if (!core) return;

#ifdef USE_OPENGL_UI
    /* If compiled with freeglut-based approach, run the gl.
       Otherwise fallback to text mode if not found. */
    opengl_ui_run(core);
#else
    while (true) {
        printf("\n-- OS UI (Text Mode) --\n");
        printf("[1] HPC threads (%d)\n", core->hpc_threads);
        printf("[2] Pipeline stages (%d)\n", core->pipeline_stages);
        printf("[3] Distributed count (%d)\n", core->distributed_count);
        printf("[4] Sched quantum ms (%d)\n", core->sched_quantum_ms);
        printf("[5] Toggle HPC (cur=%s)\n", os_has_flag(core, OS_FLAG_MODE_HPC)?"ON":"OFF");
        printf("[6] Toggle Container (cur=%s)\n", os_has_flag(core, OS_FLAG_MODE_CONTAINER)?"ON":"OFF");
        printf("[7] Toggle Pipeline (cur=%s)\n", os_has_flag(core, OS_FLAG_MODE_PIPELINE)?"ON":"OFF");
        printf("[8] Toggle Dist (cur=%s)\n", os_has_flag(core, OS_FLAG_MODE_DISTRIBUTED)?"ON":"OFF");
        printf("[9] Toggle MP (cur=%s)\n", os_has_flag(core, OS_FLAG_MULTIPROCESS)?"ON":"OFF");
        printf("[R] Run OS\n");
        printf("[0] Quit UI\n");
        printf("Choice: ");
        fflush(stdout);

        char input[64];
        if (!fgets(input, sizeof(input), stdin)) {
            printf("End of input.\n");
            break;
        }
        if (input[0] == '0') {
            printf("Exiting UI.\n");
            break;
        }
        switch(input[0]) {
            case '1': {
                printf("New HPC threads: ");
                fflush(stdout);
                if (fgets(input, sizeof(input), stdin)) {
                    int val = parse_int_strtol(input, core->hpc_threads);
                    if (val > 0) core->hpc_threads = val;
                }
                break;
            }
            case '2': {
                printf("New pipeline stages: ");
                fflush(stdout);
                if (fgets(input, sizeof(input), stdin)) {
                    int val = parse_int_strtol(input, core->pipeline_stages);
                    if (val > 0) core->pipeline_stages = val;
                }
                break;
            }
            case '3': {
                printf("New distributed count: ");
                fflush(stdout);
                if (fgets(input, sizeof(input), stdin)) {
                    int val = parse_int_strtol(input, core->distributed_count);
                    if (val >= 0) core->distributed_count = val;
                }
                break;
            }
            case '4': {
                printf("New scheduling quantum ms: ");
                fflush(stdout);
                if (fgets(input, sizeof(input), stdin)) {
                    int val = parse_int_strtol(input, core->sched_quantum_ms);
                    if (val > 0) core->sched_quantum_ms = val;
                }
                break;
            }
            case '5': {
                if (os_has_flag(core, OS_FLAG_MODE_HPC)) os_clear_flag(core, OS_FLAG_MODE_HPC);
                else os_set_flag(core, OS_FLAG_MODE_HPC);
                break;
            }
            case '6': {
                if (os_has_flag(core, OS_FLAG_MODE_CONTAINER)) os_clear_flag(core, OS_FLAG_MODE_CONTAINER);
                else os_set_flag(core, OS_FLAG_MODE_CONTAINER);
                break;
            }
            case '7': {
                if (os_has_flag(core, OS_FLAG_MODE_PIPELINE)) os_clear_flag(core, OS_FLAG_MODE_PIPELINE);
                else os_set_flag(core, OS_FLAG_MODE_PIPELINE);
                break;
            }
            case '8': {
                if (os_has_flag(core, OS_FLAG_MODE_DISTRIBUTED)) os_clear_flag(core, OS_FLAG_MODE_DISTRIBUTED);
                else os_set_flag(core, OS_FLAG_MODE_DISTRIBUTED);
                break;
            }
            case '9': {
                if (os_has_flag(core, OS_FLAG_MULTIPROCESS)) os_clear_flag(core, OS_FLAG_MULTIPROCESS);
                else os_set_flag(core, OS_FLAG_MULTIPROCESS);
                break;
            }
            case 'R':
            case 'r': {
                bool ok = os_run(core);
                if (ok) {
                    printf("os_run success => HPC result=%ld\n", core->hpc_result);
                } else {
                    printf("os_run failed => error.\n");
                }
                break;
            }
            default:
                printf("Unknown.\n");
                break;
        }
    }
#endif
}
