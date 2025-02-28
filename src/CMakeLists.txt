
add_executable(main
        ../lib/cJSON.c
        ../lib/cJSON.h
        library.c
        library.h
        log.c
        log.h
        scoreboard.c
        scoreboard.h
        tests.c
        tests.h
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