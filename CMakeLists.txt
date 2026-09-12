cmake_minimum_required(VERSION 3.21)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PROJECT_VERSION_STRING LIMIT_COUNT 1)
string(STRIP "${PROJECT_VERSION_STRING}" PROJECT_VERSION_STRING)

project(
    DisableRedundantKeys
    VERSION ${PROJECT_VERSION_STRING}
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CommonLibSSE-NG (alandtse fork, SE/AE/VR multi-targeting with 1.7.x support),
# consumed from source per its test_consumer pattern. The vcpkg manifest
# provides CommonLibSSE's own dependencies (spdlog, rapidcsv, directxtk, ...).
include(FetchContent)
FetchContent_Declare(
    CommonLibSSE
    GIT_REPOSITORY https://github.com/alandtse/CommonLibSSE-NG.git
    GIT_TAG v7.5.4
    GIT_SHALLOW TRUE
)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(CommonLibSSE)

# The add_commonlibsse_plugin() helper is only included by CommonLibSSE's own
# CMakeLists on its prebuilt path; when the prebuilt bundle is rejected (e.g.
# MSVC toolset mismatch) and the library falls back to a source build, define
# it here explicitly.
include("${commonlibsse_SOURCE_DIR}/cmake/CommonLibSSE.cmake")

find_package(simpleini CONFIG REQUIRED)

add_commonlibsse_plugin(${PROJECT_NAME}
    SOURCES
        src/plugin.cpp
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        SimpleIni::SimpleIni
)

target_precompile_headers(${PROJECT_NAME}
    PRIVATE
        src/PCH.h
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        _UNICODE
        UNICODE
        NOMINMAX
        DISABLE_REDUNDANT_KEYS_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /permissive- /W4 /utf-8)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "Disable Redundant keys"
)
