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

include(GNUInstallDirs)

target_include_directories(
    bitefile
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(
    TARGETS bitefile
    EXPORT bitefile
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT bitefile
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bitefile
    NAMESPACE bitefile::
)

if(BITEFILE_BUILD_EXAMPLES)
    message("Building examples")
    add_subdirectory("examples/")
endif()
