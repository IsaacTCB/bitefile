## SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.11)

project(bitefile VERSION 1.0.0 LANGUAGES C)

# Options
option(BITEFILE_BUILD_EXAMPLES "Build example projects" ${PROJECT_IS_TOP_LEVEL})
option(BITEFILE_BUILD_SHARED "Build bitefile as a shared library" ${BUILD_SHARED_LIBS})
option(BITEFILE_LARGE_FILES "Enable reading files larger than 2 GiB" ON)

# Building
set(BITEFILE_SOURCES src/bite.c)

if(NOT BITEFILE_BUILD_SHARED)
    message(STATUS "Building bitefile as a static library")
    add_library(bitefile STATIC ${BITEFILE_SOURCES})
else()
    message(STATUS "Building bitefile as a shared library")
    add_library(bitefile SHARED ${BITEFILE_SOURCES})
    set_target_properties(bitefile PROPERTY C_VISIBILITY_PRESET hidden)
    target_compile_definitions(bitefile
        PRIVATE $<BUILD_INTERFACE:BITEFILE_BUILD_LIB>
        INTERFACE $<BUILD_INTERFACE:BITEFILE_USE_SHARED>
    )
endif()

if(BITEFILE_LARGE_FILES)
    message(STATUS "Building bitefile with large file support")
    target_compile_definitions(bitefile
        PRIVATE BITEFILE_LARGE_FILES
    )
else()
    message(STATUS "Building bitefile WITHOUT large file support.")
    message(NOTICE "The library won't be able to open bite files larger than 2 GiB!")
endif()

target_include_directories(
    bitefile
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Contains all necessary install instructions
include(cmake/InstallInstructions.cmake)

if(BITEFILE_BUILD_EXAMPLES)
    # Path to python script tools, used to autogenerate 'data.bite' file
    set(BITEFILE_TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools")
    add_subdirectory("examples/")
endif()
