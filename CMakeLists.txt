cmake_minimum_required(VERSION 3.20)

project(micula VERSION 0.1.0 DESCRIPTION "A small Fluent control set for plain Win32" LANGUAGES CXX)

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
# MSVC gets these from window.h's #pragma comment lines as well; other toolchains
# need them spelled out.
target_link_libraries(micula INTERFACE
    d2d1 d3d11 dxgi dcomp dwrite dwmapi imm32 windowscodecs user32 gdi32 ole32 advapi32)

option(MICULA_BUILD_EXAMPLES "Build the examples" ${PROJECT_IS_TOP_LEVEL})
if(MICULA_BUILD_EXAMPLES)
    foreach(example gallery settings)
        add_executable(micula-${example} WIN32 examples/${example}/${example}.cpp)
        target_link_libraries(micula-${example} PRIVATE micula::micula)
        target_compile_definitions(micula-${example} PRIVATE UNICODE _UNICODE)
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
