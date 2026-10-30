add_library(matching_version1 engine.cpp order_book.cpp)
add_library(matching::version1 ALIAS matching_version1)

target_link_libraries(matching_version1 PUBLIC matching::core)
