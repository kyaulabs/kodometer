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

find_package(Qt6 6.4 REQUIRED COMPONENTS Core)

add_library(codexbar-core STATIC)
target_sources(codexbar-core
    PRIVATE
        src/dashboard_snapshot.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/codexbar/dashboard_snapshot.hpp
)
target_link_libraries(codexbar-core PUBLIC Qt6::Core)
target_compile_features(codexbar-core PUBLIC cxx_std_20)

if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
