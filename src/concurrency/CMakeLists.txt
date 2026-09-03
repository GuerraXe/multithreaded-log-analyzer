add_library(la_concurrency STATIC
    parallel_aggregate.cpp
)
target_include_directories(la_concurrency PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(la_concurrency PUBLIC cxx_std_20)
target_link_libraries(la_concurrency PUBLIC la_io la_aggregate la_filter la_parse)

find_package(Threads REQUIRED)
target_link_libraries(la_concurrency PUBLIC Threads::Threads)
