cmake_minimum_required(VERSION 3.20)

project(micula
    VERSION 0.1.0
    DESCRIPTION "A small Fluent control set for plain Win32"
    LANGUAGES CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "Micula is a Windows library: it draws with Direct2D over DirectComposition.")
endif()

# Header-only. The target carries the include path, the language level and the system
# libraries; there is nothing to compile until a program includes it.
add_library(micula INTERFACE)
add_library(micula::micula ALIAS micula)

target_include_directories(micula INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(micula INTERFACE cxx_std_17)

# Ensure library and its consumers use UTF-8.
target_compile_definitions(micula INTERFACE UNICODE _UNICODE)

# Prevent non-msvc toolchains that contains weird WINVER values on newer platforms.
target_compile_definitions(micula INTERFACE WINVER=0x0A00 _WIN32_WINNT=0x0A00)

# For non-msvc toolchains that don't work with `#pragma comment(lib)`
target_link_libraries(micula INTERFACE
    d2d1 d3d11 dxgi dcomp dwrite dwmapi imm32 windowscodecs user32 gdi32 ole32 advapi32)

option(MICULA_BUILD_EXAMPLES "Build the examples" ${PROJECT_IS_TOP_LEVEL})
if(MICULA_BUILD_EXAMPLES)
    foreach(example gallery settings nav)
        add_executable(micula-${example} WIN32 examples/${example}/${example}.cpp)
        target_link_libraries(micula-${example} PRIVATE micula::micula)
        # The entry point is wWinMain, which MSVC's linker finds on its own from the
        # subsystem and MinGW's does not: without -municode the startup code looks for
        # WinMain and the link fails on the one symbol the program never mentions.
        if(MINGW)
            target_link_options(micula-${example} PRIVATE -municode)
        endif()
        if(MSVC)
            target_compile_options(micula-${example} PRIVATE /W4 /permissive-)
        endif()
    endforeach()
endif()

include(GNUInstallDirs)
install(DIRECTORY include/micula DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS micula EXPORT micula-targets)
install(EXPORT micula-targets FILE micula-config.cmake NAMESPACE micula::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/micula)
