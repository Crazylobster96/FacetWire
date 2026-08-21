# SPDX-License-Identifier: MPL-2.0
cmake_minimum_required(VERSION 3.21)
project(FacetWirePlaygroundUiSpike VERSION 0.1.0 LANGUAGES C)

include(CTest)

add_library(facetwire_ui_spike SHARED
    src/facetwire_ui_spike.c
)
target_compile_features(facetwire_ui_spike PUBLIC c_std_11)
target_compile_definitions(facetwire_ui_spike PRIVATE FWUI_BUILDING_LIBRARY)
target_include_directories(facetwire_ui_spike PUBLIC include)

if(MSVC)
    target_compile_options(facetwire_ui_spike PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(facetwire_ui_spike PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(BUILD_TESTING)
    add_executable(facetwire_ui_spike_test tests/spike_bridge_test.c)
    target_compile_features(facetwire_ui_spike_test PRIVATE c_std_11)
    target_link_libraries(facetwire_ui_spike_test PRIVATE facetwire_ui_spike)
    if(MSVC)
        target_compile_options(facetwire_ui_spike_test PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(facetwire_ui_spike_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME facetwire.ui_spike.bridge COMMAND facetwire_ui_spike_test)
endif()
