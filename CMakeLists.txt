cmake_minimum_required(VERSION 3.21)

project(
    FacetWire
    VERSION 0.1.0
    DESCRIPTION "Portable plugin contract and runtime for agent-native rich-media rendering"
    LANGUAGES C
)

include(GNUInstallDirs)
include(CTest)

option(FACETWIRE_BUILD_SHARED "Build FacetWire as a shared library" OFF)
option(FACETWIRE_BUILD_EXAMPLES "Build example plugins and hosts" ON)
option(FACETWIRE_BUILD_TESTS "Build the conformance tests" ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")


set(FACETWIRE_LIBRARY_TYPE STATIC)
if(FACETWIRE_BUILD_SHARED)
    set(FACETWIRE_LIBRARY_TYPE SHARED)
endif()

add_library(facetwire ${FACETWIRE_LIBRARY_TYPE}
    src/runtime.c
)
add_library(FacetWire::facetwire ALIAS facetwire)

target_compile_features(facetwire PUBLIC c_std_11)
target_compile_definitions(facetwire PRIVATE FACETWIRE_BUILDING_LIBRARY)
if(FACETWIRE_BUILD_SHARED)
    target_compile_definitions(facetwire PUBLIC FACETWIRE_SHARED)
endif()
target_include_directories(facetwire
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(MSVC)
    target_compile_options(facetwire PRIVATE /W4 /permissive-)
else()
    target_compile_options(facetwire PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(facetwire PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

if(FACETWIRE_BUILD_EXAMPLES)
    add_subdirectory(examples/hello_plugin)
endif()

if(BUILD_TESTING AND FACETWIRE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

install(TARGETS facetwire
    EXPORT FacetWireTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT FacetWireTargets
    FILE FacetWireTargets.cmake
    NAMESPACE FacetWire::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FacetWire
)
