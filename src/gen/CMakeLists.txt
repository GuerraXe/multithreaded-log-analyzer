add_library(la_gen STATIC
    generator.cpp
)
target_include_directories(la_gen PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(la_gen PUBLIC cxx_std_20)
target_link_libraries(la_gen PUBLIC la_parse)
