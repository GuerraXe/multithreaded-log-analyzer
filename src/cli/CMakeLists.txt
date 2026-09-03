add_library(la_cli STATIC
    parser.cpp
)
target_include_directories(la_cli PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(la_cli PUBLIC cxx_std_20)
target_link_libraries(la_cli PUBLIC la_parse la_filter)
