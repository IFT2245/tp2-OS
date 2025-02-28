
add_executable(main
        ../lib/cJSON.c
        ../lib/cJSON.h
        library.c
        library.h
        log.c
        log.h
        ephemeral.c
        ephemeral.h
        scoreboard.c
        scoreboard.h
        concurrency.c
        concurrency.h
        tests.c
        tests.h
        main.c
)

target_link_libraries(main PRIVATE Threads::Threads)