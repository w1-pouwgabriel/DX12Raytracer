cmake_minimum_required(VERSION 3.16)

# Force x64 platform BEFORE project()
set(CMAKE_GENERATOR_PLATFORM x64)

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

# Organize files by folder - FIXED PATHS!
file(GLOB APP_SOURCES "${SRC_DIR}/App/*.cpp")
file(GLOB APP_HEADERS "${SRC_DIR}/App/*.h")

# Changed from DX12Renderer to Renderer (match your folder structure!)
file(GLOB RENDERER_SOURCES "${SRC_DIR}/App/Renderer/*.cpp")
file(GLOB RENDERER_HEADERS "${SRC_DIR}/App/Renderer/*.h")

file(GLOB_RECURSE SHADER_HEADERS "${CMAKE_SOURCE_DIR}/assets/shaders/*.h")
file(GLOB_RECURSE SHADER_FILES "${CMAKE_SOURCE_DIR}/assets/shaders/*.hlsl")

set(MAIN_SOURCES 
    ${SRC_DIR}/main.cpp
)

# Group files for Visual Studio
source_group("App" FILES ${APP_SOURCES} ${APP_HEADERS})
source_group("Renderer" FILES ${RENDERER_SOURCES} ${RENDERER_HEADERS})
source_group("Source Files" FILES ${MAIN_SOURCES})
source_group("Shaders" FILES ${SHADER_FILES} ${SHADER_HEADERS})

# Create executable
add_executable(Raytracer 
    ${MAIN_SOURCES}
    ${APP_SOURCES}
    ${APP_HEADERS}
    ${RENDERER_SOURCES}
    ${RENDERER_HEADERS}
    ${SHADER_FILES}
    ${SHADER_HEADERS}
)

# Mark shaders as content only (don't compile as C++)
set_source_files_properties(${SHADER_FILES} PROPERTIES VS_TOOL_OVERRIDE "None")

# Set working directory for debugging
set_target_properties(Raytracer PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Raytracer>"
)

# Link libraries
target_link_libraries(Raytracer
    d3d12
    dxgi
    dxguid
    dxcompiler
    kernel32      # Core Windows API
    user32        # Window management
    gdi32         # Graphics
)

# Copy lib/ and assets/
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

if(NOT CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "10.0.22621.0")
endif()

set(WINSDK_ROOT "C:/Program Files (x86)/Windows Kits/10")
set(WINSDK_VER  "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")

target_include_directories(Raytracer PRIVATE
    "${WINSDK_ROOT}/Include/${WINSDK_VER}/um"
)

target_link_directories(Raytracer PRIVATE
    "${WINSDK_ROOT}/Lib/${WINSDK_VER}/um/x64"
)

# Copy DXC DLLs
foreach(DXC_DLL dxcompiler.dll dxil.dll)
    add_custom_command(TARGET Raytracer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WINSDK_ROOT}/bin/${WINSDK_VER}/x64/${DXC_DLL}"
            "$<TARGET_FILE_DIR:Raytracer>"
    )
endforeach()