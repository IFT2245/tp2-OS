
add_executable(main
        ../libExtern/cJSON.c
        ../libExtern/cJSON.h
        ../lib/library.c
        ../lib/library.h
        ../lib/log.c
        ../lib/log.h
        ../lib/scoreboard.c
        ../lib/scoreboard.h
        ../test/basic-tests.c
        ../test/basic-tests.h
        container.c
        container.h
        process.c
        process.h
        ready_queue.c
        ready_queue.h
        scheduler.c
        scheduler.h
        worker.c
        worker.h
        main.c
)

target_link_libraries(main PRIVATE Threads::Threads)