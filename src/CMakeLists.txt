add_subdirectory(parse)
add_subdirectory(filter)
add_subdirectory(cli)

# `la_app` holds cross-cutting application code (versioning, the command
# dispatcher, the analyze driver). It is linked by both the CLI binary and
# the test binary so tests exercise the same objects that ship. Library
# targets grow per milestone.
add_library(la_app STATIC
    app/version.cpp
    app/analyze.cpp
    app/run.cpp
)
target_include_directories(la_app PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(la_app PUBLIC cxx_std_20)
target_link_libraries(la_app PUBLIC la_parse la_filter la_cli)

add_executable(loganalyzer main.cpp)
target_link_libraries(loganalyzer PRIVATE la_app)
