add_library(facetwire_core_chart_renderer ${FACETWIRE_LIBRARY_TYPE}
    src/plugin.c)
add_library(FacetWire::core_chart_renderer ALIAS facetwire_core_chart_renderer)
target_compile_features(facetwire_core_chart_renderer PRIVATE c_std_11)
target_link_libraries(facetwire_core_chart_renderer PUBLIC FacetWire::facetwire)
target_include_directories(facetwire_core_chart_renderer PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if(FACETWIRE_LIBRARY_TYPE STREQUAL "SHARED")
    target_compile_definitions(facetwire_core_chart_renderer PRIVATE
        FACETWIRE_CORE_CHART_DYNAMIC_ENTRY=1)
endif()
if(MSVC)
    target_compile_options(facetwire_core_chart_renderer PRIVATE
        /W4 /WX /permissive- /utf-8)
else()
    target_compile_options(facetwire_core_chart_renderer PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
    target_link_libraries(facetwire_core_chart_renderer PRIVATE m)
endif()
if(BUILD_TESTING AND FACETWIRE_BUILD_TESTS)
    add_executable(facetwire_core_chart_renderer_test
        tests/core_chart_renderer_test.c)
    target_compile_features(facetwire_core_chart_renderer_test PRIVATE c_std_11)
    target_link_libraries(facetwire_core_chart_renderer_test PRIVATE
        FacetWire::core_chart_renderer)
    if(MSVC)
        target_compile_options(facetwire_core_chart_renderer_test PRIVATE
            /W4 /WX /permissive- /utf-8)
    else()
        target_compile_options(facetwire_core_chart_renderer_test PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
        target_link_libraries(facetwire_core_chart_renderer_test PRIVATE m)
    endif()
    add_test(NAME facetwire.core_chart_renderer.contract
        COMMAND facetwire_core_chart_renderer_test)
    set_tests_properties(facetwire.core_chart_renderer.contract PROPERTIES
        LABELS "unit;contract;chart;visual-transform;hit-test")
endif()
install(TARGETS facetwire_core_chart_renderer EXPORT FacetWireTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
