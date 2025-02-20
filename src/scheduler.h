#ifndef SCHEDULER_H
#define SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

    /*
     * Exposes the advanced "scheduler_main" entry point,
     * which:
     *  - Manages 5 test suites in a progression (basic->normal->edge->hidden->modes)
     *    each requiring >= 60% success to unlock the next
     *  - Offers OS-based, ReadyQueue-based, and single-worker concurrency approaches
     *  - Optionally uses OpenGL for OS-based UI if compiled with -DUSE_OPENGL_UI
     *
     * Returns an integer exit code, typically 0 for normal exit.
     */
    int scheduler_main(void);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_H */
