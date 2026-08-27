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
option(FACETWIRE_BUILD_PLACEHOLDER_RENDERER
    "Build the reference placeholder renderer" ON)
option(FACETWIRE_BUILD_TEXT_RENDERER
    "Build the reference text renderer" ON)
option(FACETWIRE_BUILD_CORE_IMAGE_RENDERER
    "Build the reference Core Image and Animated Image renderer" ON)
option(FACETWIRE_BUILD_CORE_MEDIA_RENDERER
    "Build the reference Audio and Video renderer" ON)
option(FACETWIRE_BUILD_PLACEHOLDER_DEMO
    "Build the Windows/macOS placeholder demo native bridge" OFF)
option(FACETWIRE_ENABLE_NATIVE_DYNAMIC_LOADING
    "Enable explicit native DLL/SO/dylib loading where supported" ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(FACETWIRE_LIBRARY_TYPE STATIC)
if(FACETWIRE_BUILD_SHARED)
    set(FACETWIRE_LIBRARY_TYPE SHARED)
endif()

add_library(facetwire ${FACETWIRE_LIBRARY_TYPE}
    src/dynamic_library.c
    src/runtime.c
)
add_library(FacetWire::facetwire ALIAS facetwire)

target_compile_features(facetwire PUBLIC c_std_11)
target_compile_definitions(facetwire PRIVATE FACETWIRE_BUILDING_LIBRARY)
if(FACETWIRE_ENABLE_NATIVE_DYNAMIC_LOADING)
    target_compile_definitions(facetwire PRIVATE
        FACETWIRE_ENABLE_NATIVE_DYNAMIC_LOADING=1)
endif()
target_link_libraries(facetwire PRIVATE ${CMAKE_DL_LIBS})
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

if(FACETWIRE_BUILD_PLACEHOLDER_RENDERER)
    add_subdirectory(plugins/placeholder_renderer)
endif()
if(FACETWIRE_BUILD_TEXT_RENDERER)
    add_subdirectory(plugins/text_renderer)
endif()
if(FACETWIRE_BUILD_CORE_IMAGE_RENDERER)
    add_subdirectory(plugins/core_image_renderer)
endif()
if(FACETWIRE_BUILD_CORE_MEDIA_RENDERER)
    add_subdirectory(plugins/core_media_renderer)
endif()

if(FACETWIRE_BUILD_EXAMPLES)
    add_subdirectory(examples/hello_plugin)
endif()

if(FACETWIRE_BUILD_PLACEHOLDER_DEMO)
    if(NOT FACETWIRE_BUILD_PLACEHOLDER_RENDERER)
        message(FATAL_ERROR
            "FACETWIRE_BUILD_PLACEHOLDER_DEMO requires "
            "FACETWIRE_BUILD_PLACEHOLDER_RENDERER")
    endif()
    add_subdirectory(examples/placeholder_demo/native)
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
