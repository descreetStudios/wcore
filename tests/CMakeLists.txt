foreach(name test_test test_assert test_logger 
        test_math test_clock test_profiler test_handle
        test_event test_arena test_pool test_free_list
        test_job_system test_io test_binary test_json
        test_uuid)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE wcore)
    add_test(NAME wcore.${name} COMMAND ${name})
endforeach()