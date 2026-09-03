add_library(la_io STATIC
    mapped_file.cpp
    chunking.cpp
)
target_include_directories(la_io PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(la_io PUBLIC cxx_std_20)
