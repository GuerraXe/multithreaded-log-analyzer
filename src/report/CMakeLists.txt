add_library(la_report STATIC
    text_renderer.cpp
)
target_include_directories(la_report PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(la_report PUBLIC cxx_std_20)
target_link_libraries(la_report PUBLIC la_stats la_core)
