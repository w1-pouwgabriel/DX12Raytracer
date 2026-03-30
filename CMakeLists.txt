cmake_minimum_required(VERSION 3.16)

# Guard against in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not allowed.\n"
        "Please build from a separate directory:\n"
        "  mkdir build && cd build && cmake ..")
endif()

project(Raytracer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SRC_DIR     ${CMAKE_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(LIB_DIR     ${CMAKE_SOURCE_DIR}/lib)

include_directories(${SRC_DIR})
include_directories(${INCLUDE_DIR})

file(GLOB_RECURSE ALL_SRC_FILES    ${SRC_DIR}/*.cpp)
file(GLOB_RECURSE ALL_HEADER_FILES ${SRC_DIR}/*.h)

add_executable(Raytracer ${ALL_SRC_FILES} ${ALL_HEADER_FILES})

target_link_libraries(Raytracer
    d3d12
    dxgi
    dxguid
    dxcompiler
    gdi32
    user32
    kernel32
)

# Copy lib/ and assets/ next to the .exe (Debug/ or Release/ under build/)
add_custom_command(TARGET Raytracer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIB_DIR}
        $<TARGET_FILE_DIR:Raytracer>
)

add_custom_command(TARGET Raytracer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:Raytracer>/assets
)