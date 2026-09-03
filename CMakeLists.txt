cmake_minimum_required(VERSION 3.21)

project(log_analyzer
    LANGUAGES CXX
    DESCRIPTION "Multithreaded log analyzer CLI"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit compile_commands.json for editor tooling (clangd, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    # /wd4127 silences "conditional expression is constant" from the
    # do { ... } while (0) idiom used by the CHECK() test macros.
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /EHsc /wd4127)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(LA_BUILD_TESTS "Build the unit/integration test suite" ON)

add_subdirectory(src)

if (LA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
