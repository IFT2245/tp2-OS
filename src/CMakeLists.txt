
add_executable(main
        os.c
        ready_queue.c
        worker.c
        process.c
        scheduler.c
        safe_calls_library.c
        ../test/basic-test.c
        ../test/normal-test.c
        ../test/modes-test.c
        ../test/edge-test.c
        ../test/hidden-test.c
        main.c
)

target_link_libraries(main PRIVATE Threads::Threads)