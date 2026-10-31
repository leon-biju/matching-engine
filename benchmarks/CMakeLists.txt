add_executable(make_workload make_workload.cpp)
target_link_libraries(make_workload PRIVATE matching::core)

add_executable(engine_benchmark engine_benchmark.cpp)
target_link_libraries(engine_benchmark PRIVATE matching::version1)

# A smoke test that the tools run end to end. It never checks timings.
if(BUILD_TESTING)
    add_test(NAME benchmark_smoke_make_workload
             COMMAND make_workload realistic 1 10000 ${CMAKE_CURRENT_BINARY_DIR}/smoke.workload)
    set_tests_properties(benchmark_smoke_make_workload PROPERTIES FIXTURES_SETUP smoke_workload)

    add_test(NAME benchmark_smoke_run
             COMMAND engine_benchmark ${CMAKE_CURRENT_BINARY_DIR}/smoke.workload 1)
    set_tests_properties(benchmark_smoke_run PROPERTIES FIXTURES_REQUIRED smoke_workload)
endif()
