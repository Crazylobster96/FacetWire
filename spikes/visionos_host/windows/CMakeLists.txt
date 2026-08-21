# SPDX-License-Identifier: MPL-2.0
cmake_minimum_required(VERSION 3.21)
project(FacetWireVisionOSWindowsSpike VERSION 0.1.0 LANGUAGES C)

include(CTest)
add_subdirectory(
    ../../playground_ui/native
    ${CMAKE_CURRENT_BINARY_DIR}/facetwire_ui_bridge
)

add_executable(facetwire_visionos_windows_spike WIN32 src/main.c)
target_compile_features(facetwire_visionos_windows_spike PRIVATE c_std_11)
target_link_libraries(
    facetwire_visionos_windows_spike
    PRIVATE facetwire_ui_spike comctl32 msimg32
)

add_custom_command(TARGET facetwire_visionos_windows_spike POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:facetwire_ui_spike>
        $<TARGET_FILE_DIR:facetwire_visionos_windows_spike>
)

if(MSVC)
    target_compile_options(
        facetwire_visionos_windows_spike PRIVATE /W4 /WX /permissive-
    )
else()
    message(FATAL_ERROR "The Windows simulator requires MSVC and Win32.")
endif()
