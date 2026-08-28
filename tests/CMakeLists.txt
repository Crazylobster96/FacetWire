add_test(NAME facetwire.plugin_manifests.contract
    COMMAND ${CMAKE_COMMAND}
        "-DFACETWIRE_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/validate_plugin_manifests.cmake)
set_tests_properties(facetwire.plugin_manifests.contract PROPERTIES
    LABELS "unit;contract;plugin;manifest")

add_executable(facetwire_runtime_test runtime_test.c)
target_compile_features(facetwire_runtime_test PRIVATE c_std_11)
target_link_libraries(facetwire_runtime_test PRIVATE FacetWire::facetwire)

add_executable(facetwire_visual_transform_test visual_transform_test.c)
target_compile_features(facetwire_visual_transform_test PRIVATE c_std_11)
target_link_libraries(facetwire_visual_transform_test PRIVATE
    FacetWire::facetwire)
if(MSVC)
    target_compile_options(facetwire_visual_transform_test PRIVATE
        /W4 /WX /permissive- /utf-8)
else()
    target_compile_options(facetwire_visual_transform_test PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
    target_link_libraries(facetwire_visual_transform_test PRIVATE m)
endif()
add_test(NAME facetwire.visual_transform.contract
    COMMAND facetwire_visual_transform_test)
set_tests_properties(facetwire.visual_transform.contract PROPERTIES
    LABELS "unit;contract;visual-transform;image;animated-image;video;chart")

add_test(NAME facetwire.runtime.contract COMMAND facetwire_runtime_test)

add_executable(facetwire_runtime_discovery_test runtime_discovery_test.c)
target_compile_features(facetwire_runtime_discovery_test PRIVATE c_std_11)
target_link_libraries(facetwire_runtime_discovery_test PRIVATE
    FacetWire::facetwire)

set(FACETWIRE_TEST_NATIVE_DYNAMIC OFF)
if(FACETWIRE_ENABLE_NATIVE_DYNAMIC_LOADING)
    if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
       CMAKE_SYSTEM_NAME STREQUAL "Android" OR
       (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
        set(FACETWIRE_TEST_NATIVE_DYNAMIC ON)
    endif()
endif()

if(FACETWIRE_TEST_NATIVE_DYNAMIC)
    add_library(facetwire_dynamic_test_plugin MODULE dynamic_plugin.c)
    target_compile_features(facetwire_dynamic_test_plugin PRIVATE c_std_11)
    target_include_directories(facetwire_dynamic_test_plugin PRIVATE
        ${PROJECT_SOURCE_DIR}/include)
    set_target_properties(facetwire_dynamic_test_plugin PROPERTIES
        PREFIX ""
        OUTPUT_NAME "facetwire_dynamic_test_plugin")
    add_dependencies(facetwire_runtime_discovery_test
        facetwire_dynamic_test_plugin)
    add_test(NAME facetwire.runtime.discovery
        COMMAND facetwire_runtime_discovery_test
            $<TARGET_FILE:facetwire_dynamic_test_plugin>)
else()
    add_test(NAME facetwire.runtime.discovery
        COMMAND facetwire_runtime_discovery_test)
endif()

foreach(test_target IN ITEMS
    facetwire_runtime_test
    facetwire_runtime_discovery_test
)
    if(MSVC)
        target_compile_options(${test_target} PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(${test_target} PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()
if(TARGET facetwire_dynamic_test_plugin)
    if(MSVC)
        target_compile_options(facetwire_dynamic_test_plugin PRIVATE
            /W4 /WX /permissive-)
    else()
        target_compile_options(facetwire_dynamic_test_plugin PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()

set_tests_properties(
    facetwire.runtime.contract
    facetwire.runtime.discovery
    PROPERTIES LABELS "unit;contract;runtime;plugin")

if(TARGET facetwire_placeholder_renderer AND
   TARGET facetwire_text_renderer AND
   TARGET facetwire_core_image_renderer AND
   TARGET facetwire_core_media_renderer AND
   TARGET facetwire_flow_layout)
    add_executable(facetwire_memory_lifetime_test memory_lifetime_test.c)
    target_compile_features(facetwire_memory_lifetime_test PRIVATE c_std_11)
    target_link_libraries(facetwire_memory_lifetime_test PRIVATE
        FacetWire::facetwire
        FacetWire::placeholder_renderer
        FacetWire::text_renderer
        FacetWire::core_image_renderer
        FacetWire::core_media_renderer
        FacetWire::flow_layout)
    if(MSVC)
        target_compile_options(facetwire_memory_lifetime_test PRIVATE
            /W4 /WX /permissive- /utf-8)
    else()
        target_compile_options(facetwire_memory_lifetime_test PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
        target_link_libraries(facetwire_memory_lifetime_test PRIVATE m)
    endif()
    add_test(NAME facetwire.memory.lifecycle
        COMMAND facetwire_memory_lifetime_test)
    set_tests_properties(facetwire.memory.lifecycle PROPERTIES
        LABELS "unit;memory;lifetime;runtime;plugin")
endif()
