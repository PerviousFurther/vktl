cmake_minimum_required(VERSION 3.16)
project(vktl LANGUAGES CXX C)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

add_library(vktl INTERFACE)
target_include_directories(vktl INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries(vktl INTERFACE Vulkan::Vulkan)

file(GLOB_RECURSE VKTL_HEADERS CONFIGURE_DEPENDS "inc/*.hpp")
target_sources(vktl INTERFACE ${VKTL_HEADERS})

option(VKTL_ENABLE_COMPILER "Enable VKTL compiler utils." ON)
if (VKTL_ENABLE_COMPILER)
	find_package(Vulkan REQUIRED COMPONENTS glslang)
	find_library(SPIRV_TOOLS_OPT_RELEASE NAMES SPIRV-Tools-opt HINTS "$ENV{VULKAN_SDK}/Lib" REQUIRED)
	find_library(SPIRV_TOOLS_OPT_DEBUG NAMES SPIRV-Tools-optd HINTS "$ENV{VULKAN_SDK}/Lib" REQUIRED)
	find_library(SPIRV_TOOLS_RELEASE NAMES SPIRV-Tools HINTS "$ENV{VULKAN_SDK}/Lib" REQUIRED)
	find_library(SPIRV_TOOLS_DEBUG NAMES SPIRV-Toolsd HINTS "$ENV{VULKAN_SDK}/Lib" REQUIRED)
    set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
    set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)
    add_subdirectory(external/SPIRV-Reflect)

	target_link_libraries(vktl INTERFACE
		spirv-reflect-static
		Vulkan::glslang
		$<$<CONFIG:Debug>:${SPIRV_TOOLS_OPT_DEBUG}>
		$<$<NOT:$<CONFIG:Debug>>:${SPIRV_TOOLS_OPT_RELEASE}>
		$<$<CONFIG:Debug>:${SPIRV_TOOLS_DEBUG}>
		$<$<NOT:$<CONFIG:Debug>>:${SPIRV_TOOLS_RELEASE}>
	)
else()
    target_compile_definitions(vktl INTERFACE VKTL_NO_COMPILER)
endif()

if(BUILD_TESTING)
    add_executable(execution_plan_tests test/execution_plan.cpp)
    target_link_libraries(execution_plan_tests PRIVATE vktl)
    add_test(NAME execution_plan_tests COMMAND execution_plan_tests)
endif()

option(BUILD_EXAMPLES "Build vktl example projects" ON)
if(BUILD_EXAMPLES)
    option(BUILD_HELLO_WORLD_EXAMPLES "Build vktl example projects" ON)
    if(BUILD_HELLO_WORLD_EXAMPLES)
        add_subdirectory(example/hello_world)
    endif()
endif()
