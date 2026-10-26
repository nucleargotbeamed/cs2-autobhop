cmake_minimum_required(VERSION 3.20)

set(CMAKE_TOOLCHAIN_FILE "" CACHE STRING "" FORCE)
set(VCPKG_MANIFEST_MODE OFF CACHE BOOL "" FORCE)

project(bhopdll LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "build this as x64")
endif()

set(imgui ${CMAKE_SOURCE_DIR}/external/imgui)
set(minhook ${CMAKE_SOURCE_DIR}/external/minhook)

file(GLOB_RECURSE minhooksources ${minhook}/src/*.c)

add_library(bhop SHARED
    src/main.cpp
    src/hooks.cpp
    src/menu.cpp
    src/bhop.cpp
    src/subtick.cpp
    src/trace.cpp
    ${imgui}/imgui.cpp
    ${imgui}/imgui_draw.cpp
    ${imgui}/imgui_tables.cpp
    ${imgui}/imgui_widgets.cpp
    ${imgui}/backends/imgui_impl_dx11.cpp
    ${imgui}/backends/imgui_impl_win32.cpp
    ${minhooksources}
)

target_include_directories(bhop PRIVATE src ${imgui} ${imgui}/backends ${minhook}/include)
target_compile_definitions(bhop PRIVATE NOMINMAX)
target_link_libraries(bhop PRIVATE d3d11 dxgi)

set_target_properties(bhop PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/dlls)
