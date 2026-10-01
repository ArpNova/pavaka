cmake_minimum_required(VERSION 4.4)

project(void LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(void src/main.cpp)
set_property(TARGET void PROPERTY CXX_SCAN_FOR_MODULES OFF)