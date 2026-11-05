cmake_minimum_required(VERSION 3.25)

project(gemapi-hello LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(gemapi CONFIG REQUIRED)

add_executable(hello main.cpp)
target_link_libraries(hello PRIVATE gemapi::gemapi)
