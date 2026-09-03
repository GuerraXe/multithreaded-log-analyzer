add_library(la_aggregate STATIC
    histogram.cpp
    endpoint_stat.cpp
    time_buckets.cpp
    aggregate.cpp
    aggregator.cpp
)
target_include_directories(la_aggregate PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(la_aggregate PUBLIC cxx_std_20)
target_link_libraries(la_aggregate PUBLIC la_parse la_filter)
