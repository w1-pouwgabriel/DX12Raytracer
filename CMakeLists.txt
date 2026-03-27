# Minimum CMake version
cmake_minimum_required(VERSION 3.16)

# Project name and version
project(Raytracer VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Specify project directories
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib)

# Include directories
include_directories(${SRC_DIR})  # Changed from INCLUDE_DIR to SRC_DIR
include_directories(${INCLUDE_DIR})

# Gather ALL source files recursively (THIS IS THE FIX!)
file(GLOB_RECURSE ALL_SRC_FILES 
    ${SRC_DIR}/*.cpp
)

# Gather ALL header files recursively (for IDE)
file(GLOB_RECURSE ALL_HEADER_FILES 
    ${SRC_DIR}/*.h
)

# Add the executable
add_executable(Raytracer ${ALL_SRC_FILES} ${ALL_HEADER_FILES})

# Link libraries
target_link_libraries(Raytracer 
    d3d12
    dxgi
    dxguid
    gdi32
    user32
    kernel32
)

# Copy all files from lib directory to the build directory
add_custom_command(TARGET Raytracer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_BINARY_DIR}
)

# Copy all files from assets directory to the build directory
add_custom_command(TARGET Raytracer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    ${CMAKE_BINARY_DIR}/assets
)