# All test cases across the suite link into a single loganalyzer_tests binary
# that self-registers TEST_CASE blocks and runs them from one main(). CTest
# invokes that one binary; new test files are added to SOURCES below.
add_executable(loganalyzer_tests
    support/runner_main.cpp
    smoke_test.cpp
    parse/timestamp_tests.cpp
    parse/log_record_tests.cpp
    parse/pipe_format_tests.cpp
    app/analyze_tests.cpp
)
target_compile_features(loganalyzer_tests PRIVATE cxx_std_20)
target_include_directories(loganalyzer_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(loganalyzer_tests PRIVATE la_app)

add_test(NAME loganalyzer_tests COMMAND loganalyzer_tests)
