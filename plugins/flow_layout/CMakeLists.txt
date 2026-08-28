add_library(facetwire_flow_layout ${FACETWIRE_LIBRARY_TYPE}
    src/plugin.c)
add_library(FacetWire::flow_layout ALIAS facetwire_flow_layout)
target_compile_features(facetwire_flow_layout PRIVATE c_std_11)
target_link_libraries(facetwire_flow_layout PUBLIC FacetWire::facetwire)
target_include_directories(facetwire_flow_layout PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if(FACETWIRE_LIBRARY_TYPE STREQUAL "SHARED")
    target_compile_definitions(facetwire_flow_layout PRIVATE
        FACETWIRE_FLOW_LAYOUT_DYNAMIC_ENTRY=1)
endif()
if(MSVC)
    target_compile_options(facetwire_flow_layout PRIVATE
        /W4 /WX /permissive- /utf-8)
else()
    target_compile_options(facetwire_flow_layout PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
    target_link_libraries(facetwire_flow_layout PRIVATE m)
endif()

if(BUILD_TESTING AND FACETWIRE_BUILD_TESTS)
    add_executable(facetwire_flow_layout_test tests/flow_layout_test.c)
    target_compile_features(facetwire_flow_layout_test PRIVATE c_std_11)
    target_link_libraries(facetwire_flow_layout_test PRIVATE
        FacetWire::flow_layout)
    if(MSVC)
        target_compile_options(facetwire_flow_layout_test PRIVATE
            /W4 /WX /permissive- /utf-8)
    else()
        target_compile_options(facetwire_flow_layout_test PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
        target_link_libraries(facetwire_flow_layout_test PRIVATE m)
    endif()
    add_test(NAME facetwire.flow_layout.contract
        COMMAND facetwire_flow_layout_test)
    set_tests_properties(facetwire.flow_layout.contract PROPERTIES
        LABELS "unit;contract;flow;layout;lifetime")
endif()

install(TARGETS facetwire_flow_layout EXPORT FacetWireTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
