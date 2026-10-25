cmake_minimum_required(VERSION 4.4)

project(pavaka LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(pavakac src/main.cpp)
set_property(TARGET pavakac PROPERTY CXX_SCAN_FOR_MODULES OFF)

add_custom_command(
    TARGET pavakac POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE:pavakac> ${CMAKE_SOURCE_DIR}/pavakac
)