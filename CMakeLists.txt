cmake_minimum_required(VERSION 3.25)
project(Notiman LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Compiler flags
add_compile_options(/W4 /WX /permissive- /utf-8)

# Release optimization flags
add_compile_options($<$<CONFIG:Release>:/O2> $<$<CONFIG:Release>:/GL> $<$<CONFIG:Release>:/GS->)

# Release link flags
add_link_options($<$<CONFIG:Release>:/LTCG> $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)

# Third-party interface library
add_library(third_party INTERFACE)
target_include_directories(third_party INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

# Copy config to build output
add_custom_target(copy_config ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/config
        ${CMAKE_BINARY_DIR}/Release/config
    COMMENT "Copying config files to build output"
)

# Subdirectories
add_subdirectory(src/shared)
add_subdirectory(src/host)
add_subdirectory(src/cli)
