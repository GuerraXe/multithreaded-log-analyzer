add_library(la_stats STATIC
    report.cpp
)
target_include_directories(la_stats PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(la_stats PUBLIC cxx_std_20)
target_link_libraries(la_stats PUBLIC la_aggregate)
