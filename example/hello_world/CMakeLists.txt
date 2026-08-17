cmake_minimum_required(VERSION 3.16)
project(hello_world LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    find_package(vktl REQUIRED)
endif()

add_executable(hello_world main.cpp window.cpp)
target_link_libraries(hello_world PRIVATE vktl)

# target_link_libraries(hello_world PRIVATE
#     spirv-reflect
# )