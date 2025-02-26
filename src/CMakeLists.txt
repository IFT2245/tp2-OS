
add_executable(main
        os.c
        os.h
        ready_queue.c
        ready_queue.h
        worker.c
        worker.h
        process.c
        process.h
        scheduler.c
        scheduler.h
        runner.c
        runner.h
        safe_calls_library.c
        ../test/basic-test.c
        ../test/normal-test.c
        ../test/modes-test.c
        ../test/edge-test.c
        ../test/hidden-test.c
        ../test/external-test.c
        ../test/test_common.c
        stats.h
        stats.c
        main.c
        main.h
        scoreboard.c
        scoreboard.h
        menu.c
        menu.h
)

target_link_libraries(main PRIVATE Threads::Threads m)