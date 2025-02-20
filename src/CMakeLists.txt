
if(USE_OPENGL_UI)
    find_package(OpenGL REQUIRED)
    find_package(GLUT REQUIRED)
    add_compile_definitions(USE_OPENGL_UI)
endif()

add_executable(scheduler
    os.h
    os.c
    process.h
    process.c
    ready.h
    ready.c
    worker.h
    worker.c
    safe_calls_library.h
    safe_calls_library.c
    ../test/basic-test.c
    ../test/basic-test.h
    ../test/normal-test.c
    ../test/normal-test.h
    ../test/edge-test.c
    ../test/edge-test.h
    ../test/hidden-test.c
    ../test/hidden-test.h
    ../test/modes-test.c
    ../test/modes-test.h
    scheduler.h
    scheduler.c
)

target_link_libraries(scheduler PRIVATE Threads::Threads OpenGL::GL GLUT::GLUT)

