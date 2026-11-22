cmake_minimum_required(VERSION 3.20)
project(chinese_native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
    godot-cpp
    GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
    GIT_TAG godot-4.5-stable
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(godot-cpp)

add_library(chinese_native SHARED
    src/hello_extension.cpp
    src/register_types.cpp
)
target_link_libraries(chinese_native PRIVATE godot-cpp)
target_include_directories(chinese_native PRIVATE src)


set_target_properties(chinese_native PROPERTIES
    OUTPUT_NAME "chinese_native"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output"
)
