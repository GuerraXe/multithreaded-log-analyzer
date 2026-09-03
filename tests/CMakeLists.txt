# All test cases across the suite link into a single loganalyzer_tests binary
# that self-registers TEST_CASE blocks and runs them from one main(). CTest
# invokes that one binary; new test files are added to SOURCES below.
add_executable(loganalyzer_tests
    support/runner_main.cpp
    smoke_test.cpp
    io/mapped_file_tests.cpp
    io/chunking_tests.cpp
    parse/timestamp_tests.cpp
    parse/log_record_tests.cpp
    parse/pipe_format_tests.cpp
    filter/record_filter_tests.cpp
    cli/parser_tests.cpp
    aggregate/histogram_tests.cpp
    aggregate/endpoint_stat_tests.cpp
    aggregate/time_buckets_tests.cpp
    aggregate/aggregate_tests.cpp
    stats/report_tests.cpp
    report/text_renderer_tests.cpp
    report/json_renderer_tests.cpp
    app/analyze_tests.cpp
    integration/analyze_text_test.cpp
)
target_compile_features(loganalyzer_tests PRIVATE cxx_std_20)
target_include_directories(loganalyzer_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(loganalyzer_tests PRIVATE
    LA_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)
target_link_libraries(loganalyzer_tests PRIVATE la_app)

add_test(NAME loganalyzer_tests COMMAND loganalyzer_tests)
