
add_executable(main
        os.c
        ready_queue.c
        worker.c
        process.c
        scheduler.c
        runner.c
        safe_calls_library.c
        ../test/basic-test.c
        ../test/normal-test.c
        ../test/modes-test.c
        ../test/edge-test.c
        ../test/hidden-test.c
        ../test/external-test.c
        ../test/test_common.c
        main.c
        scoreboard.c
        scoreboard.h
)

target_link_libraries(main PRIVATE Threads::Threads sqlite3)