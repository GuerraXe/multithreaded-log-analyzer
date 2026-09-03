add_library(la_parse STATIC
    timestamp.cpp
    log_record.cpp
    log_format.cpp
    pipe_format.cpp
    formats.cpp
)
target_include_directories(la_parse PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(la_parse PUBLIC cxx_std_20)
