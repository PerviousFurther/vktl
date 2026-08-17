cmake_minimum_required(VERSION 3.16)
project(hello_world LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    find_package(vktl REQUIRED)
endif()

add_executable(hello_world main.cpp window.cpp)
target_link_libraries(hello_world PRIVATE vktl)

foreach(shader hello_world.vert hello_world.frag)
	add_custom_command(TARGET hello_world POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${CMAKE_CURRENT_SOURCE_DIR}/${shader}
			$<TARGET_FILE_DIR:hello_world>/${shader}
	)
endforeach()

# target_link_libraries(hello_world PRIVATE
#     spirv-reflect
# )
