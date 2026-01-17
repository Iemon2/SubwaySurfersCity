cmake_minimum_required(VERSION 3.12)
project(SubwaySufersCity VERSION 0.1.0)

add_executable(SubwaySufersCity engine/src/main.c)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(glfw)

find_package(Vulkan REQUIRED)
target_link_libraries(SubwaySufersCity PRIVATE glfw Vulkan::Vulkan)
