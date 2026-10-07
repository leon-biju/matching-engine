add_library(matchengine_core STATIC src/core.cpp)
add_library(matchengine::core ALIAS matchengine_core)
target_include_directories(matchengine_core PUBLIC include)
target_compile_features(matchengine_core PUBLIC cxx_std_23)
