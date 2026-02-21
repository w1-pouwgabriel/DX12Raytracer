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
set(RENDERER_DIR ${CMAKE_SOURCE_DIR}/src/Renderer)
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib)

# Include directories
include_directories(${INCLUDE_DIR})
include_directories(${RENDERER_DIR}/Public)  # Add public headers

# Add GLFW include directory
include_directories(${INCLUDE_DIR}/GLFW)

# Gather source files from main src and Renderer
file(GLOB MAIN_SRC_FILES ${SRC_DIR}/*.cpp)
file(GLOB RENDERER_SRC_FILES 
    ${RENDERER_DIR}/Private/*.cpp
    ${RENDERER_DIR}/Public/*.cpp
)

# Gather header files (so they appear in IDE)
file(GLOB RENDERER_HEADER_FILES 
    ${RENDERER_DIR}/Public/*.h
    ${RENDERER_DIR}/Private/*.h
)

# Combine all source files
set(ALL_SRC_FILES ${MAIN_SRC_FILES} ${RENDERER_SRC_FILES} ${RENDERER_HEADER_FILES})

# Add the executable
add_executable(Raytracer ${ALL_SRC_FILES})

# Link libraries
target_link_libraries(Raytracer 
    ${LIB_DIR}/libglfw3.a
    d3d12
    dxgi
    gdi32
    user32
    kernel32
)

# Set compiler explicitly (if needed)
if(MINGW)
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_CXX_COMPILER g++)
endif()

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