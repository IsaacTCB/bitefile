cmake_minimum_required(VERSION 3.11)

project(bitefile LANGUAGES C)

option(BITEFILE_BUILD_EXAMPLES "Build example projects" ${PROJECT_IS_TOP_LEVEL})
option(BITEFILE_BUILD_SHARED "Build bitefile as a shared library" ${BUILD_SHARED_LIBS})

set(BITEFILE_TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools"
    CACHE PATH "Path to bitefile script tools"
)

set(BITEFILE_SOURCES src/bite.c)

if(NOT BUILD_SHARED_LIBS)
    add_library(bitefile STATIC ${BITEFILE_SOURCES})
    message("Building bitefile as a static library")
else()
    add_library(bitefile SHARED ${BITEFILE_SOURCES})
    message("Building bitefile as a shared library")
endif()

target_include_directories(
    bitefile
    PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if(BITEFILE_BUILD_EXAMPLES)
    message("Building examples")
    add_subdirectory("examples/")
endif()
