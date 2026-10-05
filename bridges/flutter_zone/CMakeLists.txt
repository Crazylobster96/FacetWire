add_library(facetwire_flutter_zone_bridge SHARED
    src/facetwire_flutter_zone_bridge.c)
target_compile_features(facetwire_flutter_zone_bridge PRIVATE c_std_11)
target_compile_definitions(facetwire_flutter_zone_bridge PRIVATE
    FW_FLUTTER_ZONE_BRIDGE_BUILDING_LIBRARY)
target_include_directories(facetwire_flutter_zone_bridge PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(facetwire_flutter_zone_bridge PRIVATE FacetWire::facetwire)
set_target_properties(facetwire_flutter_zone_bridge PROPERTIES
    C_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES
    BUILD_RPATH_USE_ORIGIN YES INSTALL_RPATH "@loader_path")
if(MSVC)
    target_compile_options(facetwire_flutter_zone_bridge PRIVATE
        /W4 /WX /permissive- /utf-8)
else()
    target_compile_options(facetwire_flutter_zone_bridge PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
install(TARGETS facetwire_flutter_zone_bridge EXPORT FacetWireTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES include/facetwire_flutter_zone_bridge.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(BUILD_TESTING AND FACETWIRE_BUILD_TESTS)
    add_executable(facetwire_flutter_zone_bridge_test
        tests/flutter_zone_bridge_test.c)
    target_compile_features(facetwire_flutter_zone_bridge_test PRIVATE c_std_11)
    target_include_directories(facetwire_flutter_zone_bridge_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include)
    target_link_libraries(facetwire_flutter_zone_bridge_test PRIVATE
        facetwire_flutter_zone_bridge)
    if(MINGW)
        target_link_options(facetwire_flutter_zone_bridge_test PRIVATE -municode)
    endif()
    add_dependencies(facetwire_flutter_zone_bridge_test
        facetwire_status_tile_renderer facetwire_dynamic_test_plugin)
    if(MSVC)
        foreach(target IN ITEMS facetwire_flutter_zone_bridge_test)
            target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8)
        endforeach()
    else()
        foreach(target IN ITEMS facetwire_flutter_zone_bridge_test)
            target_compile_options(${target} PRIVATE
                -Wall -Wextra -Wpedantic -Werror)
        endforeach()
    endif()
    add_test(NAME facetwire.flutter_zone.native_bridge
        COMMAND facetwire_flutter_zone_bridge_test
            $<TARGET_FILE:facetwire_status_tile_renderer>
            $<TARGET_FILE:facetwire_dynamic_test_plugin>)
    set_tests_properties(facetwire.flutter_zone.native_bridge PROPERTIES
        LABELS "unit;contract;flutter-zone;native-bridge")
    if(WIN32)
        add_test(NAME facetwire.flutter_zone.installed_long_path
            COMMAND ${CMAKE_COMMAND}
                "-DBRIDGE_TEST_EXE=$<TARGET_FILE:facetwire_flutter_zone_bridge_test>"
                "-DPLUGIN_FILE=$<TARGET_FILE:facetwire_status_tile_renderer>"
                "-DLEGACY_PLUGIN_FILE=$<TARGET_FILE:facetwire_dynamic_test_plugin>"
                "-DSTAGING_DIR=${CMAKE_CURRENT_BINARY_DIR}/installed-long-path/$<CONFIG>/identity"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/installed_long_path.cmake)
        set_tests_properties(facetwire.flutter_zone.installed_long_path PROPERTIES
            LABELS "unit;contract;flutter-zone;unicode;long-path")
    endif()
endif()
