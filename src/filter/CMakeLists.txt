add_library(la_filter STATIC
    record_filter.cpp
)
target_include_directories(la_filter PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(la_filter PUBLIC cxx_std_20)
target_link_libraries(la_filter PUBLIC la_parse)
