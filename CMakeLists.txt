cmake_minimum_required(VERSION 3.22)
project(LabHUDEditorCore LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(LAB_ENGINE_DIR "$ENV{LAB_ENGINE_DIR}")
if(NOT LAB_ENGINE_DIR)
    get_filename_component(LAB_ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    if(NOT EXISTS "${LAB_ENGINE_DIR}/include")
        set(LAB_ENGINE_DIR "C:/Users/jancz/Desktop/Lab")
    endif()
endif()

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${LAB_ENGINE_DIR}/include"
    "${LAB_ENGINE_DIR}/src/glad/include"
    "${LAB_ENGINE_DIR}/external/lua"
    "${LAB_ENGINE_DIR}/build/_deps/glfw-src/include"
)

link_directories(
    "${LAB_ENGINE_DIR}/build/Release"
    "${LAB_ENGINE_DIR}/build/Debug"
    "${LAB_ENGINE_DIR}/build/_deps/glfw-build/src/Release"
    "${LAB_ENGINE_DIR}/build/_deps/glfw-build/src/Debug"
    "${LAB_ENGINE_DIR}/bin"
)

add_library(LabHUDEditorCore SHARED
    src/LabHUDEditor.cpp
    include/LabHUDEditor.h
)

target_compile_definitions(LabHUDEditorCore PRIVATE GLAD_API_CALL_EXPORT)

if(MSVC)
    target_compile_options(LabHUDEditorCore PRIVATE /W4 /utf-8 /permissive-)
endif()

target_link_libraries(LabHUDEditorCore PUBLIC LabCore LabRender LabAnimation LabWorld glfw3)
