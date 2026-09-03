# Library targets grow per milestone. For M0 there is a single `la_app`
# static library holding shared code, linked by both the CLI binary and
# the test binary so tests exercise the same objects that ship.
add_library(la_app STATIC
    app/version.cpp
)
target_include_directories(la_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(la_app PUBLIC cxx_std_20)

add_executable(loganalyzer main.cpp)
target_link_libraries(loganalyzer PRIVATE la_app)
