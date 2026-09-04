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

add_library(codexbar-core STATIC)
target_sources(codexbar-core
    PRIVATE
        src/dashboard_controller.cpp
        src/dashboard_snapshot.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/codexbar/dashboard_controller.hpp
            include/codexbar/dashboard_snapshot.hpp
)
target_link_libraries(codexbar-core PUBLIC Qt6::Core Qt6::Qml)
target_compile_features(codexbar-core PUBLIC cxx_std_20)

if(CODEXBAR_BUILD_APPLET)
    add_subdirectory(applet)
endif()

if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
