add_library(la_core STATIC
    version.cpp
)
target_include_directories(la_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(la_core PUBLIC cxx_std_20)
