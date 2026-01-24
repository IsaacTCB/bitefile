cmake_minimum_required(VERSION 3.11)

project(bitefile LANGUAGES C)

option(BUILD_EXAMPLES "Build example projects" ${PROJECT_IS_TOP_LEVEL})
option(BUILD_SHARED_LIBS "Build library as shared" OFF)

set(BITE_TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/")

add_library(bitefile src/bite.c)

if(NOT BUILD_SHARED_LIBS)
    message("Building bitefile as a static library")
    add_library(bitefile_static ALIAS bitefile)
else()
    message("Building bitefile as a shared library")
endif()

target_include_directories(bitefile
                            PUBLIC
                            $<INSTALL_INTERFACE:include>
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            )

if(BUILD_EXAMPLES)
    message("Building examples")
    add_subdirectory("examples/")
endif()
