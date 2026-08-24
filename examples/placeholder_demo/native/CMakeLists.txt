add_library(facetwire_placeholder_demo_bridge SHARED
    src/facetwire_placeholder_demo.c
)

target_compile_features(facetwire_placeholder_demo_bridge PRIVATE c_std_11)
target_compile_definitions(facetwire_placeholder_demo_bridge PRIVATE
    FACETWIRE_PLACEHOLDER_DEMO_BUILD
)
target_include_directories(facetwire_placeholder_demo_bridge PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(facetwire_placeholder_demo_bridge PRIVATE
    FacetWire::placeholder_renderer
)
set_target_properties(facetwire_placeholder_demo_bridge PROPERTIES
    OUTPUT_NAME facetwire_placeholder_demo_bridge
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    BUILD_RPATH_USE_ORIGIN YES
    INSTALL_RPATH "@loader_path"
)

if(MSVC)
    target_compile_options(facetwire_placeholder_demo_bridge PRIVATE
        /W4 /WX /permissive-
    )
else()
    target_compile_options(facetwire_placeholder_demo_bridge PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
    target_link_libraries(facetwire_placeholder_demo_bridge PRIVATE m)
endif()

if(BUILD_TESTING AND FACETWIRE_BUILD_TESTS)
    add_executable(facetwire_placeholder_demo_bridge_test
        tests/placeholder_demo_bridge_test.c
    )
    target_compile_features(facetwire_placeholder_demo_bridge_test PRIVATE
        c_std_11
    )
    target_link_libraries(facetwire_placeholder_demo_bridge_test PRIVATE
        facetwire_placeholder_demo_bridge
    )
    if(MSVC)
        target_compile_options(facetwire_placeholder_demo_bridge_test PRIVATE
            /W4 /WX /permissive-
        )
    else()
        target_compile_options(facetwire_placeholder_demo_bridge_test PRIVATE
            -Wall -Wextra -Wpedantic -Werror
        )
    endif()
    add_test(
        NAME facetwire.placeholder_demo.bridge
        COMMAND facetwire_placeholder_demo_bridge_test
    )
    set_tests_properties(facetwire.placeholder_demo.bridge PROPERTIES
        LABELS "unit;demo;placeholder"
    )
endif()

