
add_executable(main
        os.c
        ready_queue.c
        worker.c
        process.c
        scheduler.c
        runner.c
        scoreboard.c
        safe_calls_library.c
        ../test/basic-test.c
        ../test/normal-test.c
        ../test/modes-test.c
        ../test/edge-test.c
        ../test/hidden-test.c
        ../test/external-test.c
        ../test/test_common.c
        main.c
)

find_package(SQLite3)
if(SQLITE3_FOUND)
    include_directories(${SQLITE3_INCLUDE_DIRS})
    target_link_libraries(main PRIVATE Threads::Threads sqlite3)
else()
    message(FATAL_ERROR "SQLite3 not found")
endif()

