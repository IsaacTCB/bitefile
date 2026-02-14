## SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.11)

project(bitefile VERSION 1.0.0 LANGUAGES C)

option(BITEFILE_BUILD_EXAMPLES "Build example projects" ${PROJECT_IS_TOP_LEVEL})
option(BITEFILE_BUILD_SHARED "Build bitefile as a shared library" ${BUILD_SHARED_LIBS})
option(BITEFILE_LARGE_FILES "Enable reading files larger than 2GiB" ON)

set(BITEFILE_TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools")

set(BITEFILE_SOURCES src/bite.c)

if(NOT BITEFILE_BUILD_SHARED)
    message("Building bitefile as a static library")
    add_library(bitefile STATIC ${BITEFILE_SOURCES})
else()
    message("Building bitefile as a shared library")
    add_library(bitefile SHARED ${BITEFILE_SOURCES})
    set_target_properties(bitefile PROPERTY C_VISIBILITY_PRESET hidden)
    target_compile_definitions(bitefile
        PRIVATE $<BUILD_INTERFACE:BITEFILE_BUILD_LIB>
        INTERFACE $<BUILD_INTERFACE:BITEFILE_USE_SHARED>
    )
endif()

if(BITEFILE_LARGE_FILES)
    target_compile_definitions(bitefile
        PRIVATE BITEFILE_LARGE_FILES
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
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bitefile-${PROJECT_VERSION}
    NAMESPACE bitefile::
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bitefile-${PROJECT_VERSION}
)

write_basic_package_version_file(
    "${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION},
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bitefile-${PROJECT_VERSION}
)

if(BITEFILE_BUILD_EXAMPLES)
    message("Building examples")
    add_subdirectory("examples/")
endif()
