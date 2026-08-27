add_library(facetwire_placeholder_renderer ${FACETWIRE_LIBRARY_TYPE}
    src/plugin.c
)
add_library(FacetWire::placeholder_renderer ALIAS
    facetwire_placeholder_renderer
)

target_compile_features(facetwire_placeholder_renderer PRIVATE c_std_11)
target_link_libraries(facetwire_placeholder_renderer PUBLIC
    FacetWire::facetwire
)
target_include_directories(facetwire_placeholder_renderer PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(FACETWIRE_LIBRARY_TYPE STREQUAL "SHARED")
    target_compile_definitions(facetwire_placeholder_renderer PRIVATE
        FACETWIRE_PLACEHOLDER_RENDERER_DYNAMIC_ENTRY=1)
endif()
if(MSVC)
    target_compile_options(facetwire_placeholder_renderer PRIVATE
        /W4 /WX /permissive-
    )
else()
    target_compile_options(facetwire_placeholder_renderer PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
    target_link_libraries(facetwire_placeholder_renderer PRIVATE m)
endif()

if(BUILD_TESTING AND FACETWIRE_BUILD_TESTS)
    add_executable(facetwire_placeholder_renderer_test
        tests/placeholder_renderer_test.c
    )
    add_executable(facetwire_placeholder_rendering_contract_test
        tests/placeholder_rendering_contract_test.c
    )
    foreach(test_target IN ITEMS
        facetwire_placeholder_renderer_test
        facetwire_placeholder_rendering_contract_test
    )
        target_compile_features(${test_target} PRIVATE c_std_11)
        target_link_libraries(${test_target} PRIVATE
            FacetWire::placeholder_renderer
        )
        if(MSVC)
            target_compile_options(${test_target} PRIVATE
                /W4 /WX /permissive-
            )
        else()
            target_compile_options(${test_target} PRIVATE
                -Wall -Wextra -Wpedantic -Werror
            )
            target_link_libraries(${test_target} PRIVATE m)
        endif()
    endforeach()
    add_test(
        NAME facetwire.placeholder_renderer.contract
        COMMAND facetwire_placeholder_renderer_test
    )
    add_test(
        NAME facetwire.placeholder_renderer.rendering_contract
        COMMAND facetwire_placeholder_rendering_contract_test
    )
    set_tests_properties(
        facetwire.placeholder_renderer.contract
        facetwire.placeholder_renderer.rendering_contract
        PROPERTIES LABELS "unit;contract;placeholder"
    )
endif()

install(TARGETS facetwire_placeholder_renderer
    EXPORT FacetWireTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
