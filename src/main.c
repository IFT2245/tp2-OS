#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef USE_OPENGL_UI
#include <GL/glut.h>
#endif
#include "runner.h"
#include "os.h"

#ifdef USE_OPENGL_UI
static int window_w = 800;
static int window_h = 600;

static void render_text(const char *text, float x, float y) {
    glRasterPos2f(x, y);
    while(*text) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *text);
        text++;
    }
}

static void display_cb(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f(1.0f, 1.0f, 1.0f);
    render_text("TP2-OS OpenGL Menu", -0.9f, 0.8f);
    render_text("1) Run All Unlocked Levels", -0.9f, 0.5f);
    render_text("2) Exit", -0.9f, 0.3f);
    glutSwapBuffers();
}

static void reshape_cb(int w, int h) {
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    window_w = w; window_h = h;
}

static void keyboard_cb(unsigned char key, int x, int y) {
    (void)x; (void)y;
    if(key=='1') { run_all_levels(); exit(0); }
    else if(key=='2' || key==27) { exit(0); }
}

static void init_opengl_menu(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(window_w, window_h);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("TP2-OS");
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glutDisplayFunc(display_cb);
    glutReshapeFunc(reshape_cb);
    glutKeyboardFunc(keyboard_cb);
    glutMainLoop();
}
#endif

static void menu_text_mode(void) {
    int choice = 0;
    printf("TP2-OS:\n1) Run All Unlocked Levels\n2) Exit\n> ");
    if(scanf("%d", &choice)==1) {
        if(choice==1) run_all_levels();
    }
}

int main(int argc, char** argv) {
    os_init();
#ifdef USE_OPENGL_UI
    init_opengl_menu(argc, argv);
#else
    menu_text_mode();
#endif
    os_cleanup();
    return 0;
}
