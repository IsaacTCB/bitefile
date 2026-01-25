cmake_minimum_required(VERSION 3.11)

project(bitefile LANGUAGES C)

option(BITEFILE_BUILD_EXAMPLES "Build example projects" ${PROJECT_IS_TOP_LEVEL})
option(BITEFILE_BUILD_SHARED "Build bitefile as a shared library"
    ${BUILD_SHARED_LIBS}
)

set(BITEFILE_TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools")

set(BITEFILE_SOURCES src/bite.c)

if(NOT BITEFILE_BUILD_SHARED)
    message("Building bitefile as a static library")
    add_library(bitefile STATIC ${BITEFILE_SOURCES})
else()
    message("Building bitefile as a shared library")
    add_library(bitefile SHARED ${BITEFILE_SOURCES})
    set_property(TARGET bitefile PROPERTY C_VISIBILITY_PRESET hidden)
    add_compile_definitions(TARGET bitefile
        PRIVATE $<BUILD_INTERFACE:BITEFILE_BUILD_LIB>
        INTERFACE $<BUILD_INTERFACE:BITEFILE_USE_SHARED>
    )
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
