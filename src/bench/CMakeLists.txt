add_library(la_bench STATIC
    benchmark.cpp
)
target_include_directories(la_bench PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(la_bench PUBLIC cxx_std_20)
target_link_libraries(la_bench PUBLIC la_concurrency la_aggregate la_core la_filter la_parse)
