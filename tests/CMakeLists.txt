find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(matching_tests engine_correctness_test.cpp)
target_link_libraries(matching_tests PRIVATE matching::version1 GTest::gtest_main)

gtest_discover_tests(matching_tests)
