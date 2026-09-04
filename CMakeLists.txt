cmake_minimum_required(VERSION 3.24)

project(
    kodometer
    VERSION 0.1.0
    DESCRIPTION "AI provider usage meter for KDE Plasma 6"
    HOMEPAGE_URL "https://github.com/kyaulabs/kodometer"
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

option(KODOMETER_ENABLE_COVERAGE "Instrument C++ targets for coverage" OFF)
if(KODOMETER_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "Coverage requires GCC or Clang")
    endif()
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

option(KODOMETER_BUILD_APPLET "Build the KDE Plasma applet" ON)
if(KODOMETER_BUILD_APPLET)
    set(KDE_INSTALL_USE_QT_SYS_PATHS ON CACHE BOOL "Install to the active Qt paths")
    find_package(ECM 6.0 NO_MODULE REQUIRED)
    list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
    include(KDEInstallDirs6)
    find_package(KF6 6.0 REQUIRED COMPONENTS Config CoreAddons)
    find_package(Plasma 6.0 REQUIRED)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Network Qml)

function(kodometer_enable_warnings target)
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

add_library(kodometer-core STATIC)
target_sources(kodometer-core
    PRIVATE
        src/codex_credentials.cpp
        src/codex_provider_adapter.cpp
        src/codex_usage_parser.cpp
        src/claude_credentials.cpp
        src/claude_provider_adapter.cpp
        src/claude_usage_parser.cpp
        src/gemini_credentials.cpp
        src/gemini_provider_adapter.cpp
        src/gemini_usage_parser.cpp
        src/xai_credentials.cpp
        src/xai_provider_adapter.cpp
        src/xai_usage_parser.cpp
        src/presentation_formatter.cpp
        src/provider_selection_model.cpp
        src/usage_controller.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/kodometer/codex_credentials.hpp
            include/kodometer/codex_provider_adapter.hpp
            include/kodometer/codex_usage_parser.hpp
            include/kodometer/claude_credentials.hpp
            include/kodometer/claude_provider_adapter.hpp
            include/kodometer/claude_usage_parser.hpp
            include/kodometer/gemini_credentials.hpp
            include/kodometer/gemini_provider_adapter.hpp
            include/kodometer/gemini_usage_parser.hpp
            include/kodometer/xai_credentials.hpp
            include/kodometer/xai_provider_adapter.hpp
            include/kodometer/xai_usage_parser.hpp
            include/kodometer/presentation_formatter.hpp
            include/kodometer/provider_adapter.hpp
            include/kodometer/provider_selection_model.hpp
            include/kodometer/usage_controller.hpp
)
target_link_libraries(kodometer-core PUBLIC Qt6::Core Qt6::Network Qt6::Qml)
target_compile_features(kodometer-core PUBLIC cxx_std_20)
kodometer_enable_warnings(kodometer-core)

if(KODOMETER_BUILD_APPLET)
    add_subdirectory(applet)
    install(FILES LICENSE README.md DESTINATION "${KDE_INSTALL_DATADIR}/doc/kodometer")
endif()

if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
