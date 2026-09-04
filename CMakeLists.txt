cmake_minimum_required(VERSION 3.24)

project(
    codexbar-plasma
    VERSION 0.1.0
    DESCRIPTION "CodexBar usage widget for KDE Plasma 6"
    HOMEPAGE_URL "https://github.com/kyaulabs/codexbar-plasma"
    LANGUAGES CXX
)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(CODEXBAR_ENABLE_COVERAGE "Instrument C++ targets for coverage" OFF)
if(CODEXBAR_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "Coverage requires GCC or Clang")
    endif()
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

option(CODEXBAR_BUILD_APPLET "Build the KDE Plasma applet" ON)
if(CODEXBAR_BUILD_APPLET)
    set(KDE_INSTALL_USE_QT_SYS_PATHS ON CACHE BOOL "Install to the active Qt paths")
    find_package(ECM 6.0 NO_MODULE REQUIRED)
    list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
    include(KDEInstallDirs6)
    find_package(KF6 6.0 REQUIRED COMPONENTS Config CoreAddons)
    find_package(Plasma 6.0 REQUIRED)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Qml)

function(codexbar_enable_warnings target)
    target_compile_options(
        "${target}"
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror=return-type
    )
endfunction()

add_library(codexbar-core STATIC)
target_sources(codexbar-core
    PRIVATE
        src/dashboard_controller.cpp
        src/dashboard_snapshot.cpp
        src/presentation_formatter.cpp
        src/provider_selection_model.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/codexbar/dashboard_controller.hpp
            include/codexbar/dashboard_snapshot.hpp
            include/codexbar/presentation_formatter.hpp
            include/codexbar/provider_selection_model.hpp
)
target_link_libraries(codexbar-core PUBLIC Qt6::Core Qt6::Qml)
target_compile_features(codexbar-core PUBLIC cxx_std_20)
codexbar_enable_warnings(codexbar-core)

if(CODEXBAR_BUILD_APPLET)
    add_subdirectory(applet)
    install(FILES LICENSE README.md DESTINATION "${KDE_INSTALL_DATADIR}/doc/codexbar-plasma")
endif()

if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
